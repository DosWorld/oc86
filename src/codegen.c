#include "codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

ObjFile cg_obj;

void     cg_init(void)  { rdf_init(&cg_obj); }
void     cg_free(void)  { rdf_free(&cg_obj); }
uint16_t cg_pc(void)    { return (uint16_t)cg_obj.code_len; }
uint16_t cg_dpc(void)   { return (uint16_t)cg_obj.data_len; }

void cg_emit1(uint8_t b)                   { emit_byte(&cg_obj, SEG_CODE, b); }
void cg_emit2(uint8_t b0, uint8_t b1)     { cg_emit1(b0); cg_emit1(b1); }
void cg_emit3(uint8_t b0, uint8_t b1, uint8_t b2) { cg_emit1(b0); cg_emit1(b1); cg_emit1(b2); }
void cg_emitw(uint16_t w) { cg_emit1((uint8_t)(w & 0xFF)); cg_emit1((uint8_t)(w >> 8)); }
void cg_emitd(uint32_t d) { cg_emitw((uint16_t)(d & 0xFFFF)); cg_emitw((uint16_t)(d >> 16)); }

void cg_emit_code_str(uint16_t size, const char *str) {
    while (size != 0) {
        emit_byte(&cg_obj, SEG_CODE, (uint8_t)*str);
        str++;
        size--;
    }
}

void cg_emit_data_byte(uint8_t b)   { emit_byte(&cg_obj, SEG_DATA, b); }
void cg_emit_data_word(uint16_t w)  { emit_word(&cg_obj, SEG_DATA, w); }
/* Bulk zero fill: single bounds check + memset (was N emit_byte calls). */
void cg_emit_data_zero(uint16_t n) {
    if (n == 0) return;
    if ((uint32_t)cg_obj.data_len + (uint32_t)n > SEG_LIMIT) {
        fprintf(stderr, "error: data segment exceeds %u bytes\n", (unsigned)SEG_LIMIT);
        exit(1);
    }
    memset(cg_obj.data + cg_obj.data_len, 0, n);
    cg_obj.data_len += n;
}
void cg_emit_data_string(const char *s) {
    while (*s) emit_byte(&cg_obj, SEG_DATA, (uint8_t)*s++);
    emit_byte(&cg_obj, SEG_DATA, 0);
}

/* ---- frame ---- */
void cg_prologue(uint16_t local_bytes) {
    cg_emit_code_str(5, "\x55\x8B\xEC\x81\xEC"); /* PUSH BP / MOV BP,SP / SUB SP,imm16 */
    cg_emitw(local_bytes);
}

void cg_epilogue(uint16_t arg_bytes) {
    cg_emit_code_str(3, "\x8B\xE5\x5D"); /* MOV SP,BP / POP BP */
    if (arg_bytes == 0) {
        cg_emit1(OP_RET);
    } else {
        cg_emit1(OP_RET_N); cg_emitw(arg_bytes);
    }
}

/* ---- calls ---- */
void cg_call_near(int import_id) {
    uint16_t patch_at;
    int32_t addend;
    cg_emit1(OP_CALL_NEAR);
    patch_at = cg_pc();
    addend   = -(int32_t)(patch_at + 2);
    cg_emitw((uint16_t)(addend & 0xFFFF));
    rdf_add_reloc(&cg_obj, SEG_CODE, patch_at, 2, import_id, 1);
}

void cg_call_local(uint16_t target) {
    uint16_t patch_at;
    cg_emit1(OP_CALL_NEAR);
    patch_at = cg_pc();
    cg_emitw((uint16_t)(target - (uint16_t)(patch_at + 2)));
}

/* PUSH CS + CALL NEAR — equivalent to CALL FAR for an exported proc in the same
   module.  The callee does RETF which pops IP then CS correctly.
   Produces no relocations. */
void cg_call_local_far(uint16_t target) {
    cg_emit1(OP_PUSH_CS);  /* 0E: PUSH CS */
    cg_call_local(target); /* E8 rel16   */
}

/* CALL FAR through proc variable in memory (indirect far call FF /3).
   Proc variables store {offset:2, segment:2} — same layout as far pointers.
   CALL FAR [BP+ofs]:  FF 5E imm8  (disp8)  or  FF 9E imm16  (disp16)
   CALL FAR [data]:    FF 1E imm16 */
void cg_call_proc_var_bp(int32_t ofs) {
    if (ofs >= -128 && ofs <= 127) {
        cg_emit2(0xFF, 0x5E); cg_emit1((uint8_t)(ofs & 0xFF));
    } else {
        cg_emit2(0xFF, 0x9E); cg_emitw((uint16_t)(int16_t)ofs);
    }
}
void cg_call_proc_var_mem(uint16_t data_ofs) {
    cg_emit2(0xFF, 0x1E); cg_emitw(data_ofs);
}
void cg_call_proc_var_esbx(void) {
    /* CALL FAR ES:[BX]: 26 FF 1F  (ES: CALL m16:16, mod=00, reg=3, rm=111) */
    cg_emit3(0x26, 0xFF, 0x1F);
}
/* CALL NEAR through proc variable (indirect near call FF /2).
   NEAR proc variables store {offset:2} — 2-byte word.
   CALL NEAR [BP+ofs]:  FF 56 imm8  (disp8)  or  FF 96 ofs16  (disp16)
   CALL NEAR [data]:    FF 16 ofs16 */
void cg_call_proc_var_near_bp(int32_t ofs) {
    if (ofs >= -128 && ofs <= 127) {
        cg_emit2(0xFF, 0x56); cg_emit1((uint8_t)(ofs & 0xFF));
    } else {
        cg_emit2(0xFF, 0x96); cg_emitw((uint16_t)(int16_t)ofs);
    }
}
void cg_call_proc_var_near_mem(uint16_t data_ofs) {
    cg_emit2(0xFF, 0x16); cg_emitw(data_ofs);
}

void cg_call_far(int import_id) {
    /* 9A <offset:2> <segment:2>
       offset word  → absolute RELOC  pointing at import_id (relative=0, addend=0)
       segment word → SEGRELOC        pointing at import_id
       CALL FAR uses absolute CS:IP, NOT IP-relative. The linker patches
       the offset word with the target's absolute code offset. */
    uint16_t ofs_at;
    uint16_t seg_at;
    cg_emit1(OP_CALL_FAR);
    ofs_at = cg_pc();
    cg_emitw(0);                  /* offset placeholder (addend=0, absolute) */
    seg_at = cg_pc();
    cg_emitw(0);                  /* segment placeholder */
    rdf_add_reloc   (&cg_obj, SEG_CODE, ofs_at, 2, import_id, 0); /* relative=0 */
    rdf_add_segreloc(&cg_obj, SEG_CODE, seg_at, import_id);
}

void cg_epilogue_far(uint16_t arg_bytes) {
    cg_emit_code_str(3, "\x8B\xE5\x5D"); /* MOV SP,BP / POP BP */
    if (arg_bytes == 0) {
        cg_emit1(OP_RETF);
    } else {
        cg_emit1(OP_RETF_N); cg_emitw(arg_bytes);
    }
}

/* ---- jumps ---- */
void cg_jmp_short(Backpatch *bp) {
    cg_emit1(OP_JMP_SHORT);
    bp->offset = cg_pc(); bp->width = 1;
    cg_emit1(0);
}
void cg_jmp_near(Backpatch *bp) {
    cg_emit1(OP_JMP_NEAR);
    bp->offset = cg_pc(); bp->width = 2;
    cg_emitw(0);
}
void cg_cond_short(int opcode, Backpatch *bp) {
    cg_emit1((uint8_t)opcode);
    bp->offset = cg_pc(); bp->width = 1;
    cg_emit1(0);
}
/* invert_jcc — returns the opposite condition code for any 0x70-0x7F Jcc.
   Pairs: JZ/JNZ(74/75), JL/JGE(7C/7D), JLE/JG(7E/7F), JB/JAE(72/73),
          JS/JNS(78/79), JO/JNO(70/71), JP/JNP(7A/7B), JBE/JA(76/77). */
int invert_jcc(int opcode) { return opcode ^ 1; }

void cg_cond_near(int opcode, Backpatch *bp) {
    /* 8086-safe long conditional jump:
         inv_opcode  03        ; Jcc_inv rel8=+3: skip the JMP near (3 bytes)
         E9          lo  hi    ; JMP near target
       Total = 5 bytes. bp->offset points at the rel16 inside JMP near. */
    cg_emit2((uint8_t)invert_jcc(opcode), 3);   /* Jcc_inv +3 */
    cg_emit1(OP_JMP_NEAR);
    bp->offset = cg_pc(); bp->width = 2;
    cg_emitw(0);
}
void cg_patch_short(Backpatch *bp) {
    int32_t rel = (int32_t)cg_pc() - (int32_t)(bp->offset + 1);
    assert(rel >= -128 && rel <= 127);
    cg_obj.code[bp->offset] = (uint8_t)(rel & 0xFF);
}
void cg_patch_near(Backpatch *bp) {
    int32_t rel = (int32_t)cg_pc() - (int32_t)(bp->offset + 2);
    cg_obj.code[bp->offset]   = (uint8_t)(rel & 0xFF);
    cg_obj.code[bp->offset+1] = (uint8_t)((rel >> 8) & 0xFF);
}
void cg_jmp_back(uint16_t target) {
    int32_t rel8 = (int32_t)target - (int32_t)(cg_pc() + 2);
    if (rel8 >= -128 && rel8 <= 127) {
        cg_emit1(OP_JMP_SHORT); cg_emit1((uint8_t)(rel8 & 0xFF));
    } else {
        cg_emit1(OP_JMP_NEAR);
        cg_emitw((uint16_t)(target - (uint16_t)(cg_pc() + 2)));
    }
}
void cg_cond_back(int opcode, uint16_t target) {
    int32_t rel = (int32_t)target - (int32_t)(cg_pc() + 2);
    assert(rel >= -128 && rel <= 127);
    cg_emit1((uint8_t)opcode); cg_emit1((uint8_t)(rel & 0xFF));
}

/* ---- load / store ---- */
void cg_load_bp(int32_t ofs) {
    if (ofs >= -128 && ofs <= 127) { cg_emit2(0x8B,0x46); cg_emit1(ofs & 0xFF); }
    else                           { cg_emit2(0x8B,0x86); cg_emitw(ofs); }
}
void cg_store_bp(int32_t ofs) {
    if (ofs >= -128 && ofs <= 127) { cg_emit2(0x89,0x46); cg_emit1(ofs & 0xFF); }
    else                           { cg_emit2(0x89,0x86); cg_emitw(ofs); }
}
/* Byte-sized load/store for CHAR/BYTE locals and globals.
   Load: MOV AL,[BP+ofs] then XOR AH,AH to zero-extend into AX.
   Store: MOV [BP+ofs],AL (only AL written; AH ignored). */
void cg_load_byte_bp(int32_t ofs) {
    if (ofs >= -128 && ofs <= 127) { cg_emit2(0x8A,0x46); cg_emit1(ofs & 0xFF); }
    else                           { cg_emit2(0x8A,0x86); cg_emitw(ofs); }
    cg_emit2(0x30, 0xE4);  /* XOR AH, AH */
}
void cg_store_byte_bp(int32_t ofs) {
    if (ofs >= -128 && ofs <= 127) { cg_emit2(0x88,0x46); cg_emit1(ofs & 0xFF); }
    else                           { cg_emit2(0x88,0x86); cg_emitw(ofs); }
}
void cg_load_byte_mem(uint16_t data_ofs) {
    uint16_t patch;
    cg_emit1(0xA0);                 /* MOV AL, [imm16] */
    patch = cg_pc();
    cg_emitw(data_ofs);
    rdf_add_reloc(&cg_obj, SEG_CODE, patch, 2, SEG_DATA, 0);
    cg_emit2(0x30, 0xE4);           /* XOR AH, AH  — zero-extend AL into AX */
}
void cg_store_byte_mem(uint16_t data_ofs) {
    uint16_t patch;
    cg_emit1(0xA2);
    patch = cg_pc();
    cg_emitw(data_ofs);
    rdf_add_reloc(&cg_obj, SEG_CODE, patch, 2, SEG_DATA, 0);
}
void cg_load_addr_bp(int32_t ofs) {
    if (ofs >= -128 && ofs <= 127) { cg_emit2(0x8D,0x46); cg_emit1(ofs & 0xFF); }
    else                           { cg_emit2(0x8D,0x86); cg_emitw(ofs); }
}
/* ---- static link helpers ---- */
/* Load outer frame BP into BX by traversing 'hops' static link slots.
   Static link for nested proc is at [BP+4] (pushed last by caller).
   After 1 hop: BX = [BP+4]  (direct outer frame BP)
   After 2 hops: BX = [[BP+4]+4]  (two levels up)
   etc. */
void cg_sl_load_bx(int hops) {
    /* First hop: MOV BX, [BP+4] — BP uses SS by default */
    cg_emit_code_str(3, "\x8B\x5E\x04"); /* MOV BX, [BP+4] */
    hops--;
    /* Additional hops: MOV BX, SS:[BX+4] — BX uses DS by default; override to SS */
    while (hops-- > 0)
        cg_emit_code_str(4, "\x36\x8B\x5F\x04"); /* SS: MOV BX, [BX+4] */
}

/* Load word from outer frame variable at [outer_BP + ofs] into AX.
   BX is clobbered. */
void cg_sl_load_ax(int hops, int32_t ofs) {
    cg_sl_load_bx(hops);
    /* MOV AX, SS:[BX+ofs] — BX holds outer frame BP (stack address); need SS: prefix */
    if (ofs >= -128 && ofs <= 127) {
        cg_emit1(0x36);              /* SS: prefix */
        cg_emit3(0x8B, 0x47, (uint8_t)(ofs & 0xFF)); /* MOV AX, [BX+disp8] */
    } else {
        cg_emit1(0x36);              /* SS: prefix */
        cg_emit2(0x8B, 0x87); cg_emitw((uint16_t)ofs); /* MOV AX, [BX+disp16] */
    }
}

/* Store AX into outer frame variable at [outer_BP + ofs].
   BX is clobbered. AX is preserved (value is stored from AX). */
void cg_sl_store_ax(int hops, int32_t ofs) {
    cg_emit1(0x50);              /* PUSH AX (save value) */
    cg_sl_load_bx(hops);
    cg_emit1(0x58);              /* POP AX (restore value) */
    /* MOV SS:[BX+ofs], AX — BX holds outer frame BP (stack address); need SS: prefix */
    if (ofs >= -128 && ofs <= 127) {
        cg_emit1(0x36);              /* SS: prefix */
        cg_emit3(0x89, 0x47, (uint8_t)(ofs & 0xFF)); /* MOV [BX+disp8], AX */
    } else {
        cg_emit1(0x36);              /* SS: prefix */
        cg_emit2(0x89, 0x87); cg_emitw((uint16_t)ofs); /* MOV [BX+disp16], AX */
    }
}

/* Load address of outer frame variable (near offset, SS-relative) into AX.
   Used for VAR params: the outer var's address = SS:(&outer_var).
   Since all procs share the same SS (real mode), the near offset is sufficient.
   BX is clobbered. */
void cg_sl_addr_ax(int hops, int32_t ofs) {
    cg_sl_load_bx(hops);
    /* LEA AX, [BX+ofs] */
    if (ofs >= -128 && ofs <= 127) {
        cg_emit3(0x8D, 0x47, (uint8_t)(ofs & 0xFF)); /* LEA AX, [BX+disp8] */
    } else {
        cg_emit2(0x8D, 0x87); cg_emitw((uint16_t)ofs); /* LEA AX, [BX+disp16] */
    }
}

/* Load 32-bit DX:AX from outer frame at SS:[BX+ofs] and SS:[BX+ofs+2].
   BX must already hold outer frame's BP (set by cg_sl_load_bx). */
static void sl_load_word_bx(int32_t ofs, uint8_t reg_hi_byte) {
    /* MOV reg, SS:[BX+ofs]: 36 8B <ModRM> [disp] */
    if (ofs >= -128 && ofs <= 127) {
        cg_emit1(0x36); cg_emit2(0x8B, (uint8_t)(0x47 | reg_hi_byte)); cg_emit1((uint8_t)(ofs & 0xFF));
    } else {
        cg_emit1(0x36); cg_emit2(0x8B, (uint8_t)(0x87 | reg_hi_byte)); cg_emitw((uint16_t)(int16_t)ofs);
    }
}
static void sl_store_word_bx(int32_t ofs, uint8_t reg_hi_byte) {
    /* MOV SS:[BX+ofs], reg: 36 89 <ModRM> [disp] */
    if (ofs >= -128 && ofs <= 127) {
        cg_emit1(0x36); cg_emit2(0x89, (uint8_t)(0x47 | reg_hi_byte)); cg_emit1((uint8_t)(ofs & 0xFF));
    } else {
        cg_emit1(0x36); cg_emit2(0x89, (uint8_t)(0x87 | reg_hi_byte)); cg_emitw((uint16_t)(int16_t)ofs);
    }
}

void cg_sl_load_long_ax(int hops, int32_t ofs) {
    cg_sl_load_bx(hops);
    sl_load_word_bx(ofs,     0x00); /* MOV AX, SS:[BX+ofs]   (reg=AX, hi byte = 0x00) */
    sl_load_word_bx(ofs + 2, 0x10); /* MOV DX, SS:[BX+ofs+2] (reg=DX, hi byte = 0x10) */
}

void cg_sl_store_long_ax(int hops, int32_t ofs) {
    /* cg_sl_load_bx clobbers only BX, not AX/DX — no save/restore needed */
    cg_sl_load_bx(hops);        /* BX = outer BP */
    sl_store_word_bx(ofs,     0x00); /* MOV SS:[BX+ofs],   AX */
    sl_store_word_bx(ofs + 2, 0x10); /* MOV SS:[BX+ofs+2], DX */
}

/* SS: FLD/FSTP dword/qword [BX+ofs] — BX already holds outer frame BP. */
void cg_sl_fld_real_bx(int32_t ofs) {
    /* D9 /0 mod=01/10 rm=111: D9 47 ofs8 or D9 87 ofs16lo ofs16hi */
    if (ofs >= -128 && ofs <= 127) {
        cg_emit1(0x36); cg_emit2(0xD9, 0x47); cg_emit1((uint8_t)(ofs & 0xFF));
    } else {
        cg_emit1(0x36); cg_emit2(0xD9, 0x87); cg_emitw((uint16_t)(int16_t)ofs);
    }
}
void cg_sl_fstp_real_bx(int32_t ofs) {
    if (ofs >= -128 && ofs <= 127) {
        cg_emit1(0x36); cg_emit2(0xD9, 0x5F); cg_emit1((uint8_t)(ofs & 0xFF));
    } else {
        cg_emit1(0x36); cg_emit2(0xD9, 0x9F); cg_emitw((uint16_t)(int16_t)ofs);
    }
}
void cg_sl_fld_longreal_bx(int32_t ofs) {
    if (ofs >= -128 && ofs <= 127) {
        cg_emit1(0x36); cg_emit2(0xDD, 0x47); cg_emit1((uint8_t)(ofs & 0xFF));
    } else {
        cg_emit1(0x36); cg_emit2(0xDD, 0x87); cg_emitw((uint16_t)(int16_t)ofs);
    }
}
void cg_sl_fstp_longreal_bx(int32_t ofs) {
    if (ofs >= -128 && ofs <= 127) {
        cg_emit1(0x36); cg_emit2(0xDD, 0x5F); cg_emit1((uint8_t)(ofs & 0xFF));
    } else {
        cg_emit1(0x36); cg_emit2(0xDD, 0x9F); cg_emitw((uint16_t)(int16_t)ofs);
    }
}

void cg_load_mem(uint16_t data_ofs) {
    uint16_t patch;
    cg_emit1(0xA1);
    patch = cg_pc();
    cg_emitw(data_ofs);
    rdf_add_reloc(&cg_obj, SEG_CODE, patch, 2, SEG_DATA, 0);
}
void cg_store_mem(uint16_t data_ofs) {
    uint16_t patch;
    cg_emit1(0xA3);
    patch = cg_pc();
    cg_emitw(data_ofs);
    rdf_add_reloc(&cg_obj, SEG_CODE, patch, 2, SEG_DATA, 0);
}
void cg_load_addr_mem(uint16_t data_ofs) {
    uint16_t patch;
    cg_emit2(0x8D, 0x06);
    patch = cg_pc();
    cg_emitw(data_ofs);
    rdf_add_reloc(&cg_obj, SEG_CODE, patch, 2, SEG_DATA, 0);
}
/* MOV AX, code_ofs — load code-segment offset with CODE reloc (for SYSTEM.ADR of proc) */
void cg_load_code_addr(uint16_t code_ofs) {
    uint16_t patch;
    cg_emit1(0xB8);   /* MOV AX, imm16 */
    patch = cg_pc();
    cg_emitw(code_ofs);
    rdf_add_reloc(&cg_obj, SEG_CODE, patch, 2, SEG_CODE, 0);
}

/* ---- far-pointer helpers ---- */

/* LES BX, [BP+offset]  — load 4-byte far ptr from stack frame into ES:BX */
void cg_les_bx_bp(int32_t ofs) {
    if (ofs >= -128 && ofs <= 127) {
        cg_emit2(OP_LES, 0x5E); cg_emit1(ofs & 0xFF);  /* C4 5E disp8 */
    } else {
        cg_emit2(OP_LES, 0x9E); cg_emitw(ofs);          /* C4 9E disp16 */
    }
}

/* LES BX, [data_ofs]  — load 4-byte far ptr from data segment into ES:BX */
void cg_les_bx_mem(uint16_t data_ofs) {
    uint16_t patch;
    cg_emit2(OP_LES, 0x1E);                /* C4 1E disp16 */
    patch = cg_pc();
    cg_emitw(data_ofs);
    rdf_add_reloc(&cg_obj, SEG_CODE, patch, 2, SEG_DATA, 0);
}

/* Move far pointer from DX:AX to ES:BX */
void cg_dxax_to_esbx(void) {
    cg_emit_code_str(4, "\x8B\xD8\x8E\xC2"); /* MOV BX,AX / MOV ES,DX */
}

/* Dereference far pointer: MOV AX, ES:[BX]  (assumes ES:BX set up) */
void cg_deref_far(void) {
    cg_emit_code_str(3, "\x26\x8B\x07"); /* ES: MOV AX,[BX] */
}

/* Load tag word at ES:[BX-2] (type tag stored 2 bytes before object data) */
void cg_load_tag_far(void) {
    cg_emit_code_str(4, "\x26\x8B\x47\xFE"); /* ES: MOV AX,[BX-2] */
}

/* Store immediate word through ES:[BX]: ES: MOV WORD PTR [BX], imm16 */
void cg_store_word_esbx_imm(uint16_t val) {
    cg_emit3(0x26, 0xC7, 0x07); cg_emitw(val); /* ES: MOV [BX], imm16 */
}

/* Dereference far pointer: byte load — MOV AL, ES:[BX]; zero-extend to AX */
void cg_deref_far_byte(void) {
    cg_emit_code_str(6, "\x26\x8A\x07\x25\xFF\x00"); /* ES: MOV AL,[BX] / AND AX,00FFh */
}

/* Store AX through far pointer in ES:BX: MOV ES:[BX], AX */
void cg_store_deref_far(void) {
    cg_emit_code_str(3, "\x26\x89\x07"); /* ES: MOV [BX],AX */
}

/* Store AL through far pointer in ES:BX: MOV ES:[BX], AL (byte store) */
void cg_store_deref_far_byte(void) {
    cg_emit_code_str(3, "\x26\x88\x07"); /* ES: MOV [BX],AL */
}

/* Load 32-bit LONGINT through far pointer ES:BX into DX:AX.
   MOV AX, ES:[BX]  then  MOV DX, ES:[BX+2] */
void cg_deref_far_long(void) {
    cg_emit_code_str(7, "\x26\x8B\x07\x26\x8B\x57\x02"); /* ES:MOV AX,[BX] / ES:MOV DX,[BX+2] */
}

/* Store DX:AX as 32-bit LONGINT through far pointer ES:BX.
   MOV ES:[BX], AX  then  MOV ES:[BX+2], DX */
void cg_store_deref_far_long(void) {
    cg_emit_code_str(7, "\x26\x89\x07\x26\x89\x57\x02"); /* ES:MOV [BX],AX / ES:MOV [BX+2],DX */
}

/* Legacy near-ptr deref kept for reference; NOT used with far pointers */
void cg_deref(void) {
    cg_emit2(0x8B, 0xD8);   /* MOV BX, AX */
    cg_emit2(0x8B, 0x07);   /* MOV AX, [BX] */
}
void cg_store_deref(void) {
    cg_emit2(0x89, 0x07);   /* MOV [BX], AX  (BX = near ptr, AX = value) */
}

/* Load 4-byte far pointer from [BP+ofs] into DX:AX (offset in AX, segment in DX) */
void cg_load_ptr_bp(int32_t ofs) {
    int32_t ofs2;
    cg_load_bp(ofs);                                        /* AX = [BP+ofs] (offset) */
    ofs2 = ofs + 2;
    if (ofs2 >= -128 && ofs2 <= 127) {
        cg_emit2(0x8B, 0x56); cg_emit1(ofs2 & 0xFF);      /* MOV DX,[BP+disp8]  */
    } else {
        cg_emit2(0x8B, 0x96); cg_emitw(ofs2);              /* MOV DX,[BP+disp16] */
    }
}

/* Store DX:AX as 4-byte far pointer to [BP+ofs] */
void cg_store_ptr_bp(int32_t ofs) {
    int32_t ofs2;
    cg_store_bp(ofs);                                       /* [BP+ofs]   = AX (offset) */
    ofs2 = ofs + 2;
    if (ofs2 >= -128 && ofs2 <= 127) {
        cg_emit2(0x89, 0x56); cg_emit1(ofs2 & 0xFF);      /* MOV [BP+disp8], DX  */
    } else {
        cg_emit2(0x89, 0x96); cg_emitw(ofs2);              /* MOV [BP+disp16], DX */
    }
}

/* Load 4-byte far pointer from DS:[data_ofs] into DX:AX */
void cg_load_ptr_mem(uint16_t data_ofs) {
    uint16_t patch;
    cg_load_mem(data_ofs);          /* AX = [data_ofs] (offset) + RELOC */
    /* MOV DX, [data_ofs+2]  +  RELOC */
    cg_emit2(0x8B, 0x16);
    patch = cg_pc();
    cg_emitw((uint16_t)(data_ofs + 2));
    rdf_add_reloc(&cg_obj, SEG_CODE, patch, 2, SEG_DATA, 0);
}

/* Store DX:AX as 4-byte far pointer to DS:[data_ofs] */
void cg_store_ptr_mem(uint16_t data_ofs) {
    uint16_t patch;
    cg_store_mem(data_ofs);         /* [data_ofs] = AX (offset) + RELOC */
    /* MOV [data_ofs+2], DX  +  RELOC */
    cg_emit2(0x89, 0x16);
    patch = cg_pc();
    cg_emitw((uint16_t)(data_ofs + 2));
    rdf_add_reloc(&cg_obj, SEG_CODE, patch, 2, SEG_DATA, 0);
}

/* ---- LONGINT (32-bit) load/store/arithmetic ---- */

/* Load 32-bit LONGINT from [BP+ofs] into DX:AX (AX=lo, DX=hi) */
void cg_load_long_bp(int32_t ofs) {
    int32_t ofs2;
    cg_load_bp(ofs);          /* AX = [BP+ofs]   (low word) */
    /* MOV DX, [BP+ofs+2] */
    ofs2 = ofs + 2;
    if (ofs2 >= -128 && ofs2 <= 127) {
        cg_emit2(0x8B, 0x56); cg_emit1((uint8_t)(ofs2 & 0xFF));
    } else {
        cg_emit2(0x8B, 0x96); cg_emitw((uint16_t)ofs2);
    }
}

/* Store DX:AX (AX=lo, DX=hi) as 32-bit LONGINT to [BP+ofs] */
void cg_store_long_bp(int32_t ofs) {
    int32_t ofs2 = ofs + 2;
    cg_store_bp(ofs);          /* [BP+ofs] = AX (low word) */
    if (ofs2 >= -128 && ofs2 <= 127) {
        cg_emit2(0x89, 0x56); cg_emit1((uint8_t)(ofs2 & 0xFF));
    } else {
        cg_emit2(0x89, 0x96); cg_emitw((uint16_t)ofs2);
    }
}

/* Load 32-bit LONGINT from DS:[data_ofs] into DX:AX */
void cg_load_long_mem(uint16_t data_ofs) {
    uint16_t patch;
    cg_load_mem(data_ofs);          /* AX = [data_ofs] + RELOC */
    cg_emit2(0x8B, 0x16);
    patch = cg_pc();
    cg_emitw((uint16_t)(data_ofs + 2));
    rdf_add_reloc(&cg_obj, SEG_CODE, patch, 2, SEG_DATA, 0);
}

/* Store DX:AX as 32-bit LONGINT to DS:[data_ofs] */
void cg_store_long_mem(uint16_t data_ofs) {
    uint16_t patch;
    cg_store_mem(data_ofs);         /* [data_ofs] = AX + RELOC */
    cg_emit2(0x89, 0x16);
    patch = cg_pc();
    cg_emitw((uint16_t)(data_ofs + 2));
    rdf_add_reloc(&cg_obj, SEG_CODE, patch, 2, SEG_DATA, 0);
}

/* Load 32-bit immediate into DX:AX */
void cg_load_long_imm(int32_t val) {
    uint16_t lo = (uint16_t)((uint32_t)val & 0xFFFF);
    uint16_t hi = (uint16_t)(((uint32_t)val >> 16) & 0xFFFF);
    if (lo == 0) { cg_emit2(0x33, 0xC0); }   /* XOR AX,AX */
    else         { cg_emit1(0xB8); cg_emitw(lo); }
    if (hi == 0) { cg_emit2(0x33, 0xD2); }   /* XOR DX,DX */
    else         { cg_emit1(0xBA); cg_emitw(hi); }
}

/* PUSH DX; PUSH AX — save 32-bit DX:AX onto stack */
void cg_push_dxax(void) { cg_emit1(0x52); cg_emit1(0x50); }

/* POP BX; POP CX — restore 32-bit into CX:BX (hi:lo) from stack */
void cg_pop_cxbx(void) { cg_emit1(0x5B); cg_emit1(0x59); }

/* 32-bit ADD: DX:AX += CX:BX */
void cg_add32(void) {
    cg_emit_code_str(4, "\x03\xC3\x13\xD1"); /* ADD AX,BX / ADC DX,CX */
}

/* 32-bit SUB: DX:AX = CX:BX - DX:AX */
void cg_sub32(void) {
    cg_emit_code_str(8, "\x87\xC3\x2B\xC3\x87\xD1\x1B\xD1");
    /* XCHG AX,BX / SUB AX,BX / XCHG DX,CX / SBB DX,CX */
}

/* 32-bit NEG: DX:AX = -DX:AX */
void cg_neg32(void) {
    cg_emit_code_str(10, "\xF7\xD0\xF7\xD2\x05\x01\x00\x83\xD2\x00");
    /* NOT AX / NOT DX / ADD AX,1 / ADC DX,0 */
}

/* 32-bit AND: DX:AX &= CX:BX */
void cg_and32(void) {
    cg_emit_code_str(4, "\x23\xC3\x23\xD1"); /* AND AX,BX / AND DX,CX */
}

/* 32-bit OR: DX:AX |= CX:BX */
void cg_or32(void) {
    cg_emit_code_str(4, "\x0B\xC3\x0B\xD1"); /* OR AX,BX / OR DX,CX */
}

/* 32-bit XOR: DX:AX ^= CX:BX */
void cg_xor32(void) {
    cg_emit_code_str(4, "\x33\xC3\x33\xD1"); /* XOR AX,BX / XOR DX,CX */
}

/* 32-bit signed comparison: CX:BX (LHS) op DX:AX (RHS) → AX = 0 or 1.
   Correct 8086 two-stage approach:
     1. CMP CX, DX  (hi words, signed)
     2. If hi words unequal: result determined by signed hi comparison
     3. If hi words equal: CMP BX, AX (lo words, unsigned) determines result
   For equality/inequality: XOR hi words and lo words, OR together.
   For ordered: use Jcc on hi-word result, fall through to unsigned lo-word compare.

   Parameters: hi_true = signed Jcc opcode that means "true" based on hi-word only
               hi_false = signed Jcc opcode that means "false" based on hi-word only
               lo_true  = unsigned Jcc opcode that means "true" based on lo-word only
   Pass hi_true=0 for equality/inequality (handled specially).
   Pass hi_true=0xFF for equality, hi_true=0xFE for inequality. */
void cg_cmp32(void) {
    /* Kept for compatibility but not used for ordered comparison — see cg_cmp32_bool */
    cg_emit2(0x3B, 0xD8);   /* CMP BX, AX   (BX - AX, set CF) */
    cg_emit2(0x1B, 0xCA);   /* SBB CX, DX   (CX - DX - CF → sets SF/OF) */
}

/* Emit 32-bit signed comparison CX:BX op DX:AX, result in AX (0 or 1).
   op_eq=1 → equality; op_neq=1 → inequality; otherwise ordered.
   hi_jcc_true: signed Jcc (JL/JG/JLE/JGE) that fires when hi-word alone decides TRUE.
   hi_jcc_false: signed Jcc that fires when hi-word alone decides FALSE.
   lo_jcc_true: unsigned Jcc (JB/JA/JBE/JAE) for lo-word when hi-words are equal. */
void cg_cmp32_bool(int hi_jcc_true, int hi_jcc_false, int lo_jcc_true) {
    /* Layout (all short jumps, relative to end of jump instruction):
       [0] 3B C8       CMP CX, DX          ; hi-word signed compare
       [2] 7x 0A       Jhi_true → .true    ; (10 bytes ahead: to MOV AX,1)
       [4] 7x 04       Jhi_false → .false  ; (4 bytes ahead: to XOR AX,AX)
       [6] 3B D8       CMP BX, AX          ; lo-word unsigned compare
       [8] 7x 04       Jlo_true → .true    ; (4 bytes ahead: to MOV AX,1)
      [10] 33 C0       .false: XOR AX,AX
      [12] EB 03       JMP .end
      [14] B8 01 00    .true: MOV AX, 1
      [17] (.end)
    */
    cg_emit2(0x3B, 0xCA);              /* CMP CX, DX */
    cg_emit2((uint8_t)hi_jcc_true, 10); /* Jhi_true → .true (distance to MOV AX,1) */
    cg_emit2((uint8_t)hi_jcc_false, 4); /* Jhi_false → .false (distance to XOR AX,AX) */
    cg_emit2(0x3B, 0xD8);              /* CMP BX, AX */
    cg_emit2((uint8_t)lo_jcc_true, 4); /* Jlo_true → .true */
    cg_emit2(0x33, 0xC0);              /* .false: XOR AX, AX */
    cg_emit2(0xEB, 0x03);              /* JMP .end */
    cg_emit1(0xB8); cg_emitw(0x0001); /* .true: MOV AX, 1 */
    /* .end: */
}

/* Equality: AX=1 if CX:BX == DX:AX.
   XOR hi words; if nonzero → not equal. XOR lo words; OR with hi; JNZ → not equal. */
void cg_cmp32_eq(void) {
    /* [0] 33 C8    XOR CX, DX
       [2] 33 D8    XOR BX, AX (wait: need BX-based; use XOR BX,AX but AX is RHS)
       Actually: CX^DX in CX, BX^AX in BX; OR CX,BX; test for zero */
    cg_emit2(0x33, 0xCA);   /* XOR CX, DX  (hi: CX ^= DX) */
    cg_emit2(0x33, 0xD8);   /* XOR BX, AX  (lo: BX ^= AX) */
    cg_emit2(0x0B, 0xCB);   /* OR CX, BX */
    /* now CX=0 iff equal; setcc using JZ (equal → true) */
    cg_setcc(OP_JZ);        /* JZ → AX=1 (equal); else AX=0 */
}

/* Inequality: AX=1 if CX:BX != DX:AX. */
void cg_cmp32_neq(void) {
    cg_emit2(0x33, 0xCA);   /* XOR CX, DX */
    cg_emit2(0x33, 0xD8);   /* XOR BX, AX */
    cg_emit2(0x0B, 0xCB);   /* OR CX, BX */
    cg_setcc(OP_JNZ);       /* JNZ → AX=1 (not equal); else AX=0 */
}

/* ADD BX, imm  — advance BX by a constant field/element offset */
void cg_add_bx_imm(uint16_t imm) {
    if (imm == 0) return;
    if (imm <= 127) {
        cg_emit2(0x83, 0xC3); cg_emit1(imm & 0xFF);  /* ADD BX, imm8  */
    } else {
        cg_emit2(0x81, 0xC3); cg_emitw(imm);          /* ADD BX, imm16 */
    }
}

/* TEST BX, BX  — used for far NIL check (offset part) */
void cg_test_bx(void) { cg_emit2(0x85, 0xDB); }
void cg_load_imm(int16_t val) {
    if (val == 0) { cg_emit2(0x33, 0xC0); }   /* XOR AX,AX */
    else          { cg_emit1(0xB8); cg_emitw((uint16_t)val); }
}
void cg_load_imm_cx(int16_t val) { cg_emit1(0xB9); cg_emitw((uint16_t)val); }

/* ---- item helpers ---- */

/* cg_load_item — materialise item value into AX (or DX:AX for POINTER types).
   After call: item->mode = M_REG.
   For POINTER type: AX = offset, DX = segment.
   For dereferenced far ptr (is_ref=1, M_REG): ES:BX must already be set;
     reads word at ES:[BX] into AX (or byte for CHAR/BYTE elements). */
void cg_load_item(Item *item) {
    int is_ptr     = (item->type && (item->type->form == TF_POINTER ||
                                     item->type->form == TF_ADDRESS ||
                                     (item->type->form == TF_PROC && item->type->is_far)));
    int is_byte    = (item->type &&
                      (item->type->form == TF_CHAR || item->type->form == TF_BYTE));
    int is_long    = (item->type && item->type->form == TF_LONGINT);
    int is_real    = (item->type && item->type->form == TF_REAL);
    int is_lreal   = (item->type && item->type->form == TF_LONGREAL);
    /* If already in FPU ST(0), nothing to do */
    if (item->mode == M_FREG) { return; }
    switch (item->mode) {
    case M_CONST:
        if (is_real || is_lreal) {
            /* Real constant: value stored in data segment, load from there */
            /* item->adr holds the data segment offset where the bytes were emitted */
            if (is_real) cg_fld_real_const((uint16_t)item->adr);
            else         cg_fld_longreal_const((uint16_t)item->adr);
            item->mode = M_FREG; return;
        } else if (is_long) {
            cg_load_long_imm((int32_t)item->val);
        } else {
            cg_load_imm(item->val);
            if (is_ptr || (item->type && item->type->form == TF_NILTYPE)) {
                /* NIL = {offset=0, segment=0}; also zero DX for niltype constants */
                cg_emit2(0x33, 0xD2);  /* XOR DX, DX */
            }
        }
        break;
    case M_LOCAL:
        if (item->sl_hops > 0) {
            /* uplevel access via static link chain; traverse to outer frame's BP */
            if (is_real) {
                cg_sl_load_bx(item->sl_hops);
                cg_sl_fld_real_bx(item->adr);
                item->mode = M_FREG; return;
            } else if (is_lreal) {
                cg_sl_load_bx(item->sl_hops);
                cg_sl_fld_longreal_bx(item->adr);
                item->mode = M_FREG; return;
            } else if (item->is_ref) {
                /* VAR param in outer frame: load 4-byte far addr from SS:[outer_BP + adr],
                   then dereference through ES:BX. */
                cg_sl_load_bx(item->sl_hops);
                /* LES BX, SS:[BX+adr] — 8086 LES with BX-relative addressing */
                if (item->adr >= -128 && item->adr <= 127) {
                    cg_emit3(0xC4, 0x5F, (uint8_t)(item->adr & 0xFF));
                } else {
                    cg_emit2(0xC4, 0x9F); cg_emitw((uint16_t)item->adr);
                }
                if (is_byte) cg_deref_far_byte();
                else if (is_long || is_ptr) cg_deref_far_long();
                else cg_deref_far();
                item->is_ref = 0;
            } else if (is_long || is_ptr) {
                cg_sl_load_long_ax(item->sl_hops, item->adr);   /* DX:AX */
            } else {
                cg_sl_load_ax(item->sl_hops, item->adr);        /* AX */
            }
        } else if (is_real) {
            cg_fld_real_bp(item->adr);
            item->mode = M_FREG; return;
        } else if (is_lreal) {
            cg_fld_longreal_bp(item->adr);
            item->mode = M_FREG; return;
        } else if (item->is_ref) {
            /* VAR param or array element: 4-byte far address at [BP+adr] */
            cg_les_bx_bp(item->adr);
            if (is_byte) cg_deref_far_byte();
            else if (is_long) cg_deref_far_long();
            else cg_deref_far();
            item->is_ref = 0;
        } else if (is_ptr) {
            cg_load_ptr_bp(item->adr);   /* DX:AX = far ptr */
        } else if (is_long) {
            cg_load_long_bp(item->adr);  /* DX:AX = 32-bit LONGINT */
        } else if (is_byte) {
            cg_load_byte_bp(item->adr);  /* MOV AL,[BP+ofs]; XOR AH,AH */
        } else {
            cg_load_bp(item->adr);
        }
        break;
    case M_GLOBAL:
        if (is_real) {
            cg_fld_real_mem((uint16_t)item->adr);
            item->mode = M_FREG; return;
        } else if (is_lreal) {
            cg_fld_longreal_mem((uint16_t)item->adr);
            item->mode = M_FREG; return;
        }
        if (item->is_ref) {
            cg_les_bx_mem(item->adr);
            if (is_byte) cg_deref_far_byte();
            else if (is_long) cg_deref_far_long();
            else cg_deref_far();
            item->is_ref = 0;
        } else if (is_ptr) {
            cg_load_ptr_mem(item->adr);
        } else if (is_long) {
            cg_load_long_mem((uint16_t)item->adr);  /* DX:AX = 32-bit LONGINT */
        } else if (is_byte) {
            cg_load_byte_mem((uint16_t)item->adr);  /* MOV AL,[addr]; XOR AH,AH */
        } else {
            cg_load_mem(item->adr);
        }
        break;
    case M_REG:
        if (item->is_ref) {
            /* ES:BX already set up (by array indexing or ^ deref); deref to AX (or DX:AX) */
            if (is_real) {
                cg_fld_real_esbx();
                item->is_ref = 0;
                item->mode = M_FREG;
                return;
            } else if (is_lreal) {
                cg_fld_longreal_esbx();
                item->is_ref = 0;
                item->mode = M_FREG;
                return;
            } else if (is_byte) cg_deref_far_byte();
            else if (is_long || is_ptr) cg_deref_far_long();
            else cg_deref_far();
            item->is_ref = 0;
        }
        /* else: value already in AX */
        break;
    case M_PROC:
        /* Load proc address: FAR → DX:AX (segment:offset); NEAR → AX only.
           Use cg_load_code_addr so the linker adjusts the offset to be
           relative to the .om group's code segment start. */
        cg_load_code_addr((uint16_t)item->adr);  /* MOV AX, offset + RELOC */
        if (item->is_far) {
            cg_emit1(OP_PUSH_CS);                /* PUSH CS */
            cg_emit1(OP_POP_DX);                 /* POP DX  (DX = CS) */
        }
        break;
    default:
        fprintf(stderr, "cg_load_item: bad mode %d\n", item->mode);
    }
    item->mode = M_REG;
}

/* cg_store_item — store AX (or DX:AX for POINTER types) into item's location.
   For POINTER type destinations: AX = offset, DX = segment.
   For VAR param (is_ref=1, M_LOCAL/M_GLOBAL): loads far addr, stores AX through it.
   For M_REG, is_ref=1 (ES:BX set by ^ selector): stores AX through ES:BX. */
void cg_store_item(Item *item) {
    int is_ptr   = (item->type && (item->type->form == TF_POINTER ||
                                   item->type->form == TF_ADDRESS ||
                                   (item->type->form == TF_PROC && item->type->is_far)));
    int is_byte  = (item->type &&
                    (item->type->form == TF_CHAR || item->type->form == TF_BYTE));
    int is_long  = (item->type && item->type->form == TF_LONGINT);
    int is_real  = (item->type && item->type->form == TF_REAL);
    int is_lreal = (item->type && item->type->form == TF_LONGREAL);
    switch (item->mode) {
    case M_LOCAL:
        if (item->sl_hops > 0) {
            /* uplevel store via static link chain */
            if (is_real) {
                cg_sl_load_bx(item->sl_hops);
                cg_sl_fstp_real_bx(item->adr); return;
            }
            if (is_lreal) {
                cg_sl_load_bx(item->sl_hops);
                cg_sl_fstp_longreal_bx(item->adr); return;
            }
            if (is_long || is_ptr) {
                cg_sl_store_long_ax(item->sl_hops, item->adr);
            } else {
                cg_sl_store_ax(item->sl_hops, item->adr);
            }
        } else if (is_real) {
            cg_fstp_real_bp(item->adr); return;
        } else if (is_lreal) {
            cg_fstp_longreal_bp(item->adr); return;
        } else if (item->is_ref) {
            /* VAR param store: AX (or DX:AX for LONGINT/POINTER) = value;
               must reload far address (may clobber AX/DX) */
            if (is_long || is_ptr) {
                cg_emit1(0x52);             /* PUSH DX (hi/seg) */
                cg_emit1(OP_PUSH_AX);       /* PUSH AX (lo/ofs) */
                cg_les_bx_bp(item->adr);    /* ES:BX = far address (clobbers AX,DX) */
                cg_emit1(OP_POP_AX);        /* POP AX (lo/ofs)  */
                cg_emit1(0x5A);             /* POP DX (hi/seg)  */
                cg_store_deref_far_long();
            } else {
                cg_emit1(OP_PUSH_AX);           /* save value   */
                cg_les_bx_bp(item->adr);        /* ES:BX = far address */
                cg_emit1(OP_POP_AX);            /* restore value */
                if (is_byte) cg_store_deref_far_byte(); else cg_store_deref_far();
            }
        } else if (is_ptr) {
            cg_store_ptr_bp(item->adr);     /* DX:AX → [BP+ofs] */
        } else if (is_long) {
            cg_store_long_bp(item->adr);    /* DX:AX → [BP+ofs..ofs+3] */
        } else if (is_byte) {
            cg_store_byte_bp(item->adr);    /* MOV [BP+ofs], AL */
        } else {
            cg_store_bp(item->adr);
        }
        break;
    case M_GLOBAL:
        if (is_real) { cg_fstp_real_mem((uint16_t)item->adr); return; }
        if (is_lreal){ cg_fstp_longreal_mem((uint16_t)item->adr); return; }
        if (item->is_ref) {
            if (is_long || is_ptr) {
                cg_emit1(0x52);             /* PUSH DX (hi/seg) */
                cg_emit1(OP_PUSH_AX);       /* PUSH AX (lo/ofs) */
                cg_les_bx_mem(item->adr);   /* ES:BX = far address */
                cg_emit1(OP_POP_AX);        /* POP AX (lo/ofs)  */
                cg_emit1(0x5A);             /* POP DX (hi/seg)  */
                cg_store_deref_far_long();
            } else {
                cg_emit1(OP_PUSH_AX);
                cg_les_bx_mem(item->adr);
                cg_emit1(OP_POP_AX);
                if (is_byte) cg_store_deref_far_byte(); else cg_store_deref_far();
            }
        } else if (is_ptr) {
            cg_store_ptr_mem(item->adr);
        } else if (is_long) {
            cg_store_long_mem((uint16_t)item->adr); /* DX:AX → [data_ofs..ofs+3] */
        } else if (is_byte) {
            cg_store_byte_mem((uint16_t)item->adr); /* MOV [addr], AL */
        } else {
            cg_store_mem(item->adr);
        }
        break;
    case M_REG:
        if (item->is_ref) {
            /* ES:BX set by ^ selector or array indexing; ST(0)/AX/DX:AX = value */
            if (is_real)  { cg_fstp_real_esbx(); return; }
            if (is_lreal) { cg_fstp_longreal_esbx(); return; }
            if (is_byte) cg_store_deref_far_byte();
            else if (is_long || is_ptr) cg_store_deref_far_long();
            else cg_store_deref_far();
        } else {
            fprintf(stderr, "cg_store_item: M_REG without is_ref\n");
        }
        break;
    default:
        fprintf(stderr, "cg_store_item: bad mode %d\n", item->mode);
    }
}

/* ---- arithmetic ---- */
void cg_add(void)        { cg_emit2(0x03, 0xC1); }  /* ADD AX, CX */
void cg_sub(void)        { cg_emit2(0x87,0xC1); cg_emit2(0x2B,0xC1); } /* XCHG; SUB */
void cg_mul(void)        { cg_emit2(0xF7, 0xE9); }  /* IMUL CX */
void cg_div(void)        { cg_emit2(0x87,0xC1); cg_emit1(0x99); cg_emit2(0xF7,0xF9); }
void cg_mod(void)        { cg_div(); cg_emit2(0x8B,0xC2); } /* ..then MOV AX,DX */
void cg_neg(void)        { cg_emit2(0xF7, 0xD8); }
void cg_not_bitwise(void){ cg_emit2(0xF7, 0xD0); }
void cg_and(void)        { cg_emit2(0x23, 0xC1); }
void cg_or(void)         { cg_emit2(0x0B, 0xC1); }
void cg_xor(void)        { cg_emit2(0x33, 0xC1); }
void cg_inc_ax(void)     { cg_emit1(0x40); }
void cg_dec_ax(void)     { cg_emit1(0x48); }
void cg_cmp(void)        { cg_emit2(0x3B, 0xC8); }  /* CMP CX, AX */
void cg_test_ax(void)    { cg_emit2(0x85, 0xC0); }
void cg_bool_not(void)   { cg_emit3(0x83, 0xF0, 0x01); } /* XOR AX, 1 */
/* CMP AX, imm16  —  sets flags based on AX - imm16 (signed) */
void cg_cmp_ax_imm(int16_t val) { cg_emit1(0x3D); cg_emitw((uint16_t)val); }

/* cg_ptr_cmp_eq: compare two far pointers for equality.
   Before call: LHS DX:AX already pushed as [seg, ofs] on stack (PUSH DX; PUSH AX).
   RHS DX:AX is in registers.
   After: ZF=1 iff LHS==RHS (both offset and segment equal).
   Clobbers: AX, CX, DX. */
void cg_ptr_cmp_eq(void) {
    /* Stack layout (top to bottom): LHS_ofs, LHS_seg.
       POP CX / XOR AX,CX / POP BX / XOR DX,BX / OR AX,DX / TEST AX,AX */
    cg_emit_code_str(10, "\x59\x33\xC1\x5B\x33\xD3\x0B\xC2\x85\xC0");
}

/* ---- shift/rotate (8086 real mode, 16-bit) ----
   On 8086 the only immediate shift is by 1 (D1 xx).
   For variable counts or counts > 1: load count into CL then use D3 xx form.
   AX holds the value to shift; count is passed as immediate or already in CX.

   cg_shl_imm(n)  -- SHL AX, n  (n must be 1..15, masked by caller)
   cg_shr_imm(n)  -- SHR AX, n  logical shift right
   cg_sar_imm(n)  -- SAR AX, n
   cg_ror_imm(n)  -- ROR AX, n
   cg_shl_cl()    -- SHL AX, CL (CL holds shift count, set by caller via MOV CL,<val>)
   cg_shr_cl()    -- SHR AX, CL
   cg_sar_cl()    -- SAR AX, CL
   cg_ror_cl()    -- ROR AX, CL
*/
void cg_shl_imm(int n) {
    /* 8086: only shift-by-1 has a dedicated encoding (D1 E0).
       For n > 1 load n into CL and use D3 form. */
    if (n == 1) {
        cg_emit2(0xD1, 0xE0); /* SHL AX, 1 */
    } else {
        cg_emit2(0xB1, (uint8_t)n); /* MOV CL, n */
        cg_emit2(0xD3, 0xE0);       /* SHL AX, CL */
    }
}
void cg_shr_imm(int n) {
    if (n == 1) {
        cg_emit2(0xD1, 0xE8); /* SHR AX, 1 */
    } else {
        cg_emit2(0xB1, (uint8_t)n); /* MOV CL, n */
        cg_emit2(0xD3, 0xE8);       /* SHR AX, CL */
    }
}
void cg_sar_imm(int n) {
    if (n == 1) {
        cg_emit2(0xD1, 0xF8); /* SAR AX, 1 */
    } else {
        cg_emit2(0xB1, (uint8_t)n); /* MOV CL, n */
        cg_emit2(0xD3, 0xF8);       /* SAR AX, CL */
    }
}
void cg_ror_imm(int n) {
    if (n == 1) {
        cg_emit2(0xD1, 0xC8); /* ROR AX, 1 */
    } else {
        cg_emit2(0xB1, (uint8_t)n); /* MOV CL, n */
        cg_emit2(0xD3, 0xC8);       /* ROR AX, CL */
    }
}
void cg_shl_cl(void) { cg_emit2(0xD3, 0xE0); } /* SHL AX, CL */
void cg_shr_cl(void) { cg_emit2(0xD3, 0xE8); } /* SHR AX, CL */
void cg_sar_cl(void) { cg_emit2(0xD3, 0xF8); } /* SAR AX, CL */
void cg_ror_cl(void) { cg_emit2(0xD3, 0xC8); } /* ROR AX, CL */

/* ---- 32-bit shifts: DX:AX (AX=lo, DX=hi) ---- */

/* Helper: emit shift-by-imm on a 16-bit reg. d1op = D1 form byte; d3op = D3 form byte; n must be 1..15. */
static void emit_shw(uint8_t d1op, uint8_t d3op, int n) {
    if (n == 1) { cg_emit2(0xD1, d1op); }
    else { cg_emit2(0xB1, (uint8_t)n); cg_emit2(0xD3, d3op); }
}

/* LSL32 by constant n (0..31): DX:AX <<= n (logical, zero-fill)
   Bits that overflow AX into DX: old_AX[n-1:0] go to DX[15:16-n].
   Uses BX as temp (saves/restores via PUSH/POP BX). */
void cg_lsl32_imm(int n) {
    n &= 0x1F;
    if (n == 0) return;
    if (n == 16) {
        cg_emit2(0x89, 0xC2); /* MOV DX, AX */
        cg_emit2(0x33, 0xC0); /* XOR AX, AX */
    } else if (n > 16) {
        cg_emit2(0x89, 0xC2); /* MOV DX, AX */
        cg_emit2(0x33, 0xC0); /* XOR AX, AX */
        emit_shw(0xE2, 0xE2, n - 16); /* SHL DX, (n-16) */
    } else {
        /* Use BX for overflow bits; CX only used as shift-count reg.
           PUSH BX; MOV BX,AX; SHL AX,n; SHR BX,(16-n); SHL DX,n; OR DX,BX; POP BX */
        cg_emit1(0x53);                /* PUSH BX */
        cg_emit2(0x89, 0xC3);          /* MOV BX, AX */
        emit_shw(0xE0, 0xE0, n);       /* SHL AX, n */
        emit_shw(0xEB, 0xEB, 16 - n);  /* SHR BX, (16-n): BX = overflow bits */
        emit_shw(0xE2, 0xE2, n);       /* SHL DX, n */
        cg_emit2(0x0B, 0xD3);          /* OR DX, BX */
        cg_emit1(0x5B);                /* POP BX */
    }
}

/* LSR32 by constant n (0..31): DX:AX >>= n (logical, zero-fill)
   Bits that overflow DX into AX: old_DX[n-1:0] go to AX[15:16-n].
   Uses BX as temp. */
void cg_lsr32_imm(int n) {
    n &= 0x1F;
    if (n == 0) return;
    if (n == 16) {
        cg_emit2(0x89, 0xD0); /* MOV AX, DX */
        cg_emit2(0x33, 0xD2); /* XOR DX, DX */
    } else if (n > 16) {
        cg_emit2(0x89, 0xD0); /* MOV AX, DX */
        cg_emit2(0x33, 0xD2); /* XOR DX, DX */
        emit_shw(0xE8, 0xE8, n - 16); /* SHR AX, (n-16) */
    } else {
        /* PUSH BX; MOV BX,DX; SHR DX,n; SHL BX,(16-n); SHR AX,n; OR AX,BX; POP BX */
        cg_emit1(0x53);                /* PUSH BX */
        cg_emit2(0x89, 0xD3);          /* MOV BX, DX */
        emit_shw(0xEA, 0xEA, n);       /* SHR DX, n */
        emit_shw(0xE3, 0xE3, 16 - n);  /* SHL BX, (16-n): BX = overflow bits */
        emit_shw(0xE8, 0xE8, n);       /* SHR AX, n */
        cg_emit2(0x0B, 0xC3);          /* OR AX, BX */
        cg_emit1(0x5B);                /* POP BX */
    }
}

/* ASR32 by constant n (0..31): DX:AX >>= n (arithmetic, sign-fill).
   Uses BX as temp. */
void cg_asr32_imm(int n) {
    n &= 0x1F;
    if (n == 0) return;
    if (n == 16) {
        cg_emit2(0x89, 0xD0); /* MOV AX, DX */
        /* SAR DX, 15 — fill DX with sign */
        cg_emit2(0xB1, 15); cg_emit2(0xD3, 0xFA);
    } else if (n > 16) {
        cg_emit2(0x89, 0xD0); /* MOV AX, DX */
        cg_emit2(0xB1, 15); cg_emit2(0xD3, 0xFA); /* SAR DX, 15 */
        emit_shw(0xF8, 0xF8, n - 16);  /* SAR AX, (n-16) */
    } else {
        /* Like LSR32 but SAR DX for arithmetic shift.
           PUSH BX; MOV BX,DX; SAR DX,n; SHL BX,(16-n); SHR AX,n; OR AX,BX; POP BX */
        cg_emit1(0x53);                /* PUSH BX */
        cg_emit2(0x89, 0xD3);          /* MOV BX, DX */
        emit_shw(0xFA, 0xFA, n);       /* SAR DX, n */
        emit_shw(0xE3, 0xE3, 16 - n);  /* SHL BX, (16-n): BX = sign bits for AX */
        emit_shw(0xE8, 0xE8, n);       /* SHR AX, n (logical: AX gets bits from DX below) */
        cg_emit2(0x0B, 0xC3);          /* OR AX, BX */
        cg_emit1(0x5B);                /* POP BX */
    }
}

/* ROR32 by constant n (0..31): DX:AX rotate right by n.
   old_AX[n-1:0] -> DX[15:16-n]; old_DX[n-1:0] -> AX[15:16-n].
   Uses BX as temp. */
void cg_ror32_imm(int n) {
    n &= 0x1F;
    if (n == 0) return;
    if (n == 16) {
        cg_emit2(0x87, 0xC2); /* XCHG AX, DX */
    } else if (n > 16) {
        cg_emit2(0x87, 0xC2); /* XCHG AX, DX */
        cg_ror32_imm(n - 16);
    } else {
        /* PUSH BX
           MOV BX, AX         ; BX = old AX (wrap bits for DX)
           SHR AX, n
           SHL BX, (16-n)     ; BX = old_AX wrap bits at DX[15:16-n]
           PUSH DX            ; save old DX
           SHR DX, n
           OR DX, BX          ; DX |= old AX wrap bits
           POP BX             ; BX = old DX (wrap bits for AX)
           SHL BX, (16-n)     ; BX = old_DX wrap bits at AX[15:16-n]
           OR AX, BX
           POP BX             ; restore BX */
        cg_emit1(0x53);                  /* PUSH BX */
        cg_emit2(0x89, 0xC3);            /* MOV BX, AX */
        emit_shw(0xE8, 0xE8, n);         /* SHR AX, n */
        emit_shw(0xE3, 0xE3, 16 - n);    /* SHL BX, (16-n) */
        cg_emit1(0x52);                  /* PUSH DX */
        emit_shw(0xEA, 0xEA, n);         /* SHR DX, n */
        cg_emit2(0x0B, 0xD3);            /* OR DX, BX */
        cg_emit1(0x5B);                  /* POP BX (= old DX) */
        emit_shw(0xE3, 0xE3, 16 - n);    /* SHL BX, (16-n) */
        cg_emit2(0x0B, 0xC3);            /* OR AX, BX */
        cg_emit1(0x5B);                  /* POP BX */
    }
}

/* LSL32 by CL (0..31): DX:AX <<= CL using single-bit SHL/RCL loop.
   Loop body = SHL AX,1(2) + RCL DX,1(2) + LOOP(2) = 6 bytes.
   AND CX,1Fh (not CL) to also zero CH so LOOP uses correct count. */
void cg_lsl32_cl(void) {
    cg_emit2(0x83, 0xE1); cg_emit1(0x1F); /* AND CX, 1Fh  (zero CH too) (3 bytes) */
    cg_emit2(0xE3, 0x06);                  /* JCXZ +6 (skip loop body+LOOP) */
    cg_emit2(0xD1, 0xE0);                  /* SHL AX, 1  (2) */
    cg_emit2(0xD1, 0xD2);                  /* RCL DX, 1  (2) */
    cg_emit2(0xE2, (uint8_t)(-6 & 0xFF));  /* LOOP -6 (back to SHL AX) */
}

/* LSR32 by CL (0..31): DX:AX >>= CL (logical).
   Loop body = SHR DX,1(2) + RCR AX,1(2) + LOOP(2) = 6 bytes. */
void cg_lsr32_cl(void) {
    cg_emit2(0x83, 0xE1); cg_emit1(0x1F); /* AND CX, 1Fh */
    cg_emit2(0xE3, 0x06);                  /* JCXZ +6 */
    cg_emit2(0xD1, 0xEA);                  /* SHR DX, 1 */
    cg_emit2(0xD1, 0xD8);                  /* RCR AX, 1 */
    cg_emit2(0xE2, (uint8_t)(-6 & 0xFF));  /* LOOP -6 */
}

/* ASR32 by CL (0..31): DX:AX >>= CL (arithmetic).
   Loop body = SAR DX,1(2) + RCR AX,1(2) + LOOP(2) = 6 bytes. */
void cg_asr32_cl(void) {
    cg_emit2(0x83, 0xE1); cg_emit1(0x1F); /* AND CX, 1Fh */
    cg_emit2(0xE3, 0x06);                  /* JCXZ +6 */
    cg_emit2(0xD1, 0xFA);                  /* SAR DX, 1 */
    cg_emit2(0xD1, 0xD8);                  /* RCR AX, 1 */
    cg_emit2(0xE2, (uint8_t)(-6 & 0xFF));  /* LOOP -6 */
}

/* ROR32 by CL (0..31): DX:AX rotate right by CL.
   Per iteration: save AX bit0 in BX; SHR DX,1 (DX bit0->CF); RCR AX,1 (CF->AX bit15);
   if old AX bit0 was 1, set DX bit15 (OR DX, 8000h).
   Loop body (between JCXZ target and LOOP):
     PUSH BX(1) + MOV BX,AX(2) + AND BX,1(3) + SHR DX,1(2) + RCR AX,1(2) +
     JZ+3(2) + OR DX,8000h(4) + POP BX(1) + LOOP(2) = 19 bytes total.
   JCXZ must skip all 19 bytes. */
void cg_ror32_cl(void) {
    cg_emit2(0x83, 0xE1); cg_emit1(0x1F); /* AND CX, 1Fh */
    cg_emit2(0xE3, 0x13);                  /* JCXZ +19 */
    cg_emit1(0x53);                        /* PUSH BX        (1) */
    cg_emit2(0x89, 0xC3);                  /* MOV BX, AX     (2) */
    cg_emit2(0x83, 0xE3); cg_emit1(0x01); /* AND BX, 1      (3) */
    cg_emit2(0xD1, 0xEA);                  /* SHR DX, 1      (2) */
    cg_emit2(0xD1, 0xD8);                  /* RCR AX, 1      (2) */
    cg_emit2(0x74, 0x03);                  /* JZ +3          (2) */
    cg_emit3(0x81, 0xCA, 0x00); cg_emit1(0x80); /* OR DX, 8000h (4) */
    cg_emit1(0x5B);                        /* POP BX         (1) */
    cg_emit2(0xE2, (uint8_t)(-19 & 0xFF)); /* LOOP -19       (2) */
}


void cg_setcc(int jcc) {
    /* Jcc +4 / XOR AX,AX / JMP +3 / MOV AX,1  (10 bytes) */
    cg_emit2((uint8_t)jcc, 4);
    cg_emit2(0x33, 0xC0);
    cg_emit2(0xEB, 3);
    cg_emit1(0xB8); cg_emitw(1);
}

/* Scan the type descriptor pointed to by AX (DS-relative offset) for tag_id.
   Input: AX = descriptor offset in DS (from the object's tag word).
   Output: AX = 1 if tag_id found, 0 if not.
   Saves/restores SI; all other registers preserved.
   Code layout (27 bytes):
     +0  56           PUSH SI
     +1  89 C6        MOV SI, AX
     +3  8B 04        MOV AX, [SI]      <-- loop top
     +5  85 C0        TEST AX, AX
     +7  74 0A        JZ  false (+10 → +19)
     +9  3D lo hi     CMP AX, tag_id
    +12  74 09        JE  true  (+9  → +23)
    +14  83 C6 02     ADD SI, 2
    +17  EB F0        JMP loop  (-16 → +3)
    +19  33 C0        XOR AX, AX  <-- false
    +21  EB 03        JMP done  (+3  → +26)
    +23  B8 01 00     MOV AX, 1   <-- true
    +26  5E           POP SI
    +27              <-- done */
void cg_is_tag_scan(int tag_id) {
    cg_emit1(0x56);                    /* PUSH SI */
    cg_emit2(0x89, 0xC6);              /* MOV SI, AX */
    cg_emit2(0x8B, 0x04);              /* MOV AX, [SI]  ← loop top */
    cg_emit2(0x85, 0xC0);              /* TEST AX, AX */
    cg_emit2(0x74, 0x0A);              /* JZ  +10 (→ false at +19) */
    cg_emit1(0x3D); cg_emitw((uint16_t)tag_id); /* CMP AX, tag_id */
    cg_emit2(0x74, 0x09);              /* JE  +9  (→ true  at +23) */
    cg_emit3(0x83, 0xC6, 0x02);        /* ADD SI, 2 */
    cg_emit2(0xEB, 0xF0);              /* JMP -16 (→ loop top at +3) */
    cg_emit2(0x33, 0xC0);              /* false: XOR AX, AX */
    cg_emit2(0xEB, 0x03);              /* JMP +3  (→ POP SI at +26) */
    cg_emit1(0xB8); cg_emitw(0x0001); /* true:  MOV AX, 1 */
    cg_emit1(0x5E);                    /* POP SI */
    /* done: */
}

/* ---- runtime ---- */
void cg_new(uint16_t size_bytes, int new_import_id) {
    cg_load_imm(size_bytes);
    cg_emit1(OP_PUSH_AX);
    cg_call_near(new_import_id);
    /* callee (NEW_) does RET 2 */
}
void cg_trap(int code, int trap_import_id) {
    cg_load_imm(code);
    cg_emit1(OP_PUSH_AX);
    cg_call_near(trap_import_id);
}
void cg_nil_check(int trap_import_id) {
    /* Called after cg_les_bx_*: ES:BX holds the far pointer.
       NIL = offset 0 (segment 0 is never a valid heap segment in DOS).
       Uses cg_cond_near: forward jump must be 8086-safe (no 0F prefix). */
    Backpatch bp;
    cg_test_bx();                   /* TEST BX, BX  */
    cg_cond_near(OP_JNZ, &bp);     /* skip trap if BX != 0 (far forward jump) */
    cg_trap(1, trap_import_id);
    cg_patch_near(&bp);
}

/* ---- FPU (x87) helpers ---- */
/* All instructions are encoded as raw bytes.
   8087 instruction set (no 80287/387 extensions).
   Encoding reference:
     FLD  dword [BP+d8]  = D9 46 d8
     FLD  dword [BP+d16] = D9 86 lo hi
     FSTP dword [BP+d8]  = D9 5E d8
     FSTP dword [BP+d16] = D9 9E lo hi
     FLD  qword [BP+d8]  = DD 46 d8
     FLD  qword [BP+d16] = DD 86 lo hi
     FSTP qword [BP+d8]  = DD 5E d8
     FSTP qword [BP+d16] = DD 9E lo hi
     FLD  dword [DS:ofs] = D9 06 lo hi  (+ RELOC)
     FSTP dword [DS:ofs] = D9 1E lo hi  (+ RELOC)
     FLD  qword [DS:ofs] = DD 06 lo hi  (+ RELOC)
     FSTP qword [DS:ofs] = DD 1E lo hi  (+ RELOC)
     FADDP ST(1),ST      = DE C1
     FSUBP ST(1),ST      = DE E9   (ST(1) -= ST(0))
     FMULP ST(1),ST      = DE C9
     FDIVP ST(1),ST      = DE F9   (ST(1) /= ST(0))
     FCHS                = D9 E0
     FCOMPP              = DE D9   (compare ST(0),ST(1), pop both)
     FNSTSW AX           = DF E0   (store FPU status word to AX, no WAIT)
     SAHF                = 9E      (load AH into flags: C3->ZF, C2->PF, C0->CF)
     FISTP word [SS:SP]  = DF 1C   (store ST(0) as int16 to [SS:SP], pop)
     FISTP dword [SS:SP] = DB 1C   (store ST(0) as int32 to [SS:SP], pop)
     FSTCW [SS:SP]       = D9 3C   (store control word; D9 3C = FSTCW [SP])
     FLDCW [SS:SP]       = D9 2C   (load control word from [SP])
  Segment override for DS: DS default for [disp16] mod=00 rm=110.
  No segment override needed for DS globals. */

static void fld_store_bp(uint8_t op1, uint8_t op2_d8, uint8_t op2_d16, int32_t ofs) {
    if (ofs >= -128 && ofs <= 127) {
        cg_emit2(op1, op2_d8); cg_emit1((uint8_t)(ofs & 0xFF));
    } else {
        cg_emit2(op1, op2_d16); cg_emitw((uint16_t)(int16_t)ofs);
    }
}
static void fld_store_mem(uint8_t op1, uint8_t op2, uint16_t ofs) {
    uint16_t patch;
    cg_emit2(op1, op2);
    patch = cg_pc();
    cg_emitw(ofs);
    rdf_add_reloc(&cg_obj, SEG_CODE, patch, 2, SEG_DATA, 0);
}

void cg_fld_real_bp(int32_t ofs)       { fld_store_bp(0xD9, 0x46, 0x86, ofs); }
void cg_fstp_real_bp(int32_t ofs)      { fld_store_bp(0xD9, 0x5E, 0x9E, ofs); }
void cg_fld_longreal_bp(int32_t ofs)   { fld_store_bp(0xDD, 0x46, 0x86, ofs); }
void cg_fstp_longreal_bp(int32_t ofs)  { fld_store_bp(0xDD, 0x5E, 0x9E, ofs); }
void cg_fld_real_mem(uint16_t ofs)     { fld_store_mem(0xD9, 0x06, ofs); }
void cg_fstp_real_mem(uint16_t ofs)    { fld_store_mem(0xD9, 0x1E, ofs); }
void cg_fld_longreal_mem(uint16_t ofs) { fld_store_mem(0xDD, 0x06, ofs); }
void cg_fstp_longreal_mem(uint16_t ofs){ fld_store_mem(0xDD, 0x1E, ofs); }

/* FLD dword/qword from CS-relative data (constant stored in code segment as data) */
void cg_fld_real_const(uint16_t data_ofs) {
    /* D9 06 lo hi  — but we need a SEG_DATA reloc for the data segment.
       Constants are emitted into the data segment (cg_emit_data_*).
       So use the same reloc as mem variants. */
    fld_store_mem(0xD9, 0x06, data_ofs);
}
void cg_fld_longreal_const(uint16_t data_ofs) {
    fld_store_mem(0xDD, 0x06, data_ofs);
}

/* FLD/FSTP dword/qword via ES:BX (for array element and heap pointer dereference).
   ES prefix = 0x26.  BX-indirect: mod=00 rm=111 → ModRM=07 (load) or 1F (store). */
void cg_fld_real_esbx(void)    { cg_emit3(0x26, 0xD9, 0x07); } /* ES: FLD  dword [BX] */
void cg_fstp_real_esbx(void)   { cg_emit3(0x26, 0xD9, 0x1F); } /* ES: FSTP dword [BX] */
void cg_fld_longreal_esbx(void){ cg_emit3(0x26, 0xDD, 0x07); } /* ES: FLD  qword [BX] */
void cg_fstp_longreal_esbx(void){ cg_emit3(0x26, 0xDD, 0x1F); }/* ES: FSTP qword [BX] */

void cg_fadd(void) { cg_emit2(0xDE, 0xC1); }
void cg_fsub(void) { cg_emit2(0xDE, 0xE9); }
void cg_fmul(void) { cg_emit2(0xDE, 0xC9); }
void cg_fdiv(void) { cg_emit2(0xDE, 0xF9); }
void cg_fchs(void) { cg_emit2(0xD9, 0xE0); }

/* Save ST(0) onto the CPU stack (8 bytes, qword) so LHS can be preserved
   while RHS is computed.  Protocol:
     SUB SP, 8          — allocate 8 bytes
     FSTP qword [SS:SP] — store+pop ST(0) into those bytes  (DD 1C  mod=00 rm=100 = [SI+BX] ...)
   Actually [SS:SP] encoding:  mod=00, reg=011, rm=100 = no base+index → but rm=100 mod=00 is [SI+BX].
   Correct encoding for [SS:SP] without SIB (8086):
     On 8086, memory operand with rm=100 mod=00 = [SI+BX] — no SP-relative addressing!
   Alternative: use BP-relative after adjusting BP? No.
   Best 8086 approach: SUB SP,8; push SP into BX; use [BX] addressing.
     MOV BX, SP
     FSTP qword [SS:BX]  — DD 1F  (mod=00, reg=011, rm=111 = [BX]) with SS prefix
   Since data segment = SS in compact model, we can use:
     SS: FSTP qword [BX] = 36 DD 1F
   Load back:
     SS: FLD qword [BX]  = 36 DD 07 */
void cg_fpush(void) {
    /* SUB SP,8 / MOV BX,SP / SS: FSTP qword [BX] */
    cg_emit_code_str(8, "\x83\xEC\x08\x89\xE3\x36\xDD\x1F");
}
void cg_fpop(void) {
    /* MOV BX,SP / SS: FLD qword [BX] / ADD SP,8 */
    cg_emit_code_str(8, "\x89\xE3\x36\xDD\x07\x83\xC4\x08");
}

/* FISTP: truncate ST(0) to integer, store to [SS:BX].
   For FLOOR: set rounding mode to round-toward-neg-inf first. */
void cg_fist_ax(void) {
    /* SUB SP,2 / MOV BX,SP / SS:FISTP word [BX] / SS:MOV AX,[BX] / ADD SP,2 */
    cg_emit_code_str(14,
        "\x83\xEC\x02"     /* SUB SP, 2       */
        "\x89\xE3"         /* MOV BX, SP      */
        "\x36\xDF\x1F"     /* SS: FISTP [BX]  */
        "\x36\x8B\x07"     /* SS: MOV AX,[BX] */
        "\x83\xC4\x02");   /* ADD SP, 2       */
}

/* FLOOR: truncate toward -inf using FSTCW/FLDCW to set rounding mode,
   then FISTP word.  Saves/restores rounding mode.
   Allocate 4 bytes on stack: [SP]   = old CW, [SP+2] = new CW.
   SUB SP,4; MOV BX,SP
   FSTCW [SS:BX]                  ; save current control word
   MOV AX,[SS:BX]
   AND AX, 0xF3FF                 ; clear RC bits
   OR  AX, 0x0400                 ; RC = 01 (round toward -inf)
   MOV [SS:BX+2], AX
   FLDCW [SS:BX+2]                ; load new control word
   SUB SP,2; MOV BX,SP
   FISTP word [SS:BX]             ; truncate ST(0) toward -inf
   MOV AX,[SS:BX]
   ADD SP,2
   MOV BX,SP                     ; back to CW save area
   FLDCW [SS:BX]                  ; restore original control word
   ADD SP,4 */
void cg_floor_ax(void) {
    cg_emit_code_str(47,
        "\x83\xEC\x04"         /* SUB SP, 4            */
        "\x89\xE3"             /* MOV BX, SP           */
        "\x36\xD9\x3F"         /* SS: FSTCW [BX]       */
        "\x36\x8B\x07"         /* SS: MOV AX, [BX]     */
        "\x25\xFF\xF3"         /* AND AX, 0F3FFh       */
        "\x0D\x00\x04"         /* OR  AX, 0400h        */
        "\x36\x89\x47\x02"     /* SS: MOV [BX+2], AX   */
        "\x36\xD9\x6F\x02"     /* SS: FLDCW [BX+2]     */
        "\x83\xEC\x02"         /* SUB SP, 2            */
        "\x89\xE3"             /* MOV BX, SP           */
        "\x36\xDF\x1F"         /* SS: FISTP word [BX]  */
        "\x36\x8B\x07"         /* SS: MOV AX, [BX]     */
        "\x83\xC4\x02"         /* ADD SP, 2            */
        "\x89\xE3"             /* MOV BX, SP           */
        "\x36\xD9\x2F"         /* SS: FLDCW [BX]       */
        "\x83\xC4\x04");       /* ADD SP, 4            */
}

/* FPU compare: FCOMPP (DE D9) pops both, sets C3/C2/C0.
   FNSTSW AX (DF E0): AH has C3=bit6, C2=bit2, C0=bit0.
   SAHF (9E): AH → FLAGS: C3→ZF, C2→PF, C0→CF.
   For FPU: LHS was pushed first (ST(1)), RHS pushed last (ST(0)).
   FCOMP compares ST(0) with ST(1):
     FCOMPP compares ST(0) to ST(1): sets C0=1 if ST(0)<ST(1), C3=1 if equal.
   After FCOMPP+SAHF:
     CF=1, ZF=0, PF=0 → ST(0) < ST(1) (i.e. LHS > RHS since LHS is ST(1))
   Wait, need to be careful about ordering.
   We need LHS OP RHS. If LHS was fpush'd first:
     After fpush(LHS): LHS on CPU stack.
     Compute RHS → ST(0).
     fpop → ST(0)=RHS, ST(1)=LHS? No — fpop does FLD which pushes LHS onto FPU stack.
     After fpop: ST(0)=LHS, ST(1)=RHS.
   Then FCOMPP compares ST(0) vs ST(1) = LHS vs RHS:
     C0=1, ZF=0 → ST(0) < ST(1) → LHS < RHS → CF=1
     C3=1, ZF=1 → ST(0) = ST(1) → LHS = RHS
     C0=0, ZF=0 → ST(0) > ST(1) → LHS > RHS
   So after SAHF: CF corresponds to LHS<RHS, ZF to equality.
   Signed-like conditions for floats:
     LHS < RHS  → CF=1      → JB  (0x72)
     LHS <= RHS → CF=1||ZF=1 → JBE (0x76)
     LHS > RHS  → CF=0 && ZF=0 → JA  (0x77)
     LHS >= RHS → CF=0       → JAE (0x73)
     LHS = RHS  → ZF=1       → JZ  (0x74)
     LHS != RHS → ZF=0       → JNZ (0x75)

   IMPORTANT: C2 (PF) is set for unordered (NaN). We ignore NaN for now.
   jcc_true: the Jcc opcode to test after SAHF.  */
void cg_fcmp(int jcc_true) {
    cg_emit2(0xDE, 0xD9);  /* FCOMPP */
    cg_emit2(0xDF, 0xE0);  /* FNSTSW AX */
    cg_emit1(0x9E);         /* SAHF */
    cg_setcc(jcc_true);
}

/* ---- finish ---- */
void cg_finish(const char *filename) {
    FILE *f = fopen(filename, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", filename); return; }
    rdf_write(&cg_obj, f);
    fclose(f);
}
