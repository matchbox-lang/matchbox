#include "parser.h"
#include "ast.h"
#include "conversion.h"
#include "lexer.h"
#include "string_object.h"
#include "token.h"
#include "vector.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static ASTNode* parseExpression(Parser* parser);
static ASTNode* parseIdentifier(Parser* parser);
static ASTNode* parsePrefix(Parser* parser);
static bool parseBlocklevelStatements(Parser* parser, Vector* nodes);

static void expectedExpressionError(Token token)
{
    fprintf(stderr, "Error: Expected expression but found ");
    printTokenValue(token);
    fprintf(stderr, " on line %d:%d\n", token.line, token.column);
    exit(1);
}

static void expectedOperandError(Token token)
{
    fprintf(stderr, "Error: Expected operand but found ");
    printTokenValue(token);
    fprintf(stderr, " on line %d:%d\n", token.line, token.column);
    exit(1);
}

static void expectedTypeError(Token token)
{
    fprintf(stderr, "Error: Expected type but found ");
    printTokenValue(token);
    fprintf(stderr, " on line %d:%d\n", token.line, token.column);
    exit(1);
}

static void expectedTokenError(TokenType type, Token token)
{
    fprintf(stderr, "Error: Expected %s but found ", getTokenTypeName(type));
    printTokenValue(token);
    fprintf(stderr, " on line %d:%d\n", token.line, token.column);
    exit(1);
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
    return parser->currentToken.type == TOKEN_EOF;
}

static ASTNode* parseIntegerLiteral(Parser* parser, Token token)
{
    ASTNode* ast = createASTNode(AST_INTEGER);
    ast->integerLiteral.value = integerLiteralToValue(token);
    ast->integerLiteral.token = token;
    ast->integerLiteral.typeId = ast->integerLiteral.value <= INT32_MAX ? TOKEN_I32 : TOKEN_UNKNOWN;
    
    consume(parser, token.type);

    return ast;
}

static ASTNode* parseBinaryLiteral(Parser* parser, Token token)
{
    ASTNode* ast = createASTNode(AST_INTEGER);
    ast->integerLiteral.value = binaryLiteralToValue(token);
    ast->integerLiteral.token = token;
    ast->integerLiteral.typeId = ast->integerLiteral.value <= INT32_MAX ? TOKEN_I32 : TOKEN_UNKNOWN;

    consume(parser, token.type);

    return ast;
}

static ASTNode* parseHexadecimalLiteral(Parser* parser, Token token)
{
    ASTNode* ast = createASTNode(AST_INTEGER);
    ast->integerLiteral.value = hexadecimalLiteralToValue(token);
    ast->integerLiteral.token = token;
    ast->integerLiteral.typeId = ast->integerLiteral.value <= INT32_MAX ? TOKEN_I32 : TOKEN_UNKNOWN;

    consume(parser, token.type);

    return ast;
}

static ASTNode* parseOctalLiteral(Parser* parser, Token token)
{
    ASTNode* ast = createASTNode(AST_INTEGER);
    ast->integerLiteral.value = octalLiteralToValue(token);
    ast->integerLiteral.token = token;
    ast->integerLiteral.typeId = ast->integerLiteral.value <= INT32_MAX ? TOKEN_I32 : TOKEN_UNKNOWN;

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

static ASTNode* createBinaryNode(ASTNode* leftExpr, ASTNode* rightExpr, Token token)
{
    if (!rightExpr) {
        return NULL;
    }

    ASTNode* ast = createASTNode(AST_BINARY);
    ast->binary.leftExpr = leftExpr;
    ast->binary.operator = token;
    ast->binary.rightExpr = rightExpr;
    ast->binary.typeId = TOKEN_UNKNOWN;

    return ast;
}

static ASTNode* createVariableNode(Token token)
{
    ASTNode* ast = createASTNode(AST_VARIABLE);
    ast->variable.scope = NULL;
    ast->variable.id = copyStringObject(token.chars, token.length);
    ast->variable.token = token;
    ast->variable.symbol = NULL;

    return ast;
}

static ASTNode* parsePrimary(Parser* parser)
{
    switch (parser->currentToken.type) {
        case TOKEN_INTEGER_LITERAL:
            return parseIntegerLiteral(parser, parser->currentToken);
        case TOKEN_BINARY_LITERAL:
            return parseBinaryLiteral(parser, parser->currentToken);
        case TOKEN_HEXADECIMAL_LITERAL:
            return parseHexadecimalLiteral(parser, parser->currentToken);
        case TOKEN_OCTAL_LITERAL:
            return parseOctalLiteral(parser, parser->currentToken);
        case TOKEN_LPAREN:
            return parseGroupExpression(parser);
        case TOKEN_IDENTIFIER:
            return parseIdentifier(parser);
        case TOKEN_EOF:
            return NULL;
        default:
            expectedExpressionError(parser->currentToken);
            return NULL;
    }
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

    ASTNode* ast = createASTNode(AST_PREFIX);
    ast->prefix.expr = expr;
    ast->prefix.operator = token;

    return ast;
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

static ASTNode* parseExpression(Parser* parser)
{
    return parseBooleanOR(parser);
}

static ASTNode* parseReturnStatement(Parser* parser)
{
    Token token = parser->currentToken;
    consume(parser, TOKEN_RETURN);

    ASTNode* expr = parseExpression(parser);
    if (!expr) {
        return NULL;
    }

    ASTNode* ast = createASTNode(AST_RETURN);
    ast->returnStatement.token = token;
    ast->returnStatement.expr = expr;

    return ast;
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

static ASTNode* createParameterNode(Token token, StringObject* id)
{
    ASTNode* ast = createASTNode(AST_PARAMETER);
    ast->parameter.scope = NULL;
    ast->parameter.id = id;
    ast->parameter.token = token;
    ast->parameter.typeId = TOKEN_I32;
    ast->parameter.referenceType = REFERENCE_NONE;

    return ast;
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
        ast->parameter.typeId = parser->currentToken.type;
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

static ASTNode* createFunctionCallNode(Token token)
{
    ASTNode* ast = createASTNode(AST_FUNCTION_CALL);
    ast->functionCall.scope = NULL;
    ast->functionCall.id = copyStringObject(token.chars, token.length);
    ast->functionCall.token = token;
    ast->functionCall.symbol = NULL;
    ast->functionCall.referenceOrigin = NULL;

    return ast;
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

static ASTNode* createFunctionDefinitionNode(Token token, StringObject* id)
{
    ASTNode* ast = createASTNode(AST_FUNCTION_DEFINITION);
    ast->functionDefinition.scope = NULL;
    ast->functionDefinition.id = id;
    ast->functionDefinition.token = token;
    ast->functionDefinition.typeId = TOKEN_I32;
    ast->functionDefinition.returnReferenceType = REFERENCE_NONE;
    ast->functionDefinition.returnReferenceOrigin = NULL;
    ast->functionDefinition.hasExplicitReturnType = false;
    ast->functionDefinition.body = NULL;

    return ast;
}

static void parseFunctionReturnType(Parser* parser, ASTNode* ast)
{
    if (parser->currentToken.type != TOKEN_ARROW) {
        return;
    }

    consume(parser, TOKEN_ARROW);
    ast->functionDefinition.returnReferenceType = parseReferenceType(parser);
    ast->functionDefinition.typeId = parser->currentToken.type;
    consumeType(parser);
    ast->functionDefinition.hasExplicitReturnType = true;
}

static bool parseFunctionBody(Parser* parser, ASTNode* ast)
{
    if (isEndOfFile(parser)) {
        return false;
    }

    consume(parser, TOKEN_LBRACE);

    ASTNode* body = createASTNode(AST_COMPOUND);
    body->compound.scope = NULL;
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

static ASTNode* createAssignmentNode(Token operator, Token token, ASTNode* expr)
{
    ASTNode* ast = createASTNode(AST_ASSIGNMENT);
    ast->assignment.scope = NULL;
    ast->assignment.operator = operator;
    ast->assignment.token = token;
    ast->assignment.symbol = NULL;
    ast->assignment.expr = expr;
    ast->assignment.initializesBinding = false;

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

static ASTNode* createVariableDefinitionNode(Token token, StringObject* id, bool fixed)
{
    ASTNode* ast = createASTNode(AST_VARIABLE_DEFINITION);
    ast->variableDefinition.scope = NULL;
    ast->variableDefinition.id = id;
    ast->variableDefinition.token = token;
    ast->variableDefinition.fixed = fixed;
    ast->variableDefinition.typeId = TOKEN_UNKNOWN;
    ast->variableDefinition.referenceType = REFERENCE_NONE;
    ast->variableDefinition.position = 0;
    ast->variableDefinition.expr = NULL;

    return ast;
}

static void parseVariableType(Parser* parser, ASTNode* ast)
{
    if (parser->currentToken.type == TOKEN_COLON) {
        consume(parser, TOKEN_COLON);
        ast->variableDefinition.referenceType = parseReferenceType(parser);
        ast->variableDefinition.typeId = parser->currentToken.type;
        consumeType(parser);
        
        return;
    }

    if (parser->currentToken.type != TOKEN_EQUAL) {
        expectedTokenError(TOKEN_EQUAL, parser->currentToken);
    }
}

static bool parseVariableInitializer(Parser* parser, ASTNode* ast)
{
    if (parser->currentToken.type != TOKEN_EQUAL) {
        ast->variableDefinition.expr = createASTNode(AST_NONE);

        if (ast->variableDefinition.typeId == TOKEN_UNKNOWN) {
            ast->variableDefinition.typeId = TOKEN_I32;
        }

        return true;
    }

    consume(parser, TOKEN_EQUAL);
    ast->variableDefinition.expr = parseExpression(parser);

    return ast->variableDefinition.expr != NULL;
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
        default:
            return parseExpression(parser);
    }
}

static bool parseStatements(Parser* parser, Vector* nodes, TokenType type)
{
    Token token = parser->currentToken;

    while (token.type != type) {
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

    return parseTopLevelStatements(parser);
}
