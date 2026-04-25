#ifndef SYMBOLS_H
#define SYMBOLS_H

#include <stdint.h>

/* symbol kinds */
#define K_CONST    0
#define K_TYPE     1
#define K_VAR      2
#define K_PARAM    3
#define K_VARPARAM 4
#define K_PROC     5
#define K_FIELD    6
#define K_IMPORT   7
#define K_SYSPROC  8

/* type forms */
#define TF_INTEGER  0
#define TF_BOOLEAN  1
#define TF_CHAR     2
#define TF_BYTE     3
#define TF_SET      4
#define TF_NILTYPE  5
#define TF_NOTYPE   6
#define TF_ARRAY    7
#define TF_RECORD   8
#define TF_POINTER  9
#define TF_PROC    10
#define TF_ADDRESS 11  /* SYSTEM.ADDRESS: 4-byte far pointer, compat with any POINTER/INTEGER */
#define TF_LONGINT 12  /* signed 32-bit integer; stored as DX:AX (lo:hi) or 4-byte [BP+ofs] */
#define TF_REAL    14  /* 32-bit IEEE 754 single-precision float; FPU ST(0) when in register */
#define TF_LONGREAL 15 /* 64-bit IEEE 754 double-precision float; FPU ST(0) when in register */

/* system procedure IDs */
#define SP_NEW     0
#define SP_INC     1
#define SP_DEC     2
#define SP_LEN     3
#define SP_ODD     4
#define SP_ABS     5
#define SP_ASSERT  6
#define SP_COPY    7
#define SP_ORD     8
#define SP_CHR     9
#define SP_FLOOR  10
#define SP_DISPOSE 11  /* SYSTEM extension: free a heap pointer */
/* SYSTEM compiler intrinsic IDs (qualified via SYSTEM.xxx, not in SYSTEM.def) */
#define SP_ADR    12   /* SYSTEM.ADR(v)      -> ADDRESS  */
#define SP_VAL    13   /* SYSTEM.VAL(T,x)    -> T        */
#define SP_GET    14   /* SYSTEM.GET(a,v)    procedure   */
#define SP_PUT    15   /* SYSTEM.PUT(a,x)    procedure   */
#define SP_MOVE   16   /* SYSTEM.MOVE(s,d,n) procedure   */
#define SP_PTR    17   /* SYSTEM.PTR(s,o)    -> ADDRESS  */
#define SP_SEG    18   /* SYSTEM.SEG(v)      -> INTEGER  */
#define SP_OFS    19   /* SYSTEM.OFS(v)      -> INTEGER  */
#define SP_FILL   20   /* SYSTEM.FILL(d,n,b) procedure   */
#define SP_LSL    21   /* SYSTEM.LSL(x,n)    -> INTEGER  logical shift left  */
#define SP_LSR    24   /* SYSTEM.LSR(x,n)    -> INTEGER  logical shift right */
#define SP_ASR    22   /* SYSTEM.ASR(x,n)    -> INTEGER  arithmetic shift right */
#define SP_ROR    23   /* SYSTEM.ROR(x,n)    -> INTEGER  rotate right        */
/* type cast built-ins */
#define SP_TOINT  25   /* INTEGER(x)  -> INTEGER  truncate/cast to 16-bit    */
#define SP_TOBYTE 26   /* BYTE(x)     -> BYTE     truncate/cast to 8-bit     */
#define SP_TOLONG 27   /* LONGINT(x)  -> LONGINT  sign-extend/cast to 32-bit */

/* sizes */
#define SZ_INTEGER 2
#define SZ_BOOLEAN 1
#define SZ_CHAR    1
#define SZ_BYTE    1
#define SZ_SET     2
#define SZ_POINTER  4   /* far pointer: {offset:word, segment:word} */
#define SZ_LONGINT  4   /* 32-bit signed integer */
#define SZ_REAL     4   /* 32-bit IEEE 754 single-precision */
#define SZ_LONGREAL 8   /* 64-bit IEEE 754 double-precision */

#define MAX_LEVEL   8
#define NAME_LEN   33

typedef struct TypeDesc TypeDesc;
typedef struct Symbol   Symbol;
typedef struct Scope    Scope;

struct TypeDesc {
    int       form;
    int32_t   size;
    int       align;
    /* array */
    int32_t   len;        /* -1 = open array */
    TypeDesc *elem;
    /* record */
    Symbol   *fields;
    Symbol   *fields_tail;
    TypeDesc *base;
    int       tag_ofs;
    int       n_fields;
    /* proc */
    Symbol   *params;
    Symbol   *params_tail;
    TypeDesc *ret_type;
    int       n_params;
    int32_t   arg_size;   /* total param bytes (pascal) */
    int       is_far;     /* 1 = FAR calling convention (params start at BP+6) */
    int       has_sl;    /* 1 = nested proc: hidden static link at [BP+4], params at [BP+6] */
};

struct Symbol {
    char      name[NAME_LEN];
    int       kind;
    int       exported;
    TypeDesc *type;
    int       level;
    /* const */
    int32_t  val;
    /* var / param */
    int32_t   adr;        /* data ofs (global) or BP offset (local) */
    /* proc */
    uint32_t  code_ofs;
    int       rdoff_id;   /* import id or -1 */
    int       fwd_decl;
    int       typeless;    /* 1 = typeless VAR param (VAR x without type); value is ADDRESS */
    /* field */
    int32_t   offset;
    /* import */
    char      mod_name[NAME_LEN];
    /* sysproc */
    int       sys_id;

    Symbol   *next;
};

struct Scope {
    Symbol  *symbols;
    int      n_syms;
    Scope   *outer;
    int      level;
    int32_t  local_top;  /* next negative BP offset, starts at -2 */
    int32_t  param_bot;  /* next positive BP offset, starts at 4 */
};

/* predeclared types */
extern TypeDesc *type_integer, *type_boolean, *type_char,
               *type_byte, *type_set, *type_niltype, *type_notype,
               *type_address, *type_longint, *type_real, *type_longreal;
extern Scope   *top_scope, *universe;

void      sym_init(void);
void      sym_open_scope(void);
void      sym_close_scope(void);
Symbol   *sym_new(const char *name, int kind);
void      sym_alloc_local(Symbol *sym);
void      sym_alloc_param(Symbol *sym);
uint16_t  sym_local_size(void);
Symbol   *sym_find_local(const char *name);
Symbol   *sym_find(const char *name);
Symbol   *sym_find_field(TypeDesc *rec, const char *name);
TypeDesc *type_new(int form, int32_t size);
TypeDesc *type_new_array(TypeDesc *elem, int32_t len);
TypeDesc *type_new_record(TypeDesc *base);
Symbol   *type_add_field(TypeDesc *rec, const char *name, TypeDesc *ftype);
TypeDesc *type_new_pointer(TypeDesc *base);
TypeDesc *type_new_proc(TypeDesc *ret);
Symbol   *type_add_param(TypeDesc *pt, const char *name,
                         TypeDesc *ptype, int is_var);
void      type_calc_arg_size(TypeDesc *pt);
int       type_assign_compat(TypeDesc *dst, TypeDesc *src);
int       type_param_compat(TypeDesc *formal, TypeDesc *actual, int is_var);
int       type_is_extension(TypeDesc *ext, TypeDesc *base);

#endif
