#ifndef PEXPR_H
#define PEXPR_H

#include "symbols.h"
#include "codegen.h"

/* Expression parser public interface.
   All expression-level parsing: designator, actual params,
   factor/term/simple-expr/expr, and SYSTEM intrinsics. */

void parse_designator(Item *item);
void parse_actual_params(TypeDesc *proc_type);
void parse_expr(Item *item);

#endif
