#include "parser.h"
#include "ast.h"
#include "lexer.h"
#include "scope.h"
#include "table.h"
#include "service.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

static AST* expression();
static AST* identifier();
static AST* statements();
static AST* variable();

static Token currentToken;
static Token prevToken;
static Scope* currentScope;
static Scope* topLevel;

const char* assignmentError = "Error: Invalid assignment for variable %.*s on line %d:%d\n";
const char* invalidArgumentsError = "Error: Invalid arguments to function %.*s on line %d:%d\n";
const char* invalidOperandError = "Error: Invalid operand to unary %.*s on line %d:%d\n";
const char* invalidOperandsError = "Error: Invalid operands to binary %.*s on line %d:%d\n";
const char* invalidTypeError = "Error: Invalid type for variable %.*s on line %d:%d\n";
const char* identifierError = "Error: Expected identifier on line %d:%d\n";
const char* redefinitionError = "Error: Redefinition of %.*s on line %d:%d\n";
const char* undefinedError = "Error: %.*s is undefined on line %d:%d\n";
const char* unexpectedTokenError = "Error: Unexpected %.*s on line %d:%d\n";
const char* unexpectedLastTokenError = "Error: Unexpected end of input on line %d:%d\n";
const char* uninitializedError = "Error: %.*s is uninitialized on line %d:%d\n";

static void advance()
{
    prevToken = currentToken;
    currentToken = scanToken();
}

static Token peek()
{
    return currentToken;
}

static Token prev()
{
    return prevToken;
}

static void error(const char* message, Token token)
{
    fprintf(stderr, message, token.length, token.chars, token.line, token.column);
    exit(1);
}

static void lineError(const char* message, Token token)
{
    int column = token.column + token.length;

    fprintf(stderr, message, token.line, column);
    exit(1);
}

static void tokenError()
{
    if (currentToken.type == T_EOF) {
        lineError(unexpectedLastTokenError, prevToken);
    }

    error(unexpectedTokenError, currentToken);
}

static void consume(TokenType type)
{
    Token token = peek();

    if (token.type != type) {
        tokenError();
    }
    
    advance();
}

static void consumeType()
{
    Token token = peek();

    if (!isTypeToken(token.type)) {
        tokenError();
    }

    consume(token.type);
}

static AST* booleanLiteral(Token token)
{
    AST* ast = createAST(AST_BOOLEAN);
    ast->boolVal = token.type == T_TRUE;
    consume(token.type);

    return ast;
}

static AST* decimalLiteral(Token token)
{
    AST* ast = createAST(AST_INTEGER);
    ast->intVal = strtol(token.chars, NULL, 10);
    consume(token.type);

    return ast;
}

static AST* floatLiteral(Token token)
{
    AST* ast = createAST(AST_FLOAT);
    ast->floatVal = strtod(token.chars, NULL);
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

static AST* characterLiteral(Token token)
{
    AST* ast = createAST(AST_CHARACTER);
    ast->character = token;
    consume(token.type);

    return ast;
}

static AST* groupExpression()
{
    consume(T_LPAREN);
    AST* ast = expression();
    
    if (isNone(ast)) {
        tokenError();
    }

    consume(T_RPAREN);

    return ast;
}

static AST* primary()
{
    Token token = peek();
    
    switch (token.type) {
        case T_TRUE:
        case T_FALSE:
            return booleanLiteral(token);
        case T_DECIMAL_LITERAL:
            return decimalLiteral(token);
        case T_FLOAT_LITERAL:
            return floatLiteral(token);
        case T_STRING_LITERAL:
            return stringLiteral(token);
        case T_CHARACTER_LITERAL:
            return characterLiteral(token);
        case T_LPAREN:
            return groupExpression();
        case T_IDENTIFIER:
            return identifier();
    }
    
    return createAST(AST_NONE);
}

static AST* postfix()
{
    if (!isPostfixToken(peek().type)) {
        return primary();
    }

    Token token = peek();
    AST* ast = createAST(AST_POSTFIX);
    ast->postfix.operator = token;
    ast->postfix.expr = variable();

    consume(token.type);

    return ast;
}

static AST* prefix()
{
    if (!isPrefixToken(peek().type)) {
        return postfix();
    }

    Token token = peek();
    AST* ast = createAST(AST_PREFIX);
    ast->prefix.operator = token;

    consume(token.type);

    if (peek().type == T_IDENTIFIER) {
        consume(T_IDENTIFIER);
        ast->prefix.expr = variable();
    } else if (!isPostfixToken(token.type)) {
        ast->prefix.expr = prefix();
    } else {
        tokenError();
    }

    return ast;
}

static AST* binary(AST* leftExpr, AST* rightExpr, Token token)
{
    if (isNone(rightExpr)) {
        tokenError();
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
    Token token = peek();

    while (token.type == T_POWER) {
        consume(token.type);
        expr = binary(expr, prefix(), token);
        token = peek();
    }

    return expr;
}

static AST* factor()
{
    AST* expr = exponent();
    Token token = peek();

    while (isFactorToken(token.type)) {
        consume(token.type);
        expr = binary(expr, exponent(), token);
        token = peek();
    }

    return expr;
}

static AST* term()
{
    AST* expr = factor();
    Token token = peek();

    while (isTermToken(token.type)) {
        consume(token.type);
        expr = binary(expr, factor(), token);
        token = peek();
    }

    return expr;
}

static AST* shift()
{
    AST* expr = term();
    Token token = peek();

    while (isShiftToken(token.type)) {
        consume(token.type);
        expr = binary(expr, term(), token);
        token = peek();
    }

    return expr;
}

static AST* comparison()
{
    AST* expr = shift();
    Token token = peek();

    while (isComparisonToken(token.type)) {
        consume(token.type);
        expr = binary(expr, shift(), token);
        token = peek();
    }

    return expr;
}

static AST* equality()
{
    AST* expr = comparison();
    Token token = peek();

    while (isEqualityToken(token.type)) {
        consume(token.type);
        expr = binary(expr, comparison(), token);
        token = peek();
    }

    return expr;
}

static AST* bitwiseAND()
{
    AST* expr = equality();
    Token token = peek();

    while (token.type == T_AMPERSAND) {
        consume(token.type);
        expr = binary(expr, equality(), token);
        token = peek();
    }

    return expr;
}

static AST* bitwiseXOR()
{
    AST* expr = bitwiseAND();
    Token token = peek();

    while (token.type == T_CIRCUMFLEX) {
        consume(token.type);
        expr = binary(expr, bitwiseAND(), token);
        token = peek();
    }

    return expr;
}

static AST* bitwiseOR()
{
    AST* expr = bitwiseXOR();
    Token token = peek();

    while (token.type == T_PIPE) {
        consume(token.type);
        expr = binary(expr, bitwiseXOR(), token);
        token = peek();
    }

    return expr;
}

static AST* booleanAND()
{
    AST* expr = bitwiseOR();
    Token token = peek();

    while (token.type == T_BOOLEAN_AND) {
        consume(token.type);
        expr = binary(expr, bitwiseOR(), token);
        token = peek();
    }

    return expr;
}

static AST* booleanOR()
{
    AST* expr = booleanAND();
    Token token = peek();

    while (token.type == T_BOOLEAN_OR) {
        consume(token.type);
        expr = binary(expr, booleanAND(), token);
        token = peek();
    }

    return expr;
}

static AST* expression()
{
    return booleanOR();
}

static AST* returnStatement()
{
    if (currentScope->level < 2) {
        tokenError();
    }

    consume(T_RETURN);
    
    AST* ast = createAST(AST_RETURN);
    ast->expr = expression();

    return ast;
}

static AST* argument()
{
    AST* expr = expression();

    if (isNone(expr)) {
        tokenError();
    }

    return expr;
}

static AST* parameter()
{
    Token token = peek();
    StringObject* id = copyString(token.chars, token.length);
    AST* symbol = getLocalSymbol(currentScope, id);

    if (symbol) {
        error(redefinitionError, token);
    }
    
    consume(T_IDENTIFIER);

    AST* ast = createAST(AST_PARAMETER);
    ast->param.scope = currentScope;
    ast->param.id = id;
    ast->param.typeId = T_INT;

    if (isTypeToken(peek().type)) {
        ast->param.typeId = peek().type;
        consumeType();
    }

    setLocalSymbol(currentScope, id, ast, false);

    return ast;
}

static AST* variable()
{
    Token token = prev();
    StringObject* id = copyString(token.chars, token.length);
    AST* symbol = getLocalSymbol(currentScope, id);

    if (!symbol) {
        symbol = getLocalSymbol(topLevel, id);
    }

    if (!symbol || !isVariableType(symbol)) {
        error(undefinedError, token);
    }
    
    if (!isInitialized(symbol)) {
        error(uninitializedError, token);
    }

    freeString(id);

    AST* ast = createAST(AST_VARIABLE);
    ast->var.scope = currentScope;
    ast->var.symbol = symbol;

    return ast;
}

static void compareFunctionSignature(AST* caller, AST* callee, Token token)
{
    size_t argCount = countVector(&caller->funcCall.args);
    size_t paramCount = countVector(&callee->funcDef.params);

    if (argCount != paramCount) {
        error(invalidArgumentsError, token);
    }

    for (int i = 0; i < argCount; i++) {
        AST* a = getVectorAt(&caller->funcCall.args, i);
        AST* b = getVectorAt(&callee->funcDef.params, i);
        int typeId = getTypeId(a);

        if (typeId != b->param.typeId) {
            error(invalidArgumentsError, token);
        }
    }
}

static void compareServiceSignature(AST* caller, Service* service, Token token)
{
    size_t argCount = countVector(&caller->syscall.args);

    if (argCount != service->paramCount) {
        error(invalidArgumentsError, token);
    }

    for (int i = 0; i < argCount; i++) {
        AST* expr = getVectorAt(&caller->syscall.args, i);
        int typeId = getTypeId(expr);

        if (typeId != service->params[i]) {
            error(invalidArgumentsError, token);
        }
    }
}

static void arguments(Vector* args)
{
    consume(T_LPAREN);

    while (peek().type != T_RPAREN) {
        AST* expr = argument();
        pushVectorItem(args, expr);

        if (peek().type != T_COMMA) {
            break;
        }
        
        consume(T_COMMA);
    }

    consume(T_RPAREN);
}

static void parameters(Vector* params)
{
    consume(T_LPAREN);

    int paramCount = 0;

    while (peek().type != T_RPAREN) {
        AST* expr = parameter();
        expr->param.position = paramCount++;
        pushVectorItem(params, expr);

        if (peek().type != T_COMMA) {
            break;
        }
        
        consume(T_COMMA);
    }
    
    consume(T_RPAREN);
}

static AST* systemCall(StringObject* id)
{
    Token token = prev();
    Service* service = getServiceByName(id->chars);
    
    freeString(id);

    if (!service) {
        error(undefinedError, token);
    }

    AST* ast = createAST(AST_SYSCALL);
    ast->syscall.opcode = service->opcode;
    ast->syscall.service = service;

    arguments(&ast->syscall.args);
    compareServiceSignature(ast, service, token);

    return ast;
}

static AST* functionCall()
{
    Token token = prev();
    StringObject* id = copyString(token.chars, token.length);
    AST* symbol = getSymbol(currentScope, id);
    
    if (!symbol) {
        return systemCall(id);
    }

    if (!symbol || symbol->type != AST_FUNCTION_DEFINITION) {
        error(undefinedError, token);
    }

    AST* ast = createAST(AST_FUNCTION_CALL);
    ast->funcCall.scope = currentScope;
    ast->funcCall.symbol = symbol;
    
    arguments(&ast->funcCall.args);
    compareFunctionSignature(ast, symbol, token);
    freeString(id);

    return ast;
}

static AST* functionDefinition()
{
    consume(T_FUNC);

    Token token = peek();
    StringObject* id = copyString(token.chars, token.length);
    AST* symbol = getLocalSymbol(currentScope, id);

    if (symbol) {
        error(redefinitionError, token);
    }

    if (peek().type != T_IDENTIFIER) {
        lineError(identifierError, prev());
    }

    AST* ast = createAST(AST_FUNCTION_DEFINITION);
    ast->funcDef.scope = currentScope;
    ast->funcDef.id = id;
    ast->funcDef.typeId = T_INT;

    consume(T_IDENTIFIER);
    currentScope = createScope(currentScope);
    parameters(&ast->funcDef.params);

    if (isTypeToken(peek().type)) {
        ast->funcDef.typeId = peek().type;
        consumeType();
    }

    consume(T_LBRACE);
    ast->funcDef.body = statements(T_RBRACE);
    consume(T_RBRACE);
    currentScope = currentScope->parent;
    setLocalSymbol(currentScope, id, ast, false);

    return ast;
}

static void assignmentExpression(AST* ast, Token token)
{
    AST* expr = expression();

    if (isNone(expr)) {
        tokenError();
    }

    int typeId = getTypeId(expr);
    if (typeId < 0) {
        error(assignmentError, token);
    }

    switch (ast->type) {
        case AST_VARIABLE_DEFINITION:
            ast->varDef.expr = expr;
            ast->varDef.typeId = typeId;
            return;
        case AST_ASSIGNMENT:
            ast->assignment.expr = expr;
            return;
    }

    tokenError();
}

static AST* assignment()
{
    Token operator = peek();
    Token token = prev();
    StringObject* id = copyString(token.chars, token.length);
    AST* symbol = getSymbol(currentScope, id);

    if (!symbol) {
        error(undefinedError, token);
    }

    if (currentScope != getScope(symbol) && !isInitialized(symbol)) {
        error(uninitializedError, token);
    }
    
    freeString(id);

    AST* ast = createAST(AST_ASSIGNMENT);
    ast->assignment.scope = currentScope;
    ast->assignment.operator = operator;
    ast->assignment.symbol = symbol;

    consume(operator.type);
    assignmentExpression(ast, token);
    initialize(symbol);

    return ast;
}

static AST* variableDefinition()
{
    consume(T_VAR);

    Token token = peek();
    StringObject* id = copyString(token.chars, token.length);
    AST* symbol = getLocalSymbol(currentScope, id);

    if (symbol) {
        error(redefinitionError, token);
    }

    if (peek().type != T_IDENTIFIER) {
        lineError(identifierError, prev());
    }

    consume(T_IDENTIFIER);

    AST* ast = createAST(AST_VARIABLE_DEFINITION);
    ast->varDef.scope = currentScope;
    ast->varDef.id = id;
    ast->varDef.typeId = T_INT;
    ast->varDef.position = getLocalCount(currentScope);

    if (isTypeToken(peek().type)) {
        ast->varDef.typeId = peek().type;
        consumeType();
    }
    
    if (peek().type != T_EQUAL) {
        ast->varDef.expr = createAST(AST_NONE);
    } else {
        consume(T_EQUAL);
        assignmentExpression(ast, token);
        initialize(ast);
    }

    if (ast->varDef.typeId == T_NONE) {
        error(invalidTypeError, token);
    }

    setLocalSymbol(currentScope, id, ast, true);

    return ast;
}

static AST* identifier()
{
    Token token = peek();

    consume(T_IDENTIFIER);
    
    if (isPostfixToken(peek().type)) {
        return postfix();
    }
    
    if (isAssignmentToken(peek().type)) {
        return assignment();
    }

    if (peek().type == T_LPAREN) {
        return functionCall();
    }

    return variable();
}

static AST* statement()
{
    Token token = peek();
    
    switch (token.type) {
        case T_FUNC:
            return functionDefinition();
        case T_VAR:
            return variableDefinition();
        case T_RETURN:
            return returnStatement();
        default:
            return expression();
    }

    tokenError();
}

static AST* statements(TokenType type)
{
    Token token = peek();
    AST* ast = createAST(AST_COMPOUND);

    ast->compound.scope = currentScope;

    while (token.type != type) {
        AST* stmt = statement();
        
        if (peek().line == token.line &&
            peek().type != type &&
            prev().type != T_RBRACE) {
            consume(T_SEMICOLON);
        }

        pushVectorItem(&ast->compound.statements, stmt);
        token = peek();
    }

    return ast;
}

AST* parse(char* source)
{
    initLexer(source);
    currentScope = createScope(NULL);
    topLevel = currentScope;
    advance();
    
    return statements(T_EOF);
}
