#include "scanner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---- keyword table (must stay sorted for bsearch) ---- */
static const struct { const char *name; Token tok; } kw[] = {
    {"ARRAY",     T_ARRAY},
    {"BEGIN",     T_BEGIN},
    {"BY",        T_BY},
    {"CASE",      T_CASE},
    {"CONST",     T_CONST},
    {"DIV",       T_DIV},
    {"DO",        T_DO},
    {"ELSE",      T_ELSE},
    {"ELSIF",     T_ELSIF},
    {"END",       T_END},
    {"FALSE",     T_FALSE},
    {"FOR",       T_FOR},
    {"IF",        T_IF},
    {"IMPORT",    T_IMPORT},
    {"IN",        T_IN},
    {"IS",        T_IS},
    {"MOD",       T_MOD},
    {"MODULE",    T_MODULE},
    {"NIL",       T_NIL},
    {"OF",        T_OF},
    {"OR",        T_OR},
    {"POINTER",   T_POINTER},
    {"PROCEDURE", T_PROCEDURE},
    {"RECORD",    T_RECORD},
    {"REPEAT",    T_REPEAT},
    {"RETURN",    T_RETURN},
    {"THEN",      T_THEN},
    {"TO",        T_TO},
    {"TRUE",      T_TRUE},
    {"TYPE",      T_TYPE},
    {"UNTIL",     T_UNTIL},
    {"VAR",       T_VAR},
    {"WHILE",     T_WHILE},
};
#define NKW (int)(sizeof(kw)/sizeof(kw[0]))

typedef struct { const char *name; Token tok; } KW;

static int kw_cmp(const void *key, const void *elem) {
    return strcmp((const char*)key, ((const KW*)elem)->name);
}

static Token lookup_keyword(const char *s) {
    KW *r = bsearch(s, kw, NKW, sizeof(kw[0]), kw_cmp);
    return r ? r->tok : T_IDENT;
}

/* ---- private scanner state ---- */
#define MAX_LINE_BUF 256
typedef struct {
    Scanner *pub;
    FILE    *f;
    int      ch;          /* lookahead char (-1 = EOF) */
    int      pending_sym; /* -1 = none; else a token to return on next call */
    char     line_buf[MAX_LINE_BUF]; /* text of current line (NUL-terminated) */
    int      line_len;               /* chars accumulated so far */
} SC;

static void readch(SC *sc) {
    sc->ch = fgetc(sc->f);
    if (sc->ch == '\n') {
        sc->pub->line++;
        sc->pub->col = 0;
        sc->line_len = 0;
        sc->line_buf[0] = '\0';
    } else if (sc->ch != EOF) {
        sc->pub->col++;
        if (sc->line_len < MAX_LINE_BUF - 1) {
            sc->line_buf[sc->line_len++] = (char)sc->ch;
            sc->line_buf[sc->line_len]   = '\0';
        }
    }
    sc->pub->cur_line = sc->line_buf;
}

static void scan_error(SC *sc, const char *msg) {
    fprintf(stderr, "%s(%d): %s\n%s\n",
            sc->pub->filename, sc->pub->line, msg, sc->pub->cur_line);
    sc->pub->err_count++;
}

#define MAX_SYSCOMMENT 256

static void dispatch_syscomment(SC *sc, const char *body) {
    char directive;
    const char *arg;
    if (!sc->pub->on_syscomment || body[0] == '\0') return;
    directive = body[0];
    arg = body + 1;
    sc->pub->on_syscomment(sc->pub, directive, arg);
}

static void skip_comment(SC *sc) {
    int depth = 1;
    /* check for system comment (*$...*) */
    if (sc->ch == '$') {
        char buf[MAX_SYSCOMMENT];
        int n = 0;
        readch(sc); /* consume '$' */
        /* read until closing *) or EOF, collect body */
        while (sc->ch != EOF) {
            if (sc->ch == '*') {
                readch(sc);
                if (sc->ch == ')') { readch(sc); break; }
                if (n < MAX_SYSCOMMENT-1) buf[n++] = '*';
                continue;
            }
            if (n < MAX_SYSCOMMENT-1) buf[n++] = (char)sc->ch;
            readch(sc);
        }
        buf[n] = '\0';
        dispatch_syscomment(sc, buf);
        return;
    }
    while (depth > 0 && sc->ch != EOF) {
        if (sc->ch == '(') {
            readch(sc);
            if (sc->ch == '*') { depth++; readch(sc); }
        } else if (sc->ch == '*') {
            readch(sc);
            if (sc->ch == ')') { depth--; readch(sc); }
        } else {
            readch(sc);
        }
    }
    if (depth > 0) scan_error(sc, "unterminated comment");
}

static void scan_ident(SC *sc) {
    int i = 0;
    while (isalpha(sc->ch) || isdigit(sc->ch)) {
        if (i < MAX_IDENT-1) sc->pub->id[i++] = sc->ch;
        readch(sc);
    }
    sc->pub->id[i] = '\0';
    sc->pub->sym = lookup_keyword(sc->pub->id);
}

static int ishex(int c) {
    return (c>='0'&&c<='9') || (c>='A'&&c<='F');
}
static int hexval(int c) {
    return (c>='0'&&c<='9') ? c-'0' : c-'A'+10;
}

static void scan_number(SC *sc) {
    char buf[64]; int n=0, has_hex=0;
    while (ishex(sc->ch)) {
        if (sc->ch>='A') has_hex=1;
        if (n<63) buf[n++]=sc->ch;
        readch(sc);
    }
    buf[n]='\0';
    if (sc->ch == 'H') {            /* hex integer */
        uint32_t v=0; int i;
        readch(sc);
        for(i=0;i<n;i++) v=v*16+(uint32_t)hexval(buf[i]);
        sc->pub->ival = (int32_t)v;
        sc->pub->sym  = T_INT;
    } else if (sc->ch == 'X') {     /* char literal */
        int32_t v=0; int i;
        readch(sc);
        for(i=0;i<n;i++) v=v*16+hexval(buf[i]);
        if (v>255) scan_error(sc,"char literal out of range");
        sc->pub->ival = v;
        sc->pub->sym  = T_CHAR;
    } else if (sc->ch == '.' && sc->pub->sym != T_DOTDOT) {
        /* real literal: digits . [digits] [E [+|-] digits]
           With single-char lookahead we consume '.' first; if the next char
           is also '.' (range ".."), emit T_INT for the integer part.  The
           second '.' is left in sc->ch and becomes T_DOT on the next call —
           known limitation: "1..10" scans as INT DOT DOT INT, not INT DOTDOT INT. */
        char rbuf[64];
        int rn = 0;
        int i;
        double v;
        if (has_hex) scan_error(sc,"hex digit in real literal");
        for(i=0;i<n && rn<63;i++) rbuf[rn++] = buf[i];
        readch(sc);
        if (sc->ch == '.') {
            /* digits followed by ".." — emit T_INT now, schedule T_DOTDOT next */
            uint32_t lv = 0; int j;
            for(j=0;j<n;j++) lv = lv*10 + (uint32_t)(buf[j]-'0');
            sc->pub->ival = (int32_t)lv;
            sc->pub->sym  = T_INT;
            readch(sc);  /* consume the second '.'; sc->ch is now the char after '..' */
            sc->pending_sym = T_DOTDOT;
            return;
        }
        rbuf[rn++] = '.';
        /* fractional digits */
        while (isdigit(sc->ch) && rn < 63) {
            rbuf[rn++] = (char)sc->ch; readch(sc);
        }
        /* optional exponent */
        if ((sc->ch == 'E' || sc->ch == 'e') && rn < 62) {
            rbuf[rn++] = 'E'; readch(sc);
            if ((sc->ch == '+' || sc->ch == '-') && rn < 62) {
                rbuf[rn++] = (char)sc->ch; readch(sc);
            }
            while (isdigit(sc->ch) && rn < 63) {
                rbuf[rn++] = (char)sc->ch; readch(sc);
            }
        }
        rbuf[rn] = '\0';
        v = atof(rbuf);
        sc->pub->rval = v;
        sc->pub->sym  = T_REAL;
    } else {                        /* decimal integer */
        uint32_t v=0;
        int i;
        if (has_hex) scan_error(sc,"hex digit in decimal literal");
        for(i=0;i<n;i++) {
            v = v*10 + (uint32_t)(buf[i]-'0');
        }
        sc->pub->ival = (int32_t)v;
        sc->pub->sym  = T_INT;
    }
}

static void scan_string(SC *sc, int delim) {
    int i=0;
    readch(sc);   /* skip opening delimiter */
    while (sc->ch != delim && sc->ch != '\n' && sc->ch != EOF) {
        if (i < MAX_STR-1) sc->pub->sval[i++] = sc->ch;
        readch(sc);
    }
    sc->pub->sval[i] = '\0';
    if (sc->ch == delim) readch(sc);
    else scan_error(sc, "unterminated string");
    sc->pub->sym = T_STRING;
}

/* ---- one static SC per scanner (single-file compiler) ---- */
static SC gsc;

void scanner_open(Scanner *s, const char *filename) {
    void (*cb)(Scanner*, char, const char*) = s->on_syscomment;
    memset(s, 0, sizeof(*s));
    s->on_syscomment = cb;
    s->line     = 1;
    s->filename = filename;
    gsc.line_len    = 0;
    gsc.line_buf[0] = '\0';
    gsc.pending_sym = -1;
    gsc.pub = s;
    s->cur_line = gsc.line_buf;
    gsc.f   = fopen(filename, "r");
    if (!gsc.f) { fprintf(stderr,"cannot open %s\n",filename); exit(1); }
    readch(&gsc);
    scanner_next(s);   /* prime first token */
}

void scanner_close(Scanner *s) {
    (void)s;
    if (gsc.f) { fclose(gsc.f); gsc.f=NULL; }
}

void scanner_next(Scanner *s) {
    SC *sc = &gsc;
    int c;
    /* check for pending token (set by scan_number when n.. is scanned) */
    if (sc->pending_sym >= 0) {
        s->sym = (Token)sc->pending_sym;
        sc->pending_sym = -1;
        return;
    }
    /* skip whitespace */
    while (sc->ch==' '||sc->ch=='\t'||sc->ch=='\n'||sc->ch=='\r')
        readch(sc);

    if (sc->ch == EOF) { s->sym = T_EOF; return; }

    if (isalpha(sc->ch))           { scan_ident(sc);  return; }
    if (isdigit(sc->ch))           { scan_number(sc); return; }
    if (sc->ch=='"'||sc->ch=='\'') { scan_string(sc,sc->ch); return; }

    c = sc->ch; readch(sc);
    switch (c) {
    case '+': s->sym=T_PLUS;   break;
    case '-': s->sym=T_MINUS;  break;
    case '*': s->sym=T_STAR;   break;
    case '/':
        if (sc->ch=='/') {
            readch(sc); /* consume second '/' */
            if (sc->ch == '$') {
                char buf[MAX_SYSCOMMENT];
                int n = 0;
                readch(sc); /* consume '$' */
                while (sc->ch != '\n' && sc->ch != EOF) {
                    if (n < MAX_SYSCOMMENT-1) buf[n++] = (char)sc->ch;
                    readch(sc);
                }
                buf[n] = '\0';
                dispatch_syscomment(sc, buf);
            } else {
                while (sc->ch != '\n' && sc->ch != EOF) readch(sc);
            }
            scanner_next(s);
        } else s->sym=T_SLASH;
        break;
    case '=': s->sym=T_EQL;    break;
    case '#': s->sym=T_NEQ;    break;
    case '&': s->sym=T_AND;    break;
    case '~': s->sym=T_NOT;    break;
    case '^': s->sym=T_ARROW;  break;
    case '|': s->sym=T_BAR;    break;
    case ',': s->sym=T_COMMA;  break;
    case ';': s->sym=T_SEMI;   break;
    case '[': s->sym=T_LBRAK;  break;
    case ']': s->sym=T_RBRAK;  break;
    case '{': s->sym=T_LBRACE; break;
    case '}': s->sym=T_RBRACE; break;
    case ')': s->sym=T_RPAREN; break;
    case '<': s->sym = (sc->ch=='=') ? (readch(sc),T_LEQ) : T_LSS; break;
    case '>': s->sym = (sc->ch=='=') ? (readch(sc),T_GEQ) : T_GTR; break;
    case ':': s->sym = (sc->ch=='=') ? (readch(sc),T_ASSIGN) : T_COLON; break;
    case '.': s->sym = (sc->ch=='.') ? (readch(sc),T_DOTDOT) : T_DOT; break;
    case '(':
        if (sc->ch=='*') { readch(sc); skip_comment(sc); scanner_next(s); }
        else s->sym = T_LPAREN;
        break;
    default:
        scan_error(sc,"illegal character");
        s->sym = T_ERROR;
        break;
    }
}
