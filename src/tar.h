#ifndef TAR_H
#define TAR_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

/* Minimal ustar tar writer.
   tar_begin   — call before any files (noop, reserved for future use)
   tar_add_file — write one file: 512-byte header + data padded to 512
   tar_end      — write two 512-byte zero blocks (EOF marker)             */

void tar_begin(FILE *f);
void tar_add_file(FILE *f, const char *name,
                  const uint8_t *data, size_t len);
void tar_end(FILE *f);

/* tar_extract_file: search for a member named 'member_name' (basename match)
   inside the ustar tar archive at 'lib_path'.
   If found: write its contents to a temporary file and return the open FILE*.
   The caller must fclose() the returned FILE*.
   Returns NULL if not found or on error. */
FILE *tar_extract_file(const char *lib_path, const char *member_name);

#endif
