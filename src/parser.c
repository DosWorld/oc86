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
const char *parser_entry_proc = NULL; /* -entry: must exist and be exported after parsing */
char parser_mod_name[33] = ""; /* module name read from source; set after MODULE keyword */
ExtraRdf *parser_extra_rdfs_head = NULL;
static ExtraRdf *parser_extra_rdfs_tail = NULL;
int        parser_n_extra_rdfs = 0;
long       parser_stack_size = 0;

/* Read the whole file at 'path' into a newly-malloc'd buffer.
   On success, *out_buf points to the bytes and *out_len holds the length.
   Returns 0 on success, non-zero on failure (file missing, too large, OOM).
   Cap enforces the 16-bit malloc ceiling (Open Watcom large model). */
#define PARSER_READ_MAX 65000L
static int read_file_to_buf(const char *path, uint8_t **out_buf, long *out_len) {
    FILE *f;
    long len;
    uint8_t *buf;
    *out_buf = NULL; *out_len = 0;
    f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END); len = ftell(f); rewind(f);
    if (len <= 0 || len > PARSER_READ_MAX) { fclose(f); return -2; }
    buf = (uint8_t *)malloc((size_t)len);
    if (!buf) { fclose(f); return -3; }
    if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
        free(buf); fclose(f); return -4;
    }
    fclose(f);
    *out_buf = buf; *out_len = len;
    return 0;
}

void parser_syscomment(Scanner *s, char directive, const char *arg) {
    if (directive == 'M') {
        /* $M stack_size — stack size hint (LONGINT); stored in META-INF/STACK.TXT
           when compiling with -entry.  Only the last $M wins. */
        long val = 0;
        const char *p = arg;
        while (*p == ' ' || *p == '\t') p++;
        if (*p >= '0' && *p <= '9') {
            val = 0;
            while (*p >= '0' && *p <= '9') { val = val * 10 + (*p - '0'); p++; }
        }
        if (val <= 0) {
            fprintf(stderr, "%s(%d): $M directive requires a positive integer\n",
                    s->filename, s->line);
        } else {
            parser_stack_size = val;
        }
    } else if (directive == 'L') {
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
    } else if (directive == 'R') {
        /* $R+ enable array bounds checks; $R- disable */
        const char *p = arg;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '+')      pe_bounds_check = 1;
        else if (*p == '-') pe_bounds_check = 0;
        else fprintf(stderr, "%s(%d): $R directive requires + or -\n",
                     s->filename, s->line);
    } else {
        fprintf(stderr, "%s(%d): unknown system comment directive '$%c'\n%s\n",
                s->filename, s->line, directive, s->cur_line);
    }
}
Scanner *pe_sc;                  /* current scanner, set by parse_module */
static char cur_mod[NAME_LEN];   /* current module name (set by parse_module) */
int pe_mod_uses_fpu = 0;         /* shared with pexpr.c via pstate.h */
int pe_bounds_check = 0;         /* $R+/$R-: 1=emit bounds checks, 0=off (default) */

/* Generic forward-jump backpatch list node — used by RETURN, IF (end jumps),
   CASE (arm-end and body jumps), and anywhere else we need an unbounded list
   of forward jumps to patch later. All nodes are malloc'd; callers patch and
   free via bp_list_patch_free(). */
typedef struct BpNode { Backpatch bp; struct BpNode *next; } BpNode;

/* Prepend a new node (with fresh cg_jmp_near/cg_cond_near already emitted) */
static BpNode *bp_list_push(BpNode *head) {
    BpNode *n = (BpNode *)malloc(sizeof(BpNode));
    if (!n) { pe_error("out of memory"); return NULL; }
    n->next = head;
    return n;
}

/* Patch every forward jump in the list to the current PC, then free the list */
static void bp_list_patch_free(BpNode *head) {
    while (head) { BpNode *nx = head->next; cg_patch_near(&head->bp); free(head); head = nx; }
}

/* RETURN statement backpatches: unbounded list reused per procedure */
static BpNode *ret_patch_list = NULL;

/* Forward-procedure call patch list.
   Each node records a call site (the offset of the rel16 operand in the code buffer)
   and whether it was a FAR call (PUSH CS + CALL NEAR) so we can compute the correct
   patch-at offset.  Stored in sym->fwd_patches (cast to FwdCallNode *). */
typedef struct FwdCallNode {
    uint16_t patch_at; /* offset of rel16 word in cg_obj.code */
    struct FwdCallNode *next;
} FwdCallNode;

/* Record one unresolved call to a FORWARD-declared procedure.
   patch_at is the offset of the 2-byte relative displacement in cg_obj.code. */
static void fwd_add_call(Symbol *sym, uint16_t patch_at) {
    FwdCallNode *n = (FwdCallNode *)malloc(sizeof(FwdCallNode));
    if (!n) { pe_error("out of memory"); return; }
    n->patch_at = patch_at;
    n->next = (FwdCallNode *)sym->fwd_patches;
    sym->fwd_patches = n;
}

/* Patch all recorded call sites to point at target_ofs, then free the list. */
static void fwd_patch_calls(Symbol *sym, uint16_t target_ofs) {
    FwdCallNode *n = (FwdCallNode *)sym->fwd_patches;
    while (n) {
        FwdCallNode *nx = n->next;
        /* rel16 = target_ofs - (patch_at + 2) */
        uint16_t rel = (uint16_t)(target_ofs - (uint16_t)(n->patch_at + 2));
        cg_obj.code[n->patch_at]     = (uint8_t)(rel & 0xFF);
        cg_obj.code[n->patch_at + 1] = (uint8_t)(rel >> 8);
        free(n);
        n = nx;
    }
    sym->fwd_patches = NULL;
}

/* Emit a call to a forward-declared (unresolved) local procedure and record
   the patch site.  is_far=1 → PUSH CS + CALL NEAR (like cg_call_local_far). */
static void fwd_emit_call(Symbol *sym, int is_far) {
    uint16_t patch_at;
    if (is_far) cg_emit1(OP_PUSH_CS);
    cg_emit1(OP_CALL_NEAR);
    patch_at = cg_pc();
    cg_emitw(0);   /* placeholder rel16 */
    fwd_add_call(sym, patch_at);
}

/* Public wrapper so pexpr.c can emit forward calls (sym is Symbol*, cast via void*). */
void pe_fwd_emit_call(void *sym, int is_far) {
    fwd_emit_call((Symbol *)sym, is_far);
}

/* Ident-list node: accumulates a comma-separated list of identifiers with
   optional export marker (*). Used by VAR, RECORD field, and procedure
   parameter declarations where the source syntax is
       a, b, c* : T
   Linked list so there is no hard cap on identifier count. */
typedef struct IdentNode {
    char name[NAME_LEN];
    int  exported;
    struct IdentNode *next;
} IdentNode;

static void idlist_free(IdentNode *head) {
    while (head) { IdentNode *nx = head->next; free(head); head = nx; }
}

/* Append one ident (copy of 'name', exported flag) to tail of list. Returns new head. */
static IdentNode *idlist_append(IdentNode *head, IdentNode **tail,
                                const char *name, int exported) {
    IdentNode *n = (IdentNode *)malloc(sizeof(IdentNode));
    if (!n) { pe_error("out of memory"); return head; }
    strncpy(n->name, name, NAME_LEN-1);
    n->name[NAME_LEN-1] = '\0';
    n->exported = exported;
    n->next = NULL;
    if (*tail) (*tail)->next = n;
    else       head = n;
    *tail = n;
    return head;
}

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

void pe_error2(const char *msg, const char *name) {
    if (pe_sc && pe_sc->filename)
        fprintf(stderr, "%s(%d): %s: '%s'\n%s\n",
                pe_sc->filename, pe_sc->line, msg, name,
                pe_sc->cur_line ? pe_sc->cur_line : "");
    else
        fprintf(stderr, "(?): %s: '%s'\n", msg, name);
    parser_errors++;
}

void pe_error(const char *msg) {
    if (pe_sc && pe_sc->filename)
        fprintf(stderr, "%s(%d): %s\n%s\n",
                pe_sc->filename, pe_sc->line, msg,
                pe_sc->cur_line ? pe_sc->cur_line : "");
    else
        fprintf(stderr, "(?): %s\n", msg);
    parser_errors++;
}
void pe_expect(Token t) {
    if (pe_sc && pe_sc->sym == t) { scanner_next(pe_sc); return; }
    if (pe_sc && pe_sc->filename)
        fprintf(stderr, "%s(%d): expected token %d, got %d\n%s\n",
                pe_sc->filename, pe_sc->line, (int)t, (int)(pe_sc->sym),
                pe_sc->cur_line ? pe_sc->cur_line : "");
    else
        fprintf(stderr, "(?): expected token %d\n", (int)t);
    parser_errors++;
}

/* Convenience aliases used within parser.c itself */
#define sc        pe_sc
#define mod_uses_fpu pe_mod_uses_fpu
#define error(m)  pe_error(m)
#define expect(t) pe_expect(t)
#define get_system_import(n) pe_get_system_import(n)
#define emit_push_static_link(d) pe_emit_push_static_link(d)

/* ---- forward declarations ---- */
void parse_stat_seq(TypeDesc *ret_type);
void parse_type(TypeDesc **out);
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
void parse_if_stat(TypeDesc *ret_type) {
    BpNode *end_list = NULL;
    Item cond;
    Backpatch jf;
    scanner_next(sc);  /* consume IF */
    for (;;) {
        parse_expr(&cond); cg_load_item(&cond);
        cg_test_ax();
        cg_cond_near(OP_JZ, &jf);
        expect(T_THEN);
        parse_stat_seq(ret_type);
        { BpNode *n = bp_list_push(end_list);
          if (n) { cg_jmp_near(&n->bp); end_list = n; } }
        cg_patch_near(&jf);
        if (sc->sym==T_ELSIF) scanner_next(sc);
        else break;
    }
    if (sc->sym==T_ELSE) { scanner_next(sc); parse_stat_seq(ret_type); }
    expect(T_END);
    bp_list_patch_free(end_list);
}

void parse_while_stat(TypeDesc *ret_type) {
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

void parse_repeat_stat(TypeDesc *ret_type) {
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
int parse_const_label(void) {
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
void parse_case_stat(TypeDesc *ret_type) {
    Symbol *case_sym;
    Item case_load;
    BpNode *end_list = NULL;

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
        BpNode *body_list = NULL;
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
                  BpNode *n;
                  cg_cond_near(0x7C /*JL*/, &skip_lo); /* JL skip (< lo) */
                  cg_cmp_ax_imm(hi);                    /* CMP AX, hi */
                  n = bp_list_push(body_list);
                  if (n) { cg_cond_near(OP_JLE, &n->bp); body_list = n; } /* JLE body */
                  cg_patch_near(&skip_lo); }
            } else {
                /* single value */
                BpNode *n;
                case_load.mode=M_LOCAL; case_load.adr=case_sym->adr;
                case_load.type=type_integer; case_load.is_ref=0; case_load.sl_hops=0;
                cg_load_item(&case_load);        /* AX = case expr */
                cg_cmp_ax_imm(lo);               /* CMP AX, lo */
                n = bp_list_push(body_list);
                if (n) { cg_cond_near(OP_JZ, &n->bp); body_list = n; } /* JE body */
            }
            if (sc->sym == T_COMMA) { scanner_next(sc); continue; }
            break;
        }
        /* no label matched: jump past arm body to next arm / ELSE / END */
        cg_jmp_near(&skip_arm);
        /* patch all body jumps to here (start of arm body) */
        bp_list_patch_free(body_list);
        expect(T_COLON);
        parse_stat_seq(ret_type);
        /* after arm body: jump to END */
        { BpNode *n = bp_list_push(end_list);
          if (n) { cg_jmp_near(&n->bp); end_list = n; } }
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
    bp_list_patch_free(end_list);
}

void parse_for_stat(TypeDesc *ret_type) {
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

void parse_return_stat(TypeDesc *ret_type) {
    Item val;
    scanner_next(sc);
    if (sc->sym!=T_SEMI && sc->sym!=T_END &&
        sc->sym!=T_ELSE && sc->sym!=T_ELSIF) {
        parse_expr(&val); cg_load_item(&val);
    } else if (ret_type && ret_type->form != TF_NOTYPE) {
        error("RETURN value required");
    }
    /* emit unconditional forward jump to epilogue; patch later */
    { BpNode *rp = bp_list_push(ret_patch_list);
      if (rp) { cg_jmp_near(&rp->bp); ret_patch_list = rp; } }
}

void parse_sysproc_call(int id) {
    expect(T_LPAREN);
    switch (id) {
    case SP_NEW: {
        Item ptr; int mAlloc_id;
        int is_tagged_rec; /* 1 = POINTER TO RECORD → needs type tag */
        parse_designator(&ptr);
        if (ptr.type->form!=TF_POINTER && ptr.type->form!=TF_ADDRESS)
            error("NEW requires pointer or ADDRESS");
        is_tagged_rec = (ptr.type->form == TF_POINTER && ptr.type->base &&
                         ptr.type->base->form == TF_RECORD);
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
        /* For tagged records: allocate size+2 so the tag word fits before the data. */
        if (is_tagged_rec) cg_emit3(0x83, 0xC0, 0x02);  /* ADD AX, 2 */
        /* Call far SYSTEM_Alloc(sizeInBytes: INTEGER): POINTER
           Pascal convention: Alloc cleans 2 bytes (RETF 2).
           Result: DX:AX = far pointer (offset in AX, segment in DX). */
        cg_emit1(OP_PUSH_AX);       /* push sizeInBytes */
        cg_call_far(mAlloc_id);      /* CALL FAR SYSTEM_Alloc; cleans 2 bytes */
        /* For tagged records: write descriptor offset at ES:[BX] (the tag slot),
           then advance AX by 2 so the returned pointer points to the record data. */
        if (is_tagged_rec) {
            cg_dxax_to_esbx();                              /* ES:BX = raw block start */
            cg_store_word_esbx_imm(ptr.type->base->desc_ofs); /* ES:[BX] = desc_ofs */
            cg_emit2(0x40, 0x40);                           /* INC AX / INC AX  (+2) */
        }
        /* Result: DX:AX = far pointer to record data; store into ptr variable */
        cg_store_item(&ptr);
        break;
    }
    case SP_DISPOSE: {
        Item ptr; int mFree_id;
        int is_tagged_rec;
        parse_designator(&ptr);
        if (ptr.type->form!=TF_POINTER && ptr.type->form!=TF_ADDRESS)
            error("DISPOSE requires pointer or ADDRESS");
        is_tagged_rec = (ptr.type->form == TF_POINTER && ptr.type->base &&
                         ptr.type->base->form == TF_RECORD);
        /* Call far SYSTEM_Free(p: POINTER): VOID
           Pascal convention: Free cleans 4 bytes (RETF 4).
           Push far pointer {segment, offset} as 4-byte arg. */
        mFree_id = get_system_import("SYSTEM_Free");
        cg_load_item(&ptr);  /* DX:AX = far pointer (ptr type) */
        /* For tagged records: subtract 2 from offset to get original block start */
        if (is_tagged_rec) cg_emit3(0x83, 0xE8, 0x02);  /* SUB AX, 2 */
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
    default:
        error("unsupported built-in statement");
        break;
    }
    expect(T_RPAREN);
}

/* Emit an inline procedure call.
   Two modes, selected by whether the byte pattern references any formal parameter:

   ADDRESS mode (pattern has param-name entries):
     Collect actual argument addresses without pushing; each param-name entry in the
     pattern emits a 2-byte little-endian address (BP offset for locals, DS offset
     for globals with DATA reloc).  No PUSH/CALL/RET — raw bytes only.
     Used for: INLINE(opcode, opcode, paramName) style (e.g. LoadWord).

   VALUE mode (pattern has no param-name entries, all entries are raw bytes):
     Push actual argument values via parse_actual_params (same as a normal call),
     then emit the raw byte pattern.  The pattern uses POP instructions to consume
     the pushed arguments and MUST restore SP to its value before the pushes.
     Used for: INLINE with POP-based patterns (e.g. SYSTEM.MOVE, SYSTEM.FILL). */
void pe_emit_inline_call(TypeDesc *pt) {
    int i;
    int has_param_ref = 0;
    for (i = 0; i < pt->n_inline; i++)
        if (pt->inline_data[i].is_param) { has_param_ref = 1; break; }

    if (has_param_ref) {
        /* ADDRESS mode: collect actual addresses, no pushing. */
        int32_t arg_adr[32];
        int     arg_is_global[32];
        int     n_args = 0;
        Symbol *formal;

        expect(T_LPAREN);
        formal = pt->params;
        while (sc->sym != T_RPAREN && sc->sym != T_EOF) {
            Item arg;
            parse_designator(&arg);
            if (n_args < 32) {
                arg_adr[n_args]       = arg.adr;
                arg_is_global[n_args] = (arg.mode == M_GLOBAL) ? 1 : 0;
                n_args++;
            }
            (void)formal;
            if (formal) formal = formal->next;
            if (sc->sym == T_COMMA) scanner_next(sc);
        }
        expect(T_RPAREN);

        for (i = 0; i < pt->n_inline; i++) {
            InlineEntry *e = &pt->inline_data[i];
            if (!e->is_param) {
                cg_emit1(e->raw_byte);
            } else {
                int idx     = e->param_idx;
                int32_t adr = (idx < n_args) ? arg_adr[idx] : 0;
                int global  = (idx < n_args) ? arg_is_global[idx] : 0;
                if (global) {
                    rdf_add_reloc(&cg_obj, SEG_CODE, (uint32_t)cg_pc(), 2, SEG_DATA, 0);
                    cg_emitw((uint16_t)(adr & 0xFFFF));
                } else {
                    cg_emitw((uint16_t)(int16_t)adr);
                }
            }
        }
    } else {
        /* VALUE mode: push actual argument values, then emit raw byte pattern.
           The pattern MUST consume exactly what was pushed (SP-balance rule). */
        if (pt->n_params > 0 || sc->sym == T_LPAREN)
            parse_actual_params(pt);
        for (i = 0; i < pt->n_inline; i++)
            cg_emit1(pt->inline_data[i].raw_byte);
    }
}

void parse_statement(TypeDesc *ret_type) {
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
        } else if (item.mode==M_PROC && item.type && item.type->is_inline) {
            /* inline procedure: emit byte pattern with actual arg address substitution */
            if (sc->sym==T_LPAREN || item.type->n_params > 0)
                pe_emit_inline_call(item.type);
        } else if (item.mode==M_PROC || item.mode==M_IMPORT) {
            if (sc->sym==T_LPAREN || (item.type && item.type->n_params>0)) {
                parse_actual_params(item.type ? item.type : type_new_proc(type_notype));
            }
            /* push static link after params, before CALL, for nested procs */
            if (item.mode==M_PROC && item.type && item.type->has_sl)
                emit_push_static_link(item.val);
            if (item.mode==M_IMPORT && item.is_far) cg_call_far(item.rdoff_id);
            else if (item.mode==M_IMPORT)           cg_call_near(item.rdoff_id);
            else if (item.fwd_sym)                  fwd_emit_call((Symbol *)item.fwd_sym, item.is_far);
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

void parse_stat_seq(TypeDesc *ret_type) {
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

/* Emit the type descriptor for a newly created RECORD type into the data
   segment.  The descriptor is a zero-terminated array of WORDs listing the
   tag IDs of the type itself and all its ancestors (most-derived first).
   Sets rec->desc_ofs to the data-segment offset of the array. */
static void emit_type_descriptor(TypeDesc *rec) {
    TypeDesc *t;
    rec->desc_ofs = cg_dpc();
    for (t = rec; t != NULL; t = t->base)
        cg_emit_data_word((uint16_t)t->tag_id);
    cg_emit_data_word(0);  /* sentinel */
}

/* ================================================================
   TYPES
   ================================================================ */
void parse_type(TypeDesc **out) {
    Symbol *sym;
    Symbol *tsym;
    int32_t len;
    TypeDesc *elem;
    TypeDesc *base;
    TypeDesc *rec;
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
            IdentNode *fhead = NULL, *ftail = NULL, *fi;
            int fexp;
            do {
                char nm[NAME_LEN]; strncpy(nm, sc->id, NAME_LEN-1); nm[NAME_LEN-1]='\0';
                scanner_next(sc);
                fexp = 0; if (sc->sym==T_STAR) { fexp = 1; scanner_next(sc); }
                fhead = idlist_append(fhead, &ftail, nm, fexp);
                if (sc->sym==T_COMMA) scanner_next(sc);
            } while (sc->sym==T_IDENT);
            expect(T_COLON);
            ft = type_integer; parse_type(&ft);
            for (fi = fhead; fi; fi = fi->next) {
                Symbol *fs = type_add_field(rec, fi->name, ft);
                fs->exported = fi->exported;
            }
            idlist_free(fhead);
            if (sc->sym==T_SEMI) scanner_next(sc);
        }
        expect(T_END);
        emit_type_descriptor(rec);
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
                IdentNode *phead = NULL, *ptail = NULL, *pi;
                int typeless_var2 = 0;
                is_var = (sc->sym==T_VAR); if (is_var) scanner_next(sc);
                do {
                    char nm[NAME_LEN]; strncpy(nm, sc->id, NAME_LEN-1); nm[NAME_LEN-1]='\0';
                    scanner_next(sc);
                    phead = idlist_append(phead, &ptail, nm, 0);
                    if (sc->sym==T_COMMA) scanner_next(sc);
                } while (sc->sym!=T_COLON && sc->sym!=T_SEMI && sc->sym!=T_RPAREN && sc->sym!=T_EOF);
                if (sc->sym==T_COLON) {
                    scanner_next(sc);
                    ptype = type_integer; parse_type(&ptype);
                } else {
                    if (!is_var) error("parameter requires a type");
                    typeless_var2 = 1;
                    ptype = type_address;
                }
                for (pi = phead; pi; pi = pi->next) {
                    Symbol *tp = type_add_param(pt, pi->name, ptype, is_var);
                    if (typeless_var2) tp->typeless = 1;
                }
                idlist_free(phead);
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
void parse_const_decl(void) {
    char name[NAME_LEN];
    int exported;
    Item val;
    Symbol *s;
    while (sc->sym==T_IDENT) {
        strncpy(name,sc->id,NAME_LEN-1); scanner_next(sc);
        if (sym_find_local(name)) { pe_error2("duplicate identifier", name); }
        exported = 0; if (sc->sym==T_STAR) { exported=1; scanner_next(sc); }
        expect(T_EQL);
        parse_expr(&val);
        s = sym_new(name, K_CONST);
        s->exported = exported;
        s->type = val.type;
        s->val  = (val.mode==M_CONST) ? val.val : 0;
        s->adr  = (val.mode==M_CONST) ? val.adr : 0;
        if (sc->sym==T_SEMI) scanner_next(sc);
    }
}

void parse_type_decl(void) {
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
        if (sym_find_local(name)) { pe_error2("duplicate identifier", name); }
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
        memcpy(placeholder, t, sizeof(*placeholder));
        /* Redirect forward-ref entries that point to t (a stub created during
           parse_type above) to point to placeholder instead, so the resolution
           loop patches placeholder->base rather than the now-orphaned stub. */
        for (i = 0; i < n_fwd_refs; i++)
            if (fwd_refs[i].ptr == t) fwd_refs[i].ptr = placeholder;
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

void parse_var_decl(void) {
    while (sc->sym==T_IDENT) {
        IdentNode *head = NULL, *tail = NULL, *it;
        TypeDesc *t;
        Symbol *s;
        char gname[NAME_LEN*2];
        int exported;
        do {
            char nm[NAME_LEN]; strncpy(nm, sc->id, NAME_LEN-1); nm[NAME_LEN-1]='\0';
            scanner_next(sc);
            exported = 0;
            if (sc->sym==T_STAR) { exported = 1; scanner_next(sc); }
            head = idlist_append(head, &tail, nm, exported);
            if (sc->sym==T_COMMA) scanner_next(sc);
        } while (sc->sym==T_IDENT);
        expect(T_COLON);
        t = type_integer; parse_type(&t);
        for (it = head; it; it = it->next) {
            if (sym_find_local(it->name)) { pe_error2("duplicate identifier", it->name); }
            s = sym_new(it->name, K_VAR);
            s->exported = it->exported;
            s->type = t;
            if (top_scope->level==0) {
                /* global: allocate in data segment */
                s->adr = cg_dpc();
                cg_emit_data_zero(t->size);
                if (s->exported) {
                    snprintf(gname, sizeof(gname), "%s_%s", cur_mod, it->name);
                    rdf_add_global(&cg_obj, gname, SEG_DATA, s->adr);
                }
            } else {
                sym_alloc_local(s);
            }
        }
        idlist_free(head);
        if (sc->sym==T_SEMI) scanner_next(sc);
    }
}

void parse_proc_decl(void);

void parse_decl_seq(void) {
    while (sc->sym==T_CONST || sc->sym==T_TYPE ||
           sc->sym==T_VAR   || sc->sym==T_PROCEDURE) {
        if      (sc->sym==T_CONST)     { scanner_next(sc); parse_const_decl(); }
        else if (sc->sym==T_TYPE)      { scanner_next(sc); parse_type_decl(); }
        else if (sc->sym==T_VAR)       { scanner_next(sc); parse_var_decl(); }
        else if (sc->sym==T_PROCEDURE) { parse_proc_decl(); }
    }
}

void parse_proc_decl(void) {
    char name[NAME_LEN];
    Symbol *sym;
    int exported;
    TypeDesc *pt;
    int is_var;
    TypeDesc *ptype;
    Symbol *p;
    Symbol *s;
    char gname[NAME_LEN*2];
    uint16_t prologue_patch;
    uint16_t local_sz;

    scanner_next(sc);  /* consume PROCEDURE */
    if (sc->sym!=T_IDENT) { error("procedure name expected"); return; }
    strncpy(name,sc->id,NAME_LEN-1); scanner_next(sc);

    /* Implementation of a forward-declared procedure:
       "PROCEDURE name;" with no parameters, no return type, no modifiers. */
    { Symbol *existing = sym_find_local(name);
      if (existing && existing->fwd_decl) {
        /* Consume optional semicolon after name (no params/modifiers allowed) */
        expect(T_SEMI);
        sym = existing;
        pt  = sym->type;
        exported = sym->exported;
        /* Open scope and re-insert parameters so the body can reference them */
        sym_open_scope();
        for (p = pt->params; p; p = p->next) {
            s = sym_new(p->name, p->kind);
            s->type     = p->type;
            s->adr      = p->adr;
            s->typeless = p->typeless;
        }
        /* Emit the body (same as normal proc below, but we patch forward calls after) */
        goto emit_body;
      }
    }

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
            IdentNode *phead = NULL, *ptail = NULL, *pi;
            int typeless_var = 0;
            is_var=(sc->sym==T_VAR); if (is_var) scanner_next(sc);
            /* ident list — stop at ':' (typed) or ';'/')' (typeless VAR) */
            do {
                char nm[NAME_LEN]; strncpy(nm, sc->id, NAME_LEN-1); nm[NAME_LEN-1]='\0';
                scanner_next(sc);
                phead = idlist_append(phead, &ptail, nm, 0);
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
            for (pi = phead; pi; pi = pi->next) {
                p = type_add_param(pt, pi->name, ptype, is_var);
                /* also insert into scope */
                s = sym_new(pi->name, p->kind);
                s->type = ptype;
                if (typeless_var) { p->typeless = 1; s->typeless = 1; }
            }
            idlist_free(phead);
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

    /* FORWARD modifier: declaration only; body must follow later in the same DeclSeq.
       INLINE and EXTERNAL procs may not be FORWARD. */
    if (sc->sym == T_IDENT && strcmp(sc->id, "FORWARD") == 0) {
        scanner_next(sc);
        expect(T_SEMI);
        type_calc_arg_size(pt);
        sym->fwd_decl = 1;
        sym_close_scope();
        return;
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

    /* INLINE modifier: no body, no CALL; byte pattern emitted at each use site.
       Syntax: INLINE(entry {, entry})
       entry = hex-literal | formal-param-name
       hex-literal → raw byte; formal-param-name → 2-byte signed offset placeholder. */
    if (sc->sym == T_IDENT && strcmp(sc->id, "INLINE") == 0) {
        /* Collect inline entries into a dynamically-grown array. */
        InlineEntry *ibuf = NULL;
        int ibuf_cap = 0, ibuf_len = 0;
        scanner_next(sc);
        expect(T_LPAREN);
        while (sc->sym != T_RPAREN && sc->sym != T_EOF) {
            InlineEntry ent;
            if (sc->sym == T_INT) {
                /* raw byte literal */
                ent.is_param = 0;
                ent.raw_byte = (uint8_t)(sc->ival & 0xFF);
                ent.param_idx = 0;
                scanner_next(sc);
            } else if (sc->sym == T_IDENT) {
                /* formal parameter name: look up its 0-based index */
                int idx = 0;
                Symbol *fp;
                int found = 0;
                for (fp = pt->params; fp; fp = fp->next) {
                    if (strcmp(fp->name, sc->id) == 0) { found = 1; break; }
                    idx++;
                }
                if (!found) { error("unknown parameter name in INLINE"); idx = 0; }
                ent.is_param = 1;
                ent.raw_byte = 0;
                ent.param_idx = idx;
                scanner_next(sc);
            } else {
                error("byte literal or parameter name expected in INLINE");
                break;
            }
            /* grow buffer if needed */
            if (ibuf_len >= ibuf_cap) {
                int new_cap = ibuf_cap ? ibuf_cap * 2 : 16;
                InlineEntry *nb = (InlineEntry *)malloc((size_t)new_cap * sizeof(InlineEntry));
                if (!nb) { error("out of memory"); break; }
                if (ibuf) { memcpy(nb, ibuf, (size_t)ibuf_len * sizeof(InlineEntry)); free(ibuf); }
                ibuf = nb; ibuf_cap = new_cap;
            }
            ibuf[ibuf_len++] = ent;
            if (sc->sym == T_COMMA) scanner_next(sc);
        }
        expect(T_RPAREN);
        expect(T_SEMI);
        pt->is_inline   = 1;
        pt->inline_data = ibuf;
        pt->n_inline    = ibuf_len;
        /* arg_size is not used for inline (no CALL), but calc it for correctness */
        type_calc_arg_size(pt);
        /* inline procs have no code body — no RDOFF GLOBAL record needed;
           the byte pattern is exported via the .def file (INLINE/BYTES lines). */
        sym_close_scope();
        return;
    }

    emit_body:
    type_calc_arg_size(pt);

    /* copy BP offsets from pt->params into scope symbols.
       Scope holds the just-created param symbols (prepended, reverse order);
       sym_find_local walks it once per param — O(n²) worst case but n is the
       number of formal parameters for a single procedure, which is small. */
    for (p = pt->params; p; p = p->next) {
        Symbol *ss = sym_find_local(p->name);
        if (ss) ss->adr = p->adr;
    }

    /* emit prologue placeholder */
    sym->code_ofs = cg_pc();
    if (exported) {
        snprintf(gname, sizeof(gname), "%s_%s", cur_mod, name);
        rdf_add_global(&cg_obj, gname, SEG_CODE, sym->code_ofs);
    }

    /* save outer RETURN patches (this proc gets its own) */
    { BpNode *saved_ret_list = ret_patch_list;
      uint16_t saved_prologue_patch = cur_prologue_patch;
      int saved_proc_depth = cur_proc_depth;
      Backpatch skip_nested;

    cur_proc_depth++;        /* entering this proc's body */
    ret_patch_list = NULL;   /* reset RETURN backpatch list for this procedure */

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
    bp_list_patch_free(ret_patch_list);
    if (pt->is_far) cg_epilogue_far(pt->arg_size);
    else            cg_epilogue(pt->arg_size);
    sym_close_scope();

    /* restore outer procedure's RETURN patches, cur_prologue_patch, and proc depth */
    ret_patch_list = saved_ret_list;
    cur_prologue_patch = saved_prologue_patch;
    cur_proc_depth = saved_proc_depth;
    } /* end of saved-context block */

    /* Patch all call sites that referenced this FORWARD-declared procedure */
    if (sym->fwd_decl) {
        fwd_patch_calls(sym, (uint16_t)sym->code_ofs);
        sym->fwd_decl = 0;
    }

    expect(T_SEMI);
}

/* ================================================================
   MODULE
   ================================================================ */

/* import list for __init: module names that need __init calls (linked list) */
typedef struct InitImp { char name[NAME_LEN]; struct InitImp *next; } InitImp;
static InitImp *init_imports_head = NULL;
static InitImp *init_imports_tail = NULL;
static int      n_init_imports    = 0;

static void init_imports_reset(void) {
    InitImp *p = init_imports_head;
    while (p) { InitImp *nx = p->next; free(p); p = nx; }
    init_imports_head = init_imports_tail = NULL;
    n_init_imports = 0;
}

static void init_imports_add(const char *name) {
    InitImp *p = (InitImp *)malloc(sizeof(InitImp));
    if (!p) { pe_error("out of memory"); return; }
    strncpy(p->name, name, NAME_LEN-1);
    p->name[NAME_LEN-1] = '\0';
    p->next = NULL;
    if (init_imports_tail) init_imports_tail->next = p;
    else                   init_imports_head = p;
    init_imports_tail = p;
    n_init_imports++;
}

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
    char dep_init[NAME_LEN*2];
    int dep_id;
    char rdf_name[NAME_LEN+4];
    char def_name[NAME_LEN+4];
    char lib_name[NAME_LEN+4];
    FILE *df;
    FILE *lf;
    uint8_t *rdf_buf;
    long rdf_len;
    uint8_t *def_buf;
    long def_len;

    sc = s;
    sym_init();
    cg_init();
    /* Reserve data segment offset 0: open-array calling convention passes the array's
       data-segment address as a word, and the callee also reads that word as LEN(arr).
       A global variable at offset 0 would make LEN=0 in all open-array callees.
       Emitting 2 zero bytes here ensures the first real variable gets offset >= 2. */
    cg_emit_data_zero(2);
    parser_errors = 0;
    init_imports_reset();
    mod_uses_fpu = 0;

    /* SYSTEM is implicitly imported by every module unless -SYSTEM mode is active.
       SYSTEM is NEVER added to init_imports — __init never calls SYSTEM__init. */
    if (!parser_system_mode)
        add_implicit_import("SYSTEM");

    expect(T_MODULE);
    if (sc->sym!=T_IDENT) { error("module name expected"); return; }
    strncpy(mod_name, sc->id, NAME_LEN-1); scanner_next(sc);
    strncpy(parser_mod_name, mod_name, sizeof(parser_mod_name)-1);
    parser_mod_name[sizeof(parser_mod_name)-1] = '\0';

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
                if (strcmp(iname, "SYSTEM") != 0) init_imports_add(iname);
                /* load .def for type-aware access — fatal if not found */
                load_explicit_import(iname, alias);
            }
            if (sc->sym==T_COMMA) scanner_next(sc);
        }
        expect(T_SEMI);
    }

    parse_decl_seq();

    /* Check for unresolved FORWARD declarations */
    { Symbol *sym;
      for (sym = top_scope->symbols; sym; sym = sym->next) {
          if (sym->kind == K_PROC && sym->fwd_decl) {
              fprintf(stderr, "%s: unresolved FORWARD procedure '%s'\n",
                      s->filename, sym->name);
              parser_errors++;
          }
      }
    }

    /* validate -entry proc: must exist at module level, be K_PROC, and be exported */
    if (parser_entry_proc && parser_errors == 0) {
        Symbol *ep = sym_find(parser_entry_proc);
        if (!ep || ep->kind != K_PROC || ep->level != 0) {
            fprintf(stderr, "%s: -entry '%s': procedure not found in module\n",
                    s->filename, parser_entry_proc);
            parser_errors++;
        } else if (!ep->exported) {
            fprintf(stderr, "%s: -entry '%s': procedure is not exported (add *)\n",
                    s->filename, parser_entry_proc);
            parser_errors++;
        }
    }

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
        guard_ofs = cg_pc();
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
        { InitImp *ip;
          for (ip = init_imports_head; ip; ip = ip->next) {
              snprintf(dep_init, sizeof(dep_init), "%s__init", ip->name);
              dep_id = rdf_add_import(&cg_obj, dep_init);
              cg_call_far(dep_id);
          } }

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

        /* read .rdf and .def into memory for tar packaging */
        read_file_to_buf(rdf_name, &rdf_buf, &rdf_len);
        read_file_to_buf(def_name, &def_buf, &def_len);

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
                uint8_t *ebuf; long elen;
                char user_name[128];
                for (er = parser_extra_rdfs_head; er; er = er->next) {
                    if (read_file_to_buf(er->path, &ebuf, &elen) == 0) {
                        str_upcase(up_name, (int)sizeof(up_name), path_basename(er->path));
                        snprintf(user_name, sizeof(user_name), "USER/%s", up_name);
                        tar_add_file(lf, user_name, ebuf, (size_t)elen);
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
