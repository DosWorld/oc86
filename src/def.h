#ifndef DEF_H
#define DEF_H

#include "symbols.h"
#include <stdio.h>

/* .def file format (plain text):
 *
 *   MODULE <name>
 *   CONST  <fullname> <value>
 *   VAR    <fullname> <type> <seg> <offset>
 *            type: INTEGER|BOOLEAN|CHAR|BYTE|LONGINT|REAL|LONGREAL|SET|ADDRESS|
 *                  POINTER <base> | <RecordName>
 *            seg: 0=code, 1=data; offset: decimal (signed)
 *   TYPE   <fullname> [RECORD <size> … FIELD … BASE … END | POINTER <base>]
 *   PROC   <fullname> FAR|NEAR <rettype>
 *     PARAM [VAR] <name> <type>
 *     PARAM TYPELESSVAR <name>         ← typeless VAR param (no explicit type)
 *     END
 *   INLINE <fullname> <rettype>
 *     PARAM [VAR] <name> <type>
 *     PARAM TYPELESSVAR <name>
 *     BYTES <byte|Pn> ...
 *     END
 *
 * All names are the *prefixed* form (e.g. "Foo_Bar").
 * Reader backward-compatible: old VAR lines with no type token are detected
 * by checking if the character after the name starts with a digit.
 *
 * Writer: def_write — called once per module compilation.
 * Reader: def_read  — parses a .def file and registers symbols
 *                     in the current scope under "alias.shortname" keys. */

/* Write all exported symbols from top_scope to 'f'.
   mod_name is the undecorated module name (e.g. "Foo"). */
void def_write(FILE *f, const char *mod_name);

/* Read a .def file and populate the import symbol table.
   alias is the local alias used in IMPORT (e.g. "F" for "IMPORT F := Foo").
   Returns 0 on success, -1 on parse error. */
int  def_read(FILE *f, const char *alias);

#endif
