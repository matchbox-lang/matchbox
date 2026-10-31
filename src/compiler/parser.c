#include "compiler/parser.h"
#include "builtin/builtin.h"
#include "builtin/string.h"
#include "common/memory.h"
#include "common/util.h"
#include "common/vector.h"
#include "compiler/ast.h"
#include "compiler/diagnostics.h"
#include "compiler/lexer.h"
#include "compiler/literal.h"
#include "compiler/source_position.h"
#include "compiler/token.h"
#include "runtime/string_object.h"
#include <float.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct StringReader
{
    Token token;
    SourcePosition current;
    const char* end;
    size_t width;
    bool raw;
    bool interpolated;
} StringReader;

static ASTNode* parseExpression(Parser* parser);
static ASTNode* parseIdentifier(Parser* parser);
static ASTNode* parseFunctionCall(Parser* parser);
static ASTNode* parsePrefix(Parser* parser);
static bool parseBlocklevelStatements(Parser* parser, Vector* nodes);
static ASTNode* parseConditional(Parser* parser, bool allowSingleLineBody);
static ASTNode* parseMatch(Parser* parser);
static ASTNode* parseStatement(Parser* parser);

static _Noreturn void expectedExpressionError(Token token)
{
    fatalTokenError(token, "", "Expected expression but found ");
}

static _Noreturn void expectedOperandError(Token token)
{
    fatalTokenError(token, "", "Expected operand but found ");
}

static _Noreturn void expectedTokenError(TokenType type, Token token)
{
    const char* typeName = getTokenTypeName(type);

    fatalTokenError(token, "", "Expected %s but found ", typeName);
}

static _Noreturn void expectedTypeError(Token token)
{
    fatalTokenError(token, "", "Expected type but found ");
}

static _Noreturn void invalidStringError(Token token)
{
    fatalSourceError(token.line, token.column, "Invalid UTF-8 or escape in string literal");
}

static void advance(Parser* parser)
{
    parser->previousToken = parser->currentToken;
    parser->currentToken = scanToken(&parser->lexer);
}

static void consume(Parser* parser, TokenType type)
{
    if (parser->currentToken.type != type) {
        expectedTokenError(type, parser->currentToken);
    }

    advance(parser);
}

static void consumeType(Parser* parser)
{
    if (!isTypeToken(parser->currentToken.type)) {
        expectedTypeError(parser->currentToken);
    }

    consume(parser, parser->currentToken.type);
}

static ReferenceType parseReferenceType(Parser* parser)
{
    if (parser->currentToken.type == TOKEN_AMPERSAND) {
        consume(parser, TOKEN_AMPERSAND);

        return REFERENCE_SHARED;
    }

    if (parser->currentToken.type == TOKEN_CIRCUMFLEX) {
        consume(parser, TOKEN_CIRCUMFLEX);

        return REFERENCE_EXCLUSIVE;
    }

    return REFERENCE_NONE;
}

static bool isEndOfFile(Parser* parser)
{
    return parser->currentToken.type == TOKEN_INCOMPLETE_INPUT || parser->currentToken.type == TOKEN_EOF;
}

static DataType inferIntegerLiteralType(uint64_t value)
{
    if (value <= INT32_MAX) {
        return TYPE_I32;
    }

    if (value <= INT64_MAX) {
        return TYPE_I64;
    }

    return TYPE_UNKNOWN;
}

static DataType inferFloatLiteralType(double value)
{
    if (value > FLT_MAX) {
        return TYPE_F64;
    }

    float narrowed = (float)value;

    if ((double)narrowed == value) {
        return TYPE_F32;
    }

    return TYPE_F64;
}

static ASTNode* parseBooleanLiteralNode(Parser* parser, Token token)
{
    bool value = token.type == TOKEN_TRUE;
    ASTNode* ast = createBooleanNode(token, value);

    consume(parser, token.type);

    return ast;
}

static ASTNode* parseIntegerLiteralNode(Parser* parser, Token token)
{
    uint64_t value = parseDecimalLiteral(token);
    DataType typeId = inferIntegerLiteralType(value);
    ASTNode* ast = createIntegerNode(token, value, typeId);

    consume(parser, token.type);

    return ast;
}

static ASTNode* parseFloatLiteralNode(Parser* parser, Token token)
{
    double value = parseFloatLiteral(token);
    DataType typeId = inferFloatLiteralType(value);
    ASTNode* ast = createFloatNode(token, value, typeId);

    consume(parser, token.type);

    return ast;
}

static ASTNode* parseBinaryLiteralNode(Parser* parser, Token token)
{
    uint64_t value = parseBinaryLiteral(token);
    DataType typeId = inferIntegerLiteralType(value);
    ASTNode* ast = createIntegerNode(token, value, typeId);

    consume(parser, token.type);

    return ast;
}

static ASTNode* parseHexadecimalLiteralNode(Parser* parser, Token token)
{
    uint64_t value = parseHexadecimalLiteral(token);
    DataType typeId = inferIntegerLiteralType(value);
    ASTNode* ast = createIntegerNode(token, value, typeId);

    consume(parser, token.type);

    return ast;
}

static ASTNode* parseOctalLiteralNode(Parser* parser, Token token)
{
    uint64_t value = parseOctalLiteral(token);
    DataType typeId = inferIntegerLiteralType(value);
    ASTNode* ast = createIntegerNode(token, value, typeId);

    consume(parser, token.type);

    return ast;
}

static ASTNode* parseGroupExpression(Parser* parser)
{
    consume(parser, TOKEN_LPAREN);

    ASTNode* ast = parseExpression(parser);

    if (!ast && parser->currentToken.type == TOKEN_RPAREN) {
        expectedExpressionError(parser->currentToken);
    }

    if (!ast || isEndOfFile(parser)) {
        freeASTNode(ast);

        return NULL;
    }

    consume(parser, TOKEN_RPAREN);

    return ast;
}

static ASTNode* parseCharacterLiteral(Parser* parser)
{
    Token token = parser->currentToken;
    DecodedScalar scalar;
    bool valid = decodeLiteralScalar(token.chars + 1, token.length - 2, &scalar);

    if (!valid) {
        expectedExpressionError(token);
    }

    ASTNode* ast = createCharacterNode(token, scalar.value);

    consume(parser, TOKEN_CHARACTER_LITERAL);

    return ast;
}

static void advanceStringReader(StringReader* reader, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        char c = *reader->current.chars++;
        reader->current.column++;

        if (c == '\n') {
            reader->current.line++;
            reader->current.column = 1;
        }
    }
}

static StringReader createStringReader(Token token)
{
    StringReader reader = {.token = token, .current = {token.chars, token.line, token.column}, .width = 1};
    reader.interpolated = token.chars[0] == '$';

    if (reader.interpolated) {
        advanceStringReader(&reader, 1);
    }

    char delimiter = *reader.current.chars;
    size_t run = 0;
    reader.raw = delimiter == '`';

    while (reader.current.chars[run] == delimiter) {
        run++;
    }

    if (run >= 3) {
        reader.width = run;
    }

    advanceStringReader(&reader, reader.width);

    reader.end = token.chars + token.length - reader.width;

    return reader;
}

static ASTNode* parseStringSegment(StringReader* reader)
{
    size_t capacity = reader->end - reader->current.chars;
    char* bytes = allocateMemory(capacity + 1);
    size_t count = 0;

    while (reader->current.chars < reader->end) {
        const char* chars = reader->current.chars;
        size_t length = reader->end - chars;
        char c = *chars;
        bool bracePair = reader->interpolated && length >= 2 && chars[1] == c && (c == '{' || c == '}');

        if (reader->interpolated && c == '{' && !bracePair) {
            break;
        }

        if (bracePair) {
            bytes[count++] = c;

            advanceStringReader(reader, 2);
            continue;
        }

        DecodedScalar scalar;
        bool valid;

        if (reader->raw) {
            valid = decodeUtf8(chars, length, &scalar);
        } else {
            valid = decodeLiteralScalar(chars, length, &scalar);
        }

        if (!valid) {
            freeMemory(bytes);
            invalidStringError(reader->token);
        }

        size_t encodedLength = encodeUtf8(scalar.value, bytes + count);
        count += encodedLength;

        advanceStringReader(reader, scalar.length);
    }

    StringObject* value = copyStringObject(bytes, count);
    ASTNode* ast = createStringNode(reader->token, value);

    freeMemory(bytes);

    return ast;
}

static ASTNode* parseStringInterpolation(Parser* parser, StringReader* reader)
{
    advanceStringReader(reader, 1);

    parser->lexer.current = reader->current;

    advance(parser);

    ASTNode* expression = parseExpression(parser);

    if (!expression) {
        expectedExpressionError(parser->currentToken);
    }

    if (parser->currentToken.type != TOKEN_RBRACE) {
        expectedTokenError(TOKEN_RBRACE, parser->currentToken);
    }

    reader->current = parser->lexer.current;

    return expression;
}

static void appendStringPart(ASTNode* call, ASTNode* part)
{
    if (part->type == AST_STRING && part->stringLiteral.value->length == 0) {
        freeASTNode(part);

        return;
    }

    if (part->type == AST_BUILTIN_CALL && part->builtinCall.builtin->entry == builtinString) {
        Vector* args = &part->builtinCall.args;

        for (size_t i = 0; i < args->count; i++) {
            pushVectorItem(&call->builtinCall.args, args->data[i]);
        }

        args->count = 0;

        freeASTNode(part);

        return;
    }

    pushVectorItem(&call->builtinCall.args, part);
}

static ASTNode* parseStringLiteral(Parser* parser)
{
    Token token = parser->currentToken;
    Token previous = parser->previousToken;
    Lexer lexer = parser->lexer;
    StringReader reader = createStringReader(token);
    ASTNode* ast = parseStringSegment(&reader);

    if (reader.current.chars < reader.end) {
        Builtin* builtin = resolveBuiltin("string", NULL, 0);
        ASTNode* call = createBuiltinCallNode(token, builtin);

        appendStringPart(call, ast);

        ast = call;
    }

    while (reader.current.chars < reader.end) {
        ASTNode* expression = parseStringInterpolation(parser, &reader);

        appendStringPart(ast, expression);

        ASTNode* segment = parseStringSegment(&reader);

        appendStringPart(ast, segment);
    }

    parser->lexer = lexer;
    parser->currentToken = token;
    parser->previousToken = previous;

    consume(parser, TOKEN_STRING_LITERAL);

    return ast;
}

static ASTNode* parsePrimary(Parser* parser)
{
    switch (parser->currentToken.type) {
        case TOKEN_TRUE:
        case TOKEN_FALSE:
            return parseBooleanLiteralNode(parser, parser->currentToken);
        case TOKEN_IF:
            return parseConditional(parser, false);
        case TOKEN_MATCH:
            return parseMatch(parser);
        case TOKEN_I8:
        case TOKEN_I16:
        case TOKEN_I32:
        case TOKEN_I64:
        case TOKEN_U8:
        case TOKEN_U16:
        case TOKEN_U32:
        case TOKEN_U64:
        case TOKEN_F32:
        case TOKEN_F64:
        case TOKEN_CHAR:
        case TOKEN_STRING:
            consume(parser, parser->currentToken.type);

            return parseFunctionCall(parser);
        case TOKEN_STRING_LITERAL:
            return parseStringLiteral(parser);
        case TOKEN_CHARACTER_LITERAL:
            return parseCharacterLiteral(parser);
        case TOKEN_INTEGER_LITERAL:
            return parseIntegerLiteralNode(parser, parser->currentToken);
        case TOKEN_FLOAT_LITERAL:
            return parseFloatLiteralNode(parser, parser->currentToken);
        case TOKEN_BINARY_LITERAL:
            return parseBinaryLiteralNode(parser, parser->currentToken);
        case TOKEN_HEXADECIMAL_LITERAL:
            return parseHexadecimalLiteralNode(parser, parser->currentToken);
        case TOKEN_OCTAL_LITERAL:
            return parseOctalLiteralNode(parser, parser->currentToken);
        case TOKEN_LPAREN:
            return parseGroupExpression(parser);
        case TOKEN_IDENTIFIER:
            return parseIdentifier(parser);
        case TOKEN_INCOMPLETE_INPUT:
        case TOKEN_EOF:
            return NULL;
        default:
            expectedExpressionError(parser->currentToken);
            return NULL;
    }
}

static ASTNode* parseConditionalBranch(Parser* parser)
{
    consume(parser, TOKEN_LBRACE);

    ASTNode* branch = createCompoundNode(NULL);

    if (!parseBlocklevelStatements(parser, &branch->compound.statements)) {
        freeASTNode(branch);

        return NULL;
    }

    consume(parser, TOKEN_RBRACE);

    return branch;
}

static ASTNode* parseConditionalElse(Parser* parser, bool allowSingleLineBody)
{
    if (parser->currentToken.type != TOKEN_ELSE) {
        return NULL;
    }

    consume(parser, TOKEN_ELSE);
    if (parser->currentToken.type == TOKEN_IF) {
        return parseConditional(parser, allowSingleLineBody);
    }

    return parseConditionalBranch(parser);
}

static bool isParenthesizedCondition(Parser start, Token end)
{
    if (start.currentToken.type != TOKEN_LPAREN) {
        return false;
    }

    Token token = start.currentToken;
    int depth = 1;

    while (depth > 0 && token.type != TOKEN_EOF && token.type != TOKEN_INCOMPLETE_INPUT) {
        token = scanToken(&start.lexer);
        depth += token.type == TOKEN_LPAREN;
        depth -= token.type == TOKEN_RPAREN;
    }

    return depth == 0 && token.chars == end.chars;
}

static void validateSingleLineIfBranchEnd(Parser* parser, Token token)
{
    Token end = parser->previousToken;
    bool multiline = end.line != token.line || memchr(end.chars, '\n', end.length)
        || memchr(end.chars, '\r', end.length);

    if (multiline) {
        fatalSourceError(token.line, token.column, "Expected statement to end on the same physical line as if");
    }

    if (parser->currentToken.type == TOKEN_SEMICOLON) {
        return;
    }

    if (parser->currentToken.type == TOKEN_ELSE) {
        fatalTokenError(parser->currentToken, "", "Unexpected ");
    }

    bool sameLineStatement = !isEndOfFile(parser) && parser->currentToken.line == token.line
        && parser->currentToken.type != TOKEN_RBRACE;

    if (sameLineStatement) {
        fatalTokenError(parser->currentToken, "", "Expected end of line but found ");
    }
}

static ASTNode* parseSingleLineIfBranch(Parser* parser, Token token)
{
    if (isEndOfFile(parser)) {
        return NULL;
    }

    if (parser->currentToken.line != token.line) {
        fatalSourceError(token.line, token.column, "Expected statement on the same physical line as if");
    }

    if (parser->currentToken.type == TOKEN_IF) {
        expectedTokenError(TOKEN_LBRACE, parser->currentToken);
    }

    ASTNode* statement = parseStatement(parser);

    if (!statement) {
        return NULL;
    }

    validateSingleLineIfBranchEnd(parser, token);

    ASTNode* branch = createCompoundNode(NULL);

    pushVectorItem(&branch->compound.statements, statement);

    return branch;
}

static ASTNode* parseIfBranch(Parser* parser, Parser start, Token token, bool allowSingleLineBody)
{
    if (parser->currentToken.type == TOKEN_LBRACE) {
        return parseConditionalBranch(parser);
    }

    if (!allowSingleLineBody) {
        expectedTokenError(TOKEN_LBRACE, parser->currentToken);
    }

    bool parenthesized = isParenthesizedCondition(start, parser->previousToken);

    if (!parenthesized) {
        expectedTokenError(TOKEN_LBRACE, parser->currentToken);
    }

    return parseSingleLineIfBranch(parser, token);
}

static ASTNode* parseConditional(Parser* parser, bool allowSingleLineBody)
{
    Token token = parser->currentToken;

    consume(parser, TOKEN_IF);

    Parser conditionStart = *parser;
    ASTNode* condition = parseExpression(parser);
    if (!condition) {
        return NULL;
    }

    bool singleLineBody = parser->currentToken.type != TOKEN_LBRACE;
    ASTNode* thenBranch = parseIfBranch(parser, conditionStart, token, allowSingleLineBody);

    if (!thenBranch) {
        freeASTNode(condition);

        return NULL;
    }

    if (singleLineBody) {
        return createConditionalNode(token, condition, thenBranch, NULL);
    }

    bool hasElse = parser->currentToken.type == TOKEN_ELSE;
    ASTNode* elseBranch = parseConditionalElse(parser, allowSingleLineBody);

    if (hasElse && !elseBranch) {
        freeASTNode(condition);
        freeASTNode(thenBranch);

        return NULL;
    }

    return createConditionalNode(token, condition, thenBranch, elseBranch);
}

static void markConditionalStatement(ASTNode* ast)
{
    ast->conditional.expression = false;

    ASTNode* elseBranch = ast->conditional.elseBranch;
    if (elseBranch && elseBranch->type == AST_CONDITIONAL) {
        markConditionalStatement(elseBranch);
    }
}

static bool isMatchBranchEnd(Parser* parser)
{
    return isMatchArmEndToken(parser->currentToken.type) || isEndOfFile(parser);
}

static void consumeMatchStatementSeparator(Parser* parser, Token token)
{
    bool sameLineStatement = parser->currentToken.line == token.line
        && !isMatchArmEndToken(parser->currentToken.type)
        && parser->previousToken.type != TOKEN_RBRACE;

    if (sameLineStatement || parser->currentToken.type == TOKEN_SEMICOLON) {
        consume(parser, TOKEN_SEMICOLON);
    }
}

static bool parseMatchBranchStatement(Parser* parser, ASTNode* branch)
{
    Token token = parser->currentToken;
    ASTNode* statement = parseStatement(parser);

    if (!statement) {
        return false;
    }

    consumeMatchStatementSeparator(parser, token);
    pushVectorItem(&branch->compound.statements, statement);

    return true;
}

static ASTNode* parseMatchBranch(Parser* parser)
{
    ASTNode* branch = createCompoundNode(NULL);

    if (isEndOfFile(parser)) {
        freeASTNode(branch);

        return NULL;
    }

    if (isMatchArmEndToken(parser->currentToken.type)) {
        freeASTNode(branch);
        expectedExpressionError(parser->currentToken);
    }

    while (!isMatchBranchEnd(parser)) {
        if (!parseMatchBranchStatement(parser, branch)) {
            freeASTNode(branch);

            return NULL;
        }
    }

    return branch;
}

static ASTNode* parseMatchBinding(Parser* parser)
{
    Token token = parser->currentToken;
    StringObject* id = copyStringObject(token.chars, token.length);
    ASTNode* binding = createVariableDefinitionNode(token, id, true);
    binding->variableDefinition.initialized = true;

    consume(parser, TOKEN_IDENTIFIER);

    return binding;
}

static bool parseMatchGuard(Parser* parser, MatchArm* arm)
{
    if (parser->currentToken.type != TOKEN_IF) {
        return true;
    }

    arm->guardToken = parser->currentToken;

    consume(parser, TOKEN_IF);

    arm->guard = parseExpression(parser);

    return arm->guard != NULL;
}

static bool parseMatchArm(Parser* parser, ASTNode* ast)
{
    consume(parser, TOKEN_CASE);

    MatchArm* arm = allocateZeroedMemory(1, sizeof(MatchArm));

    pushVectorItem(&ast->match.arms, arm);

    if (isWildcardToken(parser->currentToken)) {
        consume(parser, TOKEN_IDENTIFIER);
    } else if (ast->match.subject && parser->currentToken.type == TOKEN_IDENTIFIER) {
        arm->binding = parseMatchBinding(parser);
    } else {
        arm->pattern = parseExpression(parser);
    }

    if (isEndOfFile(parser)) {
        return false;
    }

    if (!parseMatchGuard(parser, arm)) {
        return false;
    }

    consume(parser, TOKEN_COLON);
    arm->branch = parseMatchBranch(parser);

    return arm->branch != NULL;
}

static bool parseMatchDefault(Parser* parser, ASTNode* ast)
{
    if (parser->currentToken.type != TOKEN_DEFAULT) {
        return true;
    }

    consume(parser, TOKEN_DEFAULT);

    if (isEndOfFile(parser)) {
        return false;
    }

    consume(parser, TOKEN_COLON);
    ast->match.defaultBranch = parseMatchBranch(parser);

    return ast->match.defaultBranch != NULL;
}

static ASTNode* parseMatch(Parser* parser)
{
    Token token = parser->currentToken;
    consume(parser, TOKEN_MATCH);

    ASTNode* ast = createMatchNode(token);

    if (parser->currentToken.type != TOKEN_LBRACE) {
        ast->match.subject = parseExpression(parser);
    }

    if (isEndOfFile(parser)) {
        freeASTNode(ast);

        return NULL;
    }

    consume(parser, TOKEN_LBRACE);
    while (parser->currentToken.type == TOKEN_CASE) {
        if (!parseMatchArm(parser, ast)) {
            freeASTNode(ast);

            return NULL;
        }
    }

    if (!parseMatchDefault(parser, ast)) {
        freeASTNode(ast);

        return NULL;
    }

    if (isEndOfFile(parser)) {
        freeASTNode(ast);

        return NULL;
    }

    consume(parser, TOKEN_RBRACE);

    return ast;
}

static ASTNode* parsePrefixOperand(Parser* parser)
{
    if (isPrefixToken(parser->currentToken.type)) {
        return parsePrefix(parser);
    }

    Token token = parser->currentToken;
    ASTNode* expr = parsePrimary(parser);

    if (!expr) {
        return NULL;
    }

    if (!isPrefix(expr) && !isPrefixOperand(expr)) {
        expectedOperandError(token);
    }

    return expr;
}

static ASTNode* parsePrefix(Parser* parser)
{
    if (!isPrefixToken(parser->currentToken.type)) {
        return parsePrimary(parser);
    }

    Token token = parser->currentToken;
    consume(parser, token.type);
    ASTNode* expr = parsePrefixOperand(parser);

    if (!expr) {
        return NULL;
    }

    return createPrefixNode(token, expr);
}

static ASTNode* parseExponent(Parser* parser)
{
    ASTNode* expr = parsePrefix(parser);
    Token token = parser->currentToken;

    if (token.type == TOKEN_POWER) {
        consume(parser, token.type);
        expr = createBinaryNode(expr, parseExponent(parser), token);
    }

    return expr;
}

static ASTNode* parseFactor(Parser* parser)
{
    ASTNode* expr = parseExponent(parser);
    Token token = parser->currentToken;

    while (isFactorToken(token.type)) {
        consume(parser, token.type);
        expr = createBinaryNode(expr, parseExponent(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static ASTNode* parseTerm(Parser* parser)
{
    ASTNode* expr = parseFactor(parser);
    Token token = parser->currentToken;

    while (isTermToken(token.type)) {
        consume(parser, token.type);
        expr = createBinaryNode(expr, parseFactor(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static ASTNode* parseShift(Parser* parser)
{
    ASTNode* expr = parseTerm(parser);
    Token token = parser->currentToken;

    while (isShiftToken(token.type)) {
        consume(parser, token.type);
        expr = createBinaryNode(expr, parseTerm(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static ASTNode* parseComparison(Parser* parser)
{
    ASTNode* expr = parseShift(parser);
    Token token = parser->currentToken;

    while (isComparisonToken(token.type)) {
        consume(parser, token.type);
        expr = createBinaryNode(expr, parseShift(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static ASTNode* parseEquality(Parser* parser)
{
    ASTNode* expr = parseComparison(parser);
    Token token = parser->currentToken;

    while (isEqualityToken(token.type)) {
        consume(parser, token.type);
        expr = createBinaryNode(expr, parseComparison(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static ASTNode* parseBitwiseAND(Parser* parser)
{
    ASTNode* expr = parseEquality(parser);
    Token token = parser->currentToken;

    while (token.type == TOKEN_AMPERSAND) {
        consume(parser, token.type);
        expr = createBinaryNode(expr, parseEquality(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static ASTNode* parseBitwiseXOR(Parser* parser)
{
    ASTNode* expr = parseBitwiseAND(parser);
    Token token = parser->currentToken;

    while (token.type == TOKEN_CIRCUMFLEX) {
        consume(parser, token.type);
        expr = createBinaryNode(expr, parseBitwiseAND(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static ASTNode* parseBitwiseOR(Parser* parser)
{
    ASTNode* expr = parseBitwiseXOR(parser);
    Token token = parser->currentToken;

    while (token.type == TOKEN_PIPE) {
        consume(parser, token.type);
        expr = createBinaryNode(expr, parseBitwiseXOR(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static ASTNode* parseBooleanAND(Parser* parser)
{
    ASTNode* expr = parseBitwiseOR(parser);
    Token token = parser->currentToken;

    while (token.type == TOKEN_AND) {
        consume(parser, token.type);
        expr = createBinaryNode(expr, parseBitwiseOR(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static ASTNode* parseBooleanOR(Parser* parser)
{
    ASTNode* expr = parseBooleanAND(parser);
    Token token = parser->currentToken;

    while (token.type == TOKEN_OR) {
        consume(parser, token.type);
        expr = createBinaryNode(expr, parseBooleanAND(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static ASTNode* parseTernaryBranch(Parser* parser)
{
    ASTNode* expression = parseExpression(parser);

    if (!expression) {
        return NULL;
    }

    ASTNode* branch = createCompoundNode(NULL);

    pushVectorItem(&branch->compound.statements, expression);

    return branch;
}

static ASTNode* parseTernary(Parser* parser, ASTNode* condition)
{
    Token token = parser->currentToken;

    consume(parser, TOKEN_QUESTION);

    ASTNode* thenBranch = parseTernaryBranch(parser);

    if (!thenBranch || isEndOfFile(parser)) {
        freeASTNode(condition);
        freeASTNode(thenBranch);

        return NULL;
    }

    consume(parser, TOKEN_COLON);

    ASTNode* elseBranch = parseTernaryBranch(parser);

    if (!elseBranch) {
        freeASTNode(condition);
        freeASTNode(thenBranch);

        return NULL;
    }

    return createConditionalNode(token, condition, thenBranch, elseBranch);
}

static ASTNode* parseExpression(Parser* parser)
{
    ASTNode* condition = parseBooleanOR(parser);

    if (!condition || parser->currentToken.type != TOKEN_QUESTION) {
        return condition;
    }

    return parseTernary(parser, condition);
}

static ASTNode* parseReturnStatement(Parser* parser)
{
    Token token = parser->currentToken;
    consume(parser, TOKEN_RETURN);

    ASTNode* expr = parseExpression(parser);
    if (!expr) {
        return NULL;
    }

    return createReturnNode(token, expr);
}

static ASTNode* parseArgument(Parser* parser)
{
    return parseExpression(parser);
}

static bool parseArgumentListItem(Parser* parser, Vector* args)
{
    ASTNode* expr = parseArgument(parser);

    if (!expr && (parser->currentToken.type == TOKEN_COMMA || parser->currentToken.type == TOKEN_RPAREN)) {
        expectedExpressionError(parser->currentToken);
    }

    if (!expr) {
        return false;
    }

    pushVectorItem(args, expr);

    if (parser->currentToken.type == TOKEN_COMMA) {
        consume(parser, TOKEN_COMMA);
    }

    return true;
}

static bool parseArgumentList(Parser* parser, Vector* args)
{
    consume(parser, TOKEN_LPAREN);

    if (isEndOfFile(parser)) {
        return false;
    }

    while (parser->currentToken.type != TOKEN_RPAREN) {
        if (!parseArgumentListItem(parser, args)) {
            return false;
        }
    }

    if (isEndOfFile(parser)) {
        return false;
    }

    consume(parser, TOKEN_RPAREN);

    return true;
}

static ASTNode* parseParameter(Parser* parser)
{
    if (isEndOfFile(parser)) {
        return NULL;
    }

    Token token = parser->currentToken;
    StringObject* id = copyStringObject(token.chars, token.length);

    consume(parser, TOKEN_IDENTIFIER);

    ASTNode* ast = createParameterNode(token, id);

    if (parser->currentToken.type == TOKEN_COLON) {
        consume(parser, TOKEN_COLON);

        ast->parameter.referenceType = parseReferenceType(parser);
        ast->parameter.typeId = getTokenDataType(parser->currentToken.type);

        consumeType(parser);
    }

    return ast;
}

static bool parseParameterListItem(Parser* parser, Vector* params)
{
    ASTNode* param = parseParameter(parser);

    if (!param) {
        return false;
    }

    pushVectorItem(params, param);

    if (parser->currentToken.type == TOKEN_COMMA) {
        consume(parser, TOKEN_COMMA);
    }

    return true;
}

static bool parseParameterList(Parser* parser, Vector* params)
{
    consume(parser, TOKEN_LPAREN);

    if (isEndOfFile(parser)) {
        return false;
    }

    while (parser->currentToken.type != TOKEN_RPAREN) {
        if (!parseParameterListItem(parser, params)) {
            return false;
        }
    }

    if (isEndOfFile(parser)) {
        return false;
    }

    consume(parser, TOKEN_RPAREN);

    size_t count = countVector(params);

    for (size_t i = 0; i < count; i++) {
        ASTNode* param = getVectorAt(params, i);
        param->parameter.position = count - i - 1;
    }

    return true;
}

static ASTNode* parseFunctionCall(Parser* parser)
{
    ASTNode* ast = createFunctionCallNode(parser->previousToken);

    if (!parseArgumentList(parser, &ast->functionCall.args)) {
        freeASTNode(ast);

        return NULL;
    }

    return ast;
}

static void parseFunctionReturnType(Parser* parser, ASTNode* ast)
{
    if (parser->currentToken.type != TOKEN_ARROW) {
        return;
    }

    consume(parser, TOKEN_ARROW);

    ast->functionDefinition.returnReferenceType = parseReferenceType(parser);
    ast->functionDefinition.returnTypeId = getTokenDataType(parser->currentToken.type);

    consumeType(parser);

    ast->functionDefinition.hasExplicitReturnType = true;
}

static bool parseFunctionBody(Parser* parser, ASTNode* ast)
{
    if (isEndOfFile(parser)) {
        return false;
    }

    consume(parser, TOKEN_LBRACE);

    ASTNode* body = createCompoundNode(NULL);
    ast->functionDefinition.body = body;

    if (!parseBlocklevelStatements(parser, &body->compound.statements)) {
        return false;
    }

    consume(parser, TOKEN_RBRACE);

    return true;
}

static ASTNode* parseFunctionDefinition(Parser* parser)
{
    consume(parser, TOKEN_FUNC);

    if (isEndOfFile(parser)) {
        return NULL;
    }

    Token token = parser->currentToken;
    StringObject* id = copyStringObject(token.chars, token.length);

    if (parser->currentToken.type != TOKEN_IDENTIFIER) {
        expectedTokenError(TOKEN_IDENTIFIER, parser->currentToken);
    }

    consume(parser, TOKEN_IDENTIFIER);

    if (isEndOfFile(parser)) {
        return NULL;
    }

    ASTNode* ast = createFunctionDefinitionNode(token, id);

    if (!parseParameterList(parser, &ast->functionDefinition.params)) {
        freeASTNode(ast);

        return NULL;
    }

    parseFunctionReturnType(parser, ast);

    if (!parseFunctionBody(parser, ast)) {
        freeASTNode(ast);

        return NULL;
    }

    return ast;
}

static ASTNode* parseAssignment(Parser* parser)
{
    Token operator = parser->currentToken;
    Token token = parser->previousToken;
    consume(parser, operator.type);

    ASTNode* expr = parseExpression(parser);
    if (!expr) {
        return NULL;
    }

    ASTNode* ast = createAssignmentNode(operator, token, expr);

    return ast;
}

static void parseVariableType(Parser* parser, ASTNode* ast)
{
    if (parser->currentToken.type == TOKEN_COLON) {
        consume(parser, TOKEN_COLON);

        ast->variableDefinition.referenceType = parseReferenceType(parser);
        ast->variableDefinition.typeId = getTokenDataType(parser->currentToken.type);

        consumeType(parser);

        return;
    }

    if (parser->currentToken.type != TOKEN_EQUAL) {
        expectedTokenError(TOKEN_EQUAL, parser->currentToken);
    }
}

static bool parseVariableInitializer(Parser* parser, ASTNode* ast)
{
    if (parser->currentToken.type == TOKEN_EQUAL) {
        consume(parser, TOKEN_EQUAL);
        ast->variableDefinition.expr = parseExpression(parser);

        return ast->variableDefinition.expr != NULL;
    }

    ast->variableDefinition.expr = createASTNode(AST_NONE);

    if (ast->variableDefinition.typeId == TYPE_UNKNOWN) {
        ast->variableDefinition.typeId = TYPE_I32;
    }

    return true;
}

static ASTNode* parseVariableDefinition(Parser* parser)
{
    bool fixed = parser->currentToken.type == TOKEN_LET;

    consume(parser, parser->currentToken.type);

    if (isEndOfFile(parser)) {
        return NULL;
    }

    Token token = parser->currentToken;
    StringObject* id = copyStringObject(token.chars, token.length);

    consume(parser, TOKEN_IDENTIFIER);

    if (isEndOfFile(parser)) {
        return NULL;
    }

    ASTNode* ast = createVariableDefinitionNode(token, id, fixed);

    parseVariableType(parser, ast);

    if (fixed && parser->currentToken.type != TOKEN_EQUAL) {
        expectedTokenError(TOKEN_EQUAL, parser->currentToken);
    }

    if (!parseVariableInitializer(parser, ast)) {
        freeASTNode(ast);
        return NULL;
    }

    return ast;
}

static ASTNode* parseIdentifier(Parser* parser)
{
    consume(parser, TOKEN_IDENTIFIER);

    if (isAssignmentToken(parser->currentToken.type)) {
        return parseAssignment(parser);
    }

    if (parser->currentToken.type == TOKEN_LPAREN) {
        return parseFunctionCall(parser);
    }

    return createVariableNode(parser->previousToken);
}

static ASTNode* parseWhileStatement(Parser* parser)
{
    Token token = parser->currentToken;
    consume(parser, TOKEN_WHILE);
    ASTNode* condition = parseExpression(parser);

    if (!condition) {
        return NULL;
    }

    ASTNode* body = parseConditionalBranch(parser);

    if (!body) {
        freeASTNode(condition);

        return NULL;
    }

    return createWhileNode(token, condition, body);
}

static ASTNode* parseIfStatement(Parser* parser)
{
    ASTNode* ast = parseConditional(parser, true);
    if (!ast) {
        return NULL;
    }

    markConditionalStatement(ast);

    return ast;
}

static ASTNode* parseMatchStatement(Parser* parser)
{
    ASTNode* ast = parseMatch(parser);
    if (!ast) {
        return NULL;
    }

    ast->match.expression = false;

    return ast;
}

static ASTNode* parseStatement(Parser* parser)
{
    switch (parser->currentToken.type) {
        case TOKEN_FUNC:
            return parseFunctionDefinition(parser);
        case TOKEN_LET:
        case TOKEN_VAR:
            return parseVariableDefinition(parser);
        case TOKEN_RETURN:
            return parseReturnStatement(parser);
        case TOKEN_WHILE:
            return parseWhileStatement(parser);
        case TOKEN_IF:
            return parseIfStatement(parser);
        case TOKEN_MATCH:
            return parseMatchStatement(parser);
        default:
            return parseExpression(parser);
    }
}

static bool parseStatements(Parser* parser, Vector* nodes, TokenType type)
{
    Token token = parser->currentToken;

    while (token.type != type && token.type != TOKEN_INCOMPLETE_INPUT) {
        ASTNode* stmt = parseStatement(parser);
        if (!stmt) {
            return false;
        }

        bool sameLineStatement = !isEndOfFile(parser) &&
            parser->currentToken.line == token.line &&
            parser->currentToken.type != type &&
            parser->previousToken.type != TOKEN_RBRACE;

        if (sameLineStatement || parser->currentToken.type == TOKEN_SEMICOLON) {
            consume(parser, TOKEN_SEMICOLON);
        }

        pushVectorItem(nodes, stmt);
        token = parser->currentToken;
    }

    return true;
}

static bool parseBlocklevelStatements(Parser* parser, Vector* nodes)
{
    return parseStatements(parser, nodes, TOKEN_RBRACE);
}

static bool parseTopLevelStatements(Parser* parser)
{
    return parseStatements(parser, &parser->topLevel->compound.statements, TOKEN_EOF);
}

void initParser(Parser* parser, ASTNode* ast)
{
    parser->topLevel = ast;
}

bool parse(Parser* parser, char* source)
{
    initLexer(&parser->lexer, source);
    advance(parser);

    return parseTopLevelStatements(parser) && parser->currentToken.type != TOKEN_INCOMPLETE_INPUT;
}
