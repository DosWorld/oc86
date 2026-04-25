#ifndef PARSER_H
#define PARSER_H

#include "scanner.h"
#include "symbols.h"
#include "codegen.h"

extern int parser_errors;
extern int parser_system_mode;  /* set to 1 by -SYSTEM flag: disables implicit SYSTEM import */
extern const char *parser_entry_proc; /* set by -entry: proc name that must exist and be exported */
extern char parser_mod_name[33]; /* module name read from source (set after MODULE keyword parsed) */

#define PARSER_MAX_EXTRA_RDFS 128

typedef struct ExtraRdf {
    char            *path;
    struct ExtraRdf *next;
} ExtraRdf;

extern ExtraRdf *parser_extra_rdfs_head; /* linked list of $L paths */
extern int       parser_n_extra_rdfs;    /* count (capped at PARSER_MAX_EXTRA_RDFS) */
extern long      parser_stack_size;      /* $M directive value (0 = not set) */

void parse_module(Scanner *s);
void parser_syscomment(Scanner *s, char directive, const char *arg);

#endif
