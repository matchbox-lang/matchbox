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
            if (checkKeyword(lexer, 1, 1, "s")) return TOKEN_AS;
            if (checkKeyword(lexer, 1, 4, "sync")) return TOKEN_ASYNC;
            if (checkKeyword(lexer, 1, 4, "wait")) return TOKEN_AWAIT;
            break;
        case 'b':
            if (checkKeyword(lexer, 1, 3, "ool")) return TOKEN_BOOL;
            if (checkKeyword(lexer, 1, 4, "reak")) return TOKEN_BREAK;
            break;
        case 'c':
            if (checkKeyword(lexer, 1, 4, "atch")) return TOKEN_CATCH;
            if (checkKeyword(lexer, 1, 3, "har")) return TOKEN_CHAR;
            if (checkKeyword(lexer, 1, 4, "lass")) return TOKEN_CLASS;
            if (checkKeyword(lexer, 1, 4, "onst")) return TOKEN_CONST;
            if (checkKeyword(lexer, 1, 7, "ontinue")) return TOKEN_CONTINUE;
            break;
        case 'd':
            if (checkKeyword(lexer, 1, 5, "ouble")) return TOKEN_DOUBLE;
            if (checkKeyword(lexer, 1, 4, "efer")) return TOKEN_DEFER;
            break;
        case 'e':
            if (checkKeyword(lexer, 1, 3, "lse")) return TOKEN_ELSE;
            if (checkKeyword(lexer, 1, 2, "nd")) return TOKEN_END;
            if (checkKeyword(lexer, 1, 3, "num")) return TOKEN_ENUM;
            if (checkKeyword(lexer, 1, 5, "xtern")) return TOKEN_EXTERN;
            break;
        case 'f':
            if (checkKeyword(lexer, 1, 4, "alse")) return TOKEN_FALSE;
            if (checkKeyword(lexer, 1, 6, "inally")) return TOKEN_FINALLY;
            if (checkKeyword(lexer, 1, 4, "loat")) return TOKEN_FLOAT;
            if (checkKeyword(lexer, 1, 2, "or")) return TOKEN_FOR;
            if (checkKeyword(lexer, 1, 3, "unc")) return TOKEN_FUNC;
            break;
        case 'g':
            if (checkKeyword(lexer, 1, 2, "et")) return TOKEN_GET;
            break;
        case 'h':
            if (checkKeyword(lexer, 1, 2, "as")) return TOKEN_HAS;
            break;
        case 'i':
            if (checkKeyword(lexer, 1, 2, "et")) return TOKEN_LET;
            if (checkKeyword(lexer, 1, 1, "f")) return TOKEN_IF;
            if (checkKeyword(lexer, 1, 1, "n")) return TOKEN_IN;
            if (checkKeyword(lexer, 1, 2, "nt")) return TOKEN_INT;
            if (checkKeyword(lexer, 1, 3, "nt8")) return TOKEN_INT8;
            if (checkKeyword(lexer, 1, 4, "nt16")) return TOKEN_INT16;
            if (checkKeyword(lexer, 1, 4, "nt32")) return TOKEN_INT32;
            if (checkKeyword(lexer, 1, 4, "nt64")) return TOKEN_INT64;
            if (checkKeyword(lexer, 1, 7, "nternal")) return TOKEN_INTERNAL;
            if (checkKeyword(lexer, 1, 1, "s")) return TOKEN_IS;
            break;
        case 'l':
            if (checkKeyword(lexer, 1, 2, "et")) return TOKEN_LET;
            break;
        case 'm':
            if (checkKeyword(lexer, 1, 4, "atch")) return TOKEN_MATCH;
            break;
        case 'p':
            if (checkKeyword(lexer, 1, 7, "rotocol")) return TOKEN_PROTOCOL;
            if (checkKeyword(lexer, 1, 6, "rivate")) return TOKEN_PRIVATE;
            if (checkKeyword(lexer, 1, 5, "ublic")) return TOKEN_PUBLIC;
            break;
        case 'r':
            if (checkKeyword(lexer, 1, 5, "eturn")) return TOKEN_RETURN;
            break;
        case 's':
            if (checkKeyword(lexer, 1, 3, "elf")) return TOKEN_SELF;
            if (checkKeyword(lexer, 1, 2, "et")) return TOKEN_SET;
            if (checkKeyword(lexer, 1, 5, "izeof")) return TOKEN_SIZEOF;
            if (checkKeyword(lexer, 1, 5, "tatic")) return TOKEN_STATIC;
            if (checkKeyword(lexer, 1, 5, "tring")) return TOKEN_STRING;
            if (checkKeyword(lexer, 1, 5, "truct")) return TOKEN_STRUCT;
            break;
        case 't':
            if (checkKeyword(lexer, 1, 4, "hrow")) return TOKEN_THROW;
            if (checkKeyword(lexer, 1, 3, "rue")) return TOKEN_TRUE;
            if (checkKeyword(lexer, 1, 2, "ry")) return TOKEN_TRY;
            if (checkKeyword(lexer, 1, 3, "ype")) return TOKEN_TYPE;
            if (checkKeyword(lexer, 1, 5, "ypeof")) return TOKEN_TYPEOF;
            break;
        case 'u':
            if (checkKeyword(lexer, 1, 3, "int")) return TOKEN_UINT;
            if (checkKeyword(lexer, 1, 4, "int8")) return TOKEN_UINT8;
            if (checkKeyword(lexer, 1, 5, "int16")) return TOKEN_UINT16;
            if (checkKeyword(lexer, 1, 5, "int32")) return TOKEN_UINT32;
            if (checkKeyword(lexer, 1, 5, "int64")) return TOKEN_UINT64;
            if (checkKeyword(lexer, 1, 5, "nless")) return TOKEN_UNLESS;
            if (checkKeyword(lexer, 1, 2, "se")) return TOKEN_USE;
            break;
        case 'v':
            if (checkKeyword(lexer, 1, 2, "ar")) return TOKEN_VAR;
            break;
        case 'w':
            if (checkKeyword(lexer, 1, 4, "here")) return TOKEN_WHERE;
            if (checkKeyword(lexer, 1, 4, "hile")) return TOKEN_WHILE;
            break;
        case 'y':
            if (checkKeyword(lexer, 1, 4, "ield")) return TOKEN_YIELD;
            break;
        default:
            break;
    }

    return TOKEN_IDENTIFIER;
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

    return makeToken(lexer, TOKEN_FLOAT_LITERAL);
}

static Token integerLiteral(Lexer* lexer)
{
    while (isDigit(peek(lexer)) || (peek(lexer) == '_' && isDigit(next(lexer)))) {
        advance(lexer);
    }

    if (peek(lexer) == '.' && next(lexer) != '.') {
        return floatLiteral(lexer);
    }

    return makeToken(lexer, TOKEN_INTEGER_LITERAL);
}

static Token hexadecimalLiteral(Lexer* lexer)
{
    while (isXDigit(peek(lexer)) || (peek(lexer) == '_' && isXDigit(next(lexer)))) {
        advance(lexer);
    }

    return makeToken(lexer, TOKEN_HEXADECIMAL_LITERAL);
}

static Token octalLiteral(Lexer* lexer)
{
    while (isODigit(peek(lexer)) || (peek(lexer) == '_' && isODigit(next(lexer)))) {
        advance(lexer);
    }

    return makeToken(lexer, TOKEN_OCTAL_LITERAL);
}

static Token binaryLiteral(Lexer* lexer)
{
    while (isBDigit(peek(lexer)) || (peek(lexer) == '_' && isBDigit(next(lexer)))) {
        advance(lexer);
    }

    return makeToken(lexer, TOKEN_BINARY_LITERAL);
}

static Token characterLiteral(Lexer* lexer)
{
    while (!isEof(lexer)) {
        if (peek(lexer) == '\'' && prev(lexer) != '\\') {
            advance(lexer);
            return makeToken(lexer, TOKEN_CHARACTER_LITERAL);
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
            return makeToken(lexer, TOKEN_STRING_LITERAL);
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
        return makeToken(lexer, TOKEN_EOF);
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
        case '(':   return makeToken(lexer, TOKEN_LPAREN);
        case ')':   return makeToken(lexer, TOKEN_RPAREN);
        case '[':   return makeToken(lexer, TOKEN_LBRACE);
        case ']':   return makeToken(lexer, TOKEN_RBRACE);
        case '{':   return makeToken(lexer, TOKEN_LBRACE);
        case '}':   return makeToken(lexer, TOKEN_RBRACE);
        case ':':   return makeToken(lexer, TOKEN_COLON);
        case ';':   return makeToken(lexer, TOKEN_SEMICOLON);
        case ',':   return makeToken(lexer, TOKEN_COMMA);
        case '$':   return makeToken(lexer, TOKEN_DOLLAR);
        case '~':   return makeToken(lexer, TOKEN_TILDE);
        case '%':
            return makeToken(lexer, 
                match(lexer, '=') ? TOKEN_PERCENT_EQUAL : TOKEN_PERCENT);
        case '=':
            return makeToken(lexer, 
                match(lexer, '~') ? TOKEN_EQUAL_TILDE :
                match(lexer, '=') ?
                match(lexer, '=') ? TOKEN_EQUAL_EQUAL_EQUAL : TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '!':
            return makeToken(lexer, 
                match(lexer, '~') ? TOKEN_NOT_TILDE :
                match(lexer, '=') ?
                match(lexer, '=') ? TOKEN_NOT_EQUAL_EQUAL : TOKEN_NOT_EQUAL : TOKEN_EXCLAMATION);
        case '&':
            return makeToken(lexer, 
                match(lexer, '&') ? TOKEN_BOOLEAN_AND :
                match(lexer, '=') ? TOKEN_AND_EQUAL : TOKEN_AMPERSAND);
        case '|':
            return makeToken(lexer, 
                match(lexer, '|') ? TOKEN_BOOLEAN_OR :
                match(lexer, '>') ? TOKEN_PIPE_FORWARD :
                match(lexer, '=') ? TOKEN_OR_EQUAL : TOKEN_PIPE);
        case '^':
            return makeToken(lexer, 
                match(lexer, '=') ? TOKEN_CIRCUMFLEX_EQUAL : TOKEN_CIRCUMFLEX);
        case '+':
            return makeToken(lexer, 
                match(lexer, '=') ? TOKEN_PLUS_EQUAL : TOKEN_PLUS);
        case '-':
            if (isDigit(peek(lexer)) || peek(lexer) == '.') {
                return integerLiteral(lexer);
            }
            return makeToken(lexer, 
                match(lexer, '>') ? TOKEN_ARROW :
                match(lexer, '=') ? TOKEN_MINUS_EQUAL : TOKEN_MINUS);
        case '*':
            return makeToken(lexer, 
                match(lexer, '*') ?
                match(lexer, '=') ? TOKEN_POWER_EQUAL : TOKEN_POWER :
                match(lexer, '=') ? TOKEN_STAR_EQUAL : TOKEN_STAR);
        case '/':
            return makeToken(lexer, 
                match(lexer, '/') ?
                match(lexer, '=') ? TOKEN_FLOOR_EQUAL : TOKEN_FLOOR :
                match(lexer, '=') ? TOKEN_SLASH_EQUAL : TOKEN_SLASH);
        case '<':
            return makeToken(lexer, 
                match(lexer, '<') ?
                match(lexer, '=') ? TOKEN_LSHIFT_EQUAL : TOKEN_LSHIFT :
                match(lexer, '|') ? TOKEN_PIPE_BACKWARD :
                match(lexer, '=') ?
                match(lexer, '>') ? TOKEN_SPACESHIP : TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>':
            return makeToken(lexer, 
                match(lexer, '>') ?
                match(lexer, '=') ? TOKEN_RSHIFT_EQUAL : TOKEN_RSHIFT :
                match(lexer, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '?':
            return makeToken(lexer, 
                match(lexer, '?') ?
                match(lexer, '=') ? TOKEN_COALESCE_EQUAL : TOKEN_COALESCE :
                match(lexer, ':') ? TOKEN_TERNARY :
                match(lexer, '.') ? TOKEN_SAFE_ACCESS : TOKEN_QUESTION);
        case '.':
            if (isDigit(peek(lexer))) {
                return floatLiteral(lexer);
            }
            return makeToken(lexer, 
                match(lexer, '.') ?
                match(lexer, '.') ? TOKEN_SPREAD :
                match(lexer, '^') ? TOKEN_RANGE_FROM_END : TOKEN_RANGE : TOKEN_DOT);
        case '@':
            return makeToken(lexer, TOKEN_AT);
        default:
            return makeToken(lexer, TOKEN_UNKNOWN);      
    }
}
