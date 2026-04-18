#ifndef PARSER_H
#define PARSER_H

#include "scanner.h"
#include "symbols.h"
#include "codegen.h"

extern int parser_errors;
extern int parser_system_mode;  /* set to 1 by -SYSTEM flag: disables implicit SYSTEM import */

#define PARSER_MAX_EXTRA_RDFS 128

typedef struct ExtraRdf {
    char            *path;
    struct ExtraRdf *next;
} ExtraRdf;

extern ExtraRdf *parser_extra_rdfs_head; /* linked list of $L paths */
extern int       parser_n_extra_rdfs;    /* count (capped at PARSER_MAX_EXTRA_RDFS) */

void parse_module(Scanner *s);
void parser_syscomment(Scanner *s, char directive, const char *arg);

#endif
