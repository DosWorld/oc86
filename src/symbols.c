#include "symbols.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

TypeDesc *type_integer, *type_boolean, *type_char,
         *type_byte, *type_set, *type_niltype, *type_notype,
         *type_address, *type_longint, *type_real, *type_longreal;
Scope    *top_scope, *universe;
int       next_tag_id = 1;  /* 0 reserved; each record type gets a unique ID */

/* ---- allocators ---- */
static TypeDesc *talloc(void) {
    TypeDesc *t = calloc(1, sizeof(TypeDesc));
    t->tag_ofs = -1;
    return t;
}
static Symbol *salloc(void) { return calloc(1, sizeof(Symbol)); }
static Scope  *calloc_scope(void) { return calloc(1, sizeof(Scope)); }

TypeDesc *type_new(int form, int32_t size) {
    TypeDesc *t = talloc();
    t->form  = form;
    t->size  = size;
    t->align = size > 2 ? 2 : size;
    return t;
}

TypeDesc *type_new_array(TypeDesc *elem, int32_t len) {
    TypeDesc *t = type_new(TF_ARRAY, 0);
    t->elem = elem;
    t->len  = len;
    t->size = (len >= 0) ? len * elem->size : SZ_POINTER;
    return t;
}

TypeDesc *type_new_record(TypeDesc *base) {
    TypeDesc *t = type_new(TF_RECORD, 0);
    t->base   = base;
    t->tag_id = next_tag_id++;
    if (base) {
        t->size    = base->size;
        t->tag_ofs = -1;
    } else {
        t->tag_ofs = -1;
        t->size    = 0;  /* fields start at offset 0; tag is before the object */
    }
    return t;
}

Symbol *type_add_field(TypeDesc *rec, const char *name, TypeDesc *ftype) {
    int32_t align;
    int32_t off;
    Symbol *f = salloc();
    strncpy(f->name, name, NAME_LEN-1);
    f->kind   = K_FIELD;
    f->type   = ftype;
    /* align */
    align = ftype->align < 1 ? 1 : ftype->align;
    off   = rec->size;
    if (align > 1 && off % align != 0) off += align - (off % align);
    f->offset = off;
    rec->size  = off + ftype->size;
    rec->n_fields++;
    /* append O(1) via tail pointer */
    if (rec->fields_tail) rec->fields_tail->next = f;
    else                  rec->fields = f;
    rec->fields_tail = f;
    return f;
}

TypeDesc *type_new_pointer(TypeDesc *base) {
    TypeDesc *t = type_new(TF_POINTER, SZ_POINTER);
    t->base = base;
    return t;
}

TypeDesc *type_new_proc(TypeDesc *ret) {
    TypeDesc *t = type_new(TF_PROC, SZ_POINTER);
    t->ret_type = ret ? ret : type_notype;
    return t;
}

Symbol *type_add_param(TypeDesc *pt, const char *name,
                       TypeDesc *ptype, int is_var) {
    Symbol *p = salloc();
    strncpy(p->name, name, NAME_LEN-1);
    p->kind = is_var ? K_VARPARAM : K_PARAM;
    p->type = ptype;
    pt->n_params++;
    /* append O(1) via tail pointer */
    if (pt->params_tail) pt->params_tail->next = p;
    else                 pt->params = p;
    pt->params_tail = p;
    return p;
}

/* Return the stack slot size for a formal parameter.
   VAR open-array params (K_VARPARAM, len=-1): 6 bytes — {far_ptr(4), LEN(2)}.
   Other VAR params: SZ_POINTER = 4 bytes — far address {offset:2, segment:2}.
   Non-VAR open-array params (len=-1): 6 bytes — {far_ptr(4), LEN(2)}.
     Caller pushes LEN first (deepest/highest BP offset), then segment, then offset.
     Callee layout: [adr]=offset, [adr+2]=segment, [adr+4]=LEN.
     LES BX,[BP+adr] loads far ptr; LEN(s) reads [BP+adr+4].
   All other non-VAR params: their natural type size (minimum 2). */
static int32_t param_slot_size(Symbol *p) {
    if (p->kind == K_VARPARAM) {
        /* VAR open-array: 6 bytes {far_ptr(4), LEN(2)}; other VAR: 4 bytes */
        if (p->type->form == TF_ARRAY && p->type->len < 0) return 6;
        return SZ_POINTER;
    }
    if (p->type->form == TF_ARRAY && p->type->len < 0) return 6; /* open array: far_ptr+LEN */
    return (p->type->size < 2) ? 2 : p->type->size;
}

void type_calc_arg_size(TypeDesc *pt) {
    Symbol *p;
    int32_t total = 0;
    int32_t ofs;
    /* Pass 1: total bytes */
    for (p = pt->params; p; p = p->next)
        total += param_slot_size(p);
    /* has_sl: nested proc gets hidden static link pushed last by caller, sits at [BP+4].
       Params are therefore shifted to start at [BP+6] instead of [BP+4].
       arg_size includes the 2-byte SL slot so epilogue (RET arg_size) cleans it. */
    pt->arg_size = total + (pt->has_sl ? 2 : 0);
    /* Pass 2: assign BP offsets
       Pascal left-to-right push: first param deepest.
       NEAR (not exported): 2-byte return addr at [BP+2]; params start at BP+4 (or BP+6 if has_sl).
       FAR  (exported):     4-byte return addr ([BP+2]=IP, [BP+4]=CS); params at BP+6.
       Walk forward, assigning from high to low. */
    ofs = (pt->is_far ? 6 : (pt->has_sl ? 6 : 4)) + total;
    for (p = pt->params; p; p = p->next) {
        int32_t sz = param_slot_size(p);
        ofs -= sz;
        p->adr = ofs;
    }
}

int type_is_extension(TypeDesc *ext, TypeDesc *base) {
    while (ext && ext != base) ext = ext->base;
    return ext == base;
}

int type_assign_compat(TypeDesc *dst, TypeDesc *src) {
    if (dst == src) return 1;
    if (dst->form == TF_INTEGER && src->form == TF_BYTE)    return 1;
    if (dst->form == TF_BYTE    && src->form == TF_INTEGER) return 1;
    if (dst->form == TF_CHAR    && src->form == TF_BYTE)    return 1;
    /* LONGINT ↔ INTEGER ↔ BYTE widening/narrowing allowed via explicit cast only;
       for assignment, LONGINT accepts INTEGER and BYTE as widening. */
    if (dst->form == TF_LONGINT && (src->form == TF_INTEGER || src->form == TF_BYTE)) return 1;
    if (dst->form == TF_POINTER && src->form == TF_NILTYPE) return 1;
    if (dst->form == TF_POINTER && src->form == TF_POINTER)
        return type_is_extension(src->base, dst->base);
    if (dst->form == TF_RECORD  && src->form == TF_RECORD)
        return type_is_extension(src, dst);
    /* SYSTEM.ADDRESS: compatible with any POINTER or INTEGER in both directions */
    if (dst->form == TF_ADDRESS &&
        (src->form == TF_POINTER || src->form == TF_INTEGER || src->form == TF_ADDRESS)) return 1;
    if (src->form == TF_ADDRESS &&
        (dst->form == TF_POINTER || dst->form == TF_INTEGER || dst->form == TF_ADDRESS)) return 1;
    /* Proc variable assignment: src must be a proc (M_PROC) with same is_far */
    if (dst->form == TF_PROC && src->form == TF_PROC)
        return (dst->is_far == src->is_far) ? 1 : 0;
    return 0;
}

int type_param_compat(TypeDesc *formal, TypeDesc *actual, int is_var) {
    if (is_var) {
        if (formal == actual) return 1;
        if (formal->form==TF_RECORD && actual->form==TF_RECORD)
            return type_is_extension(actual, formal);
        if (formal->form==TF_ARRAY && formal->len<0 && actual->form==TF_ARRAY)
            return formal->elem == actual->elem;
        return 0;
    }
    return type_assign_compat(formal, actual);
}

/* ---- scope management ---- */

void sym_open_scope(void) {
    Scope *s = calloc_scope();
    s->outer     = top_scope;
    s->level     = top_scope ? top_scope->level + 1 : 0;
    s->local_top = 0;    /* locals start at [BP-2], [BP-4], ... */
    s->param_bot =  4;
    top_scope    = s;
}

void sym_close_scope(void) {
    if (top_scope->outer) top_scope = top_scope->outer;
}

Symbol *sym_new(const char *name, int kind) {
    Symbol *s = salloc();
    strncpy(s->name, name, NAME_LEN-1);
    s->kind     = kind;
    s->level    = top_scope->level;
    s->code_ofs = (uint32_t)-1;
    s->rdoff_id = -1;
    s->next     = top_scope->symbols;
    top_scope->symbols = s;
    top_scope->n_syms++;
    return s;
}

void sym_alloc_local(Symbol *sym) {
    int32_t sz = sym->type->size;
    if (sz < 2) sz = 2;    /* minimum word-size slot; keeps frame word-aligned */
    if (top_scope->local_top % 2 != 0) top_scope->local_top--;
    top_scope->local_top -= sz;
    sym->adr = top_scope->local_top;
}

uint16_t sym_local_size(void) {
    int32_t v = -top_scope->local_top;
    return (uint16_t)v;
}

Symbol *sym_find_local(const char *name) {
    Symbol *s;
    for (s = top_scope->symbols; s; s = s->next)
        if (strcmp(s->name, name) == 0) return s;
    return NULL;
}

Symbol *sym_find(const char *name) {
    Scope *sc;
    for (sc = top_scope; sc; sc = sc->outer) {
        Symbol *s;
        for (s = sc->symbols; s; s = s->next)
            if (strcmp(s->name, name) == 0) return s;
    }
    return NULL;
}

Symbol *sym_find_field(TypeDesc *rec, const char *name) {
    while (rec) {
        Symbol *f;
        for (f = rec->fields; f; f = f->next)
            if (strcmp(f->name, name) == 0) return f;
        rec = rec->base;
    }
    return NULL;
}

/* ---- universe init ---- */
static Symbol *predef_type(const char *name, int form, int size) {
    TypeDesc *t = type_new(form, size);
    Symbol   *s = sym_new(name, K_TYPE);
    s->type = t;
    return s;
}
static void predef_const(const char *name, int val, TypeDesc *t) {
    Symbol *s = sym_new(name, K_CONST);
    s->val  = val;
    s->type = t;
}
static void predef_sysproc(const char *name, int id) {
    Symbol *s = sym_new(name, K_SYSPROC);
    s->sys_id = id;
}

void sym_init(void) {
    /* build universe scope at level -1 */
    Scope *u = calloc_scope();
    u->level = -1;
    top_scope = universe = u;

    type_notype  = type_new(TF_NOTYPE, 0);
    type_niltype = type_new(TF_NILTYPE, 0);

    predef_type("INTEGER", TF_INTEGER, SZ_INTEGER);  type_integer = sym_find("INTEGER")->type;
    predef_type("BOOLEAN", TF_BOOLEAN, SZ_BOOLEAN);  type_boolean = sym_find("BOOLEAN")->type;
    predef_type("CHAR",    TF_CHAR,    SZ_CHAR);      type_char    = sym_find("CHAR")->type;
    predef_type("BYTE",    TF_BYTE,    SZ_BYTE);      type_byte    = sym_find("BYTE")->type;
    predef_type("SET",     TF_SET,     SZ_SET);       type_set     = sym_find("SET")->type;
    predef_type("ADDRESS", TF_ADDRESS, SZ_POINTER);  type_address = sym_find("ADDRESS")->type;
    predef_type("LONGINT",  TF_LONGINT,  SZ_LONGINT);  type_longint  = sym_find("LONGINT")->type;
    predef_type("REAL",     TF_REAL,     SZ_REAL);     type_real     = sym_find("REAL")->type;
    predef_type("LONGREAL", TF_LONGREAL, SZ_LONGREAL); type_longreal = sym_find("LONGREAL")->type;

    /* FLOAT is a synonym for REAL (type-cast built-in, same codegen) */
    { Symbol *s = sym_new("FLOAT", K_TYPE); s->type = type_real; }

    predef_const("TRUE",  1, type_boolean);
    predef_const("FALSE", 0, type_boolean);
    predef_const("NIL",   0, type_niltype);

    predef_sysproc("NEW",     SP_NEW);
    predef_sysproc("INC",     SP_INC);
    predef_sysproc("DEC",     SP_DEC);
    predef_sysproc("LEN",     SP_LEN);
    predef_sysproc("ODD",     SP_ODD);
    predef_sysproc("ABS",     SP_ABS);
    predef_sysproc("ASSERT",  SP_ASSERT);
    predef_sysproc("COPY",    SP_COPY);
    predef_sysproc("ORD",     SP_ORD);
    predef_sysproc("CHR",     SP_CHR);
    predef_sysproc("FLOOR",   SP_FLOOR);
    predef_sysproc("ENTIER",  SP_FLOOR);    /* ENTIER is a synonym for FLOOR */
    predef_sysproc("DISPOSE", SP_DISPOSE);  /* SYSTEM extension */

    /* SYSTEM compiler intrinsics — accessible only via SYSTEM.xxx qualified access.
       Pre-declared in universe so sym_find("ADR") etc. succeeds from the
       SYSTEM.xxx dot branch in parse_designator. */
    predef_sysproc("VAL",  SP_VAL);
    predef_sysproc("GET",  SP_GET);
    predef_sysproc("LSL",  SP_LSL);
    predef_sysproc("LSR",  SP_LSR);
    predef_sysproc("ASR",  SP_ASR);
    predef_sysproc("ROR",  SP_ROR);
    predef_sysproc("AND",  SP_AND);
    predef_sysproc("IOR",  SP_IOR);
    predef_sysproc("XOR",  SP_XOR);

    /* SYSTEM: always implicitly available (never listed in IMPORT).
       Pre-declared in universe so SYSTEM.Foo qualified access works. */
    {
        Symbol *s = sym_new("SYSTEM", K_IMPORT);
        strncpy(s->mod_name, "SYSTEM", NAME_LEN-1);
        /* rdoff_id stays -1; individual SYSTEM_xxx imports created on demand */
    }

    sym_open_scope();   /* module scope, level 0 */
}
