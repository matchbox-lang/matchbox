#include "parser.h"
#include "ast.h"
#include "conversion.h"
#include "lexer.h"
#include "scope.h"
#include "table.h"
#include "service.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

static AST* expression();
static AST* identifier();
static bool blocklevelStatements(Vector* nodes);

typedef struct Parser
{
    AST* ast;
    Token currentToken;
    Token prevToken;
    Scope* currentScope;
    Scope* topLevel;
} Parser;

static Parser parser;

static const char* invalidArgsError = "Error: Invalid arguments to function %.*s";
static const char* invalidOperandsError = "Error: Invalid operands to binary %.*s";
static const char* invalidTypeError = "Error: Invalid type for variable %.*s";
static const char* redefinitionError = "Error: Redefinition of %.*s";
static const char* undefinedError = "Error: %.*s is undefined";
static const char* unexpectedEndError = "Error: Unexpected end of input";
static const char* unexpectedTokenError = "Error: Unexpected %.*s";
static const char* uninitializedError = "Error: %.*s is uninitialized";

static void error(const char* message, Token token)
{
    fprintf(stderr, message, token.length, token.chars);
    fprintf(stderr, " on line %d:%d\n", token.line, token.column);
    exit(1);
}

static void advance()
{
    parser.prevToken = parser.currentToken;
    parser.currentToken = scanToken();
}

static void consume(TokenType type)
{
    if (parser.currentToken.type != type) {
        error(unexpectedTokenError, parser.currentToken);
    }
    
    advance();
}

static void consumeType()
{
    if (!isTypeToken(parser.currentToken.type)) {
        error(unexpectedTokenError, parser.currentToken);
    }

    consume(parser.currentToken.type);
}

static bool isEof()
{
    return parser.currentToken.type == T_EOF;
}


static Vector* getTopLevelStatements()
{
    return &parser.ast->compound.statements;
}

static AST* booleanLiteral(Token token)
{
    AST* ast = createAST(AST_BOOLEAN);
    ast->boolValue = token.type == T_TRUE;
    consume(token.type);

    return ast;
}

static AST* integerLiteral(Token token)
{
    AST* ast = createAST(AST_INTEGER);
    ast->intValue = integerLiteralToValue(token.chars, token.length);
    consume(token.type);

    return ast;
}

static AST* binaryLiteral(Token token)
{
    AST* ast = createAST(AST_INTEGER);
    ast->intValue = binaryLiteralToValue(token.chars, token.length);
    consume(token.type);

    return ast;
}

static AST* hexadecimalLiteral(Token token)
{
    AST* ast = createAST(AST_INTEGER);
    ast->intValue = hexadecimalLiteralToValue(token.chars, token.length);
    consume(token.type);

    return ast;
}

static AST* octalLiteral(Token token)
{
    AST* ast = createAST(AST_INTEGER);
    ast->intValue = octalLiteralToValue(token.chars, token.length);
    consume(token.type);

    return ast;
}

static AST* floatLiteral(Token token)
{
    AST* ast = createAST(AST_FLOAT);
    ast->floatValue = floatLiteralToValue(token.chars, token.length);
    consume(token.type);

    return ast;
}

static AST* characterLiteral(Token token)
{
    AST* ast = createAST(AST_CHARACTER);
    ast->character = token;
    consume(token.type);

    return ast;
}

static AST* stringLiteral(Token token)
{
    AST* ast = createAST(AST_STRING);
    ast->string = token;
    consume(token.type);

    return ast;
}

static AST* groupExpression()
{
    consume(T_LPAREN);
    AST* ast = expression();

    if (!ast && parser.currentToken.type == T_RPAREN) {
        error(unexpectedTokenError, parser.currentToken);
    }
    
    if (!ast || isEof()) {
        freeAST(ast);
        return NULL;
    }

    consume(T_RPAREN);

    return ast;
}

static AST* variable()
{
    Token token = parser.prevToken;
    StringObject* id = copyStringObject(token.chars, token.length);
    AST* symbol = getLocalSymbol(parser.currentScope, id);

    if (!symbol) {
        symbol = getLocalSymbol(parser.topLevel, id);
    }

    freeStringObject(id);

    if (!symbol || !isVariableType(symbol)) {
        error(undefinedError, token);
    }
    
    if (!isInitialized(symbol)) {
        error(uninitializedError, token);
    }

    AST* ast = createAST(AST_VARIABLE);
    ast->var.scope = parser.currentScope;
    ast->var.symbol = symbol;

    return ast;
}

static AST* parameter()
{
    if (isEof()) {
        return NULL;
    }
    
    Token token = parser.currentToken;
    StringObject* id = copyStringObject(token.chars, token.length);
    AST* symbol = getLocalSymbol(parser.currentScope, id);

    if (symbol) {
        error(redefinitionError, token);
    }
    
    consume(T_IDENTIFIER);

    AST* ast = createAST(AST_PARAMETER);
    ast->param.scope = parser.currentScope;
    ast->param.id = id;
    ast->param.typeId = T_INT;
    ast->param.position = getLocalCount(parser.currentScope);

    if (isTypeToken(parser.currentToken.type)) {
        ast->param.typeId = parser.currentToken.type;
        consumeType();
    }

    setLocalVariableSymbol(parser.currentScope, id, ast);

    return ast;
}

static AST* primary()
{
    switch (parser.currentToken.type) {
        case T_TRUE:
        case T_FALSE:
            return booleanLiteral(parser.currentToken);
        case T_INTEGER_LITERAL:
            return integerLiteral(parser.currentToken);
        case T_BINARY_LITERAL:
            return binaryLiteral(parser.currentToken);
        case T_HEXADECIMAL_LITERAL:
            return hexadecimalLiteral(parser.currentToken);
        case T_OCTAL_LITERAL:
            return octalLiteral(parser.currentToken);
        case T_FLOAT_LITERAL:
            return floatLiteral(parser.currentToken);
        case T_CHARACTER_LITERAL:
            return characterLiteral(parser.currentToken);
        case T_STRING_LITERAL:
            return stringLiteral(parser.currentToken);
        case T_LPAREN:
            return groupExpression();
        case T_IDENTIFIER:
            return identifier();
        case T_EOF:
            return NULL;
    }

    error(unexpectedTokenError, parser.currentToken);
}

static AST* postfix()
{
    if (!isPostfixToken(parser.currentToken.type)) {
        return primary();
    }

    Token token = parser.currentToken;

    AST* expr = variable();
    if (!expr) {
        return NULL;
    }

    AST* ast = createAST(AST_POSTFIX);
    ast->postfix.expr = expr;
    ast->postfix.operator = token;
    consume(token.type);

    return ast;
}

static AST* prefix()
{
    if (!isPrefixToken(parser.currentToken.type)) {
        return postfix();
    }

    Token token = parser.currentToken;
    consume(token.type);

    if (isEof()) {
        return NULL;
    }

    AST* ast = createAST(AST_PREFIX);
    ast->prefix.operator = token;

    if (parser.currentToken.type == T_IDENTIFIER) {
        consume(T_IDENTIFIER);
        ast->prefix.expr = variable();
    } else if (!isPostfixToken(token.type)) {
        ast->prefix.expr = prefix();
    } else {
        error(unexpectedTokenError, parser.currentToken);
    }

    if (!ast->prefix.expr) {
        freeAST(ast);
        return NULL;
    }

    return ast;
}

static AST* binary(AST* leftExpr, AST* rightExpr, Token token)
{
    if (!rightExpr) {
        return NULL;
    }

    int a = getTypeId(leftExpr);
    int b = getTypeId(rightExpr);

    if (a != b) {
        error(invalidOperandsError, token);
    }

    int typeId = isBoolOperatorToken(token.type) ? T_BOOL : a;
    
    AST* ast = createAST(AST_BINARY);
    ast->binary.leftExpr = leftExpr;
    ast->binary.operator = token;
    ast->binary.rightExpr = rightExpr;
    ast->binary.typeId = typeId;

    return ast;
}

static AST* exponent()
{
    AST* expr = prefix();
    Token token = parser.currentToken;

    while (token.type == T_POWER) {
        consume(token.type);
        expr = binary(expr, prefix(), token);
        token = parser.currentToken;
    }

    return expr;
}

static AST* factor()
{
    AST* expr = exponent();
    Token token = parser.currentToken;

    while (isFactorToken(token.type)) {
        consume(token.type);
        expr = binary(expr, exponent(), token);
        token = parser.currentToken;
    }

    return expr;
}

static AST* term()
{
    AST* expr = factor();
    Token token = parser.currentToken;

    while (isTermToken(token.type)) {
        consume(token.type);
        expr = binary(expr, factor(), token);
        token = parser.currentToken;
    }

    return expr;
}

static AST* shift()
{
    AST* expr = term();
    Token token = parser.currentToken;

    while (isShiftToken(token.type)) {
        consume(token.type);
        expr = binary(expr, term(), token);
        token = parser.currentToken;
    }

    return expr;
}

static AST* comparison()
{
    AST* expr = shift();
    Token token = parser.currentToken;

    while (isComparisonToken(token.type)) {
        consume(token.type);
        expr = binary(expr, shift(), token);
        token = parser.currentToken;
    }

    return expr;
}

static AST* equality()
{
    AST* expr = comparison();
    Token token = parser.currentToken;

    while (isEqualityToken(token.type)) {
        consume(token.type);
        expr = binary(expr, comparison(), token);
        token = parser.currentToken;
    }

    return expr;
}

static AST* bitwiseAND()
{
    AST* expr = equality();
    Token token = parser.currentToken;

    while (token.type == T_AMPERSAND) {
        consume(token.type);
        expr = binary(expr, equality(), token);
        token = parser.currentToken;
    }

    return expr;
}

static AST* bitwiseXOR()
{
    AST* expr = bitwiseAND();
    Token token = parser.currentToken;

    while (token.type == T_CIRCUMFLEX) {
        consume(token.type);
        expr = binary(expr, bitwiseAND(), token);
        token = parser.currentToken;
    }

    return expr;
}

static AST* bitwiseOR()
{
    AST* expr = bitwiseXOR();
    Token token = parser.currentToken;

    while (token.type == T_PIPE) {
        consume(token.type);
        expr = binary(expr, bitwiseXOR(), token);
        token = parser.currentToken;
    }

    return expr;
}

static AST* booleanAND()
{
    AST* expr = bitwiseOR();
    Token token = parser.currentToken;

    while (token.type == T_BOOLEAN_AND) {
        consume(token.type);
        expr = binary(expr, bitwiseOR(), token);
        token = parser.currentToken;
    }

    return expr;
}

static AST* booleanOR()
{
    AST* expr = booleanAND();
    Token token = parser.currentToken;

    while (token.type == T_BOOLEAN_OR) {
        consume(token.type);
        expr = binary(expr, booleanAND(), token);
        token = parser.currentToken;
    }

    return expr;
}

static AST* expression()
{
    return booleanOR();
}

static AST* returnStatement()
{
    if (parser.currentScope->level < 2) {
        error(unexpectedTokenError, parser.currentToken);
    }

    consume(T_RETURN);
    
    AST* expr = expression();
    if (!expr) {
        return NULL;
    }

    AST* ast = createAST(AST_RETURN);
    ast->expr = expr;

    return ast;
}

static void compareFunctionSignature(AST* caller, AST* callee, Token token)
{
    size_t argCount = countVector(&caller->funcCall.args);
    size_t paramCount = countVector(&callee->funcDef.params);

    if (argCount != paramCount) {
        error(invalidArgsError, token);
    }

    for (int i = 0; i < argCount; i++) {
        AST* a = getVectorAt(&caller->funcCall.args, i);
        AST* b = getVectorAt(&callee->funcDef.params, i);
        int typeId = getTypeId(a);

        if (typeId != b->param.typeId) {
            error(invalidArgsError, token);
        }
    }
}

static void compareServiceSignature(AST* caller, Service* service, Token token)
{
    size_t argCount = countVector(&caller->syscall.args);

    if (argCount != service->paramCount) {
        error(invalidArgsError, token);
    }

    for (int i = 0; i < argCount; i++) {
        AST* expr = getVectorAt(&caller->syscall.args, i);
        int typeId = getTypeId(expr);

        if (typeId != service->params[i]) {
            error(invalidArgsError, token);
        }
    }
}

static bool arguments(Vector* args)
{
    consume(T_LPAREN);

    if (isEof()) {
        return false;
    }

    while (parser.currentToken.type != T_RPAREN) {
        AST* expr = expression();

        if (!expr && (parser.currentToken.type == T_COMMA || parser.currentToken.type == T_RPAREN)) {
            error(unexpectedTokenError, parser.currentToken);
        }

        if (!expr) {
            return false;
        }

        pushVectorItem(args, expr);

        if (parser.currentToken.type == T_COMMA) {
            consume(T_COMMA);
        }
    }

    if (isEof()) {
        return false;
    }

    consume(T_RPAREN);

    return true;
}

static bool parameters(Vector* params)
{
    consume(T_LPAREN);

    if (isEof()) {
        return false;
    }

    while (parser.currentToken.type != T_RPAREN) {
        AST* expr = parameter();

        if (!expr && (parser.currentToken.type == T_COMMA || parser.currentToken.type == T_RPAREN)) {
            error(unexpectedTokenError, parser.currentToken);
        }

        if (!expr) {
            return false;
        }

        pushVectorItem(params, expr);

        if (parser.currentToken.type == T_COMMA) {
            consume(T_COMMA);
        }
    }

    if (isEof()) {
        return false;
    }
    
    consume(T_RPAREN);

    return true;
}

static AST* systemCall(Token token)
{
    StringObject* id = copyStringObject(token.chars, token.length);
    Service* service = getServiceByName(id->chars);

    if (!service) {
        error(undefinedError, token);
    }

    freeStringObject(id);

    AST* ast = createAST(AST_SYSCALL);
    ast->syscall.opcode = service->opcode;
    ast->syscall.service = service;

    if (!arguments(&ast->syscall.args)) {
        freeAST(ast);
        return NULL;
    }

    compareServiceSignature(ast, service, token);

    return ast;
}

static AST* functionCall()
{
    Token token = parser.prevToken;
    StringObject* id = copyStringObject(token.chars, token.length);
    AST* symbol = getSymbol(parser.currentScope, id);
    
    freeStringObject(id);
    
    if (!symbol) {
        return systemCall(token);
    }

    if (!symbol || symbol->type != AST_FUNCTION_DEFINITION) {
        error(undefinedError, token);
    }

    AST* ast = createAST(AST_FUNCTION_CALL);
    ast->funcCall.scope = parser.currentScope;
    ast->funcCall.symbol = symbol;

    if (!arguments(&ast->funcCall.args)) {
        freeAST(ast);
        return NULL;
    }

    compareFunctionSignature(ast, symbol, token);

    return ast;
}

static AST* functionDefinition()
{
    consume(T_FUNC);

    if (isEof()) {
        return NULL;
    }

    Token token = parser.currentToken;
    StringObject* id = copyStringObject(token.chars, token.length);
    AST* symbol = getLocalSymbol(parser.currentScope, id);

    if (symbol) {
        error(redefinitionError, token);
    }

    if (parser.currentToken.type != T_IDENTIFIER) {
        error(unexpectedTokenError, parser.currentToken);
    }

    consume(T_IDENTIFIER);

    if (isEof()) {
        return NULL;
    }

    AST* ast = createAST(AST_FUNCTION_DEFINITION);
    ast->funcDef.scope = parser.currentScope;
    ast->funcDef.id = id;
    ast->funcDef.typeId = T_INT;
    ast->funcDef.body = NULL;

    parser.currentScope = createScope(parser.currentScope);
    
    if (!parameters(&ast->funcDef.params)) {
        freeAST(ast);
        return NULL;
    }

    if (isTypeToken(parser.currentToken.type)) {
        ast->funcDef.typeId = parser.currentToken.type;
        consumeType();
    }

    if (isEof()) {
        freeAST(ast);
        return NULL;
    }

    consume(T_LBRACE);

    AST* body = createAST(AST_COMPOUND);
    body->compound.scope = parser.currentScope;

    if (!blocklevelStatements(&body->compound.statements)) {
        freeAST(ast);
        return NULL;
    }

    ast->funcDef.body = body;
    consume(T_RBRACE);
    parser.currentScope = parser.currentScope->parent;
    setLocalSymbol(parser.currentScope, id, ast);

    return ast;
}

static AST* assignment()
{
    Token operator = parser.currentToken;
    Token token = parser.prevToken;
    StringObject* id = copyStringObject(token.chars, token.length);
    AST* symbol = getSymbol(parser.currentScope, id);

    freeStringObject(id);

    if (!symbol) {
        error(undefinedError, token);
    }

    if (parser.currentScope != getScope(symbol) && !isInitialized(symbol)) {
        error(uninitializedError, token);
    }
    
    consume(operator.type);
    
    AST* expr = expression();
    if (!expr) {
        return NULL;
    }

    AST* ast = createAST(AST_ASSIGNMENT);
    ast->assignment.scope = parser.currentScope;
    ast->assignment.operator = operator;
    ast->assignment.symbol = symbol;
    ast->assignment.expr = expr;

    initialize(symbol);

    return ast;
}

static AST* variableDefinition()
{
    consume(T_VAR);

    if (isEof()) {
        return NULL;
    }

    Token token = parser.currentToken;
    StringObject* id = copyStringObject(token.chars, token.length);
    AST* symbol = getLocalSymbol(parser.currentScope, id);

    if (symbol) {
        error(redefinitionError, token);
    }

    if (parser.currentToken.type != T_IDENTIFIER) {
        error(unexpectedTokenError, parser.currentToken);
    }

    consume(T_IDENTIFIER);

    if (isEof()) {
        return NULL;
    }

    AST* ast = createAST(AST_VARIABLE_DEFINITION);
    ast->varDef.scope = parser.currentScope;
    ast->varDef.id = id;
    ast->varDef.position = getLocalCount(parser.currentScope);
    ast->varDef.expr = NULL;

    if (isTypeToken(parser.currentToken.type)) {
        ast->varDef.typeId = parser.currentToken.type;
        consumeType();
    }
    
    if (parser.currentToken.type != T_EQUAL) {
        ast->varDef.expr = createAST(AST_NONE);
        ast->varDef.typeId = T_INT;
        setLocalVariableSymbol(parser.currentScope, id, ast);

        return ast;
    }
    
    consume(T_EQUAL);

    AST* expr = expression();
    if (!expr) {
        freeAST(ast);
        return NULL;
    }

    ast->varDef.typeId = getTypeId(expr);
    ast->varDef.expr = expr;

    if (ast->varDef.typeId == T_NONE) {
        error(invalidTypeError, token);
    }

    setLocalVariableSymbol(parser.currentScope, id, ast);
    initialize(ast);

    return ast;
}

static AST* identifier()
{
    consume(T_IDENTIFIER);
    
    if (isAssignmentToken(parser.currentToken.type)) {
        return assignment();
    } else if (isPostfixToken(parser.currentToken.type)) {
        return postfix();
    } else if (parser.currentToken.type == T_LPAREN) {
        return functionCall();
    }

    return variable();
}

static AST* statement()
{
    switch (parser.currentToken.type) {
        case T_FUNC:
            return functionDefinition();
        case T_VAR:
            return variableDefinition();
        case T_RETURN:
            return returnStatement();
    }

    return expression();
}

static bool statements(Vector* nodes, TokenType type)
{
    Token token = parser.currentToken;

    while (token.type != type) {
        AST* stmt = statement();
        if (!stmt) {
            return false;
        }
        
        if (!isEof() && 
            parser.currentToken.line == token.line &&
            parser.currentToken.type != type &&
            parser.prevToken.type != T_RBRACE) {
            consume(T_SEMICOLON);
        }

        pushVectorItem(nodes, stmt);
        token = parser.currentToken;
    }

    return true;
}

static bool blocklevelStatements(Vector* nodes)
{
    return statements(nodes, T_RBRACE);
}

static bool toplevelStatements()
{
    return statements(&parser.ast->compound.statements, T_EOF);
}

void initParser(AST* ast)
{
    parser.ast = ast;
    parser.currentScope = ast->compound.scope;
    parser.topLevel = parser.currentScope;
}

void parse(char* source)
{
    initLexer(source);
    advance();

    if (!toplevelStatements()) {
        error(unexpectedEndError, parser.currentToken);
    }
}
