#include "lexer.h"
#include "token.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* characterError = "Error: Missing terminating %c character";
static const char* commentError = "Error: Unterminated comment";

static void error(Lexer* lexer, const char* message, const char c)
{
    fprintf(stderr, message, c);
    fprintf(stderr, " on line %d:%d\n", lexer->start.line, lexer->start.column);
    exit(1);
}

static char peek(Lexer* lexer)
{
    return *lexer->current.chars;
}

static char prev(Lexer* lexer)
{
    return lexer->current.chars[-1];
}

static char next(Lexer* lexer)
{
    return lexer->current.chars[1];
}

static char advance(Lexer* lexer)
{
    lexer->current.column++;
    
    if (*lexer->current.chars == '\n') {
        lexer->current.line++;
        lexer->current.column = 1;
    }

    lexer->current.chars++;
    return lexer->current.chars[-1];
}

static bool isEof(Lexer* lexer)
{
    return *lexer->current.chars == '\0';
}

static bool match(Lexer* lexer, char c)
{
    if (*lexer->current.chars != c) {
        return false;
    }

    advance(lexer);
    
    return true;
}

static Token makeToken(Lexer* lexer, TokenType type)
{
    Token token;
    token.type = type;
    token.length = lexer->current.chars - lexer->start.chars;
    token.chars = lexer->start.chars;
    token.line = lexer->start.line;
    token.column = lexer->start.column;

    return token;
}

static bool isAlpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

static bool isXDigit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static bool isODigit(char c)
{
    return c >= '0' && c <= '7';
}

static bool isBDigit(char c)
{
    return c == '0' || c == '1';
}

static void skipCommentSingle(Lexer* lexer)
{
    advance(lexer);
    
    while (!isEof(lexer) && peek(lexer) != '\n') {
        advance(lexer);
    }
}

static void skipCommentMulti(Lexer* lexer)
{
    advance(lexer);
    advance(lexer);

    while (!isEof(lexer)) {
        if (peek(lexer) == '#' && next(lexer) == '#') {
            advance(lexer);
            advance(lexer);
            return;
        }
        
        advance(lexer);
    }

    error(lexer, commentError, 0);
}

static void skipComment(Lexer* lexer)
{
    lexer->start.chars = lexer->current.chars;
    lexer->start.line = lexer->current.line;
    lexer->start.column = lexer->current.column;
    
    if (next(lexer) == '#') {
        return skipCommentMulti(lexer);
    }

    skipCommentSingle(lexer);
}

static void skipWhitespace(Lexer* lexer)
{
    while (1) {
        switch (peek(lexer)) {
            case '#':
                skipComment(lexer);
                break;
            case ' ':
            case '\t':
            case '\r':
            case '\n':
            case '\f':
            case '\v':
                advance(lexer);
                break;
            default:
                return;
        }
    }
}

static int checkKeyword(Lexer* lexer, int chars, size_t len, const char* rest)
{
    if (lexer->current.chars - lexer->start.chars != chars + len) {
        return 0;
    }

    return memcmp(lexer->start.chars + chars, rest, len) == 0;
}

static TokenType getIdentifierType(Lexer* lexer)
{
    char c =* lexer->start.chars;

    switch (c) {
        case 'a':
            if (checkKeyword(lexer, 1, 1, "s")) return T_AS;
            if (checkKeyword(lexer, 1, 4, "sync")) return T_ASYNC;
            if (checkKeyword(lexer, 1, 4, "wait")) return T_AWAIT;
            break;
        case 'b':
            if (checkKeyword(lexer, 1, 3, "ool")) return T_BOOL;
            if (checkKeyword(lexer, 1, 4, "reak")) return T_BREAK;
            break;
        case 'c':
            if (checkKeyword(lexer, 1, 4, "atch")) return T_CATCH;
            if (checkKeyword(lexer, 1, 3, "har")) return T_CHAR;
            if (checkKeyword(lexer, 1, 4, "lass")) return T_CLASS;
            if (checkKeyword(lexer, 1, 4, "onst")) return T_CONST;
            if (checkKeyword(lexer, 1, 7, "ontinue")) return T_CONTINUE;
            break;
        case 'd':
            if (checkKeyword(lexer, 1, 5, "ouble")) return T_DOUBLE;
            if (checkKeyword(lexer, 1, 4, "efer")) return T_DEFER;
            break;
        case 'e':
            if (checkKeyword(lexer, 1, 3, "lse")) return T_ELSE;
            if (checkKeyword(lexer, 1, 2, "nd")) return T_END;
            if (checkKeyword(lexer, 1, 3, "num")) return T_ENUM;
            if (checkKeyword(lexer, 1, 5, "xtern")) return T_EXTERN;
            break;
        case 'f':
            if (checkKeyword(lexer, 1, 4, "alse")) return T_FALSE;
            if (checkKeyword(lexer, 1, 6, "inally")) return T_FINALLY;
            if (checkKeyword(lexer, 1, 4, "loat")) return T_FLOAT;
            if (checkKeyword(lexer, 1, 2, "or")) return T_FOR;
            if (checkKeyword(lexer, 1, 3, "unc")) return T_FUNC;
            break;
        case 'g':
            if (checkKeyword(lexer, 1, 2, "et")) return T_GET;
            break;
        case 'h':
            if (checkKeyword(lexer, 1, 2, "as")) return T_HAS;
            break;
        case 'i':
            if (checkKeyword(lexer, 1, 2, "et")) return T_LET;
            if (checkKeyword(lexer, 1, 1, "f")) return T_IF;
            if (checkKeyword(lexer, 1, 1, "n")) return T_IN;
            if (checkKeyword(lexer, 1, 2, "nt")) return T_INT;
            if (checkKeyword(lexer, 1, 3, "nt8")) return T_INT8;
            if (checkKeyword(lexer, 1, 4, "nt16")) return T_INT16;
            if (checkKeyword(lexer, 1, 4, "nt32")) return T_INT32;
            if (checkKeyword(lexer, 1, 4, "nt64")) return T_INT64;
            if (checkKeyword(lexer, 1, 7, "nternal")) return T_INTERNAL;
            if (checkKeyword(lexer, 1, 1, "s")) return T_IS;
            break;
        case 'l':
            if (checkKeyword(lexer, 1, 2, "et")) return T_LET;
            break;
        case 'm':
            if (checkKeyword(lexer, 1, 4, "atch")) return T_MATCH;
            break;
        case 'p':
            if (checkKeyword(lexer, 1, 7, "rotocol")) return T_PROTOCOL;
            if (checkKeyword(lexer, 1, 6, "rivate")) return T_PRIVATE;
            if (checkKeyword(lexer, 1, 5, "ublic")) return T_PUBLIC;
            break;
        case 'r':
            if (checkKeyword(lexer, 1, 5, "eturn")) return T_RETURN;
            break;
        case 's':
            if (checkKeyword(lexer, 1, 3, "elf")) return T_SELF;
            if (checkKeyword(lexer, 1, 2, "et")) return T_SET;
            if (checkKeyword(lexer, 1, 5, "izeof")) return T_SIZEOF;
            if (checkKeyword(lexer, 1, 5, "tatic")) return T_STATIC;
            if (checkKeyword(lexer, 1, 5, "tring")) return T_STRING;
            if (checkKeyword(lexer, 1, 5, "truct")) return T_STRUCT;
            break;
        case 't':
            if (checkKeyword(lexer, 1, 4, "hrow")) return T_THROW;
            if (checkKeyword(lexer, 1, 3, "rue")) return T_TRUE;
            if (checkKeyword(lexer, 1, 2, "ry")) return T_TRY;
            if (checkKeyword(lexer, 1, 3, "ype")) return T_TYPE;
            if (checkKeyword(lexer, 1, 5, "ypeof")) return T_TYPEOF;
            break;
        case 'u':
            if (checkKeyword(lexer, 1, 3, "int")) return T_UINT;
            if (checkKeyword(lexer, 1, 4, "int8")) return T_UINT8;
            if (checkKeyword(lexer, 1, 5, "int16")) return T_UINT16;
            if (checkKeyword(lexer, 1, 5, "int32")) return T_UINT32;
            if (checkKeyword(lexer, 1, 5, "int64")) return T_UINT64;
            if (checkKeyword(lexer, 1, 5, "nless")) return T_UNLESS;
            if (checkKeyword(lexer, 1, 2, "se")) return T_USE;
            break;
        case 'v':
            if (checkKeyword(lexer, 1, 2, "ar")) return T_VAR;
            break;
        case 'w':
            if (checkKeyword(lexer, 1, 4, "here")) return T_WHERE;
            if (checkKeyword(lexer, 1, 4, "hile")) return T_WHILE;
            break;
        case 'y':
            if (checkKeyword(lexer, 1, 4, "ield")) return T_YIELD;
            break;
        default:
            break;
    }

    return T_IDENTIFIER;
}

static Token floatLiteral(Lexer* lexer)
{
    while (isDigit(peek(lexer)) || (peek(lexer) == '_' && isDigit(next(lexer)))) {
        advance(lexer);
    }

    if (peek(lexer) == '.' && next(lexer) != '.') {
        advance(lexer);

        while (isDigit(peek(lexer)) || (peek(lexer) == '_' && isDigit(next(lexer)))) {
            advance(lexer);
        }
    }

    return makeToken(lexer, T_FLOAT_LITERAL);
}

static Token integerLiteral(Lexer* lexer)
{
    while (isDigit(peek(lexer)) || (peek(lexer) == '_' && isDigit(next(lexer)))) {
        advance(lexer);
    }

    if (peek(lexer) == '.' && next(lexer) != '.') {
        return floatLiteral(lexer);
    }

    return makeToken(lexer, T_INTEGER_LITERAL);
}

static Token hexadecimalLiteral(Lexer* lexer)
{
    while (isXDigit(peek(lexer)) || (peek(lexer) == '_' && isXDigit(next(lexer)))) {
        advance(lexer);
    }

    return makeToken(lexer, T_HEXADECIMAL_LITERAL);
}

static Token octalLiteral(Lexer* lexer)
{
    while (isODigit(peek(lexer)) || (peek(lexer) == '_' && isODigit(next(lexer)))) {
        advance(lexer);
    }

    return makeToken(lexer, T_OCTAL_LITERAL);
}

static Token binaryLiteral(Lexer* lexer)
{
    while (isBDigit(peek(lexer)) || (peek(lexer) == '_' && isBDigit(next(lexer)))) {
        advance(lexer);
    }

    return makeToken(lexer, T_BINARY_LITERAL);
}

static Token characterLiteral(Lexer* lexer)
{
    while (!isEof(lexer)) {
        if (peek(lexer) == '\'' && prev(lexer) != '\\') {
            advance(lexer);
            return makeToken(lexer, T_CHARACTER_LITERAL);
        }

        advance(lexer);
    }

    error(lexer, characterError, '\'');
}

static Token stringLiteral(Lexer* lexer, char c)
{
    while (!isEof(lexer)) {
        if (peek(lexer) == c && prev(lexer) != '\\') {
            advance(lexer);
            return makeToken(lexer, T_STRING_LITERAL);
        }

        advance(lexer);
    }

    error(lexer, characterError, c);
}

static Token identifier(Lexer* lexer)
{
    while (isAlpha(peek(lexer)) || isDigit(peek(lexer))) {
        advance(lexer);
    }

    return makeToken(lexer, getIdentifierType(lexer));
}

void initLexer(Lexer* lexer, char* source)
{
    lexer->current.chars = source;
    lexer->current.line = 1;
    lexer->current.column = 1;
}

Token scanToken(Lexer* lexer)
{
    skipWhitespace(lexer);

    lexer->start.chars = lexer->current.chars;
    lexer->start.line = lexer->current.line;
    lexer->start.column = lexer->current.column;

    if (isEof(lexer)) {
        return makeToken(lexer, T_EOF);
    }

    char c = advance(lexer);

    if (isAlpha(c)) {
        return identifier(lexer);
    }

    if (c == '0') {
        if (isXDigit(next(lexer)) && (match(lexer, 'x') || match(lexer, 'X'))) return hexadecimalLiteral(lexer);
        if (isODigit(next(lexer)) && (match(lexer, 'o') || match(lexer, 'O'))) return octalLiteral(lexer);
        if (isBDigit(next(lexer)) && (match(lexer, 'b') || match(lexer, 'B'))) return binaryLiteral(lexer);

        return integerLiteral(lexer);
    }

    if (isDigit(c)) {
        return integerLiteral(lexer);
    }

    switch (c) {
        case '"':
        case '`':   return stringLiteral(lexer, c);
        case '\'':  return characterLiteral(lexer);
        case '(':   return makeToken(lexer, T_LPAREN);
        case ')':   return makeToken(lexer, T_RPAREN);
        case '[':   return makeToken(lexer, T_LBRACE);
        case ']':   return makeToken(lexer, T_RBRACE);
        case '{':   return makeToken(lexer, T_LBRACE);
        case '}':   return makeToken(lexer, T_RBRACE);
        case ':':   return makeToken(lexer, T_COLON);
        case ';':   return makeToken(lexer, T_SEMICOLON);
        case ',':   return makeToken(lexer, T_COMMA);
        case '$':   return makeToken(lexer, T_DOLLAR);
        case '~':   return makeToken(lexer, T_TILDE);
        case '%':
            return makeToken(lexer, 
                match(lexer, '=') ? T_PERCENT_EQUAL : T_PERCENT);
        case '=':
            return makeToken(lexer, 
                match(lexer, '~') ? T_EQUAL_TILDE :
                match(lexer, '=') ?
                match(lexer, '=') ? T_EQUAL_EQUAL_EQUAL : T_EQUAL_EQUAL : T_EQUAL);
        case '!':
            return makeToken(lexer, 
                match(lexer, '~') ? T_NOT_TILDE :
                match(lexer, '=') ?
                match(lexer, '=') ? T_NOT_EQUAL_EQUAL : T_NOT_EQUAL : T_EXCLAMATION);
        case '&':
            return makeToken(lexer, 
                match(lexer, '&') ? T_BOOLEAN_AND :
                match(lexer, '=') ? T_AND_EQUAL : T_AMPERSAND);
        case '|':
            return makeToken(lexer, 
                match(lexer, '|') ? T_BOOLEAN_OR :
                match(lexer, '>') ? T_PIPE_FORWARD :
                match(lexer, '=') ? T_OR_EQUAL : T_PIPE);
        case '^':
            return makeToken(lexer, 
                match(lexer, '=') ? T_CIRCUMFLEX_EQUAL : T_CIRCUMFLEX);
        case '+':
            return makeToken(lexer, 
                match(lexer, '=') ? T_PLUS_EQUAL : T_PLUS);
        case '-':
            if (isDigit(peek(lexer)) || peek(lexer) == '.') {
                return integerLiteral(lexer);
            }
            return makeToken(lexer, 
                match(lexer, '>') ? T_ARROW :
                match(lexer, '=') ? T_MINUS_EQUAL : T_MINUS);
        case '*':
            return makeToken(lexer, 
                match(lexer, '*') ?
                match(lexer, '=') ? T_POWER_EQUAL : T_POWER :
                match(lexer, '=') ? T_STAR_EQUAL : T_STAR);
        case '/':
            return makeToken(lexer, 
                match(lexer, '/') ?
                match(lexer, '=') ? T_FLOOR_EQUAL : T_FLOOR :
                match(lexer, '=') ? T_SLASH_EQUAL : T_SLASH);
        case '<':
            return makeToken(lexer, 
                match(lexer, '<') ?
                match(lexer, '=') ? T_LSHIFT_EQUAL : T_LSHIFT :
                match(lexer, '|') ? T_PIPE_BACKWARD :
                match(lexer, '=') ?
                match(lexer, '>') ? T_SPACESHIP : T_LESS_EQUAL : T_LESS);
        case '>':
            return makeToken(lexer, 
                match(lexer, '>') ?
                match(lexer, '=') ? T_RSHIFT_EQUAL : T_RSHIFT :
                match(lexer, '=') ? T_GREATER_EQUAL : T_GREATER);
        case '?':
            return makeToken(lexer, 
                match(lexer, '?') ?
                match(lexer, '=') ? T_COALESCE_EQUAL : T_COALESCE :
                match(lexer, ':') ? T_TERNARY :
                match(lexer, '.') ? T_SAFE_ACCESS : T_QUESTION);
        case '.':
            if (isDigit(peek(lexer))) {
                return floatLiteral(lexer);
            }
            return makeToken(lexer, 
                match(lexer, '.') ?
                match(lexer, '.') ? T_SPREAD :
                match(lexer, '^') ? T_RANGE_FROM_END : T_RANGE : T_DOT);
        case '@':
            return makeToken(lexer, T_AT);
        default:
            return makeToken(lexer, T_UNKNOWN);      
    }
}
