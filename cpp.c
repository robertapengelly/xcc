/******************************************************************************
 * @file            cpp.c
 *****************************************************************************/
#include    <ctype.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>
#include    <time.h>

#include    "cc.h"
#include    "cpp.h"
#include    "eval.h"
#include    "hashtab.h"
#include    "lib.h"
#include    "ll.h"
#include    "macro.h"
#include    "report.h"
#include    "token.h"
#include    "vector.h"

static const char *root_filename = 0;


extern const char *get_filename (void);
extern unsigned long get_line_number (void);

extern void get_filename_and_line_number (const char **filename_p, unsigned long *line_number_p);
extern void set_filename_and_line_number (const char *filename, unsigned long line_number);


struct cond_special {

    const char *name;
    int (*handler) (char **pp);

};

struct special {

    const char *name;
    void (*handler) (char **pp);

};


static struct hashtab hashtab_cond_special = { 0 };
static struct hashtab hashtab_special = { 0 };

static struct vector internal_lines = { 0 };
static struct vector open_files = { 0 };

static char *start_p = 0;


static void ignore_rest_of_line (char **pp) {

    while (**pp && **pp != '\n') {
        (*pp)++;
    }

}


struct cond_statement {

    char *str;
    long val;
    
    const char *filename;
    unsigned long lineno;

};

static struct vector cond_stack = { 0 };
static int enable = 1, satisfy = 0;

#define     CF_ENABLE                   (1 << 0)
#define     CF_SATISFY_SHIFT            (1 << 0)

#define     CF_SATISFY_MASK             (3 << CF_SATISFY_SHIFT)

static long cond_value (int enable, int satisfy) {
  return (enable ? CF_ENABLE : 0) | (satisfy << CF_SATISFY_SHIFT);
}

static int handler_if (char **pp) {

    struct cond_statement *statement = xmalloc (sizeof (*statement));
    *pp = skip_whitespace (*pp);
    
    if (!**pp || **pp == '\n') {
    
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, *pp, "#if with no expression");
        
        ignore_rest_of_line (pp);
        return enable;
    
    }
    
    statement->str = "#if";
    statement->val = cond_value (enable, satisfy);
    
    statement->filename = get_filename ();
    statement->lineno = get_line_number ();
    
    vec_push (&cond_stack, (void *) statement);
    
    satisfy = eval (start_p, pp);
    enable = enable && satisfy == 1;
    
    return enable;

}

static int handler_ifdef (char **pp) {

    struct cond_statement *statement;
    struct token *tok;
    
    *pp = skip_whitespace (*pp);
    
    if (!**pp || **pp == '\n') {
    
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, *pp, "#ifdef with no expression");
        
        ignore_rest_of_line (pp);
        return enable;
    
    }
    
    tok = get_token (start_p, pp);
    
    if (tok->kind != TOK_IDENT) {
    
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, *pp - tok->len, "macro names must be identifiers");
        
        ignore_rest_of_line (pp);
        return enable;
    
    }
    
    statement = xmalloc (sizeof (*statement));
    
    statement->str = "#ifdef";
    statement->val = cond_value (enable, satisfy);
    
    statement->filename = get_filename ();
    statement->lineno = get_line_number ();
    
    vec_push (&cond_stack, (void *) statement);
    
    satisfy = has_macro (tok->ident);
    enable = enable && satisfy == 1;
    
    *pp = skip_whitespace (*pp);
    
    if (**pp && **pp != '\n') {
    
        report_line_at (get_filename (), get_line_number (), REPORT_WARNING, start_p, *pp, "extra tokens at end of #ifdef directive");
        ignore_rest_of_line (pp);
    
    }
    
    return enable;

}

static int handler_ifndef (char **pp) {

    struct cond_statement *statement;
    struct token *tok;
    
    *pp = skip_whitespace (*pp);
    
    if (!**pp || **pp == '\n') {
    
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, *pp, "#ifndef with no expression");
        
        ignore_rest_of_line (pp);
        return enable;
    
    }
    
    tok = get_token (start_p, pp);
    
    if (tok->kind != TOK_IDENT) {
    
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, *pp - tok->len, "macro names must be identifiers");
        
        ignore_rest_of_line (pp);
        return enable;
    
    }
    
    statement = xmalloc (sizeof (*statement));
    
    statement->str = "#ifndef";
    statement->val = cond_value (enable, satisfy);
    
    statement->filename = get_filename ();
    statement->lineno = get_line_number ();
    
    vec_push (&cond_stack, (void *) statement);
    
    satisfy = !has_macro (tok->ident);
    enable = enable && satisfy == 1;
    
    *pp = skip_whitespace (*pp);
    
    if (**pp && **pp != '\n') {
    
        report_line_at (get_filename (), get_line_number (), REPORT_WARNING, start_p, *pp, "extra tokens at end of #ifndef directive");
        ignore_rest_of_line (pp);
    
    }
    
    return enable;

}

static int handler_elif (char **pp) {

    long last = cond_stack.length - 1, flag;
    
    struct cond_statement *statement;
    int cond = 0;
    
    *pp = skip_whitespace (*pp);
    
    if (!**pp || **pp == '\n') {
    
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, *pp, "#elif with no expression");
        
        ignore_rest_of_line (pp);
        return enable;
    
    }
    
    if (last < 0) {
    
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, skip_whitespace (start_p + 1), "#elif without #if");
        
        ignore_rest_of_line (pp);
        return enable;
    
    }
    
    statement = cond_stack.data[last];
    flag = statement->val;
    
    if (satisfy == 2) {
    
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, skip_whitespace (start_p + 1), "#elif after #else");
        
        ignore_rest_of_line (pp);
        return enable;
    
    }
    
    if (satisfy == 0) {
    
        cond = eval (start_p, pp);
        
        if (cond) {
            satisfy = 1;
        }
    
    } else {
        ignore_rest_of_line (pp);
    }
    
    statement->str = "#elif";
    statement->lineno = get_line_number ();
    
    enable = !enable && cond && ((flag & CF_ENABLE) != 0);
    return enable;

}

static int handler_else (char **pp) {

    long last = cond_stack.length - 1, flag;
    struct cond_statement *statement;
    
    if (last < 0) {
    
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, skip_whitespace (start_p + 1), "#else without #if");
        
        ignore_rest_of_line (pp);
        return enable;
    
    }
    
    statement = cond_stack.data[last];
    flag = statement->val;
    
    if (satisfy == 2) {
    
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, skip_whitespace (start_p + 1), "#else after #else");
        
        ignore_rest_of_line (pp);
        return enable;
    
    }
    
    statement->str = "#else";
    statement->lineno = get_line_number ();
    
    enable = !enable && satisfy == 0 && ((flag & CF_ENABLE) != 0);
    satisfy = 2;
    
    return enable;

}

static int handler_endif (char **pp) {

    long len = cond_stack.length, flag;
    struct cond_statement *statement;
    
    if (len <= 0) {
    
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, skip_whitespace (start_p + 1), "#endif used without #if");
        
        ignore_rest_of_line (pp);
        return enable;
    
    }
    
    statement = cond_stack.data[--len];
    flag = statement->val;
    
    enable = (flag & CF_ENABLE) != 0;
    satisfy = (flag & CF_SATISFY_MASK) >> CF_SATISFY_SHIFT;
    
    cond_stack.length = len;
    return enable;

}


static void handler_define (char **pp) {

    struct macro_args *ma = 0;
    struct macro_body *mb = 0;
    
    struct token *tok1, *tok2 = 0;
    struct macro *me;
    
    *pp = skip_whitespace (*pp);
    
    if (!(tok1 = get_token (start_p, pp))) {
        return;
    }
    
    if (tok1->kind != TOK_IDENT) {
    
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, *pp - tok1->len, "macro names must be identifiers");
        ignore_rest_of_line (pp);
        
        return;
    
    }
    
    if (strcmp (tok1->ident, "defined") == 0) {
    
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, *pp - tok1->len, "\"%s\" cannot be used as a macro name", tok1->ident);
        return;
    
    }
    
    if ((me = find_macro (tok1->ident)) == NULL) {
    
        me = xmalloc (sizeof (*me));
        me->name = xstrdup (tok1->ident);
        
        add_macro (me);
    
    } else if (!(me->type & MACRO_HIDDEN)) {
    
        int type = (me->type & ~MACRO_HIDDEN);
        
        if (type == MACRO_COMMAND_LINE || type == MACRO_BUILTIN) {
        
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, *pp - tok1->len, "\"%s\" redefined", me->name);
            ignore_rest_of_line (pp);
            
            return;
        
        }
    
    }
    
    me->type = MACRO_USER;
    
    if (**pp == '(') {
    
        int va_args = 0;
        
        ma = xmalloc (sizeof (*ma));
        *pp = skip_whitespace (*pp + 1);
        
        while ((tok1 = get_token (start_p, pp))) {
        
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
            
                report_at (get_filename (), get_line_number (), REPORT_ERROR, "expected ')' after \"...\"");
                return;
            
            }
            
            if (tok1->kind != TOK___VA_ARGS__ && tok1->kind != TOK_IDENT) {
            
                report_at (get_filename (), get_line_number (), REPORT_ERROR, "invalid argument type passed to #define");
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
                
                    report_at (get_filename (), get_line_number (), REPORT_ERROR, "expected parameter name before end of line");
                    
                    free (tok2);
                    return;
                
                }
                
                free (tok2);
            
            }
            
            report_at (get_filename (), get_line_number (), REPORT_ERROR, "expected ')' before end of line");
            return;
        
        }
    
    }
    
    mb = xmalloc (sizeof (*mb));
    *pp = skip_whitespace (*pp);
    
    if (!**pp || **pp == '\n') {
    
        struct token *tok = xmalloc (sizeof (*tok));
        tok->kind = TOK_IDENT;
        
        tok->ident = "";
        tok->len = 0;
        
        dynarray_add (&mb->toks, &mb->nb_toks, tok);
        goto out;
    
    }
    
    while ((tok1 = get_token (start_p, pp))) {
    
        if (tok1->kind == TOK_EOL || tok1->kind == TOK_NEWLINE) {
            break;
        }
        
        if (tok1->kind == TOK_WHITESPACE) {
        
            if (*(*pp - 2) == ',' || **pp == ',') {
                continue;
            }
        
        }
        
        dynarray_add (&mb->toks, &mb->nb_toks, copy_token (tok1));
    
    }
    
out:
    
    if (ma) { me->args = ma; }
    if (mb) { me->body = mb; }

}

static void handler_undef (char **pp) {

    struct macro *me;
    struct token *tok;
    
    *pp = skip_whitespace (*pp);
    
    if (!(tok = get_token (start_p, pp))) {
    
        ignore_rest_of_line (pp);
        return;
    
    }
    
    if (tok->kind != TOK_IDENT) {
    
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, *pp - tok->len, "macro name must be an identifier");
        
        ignore_rest_of_line (pp);
        return;
    
    }
    
    if ((me = find_macro (tok->ident)) != NULL) {
    
        if ((me->type & ~MACRO_HIDDEN) == MACRO_BUILTIN) {
        
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, *pp - tok->len, "trying to undefine builtin macro");
            
            ignore_rest_of_line (pp);
            return;
        
        }
        
        me->type |= MACRO_HIDDEN;
    
    }
    
    *pp = skip_whitespace (*pp);
    
    if (**pp && **pp != '\n') {
    
        report_line_at (get_filename (), get_line_number (), REPORT_WARNING, start_p, *pp, "extra tokens at end of #undef directive");
        ignore_rest_of_line (pp);
    
    }

}

static void handler_include (char **pp) {

    char *temp, end_ch, saved_ch, *buf;
    int local_first = 1;
    
    const char *filename;
    unsigned long line_number;
    
    struct vector old_cond_stack = { 0 };
    int old_enable, old_satisfy;
    
    int result = -2;
    unsigned long i;
    
    *pp = skip_whitespace (*pp);
    
    if (**pp != '"' && **pp != '<') {
    
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, *pp, "#include expects \"FILENAME\" or <FILENAME>");
        
        ignore_rest_of_line (pp);
        return;
    
    }
    
    if (**pp == '"') {
        end_ch = '"';
    } else {
    
        local_first = 0;
        end_ch = '>';
    
    }
    
    temp = ++(*pp);
    
    while (**pp && **pp != end_ch) {
        (*pp)++;
    }
    
    if (**pp != end_ch) {
    
        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, temp, "missing terminating %c character", end_ch);
        return;
    
    }
    
    saved_ch = **pp;
    **pp = '\0';
    
    memcpy (&old_cond_stack, &cond_stack, sizeof (cond_stack));
    
    old_enable = enable;
    old_satisfy = satisfy;
    
    get_filename_and_line_number (&filename, &line_number);
    
    if (local_first) {
    
        char *p;
        
        if (!(result = preprocess (temp))) {
            goto success;
        }
        
        if ((p = strrchr (root_filename, '/'))) {
        
            buf = xmalloc ((p - filename) + 1 + strlen (temp) + 1);
            sprintf (buf, "%.*s/%s", (int) (p - filename), filename, temp);
            
            if (!(result = preprocess (buf))) {
            
                free (buf);
                goto success;
            
            }
            
            free (buf);
        
        }
    
    }
    
    for (i = 0; i < state->nb_inc_paths; i++) {
    
        char *path = state->inc_paths[i];
        
        if (strncmp (path, "./", 2) == 0 || strncmp (path, "../", 3) == 0) {
        
            char *p = xstrdup (root_filename), *end;
            
            if ((end = strrchr (p, '/'))) {
            
                *end = '\0';
                
                for (;;) {
                
                    if (strncmp (path, "./", 2) == 0) {
                    
                        path += 2;
                        continue;
                    
                    }
                    
                    if (strncmp (path, "../", 3) == 0) {
                    
                        if ((end = strrchr (p, '/'))) {
                        
                            path += 3;
                            *end = '\0';
                        
                        }
                        
                        break;
                    
                    }
                    
                    break;
                
                }
                
                buf = xmalloc(strlen (p) + 1 + strlen (path) + strlen (temp) + 1);
                sprintf (buf, "%s/%s%s", p, path, temp);
                
                free (p);
                goto got_filename;
            
            }
        
        }
        
        buf = xmalloc (strlen (path) + strlen (temp) + 1);
        sprintf (buf, "%s%s", path, temp);
        
    got_filename:
        
        if (!(result = preprocess (buf))) {
        
            free (buf);
            goto success;
        
        }
        
        free (buf);
    
    }
    
    if (result == -2) {
    
        char *fn = xstrdup (temp);
        **pp = saved_ch;
        
        saved_ch = *(++(*pp));
        **pp = '\0';
        
        report_line_at (filename, line_number, REPORT_FATAL_ERROR, start_p, temp, "%s not found", fn);
    
    }
    
success:
    
    **pp = saved_ch;
    *pp = skip_whitespace (*pp + 1);
    
    memcpy (&cond_stack, &old_cond_stack, sizeof (cond_stack));
    
    enable = old_enable;
    satisfy = old_satisfy;
    
    set_filename_and_line_number (filename, line_number);
    fprintf (state->ofp, "# %lu \"%s\"\n", line_number, filename);
    
    *pp = skip_whitespace (*pp);
    
    if (**pp && **pp != '\n') {
    
        report_line_at (get_filename (), get_line_number (), REPORT_WARNING, start_p, *pp, "extra tokens at end of #include directive");
        ignore_rest_of_line (pp);
    
    }

}

static void handler_error (char **pp) {

    char saved_ch;
    char *p1 = (*pp = skip_whitespace (*pp));
    
    while (*p1 && *p1 != '\n') {
        p1++;
    }
    
    saved_ch = *p1;
    *p1 = '\0';
    
    report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, skip_whitespace (start_p + 1), "#error %s", *pp);
    
    *p1 = saved_ch;
    ignore_rest_of_line (pp);

}

static void handler_warning (char **pp) {

    char saved_ch;
    char *p1 = (*pp = skip_whitespace (*pp));
    
    while (*p1 && *p1 != '\n') {
        p1++;
    }
    
    saved_ch = *p1;
    *p1 = '\0';
    
    report_line_at (get_filename (), get_line_number (), REPORT_WARNING, start_p, skip_whitespace (start_p + 1), "#error %s", *pp);
    
    *p1 = saved_ch;
    ignore_rest_of_line (pp);

}


int preprocess (const char *filename) {

    struct token *tok;
    FILE *fp;
    
    unsigned long new_line_number = 1;
    void *load_line_internal_data = NULL;
    
    unsigned long newlines;
    char *line, *line_end;
    
    int ident = 0, code_started = 0, at_bol, enabled = 1, nested = 0;
    set_filename_and_line_number (filename, 0);
    
    if ((load_line_internal_data = load_line_create_internal_data (&new_line_number)) == NULL) {
        return -1;
    }
    
    if ((fp = fopen (filename, "r")) == NULL) {
    
        free (load_line_internal_data);
        return -2;
    
    }
    
    vec_push (&internal_lines, load_line_internal_data);
    vec_push (&open_files, fp);
    
    enable = 1;
    satisfy = 0;
    
    memset (&cond_stack, 0, sizeof (cond_stack));
    
    if (!root_filename) {
        root_filename = filename;
    }
    
    /*
     * fprintf (state->ofp, "# 1 \"%s\"\n", filename);
     * fprintf (state->ofp, "# 1 \"<built-in>\"\n");
     * fprintf (state->ofp, "# 1 \"<command-line>\"\n");
     */
    
    fprintf (state->ofp, "# 1 \"%s\"\n", filename);
    
    while (!load_line (&line, &line_end, &newlines, fp, &load_line_internal_data)) {
    
        start_p = (line = skip_whitespace (line));
        at_bol = 1;
        
        set_filename_and_line_number (filename, new_line_number);
        new_line_number += newlines + 1;
        
        if (*start_p == '#') {
        
            char *temp = skip_whitespace (start_p + 1);
            struct token *tok = get_token (start_p, &temp);
            
            if (tok->kind == TOK_IDENT) {
            
                struct cond_special *cse;
                struct hashtab_name *key;
                
                if ((key = hashtab_alloc_name (tok->ident))) {
                
                    if ((cse = hashtab_get (&hashtab_cond_special, key))) {
                    
                        free (key);
                        
                        if (code_started) {
                            fprintf (state->ofp, "# %lu \"%s\"\n", get_line_number () + 1, filename);
                        }
                        
                        enabled = (cse->handler) (&temp);
                        continue;
                    
                    }
                    
                    free (key);
                
                }
            
            }
        
        }
        
        if (!enabled) {
            continue;
        }
        
        while (line <= line_end) {
        
            char *temp;
            tok = get_token (start_p, &line);
            
            if (tok->kind == TOK_EOL) {
                continue;
            }
            
            if (tok->kind == TOK_LBRACE) {
            
                if (at_bol) {
                    fprintf (state->ofp, "%*s%s", ident, "", tok->ident);
                } else {
                    fprintf (state->ofp, "%s", tok->ident);
                }
                
                ident += 4;
                nested++;
                
                fprintf (state->ofp, "\n");
                line = skip_whitespace (line);
                
                if (code_started) {
                
                    if (*line == '\n') {
                    
                        line = skip_whitespace (line + 1);
                        fprintf (state->ofp, "# %lu \"%s\"\n", get_line_number () + 1, filename);
                    
                    } else {
                        fprintf (state->ofp, "# %lu \"%s\"\n", get_line_number (), filename);
                    }
                
                }
                
                at_bol = 1;
                continue;
            
            }
            
            if (tok->kind == TOK_RBRACE) {
            
                int at_bol1 = at_bol;
                
                if (*start_p != '}') {
                
                    at_bol = 1;
                    
                    if (*(line - 2) == ' ') {
                        fseek (state->ofp, -1, SEEK_CUR);
                    }
                    
                    fprintf (state->ofp, "\n");
                
                }
                
                if (ident >= 4) {
                    ident -= 4;
                }
                
                nested--;
                
                if (code_started) {
                
                    if (nested) {
                        fprintf (state->ofp, "# %lu \"%s\"\n", get_line_number (), filename);
                    } else {
                        fprintf (state->ofp, "# %lu \"%s\"\n", get_line_number () - 1, filename);
                    }
                
                }
                
                if (at_bol) {
                    fprintf (state->ofp, "%*s%s", ident, "", tok->ident);
                } else {
                    fprintf (state->ofp, "%s", tok->ident);
                }
                
                at_bol = at_bol1;
                continue;
            
            }
            
            if (tok->kind == TOK_HASH) {
            
                line = skip_whitespace (line);
                tok = get_token (start_p, &line);
                
                if (tok->kind == TOK_IDENT) {
                
                    struct hashtab_name *key;
                    
                    if ((key = hashtab_alloc_name (tok->ident))) {
                    
                        struct special *se;
                        
                        if ((se = hashtab_get (&hashtab_special, key))) {
                        
                            (se->handler) (&line);
                            
                            free (key);
                            continue;
                        
                        }
                        
                        free (key);
                        
                        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, line - tok->len, "invalid preprocessing directive #%s", tok->ident);
                        ignore_rest_of_line (&line);
                    
                    }
                
                }
                
                continue;
            
            }
            
            if (tok->kind == TOK_NEWLINE) {
            
                if (code_started) {
                    fprintf (state->ofp, "%s", tok->ident);
                }
                
                at_bol = 1;
                continue;
            
            }
            
            if (!code_started) {
            
                code_started = 1;
                fprintf (state->ofp, "# %lu \"%s\"\n", get_line_number (), filename);
            
            }
            
            if (tok->kind == TOK_IDENT) {
            
                char *temp3 = tok->ident;
                struct macro *me;
                
                while (*temp3 == '*') {
                    temp3 = skip_whitespace (temp3 + 1);
                }
                
                if ((me = find_macro (temp3))) {
                
                    if (!(me->type & MACRO_HIDDEN)) {
                    
                        if (me->handler) {
                        
                            (me->handler) (state->ofp);
                            continue;
                        
                        }
                        
                        char *temp = xstrndup (tok->ident, (temp3 - tok->ident));
                        tok = xmalloc (sizeof (*tok));
                        
                        tok->ident = temp;
                        tok->len = strlen (temp);
                        
                        if (!(temp3 = expand_macro (me, start_p, &line))) {
                            continue;
                        }
                        
                        tok->ident = xrealloc (tok->ident, tok->len + strlen (temp3) + 1);
                        tok->len += sprintf (tok->ident + tok->len, "%s", temp3);
                        
                        tok->ident[tok->len] = '\0';
                    
                    }
                
                }
            
            }
            
        /*print_ident:*/
            
            temp = line - 1;
            
            while (*temp == ' ') {
                temp--;
            }
            
            /*report_at (__FILE__, __LINE__, REPORT_FATAL_ERROR, "%s", temp);*/
            
            if (at_bol && *temp != '}') {
                fprintf (state->ofp, "%*s%s", ident, "", tok->ident);
            } else {
                fprintf (state->ofp, "%s", tok->ident);
            }
            
            if (tok->kind == TOK_SEMI) {
            
                at_bol = 1;
                line = skip_whitespace (line);
                
                if (*line != '\n') {
                    fprintf (state->ofp, "\n");
                }
            
            } else {
                at_bol = 0;
            }
        
        }
    
    }
    
    root_filename = 0;
    
    load_line_destory_internal_data (vec_pop (&internal_lines));
    fprintf (state->ofp, "\n");
    
    if (cond_stack.length > 0) {
    
        struct cond_statement *statement;
        long count = cond_stack.length, i;
        
        for (i = 0; i < count; i++) {
        
            statement = cond_stack.data[i];
            report_at (statement->filename, statement->lineno, REPORT_ERROR, "unterminated %s", statement->str);
        
        }
    
    }
    
    vec_pop (&open_files);
    return 0;

}


static struct cond_special cond_special_table[] = {

    {   "if",           handler_if          },
    {   "ifdef",        handler_ifdef       },
    {   "ifndef",       handler_ifndef      },
    {   "elif",         handler_elif        },
    {   "else",         handler_else        },
    {   "endif",        handler_endif       },
    
    {   0,              0                   }

};

static struct special special_table[] = {

    {   "define",       handler_define      },
    {   "undef",        handler_undef       },
    
    {   "error",        handler_error       },
    {   "warning",      handler_warning     },
    
    {   "include",      handler_include     },
    {   0,              0                   }

};

void init_cpp (void) {

    struct hashtab_name *key;
    struct cond_special *cse;
    struct special *se;
    
    for (cse = cond_special_table; cse->name; cse++) {
    
        if (!(key = hashtab_alloc_name (cse->name))) {
            continue;
        }
        
        hashtab_put (&hashtab_cond_special, key, cse);
    
    }
    
    for (se = special_table; se->name; se++) {
    
        if (!(key = hashtab_alloc_name (se->name))) {
            continue;
        }
        
        hashtab_put (&hashtab_special, key, se);
    
    }

}
