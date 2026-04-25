#include "def.h"
#include "rdoff.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ---- writer ---- */

/* Write a type descriptor as a type-name token for .def PARAM/FIELD lines */
static void write_type(FILE *f, TypeDesc *t) {
    if (!t)                           { fprintf(f, "VOID"); return; }
    switch (t->form) {
    case TF_INTEGER:  fprintf(f, "INTEGER"); return;
    case TF_BOOLEAN:  fprintf(f, "BOOLEAN"); return;
    case TF_CHAR:     fprintf(f, "CHAR");    return;
    case TF_BYTE:     fprintf(f, "BYTE");    return;
    case TF_LONGINT:  fprintf(f, "LONGINT"); return;
    case TF_ARRAY:    fprintf(f, "ARRAY");   return;
    case TF_POINTER:  fprintf(f, "POINTER VOID"); return;
    case TF_NOTYPE:   fprintf(f, "VOID");    return;
    default:          fprintf(f, "INTEGER"); return; /* fallback */
    }
}

void def_write(FILE *f, const char *mod_name) {
    /* walk the current (module-level) scope */
    Scope *scope = top_scope;
    Symbol *sym;
    fprintf(f, "MODULE %s\n", mod_name);

    for (sym = scope->symbols; sym; sym = sym->next) {
        if (!sym->exported) continue;
        /* skip imported symbols — their names contain '.' (e.g. "SYSTEM.FCarry") */
        if (strchr(sym->name, '.')) continue;

        switch (sym->kind) {
        case K_CONST:
            fprintf(f, "CONST %s_%s %ld\n",
                    mod_name, sym->name, (long)sym->val);
            break;
        case K_VAR: {
            int seg = (sym->level == 0) ? SEG_DATA : SEG_CODE;
            fprintf(f, "VAR %s_%s %d %ld\n",
                    mod_name, sym->name, seg, (long)sym->adr);
            break;
        }
        case K_TYPE: {
            TypeDesc *td = sym->type;
            if (td && td->form == TF_RECORD) {
                Symbol *fld;
                int has_exported = 0;
                for (fld = td->fields; fld; fld = fld->next)
                    if (fld->exported) { has_exported = 1; break; }
                if (has_exported) {
                    fprintf(f, "TYPE %s_%s RECORD %ld\n",
                            mod_name, sym->name, (long)td->size);
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
            } else {
                fprintf(f, "TYPE %s_%s\n", mod_name, sym->name);
            }
            break;
        }
        case K_PROC: {
            TypeDesc *pt = sym->type;
            fprintf(f, "PROC %s_%s %s ", mod_name, sym->name,
                    (pt && pt->is_far) ? "FAR" : "NEAR");
            write_type(f, pt ? pt->ret_type : NULL);
            fprintf(f, "\n");
            if (pt && pt->n_params > 0) {
                Symbol *p;
                for (p = pt->params; p; p = p->next) {
                    if (p->kind == K_VARPARAM)
                        fprintf(f, "  PARAM VAR %s ", p->name);
                    else
                        fprintf(f, "  PARAM %s ", p->name);
                    write_type(f, p->type);
                    fprintf(f, "\n");
                }
            }
            fprintf(f, "END\n");
            break;
        }
        default:
            break;
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

/* Look up a type by its short name in the current scope.
   Used when parsing PARAM/FIELD types that refer to other types by name. */
static TypeDesc *resolve_type_name(const char *tname) {
    Symbol *s;
    if (strcmp(tname, "INTEGER") == 0) return type_integer;
    if (strcmp(tname, "BOOLEAN") == 0) return type_boolean;
    if (strcmp(tname, "CHAR")    == 0) return type_char;
    if (strcmp(tname, "BYTE")    == 0) return type_byte;
    if (strcmp(tname, "LONGINT") == 0) return type_longint;
    if (strcmp(tname, "VOID")    == 0) return type_notype;
    if (strcmp(tname, "ARRAY")   == 0) return type_new_array(type_char, -1); /* open ARRAY OF CHAR */
    /* Search current scope for a K_TYPE symbol */
    s = sym_find(tname);
    if (s && s->kind == K_TYPE) return s->type;
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

int def_read(FILE *f, const char *alias) {
    char line[512];
    char mod_name[NAME_LEN] = "";

    /* current RECORD type being built (for FIELD lines) */
    TypeDesc *cur_rec  = NULL;
    int32_t cur_rec_total_size = 0; /* declared total size from .def (authoritative) */
    /* current PROC type being built (for PARAM lines) */
    TypeDesc *cur_proc = NULL;

    while (fgets(line, sizeof(line), f)) {
        char *p;
        char keyword[32];
        int  kn;
        char *q;
        char fullmod[NAME_LEN*2];
        char fullname[NAME_LEN*2];
        char short_name[NAME_LEN];
        char fname[NAME_LEN];
        char typestr[NAME_LEN];
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

        /* ── END: close current RECORD or PROC block ── */
        if (strcmp(keyword, "END") == 0) {
            if (cur_rec) {
                /* Restore authoritative total size declared in .def header.
                   type_add_field() accumulates rec->size, but .def provides
                   explicit field offsets and a precise total size — use it. */
                cur_rec->size = cur_rec_total_size;
                cur_rec = NULL;
            } else if (cur_proc) {
                type_calc_arg_size(cur_proc);
                cur_proc = NULL;
            }
            continue;
        }

        /* ── FIELD: add field to current RECORD ── */
        if (strcmp(keyword, "FIELD") == 0 && cur_rec) {
            /* FIELD <name> <type> <offset> <size> */
            ofs_l = 0; /* size unused */
            sscanf(q, "%32s %32s %ld", fname, typestr, &ofs_l);
            offset = (int32_t)ofs_l;
            ft = resolve_type_name(typestr);
            fs = type_add_field(cur_rec, fname, ft);
            /* override offset from .def (authoritative) */
            fs->offset = offset;
            /* fix up the record size to include this field (may already be correct) */
            (void)fs;
            continue;
        }

        /* ── PARAM: add parameter to current PROC ── */
        if (strcmp(keyword, "PARAM") == 0 && cur_proc) {
            /* PARAM [VAR] <name> <type...> */
            is_var2 = 0;
            kw2[0] = '\0';
            sscanf(q, "%32s", kw2);
            if (strcmp(kw2, "VAR") == 0) {
                is_var2 = 1;
                /* advance past VAR */
                while (*q && *q != ' ' && *q != '\t') q++;
                q = trim_lead(q);
            }
            sscanf(q, "%32s", pname);
            /* advance past pname */
            while (*q && *q != ' ' && *q != '\t') q++;
            q = trim_lead(q);
            ptype = parse_def_type(&q);
            type_add_param(cur_proc, pname, ptype, is_var2);
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

        /* ── VAR (legacy format: VAR name seg offset) ── */
        } else if (strcmp(keyword, "VAR") == 0) {
            seg = 0; ofs_l = 0;
            sscanf(q, "%d %ld", &seg, &ofs_l);
            offset = (int32_t)ofs_l;
            s = sym_new(qname, K_VAR);
            s->adr      = offset;
            s->level    = (seg == SEG_DATA) ? 0 : 1;
            s->type     = type_integer;
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
                /* Build new record type; size will be restored at END */
                rec = type_new_record(NULL);
                rec->size = 0;   /* start at 0 so type_add_field offsets compute cleanly */
                s = sym_new(qname, K_TYPE);
                s->type     = rec;
                s->exported = 1;
                strncpy(s->mod_name, alias, NAME_LEN-1);
                cur_rec = rec;
                cur_rec_total_size = total_size; /* saved; restored at END */
                (void)s;  /* registered in scope; no further ref needed */
            } else {
                /* plain opaque type */
                s = sym_new(qname, K_TYPE);
                s->type     = type_integer;
                s->exported = 1;
                strncpy(s->mod_name, alias, NAME_LEN-1);
            }

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

    /* Close any unclosed PROC block (missing END) */
    if (cur_proc) type_calc_arg_size(cur_proc);

    return 0;
}
