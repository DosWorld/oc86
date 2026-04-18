#ifndef COMPAT_H
#define COMPAT_H

/* Platform portability for DOS (Open Watcom) and Unix (gcc) builds.
   Include this before any path operations. */

/* ---- Path separator ---- */
#ifdef __WATCOMC__
#  define PATH_SEP     '\\'
#  define PATH_SEP_STR "\\"
#  define IS_SEP(c)    ((c) == '\\' || (c) == '/')
#else
#  define PATH_SEP     '/'
#  define PATH_SEP_STR "/"
#  define IS_SEP(c)    ((c) == '/')
#endif

/* ---- Environment variable list separator ---- */
#ifdef __WATCOMC__
#  define ENV_SEP ';'
#else
#  define ENV_SEP ':'
#endif

/* ---- Path utility declarations ---- */

/* Return pointer to the basename portion of path (after last separator).
   Returns path itself if no separator is found. */
char *path_basename(const char *path);

/* Join dir and file into dst (at most dstsz bytes including NUL).
   Inserts PATH_SEP between dir and file if dir does not already end with one. */
void path_join(char *dst, int dstsz, const char *dir, const char *file);

/* Normalize all separators in path to PATH_SEP in-place. */
void path_normalize(char *path);

/* Return 1 if the file at path exists and can be opened for reading, else 0. */
int file_exists(const char *path);

/* Copy src to dst (at most dstsz bytes including NUL), converting to upper case. */
void str_upcase(char *dst, int dstsz, const char *src);

#endif /* COMPAT_H */
