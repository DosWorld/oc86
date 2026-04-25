#include "scanner.h"
#include "parser.h"
#include "codegen.h"
#include "rdoff.h"
#include "tar.h"
#include "compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Generate MAIN.RDF:
     CALL FAR SYSTEM_INIT       ; runtime init
     CALL FAR mymod__init       ; module init (transitively inits imports)
     CALL FAR mymod_entryproc   ; entry point
     CALL FAR SYSTEM_DONE       ; runtime cleanup / DOS exit

   RDOFF header imports (in order):
     seg 3: SYSTEM_INIT
     seg 4: SYSTEM_DONE
     seg 5: mymod__init
     seg 6: mymod_entryproc

   Also exports GLOBAL "start" at code offset 0 (linker entry).         */
static void gen_entry_stub(const char *mod_name, const char *proc_name,
                            const char *out_rdf) {
    ObjFile obj;
    int sys_init_id, sys_done_id, mod_init_id, proc_id;
    char init_sym[128];
    char proc_sym[128];

    rdf_init(&obj);

    sys_init_id = rdf_add_import(&obj, "SYSTEM_INIT");
    sys_done_id = rdf_add_import(&obj, "SYSTEM_DONE");

    snprintf(init_sym, sizeof(init_sym), "%s__init", mod_name);
    mod_init_id = rdf_add_import(&obj, init_sym);

    snprintf(proc_sym, sizeof(proc_sym), "%s_%s", mod_name, proc_name);
    proc_id = rdf_add_import(&obj, proc_sym);

    /* Export "start" at offset 0 so the linker knows the entry point */
    rdf_add_global(&obj, "start", SEG_CODE, 0);

    /* Helper: emit a FAR call to an import.
       CALL FAR 9A uses absolute CS:IP, not IP-relative.
       addend=0, relative=0 so linker patches with absolute offset. */
#define EMIT_FAR_CALL(id)  do {                                         \
        uint16_t _oa, _sa;                                              \
        emit_byte(&obj, SEG_CODE, OP_CALL_FAR);                         \
        _oa = (uint16_t)obj.code_len;                                   \
        emit_byte(&obj, SEG_CODE, 0);                                   \
        emit_byte(&obj, SEG_CODE, 0);                                   \
        _sa = (uint16_t)obj.code_len;                                   \
        emit_byte(&obj, SEG_CODE, 0);                                   \
        emit_byte(&obj, SEG_CODE, 0);                                   \
        rdf_add_reloc   (&obj, SEG_CODE, _oa, 2, (id), 0);              \
        rdf_add_segreloc(&obj, SEG_CODE, _sa, (id));                    \
    } while(0)

    EMIT_FAR_CALL(sys_init_id);
    EMIT_FAR_CALL(mod_init_id);
    EMIT_FAR_CALL(proc_id);
    EMIT_FAR_CALL(sys_done_id);

#undef EMIT_FAR_CALL

    {
        FILE *f = fopen(out_rdf, "wb");
        if (!f) {
            fprintf(stderr, "cannot write %s\n", out_rdf);
            rdf_free(&obj);
            return;
        }
        rdf_write(&obj, f);
        fclose(f);
    }
    rdf_free(&obj);
    printf("wrote %s\n", out_rdf);
}

/* Prepend directories from -L flags to OBERON_LIB environment variable.
   Builds a new string: "dir1:dir2:existing_OBERON_LIB" and sets it.
   Uses static buffers to avoid large DOS stack allocations. */
static void set_lib_path(const char **lib_dirs, int n_lib_dirs) {
    /* Static: avoids large stack frame on 16-bit DOS (4KB stack limit). */
    static char buf[512];
    static char env_buf[528]; /* "OBERON_LIB=" + 512 + NUL */
    const char *existing;
    int i;
    int pos = 0;
    if (n_lib_dirs == 0) return;
    buf[0] = '\0';
    for (i = 0; i < n_lib_dirs; i++) {
        if (pos > 0 && pos < (int)sizeof(buf) - 2) buf[pos++] = ENV_SEP;
        strncpy(buf + pos, lib_dirs[i], sizeof(buf) - pos - 1);
        pos += (int)strlen(lib_dirs[i]);
        if (pos >= (int)sizeof(buf) - 2) { pos = (int)sizeof(buf) - 2; break; }
    }
    existing = getenv("OBERON_LIB");
    if (existing && existing[0]) {
        if (pos < (int)sizeof(buf) - 2) buf[pos++] = ENV_SEP;
        strncpy(buf + pos, existing, sizeof(buf) - pos - 1);
    }
    buf[sizeof(buf)-1] = '\0';
    /* putenv requires a persistent string; env_buf is static */
    snprintf(env_buf, sizeof(env_buf), "OBERON_LIB=%s", buf);
    putenv(env_buf);
}

int main(int argc, char **argv) {
    const char *entry_proc = NULL;
    const char *src_file   = NULL;
    const char *lib_dirs[32];
    int n_lib_dirs = 0;
    int verbose = 0;
    int i;

    if (argc < 2) {
        fprintf(stderr, "usage: oc [-L dir] [-entry ProcName] [-SYSTEM] [-v] <file.Mod>\n");
        return 1;
    }

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-entry") == 0 && i+1 < argc) {
            entry_proc = argv[++i];
        } else if (strcmp(argv[i], "-L") == 0 && i+1 < argc) {
            if (n_lib_dirs < 32) lib_dirs[n_lib_dirs++] = argv[++i];
            else { fprintf(stderr, "oc: too many -L dirs\n"); }
        } else if (strncmp(argv[i], "-L", 2) == 0 && argv[i][2] != '\0') {
            /* -Ldir (no space) */
            if (n_lib_dirs < 32) lib_dirs[n_lib_dirs++] = argv[i] + 2;
            else { fprintf(stderr, "oc: too many -L dirs\n"); }
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "-SYSTEM") == 0) {
            parser_system_mode = 1;
        } else if (argv[i][0] != '-') {
            src_file = argv[i];
        } else {
            fprintf(stderr, "oc: unknown option '%s'\n", argv[i]);
            return 1;
        }
    }

    /* Apply -L dirs to OBERON_LIB before any file search */
    set_lib_path(lib_dirs, n_lib_dirs);
    (void)verbose; /* verbose output to be wired into parser/def later */

    if (!src_file) {
        fprintf(stderr, "oc: no source file\n");
        return 1;
    }

    {
        Scanner s;
        memset(&s, 0, sizeof(s));
        s.on_syscomment = parser_syscomment;
        parser_entry_proc = entry_proc;
        scanner_open(&s, src_file);
        parse_module(&s);
        scanner_close(&s);
    }

    if (parser_errors > 0) { cg_free(); return 1; }

    if (entry_proc) {
        const char *mod_name = parser_mod_name;
        char lib_name[68];
        size_t rdf_len, def_len, main_len;
        uint8_t *rdf_buf, *def_buf, *main_buf;
        FILE *f;
        FILE *lf;

        gen_entry_stub(mod_name, entry_proc, "MAIN.RDF");

        /* Repack ModName.om to include MAIN.RDF */
        snprintf(lib_name, sizeof(lib_name), "%s.om", mod_name);

        /* Read existing .rdf and .def from the lib */
        {
            char rdf_name[68], def_name[68];
            snprintf(rdf_name, sizeof(rdf_name), "%s.rdf", mod_name);
            snprintf(def_name, sizeof(def_name), "%s.def", mod_name);

            rdf_buf = NULL; rdf_len = 0;
            f = tar_extract_file(lib_name, rdf_name);
            if (f) {
                fseek(f, 0, SEEK_END); rdf_len = (size_t)ftell(f); rewind(f);
                rdf_buf = (uint8_t*)malloc(rdf_len);
                if (rdf_buf) fread(rdf_buf, 1, rdf_len, f);
                fclose(f);
            }

            def_buf = NULL; def_len = 0;
            f = tar_extract_file(lib_name, def_name);
            if (f) {
                fseek(f, 0, SEEK_END); def_len = (size_t)ftell(f); rewind(f);
                def_buf = (uint8_t*)malloc(def_len);
                if (def_buf) fread(def_buf, 1, def_len, f);
                fclose(f);
            }

            main_buf = NULL; main_len = 0;
            f = fopen("MAIN.RDF", "rb");
            if (f) {
                fseek(f, 0, SEEK_END); main_len = (size_t)ftell(f); rewind(f);
                main_buf = (uint8_t*)malloc(main_len);
                if (main_buf) fread(main_buf, 1, main_len, f);
                fclose(f);
            }

            if (!rdf_buf || !def_buf || !main_buf) {
                fprintf(stderr, "oc: -entry: missing %s in %s (cannot rewrite .om)\n",
                        !rdf_buf ? rdf_name : (!def_buf ? def_name : "MAIN.RDF"),
                        lib_name);
                free(rdf_buf); free(def_buf); free(main_buf);
                remove("MAIN.RDF");
                cg_free();
                return 1;
            }
            /* Rewrite lib with _main.rdf prepended; all names uppercase */
            lf = fopen(lib_name, "wb");
            if (lf) {
                char up_rdf[68], up_def[68];
                str_upcase(up_rdf, (int)sizeof(up_rdf), rdf_name);
                str_upcase(up_def, (int)sizeof(up_def), def_name);
                tar_begin(lf);
                tar_add_file(lf, "MAIN.RDF", main_buf, main_len);
                tar_add_file(lf, up_rdf,    rdf_buf,  rdf_len);
                tar_add_file(lf, up_def,    def_buf,  def_len);
                if (parser_stack_size > 0) {
                    char stack_txt[32];
                    int stack_txt_len;
                    stack_txt_len = snprintf(stack_txt, sizeof(stack_txt),
                                            "%ld\n", parser_stack_size);
                    tar_add_file(lf, "META-INF/STACK.TXT",
                                 (uint8_t*)stack_txt, (size_t)stack_txt_len);
                }
                tar_end(lf);
                fclose(lf);
            }
            free(rdf_buf); free(def_buf); free(main_buf);
            remove("MAIN.RDF");
        }
    }

    cg_free();
    return 0;
}
