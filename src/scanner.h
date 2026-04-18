#ifndef SCANNER_H
#define SCANNER_H

#include <stdint.h>

#define MAX_IDENT  33
#define MAX_STR   257

/* token kinds — plain #defines, no enum sequencing surprises */

#define T_IDENT     0
#define T_INT       1
#define T_CHAR      2
#define T_STRING    3
#define T_REAL      5  /* floating-point literal: rval holds the value */

#define T_PLUS      10
#define T_MINUS     11
#define T_STAR      12
#define T_SLASH     13
#define T_EQL       14
#define T_NEQ       15
#define T_LSS       16
#define T_LEQ       17
#define T_GTR       18
#define T_GEQ       19
#define T_AND       20
#define T_OR2       21
#define T_NOT       22
#define T_ARROW     23
#define T_ASSIGN    24
#define T_DOTDOT    25

#define T_LPAREN    30
#define T_RPAREN    31
#define T_LBRAK     32
#define T_RBRAK     33
#define T_LBRACE    34
#define T_RBRACE    35
#define T_DOT       36
#define T_COMMA     37
#define T_SEMI      38
#define T_COLON     39
#define T_BAR       40

#define T_ARRAY     50
#define T_BEGIN     51
#define T_BY        52
#define T_CASE      53
#define T_CONST     54
#define T_DIV       55
#define T_DO        56
#define T_ELSE      57
#define T_ELSIF     58
#define T_END       59
#define T_FALSE     60
#define T_FOR       61
#define T_IF        62
#define T_IMPORT    63
#define T_IN        64
#define T_IS        65
#define T_MOD       66
#define T_MODULE    67
#define T_NIL       68
#define T_OF        69
#define T_OR        70
#define T_POINTER   71
#define T_PROCEDURE 72
#define T_RECORD    73
#define T_REPEAT    74
#define T_RETURN    75
#define T_THEN      76
#define T_TO        77
#define T_TRUE      78
#define T_TYPE      79
#define T_UNTIL     80
#define T_VAR       81
#define T_WHILE     82

#define T_EOF       99
#define T_ERROR    100

typedef int Token;

typedef struct Scanner Scanner;
struct Scanner {
    Token       sym;
    char        id[MAX_IDENT];
    int32_t     ival;
    double      rval;   /* T_REAL: parsed floating-point value */
    char        sval[MAX_STR];
    int         line, col;
    int         err_count;
    const char *filename;    /* source file path (set by scanner_open) */
    const char *cur_line;    /* pointer into SC.line_buf: text of current line */
    /* optional callback for system comments (*$...*) and //$... */
    void      (*on_syscomment)(Scanner *s, char directive, const char *arg);
};

void scanner_open(Scanner *s, const char *filename);
void scanner_close(Scanner *s);
void scanner_next(Scanner *s);

#endif
