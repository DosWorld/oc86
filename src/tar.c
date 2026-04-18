#include "tar.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* ustar header layout (POSIX.1-1988) */
typedef struct {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
} UstarHeader;   /* exactly 512 bytes */

static void fill_octal(char *field, int width, uint32_t val) {
    char buf[32];
    int i;
    /* write octal digits right-to-left, then copy left-padded with zeros */
    for (i = width - 2; i >= 0; i--) {
        buf[i] = '0' + (val & 7);
        val >>= 3;
    }
    buf[width - 1] = '\0';
    memcpy(field, buf, width);
}

static uint32_t tar_checksum(const UstarHeader *h) {
    const uint8_t *p = (const uint8_t *)h;
    uint32_t sum = 0;
    int i;
    for (i = 0; i < 512; i++) {
        /* checksum field is treated as all spaces during calculation */
        if (i >= 148 && i < 156)
            sum += ' ';
        else
            sum += p[i];
    }
    return sum;
}

void tar_begin(FILE *f) {
    (void)f;  /* nothing to write at start */
}

void tar_add_file(FILE *f, const char *name,
                  const uint8_t *data, size_t len) {
    UstarHeader h;
    static const uint8_t zero_block[512];
    size_t padded;
    uint32_t csum;
    char cs[8];

    memset(&h, 0, sizeof(h));

    /* name (truncated to 99 chars + NUL) */
    strncpy(h.name, name, 99);

    fill_octal(h.mode,  8, 0644);
    fill_octal(h.uid,   8, 0);
    fill_octal(h.gid,   8, 0);
    fill_octal(h.size, 12, (uint32_t)len);
    fill_octal(h.mtime,12, 0);

    h.typeflag = '0';  /* regular file */

    memcpy(h.magic,   "ustar", 5);
    h.magic[5] = '\0';
    h.version[0] = '0';
    h.version[1] = '0';

    /* compute and write checksum */
    csum = tar_checksum(&h);
    /* stored as 6-digit octal + NUL + space (POSIX convention) */
    fill_octal(cs, 7, csum);
    cs[7] = ' ';
    memcpy(h.checksum, cs, 8);

    fwrite(&h, 512, 1, f);

    /* data, padded to multiple of 512 */
    if (len > 0) fwrite(data, 1, len, f);
    padded = (len + 511) & ~(size_t)511;
    if (padded > len) fwrite(zero_block, 1, padded - len, f);
}

void tar_end(FILE *f) {
    static const uint8_t zero_block[512];
    fwrite(zero_block, 512, 1, f);
    fwrite(zero_block, 512, 1, f);
}

/* Parse an octal field from a tar header. */
static uint32_t parse_octal(const char *field, int width) {
    uint32_t val = 0;
    int i;
    for (i = 0; i < width && field[i] >= '0' && field[i] <= '7'; i++)
        val = val * 8 + (uint32_t)(field[i] - '0');
    return val;
}

static int str_icmp(const char *a, const char *b) {
    while (*a && *b) {
        int ca = toupper((unsigned char)*a);
        int cb = toupper((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return toupper((unsigned char)*a) - toupper((unsigned char)*b);
}

/* Return the basename of a path (pointer to last component). */
static const char *tar_basename(const char *p) {
    const char *s = p;
    while (*p) { if (*p == '/' || *p == '\\') s = p + 1; p++; }
    return s;
}

FILE *tar_extract_file(const char *lib_path, const char *member_name) {
    FILE *lib;
    UstarHeader h;
    uint32_t fsize;
    size_t padded;
    uint8_t *buf;
    FILE *tmp;
    const char *bname;

    lib = fopen(lib_path, "rb");
    if (!lib) return NULL;

    for (;;) {
        if (fread(&h, 512, 1, lib) != 1) break;
        /* Two zero blocks = end of archive */
        if (h.name[0] == '\0') break;

        fsize  = parse_octal(h.size, 12);
        padded = (fsize + 511) & ~(size_t)511;

        /* Compare basename of header name to member_name (case-insensitive) */
        bname = tar_basename(h.name);
        if (str_icmp(bname, member_name) == 0 && fsize > 0) {
            if (fsize > 65000u) { fclose(lib); return NULL; }
            buf = (uint8_t *)malloc((size_t)fsize);
            if (!buf) { fclose(lib); return NULL; }
            if (fread(buf, 1, fsize, lib) != fsize) {
                free(buf); fclose(lib); return NULL;
            }
            fclose(lib);
            /* Write to tmpfile so caller gets a FILE* */
            tmp = tmpfile();
            if (!tmp) { free(buf); return NULL; }
            fwrite(buf, 1, fsize, tmp);
            free(buf);
            rewind(tmp);
            return tmp;
        }

        /* skip this member's data */
        fseek(lib, (long)padded, SEEK_CUR);
    }
    fclose(lib);
    return NULL;
}
