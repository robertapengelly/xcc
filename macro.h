/******************************************************************************
 * @file            macro.h
 *****************************************************************************/
#ifndef     _MACRO_H
#define     _MACRO_H

#include    <stdio.h>

enum macro_type {

    MACRO_COMMAND_LINE,
    MACRO_BUILTIN,
    MACRO_USER,
    
    MACRO_HIDDEN = 16

};

#include    "token.h"

struct macro_body {

    struct token **toks;
    unsigned long nb_toks;

};

struct macro_args {

    struct token **toks;
    unsigned long nb_toks;

};

struct macro {

    enum macro_type type;
    char *name;
    
    struct macro_args *args;
    struct macro_body *body;
    
    void (*handler) (FILE *ofp);

};

struct macro *find_macro (const char *name);
char *expand_macro (struct macro *me, char *start_p, char **pp);

int has_macro (const char *name);
void add_macro (struct macro *me);

void define_macro (char *p);
void undefine_macro (char *p);

void clear_macros (void);
void init_macros (void);

#endif      /* _MACRO_H */
