/******************************************************************************
 * @file            eval.c
 *****************************************************************************/
#include    <ctype.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>

#include    "eval.h"
#include    "lib.h"
#include    "macro.h"
#include    "report.h"
#include    "token.h"

extern const char *get_filename (void);
extern unsigned long get_line_number (void);

static void ignore_rest_of_line (char **pp) {

    while (*pp && **pp != '\n') {
        (*pp)++;
    }

}

static int from_hex (char c) {

    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    
    return c - 'A' + 10;

}

static int eval_unary (char *start_p, char **pp) {

    struct token *tok;
    int result = 0;
    
    *pp = skip_whitespace (*pp);
    tok = get_token (start_p, pp);
    
    if (tok->kind == TOK_CHAR_LITERAL) {
    
        char ch;
        
        if (tok->len == 2) {
        
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, *pp - tok->len, "empty charater constant");
            
            ignore_rest_of_line (pp);
            return 0;
        
        }
        
        if (tok->len > 3) {
        
            char *p = tok->ident + 1;
            char ch;
            
            if ((ch = *p++) == '\\') {
            
                if (*p >= '0' && *p <= '7') {
                
                    result = *p++ - '0';
                    
                    if (*p >= '0' && *p <= '7') {
                    
                        result = (result << 3) + (*p++ - '0');
                        
                        if (*p >= '0' && *p <= '7') {
                            result = (result << 3) + (*p++ - '0');
                        }
                    
                    }
                    
                    if (*p == '\'') {
                        return result;
                    }
                    
                    goto error;
                
                }
                
                if (*p == 'x') {
                
                    p++;
                    
                    if (!isxdigit ((int) *p)) {
                    
                        report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, (*pp - tok->len) + (p - tok->ident), "invalid hex escape sequence");
                        return 0;
                    
                    }
                    
                    for (; isxdigit ((int) *p); p++) {
                        result = (result << 4) + from_hex (*p);
                    }
                    
                    if (*p == '\'') {
                        return result;
                    }
                    
                    goto error;
                
                }
            
            }
            
            switch (*p) {
            
                case 'a':
                
                    return '\a';
                
                case 'b':
                
                    return '\b';
                
                case 'f':
                
                    return '\f';
                
                case 'n':
                
                    return '\n';
                
                case 'r':
                
                    return '\r';
                
                case 't':
                
                    return '\t';
                
                case 'v':
                
                    return '\v';
            
            }
            
        error:
            
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, *pp - tok->len, "multi-character charater constant");
            
            ignore_rest_of_line (pp);
            return 0;
        
        }
        
        ch = *(tok->ident + 1);
        
        if (ch >= '0' && ch <= '9') {
            return (ch - '0');
        }
        
        return ((ch & 0xdf) - ('A' - 10));
    
    }
    
    if (tok->kind == TOK_PP_NUM) {
    
        int base = 10, ch;
        result = 0;
        
        if (*tok->ident == '0') {
        
            base = 8;
            tok->ident++;
        
        }
        
        if (*tok->ident == 'x' || *tok->ident == 'X') {
        
            base = 16;
            tok->ident++;
        
        }
        
        while ((ch = *tok->ident++)) {
            result = (result * base) + from_hex (ch);
        }
        
        return result;
        
    }
    
    if (tok->kind == TOK_IDENT) {
    
        char *body;
        
        if (strcmp (tok->ident, "defined") == 0) {
        
            *pp = skip_whitespace (*pp);
            
            if (**pp && **pp == '(') {
            
                *pp = skip_whitespace (*pp + 1);
                tok = get_token (start_p, pp);
                
                if (tok->kind != TOK_IDENT) {
                
                    report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, *pp - tok->len, "operator \"defined\" requires an identifier");
                    
                    ignore_rest_of_line (pp);
                    return 0;
                
                }
                
                *pp = skip_whitespace (*pp);
                
                if (**pp != ')') {
                
                    report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, *pp, "missing ')' after \"defined\"");
                    
                    ignore_rest_of_line (pp);
                    return 0;
                
                }
                
                *pp = skip_whitespace (*pp + 1);
                return has_macro (tok->ident);
            
            }
            
            tok = get_token (start_p, pp);
            
            if (tok->kind != TOK_IDENT) {
            
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, *pp - tok->len, "operator \"defined\" requires an identifier");
                
                ignore_rest_of_line (pp);
                return 0;
            
            }
            
            return has_macro (tok->ident);
        
        }
        
        if (!has_macro (tok->ident)) {
            return 0;
        }
        
        body = expand_macro (find_macro (tok->ident), start_p, pp);
        
        if (!body) {
            return 0;
        }
        
        return eval_unary (body, &body);
    
    }
    
    if (tok->kind == TOK_NOT) {
        return !eval_unary (start_p, pp);
    }
    
    if (tok->kind == TOK_LBRACKET) {
    
        char *line_start = *pp, *temp;
        int nested = 0;
        
        *pp = skip_whitespace (*pp);
        
        while (**pp) {
        
            if (**pp == ')') {
            
                if (!nested) { break; }
                nested--;
            
            }
            
            if (**pp == '(') {
                nested++;
            }
            
            (*pp)++; 
        
        }
        
        if (**pp != ')') {
        
            report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, line_start - 1, "missing ')' in #if expression");
            
            ignore_rest_of_line (pp);
            return 0;
        
        }
        
        temp = xmalloc ((*pp - line_start) + 1);
        sprintf (temp, "%.*s", (int) (*pp - line_start), line_start);
        
        *pp = skip_whitespace (*pp + 1);
        return eval (temp, &temp);
    
    }
    
    report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, *pp - tok->len, "token '%s' is not valid in preprocessor expressions", tok->ident);
    
    ignore_rest_of_line (pp);
    return 0;

}

static int operator_precedence (enum token_kind kind) {

    switch (kind) {
    
        case TOK_STAR:  case TOK_DIV:   case TOK_MOD:
        
            return 3;
        
        case TOK_ADD:   case TOK_SUB:
        
            return 4;
        
        case TOK_LT:    case TOK_GT:    case TOK_LTEQ:  case TOK_GTEQ:
        
            return 6;
        
        case TOK_EQEQ:  case TOK_NOTEQ:
        
            return 7;
        
        case TOK_AND:
        
            return 8;
        
        case TOK_XOR:
        
            return 9;
        
        case TOK_OR:
        
            return 10;
        
        case TOK_ANDAND:
        
            return 11;
        
        case TOK_OROR:
        
            return 12;
        
        case TOK_QUESTION:
        
            return 13;
        
        default:
        
            return 100;
    
    }

}

static int eval_expr (char *start_p, char **pp, int lhs, int outer_prec) {

    struct token *tok;
    
    int op, prec, look_ahead_prec;
    int l, r, rhs;
    
    for (;;) {
    
        *pp = skip_whitespace (*pp);
        tok = get_token (start_p, pp);
        
        op = tok->kind;
        
        if ((prec = operator_precedence (op)) > outer_prec) {
        
            /*if (tok->kind == TOK_LBRACKET) {
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, *pp - tok->len, "missing binary operator before token '('");
            } else if (tok->kind == TOK_RBRACKET) {
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, *pp - tok->len, "missing '(' in expression");
            } else if (tok->kind != TOK_EOL && tok->kind != TOK_NEWLINE) {
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, *pp - tok->len, "token '%s' is not valid in preprocessor expressions", tok->ident);
            }*/
            
            break;
        
        }
        
        if (op == TOK_QUESTION) {
        
            char *start = *pp - tok->len;
            
            l = eval_unary (start_p, pp);
            l = eval_expr (start_p, pp, l, 14);
            
            if (tok->kind != TOK_COLON) {
            
                report_line_at (get_filename (), get_line_number (), REPORT_ERROR, start_p, start, "'?' without following ':'");
                
                ignore_rest_of_line (pp);
                return lhs;
            
            }
            
            r = eval_unary (start_p, pp);
            r = eval_expr (start_p, pp, r, 14);
            
            lhs = lhs ? l : r;
            continue;
        
        }
        
        *pp = skip_whitespace (*pp);
        rhs = eval_unary (start_p, pp);
        
        for (;;) {
        
            *pp = skip_whitespace (*pp);
            tok = get_token (start_p, pp);
            
            if ((look_ahead_prec = operator_precedence (tok->kind)) > prec) {
            
                *pp -= tok->len;
                break;
            
            }
            
            *pp = skip_whitespace (*pp);
            rhs = eval_expr (start_p, pp, rhs, look_ahead_prec);
        
        }
        
        switch (op) {
        
            case TOK_STAR:
            
                lhs *= rhs;
                break;
            
            case TOK_DIV:
            
                lhs /= rhs;
                break;
            
            case TOK_MOD:
            
                lhs %= rhs;
                break;
            
            case TOK_ADD:
            
                lhs += rhs;
                break;
            
            case TOK_SUB:
            
                lhs -= rhs;
                break;
            
            case TOK_GT:
            
                lhs = (lhs > rhs);
                break;
            
            case TOK_LT:
            
                lhs = (lhs < rhs);
                break;
            
            case TOK_LTEQ:
            
                lhs = (lhs <= rhs);
                break;
            
            case TOK_GTEQ:
            
                lhs = (lhs >= rhs);
                break;
            
            case TOK_EQEQ:
            
                lhs = (lhs == rhs);
                break;
            
            case TOK_NOTEQ:
            
                lhs = (lhs != rhs);
                break;
            
            case TOK_AND:
            
                lhs &= rhs;
                break;
            
            case TOK_XOR:
            
                lhs ^= rhs;
                break;
            
            case TOK_OR:
            
                lhs |= rhs;
                break;
            
            case TOK_ANDAND:
            
                lhs = (lhs && rhs);
                break;
            
            case TOK_OROR:
            
                lhs = (lhs || rhs);
                break;
            
            default:
            
                report_at (__FILE__, __LINE__, REPORT_INTERNAL_ERROR, "%s:%lu: unhandled op type", get_filename (), get_line_number ());
                break;
        
        }
    
    }
    
    return lhs;

}

int eval (char *start_p, char **pp) {

    int left = eval_unary (start_p, pp);
    return eval_expr (start_p, pp, left, 15);

}
