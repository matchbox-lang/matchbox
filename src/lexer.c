#include "lexer.h"
#include "stringobject.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct Position
{
    char* chars;
    int line;
    int column;
} Position;

Position current;
Position start;

const char* stringError = "Error: Missing terminating %c character on line %d:%d\n";
const char* characterError = "Error: Missing terminating ' character on line %d:%d\n";
const char* commentError = "Error: Unterminated comment on line %d:%d\n";

void initLexer(char* source)
{
    current.chars = source;
    current.line = 1;
    current.column = 1;
}

static void error(const char* message)
{
    fprintf(stderr, message, start.line, start.column);
    exit(1);
}

static void errorc(const char* message, const char c)
{
    fprintf(stderr, message, c, start.line, start.column);
    exit(1);
}

static char peek()
{
    return *current.chars;
}

static char prev()
{
    return current.chars[-1];
}

static char next()
{
    return current.chars[1];
}

static char advance()
{
    current.column++;
    
    if (*current.chars == '\n') {
        current.line++;
        current.column = 1;
    }

    current.chars++;
    return current.chars[-1];
}

static bool isEof()
{
    return *current.chars == '\0';
}

static bool match(char c)
{
    if (*current.chars != c) {
        return false;
    }

    advance();
    
    return true;
}

static Token makeToken(TokenType type)
{
    Token token;
    token.type = type;
    token.length = current.chars - start.chars;
    token.chars = start.chars;
    token.line = start.line;
    token.column = start.column;

    return token;
}

void printToken(Token* token)
{
    printf("Token\n");
    printf("{\n");
    printf("\ttype: %d\n", token->type);
    printf("\tvalue: ");
    printf("%.*s\n", token->length, token->chars);
    printf("\tline: %i:%i\n", token->line, token->column);
    printf("}\n");
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

static void skipCommentSingle()
{
    advance();
    
    while (!isEof() && peek() != '\n') {
        advance();
    }
}

static void skipCommentMulti()
{
    advance();
    advance();

    while (!isEof()) {
        if (peek() == '#' && next() == '#') {
            advance();
            advance();
            return;
        }
        
        advance();
    }

    error(commentError);
}

static void skipComment()
{
    start.chars = current.chars;
    start.line = current.line;
    start.column = current.column;
    
    if (next() == '#') {
        return skipCommentMulti();
    }

    skipCommentSingle();
}

static void skipWhitespace()
{
    while (1) {
        switch (peek()) {
            case '#':
                skipComment();
                break;
            case ' ':
            case '\t':
            case '\r':
            case '\n':
            case '\f':
            case '\v':
                advance();
                break;
            default:
                return;
        }
    }
}

static int checkKeyword(int chars, int length, const char* rest)
{
    if (current.chars - start.chars != chars + length) {
        return 0;
    }

    return memcmp(start.chars + chars, rest, length) == 0;
}

static TokenType getIdentifierType()
{
    char c =* start.chars;

    switch (c) {
        case 'a':
            if (checkKeyword(1, 1, "s")) return T_AS;
            if (checkKeyword(1, 4, "sync")) return T_ASYNC;
            if (checkKeyword(1, 4, "wait")) return T_AWAIT;
            break;
        case 'b':
            if (checkKeyword(1, 3, "ool")) return T_BOOL;
            if (checkKeyword(1, 4, "reak")) return T_BREAK;
            break;
        case 'c':
            if (checkKeyword(1, 4, "atch")) return T_CATCH;
            if (checkKeyword(1, 3, "har")) return T_CHAR;
            if (checkKeyword(1, 4, "lass")) return T_CLASS;
            if (checkKeyword(1, 4, "onst")) return T_CONST;
            if (checkKeyword(1, 8, "onstruct")) return T_CONSTRUCT;
            if (checkKeyword(1, 7, "ontinue")) return T_CONTINUE;
            break;
        case 'd':
            if (checkKeyword(1, 4, "efer")) return T_DEFER;
            if (checkKeyword(1, 5, "elete")) return T_DELETE;
            if (checkKeyword(1, 7, "estruct")) return T_DESTRUCT;
            break;
        case 'e':
            if (checkKeyword(1, 3, "lse")) return T_ELSE;
            if (checkKeyword(1, 3, "num")) return T_ENUM;
            if (checkKeyword(1, 3, "xtends")) return T_EXTENDS;
            if (checkKeyword(1, 8, "xtension")) return T_EXTENSION;
            if (checkKeyword(1, 5, "xtern")) return T_EXTERN;
            break;
        case 'f':
            if (checkKeyword(1, 4, "alse")) return T_FALSE;
            if (checkKeyword(1, 6, "inally")) return T_FINALLY;
            if (checkKeyword(1, 4, "loat")) return T_FLOAT;
            if (checkKeyword(1, 5, "ouble")) return T_DOUBLE;
            if (checkKeyword(1, 2, "or")) return T_FOR;
            if (checkKeyword(1, 3, "unc")) return T_FUNC;
            break;
        case 'h':
            if (checkKeyword(1, 2, "as")) return T_HAS;
            break;
        case 'i':
            if (checkKeyword(1, 1, "f")) return T_IF;
            if (checkKeyword(1, 1, "n")) return T_IN;
            if (checkKeyword(1, 9, "nstanceof")) return T_INSTANCEOF;
            if (checkKeyword(1, 2, "nt")) return T_INT;
            if (checkKeyword(1, 3, "nt8")) return T_INT8;
            if (checkKeyword(1, 4, "nt16")) return T_INT16;
            if (checkKeyword(1, 4, "nt32")) return T_INT32;
            if (checkKeyword(1, 4, "nt64")) return T_INT64;
            if (checkKeyword(1, 1, "s")) return T_IS;
            break;
        case 'm':
            if (checkKeyword(1, 4, "atch")) return T_MATCH;
            break;
        case 'p':
            if (checkKeyword(1, 8, "rotolcol")) return T_PROTOCOL;
            if (checkKeyword(1, 5, "ublic")) return T_PUBLIC;
            break;
        case 'r':
            if (checkKeyword(1, 5, "eturn")) return T_RETURN;
            break;
        case 's':
            if (checkKeyword(1, 3, "elf")) return T_SELF;
            if (checkKeyword(1, 5, "izeof")) return T_SIZEOF;
            if (checkKeyword(1, 5, "tatic")) return T_STATIC;
            if (checkKeyword(1, 5, "truct")) return T_STRUCT;
            if (checkKeyword(1, 5, "witch")) return T_SWITCH;
            break;
        case 't':
            if (checkKeyword(1, 4, "hrow")) return T_THROW;
            if (checkKeyword(1, 4, "rait")) return T_TRAIT;
            if (checkKeyword(1, 3, "rue")) return T_TRUE;
            if (checkKeyword(1, 2, "ry")) return T_TRY;
            if (checkKeyword(1, 3, "ype")) return T_TYPE;
            if (checkKeyword(1, 5, "ypeof")) return T_TYPEOF;
            break;
        case 'u':
            if (checkKeyword(1, 3, "int")) return T_UINT;
            if (checkKeyword(1, 4, "int8")) return T_UINT8;
            if (checkKeyword(1, 5, "int16")) return T_UINT16;
            if (checkKeyword(1, 5, "int32")) return T_UINT32;
            if (checkKeyword(1, 5, "int64")) return T_UINT64;
            if (checkKeyword(1, 2, "se")) return T_USE;
            break;
        case 'v':
            if (checkKeyword(1, 2, "ar")) return T_VAR;
            break;
        case 'w':
            if (checkKeyword(1, 4, "here")) return T_WHERE;
            if (checkKeyword(1, 4, "hile")) return T_WHILE;
            break;
        case 'y':
            if (checkKeyword(1, 4, "ield")) return T_YIELD;
            break;
    }

    return T_IDENTIFIER;
}

static Token floatLiteral()
{
    while (isDigit(peek()) || (peek() == '_' && isDigit(next()))) {
        advance();
    }

    if (peek() == '.' && next() != '.') {
        advance();

        while (isDigit(peek()) || (peek() == '_' && isDigit(next()))) {
            advance();
        }
    }

    return makeToken(T_FLOAT_LITERAL);
}

static Token decimalLiteral()
{
    while (isDigit(peek()) || (peek() == '_' && isDigit(next()))) {
        advance();
    }

    if (peek() == '.' && next() != '.') {
        return floatLiteral();
    }

    return makeToken(T_DECIMAL_LITERAL);
}

static Token hexadecimalLiteral()
{
    while (isXDigit(peek()) || (peek() == '_' && isXDigit(next()))) {
        advance();
    }

    return makeToken(T_HEXADECIMAL_LITERAL);
}

static Token octalLiteral()
{
    while (isODigit(peek()) || (peek() == '_' && isODigit(next()))) {
        advance();
    }

    return makeToken(T_OCTAL_LITERAL);
}

static Token binaryLiteral()
{
    while (isBDigit(peek()) || (peek() == '_' && isBDigit(next()))) {
        advance();
    }

    return makeToken(T_BINARY_LITERAL);
}

static Token characterLiteral()
{
    while (!isEof()) {
        if (peek() == '\'' && prev() != '\\') {
            advance();
            return makeToken(T_CHARACTER_LITERAL);
        }

        advance();
    }

    error(characterError);
}

static Token stringLiteral(char c)
{
    while (!isEof()) {
        if (peek() == c && prev() != '\\') {
            advance();
            return makeToken(T_STRING_LITERAL);
        }

        advance();
    }

    errorc(stringError, c);
}

static Token identifier()
{
    while (isAlpha(peek()) || isDigit(peek())) {
        advance();
    }

    return makeToken(getIdentifierType());
}

Token scanToken()
{
    skipWhitespace();

    start.chars = current.chars;
    start.line = current.line;
    start.column = current.column;

    if (isEof()) {
        return makeToken(T_EOF);
    }

    char c = advance();

    if (isAlpha(c)) {
        return identifier();
    }

    if (c == '0') {
        if (isXDigit(next()) && (match('x') || match('X'))) return hexadecimalLiteral();
        if (isODigit(next()) && (match('o') || match('O'))) return octalLiteral();
        if (isBDigit(next()) && (match('b') || match('B'))) return binaryLiteral();

        return decimalLiteral();
    }

    if (isDigit(c)) {
        return decimalLiteral();
    }

    switch (c) {
        case '"':   return stringLiteral(c);
        case '`':   return stringLiteral(c);
        case '\'':  return characterLiteral();
        case '(':   return makeToken(T_LPAREN);
        case ')':   return makeToken(T_RPAREN);
        case '[':   return makeToken(T_LBRACE);
        case ']':   return makeToken(T_RBRACE);
        case '{':   return makeToken(T_LBRACE);
        case '}':   return makeToken(T_RBRACE);
        case ';':   return makeToken(T_SEMICOLON);
        case ',':   return makeToken(T_COMMA);
        case '$':   return makeToken(T_DOLLAR);
        case '~':   return makeToken(T_TILDE);
        case ':':   return makeToken(T_COLON);
        case '%':
            return makeToken(
                match('=') ? T_PERCENT_EQUAL : T_PERCENT);
        case '!':
            return makeToken(
                match('~') ? T_NOT_TILDE :
                match('=') ? T_NOT_EQUAL : T_EXCLAMATION);
        case '&':
            return makeToken(
                match('&') ? T_BOOLEAN_AND :
                match('=') ? T_AND_EQUAL : T_AMPERSAND);
        case '|':
            return makeToken(
                match('|') ? T_BOOLEAN_OR :
                match('>') ? T_PIPE_FORWARD :
                match('=') ? T_OR_EQUAL : T_PIPE);
        case '^':
            return makeToken(
                match('=') ? T_CIRCUMFLEX_EQUAL : T_CIRCUMFLEX);
        case '+':
            return makeToken(
                match('+') ? T_INCREMENT :
                match('=') ? T_PLUS_EQUAL : T_PLUS);
        case '-':
            if (isDigit(peek()) || peek() == '.') {
                return decimalLiteral();
            }
            return makeToken(
                match('-') ? T_DECREMENT :
                match('=') ? T_MINUS_EQUAL : T_MINUS);
        case '*':
            return makeToken(
                match('*') ?
                match('=') ? T_POWER_EQUAL : T_POWER :
                match('=') ? T_STAR_EQUAL : T_STAR);
        case '/':
            return makeToken(
                match('/') ?
                match('=') ? T_FLOOR_EQUAL : T_FLOOR :
                match('=') ? T_SLASH_EQUAL : T_SLASH);
        case '<':
            return makeToken(
                match('<') ?
                match('=') ? T_LSHIFT_EQUAL : T_LSHIFT :
                match('|') ? T_PIPE_BACKWARD :
                match('=') ?
                match('>') ? T_SPACESHIP : T_LESS_EQUAL : T_LESS);
        case '>':
            return makeToken(
                match('>') ?
                match('=') ? T_RSHIFT_EQUAL : T_RSHIFT :
                match('=') ? T_GREATER_EQUAL : T_GREATER);
        case '=':
            return makeToken(
                match('=') ? T_EQUAL_EQUAL :
                match('~') ? T_EQUAL_TILDE :
                match('>') ? T_LAMBDA : T_EQUAL);
        case '?':
            return makeToken(
                match('?') ?
                match('=') ? T_COALESCE_EQUAL : T_COALESCE :
                match(':') ? T_TERNARY :
                match('.') ? T_SAFE_ACCESS : T_QUESTION);
        case '.':
            if (isDigit(peek())) {
                return floatLiteral();
            }
            return makeToken(
                match('.') ?
                match('.') ? T_SPREAD :
                match('^') ? T_RANGE_FROM_END : T_RANGE : T_DOT);
        case '@':
            return makeToken(T_AT);
    }

    return makeToken(T_UNKNOWN);
}
