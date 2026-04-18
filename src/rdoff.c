#include "rdoff.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void rdf_init(ObjFile *obj) {
    obj->code    = (uint8_t *)malloc(MAX_BUF);
    obj->data    = (uint8_t *)malloc(MAX_BUF);
    if (!obj->code || !obj->data) {
        fprintf(stderr, "rdf_init: out of memory\n");
        exit(1);
    }
    obj->code_len       = 0;
    obj->data_len       = 0;
    obj->bss_size       = 0;
    obj->reloc_head     = NULL; obj->reloc_tail  = NULL; obj->n_relocs  = 0;
    obj->import_head    = NULL; obj->import_tail = NULL; obj->n_imports = 0;
    obj->global_head    = NULL; obj->global_tail = NULL; obj->n_globals = 0;
    obj->next_import_id = 3;
}

void rdf_free(ObjFile *obj) {
    Reloc  *r; Import *imp; Global *g;
    free(obj->code); obj->code = NULL;
    free(obj->data); obj->data = NULL;
    r = obj->reloc_head;
    while (r) { Reloc  *nx = r->next;   free(r); r   = nx; }
    obj->reloc_head = obj->reloc_tail = NULL;
    imp = obj->import_head;
    while (imp) { Import *nx = imp->next; free(imp); imp = nx; }
    obj->import_head = obj->import_tail = NULL;
    g = obj->global_head;
    while (g) { Global *nx = g->next;   free(g); g   = nx; }
    obj->global_head = obj->global_tail = NULL;
}

int rdf_add_import(ObjFile *obj, const char *name) {
    Import *imp = (Import *)malloc(sizeof(Import));
    if (!imp) { fprintf(stderr, "rdf_add_import: out of memory\n"); exit(1); }
    imp->flags  = 0;
    imp->seg_id = (uint16_t)obj->next_import_id++;
    strncpy(imp->name, name, MAX_NAME-1);
    imp->name[MAX_NAME-1] = '\0';
    imp->next = NULL;
    if (obj->import_tail) obj->import_tail->next = imp;
    else                  obj->import_head = imp;
    obj->import_tail = imp;
    obj->n_imports++;
    return imp->seg_id;
}

void rdf_add_global(ObjFile *obj, const char *name,
                    uint8_t seg_id, uint32_t offset) {
    Global *g = (Global *)malloc(sizeof(Global));
    if (!g) { fprintf(stderr, "rdf_add_global: out of memory\n"); exit(1); }
    g->flags  = 0x01;
    g->seg_id = seg_id;
    g->offset = offset;
    strncpy(g->name, name, MAX_NAME-1);
    g->name[MAX_NAME-1] = '\0';
    g->next = NULL;
    if (obj->global_tail) obj->global_tail->next = g;
    else                  obj->global_head = g;
    obj->global_tail = g;
    obj->n_globals++;
}

static void add_reloc_node(ObjFile *obj, uint8_t phys_seg, uint32_t offset,
                           uint8_t width, uint16_t rseg) {
    Reloc *r = (Reloc *)malloc(sizeof(Reloc));
    if (!r) { fprintf(stderr, "rdf_add_reloc: out of memory\n"); exit(1); }
    r->phys_seg = phys_seg;
    r->offset   = offset;
    r->width    = width;
    r->rseg     = rseg;
    r->next     = NULL;
    if (obj->reloc_tail) obj->reloc_tail->next = r;
    else                 obj->reloc_head = r;
    obj->reloc_tail = r;
    obj->n_relocs++;
}

void rdf_add_reloc(ObjFile *obj, uint8_t phys_seg, uint32_t offset,
                   uint8_t width, uint16_t rseg, int relative) {
    add_reloc_node(obj,
        (uint8_t)(phys_seg | (relative ? 0x40 : 0)),
        offset, width, rseg);
}

void rdf_add_segreloc(ObjFile *obj, uint8_t phys_seg, uint32_t offset,
                      uint16_t rseg) {
    add_reloc_node(obj,
        (uint8_t)(phys_seg | 0x80),  /* bit 7 set = SEGRELOC (type 6) */
        offset, 2, rseg);
}

void rdf_set_bss(ObjFile *obj, uint32_t size) {
    if (size > SEG_LIMIT) {
        fprintf(stderr, "error: bss segment exceeds %u bytes\n", (unsigned)SEG_LIMIT);
        exit(1);
    }
    obj->bss_size = size;
}

/* emit helpers */
void emit_byte(ObjFile *o, int seg, uint8_t b) {
    if (seg == SEG_CODE) {
        if (o->code_len >= SEG_LIMIT) {
            fprintf(stderr, "error: code segment exceeds %u bytes\n", (unsigned)SEG_LIMIT);
            exit(1);
        }
        o->code[o->code_len++] = b;
    } else {
        if (o->data_len >= SEG_LIMIT) {
            fprintf(stderr, "error: data segment exceeds %u bytes\n", (unsigned)SEG_LIMIT);
            exit(1);
        }
        o->data[o->data_len++] = b;
    }
}
void emit_word(ObjFile *o, int seg, uint16_t w) {
    emit_byte(o, seg, (uint8_t)(w & 0xFF));
    emit_byte(o, seg, (uint8_t)((w >> 8) & 0xFF));
}

/* write little-endian helpers */
static void w16(FILE *f, uint16_t v) { uint8_t b[2]; b[0]=v&0xFF; b[1]=(v>>8)&0xFF; fwrite(b,1,2,f); }
static void w32(FILE *f, uint32_t v) { uint8_t b[4]; b[0]=v&0xFF; b[1]=(v>>8)&0xFF; b[2]=(v>>16)&0xFF; b[3]=(v>>24)&0xFF; fwrite(b,1,4,f); }

void rdf_write(ObjFile *obj, FILE *f) {
    /* --- build header records into a heap buffer --- */
    uint8_t *hdr;
    uint32_t hlen = 0;
    Reloc  *r;
    Import *imp;
    Global *g;
    uint32_t mod_size;

    hdr = (uint8_t *)malloc(65000);
    if (!hdr) { fprintf(stderr, "rdf_write: out of memory\n"); exit(1); }

#define H1(b)   hdr[hlen++] = (uint8_t)(b)
#define H2(w)   H1((w)&0xFF); H1(((w)>>8)&0xFF)
#define H4(d)   H2((d)&0xFFFF); H2(((d)>>16)&0xFFFF)
#define HSTR(s) { const char*_p=(s); while(*_p) hdr[hlen++]=(uint8_t)*_p++; hdr[hlen++]=0; }

    /* RELOC / SEGRELOC records */
    for (r = obj->reloc_head; r; r = r->next) {
        if (r->phys_seg & 0x80) {
            /* SEGRELOC (type 6): same layout as RELOC but different type byte */
            H1(REC_SEGRELOC); H1(8);
            H1(r->phys_seg & 0x7F);     /* strip sentinel bit */
            H4(r->offset);
            H1(r->width);
            H2(r->rseg);
        } else {
            H1(REC_RELOC); H1(8);       /* type, length=8 */
            H1(r->phys_seg);
            H4(r->offset);
            H1(r->width);
            H2(r->rseg);
        }
    }

    /* IMPORT records */
    for (imp = obj->import_head; imp; imp = imp->next) {
        int nlen = (int)strlen(imp->name) + 1;
        H1(REC_IMPORT); H1(3 + nlen);  /* flags(1)+segid(2)+name */
        H1(imp->flags);
        H2(imp->seg_id);               /* 2 bytes for import! */
        HSTR(imp->name);
    }

    /* GLOBAL records */
    for (g = obj->global_head; g; g = g->next) {
        int nlen = (int)strlen(g->name) + 1;
        H1(REC_GLOBAL); H1(6 + nlen);  /* flags(1)+segid(1)+offset(4)+name */
        H1(g->flags);
        H1(g->seg_id);                 /* 1 byte for global! */
        H4(g->offset);
        HSTR(g->name);
    }

    /* BSS record */
    if (obj->bss_size > 0) {
        H1(REC_BSS); H1(4);
        H4(obj->bss_size);
    }

#undef H1
#undef H2
#undef H4
#undef HSTR

    /* --- compute total module size ---
       file sig(6) + module_size(4) + header_size(4)
       + header(hlen)
       + code seg header(10) + code data
       + data seg header(10) + data data  (if non-empty)
       + eof seg header(10)
    */
    mod_size = (uint32_t)(6 + 4 + 4 + hlen
        + 10 + obj->code_len
        + (obj->data_len > 0 ? 10 + obj->data_len : 0)
        + 10);

    /* --- write file --- */
    fwrite("RDOFF2", 1, 6, f);
    w32(f, mod_size);                  /* module_length = total file size */
    w32(f, (uint32_t)hlen);            /* header size */
    fwrite(hdr, 1, (size_t)hlen, f);   /* header records */

    free(hdr);

    /* code segment */
    w16(f, 1);                         /* type = code */
    w16(f, 0);                         /* number = 0 */
    w16(f, 0);                         /* reserved */
    w32(f, (uint32_t)obj->code_len);
    fwrite(obj->code, 1, (size_t)obj->code_len, f);

    /* data segment (only if non-empty) */
    if (obj->data_len > 0) {
        w16(f, 2);                     /* type = data */
        w16(f, 1);                     /* number = 1 */
        w16(f, 0);
        w32(f, (uint32_t)obj->data_len);
        fwrite(obj->data, 1, (size_t)obj->data_len, f);
    }

    /* EOF segment */
    w16(f, 0); w16(f, 0); w16(f, 0); w32(f, 0);
}
