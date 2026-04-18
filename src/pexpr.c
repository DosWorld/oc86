#include "pexpr.h"
#include "pstate.h"
#include "import.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* forward declaration — parse_system_intrinsic is defined after parse_factor */
static void parse_system_intrinsic(Item *item, int id);

/* ================================================================
   DESIGNATOR
   ================================================================ */
void parse_designator(Item *item) {
    int id;
    Symbol *sym;
    if (pe_sc->sym != T_IDENT) { pe_error("identifier expected"); return; }
    sym = sym_find(pe_sc->id);
    if (!sym) { pe_error("undefined identifier"); scanner_next(pe_sc); return; }
    scanner_next(pe_sc);

    item->type    = sym->type;
    item->is_ref  = 0;
    item->is_far  = 0;
    item->sl_hops = 0;
    item->typeless = 0;
    item->rdoff_id = sym->rdoff_id;

    switch (sym->kind) {
    case K_CONST:
        item->mode = M_CONST;
        item->val  = sym->val;
        item->adr  = sym->adr; /* needed for REAL/LONGREAL constants (data-seg offset) */
        break;
    case K_VAR:     item->mode = sym->level==0 ? M_GLOBAL : M_LOCAL;
                    item->adr  = sym->adr;
                    /* uplevel access: sym declared in outer proc scope */
                    if (sym->level > 0 && sym->level < top_scope->level)
                        item->sl_hops = top_scope->level - sym->level;
                    break;
    case K_PARAM:   item->mode=M_LOCAL;  item->adr=sym->adr;
                    if (sym->level > 0 && sym->level < top_scope->level)
                        item->sl_hops = top_scope->level - sym->level;
                    break;
    case K_VARPARAM:item->mode=M_LOCAL;  item->adr=sym->adr;
                    if (sym->typeless) {
                        /* Typeless VAR: the param slot holds a far address; the value
                           IS that address (ADDRESS type), not a reference through it. */
                        item->is_ref = 0;
                        item->typeless = 1;
                    } else {
                        item->is_ref = 1;
                    }
                    if (sym->level > 0 && sym->level < top_scope->level)
                        item->sl_hops = top_scope->level - sym->level;
                    break;
    case K_PROC:
                    if (sym->rdoff_id >= 0) {
                        /* EXTERNAL proc: treat as import */
                        item->mode    = M_IMPORT;
                        item->type    = sym->type;
                        item->rdoff_id = sym->rdoff_id;
                        item->is_far  = sym->type ? sym->type->is_far : 1;
                    } else {
                        item->mode    = M_PROC;
                        item->adr     = sym->code_ofs;
                        item->is_far  = sym->type ? sym->type->is_far : sym->exported;
                        item->val     = sym->level;
                    }
                    break;
    case K_IMPORT:
        /* Module alias — qualified access (Alias.SymName) is handled below.
           We store the alias Symbol pointer for the DOT branch. */
        item->mode     = M_IMPORT;
        item->type     = type_notype;   /* not a value until qualified */
        item->rdoff_id = -1;
        /* sym pointer is kept in local var 'sym' below */
        break;
    case K_SYSPROC: item->mode=M_SYSPROC; item->val=sym->sys_id;
                    item->type=type_notype; break;
    default: pe_error("not a value"); break;
    }

    /* selectors */
    while (pe_sc->sym==T_DOT || pe_sc->sym==T_LBRAK || pe_sc->sym==T_ARROW) {
        if (pe_sc->sym == T_DOT) {
            scanner_next(pe_sc);

            /* Qualified module access: Alias.SymName
               sym->mod_name holds the real module name (e.g. "Kernel").
               The exported name is  RealMod_SymName  (naming convention). */
            if (item->mode == M_IMPORT) {
                const char *real_mod;
                char short_id[NAME_LEN];
                char full_name[NAME_LEN*2];
                Symbol *def_sym;
                if (pe_sc->sym != T_IDENT) { pe_error("identifier expected after '.'"); return; }
                real_mod = sym->mod_name[0] ? sym->mod_name : sym->name;
                strncpy(short_id, pe_sc->id, NAME_LEN-1); short_id[NAME_LEN-1] = '\0';
                snprintf(full_name, sizeof(full_name), "%s_%s", real_mod, short_id);
                scanner_next(pe_sc);

                /* Check if the symbol was loaded from a .def file into scope.
                   def_read registers symbols under "alias.shortname" to avoid
                   collisions with local declarations in the importing module.
                   Fall back to plain short_id for SYSTEM universe pre-declarations
                   (SP_ADR, SP_PUT, etc.) which bypass def_read entirely. */
                {
                char def_key[NAME_LEN*2];
                snprintf(def_key, sizeof(def_key), "%s.%s", sym->name, short_id);
                def_sym = sym_find(def_key);
                if (!def_sym) def_sym = sym_find(short_id);
                }
                if (def_sym && def_sym->kind == K_CONST) {
                    /* constant from .def — return as M_CONST directly */
                    item->mode = M_CONST;
                    item->val  = def_sym->val;
                    item->type = def_sym->type;
                    continue;
                }
                if (def_sym && def_sym->kind == K_TYPE) {
                    /* type from .def — return as M_CONST with type pointer */
                    item->mode = M_CONST;
                    item->val  = 0;
                    item->type = def_sym->type;
                    continue;
                }
                if (def_sym && def_sym->kind == K_SYSPROC) {
                    /* compiler intrinsic (SYSTEM.ADR, SYSTEM.VAL, etc.) */
                    item->mode = M_SYSPROC;
                    item->val  = def_sym->sys_id;
                    item->type = type_notype;
                    continue;
                }

                /* find or add RDOFF import */
                id = pe_get_system_import(full_name);
                item->rdoff_id = id;
                /* Use type info from .def if available, else unknown proc */
                if (def_sym && def_sym->kind == K_PROC && def_sym->type) {
                    item->type = def_sym->type;
                    item->is_far = def_sym->type->is_far;
                } else {
                    item->type = type_new_proc(type_notype);
                    item->is_far = 1; /* cross-module calls are always FAR */
                }
                /* item->mode stays M_IMPORT */
                continue;
            }

            {
            Symbol *f;
            /* Oberon-07: ptr.field is shorthand for ptr^.field */
            if (item->type->form == TF_POINTER) {
                if (item->mode == M_LOCAL && !item->is_ref)  cg_les_bx_bp(item->adr);
                else if (item->mode == M_GLOBAL && !item->is_ref) cg_les_bx_mem(item->adr);
                else { cg_load_item(item); /* AX=ofs, DX=seg */
                       cg_emit2(0x89, 0xC3); /* MOV BX, AX */
                       cg_emit2(0x8E, 0xC2); /* MOV ES, DX */ }
                item->type = item->type->base;
                item->mode = M_REG;
                item->is_ref = 1;
            }
            if (item->type->form != TF_RECORD) { pe_error("not a record"); return; }
            f = sym_find_field(item->type, pe_sc->id);
            if (!f) { pe_error("unknown field"); scanner_next(pe_sc); return; }
            scanner_next(pe_sc);
            if (item->is_ref) {
                /* item holds a far address (VAR param or ^ deref).
                   Load far ptr into ES:BX if not already there, then
                   advance BX by field offset. */
                if (item->mode == M_LOCAL)       cg_les_bx_bp(item->adr);
                else if (item->mode == M_GLOBAL) cg_les_bx_mem(item->adr);
                /* else M_REG: ES:BX already set */
                cg_add_bx_imm(f->offset);
                item->mode = M_REG; /* is_ref stays 1 */
            } else if (item->mode == M_LOCAL)  { item->adr += f->offset; }
            else if (item->mode == M_GLOBAL)   { item->adr += f->offset; }
            else { /* complex: load base addr, add offset */
                cg_load_item(item);
                cg_load_imm_cx(f->offset);
                cg_add();
                item->mode = M_REG;
            }
            item->type = f->type;
            }
        } else if (pe_sc->sym == T_LBRAK) {
            TypeDesc *elem;
            Item idx;
            scanner_next(pe_sc);
            /* Oberon-07: p[i] is shorthand for p^[i] when p is a pointer */
            if (item->type->form == TF_POINTER) {
                if (item->mode == M_LOCAL && !item->is_ref)  cg_les_bx_bp(item->adr);
                else if (item->mode == M_GLOBAL && !item->is_ref) cg_les_bx_mem(item->adr);
                else { cg_load_item(item);
                       cg_emit2(0x89, 0xC3); /* MOV BX, AX */
                       cg_emit2(0x8E, 0xC2); /* MOV ES, DX */ }
                item->type = item->type->base;
                item->mode = M_REG;
                item->is_ref = 1;
            }
            if (item->type->form != TF_ARRAY) { pe_error("not an array"); }
            elem = item->type->elem;
            if (item->mode == M_REG && item->is_ref) {
                /* ES:BX points to array base (through far pointer);
                   compute index offset in AX, then ADD BX, AX */
                cg_emit1(OP_PUSH_BX);               /* save base offset */
                parse_expr(&idx);
                cg_load_item(&idx);                 /* AX = index */
                if (elem->size == 2)      { cg_emit1(0xD1); cg_emit1(0xE0); } /* SHL AX,1 */
                else if (elem->size > 2)  { cg_load_imm_cx(elem->size); cg_mul(); }
                /* AX = byte offset; POP CX = saved base; BX = base+offset */
                cg_emit1(OP_POP_CX);
                cg_emit2(0x03, 0xC1);               /* ADD AX, CX */
                cg_emit2(0x8B, 0xD8);               /* MOV BX, AX */
                /* ES unchanged; item stays M_REG / is_ref=1 */
            } else {
                /* Load array base into ES:BX.
                   Open-array formal (len=-1): [BP+adr] is a far ptr {ofs:2,seg:2}.
                   VAR array param (is_ref=1, any len): [BP+adr] is a far ptr to caller's array.
                   Both use LES BX,[BP+adr] to load the far pointer without dereferencing.
                   Global array (M_GLOBAL): DS-relative; set ES=DS.
                   Local (non-VAR) array (M_LOCAL, is_ref=0): SS-relative; set ES=SS. */
                if (item->mode == M_LOCAL && (item->is_ref || item->type->len < 0)) {
                    /* Open-array formal or VAR array param: far ptr at [BP+adr] */
                    cg_les_bx_bp(item->adr);             /* LES BX, [BP+adr] */
                } else if (item->mode == M_GLOBAL) {
                    cg_load_addr_mem(item->adr);         /* AX = DS offset */
                    cg_emit2(0x8C, 0xDA);                /* MOV DX, DS */
                    cg_emit2(0x8E, 0xC2);                /* MOV ES, DX  (ES = DS) */
                    cg_emit2(0x8B, 0xD8);                /* MOV BX, AX */
                } else if (item->mode == M_LOCAL) {
                    cg_load_addr_bp(item->adr);          /* AX = SS offset */
                    cg_emit2(0x8C, 0xD2);                /* MOV DX, SS */
                    cg_emit2(0x8E, 0xC2);                /* MOV ES, DX  (ES = SS) */
                    cg_emit2(0x8B, 0xD8);                /* MOV BX, AX */
                } else {
                    cg_load_item(item);                  /* DX:AX = far ptr */
                    cg_dxax_to_esbx();                   /* ES:BX = far ptr */
                }
                /* ES:BX = base of array; add index byte-offset */
                parse_expr(&idx);
                cg_load_item(&idx);
                if (elem->size == 2)      { cg_emit1(0xD1); cg_emit1(0xE0); }
                else if (elem->size > 2)  { cg_load_imm_cx(elem->size); cg_mul(); }
                cg_emit2(0x01, 0xC3);               /* ADD BX, AX  (BX = base+index) */
                item->mode = M_REG; item->is_ref = 1;
            }
            item->type = elem;
            /* Multi-dim index: mat[i, j] == mat[i][j] */
            while (pe_sc->sym == T_COMMA) {
                scanner_next(pe_sc);  /* consume comma */
                if (item->type->form != TF_ARRAY) { pe_error("too many index dimensions"); break; }
                elem = item->type->elem;
                if (item->mode == M_REG && item->is_ref) {
                    cg_emit1(OP_PUSH_BX);
                    parse_expr(&idx); cg_load_item(&idx);
                    if (elem->size == 2)     { cg_emit1(0xD1); cg_emit1(0xE0); }
                    else if (elem->size > 2) { cg_load_imm_cx(elem->size); cg_mul(); }
                    cg_emit1(OP_POP_CX);
                    cg_emit2(0x03, 0xC1);
                    cg_emit2(0x8B, 0xD8);
                } else {
                    parse_expr(&idx); cg_load_item(&idx);
                    if (elem->size == 2)     { cg_emit1(0xD1); cg_emit1(0xE0); }
                    else if (elem->size > 2) { cg_load_imm_cx(elem->size); cg_mul(); }
                    cg_emit2(0x01, 0xC3);
                    item->mode = M_REG; item->is_ref = 1;
                }
                item->type = elem;
            }
            pe_expect(T_RBRAK);
        } else { /* T_ARROW */
            scanner_next(pe_sc);
            if (item->type->form != TF_POINTER) { pe_error("not a pointer"); return; }
            /* Load far pointer into ES:BX */
            if (item->mode == M_LOCAL && !item->is_ref) {
                cg_les_bx_bp(item->adr);        /* LES BX, [BP+adr] */
            } else if (item->mode == M_GLOBAL && !item->is_ref) {
                cg_les_bx_mem(item->adr);       /* LES BX, [DS:adr] */
            } else {
                /* Complex (chained ^ or returned pointer): load as DX:AX then move */
                cg_load_item(item);             /* DX:AX = far ptr */
                cg_dxax_to_esbx();              /* ES:BX = far ptr */
            }
            /* NIL check would go here */
            item->type   = item->type->base;
            item->mode   = M_REG;
            item->is_ref = 1;  /* ES:BX = address of the dereferenced value */
        }
    }
}

/* ================================================================
   ACTUAL PARAMETERS  (pascal: push left to right)
   ================================================================ */
void parse_actual_params(TypeDesc *proc_type) {
    Symbol *formal;
    Item arg;
    /* Only check arg count when the proc type has declared parameters.
       Legacy .def procs have n_params==0 with params==NULL — treat as unknown. */
    int check_count = (proc_type->n_params > 0);
    pe_expect(T_LPAREN);
    formal = proc_type->params;
    while (pe_sc->sym != T_RPAREN && pe_sc->sym != T_EOF) {
        if (check_count && !formal) {
            pe_error("too many arguments");
            /* consume remainder to avoid cascade errors */
            while (pe_sc->sym != T_RPAREN && pe_sc->sym != T_EOF) scanner_next(pe_sc);
            break;
        }
        if (formal && formal->kind == K_VARPARAM) {
            /* VAR param: push far address {segment:2, offset:2} = 4 bytes.
               VAR open-array param: also push LEN = 6 bytes total.
               Layout (Pascal left-to-right; first push = deepest = highest [BP+adr+N]):
                 open-array VAR: PUSH LEN, PUSH segment, PUSH offset
                 other VAR:                PUSH segment, PUSH offset
               Large memory model: SS = stack segment (locals), DS = data segment (globals). */
            parse_designator(&arg);
            /* For VAR open-array: push LEN first (deepest) */
            if (formal->type->form == TF_ARRAY && formal->type->len < 0) {
                if (arg.type && arg.type->len >= 0) {
                    cg_load_imm((int16_t)arg.type->len);    /* fixed-size actual: compile-time LEN */
                } else if (arg.mode == M_LOCAL && arg.type && arg.type->len < 0) {
                    cg_load_bp((int32_t)(arg.adr + 4));     /* forward open-array formal's LEN */
                } else {
                    cg_load_imm(0);                         /* unknown fallback */
                }
                cg_emit1(OP_PUSH_AX);   /* PUSH LEN */
            }
            if (arg.typeless && arg.mode == M_LOCAL) {
                /* Forwarding a typeless VAR param: slot at [BP+adr] already holds the
                   far address {offset:2, segment:2}. Copy both words directly. */
                cg_load_bp((int32_t)(arg.adr + 2));  /* AX = segment word */
                cg_emit1(OP_PUSH_AX);                /* PUSH segment */
                cg_load_bp(arg.adr);                 /* AX = offset word */
                cg_emit1(OP_PUSH_AX);                /* PUSH offset */
            } else if (arg.sl_hops > 0) {
                cg_sl_addr_ax(arg.sl_hops, arg.adr);
                /* uplevel var lives in caller's stack frame → SS */
                cg_emit1(OP_PUSH_SS);    /* PUSH SS */
                cg_emit1(OP_PUSH_AX);    /* PUSH AX (offset) */
            } else if (arg.mode == M_LOCAL) {
                cg_load_addr_bp(arg.adr);
                /* stack local → SS */
                cg_emit1(OP_PUSH_SS);    /* PUSH SS  (segment, pushed first = higher addr) */
                cg_emit1(OP_PUSH_AX);    /* PUSH AX  (offset,  pushed last  = lower addr)  */
            } else if (arg.mode == M_GLOBAL) {
                cg_load_addr_mem(arg.adr);
                /* global data → DS */
                cg_emit1(OP_PUSH_DS);    /* PUSH DS  (data segment) */
                cg_emit1(OP_PUSH_AX);    /* PUSH AX  (offset) */
            } else {
                cg_load_item(&arg);
                /* fallback (e.g. already-loaded far ptr) — push DS */
                cg_emit1(OP_PUSH_DS);
                cg_emit1(OP_PUSH_AX);
            }
        } else {
            parse_expr(&arg);
            if (arg.type && arg.type->form == TF_ARRAY
                && formal && (formal->type->form == TF_CHAR
                              || formal->type->form == TF_BYTE)) {
                /* Single-char string "X" passed to CHAR/BYTE formal:
                   load the first byte from the data segment as the char value. */
                if (arg.mode == M_GLOBAL) {
                    cg_load_mem(arg.adr); /* AX = word at data[adr]; AL = first char */
                } else {
                    cg_load_item(&arg);   /* AX = address; fall back */
                }
                cg_emit3(0x25, 0xFF, 0x00); /* AND AX, 00FFh — zero-extend byte */
                cg_emit1(OP_PUSH_AX);       /* push char value as CHAR param */
            } else if (arg.type && arg.type->form == TF_ARRAY) {
                /* Non-VAR open-array param: pass as {far_ptr(4), LEN(2)} = 6 bytes.
                   Caller pushes in this order (Pascal left-to-right, first = deepest):
                     1. PUSH LEN   (2 bytes, pushed first → highest BP offset, [BP+adr+4])
                     2. PUSH seg   (2 bytes, segment of the far ptr, [BP+adr+2])
                     3. PUSH ofs   (2 bytes, offset  of the far ptr, [BP+adr+0])
                   Callee: LES BX,[BP+adr] loads far ptr; LEN(arr) reads [BP+adr+4].
                   LEN = number of elements.
                   For open-array formal forwarded: LEN is at [BP+adr+4] of the callee. */
                /* Step 1: push LEN (deepest) */
                if (arg.type->len >= 0) {
                    /* Fixed-size array: LEN known at compile time */
                    cg_load_imm((int16_t)arg.type->len);
                } else if (arg.mode == M_LOCAL) {
                    /* Open-array formal being forwarded: LEN is at [BP+adr+4] */
                    cg_load_bp((int32_t)(arg.adr + 4));
                } else {
                    /* Fallback: push 0 (unknown LEN) */
                    cg_load_imm(0);
                }
                cg_emit1(OP_PUSH_AX);   /* PUSH LEN */
                /* Step 2: push far ptr segment then offset */
                if (arg.mode == M_GLOBAL) {
                    /* global array in DS */
                    cg_emit1(OP_PUSH_DS);                /* PUSH DS (segment) */
                    cg_load_addr_mem(arg.adr);           /* AX = DS-relative offset */
                    cg_emit1(OP_PUSH_AX);                /* PUSH offset */
                } else if (arg.mode == M_LOCAL && arg.type->len >= 0) {
                    /* fixed local array in SS */
                    cg_emit1(OP_PUSH_SS);                /* PUSH SS (segment) */
                    cg_load_addr_bp(arg.adr);            /* AX = SS-relative offset (LEA) */
                    cg_emit1(OP_PUSH_AX);                /* PUSH offset */
                } else if (arg.mode == M_LOCAL) {
                    /* open-array formal being forwarded: far ptr at [BP+adr] */
                    cg_load_bp((int32_t)(arg.adr + 2));  /* AX = segment word */
                    cg_emit1(OP_PUSH_AX);                /* PUSH segment */
                    cg_load_bp(arg.adr);                 /* AX = offset word */
                    cg_emit1(OP_PUSH_AX);                /* PUSH offset */
                } else {
                    /* fallback: already a far ptr in DX:AX */
                    cg_load_item(&arg);
                    cg_emit1(0x52);                      /* PUSH DX (segment) */
                    cg_emit1(OP_PUSH_AX);                /* PUSH AX (offset) */
                }
            } else {
                /* Implicit INTEGER -> LONGINT coercion when formal expects LONGINT. */
                if (formal && formal->type &&
                    formal->type->form == TF_LONGINT &&
                    arg.type && arg.type->form != TF_LONGINT &&
                    arg.mode == M_CONST) {
                    arg.type = type_longint; /* promote constant to 32-bit */
                }
                cg_load_item(&arg);  /* AX = value (or DX:AX for POINTER/LONGINT; ST(0) for REAL) */
                /* REAL/LONGREAL: value in ST(0); push raw bytes onto CPU stack.
                   Use formal type (if known) to decide push size so that a REAL actual
                   passed to a LONGREAL formal is promoted to 8 bytes. */
                {
                if (arg.type && (arg.type->form == TF_REAL ||
                                 arg.type->form == TF_LONGREAL)) {
                    int push_longreal = (arg.type->form == TF_LONGREAL) ||
                                        (formal && formal->type &&
                                         formal->type->form == TF_LONGREAL);
                    if (push_longreal) {
                        /* SUB SP,8; MOV BX,SP; FSTP qword SS:[BX] */
                        cg_emit2(0x83, 0xEC); cg_emit1(0x08);
                        cg_emit2(0x89, 0xE3);
                        cg_emit2(0x36, 0xDD); cg_emit1(0x1F);
                    } else {
                        /* SUB SP,4; MOV BX,SP; FSTP dword SS:[BX] */
                        cg_emit2(0x83, 0xEC); cg_emit1(0x04);
                        cg_emit2(0x89, 0xE3);
                        cg_emit2(0x36, 0xD9); cg_emit1(0x1F);
                    }
                } else {
                    /* After load, sign-extend non-constant INTEGER to LONGINT if needed */
                    if (formal && formal->type && formal->type->form == TF_LONGINT &&
                        arg.type && arg.type->form != TF_LONGINT) {
                        cg_emit1(0x99); /* CWD: sign-extend AX -> DX:AX */
                    }
                    if (arg.type && (arg.type->form == TF_POINTER ||
                                     arg.type->form == TF_NILTYPE ||
                                     arg.type->form == TF_ADDRESS ||
                                     arg.type->form == TF_PROC) &&
                        (!formal || !formal->type ||
                         formal->type->form == TF_POINTER ||
                         formal->type->form == TF_ADDRESS ||
                         formal->type->form == TF_NILTYPE ||
                         (formal->type->form == TF_PROC && formal->type->is_far))) {
                        /* 4-byte far pointer: push segment only when formal also expects 4 bytes */
                        cg_emit1(OP_PUSH_DX);  /* PUSH DX (segment) */
                    } else if (arg.type && (arg.type->form == TF_LONGINT ||
                               (formal && formal->type && formal->type->form == TF_LONGINT))) {
                        /* 32-bit LONGINT: push hi word (DX) first, then lo word (AX) */
                        cg_emit1(OP_PUSH_DX);  /* PUSH DX (high word) */
                    }
                    cg_emit1(OP_PUSH_AX);
                }
                } /* end push_as_huge block */
            }
        }
        if (formal) formal = formal->next;
        if (pe_sc->sym == T_COMMA) scanner_next(pe_sc);
    }
    if (check_count && formal) pe_error("too few arguments");
    pe_expect(T_RPAREN);
}

/* ================================================================
   EXPRESSIONS
   ================================================================ */
static void parse_factor(Item *item) {
    item->is_ref = 0;
    item->is_far = 0;
    item->sl_hops = 0;
    switch (pe_sc->sym) {
    case T_INT:
        item->mode = M_CONST; item->val  = pe_sc->ival;
        /* Promote to LONGINT only when the value exceeds 16-bit range entirely.
           Values in [-32768..65535] fit in 16 bits (signed or unsigned) so stay INTEGER.
           Values outside that range (e.g. 80000000H, 7FFFFFFFH) require LONGINT. */
        if ((uint32_t)pe_sc->ival > (uint32_t)0xFFFF)
            item->type = type_longint;
        else
            item->type = type_integer;
        scanner_next(pe_sc); break;
    case T_REAL: {
        /* Store the 32-bit IEEE 754 float bytes in the data segment.
           item->adr = data segment offset where bytes were emitted. */
        float fv;
        uint32_t bits;
        pe_mod_uses_fpu = 1;
        fv = (float)pe_sc->rval;
        memcpy(&bits, &fv, 4);
        item->mode = M_CONST;
        item->adr  = (int32_t)cg_dpc();
        item->type = type_real;
        cg_emit_data_byte((uint8_t)(bits & 0xFF));
        cg_emit_data_byte((uint8_t)((bits >> 8) & 0xFF));
        cg_emit_data_byte((uint8_t)((bits >> 16) & 0xFF));
        cg_emit_data_byte((uint8_t)((bits >> 24) & 0xFF));
        scanner_next(pe_sc);
        break;
    }
    case T_CHAR:
        item->mode = M_CONST; item->val  = pe_sc->ival;
        item->type = type_char; scanner_next(pe_sc); break;
    case T_STRING: {
        int32_t slen = (int32_t)strlen(pe_sc->sval) + 1; /* include NUL */
        item->mode = M_GLOBAL; item->adr = cg_dpc();
        item->type = type_new_array(type_char, slen);
        cg_emit_data_string(pe_sc->sval); scanner_next(pe_sc); break;
    }
    case T_TRUE:
        item->mode=M_CONST; item->val=1; item->type=type_boolean;
        scanner_next(pe_sc); break;
    case T_FALSE:
        item->mode=M_CONST; item->val=0; item->type=type_boolean;
        scanner_next(pe_sc); break;
    case T_NIL:
        item->mode=M_CONST; item->val=0; item->type=type_niltype;
        scanner_next(pe_sc); break;
    case T_LBRACE: {
        /* SET literal: { [elem { , elem } ] }  where elem = expr [ .. expr ]
           Strategy: keep running accumulator on stack (PUSH 0 to start).
           For each element e: POP CX(accum), compute bit=1<<e into AX, OR AX,CX, PUSH AX.
           For range lo..hi: BX=lo, DX=hi loop; each iteration ORs bit into stack-top.
           Final: POP AX. */
        Item lo, hi;
        scanner_next(pe_sc);
        cg_emit2(0x33, 0xC0);                      /* XOR AX, AX */
        cg_emit1(OP_PUSH_AX);                      /* PUSH AX (0) — accumulator */
        while (pe_sc->sym != T_RBRACE) {
            if (pe_sc->sym == T_COMMA) scanner_next(pe_sc);
            parse_expr(&lo); cg_load_item(&lo);    /* AX = lo (element or range start) */
            if (pe_sc->sym == T_DOTDOT) {
                /* range lo..hi: loop from BX=lo to DX=hi, ORing each bit into accumulator
                   Note: write ranges with spaces (0 .. 7) not (0..7) — the scanner backs
                   off at the first dot after an integer literal and returns T_DOT. */
                Backpatch skip_bp;
                uint16_t loop_top;
                scanner_next(pe_sc);
                cg_emit1(OP_PUSH_AX);              /* PUSH lo */
                parse_expr(&hi); cg_load_item(&hi);/* AX = hi */
                cg_emit2(0x89, 0xC2);              /* MOV DX, AX  (hi) */
                cg_emit1(OP_POP_BX);               /* POP  BX     (lo) */
                cg_emit2(0x3B, 0xDA);              /* CMP BX, DX */
                cg_cond_near(OP_JG, &skip_bp);
                loop_top = cg_pc();
                cg_emit2(0x8A, 0xCB);              /* MOV CL, BL */
                cg_emit1(0xB8); cg_emitw(0x0001);  /* MOV AX, 1 */
                cg_shl_cl();                       /* SHL AX, CL */
                cg_emit1(OP_POP_CX);               /* POP CX (accumulator) */
                cg_or();                           /* OR AX, CX */
                cg_emit1(OP_PUSH_AX);              /* PUSH AX (updated accumulator) */
                cg_emit1(0x43);                    /* INC BX */
                cg_emit2(0x3B, 0xDA);              /* CMP BX, DX */
                cg_cond_back(OP_JLE, loop_top);
                cg_patch_near(&skip_bp);
            } else {
                /* single element: bit = 1 << lo; OR into accumulator */
                cg_emit2(0x8A, 0xC8);              /* MOV CL, AL */
                cg_emit1(0xB8); cg_emitw(0x0001);  /* MOV AX, 1 */
                cg_shl_cl();                       /* SHL AX, CL */
                cg_emit1(OP_POP_CX);               /* POP CX (accumulator) */
                cg_or();                           /* OR AX, CX */
                cg_emit1(OP_PUSH_AX);              /* PUSH AX (updated accumulator) */
            }
        }
        pe_expect(T_RBRACE);
        cg_emit1(OP_POP_AX);                       /* POP AX (final set value) */
        item->mode = M_REG;
        item->type = type_set;
        break;
    }
    case T_LPAREN:
        scanner_next(pe_sc); parse_expr(item); pe_expect(T_RPAREN); break;
    case T_NOT:
        scanner_next(pe_sc); parse_factor(item);
        cg_load_item(item); cg_bool_not();
        item->type=type_boolean; item->mode=M_REG; break;
    case T_IDENT: {
        Symbol *sym = sym_find(pe_sc->id);
        int id;
        Item arg;
        /* Type cast built-ins: INTEGER(x), BYTE(x), LONGINT(x)
           Detected as: K_TYPE symbol with castable form, followed by (.
           We must consume the ident before checking the next token. */
        if (sym && sym->kind == K_TYPE) {
            int form = sym->type ? sym->type->form : -1;
            if (form == TF_INTEGER || form == TF_BYTE || form == TF_LONGINT ||
                form == TF_REAL    || form == TF_LONGREAL) {
                scanner_next(pe_sc);  /* consume the type name */
                if (pe_sc->sym == T_LPAREN) {
                    int src_form;
                    scanner_next(pe_sc);   /* consume ( */
                    parse_expr(&arg); cg_load_item(&arg);
                    pe_expect(T_RPAREN);
                    src_form = arg.type ? arg.type->form : TF_INTEGER;
                    switch (form) {
                    case TF_INTEGER:
                        /* Truncate to 16-bit: AX already holds low word.
                           If LONGINT source: DX:AX; AX is the low 16-bit — no-op.
                           If REAL/LONGREAL source: FLOOR → AX */
                        if (src_form == TF_REAL || src_form == TF_LONGREAL) {
                            cg_fist_ax();  /* truncate toward zero: FISTP word */
                        }
                        item->type = type_integer;
                        item->mode = M_REG;
                        break;
                    case TF_BYTE:
                        /* Truncate to 8-bit: AND AX, 0xFF */
                        if (src_form == TF_REAL || src_form == TF_LONGREAL) {
                            cg_fist_ax();
                        }
                        cg_emit2(0x25, 0xFF); cg_emit1(0x00);  /* AND AX, 00FFh */
                        item->type = type_byte;
                        item->mode = M_REG;
                        break;
                    case TF_LONGINT:
                        /* Sign-extend to 32-bit if source is 16-bit */
                        if (src_form == TF_REAL || src_form == TF_LONGREAL) {
                            /* REAL/LONGREAL in ST(0): FISTP dword via 4-byte temp. */
                            cg_emit2(0x83, 0xEC); cg_emit1(0x04); /* SUB SP,4 */
                            cg_emit2(0x89, 0xE3);                   /* MOV BX,SP */
                            cg_emit2(0x36, 0xDB); cg_emit1(0x1F);  /* SS: FISTP dword [BX] */
                            cg_emit2(0x36, 0x8B); cg_emit1(0x07);  /* MOV AX,SS:[BX] */
                            cg_emit2(0x36, 0x8B); cg_emit1(0x57); cg_emit1(0x02); /* MOV DX,SS:[BX+2] */
                            cg_emit2(0x83, 0xC4); cg_emit1(0x04); /* ADD SP,4 */
                        } else if (src_form != TF_LONGINT) {
                            cg_emit1(0x99);   /* CWD: sign-extend AX → DX:AX */
                        }
                        /* If already LONGINT, no-op */
                        item->type = type_longint;
                        item->mode = M_REG;
                        break;
                    case TF_REAL:
                        /* Convert source to float ST(0) */
                        if (src_form == TF_LONGREAL) {
                            /* LONGREAL → REAL: already in ST(0); truncate via FSTP+FLD */
                            /* Actually just leave in ST(0) — FPU stores as extended anyway */
                        } else if (src_form == TF_LONGINT) {
                            /* 32-bit integer in DX:AX → push as dword, FILD dword [SP] */
                            cg_emit2(0x83, 0xEC); cg_emit1(0x04); /* SUB SP,4 */
                            cg_emit2(0x89, 0xE3);                   /* MOV BX,SP */
                            cg_emit2(0x36, 0x89); cg_emit1(0x07);  /* MOV SS:[BX],AX */
                            cg_emit2(0x36, 0x89); cg_emit1(0x57); cg_emit1(0x02); /* MOV SS:[BX+2],DX */
                            cg_emit2(0x36, 0xDB); cg_emit1(0x07);  /* FILD dword SS:[BX] */
                            cg_emit2(0x83, 0xC4); cg_emit1(0x04); /* ADD SP,4 */
                        } else {
                            /* 16-bit integer in AX → PUSH AX, FILD word [SS:SP], POP */
                            cg_emit2(0x83, 0xEC); cg_emit1(0x02); /* SUB SP,2 */
                            cg_emit2(0x89, 0xE3);                   /* MOV BX,SP */
                            cg_emit2(0x36, 0x89); cg_emit1(0x07);  /* MOV SS:[BX],AX */
                            cg_emit2(0x36, 0xDF); cg_emit1(0x07);  /* FILD word SS:[BX] */
                            cg_emit2(0x83, 0xC4); cg_emit1(0x02); /* ADD SP,2 */
                        }
                        pe_mod_uses_fpu = 1;
                        item->type = type_real;
                        item->mode = M_FREG;
                        break;
                    case TF_LONGREAL:
                        /* Convert source to double-precision float in ST(0) */
                        if (src_form == TF_REAL) {
                            /* already in ST(0) as extended precision — no-op */
                        } else if (src_form == TF_LONGINT) {
                            cg_emit2(0x83, 0xEC); cg_emit1(0x04);
                            cg_emit2(0x89, 0xE3);
                            cg_emit2(0x36, 0x89); cg_emit1(0x07);
                            cg_emit2(0x36, 0x89); cg_emit1(0x57); cg_emit1(0x02);
                            cg_emit2(0x36, 0xDB); cg_emit1(0x07);  /* FILD dword */
                            cg_emit2(0x83, 0xC4); cg_emit1(0x04);
                        } else {
                            cg_emit2(0x83, 0xEC); cg_emit1(0x02);
                            cg_emit2(0x89, 0xE3);
                            cg_emit2(0x36, 0x89); cg_emit1(0x07);
                            cg_emit2(0x36, 0xDF); cg_emit1(0x07);  /* FILD word */
                            cg_emit2(0x83, 0xC4); cg_emit1(0x02);
                        }
                        pe_mod_uses_fpu = 1;
                        item->type = type_longreal;
                        item->mode = M_FREG;
                        break;
                    default: break;
                    }
                    break;  /* break out of switch(pe_sc->sym) T_IDENT case */
                }
                /* Not a cast call — type names can't appear in expressions. */
                pe_error("type name not valid as expression");
                item->mode=M_CONST; item->val=0; item->type=type_integer;
                break;
            }
        }
        if (sym && sym->kind == K_SYSPROC) {
            /* built-in used as expression (ODD, ABS, ORD, CHR, FLOOR, LEN) */
            id = sym->sys_id; scanner_next(pe_sc);
            pe_expect(T_LPAREN);
            if (id == SP_LEN) {
                /* LEN needs the raw designator (not loaded into AX) to inspect mode/adr/type.
                   Open-array formal (len=-1): 6-byte slot {ofs:2, seg:2, LEN:2}.
                     LEN hidden param lives at [BP+adr+4].
                   Fixed-size array: LEN is the compile-time type->len constant. */
                parse_expr(&arg);
                pe_expect(T_RPAREN);
                if (arg.type && arg.type->form == TF_ARRAY) {
                    if (arg.type->len >= 0) {
                        cg_load_imm((int16_t)arg.type->len);
                    } else if (arg.mode == M_LOCAL) {
                        cg_load_bp((int32_t)(arg.adr + 4)); /* LEN at [BP+adr+4] */
                    } else {
                        cg_load_imm(0); /* unknown — fallback */
                    }
                } else {
                    cg_load_imm(0);
                }
                item->type = type_integer;
                item->mode = M_REG;
            } else {
            parse_expr(&arg); cg_load_item(&arg);
            pe_expect(T_RPAREN);
            switch (id) {
            case SP_ODD:   cg_emit2(0x83,0xE0); cg_emit1(1); /* AND AX,1 */
                           item->type=type_boolean; break;
            case SP_ABS:   { Backpatch bp; cg_test_ax();
                             cg_cond_near(0x79,&bp); cg_neg();  /* JNS — forward, use near */
                             cg_patch_near(&bp); item->type=type_integer; break; }
            case SP_ORD:   item->type=type_integer; break;
            case SP_CHR:   item->type=type_char;    break;
            case SP_FLOOR:
                /* FLOOR(x: REAL): truncate toward -infinity, return INTEGER */
                if (arg.type && (arg.type->form == TF_REAL ||
                                 arg.type->form == TF_LONGREAL)) {
                    cg_floor_ax();  /* ST(0) was loaded; result in AX */
                }
                /* else: integer input — AX already holds value */
                item->type=type_integer; break;
            default: pe_error("built-in not usable as expression"); break;
            }
            item->mode=M_REG;
            } /* end else (not SP_LEN) */
        } else {
            parse_designator(item);
            if (item->mode == M_SYSPROC && pe_sc->sym == T_LPAREN) {
                /* SYSTEM.xxx or other intrinsic used as expression */
                parse_system_intrinsic(item, item->val);
            } else if (item->type && item->type->form == TF_PROC &&
                       (item->mode == M_LOCAL || item->mode == M_GLOBAL ||
                        (item->mode == M_REG && item->is_ref)) &&
                       pe_sc->sym == T_LPAREN) {
                /* Call through proc variable as expression */
                TypeDesc *pt = item->type;
                int32_t var_adr = item->adr;
                int var_global  = (item->mode == M_GLOBAL);
                int var_ref     = (item->mode == M_REG && item->is_ref);
                if (var_ref) {
                    /* ES:BX points to proc var slot; save before args clobber BX.
                       Stack layout after save: ... [PUSH_BX] [PUSH_ES] [args...]
                       PUSH BX (53), PUSH ES (06) */
                    cg_emit1(0x53); /* PUSH BX */
                    cg_emit1(0x06); /* PUSH ES */
                    parse_actual_params(pt);
                    /* Reload ES:BX from saved slot at [SP + arg_size]:
                       MOV BX, SP; ADD BX, arg_size; SS:MOV CX,[BX]; SS:MOV BX,[BX+2]; MOV ES,CX */
                    cg_emit2(0x89, 0xE3);  /* MOV BX, SP */
                    if (pt->arg_size > 0) {
                        if (pt->arg_size <= 127) {
                            cg_emit2(0x83, 0xC3); cg_emit1((uint8_t)pt->arg_size); /* ADD BX, imm8 */
                        } else {
                            cg_emit2(0x81, 0xC3); cg_emitw((uint16_t)pt->arg_size); /* ADD BX, imm16 */
                        }
                    }
                    cg_emit3(0x36, 0x8B, 0x0F); /* SS: MOV CX, [BX] — load saved ES */
                    cg_emit3(0x36, 0x8B, 0x5F); cg_emit1(0x02); /* SS: MOV BX, [BX+2] — load saved BX */
                    cg_emit2(0x8E, 0xC1); /* MOV ES, CX */
                    cg_call_proc_var_esbx(); /* CALL FAR ES:[BX] */
                    cg_emit2(0x83, 0xC4); cg_emit1(0x04); /* ADD SP, 4 — remove saved BX/ES */
                } else {
                    parse_actual_params(pt);
                    if (pt->is_far) {
                        if (var_global) cg_call_proc_var_mem((uint16_t)var_adr);
                        else            cg_call_proc_var_bp(var_adr);
                    } else {
                        if (var_global) cg_call_proc_var_near_mem((uint16_t)var_adr);
                        else            cg_call_proc_var_near_bp(var_adr);
                    }
                }
                item->type = pt->ret_type;
                item->is_ref = 0;
                if (pt->ret_type && (pt->ret_type->form == TF_REAL ||
                                     pt->ret_type->form == TF_LONGREAL))
                    item->mode = M_FREG;
                else
                    item->mode = M_REG;
            } else if ((item->mode==M_PROC || item->mode==M_IMPORT)
                    && pe_sc->sym==T_LPAREN) {
                /* function call as expression */
                int rdoff_id = item->rdoff_id;
                uint16_t code_ofs = (uint16_t)item->adr;
                int is_import = item->mode==M_IMPORT;
                TypeDesc *pt  = item->type;
                parse_actual_params(pt);
                /* push static link after params for nested procs */
                if (!is_import && !item->is_far && pt && pt->has_sl)
                    pe_emit_push_static_link(item->val);
                if (is_import && item->is_far) cg_call_far(rdoff_id);
                else if (is_import)          cg_call_near(rdoff_id);
                else if (item->is_far)       cg_call_local_far(code_ofs);
                else                         cg_call_local(code_ofs);
                item->type = pt->ret_type;
                /* If return type is REAL/LONGREAL, result is in ST(0) not AX */
                if (pt->ret_type && (pt->ret_type->form == TF_REAL ||
                                     pt->ret_type->form == TF_LONGREAL))
                    item->mode = M_FREG;
                else
                    item->mode = M_REG;
            }
        }
        break;
    }
    default:
        pe_error("factor expected");
        item->mode=M_CONST; item->val=0; item->type=type_integer;
        break;
    }
}

/* Coerce integer in AX to FPU ST(0): push AX as word on SS stack, FILD, pop. */
static void coerce_ax_to_fpu(void) {
    cg_emit2(0x83, 0xEC); cg_emit1(0x02); /* SUB SP, 2 */
    cg_emit2(0x89, 0xE3);                  /* MOV BX, SP */
    cg_emit3(0x36, 0x89, 0x07);            /* SS: MOV [BX], AX */
    cg_emit3(0x36, 0xDF, 0x07);            /* SS: FILD word [BX] */
    cg_emit2(0x83, 0xC4); cg_emit1(0x02); /* ADD SP, 2 */
    pe_mod_uses_fpu = 1;
}

/* Coerce LONGINT in DX:AX to FPU ST(0): push dword on SS stack, FILD dword, pop. */
static void coerce_dxax_to_fpu(void) {
    cg_emit2(0x83, 0xEC); cg_emit1(0x04); /* SUB SP, 4 */
    cg_emit2(0x89, 0xE3);                  /* MOV BX, SP */
    cg_emit3(0x36, 0x89, 0x07);            /* SS: MOV [BX], AX    (lo word) */
    cg_emit3(0x36, 0x89, 0x57); cg_emit1(0x02); /* SS: MOV [BX+2], DX  (hi word) */
    cg_emit3(0x36, 0xDB, 0x07);            /* SS: FILD dword [BX] */
    cg_emit2(0x83, 0xC4); cg_emit1(0x04); /* ADD SP, 4 */
    pe_mod_uses_fpu = 1;
}

/* Return 1 if the item type is an integer scalar (INTEGER, BYTE, LONGINT). */
static int is_int_type(Item *it) {
    int f;
    if (!it->type) return 0;
    f = it->type->form;
    return (f == TF_INTEGER || f == TF_BYTE || f == TF_LONGINT);
}

/* Return 1 if the item type is a REAL or LONGREAL. */
static int is_fpu_type(Item *it) {
    int f;
    if (!it->type) return 0;
    f = it->type->form;
    return (f == TF_REAL || f == TF_LONGREAL);
}

static void parse_term(Item *item) {
    Item rhs;
    int is_long;
    int is_fpu;
    parse_factor(item);
    while (pe_sc->sym==T_STAR || pe_sc->sym==T_DIV ||
           pe_sc->sym==T_MOD  || pe_sc->sym==T_AND || pe_sc->sym==T_SLASH) {
        Token op = pe_sc->sym; scanner_next(pe_sc);
        cg_load_item(item);
        is_fpu  = (item->type && (item->type->form == TF_REAL ||
                                   item->type->form == TF_LONGREAL));
        is_long = (item->type && item->type->form == TF_LONGINT);
        if (item->type && item->type->form == TF_SET) {
            /* SET * (intersection=AND), SET / (symmetric diff=XOR) */
            cg_emit1(OP_PUSH_AX);
            parse_factor(&rhs); cg_load_item(&rhs);
            cg_emit1(OP_POP_CX);
            if      (op == T_STAR)  cg_and();
            else if (op == T_SLASH) cg_xor();
            else pe_error("operator not supported for SET in term");
            item->type = type_set; item->mode = M_REG;
            continue;
        }
        if (is_fpu) {
            /* REAL / LONGREAL LHS: save LHS on CPU stack, compute RHS, pop LHS */
            cg_fpush();
            parse_factor(&rhs); cg_load_item(&rhs);
            /* Implicit coercion: integer RHS → FPU */
            if (is_int_type(&rhs)) {
                if (rhs.type->form == TF_LONGINT) coerce_dxax_to_fpu();
                else                              coerce_ax_to_fpu();
            }
            cg_fpop();  /* ST(0)=LHS, ST(1)=RHS */
            switch (op) {
            case T_STAR:  cg_fmul(); break;
            case T_SLASH: cg_emit2(0xDE, 0xF1); break; /* FDIVRP */
            default: pe_error("operator not supported for REAL"); break;
            }
            item->type = (item->type && item->type->form == TF_LONGREAL) ? type_longreal : type_real;
            item->mode = M_FREG;
            continue;
        }
        if (is_long) {  /* 32-bit LONGINT */
            /* Save 32-bit LHS on stack: PUSH DX (hi), PUSH AX (lo) */
            cg_push_dxax();
        } else {
            cg_emit1(OP_PUSH_AX);   /* save 16-bit LHS */
        }
        parse_factor(&rhs); cg_load_item(&rhs);
        /* Implicit coercion: integer LHS, REAL/LONGREAL RHS → promote both to FPU */
        if (is_fpu_type(&rhs)) {
            /* RHS is now in ST(0).  Recover integer LHS from CPU stack into AX/DX:AX. */
            if (is_long) {
                /* POP AX (lo), POP DX (hi) — cg_push_dxax pushed DX then AX */
                cg_emit1(0x58); /* POP AX */
                cg_emit1(0x5A); /* POP DX */
                coerce_dxax_to_fpu(); /* ST(0)=LHS_float, ST(1)=RHS */
            } else {
                cg_emit1(0x58); /* POP AX */
                coerce_ax_to_fpu();   /* ST(0)=LHS_float, ST(1)=RHS */
            }
            /* Now ST(0)=LHS_float, ST(1)=RHS_float */
            switch (op) {
            case T_STAR:  cg_fmul(); break;
            case T_SLASH: cg_emit2(0xDE, 0xF1); break; /* FDIVRP */
            default: pe_error("operator not supported for mixed int/REAL"); break;
            }
            item->type = (rhs.type->form == TF_LONGREAL) ? type_longreal : type_real;
            item->mode = M_FREG;
            continue;
        }
        /* Check if RHS is LONGINT (LHS may have been widened) */
        if (!is_long && rhs.type && rhs.type->form == TF_LONGINT) {
            pe_error("mixed INTEGER/LONGINT: use explicit LONGINT() cast");
        }
        /* If LHS is LONGINT but RHS is not, sign-extend RHS AX → DX:AX */
        if (is_long && rhs.type && rhs.type->form != TF_LONGINT) {
            cg_emit1(0x99); /* CWD: sign-extend AX → DX:AX */
        }
        if (is_long) {
            /* RHS is in DX:AX; push it for runtime call or pop into CX:BX for inline */
            switch (op) {
            case T_STAR: {
                int id = pe_get_system_import("SYSTEM_S32MUL");
                /* Stack: lhs_hi, lhs_lo (pushed earlier).  RHS in DX:AX.
                   SYSTEM_S32MUL args (Pascal left→right): a_high, a_low, b_high, b_low
                   Caller pushes: a_high (LHS_hi), a_low (LHS_lo), b_high (RHS_hi), b_low (RHS_lo)
                   LHS already on stack (hi first, lo second from our cg_push_dxax which did PUSH DX, PUSH AX).
                   Wait: cg_push_dxax() emits PUSH DX then PUSH AX.
                   Stack (top→bottom after push): AX(lhs_lo), DX(lhs_hi).
                   We need args as: a_high, a_low, b_high, b_low pushed left-to-right.
                   From SYSTEM.ASM: a_high at [bp+8], a_low at [bp+6], b_high at [bp+4], b_low at [bp+2].
                   So caller pushes: a_high first (deepest), a_low, b_high, b_low last (top).
                   Our cg_push_dxax for LHS pushed DX(hi) then AX(lo) — stack: lo(top), hi(below).
                   That means lo is at lower address. On 8086 stack grows down. [BP+6]=a_low(top-pushed), [BP+8]=a_high.
                   cg_push_dxax = PUSH DX (hi first) then PUSH AX (lo).
                   After PUSH DX: hi at SP. After PUSH AX: lo at SP, hi at SP+2.
                   So [BP+6] = lo = a_low, [BP+8] = hi = a_high. Correct.
                   Now push RHS: PUSH DX (b_hi), PUSH AX (b_lo) */
                cg_push_dxax();  /* push RHS = b_high, b_low */
                cg_call_far(id);
                item->type = type_longint;
                break;
            }
            case T_DIV: {
                int id = pe_get_system_import("SYSTEM_S32DIV");
                cg_push_dxax();  /* push RHS = b_high, b_low */
                cg_call_far(id);
                item->type = type_longint;
                break;
            }
            case T_MOD: {
                int id = pe_get_system_import("SYSTEM_S32MOD");
                cg_push_dxax();  /* push RHS = b_high, b_low */
                cg_call_far(id);
                item->type = type_longint;
                break;
            }
            case T_AND: {
                /* Inline 32-bit AND: pop LHS into CX:BX, then AND DX:AX */
                cg_pop_cxbx();   /* POP BX (lhs_lo), POP CX (lhs_hi) */
                cg_and32();
                item->type = type_boolean;
                break;
            }
            default:
                pe_error("operator not supported for LONGINT in term");
                break;
            }
        } else {
            cg_emit1(OP_POP_CX);   /* restore 16-bit LHS into CX */
            switch (op) {
            case T_STAR: cg_mul(); break;
            case T_DIV:  cg_div(); break;
            case T_MOD:  cg_mod(); break;
            case T_AND:  cg_and(); item->type=type_boolean; break;
            case T_SLASH: pe_error("REAL not supported"); break;
            default: break;
            }
        }
        item->mode=M_REG;
    }
}

static void parse_simple_expr(Item *item) {
    int negate = 0;
    int is_long;
    int is_fpu;
    Item rhs;
    if      (pe_sc->sym==T_MINUS) { negate=1; scanner_next(pe_sc); }
    else if (pe_sc->sym==T_PLUS)  { scanner_next(pe_sc); }
    parse_term(item);
    if (negate) {
        if (item->mode == M_CONST &&
            !(item->type && (item->type->form == TF_REAL ||
                              item->type->form == TF_LONGREAL))) {
            item->val = -item->val; /* constant folding: no code emitted */
        } else {
            cg_load_item(item);
            if (item->type && item->type->form == TF_LONGINT) {
                cg_neg32();
                item->mode=M_REG;
            } else if (item->type && (item->type->form == TF_REAL ||
                                       item->type->form == TF_LONGREAL)) {
                cg_fchs();
                item->mode=M_FREG;
            } else {
                cg_neg();
                item->mode=M_REG;
            }
        }
    }
    while (pe_sc->sym==T_PLUS || pe_sc->sym==T_MINUS || pe_sc->sym==T_OR) {
        Token op = pe_sc->sym; scanner_next(pe_sc);
        cg_load_item(item);
        is_fpu  = (item->type && (item->type->form == TF_REAL ||
                                   item->type->form == TF_LONGREAL));
        is_long = (item->type && item->type->form == TF_LONGINT);
        if (item->type && item->type->form == TF_SET) {
            /* SET + (union=OR), SET - (difference=AND NOT) */
            cg_emit1(OP_PUSH_AX);
            parse_term(&rhs); cg_load_item(&rhs);
            cg_emit1(OP_POP_CX);
            if (op == T_PLUS) {
                cg_or();                           /* AX = AX | CX */
            } else if (op == T_MINUS) {
                cg_emit2(0xF7, 0xD0);              /* NOT AX  (complement RHS) */
                cg_and();                          /* AX = CX & ~RHS */
            } else {
                pe_error("operator not supported for SET in expr");
            }
            item->type = type_set; item->mode = M_REG;
            continue;
        }
        if (is_fpu) {
            cg_fpush();
            parse_term(&rhs); cg_load_item(&rhs);
            /* Implicit coercion: integer RHS → FPU */
            if (is_int_type(&rhs)) {
                if (rhs.type->form == TF_LONGINT) coerce_dxax_to_fpu();
                else                              coerce_ax_to_fpu();
            }
            cg_fpop();  /* ST(0)=LHS, ST(1)=RHS */
            switch (op) {
            case T_PLUS:  cg_fadd(); break;
            case T_MINUS: cg_emit2(0xDE, 0xE1); break; /* FSUBRP: LHS-RHS */
            default: pe_error("operator not supported for REAL"); break;
            }
            item->mode = M_FREG;
            continue;
        }
        if (is_long) {
            cg_push_dxax();          /* save 32-bit LHS */
        } else {
            cg_emit1(OP_PUSH_AX);   /* save 16-bit LHS */
        }
        parse_term(&rhs); cg_load_item(&rhs);
        /* Implicit coercion: integer LHS, REAL/LONGREAL RHS → promote both to FPU */
        if (is_fpu_type(&rhs)) {
            if (is_long) {
                cg_emit1(0x58); /* POP AX (lo) */
                cg_emit1(0x5A); /* POP DX (hi) */
                coerce_dxax_to_fpu(); /* ST(0)=LHS_float, ST(1)=RHS */
            } else {
                cg_emit1(0x58); /* POP AX */
                coerce_ax_to_fpu();   /* ST(0)=LHS_float, ST(1)=RHS */
            }
            switch (op) {
            case T_PLUS:  cg_fadd(); break;
            case T_MINUS: cg_emit2(0xDE, 0xE1); break; /* FSUBRP: LHS-RHS */
            default: pe_error("operator not supported for mixed int/REAL"); break;
            }
            item->type = (rhs.type->form == TF_LONGREAL) ? type_longreal : type_real;
            item->mode = M_FREG;
            continue;
        }
        if (!is_long && rhs.type && rhs.type->form == TF_LONGINT) {
            pe_error("mixed INTEGER/LONGINT in expression: use explicit LONGINT() cast");
        }
        /* If LHS is LONGINT but RHS is not, sign-extend RHS AX → DX:AX */
        if (is_long && rhs.type && rhs.type->form != TF_LONGINT) {
            cg_emit1(0x99); /* CWD */
        }
        if (is_long) {
            cg_pop_cxbx();           /* CX:BX = LHS (hi:lo) */
            switch (op) {
            case T_PLUS:  cg_add32(); break;
            case T_MINUS: cg_sub32(); break;
            case T_OR:    cg_or32();  item->type=type_boolean; break;
            default: break;
            }
        } else {
            cg_emit1(OP_POP_CX);
            switch (op) {
            case T_PLUS:  cg_add(); break;
            case T_MINUS: cg_sub(); break;
            case T_OR:    cg_or();  item->type=type_boolean; break;
            default: break;
            }
        }
        item->mode=M_REG;
    }
}

static int relop_jcc(Token op) {
    switch(op) {
    case T_EQL: return OP_JZ;
    case T_NEQ: return OP_JNZ;
    case T_LSS: return OP_JL;
    case T_LEQ: return OP_JLE;
    case T_GTR: return OP_JG;
    case T_GEQ: return OP_JGE;
    default:    return OP_JZ;
    }
}

/* Map relop token to FPU Jcc (after FCOMPP+FNSTSW+SAHF: CF/ZF set per ST(0) vs ST(1)).
   After FCOMPP+SAHF with ST(0)=LHS, ST(1)=RHS:
     C0=CF=1, ZF=0 → LHS < RHS
     C3=ZF=1       → LHS = RHS
     C0=CF=0, ZF=0 → LHS > RHS
   So: JB=0x72 means LHS<RHS, JBE=0x76 means LHS<=RHS, JA=0x77 means LHS>RHS,
       JAE=0x73 means LHS>=RHS, JZ=0x74 means LHS==RHS, JNZ=0x75 means LHS!=RHS. */
static int frelop_jcc(Token op) {
    switch(op) {
    case T_EQL: return 0x74; /* JZ  */
    case T_NEQ: return 0x75; /* JNZ */
    case T_LSS: return 0x72; /* JB  */
    case T_LEQ: return 0x76; /* JBE */
    case T_GTR: return 0x77; /* JA  */
    case T_GEQ: return 0x73; /* JAE */
    default:    return 0x74;
    }
}

void parse_expr(Item *item) {
    Item rhs;
    parse_simple_expr(item);
    if (pe_sc->sym>=T_EQL && pe_sc->sym<=T_GEQ) {
        Token op = pe_sc->sym; scanner_next(pe_sc);
        cg_load_item(item);
        /* FPU comparison: LHS is REAL/LONGREAL in ST(0) */
        if (item->type && (item->type->form == TF_REAL ||
                           item->type->form == TF_LONGREAL)) {
            /* Save LHS, compute RHS, reload LHS → ST(0)=LHS, ST(1)=RHS, FCOMPP */
            cg_fpush();
            parse_simple_expr(&rhs); cg_load_item(&rhs);
            /* Implicit coercion: integer RHS → FPU */
            if (is_int_type(&rhs)) {
                if (rhs.type->form == TF_LONGINT) coerce_dxax_to_fpu();
                else                              coerce_ax_to_fpu();
            }
            cg_fpop();  /* ST(0)=LHS, ST(1)=RHS */
            cg_fcmp(frelop_jcc(op));
            item->type=type_boolean; item->mode=M_REG;
            return;
        }
        /* Pointer comparison: compare both segment (DX) and offset (AX). */
        if (item->type && (item->type->form == TF_POINTER ||
                           item->type->form == TF_ADDRESS ||
                           item->type->form == TF_NILTYPE)) {
            cg_emit1(OP_PUSH_DX);       /* push LHS segment */
            cg_emit1(OP_PUSH_AX);       /* push LHS offset  */
            parse_simple_expr(&rhs); cg_load_item(&rhs);
            /* cg_ptr_cmp_eq pops LHS ofs+seg, XORs with RHS DX:AX, ORs, sets ZF */
            cg_ptr_cmp_eq();
            /* Only = and # are valid for pointers; use JZ for =, JNZ for # */
            cg_setcc(op == T_EQL ? OP_JZ : OP_JNZ);
        } else if (item->type && item->type->form == TF_LONGINT) {
            /* 32-bit signed comparison: LHS in DX:AX; save it, load RHS, compare */
            cg_push_dxax();              /* push LHS (DX hi, AX lo) */
            parse_simple_expr(&rhs); cg_load_item(&rhs);
            /* Implicit coercion: LONGINT LHS, REAL/LONGREAL RHS → FPU comparison */
            if (is_fpu_type(&rhs)) {
                /* Before: ST(0)=RHS float. Recover integer LHS from CPU stack. */
                cg_emit1(0x58); /* POP AX (lo word) */
                cg_emit1(0x5A); /* POP DX (hi word) */
                /* coerce pushes LHS onto FPU: ST(0)=LHS_float, ST(1)=RHS — ready for cg_fcmp */
                coerce_dxax_to_fpu();
                cg_fcmp(frelop_jcc(op));
                item->type=type_boolean; item->mode=M_REG;
                return;
            }
            /* If RHS is not LONGINT, sign-extend AX → DX:AX */
            if (rhs.type && rhs.type->form != TF_LONGINT) {
                cg_emit1(0x99); /* CWD */
            }
            cg_pop_cxbx();              /* CX:BX = LHS (hi:lo), DX:AX = RHS */
            /* Use two-stage compare: hi-word signed, lo-word unsigned */
            switch (op) {
            case T_EQL: cg_cmp32_eq();  break;
            case T_NEQ: cg_cmp32_neq(); break;
            /* JL=0x7C JG=0x7F; JB=0x72 JA=0x77 */
            case T_LSS: cg_cmp32_bool(0x7C, 0x7F, 0x72); break; /* hi<: JL=T; hi>: JG=F; lo: JB=T */
            case T_GTR: cg_cmp32_bool(0x7F, 0x7C, 0x77); break; /* hi>: JG=T; hi<: JL=F; lo: JA=T */
            /* JLE=0x7E JGE=0x7D; JBE=0x76 JAE=0x73 */
            case T_LEQ: cg_cmp32_bool(0x7C, 0x7F, 0x76); break; /* hi<: JL=T; hi>: JG=F; lo: JBE=T */
            case T_GEQ: cg_cmp32_bool(0x7F, 0x7C, 0x73); break; /* hi>: JG=T; hi<: JL=F; lo: JAE=T */
            default:    cg_cmp32_eq();  break;
            }
        } else {
            cg_emit1(OP_PUSH_AX);
            parse_simple_expr(&rhs); cg_load_item(&rhs);
            /* Implicit coercion: INTEGER LHS, REAL/LONGREAL RHS → FPU comparison */
            if (is_fpu_type(&rhs)) {
                cg_emit1(0x58);       /* POP AX — recover integer LHS */
                coerce_ax_to_fpu();   /* ST(0)=LHS_float, ST(1)=RHS (from parse) */
                cg_fcmp(frelop_jcc(op));
                item->type=type_boolean; item->mode=M_REG;
                return;
            }
            cg_emit1(OP_POP_CX);
            cg_cmp();                    /* CMP CX, AX */
            cg_setcc(relop_jcc(op));
        }
        item->type=type_boolean; item->mode=M_REG;
    } else if (pe_sc->sym == T_IN) {
        /* i IN s: test bit i of set s; result is BOOLEAN.
           Emit: PUSH AX (i); parse RHS (s) into AX; MOV CX,AX;
                 POP AX (i); MOV CL,AL; MOV AX,1; SHL AX,CL; TEST AX,CX; cg_setcc(JNZ). */
        scanner_next(pe_sc);
        cg_load_item(item);
        cg_emit1(OP_PUSH_AX);                      /* PUSH i */
        parse_simple_expr(&rhs); cg_load_item(&rhs);/* AX = s */
        cg_emit2(0x89, 0xC3);                      /* MOV BX, AX  (save s in BX) */
        cg_emit1(OP_POP_AX);                       /* POP AX      (restore i) */
        cg_emit2(0x8A, 0xC8);                      /* MOV CL, AL  (CL = i) */
        cg_emit1(0xB8); cg_emitw(0x0001);          /* MOV AX, 1 */
        cg_shl_cl();                               /* SHL AX, CL  (AX = 1 << i) */
        cg_emit2(0x85, 0xC3);                      /* TEST AX, BX */
        cg_setcc(OP_JNZ);
        item->type=type_boolean; item->mode=M_REG;
    }
}
/* ================================================================
   SYSTEM INTRINSICS (expression-mode: ADR, VAL, PTR, SEG, OFS)
   Called from parse_factor when item->mode == M_SYSPROC && T_LPAREN.
   On entry: T_LPAREN not yet consumed.
   On exit: item->mode = M_REG, item->type set.
   ================================================================ */
static void parse_system_intrinsic(Item *item, int id) {
    Item arg;
    TypeDesc *tgt;
    pe_expect(T_LPAREN);
    switch (id) {

    case SP_ADR:
        /* SYSTEM.ADR(v) -> ADDRESS: offset of v within its segment.
           Supports: global/local variables and arrays/records (M_GLOBAL/M_LOCAL),
           VAR params (M_LOCAL + is_ref), procedure names (M_PROC),
           array elements and pointer dereferences (M_REG + is_ref=1 → BX),
           and uplevel variables (M_LOCAL, sl_hops>0 → SS-relative via SL chain). */
        parse_designator(&arg);
        if (arg.mode == M_PROC) {
            /* Procedure: code-segment offset; DX = CS (segment of the procedure). */
            cg_load_code_addr((uint16_t)(uint32_t)arg.adr);
            cg_emit2(0x8C, 0xCA);             /* MOV DX, CS */
        } else if (arg.mode == M_LOCAL && arg.is_ref) {
            /* VAR param: far ptr {offset,segment} at [BP+adr].
               Return the address the VAR param points to: AX=offset, DX=segment. */
            cg_load_bp(arg.adr);              /* AX = [BP+adr]   = offset of caller's var */
            cg_load_bp(arg.adr + 2);          /* AX = [BP+adr+2] = segment (clobbers AX) */
            cg_emit2(0x89, 0xC2);             /* MOV DX, AX   — save segment in DX */
            cg_load_bp(arg.adr);              /* AX = [BP+adr] = offset (reload) */
        } else if (arg.mode == M_LOCAL && arg.sl_hops > 0) {
            /* Uplevel variable: outer frame BP in BX via SL chain, then LEA */
            cg_sl_load_bx(arg.sl_hops);
            /* LEA AX, [BX+ofs] — SS-relative address of outer variable */
            if (arg.adr >= -128 && arg.adr <= 127) {
                cg_emit3(0x8D, 0x47, (uint8_t)(arg.adr & 0xFF));
            } else {
                cg_emit2(0x8D, 0x87); cg_emitw((uint16_t)(int16_t)arg.adr);
            }
            cg_emit2(0x8C, 0xD2);             /* MOV DX, SS — segment for stack locals */
        } else if (arg.mode == M_LOCAL) {
            cg_load_addr_bp(arg.adr);         /* LEA AX, [BP+adr] */
            cg_emit2(0x8C, 0xD2);             /* MOV DX, SS — segment for stack locals */
        } else if (arg.mode == M_GLOBAL) {
            cg_load_addr_mem((uint16_t)(uint32_t)arg.adr); /* LEA AX, [data_ofs]+RELOC */
            cg_emit2(0x8C, 0xDA);             /* MOV DX, DS — segment for data globals */
        } else if (arg.mode == M_REG && arg.is_ref) {
            /* Pointer deref (n^) or array element: ES:BX = far address.
               Return full far pointer DX:AX = ES:BX so ADR(n^) = n. */
            cg_emit2(0x8B, 0xC3);             /* MOV AX, BX */
            cg_emit2(0x8C, 0xC2);             /* MOV DX, ES */
        } else {
            pe_error("ADR: variable or procedure required");
            cg_load_imm(0);
        }
        item->type = type_address;
        item->mode = M_REG;
        break;

    case SP_PTR:
        /* SYSTEM.PTR(s, o) -> ADDRESS: construct far pointer from segment s and offset o.
           s is pushed first (segment), o evaluated second (offset). */
        parse_expr(&arg);
        cg_load_item(&arg);                   /* AX = segment */
        cg_emit1(OP_PUSH_AX);                 /* save segment */
        pe_expect(T_COMMA);
        parse_expr(&arg);
        cg_load_item(&arg);                   /* AX = offset */
        cg_emit1(OP_POP_DX);                  /* DX = segment */
        /* result: DX:AX = far pointer */
        item->type = type_address;
        item->mode = M_REG;
        break;

    case SP_VAL: {
        /* SYSTEM.VAL(Type, expr) -> Type: reinterpret bits (no code generated).
           Sizes must match. */
        tgt = type_integer;
        pe_parse_type(&tgt);
        pe_expect(T_COMMA);
        parse_expr(&arg);
        cg_load_item(&arg);
        if (tgt->size != arg.type->size) {
            pe_error("SYSTEM.VAL: type sizes must match");
        }
        item->type = tgt;
        item->mode = M_REG;
        break;
    }

    case SP_SEG:
        /* SYSTEM.SEG(v) -> INTEGER: segment word of variable v.
           Large model: locals live in SS (stack segment), globals in DS (data segment).
           For VAR params (is_ref=1): return segment stored in the far ptr slot.
           For pointer deref (n^): ES:BX is the pointer value; return ES. */
        parse_designator(&arg);
        if (arg.mode == M_REG && arg.is_ref) {
            /* Pointer deref (n^): ES:BX = pointer value; SEG(n^) = segment of n */
            cg_emit2(0x8C, 0xC0);             /* MOV AX, ES */
        } else if (arg.mode == M_LOCAL && arg.is_ref) {
            /* load segment word from VAR param's far ptr at [BP+adr+2] */
            cg_load_bp(arg.adr + 2);          /* AX = segment */
        } else if (arg.mode == M_LOCAL) {
            cg_emit2(0x8C, 0xD0);             /* MOV AX, SS  (stack locals) */
        } else if (arg.mode == M_GLOBAL) {
            cg_emit2(0x8C, 0xD8);             /* MOV AX, DS  (data segment globals) */
        } else {
            pe_error("SEG: variable required");
            cg_load_imm(0);
        }
        item->type = type_integer;
        item->mode = M_REG;
        break;

    case SP_OFS:
        /* SYSTEM.OFS(v) -> INTEGER: offset of variable v within its segment.
           For pointer deref (n^): ES:BX is the pointer value; return BX. */
        parse_designator(&arg);
        if (arg.mode == M_REG && arg.is_ref) {
            /* Pointer deref (n^): ES:BX = pointer value; OFS(n^) = offset of n */
            cg_emit2(0x8B, 0xC3);             /* MOV AX, BX */
        } else if (arg.mode == M_LOCAL && arg.is_ref) {
            /* load offset word from VAR param's far ptr at [BP+adr] */
            cg_load_bp(arg.adr);              /* AX = offset */
        } else if (arg.mode == M_LOCAL) {
            cg_load_addr_bp(arg.adr);         /* LEA AX, [BP+adr] */
        } else if (arg.mode == M_GLOBAL) {
            cg_load_addr_mem(arg.adr);        /* LEA AX, [data_ofs]+RELOC */
        } else {
            pe_error("OFS: variable required");
            cg_load_imm(0);
        }
        item->type = type_integer;
        item->mode = M_REG;
        break;

    case SP_LSL:
    case SP_LSR:
    case SP_ASR:
    case SP_ROR: {
        /* SYSTEM.LSL/LSR/ASR/ROR(x, n) -> same type as x.
           Supports INTEGER (16-bit, AX) and LONGINT (32-bit, DX:AX). */
        int n_const, n_val, is_long;
        Item x_item;
        parse_expr(&x_item);
        is_long = (x_item.type && x_item.type->form == TF_LONGINT);
        cg_load_item(&x_item);       /* AX (or DX:AX) = x */
        if (is_long) { cg_push_dxax(); } else { cg_emit1(OP_PUSH_AX); }
        pe_expect(T_COMMA);
        parse_expr(&arg);
        n_const = (arg.mode == M_CONST);
        n_val   = (int)arg.val;
        cg_load_item(&arg);           /* AX = n */
        if (n_const) {
            n_val &= 0x1F;
            if (is_long) {
                cg_emit1(OP_POP_AX);  /* AX = lo */
                cg_emit1(OP_POP_DX);  /* DX = hi */
                switch (id) {
                case SP_LSL: cg_lsl32_imm(n_val); break;
                case SP_LSR: cg_lsr32_imm(n_val); break;
                case SP_ASR: cg_asr32_imm(n_val); break;
                default:     cg_ror32_imm(n_val); break;
                }
            } else {
                cg_emit1(OP_POP_AX);
                if (n_val == 0) {
                    /* no-op */
                } else if (n_val >= 16) {
                    if (id == SP_LSL || id == SP_LSR) {
                        cg_emit2(0x33, 0xC0); /* XOR AX, AX */
                    } else if (id == SP_ASR) {
                        cg_sar_imm(15);
                    } else {
                        int nr = n_val & 0x0F;
                        if (nr > 0) cg_ror_imm(nr);
                    }
                } else {
                    switch (id) {
                    case SP_LSL: cg_shl_imm(n_val); break;
                    case SP_LSR: cg_shr_imm(n_val); break;
                    case SP_ASR: cg_sar_imm(n_val); break;
                    default:     cg_ror_imm(n_val); break;
                    }
                }
            }
        } else {
            /* variable count in AX: MOV CL, AL */
            cg_emit2(0x8A, 0xC8);    /* MOV CL, AL */
            if (is_long) {
                cg_emit1(OP_POP_AX);  /* AX = lo */
                cg_emit1(OP_POP_DX);  /* POP DX = hi */
                switch (id) {
                case SP_LSL: cg_lsl32_cl(); break;
                case SP_LSR: cg_lsr32_cl(); break;
                case SP_ASR: cg_asr32_cl(); break;
                default:     cg_ror32_cl(); break;
                }
            } else {
                cg_emit3(0x80, 0xE1, 0x0F); /* AND CL, 0Fh */
                cg_emit1(OP_POP_AX);
                switch (id) {
                case SP_LSL: cg_shl_cl(); break;
                case SP_LSR: cg_shr_cl(); break;
                case SP_ASR: cg_sar_cl(); break;
                default:     cg_ror_cl(); break;
                }
            }
        }
        item->type = is_long ? type_longint : type_integer;
        item->mode = M_REG;
        break;
    }

    default:
        pe_error("SYSTEM intrinsic not usable as expression");
        item->type = type_integer;
        item->mode = M_CONST;
        item->val  = 0;
        break;
    }
    pe_expect(T_RPAREN);
}
