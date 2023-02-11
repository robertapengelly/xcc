/******************************************************************************
 * @file            token.c
 *****************************************************************************/
#include    <ctype.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>

#include    "hashtab.h"
#include    "lib.h"
#include    "report.h"
#include    "token.h"

struct punct { const char *name; enum token_kind kind; };
static struct hashtab hashtab_punct = { 0 };

static const char *tok_filename = 0;
static unsigned long tok_lineno = 0;

void get_filename_and_line_number (const char **filename_p, unsigned long *line_number_p) {

    *filename_p = tok_filename;
    *line_number_p = tok_lineno;

}

const char *get_filename (void) {
    return tok_filename;
}

unsigned long get_line_number (void) {
    return tok_lineno;
}

void set_filename_and_line_number (const char *filename, unsigned long line_number) {

    tok_filename = filename;
    tok_lineno = line_number;

}


static int is_alpha (int c) {

    if (c >= 'A' && c <= 'Z') {
        return 1;
    }
    
    if (c >= 'a' && c <= 'z') {
        return 1;
    }
    
    return (c == '_');

}

static int is_alnum (int c) {

    if (is_alpha (c)) {
        return 1;
    }
    
    return (c >= '0' && c <= '9');

}

static int is_punct (char *p) {

    static char *kws[] = {
    
        "<<=",  ">>=",  "...",  "==",   "!=",   "<=",
        ">=",   "->",   "+=",   "-=",   "*=",   "/=",
        "++",   "--",   "%=",   "&=",   "|=",   "^=",
        "&&",   "||",   "<<",   ">>",   "##"
    
    };
    
#define     ARRAY_SIZE(arr)             (sizeof ((arr)) / sizeof ((arr)[0]))
    
    size_t i;
    
    for (i = 0; i < ARRAY_SIZE (kws); i++) {
    
        if (strncmp (p, kws[i], strlen (kws[i])) == 0) {
            return strlen (kws[i]);
        }
    
    }
    
#undef      ARRAY_SIZE
    
    return (ispunct ((int) *p) ? 1 : 0);

}


static enum token_kind token_from_punct (const char *name) {

    struct hashtab_name *key;
    struct punct *pe;
    
    if ((key = hashtab_alloc_name (name)) == NULL) {
        return TOK_PUNCT;
    }
    
    pe = hashtab_get (&hashtab_punct, key);
    free (key);
    
    if (pe == NULL) {
        return TOK_PUNCT;
    }
    
    return pe->kind;

}

static void ignore_rest_of_line (char **pp) {

    while (**pp/* && **pp != '\n'*/) {
        (*pp)++;
    }

}

static char *read_quotes (char ch, char *start_p, char **pp) {

    char *start = (*pp)++;
    
    while (**pp && **pp != '\n' && **pp != ch) {
        (*pp)++;
    }
    
    if (**pp != ch) {
    
        char *result = xmalloc (*pp - start + 1);
        sprintf (result, "%.*s%c", (int) (*pp - start), start, ch);
        
        report_line_at (tok_filename, tok_lineno, REPORT_WARNING, start_p, start, "missing terminating %c character", ch);
        return result;
    
    }
    
    (*pp)++;
    return xstrndup (start, (*pp) - start);

}


struct token *copy_token (struct token *tok) {

    struct token *t = xmalloc (sizeof (*t));
    memcpy (t, tok, sizeof (*t));
    
    return t;

}

struct token *get_token (char *start_p, char **pp) {

    static struct token tok = { 0 };
    int ch, len;
    
    memset (&tok, 0, sizeof (tok));
    
    if ((*pp)[0] == '/' && (*pp)[1] == '/') {
    
        report_line_at (tok_filename, tok_lineno, REPORT_ERROR, start_p, *pp, "C++ style comments are not allowed in ISO C90");
        ignore_rest_of_line (pp);
        
        tok.ident = "\0";
        tok.len = 1;
        
        return &tok;
    
    }
    
    if (**pp == '\n') {
    
        (*pp)++;
        
        tok.ident = "\n";
        tok.len = 1;
        
        tok.kind = TOK_NEWLINE;
        return &tok;
    
    }
    
    if (isspace ((int) **pp)) {
    
        tok.ident = " ";
        tok.len = 1;
        
        *pp = skip_whitespace (*pp);
        
        tok.kind = TOK_WHITESPACE;
        return &tok;
    
    }
    
    if (isdigit ((int) **pp) || ((*pp)[0] == '.' && isdigit ((int) (*pp)[1]))) {
    
        char *start = (*pp)++;
        
        for (;;) {
        
            if ((*pp)[0] && (*pp)[1] && strchr ("eEpP", (*pp)[0]) && strchr ("+-", (*pp)[1])) {
                (*pp) += 2;
            } else if (isdigit ((int) **pp) || isxdigit ((int) **pp) || **pp == 'x' || **pp == '.') {
                (*pp)++;
            } else {
                break;
            }
        
        }
        
        if (**pp == 'L') {
            (*pp)++;
        }
        
        tok.ident = xstrndup (start, *pp - start);
        tok.len = ((*pp) - start);
        
        tok.kind = TOK_PP_NUM;
        return &tok;
    
    }
    
    if (strlen (*pp) >= 3 && (*pp)[0] == '.' && (*pp)[1] == '.' && (*pp)[2] == '.') {
    
        *pp += 3;
        
        tok.ident = "...";
        tok.len = 3;
        
        tok.kind = TOK___VA_ARGS__;
        return &tok;
    
    }
    
    if ((ch = **pp) == '"') {
    
        char *start = *pp;
        
        tok.ident = read_quotes (ch, start_p, pp);
        tok.len = ((*pp) - start);
        
        tok.kind = TOK_STRING_LITERAL;
        return &tok;
    
    }
    
    if ((ch = **pp) == '\'') {
    
        char *start = *pp;
        
        tok.ident = read_quotes (ch, start_p, pp);
        tok.len = ((*pp) - start);
        
        tok.kind = TOK_CHAR_LITERAL;
        return &tok;
    
    }
    
    if (is_alpha ((int) **pp) || **pp == '_' || (**pp & 0x80)) {
    
        char *start = (*pp)++;
        
        while (is_alnum ((int) **pp) || **pp == '_' || (**pp & 0x80)) {
            (*pp)++;
        }
        
        tok.ident = xstrndup (start, (*pp) - start);
        tok.len = ((*pp) - start);
        
        tok.kind = TOK_IDENT;
        return &tok;
    
    }
    
    if (**pp == '*') {
    
        char *start = (*pp)++, *temp = skip_whitespace (*pp);
        
        while (*temp == '*') {
            temp = skip_whitespace (temp + 1);
        }
        
        if (is_alpha ((int) **pp) || **pp == '_' || (**pp & 0x80)) {
        
            *pp = temp;
            
            while (is_alnum ((int) **pp) || **pp == '_' || (**pp & 0x80)) {
                (*pp)++;
            }
            
            tok.ident = xstrndup (start, *pp - start);
            tok.len = (*pp - start);
            
            tok.kind = TOK_IDENT;
            return &tok;
        
        }
        
        (*pp)--;
    
    }
    
    if ((len = is_punct (*pp)) > 0) {
    
        tok.ident = xstrndup (*pp, len);
        tok.len = len;
        
        (*pp) += len;
        
        tok.kind = token_from_punct (tok.ident);
        return &tok;
    
    }
    
    tok.ident = "\0";
    tok.len = 1;
    
    return &tok;

}


static struct punct punct_table[] = {

    {   "==",       TOK_EQEQ        },
    {   "!=",       TOK_NOTEQ       },
    {   "<=",       TOK_LTEQ        },
    {   ">=",       TOK_GTEQ        },
    {   "&&",       TOK_ANDAND      },
    {   "||",       TOK_OROR        },
    
    {   "*",        TOK_STAR        },
    {   "/",        TOK_DIV         },
    {   "&",        TOK_AND         },
    {   "%",        TOK_MOD         },
    {   "+",        TOK_ADD         },
    {   "-",        TOK_SUB         },
    {   "<",        TOK_LT          },
    {   ">",        TOK_GT          },
    {   "&",        TOK_AND         },
    {   "^",        TOK_XOR         },
    {   "|",        TOK_OR          },
    {   "?",        TOK_QUESTION    },
    {   ":",        TOK_COLON       },
    
    {   "!",        TOK_NOT         },
    {   "#",        TOK_HASH        },
    {   ";",        TOK_SEMI        },
    {   ",",        TOK_COMMA       },
    {   "{",        TOK_LBRACE      },
    {   "}",        TOK_RBRACE      },
    {   "(",        TOK_LBRACKET    },
    {   ")",        TOK_RBRACKET    },
    
    {   "##",       TOK_CONCAT      },
    {   0,          0               }

};

void init_punct (void) {

    struct hashtab_name *key;
    struct punct *pe;
    
    for (pe = punct_table; pe->name; pe++) {
    
        if ((key = hashtab_alloc_name (pe->name)) == NULL) {
            continue;
        }
        
        hashtab_put (&hashtab_punct, key, pe);
    
    }

}
