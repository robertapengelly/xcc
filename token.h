/******************************************************************************
 * @file            token.h
 *****************************************************************************/
#ifndef     _TOKEN_H
#define     _TOKEN_H

enum token_kind {

    TOK_EOL = 0,
    
    TOK_CHAR_LITERAL,
    TOK_STRING_LITERAL,
    
    TOK_IDENT,
    TOK_PUNCT,
    TOK_PP_NUM,
    
    TOK_EQ,
    TOK_GT,
    TOK_LT,
    TOK_OR,
    TOK_ADD,
    TOK_AND,
    TOK_DIV,
    TOK_MOD,
    TOK_SUB,
    TOK_XOR,
    TOK_EQEQ,
    TOK_GTEQ,
    TOK_LTEQ,
    TOK_OROR,
    TOK_STAR,
    TOK_COLON,
    TOK_NOTEQ,
    TOK_ANDAND,
    TOK_QUESTION,
    
    TOK_NOT,
    TOK_HASH,
    TOK_SEMI,
    TOK_COMMA,
    TOK_CONCAT,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LBRACKET,
    TOK_RBRACKET,
    
    TOK_NEWLINE,
    TOK_WHITESPACE,
    TOK___VA_ARGS__

};

struct token {

    enum token_kind kind;
    
    char *ident;
    unsigned long len;

};

struct token *copy_token (struct token *tok);
struct token *get_token (char *start_p, char **pp);

void init_punct (void);

#endif      /* _TOKEN_H */
