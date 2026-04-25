#ifndef OLINK_H
#define OLINK_H

/* olink.h -- RDOFF2 smart linker for MS-DOS
   Large memory model: modules from the same .om file share one code segment
   (one CS per .om group, 2-byte aligned packing within), one combined
   data+BSS segment (DS), one separate stack segment (SS).
   C89; stdint.h available on both gcc and Open Watcom. */

#include "rdoff.h"    /* uint8_t, uint16_t, uint32_t, SEG_CODE, SEG_DATA */
#include "compat.h"   /* path_basename, path_join, ENV_SEP */

/* ---- forward declarations ---- */
typedef struct TModule    TModule;
typedef struct TSymbol    TSymbol;
typedef struct TMZReloc   TMZReloc;
typedef struct TImportNode TImportNode;

/* ---- module descriptor ---- */
struct TModule {
    char        *filename;
    int          is_lib_member;  /* non-zero if inside a tar archive */
    uint32_t     lib_offset;     /* byte offset within the archive file */
    uint32_t     raw_size;       /* total RDOFF2 payload size (after sig+rawsize field) */
    uint32_t     hdr_size;       /* RDOFF2 header area size */
    uint32_t     code_size;
    uint32_t     data_size;
    uint32_t     bss_size;
    int          group_id;        /* .om group: all members share one CS paragraph */
    uint32_t     final_code_ofs;  /* byte offset in combined code buffer (assigned by calculate_layout) */
    uint16_t     group_code_para; /* CS paragraph of the .om group (= group start offset >> 4) */
    uint32_t     final_data_ofs;
    uint32_t     final_bss_ofs;
    int          used;           /* reachability flag */
    TModule     *next;
};

/* ---- global symbol ---- */
struct TSymbol {
    char        *name;
    TModule     *owner;
    uint8_t      seg_id;   /* 0=code,1=data,2=bss */
    uint32_t     offset;
    TSymbol     *next;       /* iteration / free list */
    TSymbol     *hash_next;  /* hash bucket chain */
};

/* ---- MZ relocation entry ---- */
struct TMZReloc {
    uint16_t     seg;
    uint16_t     ofs;
    TMZReloc    *next;
};

/* ---- per-module import id -> symbol map ---- */
struct TImportNode {
    uint16_t      id;
    TSymbol      *sym;
    TImportNode  *next;
};

/* ---- MZ EXE header (28 bytes, written field-by-field) ---- */
#define MZ_HDR_BYTES   28
/* Stack segment size in bytes (separate segment after data+BSS). */
#define STACK_SIZE     8192

/* Hash table size for symbol lookup (power of 2). */
#define SYM_HASH_SIZE 256

/* ---- linker state (all globals collected here) ---- */
typedef struct {
    TModule     *mod_head;
    TModule     *mod_tail;
    TSymbol     *sym_head;
    TSymbol     *sym_hash[SYM_HASH_SIZE]; /* hash buckets for fast lookup */
    TMZReloc    *reloc_head;
    TMZReloc    *reloc_tail;
    uint8_t     *code_buf;
    uint8_t     *data_buf;
    uint32_t     total_code_len;
    uint32_t     total_data_len;
    uint32_t     total_bss_len;
    uint16_t     mz_reloc_cnt;
    uint16_t     entry_ip;        /* IP of "start" within its module's code segment */
    uint16_t     entry_cs_para;   /* code paragraph of the module owning "start" (0-based from code start) */
    int          entry_found;

    /* extra library search paths (-L): heap-allocated, max 16 paths */
    char        *lib_paths[16];
    int          n_lib_paths;

    /* group ID counter: each .om file (or standalone .rdf) gets a unique group */
    int          next_group_id;

    /* stack segment size in bytes; default STACK_SIZE, overridden by META-INF/STACK.TXT */
    uint32_t     stack_size;
} LinkerState;

/* ---- public API ---- */

/* Initialise a zeroed LinkerState. */
void olink_init(LinkerState *ls);

/* Free all heap memory owned by *ls. */
void olink_free(LinkerState *ls);

/* Process one input argument (filename or @listfile). */
void olink_process_input(LinkerState *ls, const char *arg);

/* Smart-link: mark modules reachable from "start". */
void olink_smart_link(LinkerState *ls);

/* Assign final code/data/bss offsets to each used module. */
void olink_calculate_layout(LinkerState *ls);

/* Read segment data and apply relocations into code_buf/data_buf. */
void olink_perform_linking(LinkerState *ls);

/* Write the DOS MZ executable. */
void olink_write_exe(LinkerState *ls, const char *out_filename);

#endif /* OLINK_H */
