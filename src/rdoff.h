#ifndef RDOFF_H
#define RDOFF_H

#include <stdint.h>
#include <stdio.h>

/* ---- segment IDs ---- */
#define SEG_CODE  0
#define SEG_DATA  1
#define SEG_BSS   2
/* imports: >= 3 */

/* ---- record types ---- */
#define REC_RELOC    1
#define REC_IMPORT   2
#define REC_GLOBAL   3
#define REC_BSS      5
#define REC_SEGRELOC 6

#define MAX_BUF      65500   /* pre-alloc; segment overflow reported at 64000 */
#define MAX_NAME        64
#define SEG_LIMIT    64000   /* error if code/data/bss exceeds this */

typedef struct Reloc {
    uint8_t  phys_seg;   /* bits 0-5 = seg idx, bit 6 = relative, bit 7 = segreloc */
    uint32_t offset;
    uint8_t  width;      /* 1, 2, or 4 */
    uint16_t rseg;       /* 0=code,1=data,2=bss, >=3 import id */
    struct Reloc *next;
} Reloc;

typedef struct Import {
    uint8_t  flags;      /* usually 0 */
    uint16_t seg_id;     /* 2 bytes! assigned starting at 3 */
    char     name[MAX_NAME];
    struct Import *next;
} Import;

typedef struct Global {
    uint8_t  flags;      /* 0x01 = exported */
    uint8_t  seg_id;     /* 1 byte! 0=code,1=data,2=bss */
    uint32_t offset;
    char     name[MAX_NAME];
    struct Global *next;
} Global;

typedef struct {
    /* code + data buffers — heap allocated by rdf_init, freed by rdf_free */
    uint8_t *code;   uint32_t code_len;
    uint8_t *data;   uint32_t data_len;
    uint32_t bss_size;

    Reloc   *reloc_head;  Reloc   *reloc_tail;  int n_relocs;
    Import  *import_head; Import  *import_tail;  int n_imports;
    Global  *global_head; Global  *global_tail;  int n_globals;

    int      next_import_id;   /* starts at 3 */
} ObjFile;

void  rdf_init(ObjFile *obj);    /* malloc all buffers */
void  rdf_free(ObjFile *obj);    /* free all buffers */
int   rdf_add_import(ObjFile *obj, const char *name);   /* returns seg_id */
void  rdf_add_global(ObjFile *obj, const char *name, uint8_t seg_id, uint32_t offset);
void  rdf_add_reloc(ObjFile *obj, uint8_t phys_seg, uint32_t offset,
                    uint8_t width, uint16_t rseg, int relative);
void  rdf_add_segreloc(ObjFile *obj, uint8_t phys_seg, uint32_t offset,
                       uint16_t rseg);   /* segment-word reloc (type 6) */
void  rdf_set_bss(ObjFile *obj, uint32_t size);
void  rdf_write(ObjFile *obj, FILE *f);

/* emit helpers — defined in rdoff.c */
void emit_byte(ObjFile *o, int seg, uint8_t b);
void emit_word(ObjFile *o, int seg, uint16_t w);

#endif
