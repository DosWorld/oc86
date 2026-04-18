#ifndef DEF_H
#define DEF_H

#include "symbols.h"
#include <stdio.h>

/* .def file format (plain text):
 *
 *   MODULE <name>
 *   CONST  <name> <value>
 *   VAR    <name> <seg> <offset>
 *   TYPE   <name>
 *   PROC   <name> <seg> <offset>
 *
 * All names are the *prefixed* form (e.g. "Foo_Bar").
 * <seg> is 0=code, 1=data, 2=bss.
 * <offset> is decimal.
 *
 * Writer: def_write — called once per module compilation.
 * Reader: def_read  — parses a .def file and registers symbols
 *                     in the current scope as K_IMPORT entries.    */

/* Write all exported symbols from top_scope to 'f'.
   mod_name is the undecorated module name (e.g. "Foo"). */
void def_write(FILE *f, const char *mod_name);

/* Read a .def file and populate the import symbol table.
   alias is the local alias used in IMPORT (e.g. "F" for "IMPORT F := Foo").
   Returns 0 on success, -1 on parse error. */
int  def_read(FILE *f, const char *alias);

#endif
