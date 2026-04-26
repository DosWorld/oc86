#ifndef PSTATE_H
#define PSTATE_H

/* Internal shared state between parser.c and pexpr.c.
   Not part of the public API — do not include from outside src/. */

#include "scanner.h"
#include "symbols.h"
#include "codegen.h"
#include <stdint.h>

/* Shared mutable state owned by parser.c */
extern Scanner *pe_sc;          /* current scanner */
extern int      pe_mod_uses_fpu;/* set when REAL/LONGREAL appears */
extern int      pe_bounds_check; /* $R+/$R- directive: 1=emit bounds checks, 0=off (default) */

/* Helpers defined in parser.c, used by pexpr.c */
void pe_error(const char *msg);
void pe_error2(const char *msg, const char *name);
void pe_expect(Token t);
int  pe_get_system_import(const char *name);
void pe_emit_push_static_link(int def_level);

/* Forward declaration of parse_type (defined in parser.c, called from pexpr.c) */
void pe_parse_type(TypeDesc **out);

/* Emit an inline procedure byte pattern with actual arg address substitution.
   Defined in parser.c; used by pexpr.c for inline calls in expression context. */
void pe_emit_inline_call(TypeDesc *pt);

/* Emit a call to an unresolved FORWARD-declared local procedure and record
   the patch site.  is_far=1 → PUSH CS + CALL NEAR (exported FAR convention).
   Defined in parser.c; used by pexpr.c for forward calls in expression context. */
void pe_fwd_emit_call(void *sym, int is_far);

#endif
