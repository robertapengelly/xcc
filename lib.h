/******************************************************************************
 * @file            lib.h
 *****************************************************************************/
#ifndef     _LIB_H
#define     _LIB_H

#if     defined (_WIN32)
# define    PATHSEP                     ";"
#else
# define    PATHSEP                     ":"
#endif

enum { CC_COMPILE = 0, CC_PREPROCESS };

char *skip_whitespace (char *p);
char *trim (char *p);
char *xstrdup (const char *p);
char *xstrndup (const char *p, unsigned long len);

void dynarray_add (void *ptab, unsigned long *nb_ptr, void *data);
void parse_args (int *pargc, char ***pargv, int optind);

void *xmalloc (unsigned long size);
void *xrealloc (void *ptr, unsigned long size);

#endif      /* _LIB_H */
