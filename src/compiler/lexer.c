#include "compiler/lexer.h"
#include "compiler/literal.h"
#include "compiler/token.h"
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

static bool skipWhitespace(Lexer* lexer);

static const Keyword keywords[] = {
    {"alignof",     7, TOKEN_ALIGNOF},
    {"and",         3, TOKEN_AND},
    {"as",          2, TOKEN_AS},
    {"await",       5, TOKEN_AWAIT},
    {"bool",        4, TOKEN_BOOL},
    {"break",       5, TOKEN_BREAK},
    {"catch",       5, TOKEN_CATCH},
    {"case",        4, TOKEN_CASE},
    {"char",        4, TOKEN_CHAR},
    {"class",       5, TOKEN_CLASS},
    {"const",       5, TOKEN_CONST},
    {"continue",    8, TOKEN_CONTINUE},
    {"defer",       5, TOKEN_DEFER},
    {"default",     7, TOKEN_DEFAULT},
    {"else",        4, TOKEN_ELSE},
    {"end",         3, TOKEN_END},
    {"enum",        4, TOKEN_ENUM},
    {"external",    8, TOKEN_EXTERNAL},
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

static void unterminatedLiteralError(Position start, char c)
{
    fprintf(stderr, "Error: Missing terminating %c character", c);
    fprintf(stderr, " on line %d:%d\n", start.line, start.column);
    exit(1);
}

static void invalidNumberError(Position start)
{
    fprintf(stderr, "Error: Invalid number literal");
    fprintf(stderr, " on line %d:%d\n", start.line, start.column);
    exit(1);
}

static void emptyCharacterError(Position start)
{
    fprintf(stderr, "Error: Empty character literal");
    fprintf(stderr, " on line %d:%d\n", start.line, start.column);
    exit(1);
}

static void invalidCharacterEscapeError(Position start)
{
    fprintf(stderr, "Error: Invalid character escape on line %d:%d\n", start.line, start.column);
    exit(1);
}

static void multipleCharacterError(Position start)
{
    fprintf(stderr, "Error: Character literal contains multiple characters");
    fprintf(stderr, " on line %d:%d\n", start.line, start.column);
    exit(1);
}

static void newlineCharacterError(Position start)
{
    fprintf(stderr, "Error: Newline in character literal");
    fprintf(stderr, " on line %d:%d\n", start.line, start.column);
    exit(1);
}

static void invalidUtf8CharacterError(Position start)
{
    fprintf(stderr, "Error: Invalid UTF-8 character literal");
    fprintf(stderr, " on line %d:%d\n", start.line, start.column);
    exit(1);
}

static void unescapedInterpolationBraceError(Position start)
{
    fprintf(stderr, "Error: Unescaped } in interpolated string on line %d:%d\n", start.line, start.column);
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

static bool isKeyword(const Lexer* lexer, const Keyword* keyword, size_t length)
{
    return length == keyword->length && memcmp(lexer->start.chars, keyword->chars, length) == 0;
}

static bool isAlpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

static bool isNewline(char c)
{
    return c == '\r' || c == '\n';
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
            invalidNumberError(lexer->start);
        }
    }
}

static void validateNumberEnd(Lexer* lexer)
{
    bool adjacentDecimal = peek(lexer) == '.' && isDigit(next(lexer));

    if (isAlpha(peek(lexer)) || isDigit(peek(lexer)) || peek(lexer) == '_'
        || adjacentDecimal) {
        invalidNumberError(lexer->start);
    }
}

static void skipCommentSingle(Lexer* lexer)
{
    advance(lexer);
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
        if (peek(lexer) == '*' && next(lexer) == '/') {
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
    
    if (next(lexer) == '*') {
        return skipCommentMulti(lexer);
    }

    skipCommentSingle(lexer);

    return true;
}

static bool skipWhitespaceComment(Lexer* lexer)
{
    if (next(lexer) != '/' && next(lexer) != '*') {
        return true;
    }

    if (!skipComment(lexer)) {
        return false;
    }

    return skipWhitespace(lexer);
}

static bool skipWhitespace(Lexer* lexer)
{
    while (1) {
        switch (peek(lexer)) {
            case '/':
                return skipWhitespaceComment(lexer);
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
        invalidNumberError(lexer->start);
    }

    scanDigits(lexer, isDigit);
}

static Token finishFloatLiteral(Lexer* lexer)
{
    if (peek(lexer) == 'e' || peek(lexer) == 'E') {
        scanExponent(lexer);
    }

    validateNumberEnd(lexer);

    return makeToken(lexer, TOKEN_FLOAT_LITERAL);
}

static Token scanFloatLiteral(Lexer* lexer)
{
    if (peek(lexer) == '.' && next(lexer) != '.') {
        advance(lexer);
        scanDigits(lexer, isDigit);
    }

    return finishFloatLiteral(lexer);
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
        invalidNumberError(lexer->start);
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

    if (isNewline(peek(lexer))) {
        newlineCharacterError(lexer->start);
    }

    if (isEndOfFile(lexer)) {
        unterminatedLiteralError(lexer->start, '\'');
    }

    multipleCharacterError(lexer->start);

    return makeToken(lexer, TOKEN_UNKNOWN);
}

static size_t characterSourceLength(const char* chars)
{
    size_t length = 0;

    while (length < LITERAL_SCALAR_MAX_LENGTH && chars[length] != '\0') {
        length++;
    }

    return length;
}

static Token escapedCharacterLiteral(Lexer* lexer)
{
    advance(lexer);

    if (isEndOfFile(lexer)) {
        unterminatedLiteralError(lexer->start, '\'');
    }

    if (isNewline(*lexer->current.chars)) {
        newlineCharacterError(lexer->start);
    }

    const char* chars = lexer->current.chars - 1;
    size_t length = characterSourceLength(chars);
    DecodedScalar scalar;
    bool valid = decodeLiteralScalar(chars, length, &scalar);

    if (!valid) {
        invalidCharacterEscapeError(lexer->start);
    }

    advanceCharacters(lexer, scalar.length - 1);

    return endCharacterLiteral(lexer);
}

static Token scanCharacterLiteral(Lexer* lexer)
{
    if (isEndOfFile(lexer)) {
        unterminatedLiteralError(lexer->start, '\'');
    }

    if (isNewline(peek(lexer))) {
        newlineCharacterError(lexer->start);
    }

    if (peek(lexer) == '\'') {
        emptyCharacterError(lexer->start);
    }

    if (peek(lexer) == '\\') {
        return escapedCharacterLiteral(lexer);
    }

    const char* chars = lexer->current.chars;
    size_t length = characterSourceLength(chars);
    DecodedScalar scalar;
    bool valid = decodeLiteralScalar(chars, length, &scalar);

    if (!valid) {
        invalidUtf8CharacterError(lexer->start);
    }

    advanceCharacters(lexer, scalar.length);

    return endCharacterLiteral(lexer);
}

static size_t delimiterLength(const char* chars, char delimiter)
{
    size_t count = 0;

    while (chars[count] == delimiter) {
        count++;
    }

    return count;
}

static bool scanInterpolationExpression(Lexer* lexer)
{
    size_t depth = 1;

    while (depth > 0) {
        Token token = scanToken(lexer);

        if (token.type == TOKEN_EOF || token.type == TOKEN_INCOMPLETE_INPUT) {
            return false;
        }

        if (token.type == TOKEN_LBRACE) {
            depth++;
        }

        if (token.type == TOKEN_RBRACE) {
            depth--;
        }
    }

    return true;
}

static void scanStringEscape(Lexer* lexer)
{
    const char* chars = lexer->current.chars - 1;
    size_t length = characterSourceLength(chars);
    DecodedScalar scalar;
    bool valid = decodeLiteralScalar(chars, length, &scalar);

    if (!valid) {
        advance(lexer);

        return;
    }

    advanceCharacters(lexer, scalar.length - 1);
}

static size_t scanStringDelimiter(Lexer* lexer, char delimiter)
{
    size_t run = delimiterLength(lexer->current.chars, delimiter) + 1;

    if (run < 3) {
        return 1;
    }

    advanceCharacters(lexer, run - 1);

    return run;
}

static bool scanStringInterpolation(Lexer* lexer, char c, Position start)
{
    if (peek(lexer) == c) {
        advance(lexer);

        return true;
    }

    if (c == '}') {
        unescapedInterpolationBraceError(start);
    }

    return scanInterpolationExpression(lexer);
}

static bool scanStringEnd(Lexer* lexer, char delimiter, size_t width)
{
    size_t closing = delimiterLength(lexer->current.chars, delimiter);

    if (closing < width) {
        return false;
    }

    advanceCharacters(lexer, width);

    return true;
}

static void validateStringNewline(Lexer* lexer, char delimiter, size_t width, Position start)
{
    char c = peek(lexer);

    if (delimiter != '"' || width != 1 || !isNewline(c)) {
        return;
    }

    lexer->start = start;

    unterminatedLiteralError(lexer->start, delimiter);
}

static bool scanStringCharacter(Lexer* lexer, char delimiter, size_t width, bool interpolated, Position start)
{
    validateStringNewline(lexer, delimiter, width, start);

    char c = advance(lexer);

    if (delimiter == '"' && c == '\\' && !isEndOfFile(lexer)) {
        scanStringEscape(lexer);

        return true;
    }

    if (!interpolated || (c != '{' && c != '}')) {
        return true;
    }

    return scanStringInterpolation(lexer, c, start);
}

static bool scanStringContents(Lexer* lexer, char delimiter, size_t width, bool interpolated)
{
    Position start = lexer->start;

    while (!isEndOfFile(lexer)) {
        if (scanStringEnd(lexer, delimiter, width)) {
            return true;
        }

        if (!scanStringCharacter(lexer, delimiter, width, interpolated, start)) {
            return false;
        }
    }

    return false;
}

static Token incompleteStringLiteral(Lexer* lexer, char delimiter, size_t width, bool interpolated)
{
    if (width >= 3 || delimiter == '`' || interpolated) {
        return makeToken(lexer, TOKEN_INCOMPLETE_INPUT);
    }

    unterminatedLiteralError(lexer->start, delimiter);

    return makeToken(lexer, TOKEN_UNKNOWN);
}

static Token scanStringLiteral(Lexer* lexer, char delimiter, bool interpolated)
{
    Position start = lexer->start;
    size_t width = scanStringDelimiter(lexer, delimiter);
    bool complete = scanStringContents(lexer, delimiter, width, interpolated);

    lexer->start = start;

    if (complete) {
        return makeToken(lexer, TOKEN_STRING_LITERAL);
    }

    return incompleteStringLiteral(lexer, delimiter, width, interpolated);
}

static Token scanInterpolatedString(Lexer* lexer)
{
    char delimiter = peek(lexer);

    if (delimiter != '"' && delimiter != '`') {
        return makeToken(lexer, TOKEN_DOLLAR);
    }

    advance(lexer);

    return scanStringLiteral(lexer, delimiter, true);
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
        case '`':   return scanStringLiteral(lexer, c, false);
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
        case '$':   return scanInterpolatedString(lexer);
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
            return makeToken(lexer, match(lexer, '=') ? TOKEN_SLASH_EQUAL : TOKEN_SLASH);
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
                scanDigits(lexer, isDigit);

                return finishFloatLiteral(lexer);
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
