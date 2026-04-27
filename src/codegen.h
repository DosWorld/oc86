#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdint.h>
#include "rdoff.h"
#include "symbols.h"

/* item addressing modes */
#define M_CONST   0
#define M_GLOBAL  1
#define M_LOCAL   2
#define M_REG     3    /* value already in AX */
#define M_PROC    4
#define M_IMPORT  5
#define M_SYSPROC 6    /* built-in system intrinsic (val = SP_xxx); never loaded directly */
#define M_FREG    7    /* value in FPU ST(0); used for REAL/LONGREAL expressions */

typedef struct {
    int       mode;
    TypeDesc *type;
    int32_t   val;      /* M_CONST: value */
    int32_t   adr;      /* M_GLOBAL: data seg ofs; M_LOCAL: BP ofs */
    int       rdoff_id; /* M_IMPORT: import seg id */
    int       is_ref;   /* 1 if VAR param (adr holds ptr address) */
    int       is_far;   /* 1 if local exported (FAR) proc: use PUSH CS + CALL NEAR */
    int       sl_hops;  /* >0: uplevel access via static link chain (cg_sl_* helpers) */
    int       typeless; /* 1 if typeless VAR param: slot holds far addr; value IS the addr */
    void     *fwd_sym;  /* Symbol* when M_PROC and sym->fwd_decl (unresolved FORWARD proc) */
} Item;

typedef struct {
    uint16_t offset;   /* where rel8/rel16 sits in code buf (0..64000) */
    uint8_t  width;    /* 1 or 2 */
} Backpatch;

extern ObjFile cg_obj;

void     cg_init(void);
void     cg_free(void);
void     cg_finish(const char *filename);
uint16_t cg_pc(void);
uint16_t cg_dpc(void);

/* raw emit */
void cg_emit1(uint8_t b);
void cg_emit2(uint8_t b0, uint8_t b1);
void cg_emit3(uint8_t b0, uint8_t b1, uint8_t b2);
void cg_emitw(uint16_t w);
void cg_emitd(uint32_t d);
/* Emit a fixed-size byte string into the code segment.
   Use for fixed multi-byte sequences with no embedded computed values.
   str is treated as raw bytes (not NUL-terminated); size must match exactly. */
void cg_emit_code_str(uint16_t size, const char *str);
void cg_emit_data_byte(uint8_t b);
void cg_emit_data_word(uint16_t w);
void cg_emit_data_zero(uint16_t n);
void cg_emit_data_string(const char *s);

/* frame */
void cg_prologue(uint16_t local_bytes);
void cg_epilogue(uint16_t arg_bytes);
void cg_epilogue_far(uint16_t arg_bytes);  /* RETF N or RETF for exported FAR procs */

/* calls */
void cg_call_near(int import_id);
void cg_call_local(uint16_t target_offset);
void cg_call_local_far(uint16_t target_offset); /* PUSH CS + CALL NEAR (intra-module FAR proc) */
void cg_call_proc_var_bp(int32_t ofs);   /* CALL FAR [BP+ofs]  — call through local proc var */
void cg_call_proc_var_mem(uint16_t ofs); /* CALL FAR [data_ofs] — call through global proc var */
void cg_call_proc_var_esbx(void);        /* CALL FAR ES:[BX]   — call through indexed proc var */
void cg_call_proc_var_near_bp(int32_t ofs);   /* CALL NEAR [BP+ofs] — call through NEAR local proc var */
void cg_call_proc_var_near_mem(uint16_t ofs); /* CALL NEAR [data_ofs] — call through NEAR global proc var */
void cg_call_far(int import_id);     /* 9A + 4-byte far placeholder + RELOC + SEGRELOC */

/* jumps */
int  invert_jcc(int opcode);               /* opcode^1; valid for 0x70-0x7F Jcc */
void cg_jmp_short(Backpatch *bp);          /* EB rel8  — backward only */
void cg_jmp_near(Backpatch *bp);           /* E9 rel16 — forward unconditional */
void cg_cond_short(int opcode, Backpatch *bp);  /* 7x rel8  — backward only */
void cg_cond_near(int opcode, Backpatch *bp);   /* inv+3 / E9 rel16 — ALL forward conditionals */
void cg_patch_short(Backpatch *bp);        /* patch EB/7x backward jump */
void cg_patch_near(Backpatch *bp);         /* patch E9/cond_near forward jump */
void cg_jmp_back(uint16_t target);         /* auto short/near backward unconditional */
void cg_cond_back(int opcode, uint16_t target); /* short backward conditional (ASSERT range) */

/* load/store */
void cg_load_bp(int32_t offset);
void cg_store_bp(int32_t offset);
void cg_load_byte_bp(int32_t offset);   /* MOV AL,[BP+ofs]; XOR AH,AH */
void cg_store_byte_bp(int32_t offset);  /* MOV [BP+ofs],AL */
void cg_load_addr_bp(int32_t offset);
void cg_load_mem(uint16_t data_offset);
void cg_store_mem(uint16_t data_offset);
void cg_load_byte_mem(uint16_t data_offset);   /* MOV AL,[addr]; XOR AH,AH */
void cg_store_byte_mem(uint16_t data_offset);  /* MOV [addr],AL */
void cg_load_addr_mem(uint16_t data_offset);
void cg_load_code_addr(uint16_t code_offset); /* MOV AX, code_ofs + CODE reloc */
void cg_deref(void);
void cg_store_deref(void);
void cg_load_imm(int16_t val);
void cg_load_imm_cx(int16_t val);

/* item helpers */
void cg_load_item(Item *item);
void cg_store_item(Item *item);

/* static link helpers (nested proc uplevel variable access)
   BX is used as the chain pointer; callers must preserve BX if needed.
   Convention: nested procs have a hidden static link at [BP+4] (outer BP).
   Multi-hop traversal: each hop follows [BX+4] (static link in outer frame).
   After cg_sl_load_bx(hops): BX = outer frame's BP at (hops) levels up.
   cg_sl_load_ax(hops, ofs): AX = [outer_BP + ofs]  (load uplevel var)
   cg_sl_store_ax(hops, ofs): [outer_BP + ofs] = AX  (store uplevel var)
   cg_sl_addr_ax(hops, ofs): AX = &outer_BP[ofs]     (address of uplevel var for VAR param) */
void cg_sl_load_bx(int hops);
void cg_sl_load_ax(int hops, int32_t ofs);
void cg_sl_store_ax(int hops, int32_t ofs);
void cg_sl_addr_ax(int hops, int32_t ofs);
/* 32-bit (LONGINT / POINTER) uplevel load/store via static link chain.
   After cg_sl_load_bx(hops): load/store DX:AX from SS:[BX+ofs] and SS:[BX+ofs+2]. */
void cg_sl_load_long_ax(int hops, int32_t ofs);   /* DX:AX = SS:[outer_BP+ofs+2]:SS:[outer_BP+ofs] */
void cg_sl_store_long_ax(int hops, int32_t ofs);  /* SS:[outer_BP+ofs]=AX, SS:[outer_BP+ofs+2]=DX  */
/* REAL/LONGREAL uplevel load/store: BX must already be set to outer_BP by caller. */
void cg_sl_fld_real_bx(int32_t ofs);    /* SS: FLD  dword [BX+ofs] */
void cg_sl_fstp_real_bx(int32_t ofs);   /* SS: FSTP dword [BX+ofs] */
void cg_sl_fld_longreal_bx(int32_t ofs);  /* SS: FLD  qword [BX+ofs] */
void cg_sl_fstp_longreal_bx(int32_t ofs); /* SS: FSTP qword [BX+ofs] */

/* arithmetic */
void cg_add(void);      /* AX := CX + AX */
void cg_sub(void);      /* AX := CX - AX */
void cg_mul(void);      /* AX := CX * AX */
void cg_div(void);      /* AX := CX DIV AX */
void cg_mod(void);      /* AX := CX MOD AX */
void cg_neg(void);
void cg_not_bitwise(void);
void cg_and(void);
void cg_or(void);
void cg_xor(void);
void cg_inc_ax(void);
void cg_dec_ax(void);
void cg_cmp(void);      /* CMP CX, AX */
void cg_cmp_ax_imm(int16_t val); /* CMP AX, imm16 */
void cg_test_ax(void);
void cg_setcc(int jcc_opcode);   /* materialise flag into AX (0/1) */
void cg_ptr_cmp_eq(void); /* compare two far pointers; sets ZF; use with JZ/JNZ */

/* boolean short-circuit helpers */
void cg_bool_not(void);   /* XOR AX, 1 */

/* shift / rotate (8086 real mode, 16-bit, value in AX) */
void cg_shl_imm(int n);   /* SHL AX, n  (n=1: D1 E0; n>1: MOV CL,n / D3 E0) */
void cg_shr_imm(int n);   /* SHR AX, n  (n=1: D1 E8; n>1: MOV CL,n / D3 E8) */
void cg_sar_imm(int n);   /* SAR AX, n  (n=1: D1 F8; n>1: MOV CL,n / D3 F8) */
void cg_ror_imm(int n);   /* ROR AX, n  (n=1: D1 C8; n>1: MOV CL,n / D3 C8) */
void cg_shl_cl(void);     /* SHL AX, CL */
void cg_shr_cl(void);     /* SHR AX, CL */
void cg_sar_cl(void);     /* SAR AX, CL */
void cg_ror_cl(void);     /* ROR AX, CL */

/* 32-bit shifts: DX:AX (AX=lo, DX=hi) */
void cg_lsl32_imm(int n); /* DX:AX <<= n (logical, 0..31) */
void cg_lsr32_imm(int n); /* DX:AX >>= n (logical) */
void cg_asr32_imm(int n); /* DX:AX >>= n (arithmetic) */
void cg_ror32_imm(int n); /* DX:AX rotate right by n */
void cg_lsl32_cl(void);   /* DX:AX <<= CL (logical, CL masked to 0..31) */
void cg_lsr32_cl(void);   /* DX:AX >>= CL (logical) */
void cg_asr32_cl(void);   /* DX:AX >>= CL (arithmetic) */
void cg_ror32_cl(void);   /* DX:AX rotate right by CL */

/* far pointer helpers */
void cg_store_word_esbx_imm(uint16_t val); /* ES: MOV WORD PTR [BX], imm16 */
void cg_load_tag_far(void);                /* AX = ES:[BX-2]  (type tag before object) */
void cg_is_tag_scan(int tag_id);           /* scan DS:[AX] descriptor for tag_id; AX=0/1 */
void cg_is_tag_check(Item *item, int tag_id); /* load item, convert to ES:BX, load tag, scan */

/* runtime */
void cg_new(uint16_t size_bytes, int new_import_id);
void cg_trap(int code, int trap_import_id);
void cg_nil_check(int trap_import_id);

/* opcodes exported for convenience */
#define OP_CALL_NEAR  0xE8
#define OP_JMP_NEAR   0xE9
#define OP_JMP_SHORT  0xEB
#define OP_JZ         0x74
#define OP_JNZ        0x75
#define OP_JL         0x7C
#define OP_JGE        0x7D
#define OP_JLE        0x7E
#define OP_JG         0x7F
#define OP_RET        0xC3
#define OP_RET_N      0xC2
#define OP_RETF       0xCB   /* far return (no cleanup) */
#define OP_RETF_N     0xCA   /* far return with imm16 cleanup */
#define OP_CALL_FAR   0x9A   /* far call: 9A ofs(2) seg(2) */
#define OP_INT        0xCD
#define OP_PUSH_AX    0x50
#define OP_POP_AX     0x58
#define OP_POP_CX     0x59
#define OP_PUSH_BX    0x53
#define OP_POP_BX     0x5B
#define OP_PUSH_DX    0x52
#define OP_POP_DX     0x5A
#define OP_PUSH_CS    0x0E  /* PUSH CS */
#define OP_CLD        0xFC  /* CLD — clear direction flag (DF=0, forward REP) */
#define OP_PUSH_SS    0x16  /* PUSH SS — used for VAR param addr (data and stack share SS) */
#define OP_PUSH_DS    0x1E  /* PUSH DS */
#define OP_PUSH_ES    0x06  /* PUSH ES */
#define OP_POP_ES     0x07  /* POP  ES */
#define OP_LES        0xC4  /* LES r16, m32 */
#define OP_ES_PFX     0x26  /* ES segment override prefix */
#define OP_MOV_ES_DX  0xC2  /* second byte of:  8E C2 = MOV ES, DX */

/* FPU (x87) helpers — value lives in ST(0).
   REAL (TF_REAL) is 4 bytes (float); LONGREAL (TF_LONGREAL) is 8 bytes (double).
   All FPU helpers use the 8087 instruction set (no 80287/387 extensions).
   Calling convention for FPU type arguments: passed on CPU stack as raw bytes;
   return value in ST(0). */
void cg_fld_real_bp(int32_t ofs);       /* FLD dword [BP+ofs]    — load REAL local */
void cg_fstp_real_bp(int32_t ofs);      /* FSTP dword [BP+ofs]   — store REAL local */
void cg_fld_real_mem(uint16_t ofs);     /* FLD dword [DS:ofs]    — load REAL global */
void cg_fstp_real_mem(uint16_t ofs);    /* FSTP dword [DS:ofs]   — store REAL global */
void cg_fld_longreal_bp(int32_t ofs);   /* FLD qword [BP+ofs]    — load LONGREAL local */
void cg_fstp_longreal_bp(int32_t ofs);  /* FSTP qword [BP+ofs]   — store LONGREAL local */
void cg_fld_longreal_mem(uint16_t ofs); /* FLD qword [DS:ofs]    — load LONGREAL global */
void cg_fstp_longreal_mem(uint16_t ofs);/* FSTP qword [DS:ofs]   — store LONGREAL global */
void cg_fld_real_const(uint16_t data_ofs);     /* FLD dword [CS:data_ofs] — load REAL constant from data seg */
void cg_fld_longreal_const(uint16_t data_ofs); /* FLD qword [CS:data_ofs] — load LONGREAL constant */
/* FPU load/store via ES:BX (array element / heap pointer dereference) */
void cg_fld_real_esbx(void);     /* ES: FLD  dword [BX] */
void cg_fstp_real_esbx(void);    /* ES: FSTP dword [BX] */
void cg_fld_longreal_esbx(void); /* ES: FLD  qword [BX] */
void cg_fstp_longreal_esbx(void);/* ES: FSTP qword [BX] */
/* FPU arithmetic: both operands must already be on stack (ST(1) op= ST(0), pop) */
void cg_fadd(void);  /* FADDP ST(1),ST — ST(1) += ST(0), pop */
void cg_fsub(void);  /* FSUBP ST(1),ST — ST(1) -= ST(0), pop  (lhs - rhs) */
void cg_fmul(void);  /* FMULP ST(1),ST — ST(1) *= ST(0), pop */
void cg_fdiv(void);  /* FDIVP ST(1),ST — ST(1) /= ST(0), pop  (lhs / rhs) */
void cg_fchs(void);  /* FCHS — ST(0) = -ST(0) */
/* FPU compare: pops both operands, result in AX (0 or 1) */
void cg_fcmp(int jcc_true); /* FCOMPP + FNSTSW AX + SAHF; then setcc(jcc_true) */
/* FPU push ST(0) copy for binary ops: PUSH via FST to temp on stack */
void cg_fpush(void); /* allocate 8 bytes on CPU stack, FSTP qword [SS:SP] (save ST(0)) */
void cg_fpop(void);  /* FLD qword [SS:SP], ADD SP,8 (restore ST(0) from CPU stack) */
/* FPU conversion: ST(0) → integer (FIST/FISTP) */
void cg_fist_ax(void);  /* FISTP word [SS:SP] → MOV AX,[SS:SP] (truncate ST(0) to 16-bit int) */
void cg_floor_ax(void); /* FRNDINT (round toward -inf via FSTCW/FLDCW) then FIST → AX */

/* LONGINT (32-bit signed) helpers — value lives in DX:AX (DX=high, AX=low).
   For local vars: stored as 4 bytes at [BP+ofs..BP+ofs+3] (lo-word first).
   For global vars: stored as 4 bytes at [data_ofs..data_ofs+3] (lo-word first).
   Stack protocol for runtime calls (SYSTEM_S32MUL/SYSTEM_S32DIV/SYSTEM_S32MOD):
     push DX (high), push AX (low)  — then push b high, push b low before call.
   Result returned in DX:AX. */
void cg_load_long_bp(int32_t ofs);     /* DX:AX = [BP+ofs+2]:[BP+ofs] */
void cg_store_long_bp(int32_t ofs);    /* [BP+ofs]=[AX], [BP+ofs+2]=[DX] */
void cg_load_long_mem(uint16_t ofs);   /* DX:AX from DS:[ofs] and DS:[ofs+2] */
void cg_store_long_mem(uint16_t ofs);  /* DS:[ofs]=AX, DS:[ofs+2]=DX */
void cg_load_long_imm(int32_t val);    /* AX=lo16, DX=hi16 immediate */
/* 32-bit inline arithmetic (DX:AX op CX:BX; result in DX:AX) */
void cg_add32(void);      /* DX:AX += CX:BX  (ADD AX,BX / ADC DX,CX) */
void cg_sub32(void);      /* DX:AX = CX:BX - DX:AX  */
void cg_neg32(void);      /* DX:AX = -DX:AX */
void cg_and32(void);      /* DX:AX &= CX:BX */
void cg_or32(void);       /* DX:AX |= CX:BX */
void cg_xor32(void);      /* DX:AX ^= CX:BX */
void cg_cmp32(void);      /* compare CX:BX with DX:AX; sets SF/OF (not ZF-correct for JG) */
void cg_cmp32_bool(int hi_jcc_true, int hi_jcc_false, int lo_jcc_true); /* full 32-bit compare → AX=0/1 */
void cg_cmp32_eq(void);   /* AX=1 if CX:BX == DX:AX */
void cg_cmp32_neq(void);  /* AX=1 if CX:BX != DX:AX */
/* push/pop 32-bit DX:AX pair (used before calling SYSTEM_S32MUL/SYSTEM_S32DIV/SYSTEM_S32MOD) */
void cg_push_dxax(void);  /* PUSH DX; PUSH AX */
void cg_pop_cxbx(void);   /* POP  BX; POP  CX  (load rhs before runtime call) */

/* far-pointer helpers
   Convention: a far pointer value lives in ES:BX (segment in ES, offset in BX).
   When passed as a 4-byte stack argument: PUSH DS/ES first, then PUSH BX/AX.
   In memory: {offset:word, segment:word} (little-endian, offset first). */
void cg_les_bx_bp(int32_t offset);       /* LES BX, [BP+offset]  */
void cg_les_bx_mem(uint16_t data_ofs);   /* LES BX, [data_ofs]+RELOC */
void cg_dxax_to_esbx(void);          /* MOV BX,AX ; MOV ES,DX  (far ptr DX:AX → ES:BX) */
void cg_deref_far(void);             /* MOV AX, ES:[BX]  */
void cg_deref_far_byte(void);        /* MOV AL, ES:[BX]; zero-extend (for CHAR/BYTE) */
void cg_store_deref_far(void);       /* MOV ES:[BX], AX  */
void cg_store_deref_far_byte(void);  /* MOV ES:[BX], AL  (byte store for CHAR/BYTE) */
void cg_load_ptr_bp(int32_t ofs);        /* load 4-byte ptr from [BP+ofs]: AX=offset, DX=segment */
void cg_store_ptr_bp(int32_t ofs);       /* store DX:AX as 4-byte ptr to [BP+ofs] */
void cg_load_ptr_mem(uint16_t ofs);      /* load 4-byte ptr from DS:[ofs]: AX=offset, DX=segment */
void cg_store_ptr_mem(uint16_t ofs);     /* store DX:AX as 4-byte ptr to DS:[ofs] */
void cg_add_bx_imm(uint16_t imm);        /* ADD BX, imm16  (field / element offset) */
void cg_test_bx(void);               /* TEST BX, BX  (far NIL check on offset) */

#endif
