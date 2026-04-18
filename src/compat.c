#include "compat.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

char *path_basename(const char *path) {
    const char *p = path;
    const char *last = path;
    while (*p) {
        if (IS_SEP(*p)) last = p + 1;
        p++;
    }
    return (char *)last;
}

void path_join(char *dst, int dstsz, const char *dir, const char *file) {
    int dlen;
    strncpy(dst, dir, (size_t)(dstsz - 1));
    dst[dstsz - 1] = '\0';
    dlen = (int)strlen(dst);
    if (dlen > 0 && !IS_SEP(dst[dlen - 1]) && dlen < dstsz - 1) {
        dst[dlen] = PATH_SEP;
        dst[dlen + 1] = '\0';
        dlen++;
    }
    strncpy(dst + dlen, file, (size_t)(dstsz - dlen - 1));
    dst[dstsz - 1] = '\0';
}

void path_normalize(char *path) {
    char *p = path;
    while (*p) {
        if (IS_SEP(*p)) *p = PATH_SEP;
        p++;
    }
}

int file_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}

void str_upcase(char *dst, int dstsz, const char *src) {
    int i = 0;
    while (src[i] && i < dstsz - 1) {
        dst[i] = (char)toupper((unsigned char)src[i]);
        i++;
    }
    dst[i] = '\0';
}
