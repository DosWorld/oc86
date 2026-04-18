#ifndef IMPORT_H
#define IMPORT_H

#include <stdio.h>

/* Search for <mod_name>.def (or extract from <mod_name>.om) in the current
   directory and $OBERON_LIB paths.  Returns an open FILE* or NULL. */
FILE *find_import_file(const char *filename, char *found_path, int path_sz);

/* Load <mod_name>.def for an explicit IMPORT entry.
   Registers symbols under 'alias'.  Calls exit(1) if the file is not found. */
void load_explicit_import(const char *mod_name, const char *alias);

/* Load <mod_name>.def if it can be found; silent no-op otherwise. */
void add_implicit_import(const char *mod_name);

#endif
