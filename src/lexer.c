#include "lexer.h"
#include "token.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Keyword
{
    const char* chars;
    size_t length;
    TokenType type;
} Keyword;

static const Keyword keywords[] = {
    {"alignof",     7, TOKEN_ALIGNOF},
    {"and",         3, TOKEN_AND},
    {"as",          2, TOKEN_AS},
    {"await",       5, TOKEN_AWAIT},
    {"bool",        4, TOKEN_BOOL},
    {"break",       5, TOKEN_BREAK},
    {"catch",       5, TOKEN_CATCH},
    {"char",        4, TOKEN_CHAR},
    {"class",       5, TOKEN_CLASS},
    {"const",       5, TOKEN_CONST},
    {"continue",    8, TOKEN_CONTINUE},
    {"defer",       5, TOKEN_DEFER},
    {"else",        4, TOKEN_ELSE},
    {"end",         3, TOKEN_END},
    {"enum",        4, TOKEN_ENUM},
    {"false",       5, TOKEN_FALSE},
    {"finally",     7, TOKEN_FINALLY},
    {"f32",         3, TOKEN_F32},
    {"f64",         3, TOKEN_F64},
    {"for",         3, TOKEN_FOR},
    {"func",        4, TOKEN_FUNC},
    {"get",         3, TOKEN_GET},
    {"if",          2, TOKEN_IF},
    {"in",          2, TOKEN_IN},
    {"i8",          2, TOKEN_I8},
    {"i16",         3, TOKEN_I16},
    {"i32",         3, TOKEN_I32},
    {"i64",         3, TOKEN_I64},
    {"internal",    8, TOKEN_INTERNAL},
    {"is",          2, TOKEN_IS},
    {"let",         3, TOKEN_LET},
    {"match",       5, TOKEN_MATCH},
    {"not",         3, TOKEN_NOT},
    {"or",          2, TOKEN_OR},
    {"private",     7, TOKEN_PRIVATE},
    {"protocol",    8, TOKEN_PROTOCOL},
    {"public",      6, TOKEN_PUBLIC},
    {"return",      6, TOKEN_RETURN},
    {"self",        4, TOKEN_SELF},
    {"set",         3, TOKEN_SET},
    {"sizeof",      6, TOKEN_SIZEOF},
    {"spawn",       5, TOKEN_SPAWN},
    {"static",      6, TOKEN_STATIC},
    {"string",      6, TOKEN_STRING},
    {"struct",      6, TOKEN_STRUCT},
    {"throw",       5, TOKEN_THROW},
    {"true",        4, TOKEN_TRUE},
    {"try",         3, TOKEN_TRY},
    {"type",        4, TOKEN_TYPE},
    {"typeof",      6, TOKEN_TYPEOF},
    {"u8",          2, TOKEN_U8},
    {"u16",         3, TOKEN_U16},
    {"u32",         3, TOKEN_U32},
    {"u64",         3, TOKEN_U64},
    {"use",         3, TOKEN_USE},
    {"var",         3, TOKEN_VAR},
    {"void",        4, TOKEN_VOID},
    {"where",       5, TOKEN_WHERE},
    {"while",       5, TOKEN_WHILE},
    {"yield",       5, TOKEN_YIELD}
};

static void unterminatedLiteralError(const Lexer* lexer, char c)
{
    fprintf(stderr, "Error: Missing terminating %c character", c);
    fprintf(stderr, " on line %d:%d\n", lexer->start.line, lexer->start.column);
    exit(1);
}

static void invalidNumberError(const Lexer* lexer)
{
    fprintf(stderr, "Error: Invalid number literal");
    fprintf(stderr, " on line %d:%d\n", lexer->start.line, lexer->start.column);
    exit(1);
}

static void emptyCharacterError(const Lexer* lexer)
{
    fprintf(stderr, "Error: Empty character literal");
    fprintf(stderr, " on line %d:%d\n", lexer->start.line, lexer->start.column);
    exit(1);
}

static void multipleCharacterError(const Lexer* lexer)
{
    fprintf(stderr, "Error: Character literal contains multiple characters");
    fprintf(stderr, " on line %d:%d\n", lexer->start.line, lexer->start.column);
    exit(1);
}

static void newlineCharacterError(const Lexer* lexer)
{
    fprintf(stderr, "Error: Newline in character literal");
    fprintf(stderr, " on line %d:%d\n", lexer->start.line, lexer->start.column);
    exit(1);
}

static void invalidUtf8CharacterError(const Lexer* lexer)
{
    fprintf(stderr, "Error: Invalid UTF-8 character literal");
    fprintf(stderr, " on line %d:%d\n", lexer->start.line, lexer->start.column);
    exit(1);
}

static char peek(const Lexer* lexer)
{
    return *lexer->current.chars;
}

static char next(const Lexer* lexer)
{
    if (*lexer->current.chars == '\0') {
        return '\0';
    }

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

static bool isEndOfFile(const Lexer* lexer)
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

static Token makeToken(const Lexer* lexer, TokenType type)
{
    Token token;
    token.type = type;
    token.length = (size_t)(lexer->current.chars - lexer->start.chars);
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

static bool isUtf8Continuation(char c)
{
    return ((unsigned char)c & 0xC0) == 0x80;
}

static bool hasUtf8Continuations(const char* chars, size_t count)
{
    for (size_t i = 1; i <= count; i++) {
        if (chars[i] == '\0') {
            return false;
        }

        if (!isUtf8Continuation(chars[i])) {
            return false;
        }
    }

    return true;
}

static size_t utf8CodePointLength(const char* chars)
{
    unsigned char first = (unsigned char)chars[0];
    unsigned char second = (unsigned char)chars[1];

    if (first <= 0x7F) {
        return 1;
    }

    if (first >= 0xC2 && first <= 0xDF && hasUtf8Continuations(chars, 1)) {
        return 2;
    }

    if (first == 0xE0 && second >= 0xA0 && second <= 0xBF && hasUtf8Continuations(chars, 2)) {
        return 3;
    }

    if (first >= 0xE1 && first <= 0xEC && hasUtf8Continuations(chars, 2)) {
        return 3;
    }

    if (first == 0xED && second >= 0x80 && second <= 0x9F && hasUtf8Continuations(chars, 2)) {
        return 3;
    }

    if (first >= 0xEE && first <= 0xEF && hasUtf8Continuations(chars, 2)) {
        return 3;
    }

    if (first == 0xF0 && second >= 0x90 && second <= 0xBF && hasUtf8Continuations(chars, 3)) {
        return 4;
    }

    if (first >= 0xF1 && first <= 0xF3 && hasUtf8Continuations(chars, 3)) {
        return 4;
    }

    if (first == 0xF4 && second >= 0x80 && second <= 0x8F && hasUtf8Continuations(chars, 3)) {
        return 4;
    }

    return 0;
}

static void scanDigits(Lexer* lexer, bool (*isValidDigit)(char))
{
    while (true) {
        if (isValidDigit(peek(lexer))) {
            advance(lexer);
            continue;
        }

        if (peek(lexer) != '_') {
            return;
        }

        advance(lexer);

        if (!isValidDigit(peek(lexer))) {
            invalidNumberError(lexer);
        }
    }
}

static void validateNumberEnd(Lexer* lexer)
{
    if (isAlpha(peek(lexer)) || isDigit(peek(lexer)) || peek(lexer) == '_') {
        invalidNumberError(lexer);
    }
}

static void skipCommentSingle(Lexer* lexer)
{
    advance(lexer);
    
    while (!isEndOfFile(lexer) && peek(lexer) != '\n') {
        advance(lexer);
    }
}

static bool skipCommentMulti(Lexer* lexer)
{
    advance(lexer);
    advance(lexer);

    while (!isEndOfFile(lexer)) {
        if (peek(lexer) == '#' && next(lexer) == '#') {
            advance(lexer);
            advance(lexer);
            return true;
        }
        
        advance(lexer);
    }

    return false;
}

static bool skipComment(Lexer* lexer)
{
    lexer->start.chars = lexer->current.chars;
    lexer->start.line = lexer->current.line;
    lexer->start.column = lexer->current.column;
    
    if (next(lexer) == '#') {
        return skipCommentMulti(lexer);
    }

    skipCommentSingle(lexer);

    return true;
}

static bool skipWhitespace(Lexer* lexer)
{
    while (1) {
        switch (peek(lexer)) {
            case '#':
                if (!skipComment(lexer)) {
                    return false;
                }
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
                return true;
        }
    }
}

static bool isKeyword(const Lexer* lexer, const Keyword* keyword, size_t length)
{
    return length == keyword->length &&
        memcmp(lexer->start.chars, keyword->chars, length) == 0;
}

static TokenType getIdentifierType(const Lexer* lexer)
{
    size_t length = (size_t)(lexer->current.chars - lexer->start.chars);
    size_t keywordCount = sizeof(keywords) / sizeof(keywords[0]);

    for (size_t i = 0; i < keywordCount; i++) {
        if (!isKeyword(lexer, &keywords[i], length)) {
            continue;
        }

        return keywords[i].type;
    }

    return TOKEN_IDENTIFIER;
}

static void scanExponent(Lexer* lexer)
{
    advance(lexer);

    if (peek(lexer) == '+' || peek(lexer) == '-') {
        advance(lexer);
    }

    if (!isDigit(peek(lexer))) {
        invalidNumberError(lexer);
    }

    scanDigits(lexer, isDigit);
}

static Token scanFloatLiteral(Lexer* lexer)
{
    if (peek(lexer) == '.' && next(lexer) != '.') {
        advance(lexer);
        scanDigits(lexer, isDigit);
    }

    if (peek(lexer) == 'e' || peek(lexer) == 'E') {
        scanExponent(lexer);
    }

    validateNumberEnd(lexer);

    return makeToken(lexer, TOKEN_F32_LITERAL);
}

static Token scanIntegerLiteral(Lexer* lexer)
{
    scanDigits(lexer, isDigit);

    if (peek(lexer) == '.' && next(lexer) != '.') {
        return scanFloatLiteral(lexer);
    }

    if (peek(lexer) == 'e' || peek(lexer) == 'E') {
        return scanFloatLiteral(lexer);
    }

    validateNumberEnd(lexer);

    return makeToken(lexer, TOKEN_INTEGER_LITERAL);
}

static Token scanPrefixedIntegerLiteral(Lexer* lexer, bool (*isValidDigit)(char), TokenType type)
{
    if (!isValidDigit(peek(lexer))) {
        invalidNumberError(lexer);
    }

    scanDigits(lexer, isValidDigit);
    validateNumberEnd(lexer);

    return makeToken(lexer, type);
}

static Token scanZeroLiteral(Lexer* lexer)
{
    if (match(lexer, 'x') || match(lexer, 'X')) {
        return scanPrefixedIntegerLiteral(lexer, isXDigit, TOKEN_HEXADECIMAL_LITERAL);
    }

    if (match(lexer, 'o') || match(lexer, 'O')) {
        return scanPrefixedIntegerLiteral(lexer, isODigit, TOKEN_OCTAL_LITERAL);
    }

    if (match(lexer, 'b') || match(lexer, 'B')) {
        return scanPrefixedIntegerLiteral(lexer, isBDigit, TOKEN_BINARY_LITERAL);
    }

    return scanIntegerLiteral(lexer);
}

static void advanceCharacters(Lexer* lexer, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        advance(lexer);
    }
}

static Token endCharacterLiteral(Lexer* lexer)
{
    if (match(lexer, '\'')) {
        return makeToken(lexer, TOKEN_CHARACTER_LITERAL);
    }

    if (peek(lexer) == '\n') {
        newlineCharacterError(lexer);
    }

    if (isEndOfFile(lexer)) {
        unterminatedLiteralError(lexer, '\'');
    }

    multipleCharacterError(lexer);

    return makeToken(lexer, TOKEN_UNKNOWN);
}

static Token escapedCharacterLiteral(Lexer* lexer)
{
    advance(lexer);

    if (isEndOfFile(lexer)) {
        unterminatedLiteralError(lexer, '\'');
    }

    if (peek(lexer) == '\n') {
        newlineCharacterError(lexer);
    }

    advance(lexer);

    return endCharacterLiteral(lexer);
}

static Token scanCharacterLiteral(Lexer* lexer)
{
    size_t length;

    if (isEndOfFile(lexer)) {
        unterminatedLiteralError(lexer, '\'');
    }

    if (peek(lexer) == '\n') {
        newlineCharacterError(lexer);
    }

    if (peek(lexer) == '\'') {
        emptyCharacterError(lexer);
    }

    if (peek(lexer) == '\\') {
        return escapedCharacterLiteral(lexer);
    }

    length = utf8CodePointLength(lexer->current.chars);

    if (length == 0) {
        invalidUtf8CharacterError(lexer);
    }

    advanceCharacters(lexer, length);

    return endCharacterLiteral(lexer);
}

static Token scanStringLiteral(Lexer* lexer, char delimiter, TokenType type)
{
    bool escaped = false;

    while (!isEndOfFile(lexer)) {
        char c = advance(lexer);

        if (c == delimiter && !escaped) {
            return makeToken(lexer, type);
        }

        if (c == '\\') {
            escaped = !escaped;
        } else {
            escaped = false;
        }
    }

    unterminatedLiteralError(lexer, delimiter);
    
    return makeToken(lexer, TOKEN_UNKNOWN);
}

static Token scanIdentifier(Lexer* lexer)
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
    if (!skipWhitespace(lexer)) {
        return makeToken(lexer, TOKEN_INCOMPLETE_INPUT);
    }

    lexer->start.chars = lexer->current.chars;
    lexer->start.line = lexer->current.line;
    lexer->start.column = lexer->current.column;

    if (isEndOfFile(lexer)) {
        return makeToken(lexer, TOKEN_EOF);
    }

    char c = advance(lexer);

    if (c == '0') {
        return scanZeroLiteral(lexer);
    }

    if (isDigit(c)) {
        return scanIntegerLiteral(lexer);
    }

    if (isAlpha(c)) {
        return scanIdentifier(lexer);
    }

    switch (c) {
        case '"':
        case '`':   return scanStringLiteral(lexer, c, TOKEN_STRING_LITERAL);
        case '\'':  return scanCharacterLiteral(lexer);
        case '(':   return makeToken(lexer, TOKEN_LPAREN);
        case ')':   return makeToken(lexer, TOKEN_RPAREN);
        case '[':   return makeToken(lexer, TOKEN_LSQUARE);
        case ']':   return makeToken(lexer, TOKEN_RSQUARE);
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
                match(lexer, '=') ? TOKEN_NOT_EQUAL_EQUAL : TOKEN_NOT_EQUAL : TOKEN_UNKNOWN);
        case '&':
            return makeToken(lexer, 
                match(lexer, '&') ? TOKEN_UNKNOWN :
                match(lexer, '=') ? TOKEN_AND_EQUAL : TOKEN_AMPERSAND);
        case '|':
            return makeToken(lexer, 
                match(lexer, '|') ? TOKEN_UNKNOWN :
                match(lexer, '>') ? TOKEN_PIPE_FORWARD :
                match(lexer, '=') ? TOKEN_OR_EQUAL : TOKEN_PIPE);
        case '^':
            return makeToken(lexer, 
                match(lexer, '=') ? TOKEN_CIRCUMFLEX_EQUAL : TOKEN_CIRCUMFLEX);
        case '+':
            return makeToken(lexer, 
                match(lexer, '=') ? TOKEN_PLUS_EQUAL : TOKEN_PLUS);
        case '-':
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
                return scanFloatLiteral(lexer);
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
