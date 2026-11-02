#include "compiler/lexer.h"
#include "compiler/diagnostics.h"
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

static bool skip_whitespace(Lexer* lexer);
static Token scan_discovery_token(Lexer* lexer);
static void skip_discovery_interpolation(Lexer* lexer);

static const Keyword keywords[] = {
    {"alignof",     7, TokenKeywordAlignof},
    {"and",         3, TokenKeywordAnd},
    {"as",          2, TokenKeywordAs},
    {"await",       5, TokenKeywordAwait},
    {"bool",        4, TokenKeywordBool},
    {"break",       5, TokenKeywordBreak},
    {"catch",       5, TokenKeywordCatch},
    {"case",        4, TokenKeywordCase},
    {"char",        4, TokenKeywordChar},
    {"class",       5, TokenKeywordClass},
    {"const",       5, TokenKeywordConst},
    {"continue",    8, TokenKeywordContinue},
    {"defer",       5, TokenKeywordDefer},
    {"default",     7, TokenKeywordDefault},
    {"else",        4, TokenKeywordElse},
    {"end",         3, TokenKeywordEnd},
    {"enum",        4, TokenKeywordEnum},
    {"external",    8, TokenKeywordExternal},
    {"false",       5, TokenKeywordFalse},
    {"finally",     7, TokenKeywordFinally},
    {"f32",         3, TokenKeywordF32},
    {"f64",         3, TokenKeywordF64},
    {"for",         3, TokenKeywordFor},
    {"func",        4, TokenKeywordFunc},
    {"get",         3, TokenKeywordGet},
    {"if",          2, TokenKeywordIf},
    {"in",          2, TokenKeywordIn},
    {"i8",          2, TokenKeywordI8},
    {"i16",         3, TokenKeywordI16},
    {"i32",         3, TokenKeywordI32},
    {"i64",         3, TokenKeywordI64},
    {"internal",    8, TokenKeywordInternal},
    {"is",          2, TokenKeywordIs},
    {"isize",       5, TokenKeywordIsize},
    {"let",         3, TokenKeywordLet},
    {"match",       5, TokenKeywordMatch},
    {"not",         3, TokenKeywordNot},
    {"or",          2, TokenKeywordOr},
    {"private",     7, TokenKeywordPrivate},
    {"protocol",    8, TokenKeywordProtocol},
    {"public",      6, TokenKeywordPublic},
    {"return",      6, TokenKeywordReturn},
    {"self",        4, TokenKeywordSelf},
    {"set",         3, TokenKeywordSet},
    {"sizeof",      6, TokenKeywordSizeof},
    {"spawn",       5, TokenKeywordSpawn},
    {"static",      6, TokenKeywordStatic},
    {"string",      6, TokenKeywordString},
    {"struct",      6, TokenKeywordStruct},
    {"test",        4, TokenKeywordTest},
    {"throw",       5, TokenKeywordThrow},
    {"true",        4, TokenKeywordTrue},
    {"try",         3, TokenKeywordTry},
    {"type",        4, TokenKeywordType},
    {"typeof",      6, TokenKeywordTypeof},
    {"u8",          2, TokenKeywordU8},
    {"u16",         3, TokenKeywordU16},
    {"u32",         3, TokenKeywordU32},
    {"u64",         3, TokenKeywordU64},
    {"use",         3, TokenKeywordUse},
    {"usize",       5, TokenKeywordUsize},
    {"var",         3, TokenKeywordVar},
    {"void",        4, TokenKeywordVoid},
    {"while",       5, TokenKeywordWhile},
    {"yield",       5, TokenKeywordYield}
};

static _Noreturn void empty_character_error(SourcePosition start)
{
    fatal_source_error(start.line, start.column, "Empty character literal");
}

static _Noreturn void invalid_character_escape_error(SourcePosition start)
{
    fatal_source_error(start.line, start.column, "Invalid character escape");
}

static _Noreturn void invalid_number_error(SourcePosition start)
{
    fatal_source_error(start.line, start.column, "Invalid number literal");
}

static _Noreturn void invalid_utf8_character_error(SourcePosition start)
{
    fatal_source_error(start.line, start.column, "Invalid UTF-8 character literal");
}

static _Noreturn void multiple_character_error(SourcePosition start)
{
    fatal_source_error(start.line, start.column, "Character literal contains multiple characters");
}

static _Noreturn void newline_character_error(SourcePosition start)
{
    fatal_source_error(start.line, start.column, "Newline in character literal");
}

static _Noreturn void unescaped_interpolation_brace_error(SourcePosition start)
{
    fatal_source_error(start.line, start.column, "Unescaped } in interpolated string");
}

static _Noreturn void unterminated_literal_error(SourcePosition start, char c)
{
    fatal_source_error(start.line, start.column, "Missing terminating %c character", c);
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

static bool is_end_of_file(const Lexer* lexer)
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

static Token make_token(const Lexer* lexer, TokenType type)
{
    Token token;
    token.type = type;
    token.length = (size_t)(lexer->current.chars - lexer->start.chars);
    token.chars = lexer->start.chars;
    token.line = lexer->start.line;
    token.column = lexer->start.column;

    return token;
}

static bool is_keyword(const Lexer* lexer, const Keyword* keyword, size_t length)
{
    return length == keyword->length && memcmp(lexer->start.chars, keyword->chars, length) == 0;
}

static bool is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static bool is_newline(char c)
{
    return c == '\r' || c == '\n';
}

static bool is_x_digit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static bool is_o_digit(char c)
{
    return c >= '0' && c <= '7';
}

static bool is_b_digit(char c)
{
    return c == '0' || c == '1';
}

static void scan_digits(Lexer* lexer, bool (*is_valid_digit)(char))
{
    while (true) {
        if (is_valid_digit(peek(lexer))) {
            advance(lexer);
            continue;
        }

        if (peek(lexer) != '_') {
            return;
        }

        advance(lexer);

        if (!is_valid_digit(peek(lexer))) {
            invalid_number_error(lexer->start);
        }
    }
}

static void validate_number_end(Lexer* lexer)
{
    bool adjacent_decimal = peek(lexer) == '.' && is_digit(next(lexer));

    if (is_alpha(peek(lexer)) || is_digit(peek(lexer)) || peek(lexer) == '_'
        || adjacent_decimal) {
        invalid_number_error(lexer->start);
    }
}

static void skip_comment_single(Lexer* lexer)
{
    advance(lexer);
    advance(lexer);

    while (!is_end_of_file(lexer) && peek(lexer) != '\n') {
        advance(lexer);
    }
}

static bool skip_comment_multi(Lexer* lexer)
{
    advance(lexer);
    advance(lexer);

    while (!is_end_of_file(lexer)) {
        if (peek(lexer) == '*' && next(lexer) == '/') {
            advance(lexer);
            advance(lexer);
            return true;
        }

        advance(lexer);
    }

    return false;
}

static bool skip_comment(Lexer* lexer)
{
    lexer->start.chars = lexer->current.chars;
    lexer->start.line = lexer->current.line;
    lexer->start.column = lexer->current.column;

    if (next(lexer) == '*') {
        return skip_comment_multi(lexer);
    }

    skip_comment_single(lexer);

    return true;
}

static bool skip_whitespace_comment(Lexer* lexer)
{
    if (next(lexer) != '/' && next(lexer) != '*') {
        return true;
    }

    if (!skip_comment(lexer)) {
        return false;
    }

    return skip_whitespace(lexer);
}

static bool skip_whitespace(Lexer* lexer)
{
    while (1) {
        switch (peek(lexer)) {
            case '/':
                return skip_whitespace_comment(lexer);
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

static TokenType get_identifier_type(const Lexer* lexer)
{
    size_t length = (size_t)(lexer->current.chars - lexer->start.chars);
    size_t keyword_count = sizeof(keywords) / sizeof(keywords[0]);

    for (size_t i = 0; i < keyword_count; i++) {
        if (!is_keyword(lexer, &keywords[i], length)) {
            continue;
        }

        return keywords[i].type;
    }

    return TokenIdentifier;
}

static void scan_exponent(Lexer* lexer)
{
    advance(lexer);

    if (peek(lexer) == '+' || peek(lexer) == '-') {
        advance(lexer);
    }

    if (!is_digit(peek(lexer))) {
        invalid_number_error(lexer->start);
    }

    scan_digits(lexer, is_digit);
}

static Token finish_float_literal(Lexer* lexer)
{
    if (peek(lexer) == 'e' || peek(lexer) == 'E') {
        scan_exponent(lexer);
    }

    validate_number_end(lexer);

    return make_token(lexer, TokenFloatLiteral);
}

static Token scan_float_literal(Lexer* lexer)
{
    if (peek(lexer) == '.' && next(lexer) != '.') {
        advance(lexer);
        scan_digits(lexer, is_digit);
    }

    return finish_float_literal(lexer);
}

static Token scan_integer_literal(Lexer* lexer)
{
    scan_digits(lexer, is_digit);

    if (peek(lexer) == '.' && next(lexer) != '.') {
        return scan_float_literal(lexer);
    }

    if (peek(lexer) == 'e' || peek(lexer) == 'E') {
        return scan_float_literal(lexer);
    }

    validate_number_end(lexer);

    return make_token(lexer, TokenIntegerLiteral);
}

static Token scan_prefixed_integer_literal(Lexer* lexer, bool (*is_valid_digit)(char), TokenType type)
{
    if (!is_valid_digit(peek(lexer))) {
        invalid_number_error(lexer->start);
    }

    scan_digits(lexer, is_valid_digit);
    validate_number_end(lexer);

    return make_token(lexer, type);
}

static Token scan_zero_literal(Lexer* lexer)
{
    if (match(lexer, 'x') || match(lexer, 'X')) {
        return scan_prefixed_integer_literal(lexer, is_x_digit, TokenHexadecimalLiteral);
    }

    if (match(lexer, 'o') || match(lexer, 'O')) {
        return scan_prefixed_integer_literal(lexer, is_o_digit, TokenOctalLiteral);
    }

    if (match(lexer, 'b') || match(lexer, 'B')) {
        return scan_prefixed_integer_literal(lexer, is_b_digit, TokenBinaryLiteral);
    }

    return scan_integer_literal(lexer);
}

static void advance_characters(Lexer* lexer, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        advance(lexer);
    }
}

static Token end_character_literal(Lexer* lexer)
{
    if (match(lexer, '\'')) {
        return make_token(lexer, TokenCharacterLiteral);
    }

    if (is_newline(peek(lexer))) {
        newline_character_error(lexer->start);
    }

    if (is_end_of_file(lexer)) {
        unterminated_literal_error(lexer->start, '\'');
    }

    multiple_character_error(lexer->start);

    return make_token(lexer, TokenUnknown);
}

static size_t character_source_length(const char* chars)
{
    size_t length = 0;

    while (length < LITERAL_SCALAR_MAX_LENGTH && chars[length] != '\0') {
        length++;
    }

    return length;
}

static Token escaped_character_literal(Lexer* lexer)
{
    advance(lexer);

    if (is_end_of_file(lexer)) {
        unterminated_literal_error(lexer->start, '\'');
    }

    if (is_newline(*lexer->current.chars)) {
        newline_character_error(lexer->start);
    }

    const char* chars = lexer->current.chars - 1;
    size_t length = character_source_length(chars);
    DecodedScalar scalar;
    bool valid = decode_literal_scalar(chars, length, &scalar);

    if (!valid) {
        invalid_character_escape_error(lexer->start);
    }

    advance_characters(lexer, scalar.length - 1);

    return end_character_literal(lexer);
}

static Token scan_character_literal(Lexer* lexer)
{
    if (is_end_of_file(lexer)) {
        unterminated_literal_error(lexer->start, '\'');
    }

    if (is_newline(peek(lexer))) {
        newline_character_error(lexer->start);
    }

    if (peek(lexer) == '\'') {
        empty_character_error(lexer->start);
    }

    if (peek(lexer) == '\\') {
        return escaped_character_literal(lexer);
    }

    const char* chars = lexer->current.chars;
    size_t length = character_source_length(chars);
    DecodedScalar scalar;
    bool valid = decode_literal_scalar(chars, length, &scalar);

    if (!valid) {
        invalid_utf8_character_error(lexer->start);
    }

    advance_characters(lexer, scalar.length);

    return end_character_literal(lexer);
}

static size_t delimiter_length(const char* chars, char delimiter)
{
    size_t count = 0;

    while (chars[count] == delimiter) {
        count++;
    }

    return count;
}

static bool scan_interpolation_expression(Lexer* lexer)
{
    size_t depth = 1;

    while (depth > 0) {
        Token token = scan_token(lexer);

        if (token.type == TokenEof || token.type == TokenIncompleteInput) {
            return false;
        }

        if (token.type == TokenLbrace) {
            depth++;
        }

        if (token.type == TokenRbrace) {
            depth--;
        }
    }

    return true;
}

static void scan_string_escape(Lexer* lexer)
{
    const char* chars = lexer->current.chars - 1;
    size_t length = character_source_length(chars);
    DecodedScalar scalar;
    bool valid = decode_literal_scalar(chars, length, &scalar);

    if (!valid) {
        advance(lexer);

        return;
    }

    advance_characters(lexer, scalar.length - 1);
}

static size_t scan_string_delimiter(Lexer* lexer, char delimiter)
{
    size_t run = delimiter_length(lexer->current.chars, delimiter) + 1;

    if (run < 3) {
        return 1;
    }

    advance_characters(lexer, run - 1);

    return run;
}

static bool scan_string_interpolation(Lexer* lexer, char c, SourcePosition start)
{
    if (peek(lexer) == c) {
        advance(lexer);

        return true;
    }

    if (c == '}') {
        unescaped_interpolation_brace_error(start);
    }

    return scan_interpolation_expression(lexer);
}

static bool scan_string_end(Lexer* lexer, char delimiter, size_t width)
{
    size_t closing = delimiter_length(lexer->current.chars, delimiter);

    if (closing < width) {
        return false;
    }

    advance_characters(lexer, width);

    return true;
}

static void validate_string_newline(Lexer* lexer, char delimiter, size_t width, SourcePosition start)
{
    char c = peek(lexer);

    if (delimiter != '"' || width != 1 || !is_newline(c)) {
        return;
    }

    lexer->start = start;

    unterminated_literal_error(lexer->start, delimiter);
}

static bool scan_string_character(Lexer* lexer, char delimiter, size_t width, bool interpolated, SourcePosition start)
{
    validate_string_newline(lexer, delimiter, width, start);

    char c = advance(lexer);

    if (delimiter == '"' && c == '\\' && !is_end_of_file(lexer)) {
        scan_string_escape(lexer);

        return true;
    }

    if (!interpolated || (c != '{' && c != '}')) {
        return true;
    }

    return scan_string_interpolation(lexer, c, start);
}

static bool scan_string_contents(Lexer* lexer, char delimiter, size_t width, bool interpolated)
{
    SourcePosition start = lexer->start;

    while (!is_end_of_file(lexer)) {
        if (scan_string_end(lexer, delimiter, width)) {
            return true;
        }

        if (!scan_string_character(lexer, delimiter, width, interpolated, start)) {
            return false;
        }
    }

    return false;
}

static Token incomplete_string_literal(Lexer* lexer, char delimiter, size_t width, bool interpolated)
{
    if (width >= 3 || delimiter == '`' || interpolated) {
        return make_token(lexer, TokenIncompleteInput);
    }

    unterminated_literal_error(lexer->start, delimiter);

    return make_token(lexer, TokenUnknown);
}

static Token scan_string_literal(Lexer* lexer, char delimiter, bool interpolated)
{
    SourcePosition start = lexer->start;
    size_t width = scan_string_delimiter(lexer, delimiter);
    bool complete = scan_string_contents(lexer, delimiter, width, interpolated);

    lexer->start = start;

    if (complete) {
        return make_token(lexer, TokenStringLiteral);
    }

    return incomplete_string_literal(lexer, delimiter, width, interpolated);
}

static Token scan_interpolated_string(Lexer* lexer)
{
    char delimiter = peek(lexer);

    if (delimiter != '"' && delimiter != '`') {
        return make_token(lexer, TokenDollar);
    }

    advance(lexer);

    return scan_string_literal(lexer, delimiter, true);
}

static Token scan_identifier(Lexer* lexer)
{
    while (is_alpha(peek(lexer)) || is_digit(peek(lexer))) {
        advance(lexer);
    }

    return make_token(lexer, get_identifier_type(lexer));
}

void init_lexer(Lexer* lexer, const char* source)
{
    lexer->current.chars = source;
    lexer->current.line = 1;
    lexer->current.column = 1;
}

Token scan_token(Lexer* lexer)
{
    if (!skip_whitespace(lexer)) {
        return make_token(lexer, TokenIncompleteInput);
    }

    lexer->start.chars = lexer->current.chars;
    lexer->start.line = lexer->current.line;
    lexer->start.column = lexer->current.column;

    if (is_end_of_file(lexer)) {
        return make_token(lexer, TokenEof);
    }

    char c = advance(lexer);

    if (c == '0') {
        return scan_zero_literal(lexer);
    }

    if (is_digit(c)) {
        return scan_integer_literal(lexer);
    }

    if (is_alpha(c)) {
        return scan_identifier(lexer);
    }

    switch (c) {
        case '"':
        case '`':
            return scan_string_literal(lexer, c, false);
        case '\'':
            return scan_character_literal(lexer);
        case '(':
            return make_token(lexer, TokenLparen);
        case ')':
            return make_token(lexer, TokenRparen);
        case '[':
            return make_token(lexer, TokenLsquare);
        case ']':
            return make_token(lexer, TokenRsquare);
        case '{':
            return make_token(lexer, TokenLbrace);
        case '}':
            return make_token(lexer, TokenRbrace);
        case ':':
            return make_token(lexer, TokenColon);
        case ';':
            return make_token(lexer, TokenSemicolon);
        case ',':
            return make_token(lexer, TokenComma);
        case '$':
            return scan_interpolated_string(lexer);
        case '~':
            return make_token(lexer, TokenTilde);
        case '%':
            return make_token(lexer,
                match(lexer, '=') ? TokenPercentEqual : TokenPercent);
        case '=':
            return make_token(lexer,
                match(lexer, '~') ? TokenEqualTilde :
                match(lexer, '=') ?
                match(lexer, '=') ? TokenEqualEqualEqual : TokenEqualEqual : TokenEqual);
        case '!':
            return make_token(lexer,
                match(lexer, '~') ? TokenNotTilde :
                match(lexer, '=') ?
                match(lexer, '=') ? TokenNotEqualEqual : TokenNotEqual : TokenUnknown);
        case '&':
            return make_token(lexer,
                match(lexer, '&') ? TokenUnknown :
                match(lexer, '=') ? TokenAndEqual : TokenAmpersand);
        case '|':
            return make_token(lexer,
                match(lexer, '|') ? TokenUnknown :
                match(lexer, '>') ? TokenPipeForward :
                match(lexer, '=') ? TokenOrEqual : TokenPipe);
        case '^':
            return make_token(lexer,
                match(lexer, '=') ? TokenCircumflexEqual : TokenCircumflex);
        case '+':
            return make_token(lexer,
                match(lexer, '=') ? TokenPlusEqual : TokenPlus);
        case '-':
            return make_token(lexer,
                match(lexer, '>') ? TokenArrow :
                match(lexer, '=') ? TokenMinusEqual : TokenMinus);
        case '*':
            return make_token(lexer,
                match(lexer, '*') ?
                match(lexer, '=') ? TokenPowerEqual : TokenPower :
                match(lexer, '=') ? TokenStarEqual : TokenStar);
        case '/':
            return make_token(lexer, match(lexer, '=') ? TokenSlashEqual : TokenSlash);
        case '<':
            return make_token(lexer,
                match(lexer, '<') ?
                match(lexer, '=') ? TokenLshiftEqual : TokenLshift :
                match(lexer, '|') ? TokenPipeBackward :
                match(lexer, '=') ?
                match(lexer, '>') ? TokenSpaceship : TokenLessEqual : TokenLess);
        case '>':
            return make_token(lexer,
                match(lexer, '>') ?
                match(lexer, '=') ? TokenRshiftEqual : TokenRshift :
                match(lexer, '=') ? TokenGreaterEqual : TokenGreater);
        case '?':
            return make_token(lexer,
                match(lexer, '?') ?
                match(lexer, '=') ? TokenCoalesceEqual : TokenCoalesce :
                match(lexer, '.') ? TokenSafeAccess : TokenQuestion);
        case '.':
            if (is_digit(peek(lexer))) {
                scan_digits(lexer, is_digit);

                return finish_float_literal(lexer);
            }
            return make_token(lexer,
                match(lexer, '.') ?
                match(lexer, '.') ? TokenSpread :
                match(lexer, '^') ? TokenRangeFromEnd : TokenRange : TokenDot);
        case '@':
            return make_token(lexer, TokenAt);
        default:
            return make_token(lexer, TokenUnknown);
    }
}

static bool scan_discovery_literal_end(Lexer* lexer, char delimiter, size_t width)
{
    if (is_end_of_file(lexer) || scan_string_end(lexer, delimiter, width)) {
        return true;
    }

    bool single_line = delimiter != '`' && width == 1;
    char c = peek(lexer);

    return single_line && is_newline(c);
}

static void skip_discovery_interpolation_character(Lexer* lexer, char c)
{
    if (c != '{' && c != '}') {
        return;
    }

    if (peek(lexer) == c) {
        advance(lexer);

        return;
    }

    if (c == '{') {
        skip_discovery_interpolation(lexer);
    }
}

static bool skip_discovery_literal_character(Lexer* lexer, char delimiter, size_t width, bool interpolated)
{
    if (scan_discovery_literal_end(lexer, delimiter, width)) {
        return true;
    }

    char c = advance(lexer);

    if (c == '\\' && delimiter != '`' && !is_end_of_file(lexer)) {
        advance(lexer);

        return false;
    }

    if (interpolated) {
        skip_discovery_interpolation_character(lexer, c);
    }

    return false;
}

static void skip_discovery_literal(Lexer* lexer, char delimiter, bool interpolated)
{
    size_t width = 1;

    if (delimiter != '\'') {
        width = scan_string_delimiter(lexer, delimiter);
    }

    while (!skip_discovery_literal_character(lexer, delimiter, width, interpolated)) {
        continue;
    }
}

static Token scan_discovery_interpolated_string(Lexer* lexer)
{
    char delimiter = peek(lexer);

    if (delimiter != '"' && delimiter != '`') {
        return make_token(lexer, TokenUnknown);
    }

    advance(lexer);
    skip_discovery_literal(lexer, delimiter, true);

    return make_token(lexer, TokenStringLiteral);
}

static Token scan_discovery_symbol(Lexer* lexer, char c)
{
    switch (c) {
        case '"':
        case '`':
        case '\'':
            skip_discovery_literal(lexer, c, false);

            return make_token(lexer, TokenStringLiteral);
        case '$':
            return scan_discovery_interpolated_string(lexer);
        case '{':
            return make_token(lexer, TokenLbrace);
        case '}':
            return make_token(lexer, TokenRbrace);
        default:
            return make_token(lexer, TokenUnknown);
    }
}

static Token scan_discovery_token(Lexer* lexer)
{
    if (!skip_whitespace(lexer)) {
        return make_token(lexer, TokenEof);
    }

    lexer->start = lexer->current;

    if (is_end_of_file(lexer)) {
        return make_token(lexer, TokenEof);
    }

    char c = advance(lexer);

    if (is_alpha(c) || is_digit(c)) {
        return scan_identifier(lexer);
    }

    return scan_discovery_symbol(lexer, c);
}

static void update_discovery_depth(Token token, size_t* depth)
{
    switch (token.type) {
        case TokenLbrace:
            (*depth)++;
            break;
        case TokenRbrace:
            (*depth)--;
            break;
        case TokenEof:
            *depth = 0;
            break;
        default:
            break;
    }
}

static void skip_discovery_interpolation(Lexer* lexer)
{
    size_t depth = 1;

    while (depth > 0) {
        Token token = scan_discovery_token(lexer);

        update_discovery_depth(token, &depth);
    }
}

bool source_has_test_keyword(const char* source)
{
    Lexer lexer;
    Token token;

    init_lexer(&lexer, source);

    do {
        token = scan_discovery_token(&lexer);
    } while (token.type != TokenKeywordTest && token.type != TokenEof);

    return token.type == TokenKeywordTest;
}
