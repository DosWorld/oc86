#include "parser.h"
#include "pexpr.h"
#include "pstate.h"
#include "import.h"
#include "def.h"
#include "tar.h"
#include "compat.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

int parser_errors = 0;
int parser_system_mode = 0; /* 1 = compiling SYSTEM itself: no implicit SYSTEM import */
ExtraRdf *parser_extra_rdfs_head = NULL;
static ExtraRdf *parser_extra_rdfs_tail = NULL;
int        parser_n_extra_rdfs = 0;

void parser_syscomment(Scanner *s, char directive, const char *arg) {
    if (directive == 'L') {
        if (parser_n_extra_rdfs < PARSER_MAX_EXTRA_RDFS) {
            const char *start = arg;
            const char *end;
            size_t len;
            char *copy;
            ExtraRdf *node;
            while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') start++;
            end = start + strlen(start);
            while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) end--;
            len = (size_t)(end - start);
            copy = (char*)malloc(len + 1);
            if (!copy) { fprintf(stderr, "oc: out of memory\n"); return; }
            memcpy(copy, start, len);
            copy[len] = '\0';
            node = (ExtraRdf*)malloc(sizeof(ExtraRdf));
            if (!node) { fprintf(stderr, "oc: out of memory\n"); free(copy); return; }
            node->path = copy;
            node->next = NULL;
            if (parser_extra_rdfs_tail) parser_extra_rdfs_tail->next = node;
            else                        parser_extra_rdfs_head = node;
            parser_extra_rdfs_tail = node;
            parser_n_extra_rdfs++;
        } else {
            fprintf(stderr, "oc: too many $L directives (max %d)\n", PARSER_MAX_EXTRA_RDFS);
        }
    } else {
        fprintf(stderr, "%s(%d): unknown system comment directive '$%c'\n%s\n",
                s->filename, s->line, directive, s->cur_line);
    }
}
Scanner *pe_sc;                  /* shared with pexpr.c via pstate.h */
static char cur_mod[NAME_LEN];   /* current module name (set by parse_module) */
int pe_mod_uses_fpu = 0;         /* shared with pexpr.c via pstate.h */

/* RETURN statement backpatches: collected during parse_proc_decl, patched at epilogue */
#define MAX_RETURNS 64
static Backpatch ret_patches[MAX_RETURNS];
static int       n_ret_patches = 0;

/* Prologue frame size patch: address of the imm16 in SUB SP, imm16.
   parse_for_stat updates the frame size here after allocating $lim. */
static uint16_t cur_prologue_patch = 0;

/* Current proc nesting depth: 0 = module body, 1 = top-level proc, 2 = nested inside proc, etc.
   Used to determine if a proc needs a static link (has_sl) and for uplevel variable access. */
static int cur_proc_depth = 0;

/* Emit code to push the static link for a callee defined at scope level def_level.
   The SL passed to the callee is the BP of the scope at def_level.
   From the current scope (top_scope->level), we traverse the static link chain.

   def_level == top_scope->level: callee defined in current scope → PUSH BP
   def_level <  top_scope->level: callee is sibling/ancestor → load outer BP via SL chain

   Hop count to reach BP at def_level from current top_scope->level:
     hops = top_scope->level - def_level
   But SL at [BP+4] = parent's BP (1 hop).
   0 hops = current BP → PUSH BP
   1 hop = [BP+4] → MOV BX,[BP+4]; PUSH BX
   2 hops = [[BP+4]+4] → MOV BX,[BP+4]; MOV BX,[BX+4]; PUSH BX
   etc. */
void pe_emit_push_static_link(int def_level) {
    int hops = top_scope->level - def_level;
    if (hops <= 0) {
        cg_emit1(0x55);
    } else {
        cg_sl_load_bx(hops);
        cg_emit1(OP_PUSH_BX);
    }
}

int pe_get_system_import(const char *name) {
    Import *imp;
    for (imp = cg_obj.import_head; imp; imp = imp->next) {
        if (strcmp(imp->name, name) == 0)
            return imp->seg_id;
    }
    return rdf_add_import(&cg_obj, name);
}

void pe_error(const char *msg) {
    fprintf(stderr, "%s(%d): %s\n%s\n",
            pe_sc->filename, pe_sc->line, msg, pe_sc->cur_line);
    parser_errors++;
}
void pe_expect(Token t) {
    if (pe_sc->sym == t) scanner_next(pe_sc);
    else pe_error("unexpected token");
}

/* Convenience aliases used within parser.c itself */
#define sc        pe_sc
#define mod_uses_fpu pe_mod_uses_fpu
#define error(m)  pe_error(m)
#define expect(t) pe_expect(t)
#define get_system_import(n) pe_get_system_import(n)
#define emit_push_static_link(d) pe_emit_push_static_link(d)

/* ---- forward declarations ---- */
static void parse_stat_seq(TypeDesc *ret_type);
static void parse_type(TypeDesc **out);
void pe_parse_type(TypeDesc **out);

/* Forward references within TYPE declaration section:
   POINTER TO <name> where name is not yet declared.
   Collected during parse_type_decl, resolved after all declarations. */
#define MAX_FWD_REFS 32
static struct { char name[NAME_LEN]; TypeDesc *ptr; } fwd_refs[MAX_FWD_REFS];
static int n_fwd_refs = 0;


/* ================================================================
   STATEMENTS
   ================================================================ */
static void parse_if_stat(TypeDesc *ret_type) {
    Backpatch end_patches[64]; int n_end=0; int i;
    Item cond;
    Backpatch jf;
    scanner_next(sc);  /* consume IF */
    for (;;) {
        parse_expr(&cond); cg_load_item(&cond);
        cg_test_ax();
        cg_cond_near(OP_JZ, &jf);
        expect(T_THEN);
        parse_stat_seq(ret_type);
        if (n_end < 64) cg_jmp_near(&end_patches[n_end++]);
        else error("too many ELSIF branches (max 64)");
        cg_patch_near(&jf);
        if (sc->sym==T_ELSIF) scanner_next(sc);
        else break;
    }
    if (sc->sym==T_ELSE) { scanner_next(sc); parse_stat_seq(ret_type); }
    expect(T_END);
    for (i=0;i<n_end;i++) cg_patch_near(&end_patches[i]);
}

static void parse_while_stat(TypeDesc *ret_type) {
    /* Oberon-07 extended WHILE:
         WHILE g1 DO s1 ELSIF g2 DO s2 ... END
       All guards are re-evaluated from the top on each iteration.
       Any true guard executes its body then jumps back to loop_top.
       If no guard is true, fall through to after END. */
    uint16_t loop_top;
    Item cond;
    Backpatch jf;
    scanner_next(sc);
    loop_top = cg_pc();
    /* first guard */
    parse_expr(&cond); cg_load_item(&cond);
    cg_test_ax();
    cg_cond_near(OP_JZ, &jf);
    expect(T_DO);
    parse_stat_seq(ret_type);
    cg_jmp_back(loop_top);
    cg_patch_near(&jf);
    /* ELSIF guards */
    while (sc->sym == T_ELSIF) {
        scanner_next(sc);
        parse_expr(&cond); cg_load_item(&cond);
        cg_test_ax();
        cg_cond_near(OP_JZ, &jf);
        expect(T_DO);
        parse_stat_seq(ret_type);
        cg_jmp_back(loop_top);
        cg_patch_near(&jf);
    }
    expect(T_END);
}

static void parse_repeat_stat(TypeDesc *ret_type) {
    uint16_t loop_top;
    Item cond;
    scanner_next(sc);
    loop_top = cg_pc();
    parse_stat_seq(ret_type);
    expect(T_UNTIL);
    parse_expr(&cond); cg_load_item(&cond);
    cg_test_ax();
    cg_cond_back(OP_JZ, loop_top);  /* repeat if FALSE */
}

/* parse_const_label: parse a constant integer label value.
   Handles negative literals (T_MINUS T_INT) and named constants. */
static int parse_const_label(void) {
    int neg = 0, val = 0;
    if (sc->sym == T_MINUS) { neg = 1; scanner_next(sc); }
    if (sc->sym == T_INT) {
        val = sc->ival; scanner_next(sc);
    } else if (sc->sym == T_CHAR) {
        val = (int)(unsigned char)sc->ival; scanner_next(sc);
    } else if (sc->sym == T_IDENT) {
        Symbol *s = sym_find(sc->id); scanner_next(sc);
        if (!s || s->kind != K_CONST) { error("constant expected in CASE label"); return 0; }
        val = s->val;
    } else {
        error("constant expected in CASE label"); return 0;
    }
    return neg ? -val : val;
}

/* parse_case_stat: CASE expr OF arm { | arm } [ELSE StatSeq] END
   arm = label { , label } : StatSeq
   label = const | const .. const
   Strategy: store case expr in hidden local, then for each arm emit
   a chain of comparisons that jump into the arm body on match.
   After arm body, jump to END. */
static void parse_case_stat(TypeDesc *ret_type) {
    Symbol *case_sym;
    Item case_load;
    Backpatch end_patches[64]; int n_end = 0, i;

    scanner_next(sc);   /* consume CASE */
    /* evaluate case expression into AX, store in hidden local $case */
    { Item expr;
      parse_expr(&expr); cg_load_item(&expr); }
    case_sym = sym_new("$case", K_VAR);
    case_sym->type = type_integer;
    sym_alloc_local(case_sym);
    { Item dst; dst.mode=M_LOCAL; dst.adr=case_sym->adr;
      dst.type=type_integer; dst.is_ref=0; dst.sl_hops=0;
      cg_store_item(&dst); }
    /* update prologue frame size */
    { uint16_t new_sz = sym_local_size();
      cg_obj.code[cur_prologue_patch]   = new_sz & 0xFF;
      cg_obj.code[cur_prologue_patch+1] = (new_sz >> 8) & 0xFF; }
    expect(T_OF);

    /* parse arms: each arm is  label { , label } : StatSeq  */
    while (sc->sym != T_ELSE && sc->sym != T_END && sc->sym != T_EOF) {
        /* parse label list; for each label emit a comparison + conditional jump to body.
           Collect "jump-to-body" patches; after all labels, emit unconditional jump
           past the body (to next arm). Then patch body-jumps here and emit body. */
        Backpatch body_patches[32]; int n_body = 0;
        Backpatch skip_arm;

        /* emit label checks; each successful match jumps to arm body */
        for (;;) {
            int lo, hi;
            lo = parse_const_label();
            if (sc->sym == T_DOTDOT) {
                scanner_next(sc);
                hi = parse_const_label();
                /* range lo..hi: if AX < lo → skip; if AX <= hi → match */
                case_load.mode=M_LOCAL; case_load.adr=case_sym->adr;
                case_load.type=type_integer; case_load.is_ref=0; case_load.sl_hops=0;
                cg_load_item(&case_load);        /* AX = case expr */
                cg_cmp_ax_imm(lo);               /* CMP AX, lo */
                { Backpatch skip_lo;
                  cg_cond_near(0x7C /*JL*/, &skip_lo); /* JL skip (< lo) */
                  cg_cmp_ax_imm(hi);                    /* CMP AX, hi */
                  if (n_body < 32) cg_cond_near(OP_JLE, &body_patches[n_body++]); /* JLE body */
                  cg_patch_near(&skip_lo); }
            } else {
                /* single value */
                case_load.mode=M_LOCAL; case_load.adr=case_sym->adr;
                case_load.type=type_integer; case_load.is_ref=0; case_load.sl_hops=0;
                cg_load_item(&case_load);        /* AX = case expr */
                cg_cmp_ax_imm(lo);               /* CMP AX, lo */
                if (n_body < 32) cg_cond_near(OP_JZ, &body_patches[n_body++]); /* JE body */
            }
            if (sc->sym == T_COMMA) { scanner_next(sc); continue; }
            break;
        }
        /* no label matched: jump past arm body to next arm / ELSE / END */
        cg_jmp_near(&skip_arm);
        /* patch all body jumps to here (start of arm body) */
        for (i = 0; i < n_body; i++) cg_patch_near(&body_patches[i]);
        expect(T_COLON);
        parse_stat_seq(ret_type);
        /* after arm body: jump to END */
        if (n_end < 64) cg_jmp_near(&end_patches[n_end++]);
        /* patch skip_arm to here (start of next arm's label checks) */
        cg_patch_near(&skip_arm);
        if (sc->sym == T_BAR) scanner_next(sc);
    }
    /* ELSE arm */
    if (sc->sym == T_ELSE) {
        scanner_next(sc);
        parse_stat_seq(ret_type);
    }
    expect(T_END);
    /* patch all arm-end jumps to here */
    for (i = 0; i < n_end; i++) cg_patch_near(&end_patches[i]);
}

static void parse_for_stat(TypeDesc *ret_type) {
    Symbol *var;
    Symbol *lim_sym;
    Item init;
    Item dst;
    int32_t step;
    uint16_t loop_top;
    Item v;
    Item lim_item;
    Backpatch jend;
    (void)ret_type;
    scanner_next(sc);
    if (sc->sym!=T_IDENT) { error("identifier expected"); return; }
    var = sym_find(sc->id); scanner_next(sc);
    if (!var || var->type != type_integer) error("INTEGER var expected");
    expect(T_ASSIGN);
    parse_expr(&init); cg_load_item(&init);
    /* store initial value */
    dst.mode=(var->level==0?M_GLOBAL:M_LOCAL); dst.adr=var->adr;
    dst.type=type_integer; dst.is_ref=0; dst.sl_hops=0;
    cg_store_item(&dst);
    expect(T_TO);
    /* evaluate limit once and store in a hidden local temp */
    lim_sym = sym_new("$lim", K_VAR);
    lim_sym->type = type_integer;
    sym_alloc_local(lim_sym);
    parse_expr(&lim_item); cg_load_item(&lim_item);
    {
        Item lim_dst;
        lim_dst.mode=M_LOCAL; lim_dst.adr=lim_sym->adr;
        lim_dst.type=type_integer; lim_dst.is_ref=0; lim_dst.sl_hops=0;
        cg_store_item(&lim_dst);
    }
    step=1;
    if (sc->sym==T_BY) {
        int neg=0;
        scanner_next(sc);
        if (sc->sym==T_MINUS) { neg=1; scanner_next(sc); }
        if (sc->sym!=T_INT) error("constant expected");
        step = neg ? -(sc->ival) : sc->ival;
        scanner_next(sc);
    }
    /* extend the prologue frame size to include $lim (allocated after the initial patch) */
    { uint16_t new_sz = sym_local_size();
      cg_obj.code[cur_prologue_patch]   = new_sz & 0xFF;
      cg_obj.code[cur_prologue_patch+1] = (new_sz >> 8) & 0xFF; }
    expect(T_DO);
    loop_top = cg_pc();
    /* load var, load pre-stored limit, compare */
    v.mode=dst.mode; v.adr=dst.adr; v.type=type_integer; v.is_ref=0; v.sl_hops=0;
    cg_load_item(&v); cg_emit1(OP_PUSH_AX);
    {
        Item lim_load;
        lim_load.mode=M_LOCAL; lim_load.adr=lim_sym->adr;
        lim_load.type=type_integer; lim_load.is_ref=0; lim_load.sl_hops=0;
        cg_load_item(&lim_load);
    }
    cg_emit1(OP_POP_CX);   /* CX=var, AX=limit */
    cg_cmp();              /* CMP CX, AX */
    cg_cond_near(step>=0 ? OP_JG : 0x7C /*JL*/, &jend);
    parse_stat_seq(type_notype);
    /* increment: reload loop var from memory (v.mode may have been set to M_REG) */
    v.mode = dst.mode; v.is_ref = 0; v.sl_hops = 0;  /* reset mode so cg_load_item re-emits the load */
    cg_load_item(&v);
    if (step==1) cg_inc_ax();
    else if (step==-1) cg_dec_ax();
    else { cg_load_imm_cx(step); cg_add(); }
    cg_store_item(&dst);
    cg_jmp_back(loop_top);
    cg_patch_near(&jend);
    expect(T_END);
}

static void parse_return_stat(TypeDesc *ret_type) {
    Item val;
    scanner_next(sc);
    if (sc->sym!=T_SEMI && sc->sym!=T_END &&
        sc->sym!=T_ELSE && sc->sym!=T_ELSIF) {
        parse_expr(&val); cg_load_item(&val);
    } else if (ret_type && ret_type->form != TF_NOTYPE) {
        error("RETURN value required");
    }
    /* emit unconditional forward jump to epilogue; patch later */
    if (n_ret_patches < MAX_RETURNS)
        cg_jmp_near(&ret_patches[n_ret_patches++]);
    else
        error("too many RETURN statements (max 64)");
}

static void parse_sysproc_call(int id) {
    expect(T_LPAREN);
    switch (id) {
    case SP_NEW: {
        Item ptr; int mAlloc_id;
        parse_designator(&ptr);
        if (ptr.type->form!=TF_POINTER && ptr.type->form!=TF_ADDRESS)
            error("NEW requires pointer or ADDRESS");
        mAlloc_id = get_system_import("SYSTEM_Alloc");
        if (sc->sym==T_COMMA) {
            /* NEW(p, n): n = byte count (always). */
            Item sz_item;
            scanner_next(sc);
            parse_expr(&sz_item);
            cg_load_item(&sz_item);  /* AX = n (bytes) */
        } else {
            /* NEW(p): compile-time size from pointed-to type */
            int32_t sz = (ptr.type->form==TF_POINTER) ? ptr.type->base->size : 1;
            cg_load_imm(sz);         /* AX = sizeInBytes */
        }
        /* Call far SYSTEM_Alloc(sizeInBytes: INTEGER): POINTER
           Pascal convention: Alloc cleans 2 bytes (RETF 2).
           Result: DX:AX = far pointer (offset in AX, segment in DX). */
        cg_emit1(OP_PUSH_AX);       /* push sizeInBytes */
        cg_call_far(mAlloc_id);      /* CALL FAR SYSTEM_Alloc; cleans 2 bytes */
        /* Result: DX:AX = far pointer; store into ptr variable */
        cg_store_item(&ptr);
        break;
    }
    case SP_DISPOSE: {
        Item ptr; int mFree_id;
        parse_designator(&ptr);
        if (ptr.type->form!=TF_POINTER && ptr.type->form!=TF_ADDRESS)
            error("DISPOSE requires pointer or ADDRESS");
        /* Call far SYSTEM_Free(p: POINTER): VOID
           Pascal convention: Free cleans 4 bytes (RETF 4).
           Push far pointer {segment, offset} as 4-byte arg. */
        mFree_id = get_system_import("SYSTEM_Free");
        /* Load the far pointer value into DX:AX */
        cg_load_item(&ptr);  /* DX:AX = far pointer (ptr type) */
        /* Push as 4-byte arg: segment first (higher address), offset second */
        cg_emit1(OP_PUSH_DX);  /* PUSH DX (segment) */
        cg_emit1(OP_PUSH_AX);  /* PUSH AX (offset)  */
        cg_call_far(mFree_id); /* CALL FAR SYSTEM_Free; cleans 4 bytes */
        break;
    }
    case SP_INC: case SP_DEC: {
        Item v;
        Item vstore; /* saved target for store (mode preserved) */
        Item step;
        parse_designator(&v);
        vstore = v;             /* save before cg_load_item mutates mode */
        cg_load_item(&v);       /* AX = current value; v.mode becomes M_REG */
        if (sc->sym==T_COMMA) {
            scanner_next(sc); parse_expr(&step);
            cg_emit1(OP_PUSH_AX);   /* save current value */
            cg_load_item(&step);    /* AX = step */
            cg_emit1(OP_POP_CX);    /* CX = current value */
            if (id==SP_INC) cg_add(); else cg_sub(); /* AX = CX ± AX */
        } else {
            if (id==SP_INC) cg_inc_ax(); else cg_dec_ax();
        }
        cg_store_item(&vstore);
        break;
    }
    case SP_ASSERT: {
        /* ASSERT(cond) or ASSERT(cond, n): if cond is false, call SYSTEM_Halt(n).
           Default n=1. SYSTEM_Halt(ErrCode: BYTE) — Pascal conv, cleans 2 bytes. */
        Item cond;
        Backpatch ok;
        int32_t err_code = 1;
        int halt_id = get_system_import("SYSTEM_Halt");
        parse_expr(&cond); cg_load_item(&cond);
        if (sc->sym == T_COMMA) {
            scanner_next(sc);
            if (sc->sym == T_INT) { err_code = sc->ival; scanner_next(sc); }
            else { error("integer constant expected"); }
        }
        cg_test_ax();
        cg_cond_near(OP_JNZ, &ok);
        cg_load_imm((int16_t)err_code);
        cg_emit1(OP_PUSH_AX);
        cg_call_far(halt_id);
        cg_patch_near(&ok);
        break;
    }
    case SP_GET: {
        /* SYSTEM.GET(src: ADDRESS; VAR dst): load one word from far pointer src into dst.
           src is any ADDRESS expression (seg:ofs); works with heap, stack, data segment. */
        Item addr_it, var_it;
        parse_expr(&addr_it); cg_load_item(&addr_it); /* DX:AX = far ptr */
        cg_emit2(0x8E, 0xC2);        /* MOV ES, DX */
        cg_emit2(0x8B, 0xD8);        /* MOV BX, AX */
        expect(T_COMMA);
        parse_designator(&var_it);
        cg_emit3(OP_ES_PFX, 0x8B, 0x07); /* MOV AX, ES:[BX] */
        cg_store_item(&var_it);
        break;
    }
    case SP_PUT: {
        /* SYSTEM.PUT(dst: ADDRESS; expr): store one word (expr) to far pointer dst.
           dst is any ADDRESS expression (seg:ofs); works with heap, stack, data segment. */
        Item addr_it, val_it;
        parse_expr(&addr_it); cg_load_item(&addr_it); /* DX:AX = far ptr */
        cg_emit1(OP_PUSH_DX);        /* save segment */
        cg_emit1(OP_PUSH_AX);        /* save offset */
        expect(T_COMMA);
        parse_expr(&val_it); cg_load_item(&val_it);   /* AX = value */
        cg_emit1(OP_POP_BX);         /* BX = offset */
        cg_emit1(0x07);              /* POP ES  (segment) */
        cg_emit3(OP_ES_PFX, 0x89, 0x07); /* MOV ES:[BX], AX */
        break;
    }
    case SP_MOVE: {
        /* SYSTEM.MOVE(src: ADDRESS; dst: ADDRESS; n: INTEGER): REP MOVSB.
           src and dst are full ADDRESS values (any segment: heap, stack, data).
           Sequence: SI=src_ofs, push src_seg; DI=dst_ofs, ES=dst_seg; CX=cnt;
           pop src_seg into AX; PUSH DS (save); MOV DS,AX; REP MOVSB; POP DS (restore). */
        Item src_it, dst_it, cnt_it;
        parse_expr(&src_it); cg_load_item(&src_it);  /* DX:AX = src far ptr */
        cg_emit2(0x8B, 0xF0);        /* MOV SI, AX  (src offset) */
        cg_emit1(OP_PUSH_DX);        /* push src segment (saved for DS load after cnt) */
        expect(T_COMMA);
        parse_expr(&dst_it); cg_load_item(&dst_it);  /* DX:AX = dst far ptr */
        cg_emit2(0x8B, 0xF8);        /* MOV DI, AX  (dst offset) */
        cg_emit2(0x8E, 0xC2);        /* MOV ES, DX  (ES = dst segment) */
        expect(T_COMMA);
        parse_expr(&cnt_it); cg_load_item(&cnt_it);  /* AX = count */
        cg_emit2(0x8B, 0xC8);        /* MOV CX, AX */
        cg_emit1(0x58);              /* POP AX  (AX = src segment) */
        cg_emit1(0x1E);              /* PUSH DS  (save original data segment) */
        cg_emit2(0x8E, 0xD8);        /* MOV DS, AX  (DS = src segment) */
        cg_emit1(OP_CLD);            /* CLD */
        cg_emit2(0xF3, 0xA4);        /* REP MOVSB */
        cg_emit1(0x1F);              /* POP DS  (restore original data segment) */
        break;
    }
    case SP_FILL: {
        /* SYSTEM.FILL(dst: ADDRESS; n: INTEGER; b: BYTE): REP STOSB.
           dst is any ADDRESS expression (any segment: heap, stack, data). */
        Item dst_it, cnt_it, val_it;
        parse_expr(&dst_it); cg_load_item(&dst_it);  /* DX:AX = dst far ptr */
        cg_emit2(0x8B, 0xF8);        /* MOV DI, AX  (dst offset) */
        cg_emit2(0x8E, 0xC2);        /* MOV ES, DX  (ES = dst segment) */
        expect(T_COMMA);
        parse_expr(&cnt_it); cg_load_item(&cnt_it);  /* AX = count */
        cg_emit2(0x8B, 0xC8);        /* MOV CX, AX */
        expect(T_COMMA);
        parse_expr(&val_it); cg_load_item(&val_it);  /* AX = fill byte */
        cg_emit1(OP_CLD);            /* CLD */
        cg_emit2(0xF3, 0xAA);        /* REP STOSB */
        break;
    }
    default:
        error("unsupported built-in statement");
        break;
    }
    expect(T_RPAREN);
}

static void parse_statement(TypeDesc *ret_type) {
    if (sc->sym == T_IDENT) {
        Symbol *sym = sym_find(sc->id);
        Item item;
        Item rhs;
        if (sym && sym->kind == K_SYSPROC) {
            int sysid = sym->sys_id;
            scanner_next(sc);
            parse_sysproc_call(sysid);
            return;
        }
        parse_designator(&item);
        if (item.mode == M_SYSPROC) {
            /* SYSTEM.xxx intrinsic used as statement */
            parse_sysproc_call(item.val);
            return;
        }
        if (sc->sym == T_ASSIGN) {
            scanner_next(sc);
            /* If destination is M_REG/is_ref (array elem or ^-deref), ES:BX hold the
               target address.  RHS evaluation may clobber both via FAR calls.
               Save ES:BX on stack now; restore just before the store. */
            if (item.mode == M_REG && item.is_ref) {
                cg_emit1(0x06);             /* PUSH ES */
                cg_emit1(OP_PUSH_BX);       /* PUSH BX */
            }
            parse_expr(&rhs);
            /* Array-to-array assignment: emit REP MOVSB to copy src bytes into dst.
               Handles ARRAY OF CHAR := string literal (and general fixed-array copy).
               Only supported when both sides are fixed-size arrays in accessible memory
               (M_GLOBAL or M_LOCAL, no far-pointer indirection). */
            if (item.type && item.type->form == TF_ARRAY &&
                rhs.type  && rhs.type->form  == TF_ARRAY &&
                (item.mode == M_GLOBAL || item.mode == M_LOCAL) &&
                (rhs.mode  == M_GLOBAL || rhs.mode  == M_LOCAL)) {
                int32_t count = rhs.type->size < item.type->size
                                ? rhs.type->size : item.type->size;
                /* Load source address into SI */
                if (rhs.mode == M_GLOBAL) cg_load_addr_mem((uint16_t)rhs.adr);
                else                      cg_load_addr_bp(rhs.adr);
                cg_emit2(0x8B, 0xF0);  /* MOV SI, AX */
                /* Load dest address into DI */
                if (item.mode == M_GLOBAL) cg_load_addr_mem((uint16_t)item.adr);
                else                       cg_load_addr_bp(item.adr);
                cg_emit2(0x8B, 0xF8);  /* MOV DI, AX */
                /* CX = byte count */
                cg_load_imm((int16_t)count);
                cg_emit2(0x8B, 0xC8);  /* MOV CX, AX */
                /* Set ES = segment of destination array.
                   Local (stack) dest → ES = SS; global (data) dest → ES = DS.
                   REP MOVSB: source is DS:SI (override with 36: if src is local). */
                if (item.mode == M_LOCAL) {
                    cg_emit2(0x8C, 0xD2);  /* MOV DX, SS */
                } else {
                    cg_emit2(0x8C, 0xDA);  /* MOV DX, DS */
                }
                cg_emit2(0x8E, 0xC2);  /* MOV ES, DX */
                cg_emit1(OP_CLD);      /* CLD */
                if (rhs.mode == M_LOCAL) {
                    cg_emit1(0x36);        /* SS: prefix (source is stack) */
                }
                cg_emit2(0xF3, 0xA4);  /* REP MOVSB */
                goto assign_done;
            }
            /* Implicit INTEGER -> LONGINT coercion at assignment boundary. */
            if (item.type && item.type->form == TF_LONGINT &&
                rhs.type  && rhs.type->form  != TF_LONGINT &&
                rhs.mode  == M_CONST) {
                rhs.type = type_longint; /* load as 32-bit sign-extended imm */
            }
            cg_load_item(&rhs);
            /* For non-constant INTEGER sources, sign-extend AX to DX:AX */
            if (item.type && item.type->form == TF_LONGINT &&
                rhs.type  && rhs.type->form  != TF_LONGINT) {
                cg_emit1(0x99); /* CWD: sign-extend AX -> DX:AX */
            }
            /* Restore ES:BX if we saved them before the RHS */
            if (item.mode == M_REG && item.is_ref) {
                cg_emit1(OP_POP_BX);        /* POP BX  (AX/DX unaffected) */
                cg_emit1(0x07);             /* POP ES  (AX/DX unaffected) */
            }
            cg_store_item(&item);
            assign_done:;
        } else if (item.type && item.type->form == TF_PROC &&
                   (item.mode == M_LOCAL || item.mode == M_GLOBAL ||
                    (item.mode == M_REG && item.is_ref))) {
            /* Call through proc variable: CALL FAR [BP+ofs] or CALL FAR [mem] or ES:[BX] */
            if (item.mode == M_REG && item.is_ref) {
                /* ES:BX points to proc var slot; save before args clobber BX */
                cg_emit1(0x53); /* PUSH BX */
                cg_emit1(0x06); /* PUSH ES */
                if (sc->sym == T_LPAREN || (item.type->n_params > 0))
                    parse_actual_params(item.type);
                cg_emit2(0x89, 0xE3);  /* MOV BX, SP */
                if (item.type->arg_size > 0) {
                    if (item.type->arg_size <= 127) {
                        cg_emit2(0x83, 0xC3); cg_emit1((uint8_t)item.type->arg_size);
                    } else {
                        cg_emit2(0x81, 0xC3); cg_emitw((uint16_t)item.type->arg_size);
                    }
                }
                cg_emit3(0x36, 0x8B, 0x0F);             /* SS: MOV CX, [BX] */
                cg_emit3(0x36, 0x8B, 0x5F); cg_emit1(0x02); /* SS: MOV BX, [BX+2] */
                cg_emit2(0x8E, 0xC1);                   /* MOV ES, CX */
                cg_call_proc_var_esbx();
                cg_emit2(0x83, 0xC4); cg_emit1(0x04);  /* ADD SP, 4 */
            } else {
                if (sc->sym == T_LPAREN || (item.type->n_params > 0))
                    parse_actual_params(item.type);
                if (item.type->is_far) {
                    if (item.mode == M_LOCAL) cg_call_proc_var_bp(item.adr);
                    else                      cg_call_proc_var_mem((uint16_t)item.adr);
                } else {
                    if (item.mode == M_LOCAL) cg_call_proc_var_near_bp(item.adr);
                    else                      cg_call_proc_var_near_mem((uint16_t)item.adr);
                }
            }
        } else if (item.mode==M_PROC || item.mode==M_IMPORT) {
            if (sc->sym==T_LPAREN || (item.type && item.type->n_params>0)) {
                parse_actual_params(item.type ? item.type : type_new_proc(type_notype));
            }
            /* push static link after params, before CALL, for nested procs */
            if (item.mode==M_PROC && item.type && item.type->has_sl)
                emit_push_static_link(item.val);
            if (item.mode==M_IMPORT && item.is_far) cg_call_far(item.rdoff_id);
            else if (item.mode==M_IMPORT)           cg_call_near(item.rdoff_id);
            else if (item.is_far)                   cg_call_local_far((uint16_t)item.adr);
            else                                    cg_call_local((uint16_t)item.adr);
            /* pascal: callee cleaned stack */
        }
    } else if (sc->sym==T_IF)     parse_if_stat(ret_type);
    else if (sc->sym==T_WHILE)    parse_while_stat(ret_type);
    else if (sc->sym==T_REPEAT)   parse_repeat_stat(ret_type);
    else if (sc->sym==T_FOR)      parse_for_stat(ret_type);
    else if (sc->sym==T_CASE)     parse_case_stat(ret_type);
    else if (sc->sym==T_RETURN)   parse_return_stat(ret_type);
    /* empty statement: do nothing */
}

static void parse_stat_seq(TypeDesc *ret_type) {
    parse_statement(ret_type);
    while (sc->sym==T_SEMI) { scanner_next(sc); parse_statement(ret_type); }
}

/* Read one array-dimension: integer literal or named integer constant.
   Returns the value, or -2 on error (message already printed). */
static int32_t parse_array_dim(void) {
    if (sc->sym == T_INT) {
        int32_t v = sc->ival; scanner_next(sc); return v;
    } else if (sc->sym == T_IDENT) {
        Symbol *s = sym_find(sc->id); scanner_next(sc);
        if (!s || s->kind != K_CONST || s->type->form != TF_INTEGER) {
            error("integer constant expected as array dimension"); return -2;
        }
        return s->val;
    }
    error("array length expected"); return -2;
}

/* ================================================================
   TYPES
   ================================================================ */
static void parse_type(TypeDesc **out) {
    Symbol *sym;
    Symbol *tsym;
    int32_t len;
    TypeDesc *elem;
    TypeDesc *base;
    TypeDesc *rec;
    char names[16][NAME_LEN]; int exported_flags[16]; int n; int i;
    TypeDesc *ft;
    TypeDesc *pt;
    int is_var;
    TypeDesc *ptype;
    if (sc->sym==T_IDENT) {
        sym = sym_find(sc->id); scanner_next(sc);
        /* Handle qualified type name: Alias.TypeName (e.g. SYSTEM.Registers) */
        if (sym && sym->kind == K_IMPORT && sc->sym == T_DOT) {
            char tkey[NAME_LEN*2];
            scanner_next(sc);  /* consume '.' */
            if (sc->sym != T_IDENT) { error("type name expected after '.'"); *out=type_integer; return; }
            snprintf(tkey, sizeof(tkey), "%s.%s", sym->name, sc->id);
            tsym = sym_find(tkey);
            if (!tsym) tsym = sym_find(sc->id); /* fallback for universe pre-declarations */
            scanner_next(sc);
            if (!tsym || tsym->kind != K_TYPE) { error("type name expected"); *out=type_integer; return; }
            *out = tsym->type;
            return;
        }
        if (!sym || sym->kind!=K_TYPE) { error("type name expected"); *out=type_integer; return; }
        *out = sym->type;
        /* Track FPU type usage for mod_uses_fpu flag */
        if (sym->type && (sym->type->form == TF_REAL ||
                          sym->type->form == TF_LONGREAL))
            mod_uses_fpu = 1;
    } else if (sc->sym==T_ARRAY) {
        scanner_next(sc);  /* consume ARRAY */
        len=-1;
        if (sc->sym!=T_OF) { len=parse_array_dim(); }
        /* Multi-dim: ARRAY m, n OF T  →  ARRAY m OF ARRAY n OF T */
        if (sc->sym==T_COMMA) {
            /* Recursively build nested arrays for remaining dimensions */
            TypeDesc *inner;
            scanner_next(sc);  /* consume comma */
            inner = type_integer;
            /* Re-enter parse_type to handle remaining dimensions via T_ARRAY path */
            /* Synthesize: treat remaining as "ARRAY n [, ...] OF T" */
            {
                /* Build the inner part by re-parsing dimensions.
                   We need to handle: n [, n2 ...] OF T.
                   Strategy: collect all dimension sizes, then build nested arrays
                   inside-out (innermost first). */
                int32_t dims[8]; int ndims = 0;
                TypeDesc *base_elem;
                dims[ndims++] = parse_array_dim();
                while (sc->sym==T_COMMA && ndims < 7) {
                    scanner_next(sc);
                    dims[ndims++] = parse_array_dim();
                }
                expect(T_OF);
                base_elem = type_integer; parse_type(&base_elem);
                /* Build inner arrays from innermost outward */
                inner = base_elem;
                while (ndims > 0) { inner = type_new_array(inner, dims[--ndims]); }
                *out = type_new_array(inner, len);
                /* Track FPU use for inner element type */
                if (base_elem->form == TF_REAL || base_elem->form == TF_LONGREAL) mod_uses_fpu = 1;
            }
        } else {
            expect(T_OF);
            elem = type_integer; parse_type(&elem);
            *out = type_new_array(elem, len);
        }
    } else if (sc->sym==T_RECORD) {
        scanner_next(sc);
        base=NULL;
        if (sc->sym==T_LPAREN) {
            scanner_next(sc); parse_type(&base); expect(T_RPAREN);
        }
        rec = type_new_record(base);
        while (sc->sym==T_IDENT) {
            n=0;
            do {
                strncpy(names[n], sc->id, NAME_LEN-1); scanner_next(sc);
                exported_flags[n] = (sc->sym==T_STAR) ? 1 : 0;
                if (sc->sym==T_STAR) scanner_next(sc);
                n++;
                if (sc->sym==T_COMMA) scanner_next(sc);
            } while (sc->sym==T_IDENT);
            expect(T_COLON);
            ft = type_integer; parse_type(&ft);
            for (i=0;i<n;i++) {
                Symbol *fs = type_add_field(rec, names[i], ft);
                fs->exported = exported_flags[i];
            }
            if (sc->sym==T_SEMI) scanner_next(sc);
        }
        expect(T_END);
        *out = rec;
    } else if (sc->sym==T_POINTER) {
        TypeDesc *ptr_td;
        scanner_next(sc); expect(T_TO);
        /* Forward reference: POINTER TO <name> where name may not be declared yet.
           If the ident is unknown and we're inside a TYPE section (n_fwd_refs >= 0),
           create a stub base and record for patching. */
        if (sc->sym == T_IDENT && !sym_find(sc->id)) {
            ptr_td = type_new_pointer(type_notype); /* stub base */
            if (n_fwd_refs < MAX_FWD_REFS) {
                strncpy(fwd_refs[n_fwd_refs].name, sc->id, NAME_LEN-1);
                fwd_refs[n_fwd_refs].ptr = ptr_td;
                n_fwd_refs++;
            } else { error("too many forward type references"); }
            scanner_next(sc);
            *out = ptr_td;
        } else {
            base = type_integer; parse_type(&base);
            *out = type_new_pointer(base);
        }
    } else if (sc->sym==T_PROCEDURE) {
        scanner_next(sc);
        pt = type_new_proc(type_notype);
        /* optional FAR/NEAR modifier on procedure type */
        if (sc->sym==T_IDENT && strcmp(sc->id,"FAR")==0) {
            pt->is_far = 1; pt->size = SZ_POINTER; scanner_next(sc);
        } else if (sc->sym==T_IDENT && strcmp(sc->id,"NEAR")==0) {
            pt->is_far = 0; pt->size = SZ_INTEGER; scanner_next(sc);
        } else {
            pt->is_far = 1; pt->size = SZ_POINTER;  /* default: FAR */
        }
        if (sc->sym==T_LPAREN) {
            scanner_next(sc);
            while (sc->sym!=T_RPAREN && sc->sym!=T_EOF) {
                char tpnames[8][NAME_LEN]; int tnp = 0; int ti;
                int typeless_var2 = 0;
                is_var = (sc->sym==T_VAR); if (is_var) scanner_next(sc);
                do {
                    strncpy(tpnames[tnp++], sc->id, NAME_LEN-1); scanner_next(sc);
                    if (sc->sym==T_COMMA) scanner_next(sc);
                } while (tnp < 8 && sc->sym!=T_COLON && sc->sym!=T_SEMI && sc->sym!=T_RPAREN && sc->sym!=T_EOF);
                if (sc->sym==T_COLON) {
                    scanner_next(sc);
                    ptype = type_integer; parse_type(&ptype);
                } else {
                    if (!is_var) error("parameter requires a type");
                    typeless_var2 = 1;
                    ptype = type_address;
                }
                for (ti=0; ti<tnp; ti++) {
                    Symbol *tp = type_add_param(pt, tpnames[ti], ptype, is_var);
                    if (typeless_var2) tp->typeless = 1;
                }
                if (sc->sym==T_SEMI) scanner_next(sc);
            }
            expect(T_RPAREN);
            if (sc->sym==T_COLON) { scanner_next(sc); parse_type(&pt->ret_type); }
            type_calc_arg_size(pt);
        }
        *out = pt;
    } else {
    }
}

/* Thin wrapper so pexpr.c can call parse_type without circular include. */
void pe_parse_type(TypeDesc **out) { parse_type(out); }

/* ================================================================
   DECLARATIONS
   ================================================================ */
static void parse_const_decl(void) {
    char name[NAME_LEN];
    int exported;
    Item val;
    Symbol *s;
    while (sc->sym==T_IDENT) {
        strncpy(name,sc->id,NAME_LEN-1); scanner_next(sc);
        if (sym_find_local(name)) error("duplicate identifier");
        exported = 0; if (sc->sym==T_STAR) { exported=1; scanner_next(sc); }
        expect(T_EQL);
        parse_expr(&val);
        s = sym_new(name, K_CONST);
        s->exported = exported;
        s->type = val.type;
        s->val  = (val.mode==M_CONST) ? val.val : 0;
        s->adr  = (val.mode==M_CONST) ? val.adr : 0; /* preserve data-seg offset for REAL consts */
        if (sc->sym==T_SEMI) scanner_next(sc);
    }
}

static void parse_type_decl(void) {
    char name[NAME_LEN];
    int exported;
    TypeDesc *placeholder;
    TypeDesc *t;
    Symbol *s;
    int i;
    Symbol *fsym;
    n_fwd_refs = 0;
    while (sc->sym==T_IDENT) {
        strncpy(name,sc->id,NAME_LEN-1); scanner_next(sc);
        if (sym_find_local(name)) error("duplicate identifier");
        exported=0; if (sc->sym==T_STAR) { exported=1; scanner_next(sc); }
        /* Pre-register name so self-references (e.g. POINTER TO T) resolve.
           Give placeholder pointer-sized dimensions so self-referential pointer
           fields get the correct 4-byte size when type_add_field runs. */
        placeholder = type_new(TF_NOTYPE, 0);
        placeholder->size  = SZ_POINTER;
        placeholder->align = 2;
        s = sym_new(name, K_TYPE);
        s->type = placeholder; s->exported = exported;
        expect(T_EQL);
        t = type_integer; parse_type(&t);
        /* Patch placeholder in-place so any captured pointers see the real type. */
        *placeholder = *t;
        if (sc->sym==T_SEMI) scanner_next(sc);
    }
    /* Resolve forward references: POINTER TO <name> where name was not yet declared. */
    for (i = 0; i < n_fwd_refs; i++) {
        fsym = sym_find(fwd_refs[i].name);
        if (!fsym || fsym->kind != K_TYPE) {
            fprintf(stderr, "error: unresolved forward type reference '%s'\n", fwd_refs[i].name);
            parser_errors++;
        } else {
            fwd_refs[i].ptr->base = fsym->type;
        }
    }
    n_fwd_refs = 0;
}

static void parse_var_decl(void) {
    while (sc->sym==T_IDENT) {
        char names[16][NAME_LEN]; int exported_flags[16]; int n=0; int i;
        TypeDesc *t;
        Symbol *s;
        char gname[NAME_LEN*2];
        do {
            strncpy(names[n],sc->id,NAME_LEN-1); scanner_next(sc);
            exported_flags[n] = 0;
            if (sc->sym==T_STAR) { exported_flags[n]=1; scanner_next(sc); }
            n++;
            if (sc->sym==T_COMMA) scanner_next(sc);
        } while (sc->sym==T_IDENT);
        expect(T_COLON);
        t = type_integer; parse_type(&t);
        for (i=0;i<n;i++) {
            if (sym_find_local(names[i])) error("duplicate identifier");
            s = sym_new(names[i], K_VAR);
            s->exported = exported_flags[i];
            s->type = t;
            if (top_scope->level==0) {
                /* global: allocate in data segment */
                s->adr = cg_dpc();
                cg_emit_data_zero(t->size);
                if (s->exported) {
                    snprintf(gname, sizeof(gname), "%s_%s", cur_mod, names[i]);
                    rdf_add_global(&cg_obj, gname, SEG_DATA, s->adr);
                }
            } else {
                sym_alloc_local(s);
            }
        }
        if (sc->sym==T_SEMI) scanner_next(sc);
    }
}

static void parse_proc_decl(void);

static void parse_decl_seq(void) {
    while (sc->sym==T_CONST || sc->sym==T_TYPE ||
           sc->sym==T_VAR   || sc->sym==T_PROCEDURE) {
        if      (sc->sym==T_CONST)     { scanner_next(sc); parse_const_decl(); }
        else if (sc->sym==T_TYPE)      { scanner_next(sc); parse_type_decl(); }
        else if (sc->sym==T_VAR)       { scanner_next(sc); parse_var_decl(); }
        else if (sc->sym==T_PROCEDURE) { parse_proc_decl(); }
    }
}

static void parse_proc_decl(void) {
    char name[NAME_LEN];
    Symbol *sym;
    int exported;
    TypeDesc *pt;
    int is_var;
    char pnames[8][NAME_LEN]; int np; int i;
    TypeDesc *ptype;
    Symbol *pp;
    Scope  *sc2;
    Symbol *slist[16]; int ns; int pi;
    Symbol *s; Symbol *p;
    char gname[NAME_LEN*2];
    uint16_t prologue_patch;
    uint16_t local_sz;

    scanner_next(sc);  /* consume PROCEDURE */
    if (sc->sym!=T_IDENT) { error("procedure name expected"); return; }
    strncpy(name,sc->id,NAME_LEN-1); scanner_next(sc);
    if (sym_find_local(name)) error("duplicate name");
    sym = sym_new(name, K_PROC);
    exported = 0;
    if (sc->sym==T_STAR) { exported=1; scanner_next(sc); sym->exported=1; }

    /* formal parameters */
    pt = type_new_proc(type_notype);
    sym->type = pt;

    sym_open_scope();

    if (sc->sym==T_LPAREN) {
        scanner_next(sc);
        while (sc->sym!=T_RPAREN && sc->sym!=T_EOF) {
            int typeless_var = 0;
            is_var=(sc->sym==T_VAR); if (is_var) scanner_next(sc);
            /* ident list — stop at ':' (typed) or ';'/')' (typeless VAR) */
            np=0;
            do {
                strncpy(pnames[np++],sc->id,NAME_LEN-1); scanner_next(sc);
                if (sc->sym==T_COMMA) scanner_next(sc);
            } while (sc->sym!=T_COLON && sc->sym!=T_SEMI && sc->sym!=T_RPAREN && sc->sym!=T_EOF);
            if (sc->sym==T_COLON) {
                scanner_next(sc);
                ptype = type_integer; parse_type(&ptype);
            } else {
                /* VAR name without type: typeless VAR param; type is ADDRESS */
                if (!is_var) error("parameter requires a type");
                typeless_var = 1;
                ptype = type_address;
            }
            for (i=0;i<np;i++) {
                p = type_add_param(pt, pnames[i], ptype, is_var);
                /* also insert into scope */
                s = sym_new(pnames[i], p->kind);
                s->type = ptype;
                if (typeless_var) { p->typeless = 1; s->typeless = 1; }
            }
            if (sc->sym==T_SEMI) scanner_next(sc);
        }
        expect(T_RPAREN);
    }
    if (sc->sym==T_COLON) { scanner_next(sc); parse_type(&pt->ret_type); }
    if (exported) pt->is_far = 1;
    /* Nested proc (inside another proc) gets a hidden static link pushed last by caller.
       Static link sits at [BP+4]; params are shifted to start at [BP+6]. */
    if (!exported && cur_proc_depth > 0) pt->has_sl = 1;

    expect(T_SEMI);

    /* FAR/NEAR modifier: PROCEDURE abc; FAR; or PROCEDURE abc; NEAR; */
    if (sc->sym == T_IDENT && strcmp(sc->id, "FAR") == 0) {
        pt->is_far = 1;
        scanner_next(sc);
        expect(T_SEMI);
    } else if (sc->sym == T_IDENT && strcmp(sc->id, "NEAR") == 0) {
        if (exported) { error("exported procedure cannot be NEAR"); }
        pt->is_far = 0;
        scanner_next(sc);
        expect(T_SEMI);
    }

    /* EXTERNAL modifier: no body; implementation in external .rdf file */
    if (sc->sym == T_IDENT && strcmp(sc->id, "EXTERNAL") == 0) {
        scanner_next(sc);
        expect(T_SEMI);
        type_calc_arg_size(pt);
        snprintf(gname, sizeof(gname), "%s_%s", cur_mod, name);
        sym->rdoff_id = rdf_add_import(&cg_obj, gname);
        sym->code_ofs = 0;
        sym_close_scope();
        return;
    }

    type_calc_arg_size(pt);

    /* copy BP offsets from pt->params into scope symbols */
    {
        pp = pt->params;
        sc2 = top_scope;
        ns=0; pi=0;
        /* scope symbols are in reverse order (prepended), params in forward order */
        /* collect scope syms */
        for (s=sc2->symbols; s; s=s->next) slist[ns++]=s;
        /* assign in reverse (scope list is backwards) */
        for (p=pp; p; p=p->next,pi++) {
            /* find matching scope sym */
            for (i=0;i<ns;i++) {
                if (strcmp(slist[i]->name,p->name)==0) {
                    slist[i]->adr = p->adr; break;
                }
            }
        }
    }

    /* emit prologue placeholder */
    sym->code_ofs = cg_pc();
    if (exported) {
        snprintf(gname, sizeof(gname), "%s_%s", cur_mod, name);
        rdf_add_global(&cg_obj, gname, SEG_CODE, sym->code_ofs);
    }

    /* save outer RETURN patches (this proc gets its own) */
    { int saved_n_ret = n_ret_patches;
      Backpatch saved_ret[MAX_RETURNS]; int ri;
      uint16_t saved_prologue_patch = cur_prologue_patch;
      int saved_proc_depth = cur_proc_depth;
      Backpatch skip_nested;
      for (ri = 0; ri < saved_n_ret; ri++) saved_ret[ri] = ret_patches[ri];

    cur_proc_depth++;     /* entering this proc's body */
    n_ret_patches = 0;  /* reset RETURN backpatch list for this procedure */

    prologue_patch = cg_pc() + 5;  /* offset of imm16 in SUB SP, imm16 (after 55 8B EC 81 EC) */
    cur_prologue_patch = prologue_patch;
    cg_prologue(0);                    /* emit with 0; patch after vars */

    /* emit a forward JMP to skip over any nested procedure bodies emitted by
       parse_decl_seq; if no nested procs are present the JMP lands immediately
       after itself (0-byte forward jump = E9 00 00), which is harmless. */
    cg_jmp_near(&skip_nested);

    parse_decl_seq();

    /* patch the skip-nested JMP to land here (start of actual body) */
    cg_patch_near(&skip_nested);

    /* restore cur_prologue_patch: parse_decl_seq may have parsed nested procs which
       clobber cur_prologue_patch; restore it so FOR/$case locals patch our frame */
    cur_prologue_patch = prologue_patch;

    /* patch local frame size */
    local_sz = sym_local_size();
    cg_obj.code[prologue_patch]   = local_sz & 0xFF;
    cg_obj.code[prologue_patch+1] = (local_sz>>8) & 0xFF;

    if (sc->sym==T_BEGIN) { scanner_next(sc); parse_stat_seq(pt->ret_type); }

    expect(T_END);
    if (sc->sym==T_IDENT) {
        if (strcmp(sc->id,name)!=0) error("procedure name mismatch");
        scanner_next(sc);
    }
    /* patch all RETURN forward jumps to here (epilogue entry) */
    { int ri; for (ri=0; ri<n_ret_patches; ri++) cg_patch_near(&ret_patches[ri]); }
    if (pt->is_far) cg_epilogue_far(pt->arg_size);
    else            cg_epilogue(pt->arg_size);
    sym_close_scope();

    /* restore outer procedure's RETURN patches, cur_prologue_patch, and proc depth */
    n_ret_patches = saved_n_ret;
    for (ri = 0; ri < saved_n_ret; ri++) ret_patches[ri] = saved_ret[ri];
    cur_prologue_patch = saved_prologue_patch;
    cur_proc_depth = saved_proc_depth;
    } /* end of saved-context block */

    expect(T_SEMI);
}

/* ================================================================
   MODULE
   ================================================================ */

/* import list for __init: module alias names that need __init calls */
static char init_imports[64][NAME_LEN];
static int  n_init_imports = 0;

void parse_module(Scanner *s) {
    char mod_name[NAME_LEN];
    char iname[NAME_LEN];
    char alias[NAME_LEN];
    Symbol *isym;
    int has_imports;
    int has_body;
    char init_name[NAME_LEN*2];
    uint16_t init_ofs;
    uint16_t guard_ofs;
    int ii;
    char dep_init[NAME_LEN*2];
    int dep_id;
    char rdf_name[NAME_LEN+4];
    char def_name[NAME_LEN+4];
    FILE *df;
    char lib_name[NAME_LEN+4];
    FILE *rf;
    uint8_t *rdf_buf;
    long rdf_len;
    FILE *df2;
    uint8_t *def_buf;
    long def_len;
    FILE *lf;

    sc = s;
    sym_init();
    cg_init();
    /* Reserve data segment offset 0: open-array calling convention passes the array's
       data-segment address as a word, and the callee also reads that word as LEN(arr).
       A global variable at offset 0 would make LEN=0 in all open-array callees.
       Emitting 2 zero bytes here ensures the first real variable gets offset >= 2. */
    cg_emit_data_zero(2);
    parser_errors = 0;
    n_init_imports = 0;
    mod_uses_fpu = 0;

    /* SYSTEM is implicitly imported by every module unless -SYSTEM mode is active.
       SYSTEM is NEVER added to init_imports — __init never calls SYSTEM__init. */
    if (!parser_system_mode)
        add_implicit_import("SYSTEM");

    expect(T_MODULE);
    if (sc->sym!=T_IDENT) { error("module name expected"); return; }
    strncpy(mod_name, sc->id, NAME_LEN-1); scanner_next(sc);

    /* Part 1: forbid '_' in module names */
    if (strchr(mod_name, '_')) {
        error("module name must not contain '_'");
        return;
    }
    strncpy(cur_mod, mod_name, NAME_LEN-1);

    expect(T_SEMI);

    /* IMPORT list */
    if (sc->sym==T_IMPORT) {
        scanner_next(sc);
        while (sc->sym==T_IDENT) {
            /* Syntax: [alias :=] ModuleName
               First token is either the alias (if ':=' follows) or the module name. */
            strncpy(iname,sc->id,NAME_LEN-1); scanner_next(sc);
            strncpy(alias,iname,NAME_LEN-1); alias[NAME_LEN-1]='\0';
            if (sc->sym==T_ASSIGN) {  /* alias := RealModuleName */
                scanner_next(sc);
                /* iname already holds the alias; now read the real module name */
                strncpy(iname,sc->id,NAME_LEN-1); scanner_next(sc);
            }
            /* In normal mode SYSTEM is implicitly imported; explicit listing is an error.
               In -SYSTEM mode SYSTEM has no implicit import, so it may appear in IMPORT. */
            if (!parser_system_mode && strcmp(iname, "SYSTEM") == 0) {
                fprintf(stderr, "line %d: SYSTEM is implicitly imported and must not appear in IMPORT list\n",
                        sc->line);
                parser_errors++;
            } else {
                isym = sym_new(alias, K_IMPORT);
                strncpy(isym->mod_name, iname, NAME_LEN-1);
                /* record for __init dep call; SYSTEM excluded (never call SYSTEM__init) */
                if (strcmp(iname, "SYSTEM") != 0) {
                    if (n_init_imports < 64)
                        strncpy(init_imports[n_init_imports++], iname, NAME_LEN-1);
                    else
                        error("too many imports (max 64)");
                }
                /* load .def for type-aware access — fatal if not found */
                load_explicit_import(iname, alias);
            }
            if (sc->sym==T_COMMA) scanner_next(sc);
        }
        expect(T_SEMI);
    }

    parse_decl_seq();

    /* ---- generate __init FAR function ---- */
    /* Case 1: no imports, no body → bare RETF
       Case 2: guard word + CMP/JE/RETF/INC + dep calls + body + RETF
       No stack frame in __init (no locals, no params).

       Guard layout (Case 2):
         [guard_ofs]  dw 0              ; mymodule__inited
         [init_ofs]   ...               ; __init entry
           2E 83 3E lo hi 00            ; CS: CMP [guard_ofs], 0
           74 01                        ; JE .cont (skip RETF)
           CB                           ; RETF  (already inited)
           2E FF 06 lo hi               ; CS: INC [guard_ofs]  (.cont)
           9A ... (calls)
           body
           CB                           ; RETF
       INC happens before dep calls → circular init safe.                       */

    has_imports = (n_init_imports > 0);
    has_body    = (sc->sym == T_BEGIN);

    snprintf(init_name, sizeof(init_name), "%s__init", mod_name);

    if (!has_imports && !has_body) {
        /* Case 1: trivial module */
        init_ofs = cg_pc();
        rdf_add_global(&cg_obj, init_name, SEG_CODE, init_ofs);
        cg_emit1(OP_RETF);
    } else {
        /* Case 2 */
        guard_ofs = cg_pc(); ii = 0;
        cg_emitw(0);                  /* mymodule__inited: dw 0 */

        init_ofs = cg_pc();
        rdf_add_global(&cg_obj, init_name, SEG_CODE, init_ofs);

        /* CS: CMP word [guard_ofs], 0  — 2E 83 3E lo hi 00 */
        cg_emit1(0x2E);
        cg_emit2(0x83, 0x3E);
        { uint16_t addr_field = cg_pc();
          cg_emitw(guard_ofs);
          /* guard_ofs is a code-segment address; needs reloc so linker adjusts
             by final_code_ofs when this module is placed in the combined image */
          rdf_add_reloc(&cg_obj, SEG_CODE, addr_field, 2, SEG_CODE, 0); }
        cg_emit1(0x00);

        /* JE .cont (over 1-byte RETF) — 74 01 */
        cg_emit2(0x74, 0x01);

        /* RETF — early exit if already inited */
        cg_emit1(OP_RETF);

        /* .cont: CS: INC word [guard_ofs]  — 2E FF 06 lo hi */
        cg_emit1(0x2E);
        cg_emit2(0xFF, 0x06);
        { uint16_t addr_field = cg_pc();
          cg_emitw(guard_ofs);
          rdf_add_reloc(&cg_obj, SEG_CODE, addr_field, 2, SEG_CODE, 0); }

        /* if module uses FPU types, call SYSTEM_REQUIRE87 after guard INC */
        if (mod_uses_fpu) {
            int req87_id = get_system_import("SYSTEM_REQUIRE87");
            cg_call_far(req87_id);
        }

        /* call each imported module's __init (SYSTEM excluded) */
        for (ii = 0; ii < n_init_imports; ii++) {
            snprintf(dep_init, sizeof(dep_init), "%s__init", init_imports[ii]);
            dep_id = rdf_add_import(&cg_obj, dep_init);
            cg_call_far(dep_id);
        }

        /* module body */
        if (sc->sym == T_BEGIN) {
            scanner_next(sc);
            parse_stat_seq(type_notype);
        }

        cg_emit1(OP_RETF);
    }

    expect(T_END);
    if (sc->sym==T_IDENT) {
        if (strcmp(sc->id,mod_name)!=0) error("module name mismatch at END");
        scanner_next(sc);
    }
    expect(T_DOT);

    /* intermediate file names — used in both success and error paths */
    snprintf(rdf_name, sizeof(rdf_name), "%s.rdf", mod_name);
    snprintf(def_name, sizeof(def_name), "%s.def", mod_name);

    if (parser_errors == 0) {
        /* write .rdf */
        cg_finish(rdf_name);

        /* write .def */
        df = fopen(def_name, "w");
        if (df) { def_write(df, mod_name); fclose(df); }

        /* pack both into .om */
        snprintf(lib_name, sizeof(lib_name), "%s.om", mod_name);

        /* read .rdf into memory for tar */
        rf = fopen(rdf_name, "rb");
        rdf_buf = NULL; rdf_len = 0;
        if (rf) {
            fseek(rf, 0, SEEK_END); rdf_len = ftell(rf); rewind(rf);
            rdf_buf = (rdf_len > 0 && rdf_len <= 65000L) ? (uint8_t*)malloc((size_t)rdf_len) : NULL;
            if (rdf_buf) fread(rdf_buf, 1, (size_t)rdf_len, rf);
            fclose(rf);
        }

        /* read .def into memory for tar */
        df2 = fopen(def_name, "rb");
        def_buf = NULL; def_len = 0;
        if (df2) {
            fseek(df2, 0, SEEK_END); def_len = ftell(df2); rewind(df2);
            def_buf = (def_len > 0 && def_len <= 65000L) ? (uint8_t*)malloc((size_t)def_len) : NULL;
            if (def_buf) fread(def_buf, 1, (size_t)def_len, df2);
            fclose(df2);
        }

        lf = fopen(lib_name, "wb");
        if (lf) {
            char up_name[128];
            tar_begin(lf);
            str_upcase(up_name, (int)sizeof(up_name), rdf_name);
            if (rdf_buf) tar_add_file(lf, up_name, rdf_buf, (size_t)rdf_len);
            str_upcase(up_name, (int)sizeof(up_name), def_name);
            if (def_buf) tar_add_file(lf, up_name, def_buf, (size_t)def_len);
            /* extra .rdf files from $L directives — stored under USER/ prefix, uppercase */
            {
                ExtraRdf *er;
                for (er = parser_extra_rdfs_head; er; er = er->next) {
                    FILE *ef; uint8_t *ebuf = NULL; long elen = 0;
                    char user_name[128];
                    ef = fopen(er->path, "rb");
                    if (ef) {
                        fseek(ef, 0, SEEK_END); elen = ftell(ef); rewind(ef);
                        ebuf = (elen > 0 && elen <= 65000L) ? (uint8_t*)malloc((size_t)elen) : NULL;
                        if (ebuf) fread(ebuf, 1, (size_t)elen, ef);
                        fclose(ef);
                        str_upcase(up_name, (int)sizeof(up_name),
                                   path_basename(er->path));
                        snprintf(user_name, sizeof(user_name), "USER/%s", up_name);
                        tar_add_file(lf, user_name, ebuf, elen);
                        free(ebuf);
                    } else {
                        fprintf(stderr, "oc: cannot open extra rdf '%s'\n", er->path);
                    }
                }
            }
            tar_end(lf);
            fclose(lf);
        }

        free(rdf_buf);
        free(def_buf);
        { ExtraRdf *er = parser_extra_rdfs_head;
          while (er) { ExtraRdf *nx = er->next; free(er->path); free(er); er = nx; }
          parser_extra_rdfs_head = NULL; parser_extra_rdfs_tail = NULL;
          parser_n_extra_rdfs = 0; }

        /* remove intermediates — only .om remains after compilation */
        remove(rdf_name);
        remove(def_name);

        printf("wrote %s\n", lib_name);
    } else {
        /* remove any partial intermediates left by a failed compile */
        remove(rdf_name);
        remove(def_name);
        { ExtraRdf *er = parser_extra_rdfs_head;
          while (er) { ExtraRdf *nx = er->next; free(er->path); free(er); er = nx; }
          parser_extra_rdfs_head = NULL; parser_extra_rdfs_tail = NULL;
          parser_n_extra_rdfs = 0; }
        fprintf(stderr, "%d error(s), no output\n", parser_errors);
    }
}
