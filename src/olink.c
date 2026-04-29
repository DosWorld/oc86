/*
 * olink.c -- RDOFF2 smart linker for MS-DOS
 *
 * Target: C89, single file.
 * Produces DOS MZ executables:
 *   - All .rdf members from the same .om file share one code segment (one CS
 *     per .om group); modules are packed with 2-byte word alignment within the
 *     group.  Standalone .rdf files each form their own single-module group.
 *   - All module data is merged into one combined data segment (DS).
 *   - BSS follows data in the same DS region.
 *   - A separate stack segment follows data+BSS (SS != DS).
 *   - SYSTEM_INIT sets DS from a SEGRELOC-patched immediate on entry.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "olink.h"
#include "tar.h"

/* ===================================================================
 * Internal constants
 * =================================================================== */

#define RDFREC_RELOC    1
#define RDFREC_IMPORT   2
#define RDFREC_GLOBAL   3
#define RDFREC_BSS      5
#define RDFREC_SEGRELOC 6

/* ===================================================================
 * Error
 * =================================================================== */

static void olink_error(const char *msg)
{
    fprintf(stderr, "Linker Error: %s\n", msg);
    exit(1);
}

static void olink_error2(const char *a, const char *b)
{
    fprintf(stderr, "Linker Error: %s%s\n", a, b);
    exit(1);
}

/* ===================================================================
 * String helpers
 * =================================================================== */

static char *xstrdup(const char *s)
{
    char *p = (char *)malloc(strlen(s) + 1);
    if (!p) olink_error("out of memory");
    strcpy(p, s);
    return p;
}

static int ascii_upper(int c) {
    return (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c;
}

static void str_upper(char *s)
{
    while (*s) { *s = (char)ascii_upper((unsigned char)*s); s++; }
}

static char *str_trim(char *s)
{
    char *end;
    while (*s && (unsigned char)*s <= ' ') s++;
    if (!*s) return s;
    end = s + strlen(s) - 1;
    while (end > s && (unsigned char)*end <= ' ') *end-- = '\0';
    return s;
}

static int str_ends_with(const char *s, const char *suffix)
{
    int ls   = (int)strlen(s);
    int lsuf = (int)strlen(suffix);
    int i;
    char a, b;
    if (ls < lsuf) return 0;
    for (i = 0; i < lsuf; i++) {
        a = (char)ascii_upper((unsigned char)s[ls - lsuf + i]);
        b = (char)ascii_upper((unsigned char)suffix[i]);
        if (a != b) return 0;
    }
    return 1;
}

/* ===================================================================
 * File helpers
 * =================================================================== */

static uint32_t get_file_size(FILE *f)
{
    long cur, sz;
    cur = ftell(f);
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, cur, SEEK_SET);
    return (uint32_t)sz;
}

/* Read bytes directly from a binary file position.
   Calls olink_error on short read. */
static void xfread(void *buf, size_t n, FILE *f, const char *ctx)
{
    if (fread(buf, 1, n, f) != n) olink_error2("read error in ", ctx);
}

/* Read a NUL-terminated string from binary file into buf (maxlen incl NUL). */
static void read_string(FILE *f, char *buf, int maxlen)
{
    int i = 0;
    int c;
    while (i < maxlen - 1) {
        c = fgetc(f);
        if (c == EOF || c == 0) break;
        buf[i++] = (char)c;
    }
    buf[i] = '\0';
}

/* ===================================================================
 * Little-endian I/O
 * =================================================================== */

static void write_u16(FILE *f, uint16_t v)
{
    uint8_t b[2];
    b[0] = (uint8_t)(v & 0xFF);
    b[1] = (uint8_t)((v >> 8) & 0xFF);
    if (fwrite(b, 1, 2, f) != 2) olink_error("write error");
}

static void read_u16_buf(const uint8_t *buf, uint16_t *out)
{
    *out = (uint16_t)(buf[0] | ((unsigned)buf[1] << 8));
}

static void read_u32_buf(const uint8_t *buf, uint32_t *out)
{
    *out = (uint32_t)buf[0]
         | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16)
         | ((uint32_t)buf[3] << 24);
}

/* ===================================================================
 * File search  (current dir → -L paths → OBERON_LIB)
 * =================================================================== */

static int try_open(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}

static void find_file(LinkerState *ls, const char *name, char *result, int rsz)
{
    char tmp[256];
    char *env;
    char *p;
    char *end;
    int i;

    /* 1. current directory / absolute path */
    if (try_open(name)) {
        strncpy(result, name, (size_t)(rsz - 1));
        result[rsz - 1] = '\0';
        return;
    }

    /* 2. -L paths */
    for (i = 0; i < ls->n_lib_paths; i++) {
        path_join(tmp, sizeof(tmp), ls->lib_paths[i], path_basename(name));
        if (try_open(tmp)) {
            strncpy(result, tmp, (size_t)(rsz - 1));
            result[rsz - 1] = '\0';
            return;
        }
    }

    /* 3. OBERON_LIB */
    env = getenv("OBERON_LIB");
    if (env) {
        p = env;
        while (*p) {
            end = p;
            while (*end && *end != ENV_SEP) end++;
            if (end > p) {
                int dlen = (int)(end - p);
                if (dlen >= (int)sizeof(tmp) - 1) dlen = (int)sizeof(tmp) - 2;
                strncpy(tmp, p, (size_t)dlen);
                tmp[dlen] = '\0';
                path_join(tmp, sizeof(tmp), tmp, path_basename(name));
                if (try_open(tmp)) {
                    strncpy(result, tmp, (size_t)(rsz - 1));
                    result[rsz - 1] = '\0';
                    return;
                }
            }
            p = (*end == ENV_SEP) ? end + 1 : end;
        }
    }

    /* not found — return original name (error will come on open) */
    strncpy(result, name, (size_t)(rsz - 1));
    result[rsz - 1] = '\0';
}

/* ===================================================================
 * Alignment
 * =================================================================== */

static uint32_t align16(uint32_t v)
{
    return (v + 15u) & ~(uint32_t)15u;
}

/* ===================================================================
 * TAR helpers
 * =================================================================== */

static uint32_t parse_octal(const char *buf, int maxlen)
{
    uint32_t res = 0;
    int i;
    for (i = 0; i < maxlen; i++) {
        if (buf[i] < '0' || buf[i] > '7') break;
        res = (res << 3) + (uint32_t)(buf[i] - '0');
    }
    return res;
}

/* Extract a NUL-terminated name from a fixed-width tar field. */
static void extract_tar_name(const char *buf, int maxlen, char *out, int outsz)
{
    int i;
    for (i = 0; i < maxlen && i < outsz - 1; i++) {
        if (buf[i] == '\0') break;
        out[i] = buf[i];
    }
    out[i] = '\0';
}

/* ===================================================================
 * Module / Symbol / Reloc lists
 * =================================================================== */

static TModule *alloc_module(LinkerState *ls, const char *fname,
                             int is_lib, uint32_t offset, uint32_t size,
                             int group_id)
{
    TModule *p  = (TModule *)malloc(sizeof(TModule));
    if (!p) olink_error("out of memory");
    p->filename      = xstrdup(fname);
    p->is_lib_member = is_lib;
    p->lib_offset    = offset;
    p->raw_size      = size;
    p->hdr_size      = 0;
    p->code_size     = 0;
    p->data_size     = 0;
    p->bss_size      = 0;
    p->group_id      = group_id;
    p->final_code_ofs = 0;
    p->group_code_para= 0;
    p->final_data_ofs = 0;
    p->final_bss_ofs  = 0;
    p->used          = 0;
    p->next          = NULL;

    /* O(1) tail append */
    if (ls->mod_tail) ls->mod_tail->next = p;
    else              ls->mod_head = p;
    ls->mod_tail = p;
    return p;
}

static unsigned int sym_hash_key(const char *name)
{
    unsigned int h = 0;
    while (*name) { h = h * 31u + (unsigned char)*name++; }
    return h & (SYM_HASH_SIZE - 1);
}

static void add_symbol(LinkerState *ls, const char *name,
                       TModule *owner, uint8_t seg_id, uint32_t offset)
{
    unsigned int slot;
    TSymbol *p = (TSymbol *)malloc(sizeof(TSymbol));
    if (!p) olink_error("out of memory");
    p->name      = xstrdup(name);
    p->owner     = owner;
    p->seg_id    = seg_id;
    p->offset    = offset;
    p->next      = ls->sym_head;
    ls->sym_head = p;
    /* insert into hash table */
    slot = sym_hash_key(name);
    p->hash_next = ls->sym_hash[slot];
    ls->sym_hash[slot] = p;
}

static TSymbol *find_symbol(LinkerState *ls, const char *name)
{
    unsigned int slot = sym_hash_key(name);
    TSymbol *p = ls->sym_hash[slot];
    while (p) {
        if (strcmp(p->name, name) == 0) return p;
        p = p->hash_next;
    }
    return NULL;
}

static void add_reloc(LinkerState *ls, uint16_t seg, uint16_t ofs)
{
    TMZReloc *p = (TMZReloc *)malloc(sizeof(TMZReloc));
    if (!p) olink_error("out of memory");
    p->seg  = seg;
    p->ofs  = ofs;
    p->next = NULL;
    if (!ls->reloc_head) ls->reloc_head = p;
    else ls->reloc_tail->next = p;
    ls->reloc_tail = p;
    ls->mz_reloc_cnt++;
}

static TImportNode *add_import_node(TImportNode *head, uint16_t id, TSymbol *sym)
{
    TImportNode *p = (TImportNode *)malloc(sizeof(TImportNode));
    if (!p) olink_error("out of memory");
    p->id   = id;
    p->sym  = sym;
    p->next = head;
    return p;
}

static TSymbol *find_import(TImportNode *head, uint16_t id)
{
    while (head) {
        if (head->id == id) return head->sym;
        head = head->next;
    }
    return NULL;
}

static void free_imports(TImportNode *head)
{
    TImportNode *n;
    while (head) {
        n = head->next;
        free(head);
        head = n;
    }
}

/* ===================================================================
 * PatchMem — apply a fixup to code_buf or data_buf
 * =================================================================== */

static void patch_mem(LinkerState *ls, uint8_t seg_type, uint32_t offset,
                      int32_t val, uint8_t width)
{
    uint8_t *p;
    uint16_t cur;

    if (seg_type == SEG_CODE) {
        if (offset + (uint32_t)width > ls->total_code_len) olink_error("patch out of bounds");
        p = ls->code_buf + offset;
    } else {
        if (offset + (uint32_t)width > ls->total_data_len) olink_error("patch out of bounds");
        p = ls->data_buf + offset;
    }

    switch (width) {
    case 1:
        p[0] = (uint8_t)((p[0] + (uint8_t)(val & 0xFF)) & 0xFF);
        break;
    case 2:
        cur = (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
        cur = (uint16_t)(cur + (uint16_t)(val & 0xFFFF));
        p[0] = (uint8_t)(cur & 0xFF);
        p[1] = (uint8_t)((cur >> 8) & 0xFF);
        break;
    default:
        olink_error("unsupported patch width");
    }
}

/* ===================================================================
 * ScanRDF — read RDOFF2 header to discover exported symbols and sizes
 * =================================================================== */

static void scan_rdf(LinkerState *ls, TModule *m)
{
    FILE *f;
    uint8_t sig[6];
    uint8_t tmp4[4];
    uint8_t tmp2[2];
    uint8_t  rec_type, rec_len;
    uint32_t end_hdr, next_rec_pos;
    uint32_t bss_amt;
    uint8_t  sym_flg, sym_seg;
    uint32_t sym_ofs;
    char     sym_name[128];
    uint16_t stype, snum, sres;
    uint32_t slen;
    long     file_pos;

    f = fopen(m->filename, "rb");
    if (!f) olink_error2("Cannot open module: ", m->filename);

    fseek(f, (long)m->lib_offset, SEEK_SET);

    xfread(sig, 6, f, m->filename);
    if (memcmp(sig, "RDOFF2", 6) != 0)
        olink_error2("Invalid RDOFF2 signature in ", m->filename);

    xfread(tmp4, 4, f, m->filename);
    read_u32_buf(tmp4, &m->raw_size);

    xfread(tmp4, 4, f, m->filename);
    read_u32_buf(tmp4, &m->hdr_size);

    /* header records start at offset 0x0E from lib_offset */
    end_hdr = m->lib_offset + 0x0E + m->hdr_size;
    fseek(f, (long)(m->lib_offset + 0x0E), SEEK_SET);
    next_rec_pos = m->lib_offset + 0x0E;

    while (next_rec_pos < end_hdr) {
        xfread(&rec_type, 1, f, m->filename);
        xfread(&rec_len,  1, f, m->filename);
        next_rec_pos += 2 + rec_len;

        if (rec_type == RDFREC_GLOBAL) {
            xfread(&sym_flg, 1, f, m->filename);
            xfread(&sym_seg, 1, f, m->filename);
            xfread(tmp4, 4, f, m->filename);
            read_u32_buf(tmp4, &sym_ofs);
            read_string(f, sym_name, sizeof(sym_name));
            add_symbol(ls, sym_name, m, sym_seg, sym_ofs);
        } else if (rec_type == RDFREC_BSS) {
            xfread(tmp4, 4, f, m->filename);
            read_u32_buf(tmp4, &bss_amt);
            m->bss_size += bss_amt;
        }

        fseek(f, (long)next_rec_pos, SEEK_SET);
    }

    /* segment blocks after header */
    fseek(f, (long)end_hdr, SEEK_SET);
    file_pos = (long)end_hdr;
    for (;;) {
        if ((uint32_t)file_pos >= m->lib_offset + m->raw_size) break;
        if (fread(tmp2, 1, 2, f) != 2) break;
        read_u16_buf(tmp2, &stype);
        if (stype == 0) break;
        xfread(tmp2, 2, f, m->filename); read_u16_buf(tmp2, &snum);
        xfread(tmp2, 2, f, m->filename); read_u16_buf(tmp2, &sres);
        xfread(tmp4, 4, f, m->filename); read_u32_buf(tmp4, &slen);
        if (stype == 1) m->code_size = slen;
        else if (stype == 2) m->data_size = slen;
        fseek(f, (long)slen, SEEK_CUR);
        file_pos += 10 + (long)slen;   /* 2+2+2+4 header + data */
        (void)snum; (void)sres;
    }

    fclose(f);
}

/* ===================================================================
 * ScanTAR — enumerate .rdf members inside a ustar tar archive
 * =================================================================== */

#define TAR_NAME_OFS   0
#define TAR_SIZE_OFS   124
#define TAR_TYPE_OFS   156
#define TAR_MAGIC_OFS  257

static void scan_tar(LinkerState *ls, const char *fname)
{
    FILE *f;
    uint8_t hdr[512];
    char name_buf[101];
    char size_buf[12];
    uint32_t file_sz, cur_pos, mod_size, padding;
    uint8_t type_flag;
    int bytes;

    f = fopen(fname, "rb");
    if (!f) olink_error2("Cannot open LIB: ", fname);
    file_sz = get_file_size(f);
    cur_pos = 0;

    while (cur_pos + 512 <= file_sz) {
        fseek(f, (long)cur_pos, SEEK_SET);
        bytes = (int)fread(hdr, 1, 512, f);
        if (bytes < 512) break;

        if (hdr[TAR_NAME_OFS] == '\0') break;  /* EOF blocks */

        extract_tar_name((const char *)(hdr + TAR_NAME_OFS), 100,
                         name_buf, sizeof(name_buf));
        memcpy(size_buf, hdr + TAR_SIZE_OFS, 12);
        mod_size  = parse_octal(size_buf, 12);
        type_flag = hdr[TAR_TYPE_OFS];

        if ((type_flag == '0' || type_flag == '\0')
            && str_ends_with(name_buf, ".rdf")) {
            /* register only .rdf members; data starts after the 512-byte header */
            TModule *m = alloc_module(ls, fname, 1, cur_pos + 512, mod_size,
                                      ls->next_group_id);
            scan_rdf(ls, m);
        }

        padding  = (mod_size + 511u) & ~(uint32_t)511u;
        cur_pos += 512u + padding;
    }

    fclose(f);
    ls->next_group_id++;  /* next .om gets a fresh group */
}

/* ===================================================================
 * ScanFile — detect format and dispatch
 * =================================================================== */

static void scan_file(LinkerState *ls, const char *raw_name)
{
    char fname[256];
    FILE *f;
    uint8_t sig[8];
    uint8_t tar_magic[5];
    uint32_t file_sz;
    int n;

    find_file(ls, raw_name, fname, sizeof(fname));

    f = fopen(fname, "rb");
    if (!f) olink_error2("Cannot open: ", raw_name);
    file_sz = get_file_size(f);

    n = (int)fread(sig, 1, 8, f);
    if (n >= 6 && memcmp(sig, "RDOFF2", 6) == 0) {
        fclose(f);
        olink_error2("Standalone .rdf not accepted; pack into .om first: ", raw_name);
    }

    /* check for ustar magic at offset 257 */
    memset(tar_magic, ' ', 5);
    if (file_sz > 262) {
        fseek(f, 257, SEEK_SET);
        fread(tar_magic, 1, 5, f);
    }
    fclose(f);

    if (memcmp(tar_magic, "ustar", 5) == 0 || str_ends_with(fname, ".OM")) {
        scan_tar(ls, fname);
    } else {
        olink_error2("Unknown file format: ", fname);
    }
}

/* ===================================================================
 * olink_init / olink_free
 * =================================================================== */

void olink_init(LinkerState *ls)
{
    memset(ls, 0, sizeof(*ls));
    ls->stack_size = STACK_SIZE;
}

void olink_free(LinkerState *ls)
{
    TModule     *m, *mn;
    TSymbol     *s, *sn;
    TMZReloc    *r, *rn;
    int i;

    for (m = ls->mod_head; m; m = mn) {
        mn = m->next;
        free(m->filename);
        free(m);
    }
    for (s = ls->sym_head; s; s = sn) {
        sn = s->next;
        free(s->name);
        free(s);
    }
    for (r = ls->reloc_head; r; r = rn) {
        rn = r->next;
        free(r);
    }
    for (i = 0; i < ls->n_lib_paths; i++) {
        free(ls->lib_paths[i]);
    }
    free(ls->code_buf);
    free(ls->data_buf);
    memset(ls, 0, sizeof(*ls));
}

/* ===================================================================
 * olink_process_input
 * =================================================================== */

void olink_process_input(LinkerState *ls, const char *arg)
{
    if (arg[0] == '@') {
        /* listfile */
        FILE *lst;
        char line[256];
        lst = fopen(arg + 1, "r");
        if (!lst) olink_error2("Cannot open list file: ", arg + 1);
        while (fgets(line, sizeof(line), lst)) {
            char *trimmed = str_trim(line);
            if (*trimmed) scan_file(ls, trimmed);
        }
        fclose(lst);
    } else {
        scan_file(ls, arg);
    }
}

/* ===================================================================
 * MarkModule — recursive reachability from a module
 * =================================================================== */

static void mark_module(LinkerState *ls, TModule *m)
{
    FILE *f;
    uint8_t  rec_type, rec_len;
    uint32_t end_hdr, next_rec_pos;
    uint8_t  imp_flg;
    uint8_t  tmp2[2];
    uint16_t imp_seg;
    char     imp_name[128];
    TSymbol *sym;
    char     errbuf[256];

    if (m->used) return;
    m->used = 1;

    f = fopen(m->filename, "rb");
    if (!f) olink_error2("Cannot open: ", m->filename);

    end_hdr = m->lib_offset + 0x0E + m->hdr_size;
    fseek(f, (long)(m->lib_offset + 0x0E), SEEK_SET);
    next_rec_pos = m->lib_offset + 0x0E;

    while (next_rec_pos < end_hdr) {
        xfread(&rec_type, 1, f, m->filename);
        xfread(&rec_len,  1, f, m->filename);
        next_rec_pos += 2 + rec_len;

        if (rec_type == RDFREC_IMPORT) {
            xfread(&imp_flg, 1, f, m->filename);
            xfread(tmp2, 2, f, m->filename);
            read_u16_buf(tmp2, &imp_seg);
            read_string(f, imp_name, sizeof(imp_name));

            sym = find_symbol(ls, imp_name);
            if (sym) {
                mark_module(ls, sym->owner);
            } else {
                snprintf(errbuf, sizeof(errbuf),
                         "Unresolved external: %s in %s",
                         imp_name, path_basename(m->filename));
                olink_error(errbuf);
            }
        }
        fseek(f, (long)next_rec_pos, SEEK_SET);
    }
    fclose(f);
}

/* ===================================================================
 * olink_smart_link
 * =================================================================== */

void olink_smart_link(LinkerState *ls)
{
    TSymbol *s = find_symbol(ls, "start");
    if (!s) olink_error("Entry point \"start\" not found!");
    ls->entry_found = 1;
    mark_module(ls, s->owner);
}

/* ===================================================================
 * olink_calculate_layout
 * =================================================================== */

/* align to 2-byte word boundary */
static uint32_t align2(uint32_t v)
{
    return (v + 1u) & ~(uint32_t)1u;
}

void olink_calculate_layout(LinkerState *ls)
{
    TModule *m, *n;
    uint32_t code_base = 0, data_base = 0, bss_base = 0;
    uint32_t group_code_start;
    uint16_t group_para;
    int cur_group;
    uint32_t group_end;
    TSymbol *entry_sym;
    int entry_group;

    /* Move the entry group to the front of the module list so that the
       entry .om always occupies the lowest code addresses regardless of
       the argument order on the command line. */
    entry_sym = find_symbol(ls, "start");
    if (entry_sym && entry_sym->owner) {
        TModule *prev = NULL, *cur2;
        entry_group = entry_sym->owner->group_id;
        /* find the first module with entry_group */
        cur2 = ls->mod_head;
        while (cur2 && cur2->group_id != entry_group) {
            prev = cur2;
            cur2 = cur2->next;
        }
        if (cur2 && prev) {
            /* detach the contiguous run of entry_group modules */
            TModule *run_head = cur2;
            TModule *run_tail = cur2;
            while (run_tail->next && run_tail->next->group_id == entry_group)
                run_tail = run_tail->next;
            /* splice out: prev -> run_tail->next */
            prev->next = run_tail->next;
            /* prepend: run_head..run_tail -> old head */
            run_tail->next = ls->mod_head;
            ls->mod_head = run_head;
        }
    }

    /* Walk modules in scan order.  For each group (consecutive same group_id
       among used modules), paragraph-align the group start, pack members with
       2-byte word alignment, then record the group's CS paragraph for all
       members. Data and BSS are combined globally (16-byte aligned per module,
       same as before). */

    m = ls->mod_head;
    while (m) {
        if (!m->used) { m = m->next; continue; }

        /* Start a new group: paragraph-align the code base. */
        code_base = align16(code_base);
        group_code_start = code_base;
        group_para       = (uint16_t)(group_code_start >> 4);
        cur_group        = m->group_id;

        /* First pass over this group: assign code offsets (2-byte aligned within group). */
        n = m;
        while (n && n->group_id == cur_group) {
            if (n->used) {
                n->final_code_ofs  = code_base;
                n->group_code_para = group_para;
                code_base = align2(code_base + n->code_size);
            }
            n = n->next;
        }
        group_end = code_base;
        if (group_end - group_code_start > 65000u)
            olink_error("code group exceeds 64KB");

        /* Assign data/bss offsets for the same group members. */
        n = m;
        while (n && n->group_id == cur_group) {
            if (n->used) {
                n->final_data_ofs = data_base;
                n->final_bss_ofs  = bss_base;
                data_base = align16(data_base + n->data_size);
                bss_base  = align16(bss_base  + n->bss_size);
            }
            n = n->next;
        }

        /* Advance m past this group. */
        while (m && m->group_id == cur_group) m = m->next;
    }

    ls->total_code_len = align16(code_base);  /* paragraph-round final code length */
    ls->total_data_len = data_base;
    ls->total_bss_len  = bss_base;

    if (ls->total_data_len + ls->total_bss_len > 65520)
        olink_error("Data+BSS segment exceeds 64KB");
}

/* ===================================================================
 * olink_perform_linking
 * =================================================================== */

void olink_perform_linking(LinkerState *ls)
{
    TModule     *m;
    TImportNode *import_head;
    FILE        *f;
    uint8_t tmp1[1], tmp2[2], tmp4[4];
    uint8_t  rec_type, rec_len;
    uint32_t end_hdr, next_rec_pos;
    uint8_t  rseg, rrel_flag, rwidth;
    uint32_t rofs;
    uint16_t rref_seg;
    uint8_t  imp_flg;
    uint16_t imp_seg;
    char     imp_name[128];
    uint16_t stype, snum, sres;
    uint32_t slen;
    int32_t  fixup_val;
    uint32_t patch_addr;
    TSymbol *sym;

    /* allocate segment buffers */
    /* Note: total_code_len may exceed 64KB — each .om group has its own CS paragraph.
       Validate per-group size during layout (see olink_calculate_layout).
       Data is combined into one 16-bit segment, so that limit is real. */
    if (ls->total_data_len > 65000u) olink_error("data segment too large");
    ls->code_buf = (uint8_t *)malloc(ls->total_code_len ? (size_t)ls->total_code_len : 1);
    ls->data_buf = (uint8_t *)malloc(ls->total_data_len ? (size_t)ls->total_data_len : 1);
    if (!ls->code_buf || !ls->data_buf) olink_error("out of memory");
    memset(ls->code_buf, 0, ls->total_code_len);
    memset(ls->data_buf, 0, ls->total_data_len);

    ls->mz_reloc_cnt = 0;
    ls->reloc_head   = NULL;
    ls->reloc_tail   = NULL;

    for (m = ls->mod_head; m; m = m->next) {
        if (!m->used) continue;

        import_head = NULL;

        f = fopen(m->filename, "rb");
        if (!f) olink_error2("Cannot open: ", m->filename);

        /* --- load segment data --- */
        fseek(f, (long)(m->lib_offset + 0x0E + m->hdr_size), SEEK_SET);
        for (;;) {
            long pos = ftell(f);
            if ((uint32_t)pos >= m->lib_offset + m->raw_size) break;
            if (fread(tmp2, 1, 2, f) != 2) break;
            read_u16_buf(tmp2, &stype);
            if (stype == 0) break;
            xfread(tmp2, 2, f, m->filename); read_u16_buf(tmp2, &snum);
            xfread(tmp2, 2, f, m->filename); read_u16_buf(tmp2, &sres);
            xfread(tmp4, 4, f, m->filename); read_u32_buf(tmp4, &slen);
            if (stype == 1) {
                fread(ls->code_buf + m->final_code_ofs, 1, (size_t)slen, f);
            } else if (stype == 2) {
                fread(ls->data_buf + m->final_data_ofs, 1, (size_t)slen, f);
            } else {
                fseek(f, (long)slen, SEEK_CUR);
            }
            (void)snum; (void)sres;
        }

        /* --- pass 1: collect IMPORT records into import_head --- */
        end_hdr = m->lib_offset + 0x0E + m->hdr_size;
        fseek(f, (long)(m->lib_offset + 0x0E), SEEK_SET);
        next_rec_pos = m->lib_offset + 0x0E;

        while (next_rec_pos < end_hdr) {
            xfread(&rec_type, 1, f, m->filename);
            xfread(&rec_len,  1, f, m->filename);
            next_rec_pos += 2 + rec_len;

            if (rec_type == RDFREC_IMPORT) {
                xfread(&imp_flg, 1, f, m->filename);
                xfread(tmp2, 2, f, m->filename);
                read_u16_buf(tmp2, &imp_seg);
                read_string(f, imp_name, sizeof(imp_name));
                sym = find_symbol(ls, imp_name);
                import_head = add_import_node(import_head, imp_seg, sym);
            }
            fseek(f, (long)next_rec_pos, SEEK_SET);
        }

        /* --- pass 2: process RELOC / SEGRELOC records --- */
        fseek(f, (long)(m->lib_offset + 0x0E), SEEK_SET);
        next_rec_pos = m->lib_offset + 0x0E;

        while (next_rec_pos < end_hdr) {
            xfread(&rec_type, 1, f, m->filename);
            xfread(&rec_len,  1, f, m->filename);
            next_rec_pos += 2 + rec_len;

            switch (rec_type) {

            case RDFREC_IMPORT:
                /* already handled in pass 1 */
                break;

            case RDFREC_RELOC:
                xfread(tmp1, 1, f, m->filename); rseg = tmp1[0];
                xfread(tmp4, 4, f, m->filename); read_u32_buf(tmp4, &rofs);
                xfread(tmp1, 1, f, m->filename); rwidth = tmp1[0];
                xfread(tmp2, 2, f, m->filename); read_u16_buf(tmp2, &rref_seg);

                rrel_flag = rseg & 0x40;
                rseg      = rseg & 0x3F;

                if (rseg == SEG_CODE)
                    patch_addr = m->final_code_ofs + rofs;
                else
                    patch_addr = m->final_data_ofs + rofs;

                /* group_ofs(mod): byte offset of a module's code start within
                   its .om group's code segment (= segment-relative offset). */
#define group_ofs(mod) ((int32_t)((mod)->final_code_ofs) \
                        - (int32_t)((uint32_t)(mod)->group_code_para << 4))

                fixup_val = 0;
                if (rref_seg == SEG_CODE) {
                    /* Intra-module code reference.
                       Non-relative (FAR offset word): compiler stored the value
                       relative to this module's code start; adjust to group start.
                       IP-relative: need absolute offset in combined code buffer. */
                    if (rrel_flag)
                        fixup_val = (int32_t)m->final_code_ofs;
                    else
                        fixup_val = group_ofs(m);
                } else if (rref_seg == SEG_DATA) {
                    fixup_val = (int32_t)m->final_data_ofs;
                } else if (rref_seg == 2) {
                    fixup_val = (int32_t)(ls->total_data_len + m->final_bss_ofs);
                } else {
                    sym = find_import(import_head, rref_seg);
                    if (!sym) olink_error("Relocation to missing import");
                    fixup_val = (int32_t)sym->offset;
                    switch (sym->seg_id) {
                    case 0:
                        /* FAR CALL offset word: compiler stored offset relative
                           to the target module's own code start; adjust to its
                           group start (CS will be set to that group by SEGRELOC).
                           IP-relative: absolute combined code offset. */
                        if (rrel_flag)
                            fixup_val += (int32_t)sym->owner->final_code_ofs;
                        else
                            fixup_val += group_ofs(sym->owner);
                        break;
                    case 1: fixup_val += (int32_t)sym->owner->final_data_ofs; break;
                    case 2: fixup_val = (int32_t)(ls->total_data_len
                                + sym->owner->final_bss_ofs + sym->offset); break;
                    }
                }
#undef group_ofs

                if (rrel_flag) {
                    /* IP-relative fixup: the displacement must be relative to
                       the byte immediately after the patched field (patch_addr + rwidth).
                       The compiler already stored addend = -(patchAt+width) in the field,
                       so the linker computes: *field += symAddress + addend
                       which equals symAddress - (origPatchAt + width).
                       After layout, origPatchAt = patch_addr (absolute in combined seg).
                       We already have fixup_val = absolute symAddress, so subtract
                       (patch_addr + rwidth) to make it relative. */
                    fixup_val -= (int32_t)(patch_addr + rwidth);
                }

                patch_mem(ls, rseg, patch_addr, fixup_val, rwidth);
                break;

            case RDFREC_SEGRELOC:
                /* SEGRELOC patches a 2-byte segment word in a FAR call or
                   segment-load instruction.  fixup_val is the 0-based paragraph
                   index of the referenced segment within the load image (i.e.
                   relative to the first byte after the MZ header).  The MZ
                   loader adds the actual load paragraph, giving the final value.
                   We also emit a MZ relocation entry so the loader can do that. */
                xfread(tmp1, 1, f, m->filename); rseg = tmp1[0];
                xfread(tmp4, 4, f, m->filename); read_u32_buf(tmp4, &rofs);
                xfread(tmp1, 1, f, m->filename); rwidth = tmp1[0];
                xfread(tmp2, 2, f, m->filename); read_u16_buf(tmp2, &rref_seg);

                if (rseg == SEG_CODE)
                    patch_addr = m->final_code_ofs + rofs;
                else
                    patch_addr = m->final_data_ofs + rofs;

                fixup_val = 0;
                if (rref_seg == SEG_CODE) {
                    /* References this module's own .om group code segment. */
                    fixup_val = (int32_t)m->group_code_para;
                } else if (rref_seg == SEG_DATA || rref_seg == 2) {
                    /* Data or BSS: one combined segment after all code.
                       Its paragraph index = total_code_len / 16. */
                    fixup_val = (int32_t)(ls->total_code_len >> 4);
                } else {
                    /* Import: resolve to the symbol's owning segment. */
                    sym = find_import(import_head, rref_seg);
                    if (sym) {
                        if (sym->seg_id == 0) {
                            /* code segment: CS paragraph of the owning .om group */
                            fixup_val = (int32_t)sym->owner->group_code_para;
                        } else {
                            /* data or BSS: shared data segment paragraph */
                            fixup_val = (int32_t)(ls->total_code_len >> 4);
                        }
                    }
                }

                patch_mem(ls, rseg, patch_addr, fixup_val, 2);

                /* MZ relocation entry: location of the segment word to be fixed up.
                   For code-resident: use the .om group's CS paragraph + offset of the
                   word within that group (final_code_ofs - group_start + rofs).
                   For data-resident: use data segment paragraph. */
                if (rseg == SEG_CODE) {
                    add_reloc(ls, m->group_code_para,
                              (uint16_t)(m->final_code_ofs - ((uint32_t)m->group_code_para << 4) + rofs));
                } else
                    add_reloc(ls, (uint16_t)(ls->total_code_len >> 4),
                              (uint16_t)rofs);
                break;

            } /* switch */

            fseek(f, (long)next_rec_pos, SEEK_SET);
        } /* while header records */

        fclose(f);
        free_imports(import_head);
    } /* for each module */

    /* resolve entry point
       Large model: entry_ip is the offset within the entry module's own code
       segment (= sym->offset, since the module's code is paragraph-aligned and
       entry_ip is relative to CS:0000 of that module).
       entry_cs_para is the 0-based paragraph index of that code segment within
       the load image (added to hdr_para in olink_write_exe to get the MZ CS). */
    /* Entry point: "start" is within an .om group.
       entry_ip = sym->offset + module's offset within the group (final_code_ofs - group_start).
       entry_cs_para = group_code_para of the module owning "start". */
    sym = find_symbol(ls, "start");
    ls->entry_ip      = (uint16_t)(sym->offset
                        + sym->owner->final_code_ofs
                        - ((uint32_t)sym->owner->group_code_para << 4));
    ls->entry_cs_para = sym->owner->group_code_para;
}

/* ===================================================================
 * olink_write_exe
 * =================================================================== */

void olink_write_exe(LinkerState *ls, const char *out_filename)
{
    FILE      *fout;
    uint16_t   code_para, data_bss_para, hdr_para;
    uint32_t   exe_size;
    TMZReloc  *rel;

    /* Segment layout (all paragraphs 0-based from load image start):
     *   [0 .. code_para-1]                  code segments (one per .om group;
     *                                        modules within a group share one CS)
     *   [code_para .. code_para+data_bss_para-1]  combined data+BSS segment
     *   [code_para+data_bss_para ..]        stack segment (ls->stack_size bytes)
     *
     * The MZ header fields SS and CS are relative to the load segment.
     * The loader sets load_seg = PSP_segment + 0x10 (segment after PSP).
     * At runtime:
     *   CS = load_seg + hdr_para + entry_cs_para
     *   DS = load_seg + hdr_para + code_para
     *   SS = load_seg + hdr_para + code_para + data_bss_para
     *   SP = ls->stack_size - 2
     */

    /* MZ header fields */
    uint16_t f_sig       = 0x5A4D;
    uint16_t f_part_page = 0;
    uint16_t f_page_cnt  = 0;
    uint16_t f_reloc_cnt;
    uint16_t f_hdr_size;
    uint16_t f_min_alloc = 0;     /* no extra paragraphs beyond the image */
    uint16_t f_max_alloc = 0xFFFF; /* give as much memory as DOS offers */
    uint16_t f_ss;
    uint16_t f_sp        = (uint16_t)(ls->stack_size - 2);
    uint16_t f_csum      = 0;
    uint16_t f_ip;
    uint16_t f_cs;
    uint16_t f_relo_ofs;
    uint16_t f_overlay   = 0;

    fout = fopen(out_filename, "wb");
    if (!fout) olink_error2("Cannot write output file: ", out_filename);

    /* Round code length up to paragraph boundary (should already be, but be safe). */
    code_para = (uint16_t)((ls->total_code_len + 15u) >> 4);

    /* Data+BSS in one segment, rounded up to paragraph boundary. */
    {
        uint32_t data_bss = ls->total_data_len + ls->total_bss_len;
        data_bss_para = (uint16_t)((data_bss + 15u) >> 4);
    }

    /* Stack: separate segment after data+BSS (STACK_SIZE bytes). */

    /* Header size in paragraphs: 28-byte fixed header + 4 bytes per reloc entry,
       rounded up to paragraph, plus 1. */
    hdr_para = (uint16_t)((MZ_HDR_BYTES + ls->mz_reloc_cnt * 4 + 15u) >> 4);
    hdr_para++;

    /* MZ SS and CS are relative to the load segment (= PSP_seg + 0x10 at runtime).
       The MZ loader adds the load segment to these, giving the actual segment values.
       We store them relative to the start of the load image (after the header).
       hdr_para is added by the loader implicitly (since it loads image at hdr_para). */
    f_cs = (uint16_t)(ls->entry_cs_para);          /* relative to image start */
    f_ss = (uint16_t)(code_para + data_bss_para);  /* stack segment after data+BSS */
    f_ip = ls->entry_ip;

    f_reloc_cnt = ls->mz_reloc_cnt;
    f_hdr_size  = hdr_para;
    f_relo_ofs  = MZ_HDR_BYTES;

    /* File size = header + code + data (BSS and stack not in file). */
    exe_size    = (uint32_t)hdr_para * 16u
                + ls->total_code_len
                + ls->total_data_len;
    f_page_cnt  = (uint16_t)(exe_size / 512);
    f_part_page = (uint16_t)(exe_size % 512);
    if (f_part_page != 0) f_page_cnt++;

    /* write MZ header (28 bytes, field by field) */
    write_u16(fout, f_sig);
    write_u16(fout, f_part_page);
    write_u16(fout, f_page_cnt);
    write_u16(fout, f_reloc_cnt);
    write_u16(fout, f_hdr_size);
    write_u16(fout, f_min_alloc);
    write_u16(fout, f_max_alloc);
    write_u16(fout, f_ss);
    write_u16(fout, f_sp);
    write_u16(fout, f_csum);
    write_u16(fout, f_ip);
    write_u16(fout, f_cs);
    write_u16(fout, f_relo_ofs);
    write_u16(fout, f_overlay);

    /* write MZ relocation table
       Each entry: (offset, segment) where segment is 0-based from load image start.
       The MZ loader adds the load paragraph to every segment word in the image at
       these locations, then adds load_paragraph to the stored segment to get the
       runtime segment.  Our segment values already account for position within the
       image; the loader adds hdr_para automatically because the image starts there. */
    for (rel = ls->reloc_head; rel; rel = rel->next) {
        write_u16(fout, rel->ofs);
        write_u16(fout, rel->seg);
    }

    /* pad header to hdr_para * 16 bytes */
    {
        static const uint8_t zeros[512] = {0};
        long gap = (long)hdr_para * 16 - ftell(fout);
        while (gap > 0) {
            size_t chunk = (gap > 512) ? 512 : (size_t)gap;
            fwrite(zeros, 1, chunk, fout);
            gap -= (long)chunk;
        }
    }

    /* code segments (all modules concatenated, paragraph-aligned gaps zero-filled) */
    if (ls->total_code_len)
        fwrite(ls->code_buf, 1, (size_t)ls->total_code_len, fout);

    /* data segment (initialised data; BSS is implicitly zero beyond file end) */
    if (ls->total_data_len)
        fwrite(ls->data_buf, 1, (size_t)ls->total_data_len, fout);

    /* stack and BSS are not written to file; DOS zero-extends the image */

    fclose(fout);

    {
        char bn[256];
        strncpy(bn, path_basename(out_filename), sizeof(bn) - 1);
        bn[sizeof(bn) - 1] = '\0';
        str_upper(bn);
        printf("%s: Code=%lu, Data=%lu, BSS=%lu, Stack=%lu\n",
               bn,
               (unsigned long)ls->total_code_len,
               (unsigned long)ls->total_data_len,
               (unsigned long)ls->total_bss_len,
               (unsigned long)ls->stack_size);
    }
}

/* ===================================================================
 * main
 * =================================================================== */

int main(int argc, char **argv)
{
    LinkerState ls;
    int i;
    int show_help = 0;
    const char *out_filename = NULL;

    if (argc < 2) show_help = 1;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-H") == 0 ||
            strcmp(argv[i], "-?") == 0 ||
            strcmp(argv[i], "/h") == 0 || strcmp(argv[i], "/H") == 0 ||
            strcmp(argv[i], "/?") == 0) {
            show_help = 1;
        }
    }

    if (show_help) {
        printf("OLINK - RDOFF2 Smart Linker for MS-DOS\n");
        printf("\n");
        printf("Usage: olink [options] <input> ... <output.exe>\n");
        printf("\n");
        printf("Inputs:\n");
        printf("  filename.om     Oberon module archive (.rdf + .def packed by oc)\n");
        printf("  @listfile       Text file with list of inputs (one per line)\n");
        printf("\n");
        printf("Options:\n");
        printf("  -L path         Add library search path\n");
        printf("  -h, -?          Show this help\n");
        printf("\n");
        printf("Environment:\n");
        printf("  OBERON_LIB      Semicolon-separated (DOS) or colon-separated\n");
        printf("                  (Linux) list of library search paths\n");
        printf("\n");
        printf("Output:\n");
        printf("  DOS MZ executable (.exe)\n");
        printf("  Smart linking: only modules reachable from \"start\" are included\n");
        return 1;
    }

    olink_init(&ls);

    /* last non-option argument is the output filename */
    out_filename = argv[argc - 1];

    for (i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "-L") == 0 && i + 1 < argc - 1) {
            if (ls.n_lib_paths < 16) {
                ls.lib_paths[ls.n_lib_paths] = xstrdup(argv[i + 1]);
                ls.n_lib_paths++;
            }
            i++;
        } else if (argv[i][0] == '-') {
            /* unknown option — ignore */
        } else {
            olink_process_input(&ls, argv[i]);
        }
    }

    olink_smart_link(&ls);

    /* Read META-INF/STACK.TXT from entry .om to override default stack size. */
    {
        TSymbol *start_sym = find_symbol(&ls, "start");
        if (start_sym && start_sym->owner && start_sym->owner->filename) {
            FILE *sf = tar_extract_file(start_sym->owner->filename,
                                        "STACK.TXT");
            if (sf) {
                char buf[32];
                if (fgets(buf, (int)sizeof(buf), sf)) {
                    long val = 0;
                    char *p = buf;
                    while (*p == ' ' || *p == '\t') p++;
                    if (*p >= '0' && *p <= '9') {
                        val = 0;
                        while (*p >= '0' && *p <= '9') {
                            val = val * 10 + (*p - '0');
                            p++;
                        }
                    }
                    if (val >= 2 && val <= 65536)
                        ls.stack_size = (uint32_t)val;
                    else
                        fprintf(stderr, "olink: META-INF/STACK.TXT: invalid stack size %ld, using default\n", val);
                }
                fclose(sf);
            }
        }
    }

    olink_calculate_layout(&ls);
    olink_perform_linking(&ls);
    olink_write_exe(&ls, out_filename);

    olink_free(&ls);
    return 0;
}
