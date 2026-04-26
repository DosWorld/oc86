#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma pack(push, 1)

#define TDINFO_MAGIC 0x52FB

typedef struct {
    uint16_t magic;              /* must be 0x52FB */
    uint8_t  minor_version;
    uint8_t  major_version;
    uint32_t names_pool_size;
    uint16_t names_count;
    uint16_t types_count;
    uint16_t members_count;
    uint16_t symbols_count;
    uint16_t globals_count;
    uint16_t modules_count;
    uint16_t locals_count;
    uint16_t scopes_count;
    uint16_t line_numbers_count;
    uint16_t source_files_count;
    uint16_t segments_count;
    uint16_t correlations_count;
    uint8_t  reserved[14];
    uint16_t extension_size;
} TDINFO_HEADER;

typedef struct {
    uint16_t index;
    uint16_t type;
    uint16_t offset;
    uint16_t segment;
    uint8_t  bitfield;
} SYMBOL_RECORD;

typedef struct {
    uint16_t name;
    uint8_t  padding[14];
} MODULE_RECORD;

typedef struct {
    uint16_t symbol_index;
    uint16_t symbol_count;
    uint16_t parent;
    uint16_t function;
    uint16_t offset;
    uint16_t length;
} SCOPE_RECORD;

typedef struct {
    uint16_t module;
    uint16_t code_segment;
    uint16_t code_offset;
    uint16_t code_length;
    uint16_t scope_index;
    uint16_t scope_count;
    uint8_t  padding[4];
} SEGMENT_RECORD;

typedef struct {
    uint8_t  id;
    uint16_t name;
    uint16_t size;
    uint8_t  class_type;
    uint16_t member_type;
} TYPE_RECORD;

typedef struct {
    uint8_t  info;
    uint16_t name;
    uint16_t type;
} MEMBER_RECORD;

#pragma pack(pop)

static char *symbol_class_names[] = {
    "STATIC", "ABSOLUTE", "AUTO", "PASCAL_VAR",
    "REGISTER", "CONSTANT", "TYPEDEF", "STRUCT_UNION_OR_ENUM"
};

static char *type_id_names[] = {
    "VOID", "LSTR", "DSTR", "PSTR", "SCHAR", "SINT", "SLONG",
    NULL, "UCHAR", "UINT", "ULONG", NULL, "PCHAR", "FLOAT", "TPREAL",
    "DOUBLE", "LDOUBLE", "BCD4", "BCD8", "BCD10", "BCDCOB",
    "NEAR", "FAR", "SEG", "NEAR386", "FAR386", "ARRAY", NULL, "PARRAY",
    NULL, "STRUCT", "UNION", NULL, NULL, "ENUM", "FUNCTION", "LABEL",
    "SET", "TFILE", "BFILE", "BOOL", "PENUM", NULL, NULL, "FUNCPROTOTYPE",
    "SPECIALFUNC", "OBJECT", NULL, NULL, NULL, NULL, NULL, "NREF", "FREF",
    "WORDBOOL", "LONGBOOL", NULL, NULL, NULL, NULL, NULL, NULL,
    "GLOBALHANDLE", "LOCALHANDLE"
};
#define TYPE_ID_MAX (sizeof(type_id_names) / sizeof(type_id_names[0]))

static char *get_name(uint16_t index, char *pool)
{
    uint16_t i;
    char *p;
    if (index == 0 || !pool) return NULL;
    p = pool;
    for (i = 1; i < index; i++) {
        p = strchr(p, '\0');
        if (!p) return NULL;
        p++;
    }
    return p;
}

static int safe_read(void *ptr, size_t size, size_t count, FILE *f)
{
    return fread(ptr, size, count, f) == count;
}

static int skip_bytes(FILE *f, long bytes)
{
    return fseek(f, bytes, SEEK_CUR) == 0;
}

static void hexdump(const uint8_t *data, int len)
{
    int i;
    for (i = 0; i < len; i++) printf("%02X ", data[i]);
}

int main(int argc, char **argv)
{
    uint16_t i, idx, cls;
    FILE *fp;
    long file_size, tdinfo_off, off;
    TDINFO_HEADER hdr;
    SYMBOL_RECORD *symbols = NULL;
    MODULE_RECORD *modules = NULL;
    uint8_t *src_files = NULL;
    uint8_t *line_nums = NULL;
    SCOPE_RECORD *scopes = NULL;
    SEGMENT_RECORD *segments = NULL;
    uint8_t *corrs = NULL;
    TYPE_RECORD *types = NULL;
    MEMBER_RECORD *members = NULL;
    char *names_pool = NULL;
    char *name, *mod, *id_str;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file.exe>\n", argv[0]);
        return 1;
    }

    fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("Failed to open file");
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    rewind(fp);

    tdinfo_off = -1;
    for (off = 0; off < file_size - 1; off++) {
        uint16_t magic;
        fseek(fp, off, SEEK_SET);
        if (safe_read(&magic, 2, 1, fp) && magic == TDINFO_MAGIC) {
            tdinfo_off = off;
            break;
        }
    }

    if (tdinfo_off < 0) {
        fprintf(stderr, "TDINFO signature (0xFB52) not found\n");
        fclose(fp);
        return 1;
    }

    printf("TDINFO at offset 0x%lX\n", tdinfo_off);
    fseek(fp, tdinfo_off, SEEK_SET);

    if (!safe_read(&hdr, sizeof(hdr), 1, fp)) {
        fprintf(stderr, "Error reading TDINFO header\n");
        fclose(fp);
        return 1;
    }

    printf("Borland TLINK v%u.%02u  names:%u types:%u members:%u symbols:%u(global:%u) modules:%u scopes:%u seg:%u src:%u lines:%u corr:%u pool:%u\n\n",
           hdr.major_version, hdr.minor_version,
           hdr.names_count, hdr.types_count, hdr.members_count,
           hdr.symbols_count, hdr.globals_count,
           hdr.modules_count, hdr.scopes_count, hdr.segments_count,
           hdr.source_files_count, hdr.line_numbers_count, hdr.correlations_count,
           hdr.names_pool_size);

    if (hdr.extension_size && !skip_bytes(fp, hdr.extension_size)) {
        fprintf(stderr, "Error skipping extension\n");
        fclose(fp);
        return 1;
    }


    if (hdr.symbols_count) {
        symbols = malloc(hdr.symbols_count * sizeof(SYMBOL_RECORD));
        if (!symbols || !safe_read(symbols, sizeof(SYMBOL_RECORD), hdr.symbols_count, fp)) goto fail;
    }
    if (hdr.modules_count) {
        modules = malloc(hdr.modules_count * sizeof(MODULE_RECORD));
        if (!modules || !safe_read(modules, sizeof(MODULE_RECORD), hdr.modules_count, fp)) goto fail;
    }
    if (hdr.source_files_count) {
        src_files = malloc(hdr.source_files_count * 6);
        if (!src_files || !safe_read(src_files, 6, hdr.source_files_count, fp)) goto fail;
    }
    if (hdr.line_numbers_count) {
        line_nums = malloc(hdr.line_numbers_count * 4);
        if (!line_nums || !safe_read(line_nums, 4, hdr.line_numbers_count, fp)) goto fail;
    }
    if (hdr.scopes_count) {
        scopes = malloc(hdr.scopes_count * sizeof(SCOPE_RECORD));
        if (!scopes || !safe_read(scopes, sizeof(SCOPE_RECORD), hdr.scopes_count, fp)) goto fail;
    }
    if (hdr.segments_count) {
        segments = malloc(hdr.segments_count * sizeof(SEGMENT_RECORD));
        if (!segments || !safe_read(segments, sizeof(SEGMENT_RECORD), hdr.segments_count, fp)) goto fail;
    }
    if (hdr.correlations_count) {
        corrs = malloc(hdr.correlations_count * 8);
        if (!corrs || !safe_read(corrs, 8, hdr.correlations_count, fp)) goto fail;
    }
    if (hdr.types_count) {
        types = malloc(hdr.types_count * sizeof(TYPE_RECORD));
        if (!types || !safe_read(types, sizeof(TYPE_RECORD), hdr.types_count, fp)) goto fail;
    }
    if (hdr.members_count) {
        members = malloc(hdr.members_count * sizeof(MEMBER_RECORD));
        if (!members || !safe_read(members, sizeof(MEMBER_RECORD), hdr.members_count, fp)) goto fail;
    }

    if (hdr.names_pool_size && hdr.names_count) {
        names_pool = malloc(hdr.names_pool_size);
        if (!names_pool) goto fail;
        fseek(fp, -(long)hdr.names_pool_size, SEEK_END);
        if (!safe_read(names_pool, 1, hdr.names_pool_size, fp)) {
            free(names_pool);
            names_pool = NULL;
        }
    }

    if (symbols) {
        printf("=== Symbols (%u) ===\n", hdr.symbols_count);
        for (i = 0; i < hdr.symbols_count; i++) {
            name = get_name(symbols[i].index, names_pool);
            cls = symbols[i].bitfield & 0x07;
            printf("  [%3u] %-40s type=%u seg:off=%04X:%04X class=%s\n",
                   i, name ? name : "(null)", symbols[i].type,
                   symbols[i].segment, symbols[i].offset, symbol_class_names[cls]);
        }
        putchar('\n');
    }

    if (modules) {
        printf("=== Modules (%u) ===\n", hdr.modules_count);
        for (i = 0; i < hdr.modules_count; i++) {
            printf("  [%3u] %s\n", i, get_name(modules[i].name, names_pool));
        }
        putchar('\n');
    }

    if (src_files) {
        printf("=== Source files (%u) ===\n", hdr.source_files_count);
        for (i = 0; i < hdr.source_files_count; i++) {
            printf("  [%3u] raw: ", i);
            hexdump(src_files + i*6, 6);
            putchar('\n');
        }
        putchar('\n');
    }

    if (line_nums) {
        printf("=== Line numbers (%u) ===\n", hdr.line_numbers_count);
        for (i = 0; i < hdr.line_numbers_count; i++) {
            printf("  [%3u] raw: ", i);
            hexdump(line_nums + i*4, 4);
            putchar('\n');
        }
        putchar('\n');
    }

    if (scopes) {
        printf("=== Scopes (%u) ===\n", hdr.scopes_count);
        for (i = 0; i < hdr.scopes_count; i++)
            printf("  [%3u] sym_first=%u cnt=%u parent=%u func=%u offset=%04X len=%04X\n",
                   i, scopes[i].symbol_index, scopes[i].symbol_count,
                   scopes[i].parent, scopes[i].function,
                   scopes[i].offset, scopes[i].length);
        putchar('\n');
    }

    if (segments) {
        printf("=== Segments (%u) ===\n", hdr.segments_count);
        for (i = 0; i < hdr.segments_count; i++) {
            mod = "?";
            if (modules && names_pool && segments[i].module > 0 && segments[i].module <= hdr.modules_count)
                mod = get_name(modules[segments[i].module - 1].name, names_pool);
            printf("  [%3u] module=%u (%s) seg:off=%04X:%04X len=%04X scope_first=%u scope_count=%u\n",
                   i, segments[i].module, mod ? mod : "?",
                   segments[i].code_segment, segments[i].code_offset,
                   segments[i].code_length,
                   segments[i].scope_index, segments[i].scope_count);
        }
        putchar('\n');
    }

    if (corrs) {
        printf("=== Correlations (%u) ===\n", hdr.correlations_count);
        for (i = 0; i < hdr.correlations_count; i++) {
            printf("  [%3u] raw: ", i);
            hexdump(corrs + i*8, 8);
            putchar('\n');
        }
        putchar('\n');
    }

    if (types) {
        printf("=== Types (%u) ===\n", hdr.types_count);
        for (i = 0; i < hdr.types_count; i++) {
            id_str = (types[i].id < TYPE_ID_MAX && type_id_names[types[i].id])
                     ? type_id_names[types[i].id] : "UNKNOWN";
            name = (types[i].name && names_pool) ? get_name(types[i].name, names_pool) : "(anonymous)";
            printf("  [%3u] %-20s id=%-12s size=%u class_type=%u member_type=%u\n",
                   i, name, id_str, types[i].size, types[i].class_type, types[i].member_type);
        }
        putchar('\n');
    }

    if (members) {
        printf("=== Members (%u) ===\n", hdr.members_count);
        for (i = 0; i < hdr.members_count; i++) {
            name = (members[i].name && names_pool) ? get_name(members[i].name, names_pool) : "(anonymous)";
            printf("  [%3u] %-30s type=%u info=0x%02X%s\n",
                   i, name, members[i].type, members[i].info,
                   (members[i].info == 0xC0) ? " [END]" : "");
        }
        putchar('\n');
    }

    if (names_pool && hdr.names_count) {
        printf("=== Names pool (%u names) ===\n", hdr.names_count);
        for (idx = 1; idx <= hdr.names_count; idx++)
            printf("  [%3u] %s\n", idx, get_name(idx, names_pool));
        putchar('\n');
    }

    free(symbols);
    free(modules);
    free(src_files);
    free(line_nums);
    free(scopes);
    free(segments);
    free(corrs);
    free(types);
    free(members);
    free(names_pool);
    fclose(fp);
    return 0;

fail:
    fprintf(stderr, "Memory allocation or read error\n");
    free(symbols);
    free(modules);
    free(src_files);
    free(line_nums);
    free(scopes);
    free(segments);
    free(corrs);
    free(types);
    free(members);
    free(names_pool);
    fclose(fp);
    return 1;
}
