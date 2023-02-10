/******************************************************************************
 * @file            macro.h
 *****************************************************************************/
#include    <ctype.h>
#include    <stddef.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>
#include    <time.h>

#include    "hashtab.h"
#include    "lib.h"
#include    "macro.h"
#include    "report.h"
#include    "token.h"
#include    "vector.h"

struct replacement { char *orig, *new; };

static time_t now = 0;
static char *timep = 0;

static void handler_date (FILE *ofp) {

    if (!timep) {
        return;
    }
    
    fprintf (ofp, "\"%.3s %.2s %.4s\"", timep + 4, timep + 8, timep + 20);

}

static void handler_time (FILE *ofp) {

    if (!timep) {
        return;
    }
    
    fprintf (ofp, "\"%.8s\"", timep + 11);

}


extern const char *get_filename (void);
extern unsigned long get_line_number (void);

extern void set_filename_and_line_number (const char *filename, unsigned long line_number);

static void handler_file (FILE *ofp) {
    fprintf (ofp, "\"%s\"", get_filename ());
}

static void handler_line (FILE *ofp) {
    fprintf (ofp, "%lu", get_line_number ());
}


static char *expand_macro_internal (struct macro *me, char **pp) {

    struct vector vec_replacements = { 0 };
    struct token *tok;
    
    struct replacement *replacement;
    unsigned long i;
    
    struct macro_args *ma;
    struct macro_body *mb;
    
    char *body = 0, **args = 0;
    unsigned long nb_args = 0;
    
    if ((ma = me->args)) {
    
        char *start_p = (*pp = skip_whitespace (*pp));
        char *temp = 0;
        
        unsigned long j = 0;
        size_t len = 0;
        
        int nested = 0;
        
        if (!**pp || **pp != '(') {
            return me->name;
        }
        
        *pp = skip_whitespace (*pp + 1);
        
        while ((tok = get_token (start_p, pp))) {
        
            if (tok->kind == TOK_EOL) {
                break;
            }
            
            if (tok->kind == TOK_LBRACKET) {
                nested++;
            }
            
            if (tok->kind == TOK_RBRACKET) {
            
                if (nested == 0) {
                    break;
                }
                
                nested--;
            
            }
            
            dynarray_add (&args, &nb_args, xstrdup (tok->ident));
        
        }
        
        if (tok->kind == TOK_EOL) {
        
            report_at (get_filename (), get_line_number (), REPORT_ERROR, "expected ')' before end of line");
            return 0;
        
        }
        
        for (i = 0; i < ma->nb_toks; i++) {
        
            tok = ma->toks[i];
            
            if (tok->kind == TOK___VA_ARGS__) {
            
                for (; i < nb_args; i++) {
                
                    temp = xmalloc (1);
                    len = 0;
                    
                    temp[len] = '\0';
                    
                    for (; j < nb_args && *args[j] != ','; j++) {
                    
                        temp = xrealloc (temp, len + strlen (args[j]) + 1);
                        len += sprintf (temp + len, "%s", args[j]);
                    
                    }
                    
                    j++;
                    
                    if (temp) {
                    
                        temp = xrealloc (temp, len + 1);
                        temp[len] = '\0';
                    
                    }
                    
                    replacement = xmalloc (sizeof (*replacement));
                    
                    replacement->orig = "__VA_ARGS__";
                    replacement->new = temp;
                    
                    vec_push (&vec_replacements, replacement);
                
                }
            
            } else {
            
                temp = xmalloc (1);
                len = 0;
                
                temp[len] = '\0';
                
                for (; j < nb_args && *args[j] != ','; j++) {
                
                    temp = xrealloc (temp, len + strlen (args[j]) + 1);
                    len += sprintf (temp + len, "%s", args[j]);
                
                }
                
                j++;
                
                if (temp) {
                
                    temp = xrealloc (temp, len + 1);
                    temp[len] = '\0';
                
                }
                
                replacement = xmalloc (sizeof (*replacement));
                
                replacement->orig = xstrdup (tok->ident);
                replacement->new = temp;
                
                vec_push (&vec_replacements, replacement);
            
            }
        
        }
    
    }
    
    if ((mb = me->body)) {
    
        unsigned long cnt = vec_replacements.length;
        size_t len = 0;
        
        unsigned long i, j;
        
        for (i = 0; i < mb->nb_toks; i++) {
        
            tok = mb->toks[i];
            
            if (tok->kind == TOK_IDENT) {
            
                for (j = 0; j < cnt; j++) {
                
                    struct replacement *data = vec_replacements.data[j];
                    
                    if (strcmp (tok->ident, data->orig) == 0) {
                    
                        if (strcmp (tok->ident, "__VA_ARGS__") == 0) {
                        
                            body = xrealloc (body, len + strlen (data->new) + 1);
                            len += sprintf (body + len, "%s", data->new);
                            
                            if (j < cnt - 1) {
                            
                                body = xrealloc (body, len + 3);
                                len += sprintf (body + len, ", ");
                            
                            }
                            
                            continue;
                        
                        }
                        
                        tok = xmalloc (sizeof (*tok));
                        
                        tok->ident = data->new;
                        tok->len = strlen (tok->ident);
                        
                        goto got_tok;
                    
                    }
                
                }
                
                if (strcmp (tok->ident, "__VA_ARGS__") == 0) {
                    continue;
                }
                
            }
            
        got_tok:
            
            body = xrealloc (body, len + tok->len + 1);
            len += sprintf (body + len, "%s", tok->ident);
            
            if (tok->len == 1 && *tok->ident == ',') {
            
                body = xrealloc (body, len + 2);
                body[len++] = ' ';
            
            }
        
        }
        
        if (body) {
        
            body = xrealloc (body, len + 1);
            body[len] = '\0';
        
        }
    
    }
    
    memset (&vec_replacements, 0, sizeof (vec_replacements));
    return body;

}


static struct macro builtin_macros[] = {

    {   MACRO_BUILTIN,      "__DATE__",     0,      0,      handler_date    },
    {   MACRO_BUILTIN,      "__TIME__",     0,      0,      handler_time    },
    {   MACRO_BUILTIN,      "__FILE__",     0,      0,      handler_file    },
    {   MACRO_BUILTIN,      "__LINE__",     0,      0,      handler_line    },
    
    {   0,                  0,              0,      0,      0               }

};

static struct hashtab hashtab_macros = { 0 };

struct macro *find_macro (const char *name) {

    struct hashtab_name *key;
    struct macro *me;
    
    if (!name) {
        return 0;
    }
    
    if ((key = hashtab_alloc_name (name)) == NULL) {
        return 0;
    }
    
    me = hashtab_get (&hashtab_macros, key);
    free (key);
    
    return me;

}

char *expand_macro (struct macro *me, char *start_p, char **pp) {

    char *temp, *temp2, *body = 0;
    struct token *tok;
    
    size_t len = 0;
    
    if ((temp = expand_macro_internal (me, pp))) {
    
        char *start;
        int nested = 0;
        
        start_p = temp;
        
        while ((tok = get_token (start_p, &temp))) {
        
            if (tok->kind == TOK_EOL || tok->kind == TOK_NEWLINE) {
                break;
            }
            
            if (tok->kind == TOK_CONCAT) {
            
                if (body[len - 1] == ' ') {
                    body[--len] = '\0';
                }
                
                while (*temp == ' ') {
                    temp++;
                }
                
                continue;
            
            }
            
            if (tok->kind == TOK_IDENT) {
            
                char *temp3 = tok->ident;
                
                while (*temp3 == '*') {
                    temp3 = skip_whitespace (temp3 + 1);
                }
                
                if ((me = find_macro (temp3))) {
                
                    int n = (temp3 - tok->ident);
                    
                    body = xrealloc (body, len + n + 1);
                    len += sprintf (body + len, "%.*s", n, tok->ident);
                    
                    if (!me->args) {
                    
                        temp2 = xstrdup (tok->ident);
                        
                        if ((temp2 = expand_macro (me, temp2, 0))) {
                        
                            body = xrealloc (body, len + strlen (temp2) + 1);
                            len += sprintf (body + len, "%s", temp2);
                        
                        }
                        
                        continue;
                    
                    }
                    
                    temp = skip_whitespace (temp);
                    
                    if (*temp == '(') {
                    
                        start = temp++;
                        
                        while (*temp) {
                        
                            if (!*temp || *temp == '\n') {
                                break;
                            }
                            
                            if (*temp == '(') {
                                nested++;
                            }
                            
                            if (*temp == ')') {
                            
                                if (nested == 0) {
                                
                                    temp++;
                                    break;
                                
                                }
                            
                            }
                            
                            temp++;
                        
                        }
                        
                        temp2 = xstrndup (start, temp - start);
                        
                        if ((temp2 = expand_macro (me, temp2, &temp2))) {
                        
                            body = xrealloc (body, len + strlen (temp2) + 1);
                            len += sprintf (body + len, "%s", temp2);
                        
                        }
                        
                        continue;
                    
                    }
                    
                    body = xrealloc (body, len + strlen (me->name) + 1);
                    len += sprintf (body + len, "%s ", me->name);
                    
                    continue;
                
                }
            
            }
            
            body = xrealloc (body, len + tok->len + 1);
            len += sprintf (body + len, "%s", tok->ident);
        
        }
    
    }
    
    if (body) {
    
        body = xrealloc (body, len + 1);
        body[len] = '\0';
    
    }
    
    return body;

}

int has_macro (const char *name) {

    struct macro *me = find_macro (name);
    
    if (me == NULL) {
        return 0;
    }
    
    return !(me->type & MACRO_HIDDEN);

}

void add_macro (struct macro *me) {

    struct hashtab_name *key;
    
    if ((key = hashtab_alloc_name (me->name)) == NULL) {
        return;
    }
    
    hashtab_put (&hashtab_macros, key, me);

}

void clear_macros (void) {

    struct hashtab_entry *entry;
    struct hashtab_name *key;
    
    struct macro *me;
    unsigned long i;
    
    for (i = 0; i < hashtab_macros.capacity; i++) {
    
        if ((entry = &hashtab_macros.entries[i])) {
        
            if ((key = entry->key)) {
            
                if ((me = entry->value) && (me->type & ~MACRO_HIDDEN) == MACRO_USER) {
                
                    memset (key, 0, sizeof (*key));
                    free (key);
                    
                    memset (entry, 0, sizeof (*entry));
                
                }
            
            }
        
        }
    
    }
    
    /*memset (&hashtab_macros, 0, sizeof (hashtab_macros));*/

}

void define_macro (char *p) {

    char *p1 = (p = skip_whitespace (p));
    
    struct macro_args *ma = 0;
    struct macro_body *mb = 0;
    
    struct token *tok1, *tok2 = 0;
    struct macro *me;
    
    set_filename_and_line_number ("<command-line>", 1);
    
    if (!(tok1 = get_token (p, &p1))) {
        return;
    }
    
    if (tok1->kind == TOK_EOL) {
        return;
    }
    
    if (tok1->kind != TOK_IDENT) {
    
        report_line_at ("<command-line>", 1, REPORT_ERROR, p, p, "macro name must be an identifier");
        return;
    
    }
    
    if (strcmp (tok1->ident, "defined") == 0) {
    
        report_line_at ("<command-line>", 1, REPORT_ERROR, p, p, "\"%s\" cannot be used as a macro name", tok1->ident);
        return;
    
    }
    
    if ((me = find_macro (tok1->ident)) == NULL) {
    
        me = xmalloc (sizeof (*me));
        me->name = xstrdup (tok1->ident);
        
        add_macro (me);
    
    } else if ((me->type & ~MACRO_HIDDEN) == MACRO_BUILTIN) {
    
        report_line_at ("<command-line>", 1, REPORT_ERROR, p, p, "trying to redefine macro");
        return;
    
    }
    
    me->type = MACRO_COMMAND_LINE;
    p1 = skip_whitespace (p1);
    
    if (*p1 == '(') {
    
        int va_args = 0;
        
        ma = xmalloc (sizeof (*ma));
        p1 = skip_whitespace (p1 + 1);
        
        while ((tok1 = get_token (p, &p1))) {
        
            if (tok1->kind == TOK_EOL) {
                break;
            }
            
            if (tok2) {
            
                free (tok2);
                tok2 = 0;
            
            }
            
            tok2 = copy_token (tok1);
            
            if (tok1->kind == TOK_RBRACKET) {
                break;
            }
            
            if (tok1->kind == TOK_COMMA) {
                continue;
            }
            
            if (tok1->kind == TOK_WHITESPACE) {
                continue;
            }
            
            if (va_args) {
            
                report_at ("<command-line>", 1, REPORT_ERROR, "expected ')' after \"...\"");
                return;
            
            }
            
            if (tok1->kind != TOK___VA_ARGS__ && tok1->kind != TOK_IDENT) {
            
                report_at ("<command-line>", 1, REPORT_ERROR, "invalid argument type passed to #define");
                return;
            
            }
            
            dynarray_add (&ma->toks, &ma->nb_toks, copy_token (tok1));
            
            if (tok1->kind == TOK___VA_ARGS__) {
                va_args = 1;
            }
        
        }
        
        if (tok1->kind == TOK_EOL) {
        
            if (tok2) {
            
                if (tok2->kind == TOK_COMMA) {
                
                    report_at ("<command-line>", 1, REPORT_ERROR, "expected parameter name before end of line");
                    
                    free (tok2);
                    return;
                
                }
                
                free (tok2);
            
            }
            
            report_at ("<command-line>", 1, REPORT_ERROR, "expected ')' before end of line");
            return;
        
        }
    
    }
    
    mb = xmalloc (sizeof (*mb));
    p1 = skip_whitespace (p1);
    
    if (*p1 != '=') {
    
        struct token *tok = xmalloc (sizeof (*tok));
        
        tok->ident = "1";
        tok->len = 1;
        
        tok->kind = TOK_PP_NUM;
        
        dynarray_add (&mb->toks, &mb->nb_toks, tok);
        goto out;
    
    }
    
    p1 = skip_whitespace (p1 + 1);
    
    if (!*p1) {
    
        struct token *tok = xmalloc (sizeof (*tok));
        tok->kind = TOK_IDENT;
        
        tok->ident = "";
        tok->len = 0;
        
        dynarray_add (&mb->toks, &mb->nb_toks, tok);
        goto out;
    
    }
    
    while ((tok1 = get_token (p, &p1))) {
    
        if (tok1->kind == TOK_EOL || tok1->kind == TOK_NEWLINE) {
            break;
        }
        
        if (tok1->kind == TOK_WHITESPACE) {
        
            if (*(p1 - 2) == ',' || *p1 == ',') {
                continue;
            }
        
        }
        
        dynarray_add (&mb->toks, &mb->nb_toks, copy_token (tok1));
    
    }
    
out:
    
    if (ma) { me->args = ma; }
    if (mb) { me->body = mb; }

}

void undefine_macro (char *p) {

    char *p1 = (p = skip_whitespace (p));
    
    struct macro *me;
    struct token *tok;
    
    set_filename_and_line_number ("<command-line>", 1);
    
    if (!(tok = get_token (p, &p1))) {
        return;
    }
    
    if (tok->kind == TOK_EOL) {
    
        report_at ("<command-line>", 1, REPORT_ERROR, "failed to get token");
        return;
    
    }
    
    if (tok->kind != TOK_IDENT) {
    
        report_line_at ("<command-line>", 1, REPORT_ERROR, p, p, "macro name must be an identifier");
        return;
    
    }
    
    if ((me = find_macro (tok->ident)) != NULL) {
    
        if ((me->type & ~MACRO_HIDDEN) == MACRO_BUILTIN) {
        
            report_line_at ("<command-line>", 1, REPORT_ERROR, p, p, "trying to undefine builtin macro");
            return;
        
        }
        
        me->type |= MACRO_HIDDEN;
    
    }
    
    if (*p1 != '\0') {
    
        p1 = skip_whitespace (p1);
        
        if (*p1 != '\0') {
        
            report_line_at ("<command-line>", 1, REPORT_WARNING, p, p1, "extra tokens at end of #undef directive");
            return;
        
        }
    
    }

}

void init_macros (void) {

    struct hashtab_name *key;
    struct macro *me;
    
    if (!now || !timep) {
    
        time (&now);
        timep = ctime (&now);
    
    }
    
    for (me = builtin_macros; me->name; me++) {
    
        if ((key = hashtab_alloc_name (me->name)) == NULL) {
            continue;
        }
        
        hashtab_put (&hashtab_macros, key, me);
    
    }

}
