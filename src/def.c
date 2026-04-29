#include "def.h"
#include "rdoff.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ---- writer ---- */

/* Find the exported symbol name for a TypeDesc in the current (module-level) scope.
   Used to emit named types (RECORD, POINTER TO SomeRecord) in .def. */
static const char *find_type_name(TypeDesc *t) {
    Symbol *sym;
    for (sym = top_scope->symbols; sym; sym = sym->next) {
        if (sym->kind == K_TYPE && sym->type == t)
            return sym->name;
    }
    return NULL;
}

/* Write a type descriptor token for .def PARAM/FIELD/PROC lines.
   POINTER emits "POINTER <base>" so the base type survives round-trips. */
static void write_type(FILE *f, TypeDesc *t) {
    const char *nm;
    if (!t)                           { fprintf(f, "VOID"); return; }
    switch (t->form) {
    case TF_INTEGER:  fprintf(f, "INTEGER");  return;
    case TF_BOOLEAN:  fprintf(f, "BOOLEAN");  return;
    case TF_CHAR:     fprintf(f, "CHAR");     return;
    case TF_BYTE:     fprintf(f, "BYTE");     return;
    case TF_LONGINT:  fprintf(f, "LONGINT");  return;
    case TF_REAL:     fprintf(f, "REAL");     return;
    case TF_LONGREAL: fprintf(f, "LONGREAL"); return;
    case TF_SET:      fprintf(f, "SET");      return;
    case TF_ADDRESS:  fprintf(f, "ADDRESS");  return;
    case TF_ARRAY:    fprintf(f, "ARRAY");    return;
    case TF_NOTYPE:   fprintf(f, "VOID");     return;
    case TF_POINTER:
        fprintf(f, "POINTER ");
        if (t->base) {
            nm = find_type_name(t->base);
            if (nm) { fprintf(f, "%s", nm); return; }
        }
        fprintf(f, "VOID");
        return;
    case TF_RECORD:
        nm = find_type_name(t);
        if (nm) { fprintf(f, "%s", nm); return; }
        fprintf(f, "VOID");
        return;
    default:
        fprintf(f, "INTEGER"); return; /* fallback */
    }
}

/* Emit one TYPE entry (record with fields, or opaque).
   Returns without writing anything for non-exported or non-TYPE symbols. */
static void write_one_type(FILE *f, const char *mod_name, Symbol *sym) {
    TypeDesc *td = sym->type;
    if (td && td->form == TF_RECORD) {
        Symbol *fld;
        int has_exported = 0;
        for (fld = td->fields; fld; fld = fld->next)
            if (fld->exported) { has_exported = 1; break; }
        if (has_exported) {
            fprintf(f, "TYPE %s_%s RECORD %ld\n",
                    mod_name, sym->name, (long)td->size);
            if (td->base) {
                const char *bnm = find_type_name(td->base);
                if (bnm) fprintf(f, "  BASE %s_%s\n", mod_name, bnm);
            }
            for (fld = td->fields; fld; fld = fld->next) {
                if (!fld->exported) continue;
                fprintf(f, "  FIELD %s ", fld->name);
                write_type(f, fld->type);
                fprintf(f, " %ld\n", (long)fld->offset);
            }
            fprintf(f, "END\n");
        } else {
            fprintf(f, "TYPE %s_%s\n", mod_name, sym->name);
        }
    } else if (td && td->form == TF_POINTER) {
        /* Emit pointer alias so importers know it's a pointer, not opaque. */
        fprintf(f, "TYPE %s_%s POINTER ", mod_name, sym->name);
        write_type(f, td->base);
        fprintf(f, "\n");
    } else if (td && td->form == TF_ADDRESS) {
        fprintf(f, "TYPE %s_%s ADDRESS\n", mod_name, sym->name);
    } else {
        fprintf(f, "TYPE %s_%s\n", mod_name, sym->name);
    }
}

void def_write(FILE *f, const char *mod_name) {
    /* Write order: CONST → TYPE → VAR → PROC
       TYPE is split into two sub-passes (root records before extended) so that
       every BASE reference is defined before it is referenced by the reader. */
    Scope *scope = top_scope;
    Symbol *sym;
    fprintf(f, "MODULE %s\n", mod_name);

    /* Pass 1: CONST */
    for (sym = scope->symbols; sym; sym = sym->next) {
        if (!sym->exported) continue;
        if (strchr(sym->name, '.')) continue;
        if (sym->kind == K_CONST)
            fprintf(f, "CONST %s_%s %ld\n",
                    mod_name, sym->name, (long)sym->val);
    }

    /* Pass 2a: TYPE with no base (root records and all non-record types) */
    for (sym = scope->symbols; sym; sym = sym->next) {
        if (!sym->exported) continue;
        if (strchr(sym->name, '.')) continue;
        if (sym->kind == K_TYPE && sym->type &&
            !(sym->type->form == TF_RECORD && sym->type->base))
            write_one_type(f, mod_name, sym);
    }

    /* Pass 2b: TYPE with a base (extended records) */
    for (sym = scope->symbols; sym; sym = sym->next) {
        if (!sym->exported) continue;
        if (strchr(sym->name, '.')) continue;
        if (sym->kind == K_TYPE && sym->type &&
            sym->type->form == TF_RECORD && sym->type->base)
            write_one_type(f, mod_name, sym);
    }

    /* Pass 3: VAR — format: VAR fullname type seg offset */
    for (sym = scope->symbols; sym; sym = sym->next) {
        if (!sym->exported) continue;
        if (strchr(sym->name, '.')) continue;
        if (sym->kind == K_VAR) {
            int seg = (sym->level == 0) ? SEG_DATA : SEG_CODE;
            fprintf(f, "VAR %s_%s ", mod_name, sym->name);
            write_type(f, sym->type);
            fprintf(f, " %d %ld\n", seg, (long)sym->adr);
        }
    }

    /* Pass 4: PROC and INLINE */
    for (sym = scope->symbols; sym; sym = sym->next) {
        TypeDesc *pt;
        if (!sym->exported) continue;
        if (strchr(sym->name, '.')) continue;
        if (sym->kind != K_PROC) continue;
        pt = sym->type;
        if (pt && pt->is_inline) {
            int j;
            fprintf(f, "INLINE %s_%s ", mod_name, sym->name);
            write_type(f, pt ? pt->ret_type : NULL);
            fprintf(f, "\n");
            if (pt->n_params > 0) {
                Symbol *p;
                for (p = pt->params; p; p = p->next) {
                    if (p->kind == K_VARPARAM && p->typeless)
                        fprintf(f, "  PARAM TYPELESSVAR %s\n", p->name);
                    else if (p->kind == K_VARPARAM)
                        fprintf(f, "  PARAM VAR %s ", p->name);
                    else
                        fprintf(f, "  PARAM %s ", p->name);
                    if (p->kind != K_VARPARAM || !p->typeless) {
                        write_type(f, p->type);
                        fprintf(f, "\n");
                    }
                }
            }
            fprintf(f, "  BYTES");
            for (j = 0; j < pt->n_inline; j++) {
                InlineEntry *e = &pt->inline_data[j];
                if (e->is_param)
                    fprintf(f, " P%d", e->param_idx);
                else
                    fprintf(f, " %d", (int)e->raw_byte);
            }
            fprintf(f, "\n");
            fprintf(f, "END\n");
        } else {
            fprintf(f, "PROC %s_%s %s ", mod_name, sym->name,
                    (pt && pt->is_far) ? "FAR" : "NEAR");
            write_type(f, pt ? pt->ret_type : NULL);
            fprintf(f, "\n");
            if (pt && pt->n_params > 0) {
                Symbol *p;
                for (p = pt->params; p; p = p->next) {
                    if (p->kind == K_VARPARAM && p->typeless)
                        fprintf(f, "  PARAM TYPELESSVAR %s\n", p->name);
                    else if (p->kind == K_VARPARAM)
                        fprintf(f, "  PARAM VAR %s ", p->name);
                    else
                        fprintf(f, "  PARAM %s ", p->name);
                    if (p->kind != K_VARPARAM || !p->typeless) {
                        write_type(f, p->type);
                        fprintf(f, "\n");
                    }
                }
            }
            fprintf(f, "END\n");
        }
    }
}

/* ---- reader ---- */

/* Trim leading whitespace in place (returns pointer to first non-space) */
static char *trim_lead(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

/* Trim trailing newline/carriage-return */
static void trim_nl(char *s) {
    int n = (int)strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r' || s[n-1] == ' ')) s[--n] = '\0';
}

/* Helper: strip "ModuleName_" prefix from fullname to get short name.
   e.g. "SYSTEM_FCarry" with underscore after "SYSTEM" → "FCarry" */
static void make_short(const char *fullname, char *short_name) {
    const char *underscore = strchr(fullname, '_');
    if (underscore)
        strncpy(short_name, underscore + 1, NAME_LEN-1);
    else
        strncpy(short_name, fullname, NAME_LEN-1);
    short_name[NAME_LEN-1] = '\0';
}

/* Alias being read — set by def_read so resolve_type_name can try alias.name */
static char s_cur_alias[NAME_LEN] = "";

/* Look up a type by its short name in the current scope.
   Used when parsing PARAM/FIELD types that refer to other types by name. */
static TypeDesc *resolve_type_name(const char *tname) {
    Symbol *s;
    char qn[NAME_LEN*2];
    if (strcmp(tname, "INTEGER")  == 0) return type_integer;
    if (strcmp(tname, "BOOLEAN")  == 0) return type_boolean;
    if (strcmp(tname, "CHAR")     == 0) return type_char;
    if (strcmp(tname, "BYTE")     == 0) return type_byte;
    if (strcmp(tname, "LONGINT")  == 0) return type_longint;
    if (strcmp(tname, "REAL")     == 0) return type_real;
    if (strcmp(tname, "LONGREAL") == 0) return type_longreal;
    if (strcmp(tname, "SET")      == 0) return type_set;
    if (strcmp(tname, "ADDRESS")  == 0) return type_address;
    if (strcmp(tname, "VOID")     == 0) return type_notype;
    if (strcmp(tname, "ARRAY")    == 0) return type_new_array(type_char, -1); /* open ARRAY OF CHAR */
    /* If tname is already qualified (contains '.'), look it up directly.
       If not found yet, create a forward RECORD stub so POINTER types are
       well-formed before the referenced module's def is loaded. */
    if (strchr(tname, '.')) {
        s = sym_find(tname);
        if (s && s->kind == K_TYPE) return s->type;
        /* Not found: create forward stub so we can form POINTER-to it. */
        s = sym_new(tname, K_TYPE);
        s->type     = type_new_record(NULL);
        s->exported = 1;
        return s->type;
    } else {
        /* Try alias-qualified name first (types from this module: alias.shortname) */
        if (s_cur_alias[0]) {
            snprintf(qn, sizeof(qn), "%s.%s", s_cur_alias, tname);
            s = sym_find(qn);
            if (s && s->kind == K_TYPE) return s->type;
        }
        /* Fall back to plain name (handles pre-loaded imported types) */
        s = sym_find(tname);
        if (s && s->kind == K_TYPE) return s->type;
    }
    /* Unknown type name: warn (likely renamed/stale .def) and fall back to INTEGER. */
    fprintf(stderr, "warning: unknown type '%s' in .def, defaulting to INTEGER\n", tname);
    return type_integer;
}

/* Parse a type token from the .def extended format.
   Forms:  INTEGER | BOOLEAN | CHAR | BYTE | VOID
           POINTER <basetype>
           (other named types resolved via resolve_type_name)
   Returns a TypeDesc*.  *pp advances past consumed tokens. */
static TypeDesc *parse_def_type(char **pp) {
    char *p = *pp;
    char token[NAME_LEN*2];
    int  n = 0;
    /* skip leading whitespace */
    while (*p == ' ' || *p == '\t') p++;
    while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
        if (n < (int)sizeof(token)-1) token[n++] = *p;
        p++;
    }
    token[n] = '\0';
    *pp = p;

    if (strcmp(token, "POINTER") == 0) {
        /* consume base type (may be VOID or a name) */
        TypeDesc *base = parse_def_type(pp);
        return type_new_pointer(base);
    }
    return resolve_type_name(token);
}

/* Deferred BASE patches: BASE lines may reference types not yet loaded.
   Linked list of (derived_rec, base_name_short) pairs; resolved after all types load. */
typedef struct BasePatchNode {
    TypeDesc *derived;
    char      base_short[NAME_LEN];
    struct BasePatchNode *next;
} BasePatchNode;

int def_read(FILE *f, const char *alias) {
    char line[512];
    char mod_name[NAME_LEN] = "";
    TypeDesc *cur_rec     = NULL;
    int32_t   cur_rec_total_size = 0;
    TypeDesc *cur_proc    = NULL;
    TypeDesc *cur_inline  = NULL;
    BasePatchNode *base_patches = NULL;  /* linked list head */
    BasePatchNode *bp_node;

    /* set module-level alias for resolve_type_name */
    strncpy(s_cur_alias, alias, NAME_LEN-1);
    s_cur_alias[NAME_LEN-1] = '\0';

    /* Pre-pass: register all RECORD type stubs before processing fields.
       This avoids "unknown type" warnings when FIELD lines reference types
       defined later in the same .def (circular/forward pointer references). */
    {
        char pline[512];
        char pkeyword[32]; int pkn;
        char pfullname[NAME_LEN*2], pshort[NAME_LEN], pqname[NAME_LEN*2];
        char pmod[NAME_LEN] = "";
        char *pp, *pq;
        while (fgets(pline, sizeof(pline), f)) {
            trim_nl(pline);
            pp = trim_lead(pline);
            pkn = 0; pq = pp;
            while (*pq && *pq != ' ' && *pq != '\t') {
                if (pkn < 31) pkeyword[pkn++] = *pq; pq++;
            }
            pkeyword[pkn] = '\0';
            pq = trim_lead(pq);
            if (strcmp(pkeyword, "MODULE") == 0) {
                sscanf(pq, "%63s", pmod);
            } else if (strcmp(pkeyword, "TYPE") == 0 && pmod[0]) {
                char psubkw[16] = "";
                pfullname[0] = '\0';
                sscanf(pq, "%63s %15s", pfullname, psubkw);
                if (strcmp(psubkw, "RECORD") == 0) {
                    make_short(pfullname, pshort);
                    snprintf(pqname, sizeof(pqname), "%s.%s", alias, pshort);
                    /* Register stub only if not already present */
                    if (!sym_find(pqname)) {
                        Symbol *ps = sym_new(pqname, K_TYPE);
                        ps->type     = type_new_record(NULL);
                        ps->exported = 1;
                        strncpy(ps->mod_name, alias, NAME_LEN-1);
                    }
                }
            }
        }
        rewind(f);
    }

    while (fgets(line, sizeof(line), f)) {
        char *p;
        char keyword[32];
        int  kn;
        char *q;
        char fullmod[NAME_LEN*2];
        char fullname[NAME_LEN*2];
        char short_name[NAME_LEN];
        char fname[NAME_LEN];
        char pname[NAME_LEN];
        char subkw[16];
        char kw2[NAME_LEN];  /* for PARAM VAR detection */
        int is_var2;         /* 1 if VAR param in .def */
        int32_t offset;
        TypeDesc *ft;
        TypeDesc *ptype;
        TypeDesc *ret;
        TypeDesc *pt;
        TypeDesc *rec;
        Symbol *s;
        Symbol *fs;
        long    val_l;   /* long for sscanf %ld; assigned to int32_t after */
        long    ofs_l;   /* long for sscanf %ld offset reads */
        int32_t val;
        int seg;
        int32_t total_size;
        char *rest;

        trim_nl(line);
        p = trim_lead(line);
        if (*p == '\0' || *p == '#') continue;

        kn = 0;
        q = p;
        while (*q && *q != ' ' && *q != '\t') {
            if (kn < 31) keyword[kn++] = *q;
            q++;
        }
        keyword[kn] = '\0';
        q = trim_lead(q);   /* q now points past keyword */

        /* ── END: close current RECORD, PROC, or INLINE block ── */
        if (strcmp(keyword, "END") == 0) {
            if (cur_rec) {
                /* Restore authoritative total size declared in .def header.
                   type_add_field() accumulates rec->size, but .def provides
                   explicit field offsets and a precise total size — use it. */
                cur_rec->size = cur_rec_total_size;
                cur_rec = NULL;
            } else if (cur_inline) {
                type_calc_arg_size(cur_inline);
                cur_inline = NULL;
            } else if (cur_proc) {
                type_calc_arg_size(cur_proc);
                cur_proc = NULL;
            }
            continue;
        }

        /* ── BYTES: inline byte pattern for current INLINE proc ── */
        if (strcmp(keyword, "BYTES") == 0 && cur_inline) {
            /* parse space-separated entries: decimal integer or P<n> for param ref */
            char *bp = q;
            InlineEntry *ibuf = NULL;
            int ibuf_cap = 0, ibuf_len = 0;
            while (*bp) {
                InlineEntry ent;
                while (*bp == ' ' || *bp == '\t') bp++;
                if (*bp == '\0') break;
                if (*bp == 'P' || *bp == 'p') {
                    /* param reference P<idx> */
                    int idx = 0;
                    bp++;
                    while (*bp >= '0' && *bp <= '9') { idx = idx * 10 + (*bp - '0'); bp++; }
                    ent.is_param = 1;
                    ent.raw_byte = 0;
                    ent.param_idx = idx;
                } else {
                    /* raw byte decimal */
                    long bv = 0;
                    while (*bp >= '0' && *bp <= '9') { bv = bv * 10 + (*bp - '0'); bp++; }
                    ent.is_param = 0;
                    ent.raw_byte = (uint8_t)(bv & 0xFF);
                    ent.param_idx = 0;
                }
                /* grow buffer if needed */
                if (ibuf_len >= ibuf_cap) {
                    int new_cap = ibuf_cap ? ibuf_cap * 2 : 16;
                    InlineEntry *nb = (InlineEntry *)malloc((size_t)new_cap * sizeof(InlineEntry));
                    if (!nb) break;
                    if (ibuf) { memcpy(nb, ibuf, (size_t)ibuf_len * sizeof(InlineEntry)); free(ibuf); }
                    ibuf = nb; ibuf_cap = new_cap;
                }
                ibuf[ibuf_len++] = ent;
            }
            cur_inline->inline_data = ibuf;
            cur_inline->n_inline    = ibuf_len;
            continue;
        }

        /* ── BASE: record base for current RECORD (deferred: base may not be loaded yet) ── */
        if (strcmp(keyword, "BASE") == 0 && cur_rec) {
            char base_full[NAME_LEN*2];
            char base_short[NAME_LEN];
            base_full[0] = '\0';
            sscanf(q, "%63s", base_full);
            make_short(base_full, base_short);
            bp_node = (BasePatchNode *)malloc(sizeof(BasePatchNode));
            if (bp_node) {
                bp_node->derived = cur_rec;
                strncpy(bp_node->base_short, base_short, NAME_LEN-1);
                bp_node->base_short[NAME_LEN-1] = '\0';
                bp_node->next  = base_patches;
                base_patches   = bp_node;
            }
            continue;
        }

        /* ── FIELD: add field to current RECORD ── */
        if (strcmp(keyword, "FIELD") == 0 && cur_rec) {
            /* FIELD <name> <type...> <offset>
               type may be multi-token (e.g. "POINTER VOID") so parse name first,
               then use parse_def_type, then read trailing offset. */
            char *qf = q;
            fname[0] = '\0';
            sscanf(qf, "%32s", fname);
            /* advance past fname */
            while (*qf && *qf != ' ' && *qf != '\t') qf++;
            qf = trim_lead(qf);
            ft = parse_def_type(&qf);
            ofs_l = 0;
            sscanf(qf, "%ld", &ofs_l);
            offset = (int32_t)ofs_l;
            fs = type_add_field(cur_rec, fname, ft);
            /* override offset from .def (authoritative) */
            fs->offset = offset;
            (void)fs;
            continue;
        }

        /* ── PARAM: add parameter to current PROC or INLINE block ── */
        if (strcmp(keyword, "PARAM") == 0 && (cur_proc || cur_inline)) {
            TypeDesc *target = cur_proc ? cur_proc : cur_inline;
            /* PARAM [VAR | TYPELESSVAR] <name> [<type...>] */
            int is_typeless2 = 0;
            is_var2 = 0;
            kw2[0] = '\0';
            sscanf(q, "%32s", kw2);
            if (strcmp(kw2, "VAR") == 0) {
                is_var2 = 1;
                while (*q && *q != ' ' && *q != '\t') q++;
                q = trim_lead(q);
            } else if (strcmp(kw2, "TYPELESSVAR") == 0) {
                is_var2 = 1;
                is_typeless2 = 1;
                while (*q && *q != ' ' && *q != '\t') q++;
                q = trim_lead(q);
            }
            sscanf(q, "%32s", pname);
            if (is_typeless2) {
                /* typeless VAR: no type token; type is ADDRESS */
                Symbol *ps = type_add_param(target, pname, type_address, 1);
                ps->typeless = 1;
            } else {
                /* advance past pname */
                while (*q && *q != ' ' && *q != '\t') q++;
                q = trim_lead(q);
                ptype = parse_def_type(&q);
                type_add_param(target, pname, ptype, is_var2);
            }
            continue;
        }

        /* Lines below require a MODULE line first */
        if (strcmp(keyword, "MODULE") == 0) {
            sscanf(q, "%63s", fullmod);
            strncpy(mod_name, fullmod, NAME_LEN-1);
            continue;
        }

        if (mod_name[0] == '\0') continue;

        /* parse fullname (first token after keyword) */
        sscanf(q, "%63s", fullname);
        /* advance q past fullname */
        while (*q && *q != ' ' && *q != '\t') q++;
        q = trim_lead(q);

        make_short(fullname, short_name);

        /* Symbols are stored under "alias.shortname" so they don't collide with
           local declarations in the importing module.  The parser looks them up
           with the same qualified key when resolving Alias.Name expressions. */
        {
            char qname[NAME_LEN*2];
            snprintf(qname, sizeof(qname), "%s.%s", alias, short_name);

        /* ── CONST ── */
        if (strcmp(keyword, "CONST") == 0) {
            val_l = 0;
            sscanf(q, "%ld", &val_l);
            val = (int32_t)val_l;
            s = sym_new(qname, K_CONST);
            s->val      = val;
            s->type     = type_integer;
            s->exported = 1;
            strncpy(s->mod_name, alias, NAME_LEN-1);

        /* ── VAR: new format "VAR name type seg offset",
                  legacy format "VAR name seg offset" (seg is a digit) ── */
        } else if (strcmp(keyword, "VAR") == 0) {
            TypeDesc *var_type = type_integer;
            seg = 0; ofs_l = 0;
            /* peek: if q starts with a digit it's legacy format (seg first) */
            if (*q >= '0' && *q <= '9') {
                sscanf(q, "%d %ld", &seg, &ofs_l);
            } else {
                /* new format: type token(s) then seg offset */
                var_type = parse_def_type(&q);
                q = trim_lead(q);
                sscanf(q, "%d %ld", &seg, &ofs_l);
            }
            offset = (int32_t)ofs_l;
            s = sym_new(qname, K_VAR);
            s->adr      = offset;
            s->level    = (seg == SEG_DATA) ? 0 : 1;
            s->type     = var_type;
            s->exported = 1;
            strncpy(s->mod_name, alias, NAME_LEN-1);

        /* ── TYPE (extended: TYPE fullname RECORD size … END) ── */
        } else if (strcmp(keyword, "TYPE") == 0) {
            /* q may be "RECORD <size>" or empty for plain opaque type */
            subkw[0] = '\0';
            sscanf(q, "%15s", subkw);
            if (strcmp(subkw, "RECORD") == 0) {
                ofs_l = 0;
                rest = q;
                while (*rest && *rest != ' ' && *rest != '\t') rest++;
                rest = trim_lead(rest);
                sscanf(rest, "%ld", &ofs_l);
                total_size = (int32_t)ofs_l;
                /* Use pre-registered stub if available; otherwise create new */
                s = sym_find(qname);
                if (s && s->kind == K_TYPE && s->type->form == TF_RECORD) {
                    rec = s->type;
                    rec->size = 0;  /* reset so type_add_field works cleanly */
                } else {
                    rec = type_new_record(NULL);
                    s = sym_new(qname, K_TYPE);
                    s->type     = rec;
                    s->exported = 1;
                    strncpy(s->mod_name, alias, NAME_LEN-1);
                }
                cur_rec = rec;
                cur_rec_total_size = total_size; /* saved; restored at END */
                (void)s;  /* registered in scope; no further ref needed */
            } else if (strcmp(subkw, "POINTER") == 0) {
                /* TYPE name POINTER base — pointer alias */
                TypeDesc *base;
                /* advance q past "POINTER" to reach the base type token */
                rest = q;
                while (*rest && *rest != ' ' && *rest != '\t') rest++;
                rest = trim_lead(rest);
                base = parse_def_type(&rest);
                s = sym_find(qname);
                if (s && s->kind == K_TYPE) {
                    /* update existing stub if present */
                    s->type = type_new_pointer(base);
                } else {
                    s = sym_new(qname, K_TYPE);
                    s->type     = type_new_pointer(base);
                    s->exported = 1;
                    strncpy(s->mod_name, alias, NAME_LEN-1);
                }
            } else if (strcmp(subkw, "ADDRESS") == 0) {
                s = sym_find(qname);
                if (!s) {
                    s = sym_new(qname, K_TYPE);
                    s->exported = 1;
                    strncpy(s->mod_name, alias, NAME_LEN-1);
                }
                s->type = type_address;
            } else {
                /* plain opaque type */
                s = sym_find(qname);
                if (!s) {
                    s = sym_new(qname, K_TYPE);
                    s->type     = type_integer;
                    s->exported = 1;
                    strncpy(s->mod_name, alias, NAME_LEN-1);
                }
            }

        /* ── INLINE proc (INLINE fullname rettype … PARAM … BYTES … END) ── */
        } else if (strcmp(keyword, "INLINE") == 0) {
            /* parse return type */
            ret = parse_def_type(&q);
            pt  = type_new_proc(ret);
            pt->is_inline = 1;
            pt->is_far    = 0;  /* inline has no calling convention */
            s   = sym_new(qname, K_PROC);
            s->code_ofs   = 0;
            s->rdoff_id   = -1;
            s->type       = pt;
            s->exported   = 1;
            strncpy(s->mod_name, fullname, NAME_LEN-1);
            cur_inline = pt;

        /* ── PROC (extended: PROC fullname FAR rettype … END) ── */
        } else if (strcmp(keyword, "PROC") == 0) {
            /* q: "FAR <rettype>"  or legacy "seg offset" */
            subkw[0] = '\0';
            sscanf(q, "%15s", subkw);
            if (strcmp(subkw, "FAR") == 0 || strcmp(subkw, "NEAR") == 0) {
                /* parse return type */
                rest = q;
                while (*rest && *rest != ' ' && *rest != '\t') rest++;
                rest = trim_lead(rest);
                ret = parse_def_type(&rest);
                pt  = type_new_proc(ret);
                pt->is_far    = (strcmp(subkw, "FAR") == 0) ? 1 : 0;
                s   = sym_new(qname, K_PROC);
                s->code_ofs   = 0;
                s->rdoff_id   = -1;
                s->type       = pt;
                s->exported   = 1;
                strncpy(s->mod_name, fullname, NAME_LEN-1);
                cur_proc = pt;
            } else {
                /* legacy format: PROC fullname seg offset */
                seg = 0; ofs_l = 0;
                sscanf(q, "%d %ld", &seg, &ofs_l);
                offset = (int32_t)ofs_l;
                s = sym_new(qname, K_PROC);
                s->code_ofs = offset;
                s->rdoff_id = -1;
                s->type     = type_new_proc(type_notype);
                s->exported = 1;
                strncpy(s->mod_name, fullname, NAME_LEN-1);
            }
        }
        } /* end qname block */
    }

    /* Close any unclosed PROC or INLINE block (missing END) */
    if (cur_proc)   type_calc_arg_size(cur_proc);
    if (cur_inline) type_calc_arg_size(cur_inline);

    /* Resolve deferred BASE links — all types are now loaded */
    for (bp_node = base_patches; bp_node; bp_node = bp_node->next) {
        char qn[NAME_LEN*2];
        Symbol *bs;
        snprintf(qn, sizeof(qn), "%s.%s", alias, bp_node->base_short);
        bs = sym_find(qn);
        if (!bs || bs->kind != K_TYPE) bs = sym_find(bp_node->base_short);
        if (bs && bs->kind == K_TYPE && bs->type->form == TF_RECORD)
            bp_node->derived->base = bs->type;
    }
    /* free the patch list */
    while (base_patches) {
        BasePatchNode *next = base_patches->next;
        free(base_patches);
        base_patches = next;
    }

    return 0;
}
