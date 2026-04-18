#include "import.h"
#include "def.h"
#include "tar.h"
#include "compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbols.h"

/* Try to open a .def file directly or extract it from a .om archive.
   'defname' is e.g. "Foo.def".  'libname' is e.g. "Foo.om".
   Tries direct open first, then tar extraction from the .om. */
static FILE *try_open_def(const char *dir, const char *defname,
                          const char *libname, char *found_path, int path_sz) {
    char path[512];
    FILE *f;

    /* try direct .def */
    if (dir && dir[0])
        path_join(path, (int)sizeof(path), dir, defname);
    else
        strncpy(path, defname, sizeof(path)-1);
    path[sizeof(path)-1] = '\0';
    f = fopen(path, "r");
    if (f) {
        if (found_path) strncpy(found_path, path, (size_t)path_sz - 1);
        return f;
    }

    /* try inside .om archive */
    if (dir && dir[0])
        path_join(path, (int)sizeof(path), dir, libname);
    else
        strncpy(path, libname, sizeof(path)-1);
    path[sizeof(path)-1] = '\0';
    f = tar_extract_file(path, defname);
    if (f) {
        if (found_path) strncpy(found_path, path, (size_t)path_sz - 1);
        return f;
    }

    return NULL;
}

/* Search for a .def file (or extract it from a .om) in:
   1. current directory
   2. $OBERON_LIB directories (ENV_SEP-separated)
   'filename' is the .def name (e.g. "Foo.def").
   Returns an open FILE* or NULL.  If found_path != NULL, writes the path. */
FILE *find_import_file(const char *filename, char *found_path, int path_sz) {
    char libname[NAME_LEN + 4];
    const char *env;
    const char *p;
    char dir[256];
    int dlen;
    FILE *f;
    char *dot;

    strncpy(libname, filename, sizeof(libname) - 1);
    libname[sizeof(libname) - 1] = '\0';
    dot = strrchr(libname, '.');
    if (dot) strcpy(dot, ".om");

    /* 1. current directory */
    f = try_open_def(NULL, filename, libname, found_path, path_sz);
    if (f) return f;

    /* 2. $OBERON_LIB */
    env = getenv("OBERON_LIB");
    if (env) {
        p = env;
        while (*p) {
            dlen = 0;
            while (*p && *p != ENV_SEP && dlen < (int)sizeof(dir) - 1)
                dir[dlen++] = *p++;
            dir[dlen] = '\0';
            if (*p == ENV_SEP) p++;
            if (dlen == 0) continue;
            f = try_open_def(dir, filename, libname, found_path, path_sz);
            if (f) return f;
        }
    }
    return NULL;
}

void load_explicit_import(const char *mod_name, const char *alias) {
    char deffile[NAME_LEN + 4];
    char found[512];
    FILE *f;
    const char *env;

    snprintf(deffile, sizeof(deffile), "%s.def", mod_name);
    found[0] = '\0';
    f = find_import_file(deffile, found, (int)sizeof(found));

    if (!f) {
        fprintf(stderr, "Error: cannot find module '%s.om' (or '%s.def')\n",
                mod_name, mod_name);
        fprintf(stderr, "  Searched: current directory\n");
        env = getenv("OBERON_LIB");
        if (env) fprintf(stderr, "  Searched: OBERON_LIB=%s\n", env);
        fprintf(stderr, "  Import of module '%s' cannot be resolved.\n", mod_name);
        exit(1);
    }

    def_read(f, alias);
    fclose(f);
}

void add_implicit_import(const char *mod_name) {
    char deffile[NAME_LEN + 4];
    FILE *f;

    snprintf(deffile, sizeof(deffile), "%s.def", mod_name);
    f = find_import_file(deffile, NULL, 0);

    if (f) {
        def_read(f, mod_name);
        fclose(f);
    }
    /* not found: not an error for SYSTEM — universe K_IMPORT handles access */
}
