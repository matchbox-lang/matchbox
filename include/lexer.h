#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>

typedef enum TokenType
{
    T_IF,
    T_ELSE,
    T_IN,
    T_INSTANCEOF,
    T_IS,
    T_HAS,
    T_SIZEOF,
    T_TYPEOF,
    T_FOR,
    T_WHILE,
    T_MATCH,
    T_SWITCH,
    T_BREAK,
    T_CONTINUE,
    T_WHERE,
    T_RETURN,
    T_YIELD,
    T_AS,
    T_CONST,
    T_VAR,
    T_USE,
    T_CLASS,
    T_STRUCT,
    T_TRAIT,
    T_CONTRACT,
    T_ENUM,
    T_FUNC,
    T_TYPE,
    T_EXTENDS,
    T_PUBLIC,
    T_STATIC,
    T_CONSTRUCT,
    T_DESTRUCT,
    T_EXTENSION,
    T_SELF,
    T_ASYNC,
    T_AWAIT,
    T_DEFER,
    T_EXTERN,
    T_TRY,
    T_CATCH,
    T_FINALLY,
    T_THROW,
    T_DELETE,
    T_NONE,
    T_INT,
    T_UINT,
    T_INT8,
    T_INT16,
    T_INT32,
    T_INT64,
    T_UINT8,
    T_UINT16,
    T_UINT32,
    T_UINT64,
    T_FLOAT,
    T_DOUBLE,
    T_CHAR,
    T_BOOL,
    T_TRUE,
    T_FALSE,
    T_PLUS,
    T_MINUS,
    T_STAR,
    T_SLASH,
    T_FLOOR,
    T_PERCENT,
    T_POWER,
    T_COALESCE,
    T_INCREMENT,
    T_DECREMENT,
    T_EQUAL,
    T_PLUS_EQUAL,
    T_MINUS_EQUAL,
    T_STAR_EQUAL,
    T_SLASH_EQUAL,
    T_FLOOR_EQUAL,
    T_PERCENT_EQUAL,
    T_POWER_EQUAL,
    T_COALESCE_EQUAL,
    T_EQUAL_EQUAL,
    T_NOT_EQUAL,
    T_GREATER,
    T_GREATER_EQUAL,
    T_LESS,
    T_LESS_EQUAL,
    T_SPACESHIP,
    T_BOOLEAN_AND,
    T_BOOLEAN_OR,
    T_EXCLAMATION,
    T_AMPERSAND,
    T_PIPE,
    T_CIRCUMFLEX,
    T_LSHIFT,
    T_RSHIFT,
    T_TILDE,
    T_QUESTION,
    T_TERNARY,
    T_COLON,
    T_EQUAL_TILDE,
    T_NOT_TILDE,
    T_AND_EQUAL,
    T_OR_EQUAL,
    T_CIRCUMFLEX_EQUAL,
    T_LSHIFT_EQUAL,
    T_RSHIFT_EQUAL,
    T_LAMBDA,
    T_SAFE_ACCESS,
    T_PIPE_FORWARD,
    T_PIPE_BACKWARD,
    T_DOLLAR,
    T_RANGE,
    T_RANGE_FROM_END,
    T_SPREAD,
    T_AT,
    T_DOT,
    T_LPAREN,
    T_RPAREN,
    T_LSQUARE,
    T_RSQUARE,
    T_LBRACE,
    T_RBRACE,
    T_SEMICOLON,
    T_COMMA,
    T_DECIMAL_LITERAL,
    T_FLOAT_LITERAL,
    T_OCTAL_LITERAL,
    T_HEXADECIMAL_LITERAL,
    T_BINARY_LITERAL,
    T_CHARACTER_LITERAL,
    T_STRING_LITERAL,
    T_IDENTIFIER,
    T_EOF,
    T_UNKNOWN
} TokenType;

typedef struct Token
{
    TokenType type;
    char* chars;
    size_t length;
    int line;
    int column;
} Token;

void initLexer(char* source);
void printToken(Token* token);
Token scanToken();

#endif
