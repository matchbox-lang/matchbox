#include "parser.h"
#include "ast.h"
#include "lexer.h"
#include "scope.h"
#include "table.h"
#include "syscall.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct Parser
{
    Token current;
    Token previous;
    Scope* scope;
} Parser;

Parser parser;

static AST* expression();
static AST* identifier();
static AST* statements();
static AST* variable();

static void advance()
{
    parser.previous = parser.current;
    parser.current = scanToken();
}

static Token peek()
{
    return parser.current;
}

static Token prev()
{
    return parser.previous;
}

static void error(const char* message, Token token)
{
    fprintf(
        stderr,
        message,
        token.length,
        token.chars,
        token.line,
        token.column
    );
    exit(1);
}

static void tokenError()
{
    error("Error: Unexpected '%.*s' on line %d:%d\n", peek());
}

static bool isBool(TokenType type)
{
    switch (type) {
        case T_TRUE:
        case T_FALSE:
            return true;
    }

    return false;
}

static bool isAssignment(TokenType type)
{
    switch (type) {
        case T_EQUAL:
        case T_PLUS_EQUAL:
        case T_MINUS_EQUAL:
        case T_STAR_EQUAL:
        case T_SLASH_EQUAL:
        case T_FLOOR_EQUAL:
        case T_PERCENT_EQUAL:
        case T_POWER_EQUAL:
            return true;
    }

    return false;
}

static bool isType(TokenType type)
{
    return type == T_INT;
}

static bool isEquality(TokenType type)
{
    switch (type) {
        case T_EQUAL_EQUAL:
        case T_NOT_EQUAL:
            return true;
    }

    return false;
}

static bool isComparison(TokenType type)
{
    switch (type) {
        case T_GREATER:
        case T_GREATER_EQUAL:
        case T_LESS:
        case T_LESS_EQUAL:
        case T_SPACESHIP:
            return true;
    }

    return false;
}

static bool isShift(TokenType type)
{
    switch (type) {
        case T_LSHIFT:
        case T_RSHIFT:
            return true;
    }

    return false;
}

static bool isTerm(TokenType type)
{
    switch (type) {
        case T_PLUS:
        case T_MINUS:
            return true;
    }

    return false;
}

static bool isFactor(TokenType type)
{
    switch (type) {
        case T_STAR:
        case T_SLASH:
        case T_FLOOR:
        case T_PERCENT:
            return true;
    }

    return false;
}

static bool isPrefix(TokenType type)
{
    switch (type) {
        case T_INCREMENT:
        case T_DECREMENT:
        case T_PLUS:
        case T_MINUS:
        case T_EXCLAMATION:
        case T_TILDE:
            return true;
    }

    return false;
}

static bool isPostfix(TokenType type)
{
    switch (type) {
        case T_INCREMENT:
        case T_DECREMENT:
            return true;
    }

    return false;
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

    if (!isType(token.type)) {
        tokenError();
    }

    consume(token.type);
}

static AST* primary()
{
    Token token = peek();

    if (isBool(token.type)) {
        AST* ast = createAST(AST_BOOLEAN);
        ast->boolVal = token.type == T_TRUE;
        consume(token.type);

        return ast;
    }

    if (token.type == T_DECIMAL_LITERAL) {
        AST* ast = createAST(AST_INTEGER);
        ast->intVal = strtol(token.chars, NULL, 10);
        consume(token.type);

        return ast;
    }

    if (token.type == T_FLOAT_LITERAL) {
        AST* ast = createAST(AST_FLOAT);
        ast->floatVal = strtod(token.chars, NULL);
        consume(token.type);

        return ast;
    }

    if (token.type == T_CHARACTER_LITERAL) {
        AST* ast = createAST(AST_CHARACTER);
        ast->character = token;
        consume(token.type);

        return ast;
    }

    if (token.type == T_STRING_LITERAL) {
        AST* ast = createAST(AST_STRING);
        ast->string = token;
        consume(token.type);

        return ast;
    }

    if (token.type == T_LPAREN) {
        consume(T_LPAREN);
        AST* ast = expression();
        consume(T_RPAREN);

        return ast;
    }

    if (token.type == T_IDENTIFIER) {
        return identifier();
    }
    
    return createAST(AST_NONE);
}

static AST* postfix()
{
    if (peek().type != T_IDENTIFIER) {
        return primary();
    }

    AST* expr = identifier();
    Token token = peek();

    if (!isPostfix(token.type)) {
        return expr;
    }

    AST* ast = createAST(AST_POSTFIX);
    ast->postfix.operator = token;
    consume(token.type);
    ast->postfix.expr = expr;

    return ast;
}

static AST* prefix()
{
    if (!isPrefix(peek().type)) {
        return postfix();
    }

    Token token = peek();
    AST* ast = createAST(AST_PREFIX);
    ast->prefix.operator = token;
    consume(token.type);

    if (peek().type == T_IDENTIFIER) {
        consume(T_IDENTIFIER);
        ast->prefix.expr = variable();
    } else {
        ast->prefix.expr = prefix();
    }

    return ast;

}

static AST* exponent()
{
    AST* expr = prefix();

    while (1) {
        Token token = peek();

        if (token.type != T_POWER) {
            break;
        }

        AST* ast = createAST(AST_BINARY);
        ast->binary.leftExpr = expr;
        ast->binary.operator = token;
        consume(token.type);

        AST* rightExpr = prefix();
        if (rightExpr->type == AST_NONE) {
            tokenError();
        }

        ast->binary.rightExpr = rightExpr;
        expr = ast;
    }

    return expr;
}

static AST* factor()
{
    AST* expr = exponent();

    while (1) {
        Token token = peek();

        if (!isFactor(token.type)) {
            break;
        }

        AST* ast = createAST(AST_BINARY);
        ast->binary.leftExpr = expr;
        ast->binary.operator = token;
        consume(token.type);

        AST* rightExpr = exponent();
        if (rightExpr->type == AST_NONE) {
            tokenError();
        }
        
        ast->binary.rightExpr = rightExpr;
        expr = ast;
    }

    return expr;
}

static AST* term()
{
    AST* expr = factor();

    while (1) {
        Token token = peek();

        if (!isTerm(token.type)) {
            break;
        }

        AST* ast = createAST(AST_BINARY);
        ast->binary.leftExpr = expr;
        ast->binary.operator = token;
        consume(token.type);

        AST* rightExpr = factor();
        if (rightExpr->type == AST_NONE) {
            tokenError();
        }
        
        ast->binary.rightExpr = rightExpr;
        expr = ast;
    }

    return expr;
}

static AST* shift()
{
    AST* expr = term();

    while (1) {
        Token token = peek();

        if (!isShift(token.type)) {
            break;
        }

        AST* ast = createAST(AST_BINARY);
        ast->binary.leftExpr = expr;
        ast->binary.operator = token;
        consume(token.type);

        AST* rightExpr = term();
        if (rightExpr->type == AST_NONE) {
            tokenError();
        }

        ast->binary.rightExpr = rightExpr;
        expr = ast;
    }

    return expr;
}

static AST* comparison()
{
    AST* expr = shift();

    while (1) {
        Token token = peek();

        if (!isComparison(token.type)) {
            break;
        }

        AST* ast = createAST(AST_BINARY);
        ast->binary.leftExpr = expr;
        ast->binary.operator = token;
        consume(token.type);

        AST* rightExpr = shift();
        if (rightExpr->type == AST_NONE) {
            tokenError();
        }

        ast->binary.rightExpr = rightExpr;
        expr = ast;
    }

    return expr;
}

static AST* equality()
{
    AST* expr = comparison();

    while (1) {
        Token token = peek();

        if (!isEquality(token.type)) {
            break;
        }

        AST* ast = createAST(AST_BINARY);
        ast->binary.leftExpr = expr;
        ast->binary.operator = token;
        consume(token.type);

        AST* rightExpr = comparison();
        if (rightExpr->type == AST_NONE) {
            tokenError();
        }

        ast->binary.rightExpr = rightExpr;
        expr = ast;
    }

    return expr;
}

static AST* bitwiseAND()
{
    AST* expr = equality();

    while (1) {
        Token token = peek();

        if (token.type != T_AMPERSAND) {
            break;
        }

        AST* ast = createAST(AST_BINARY);
        ast->binary.leftExpr = expr;
        ast->binary.operator = token;
        consume(token.type);

        AST* rightExpr = equality();
        if (rightExpr->type == AST_NONE) {
            tokenError();
        }

        ast->binary.rightExpr = rightExpr;
        expr = ast;
    }

    return expr;
}

static AST* bitwiseXOR()
{
    AST* expr = bitwiseAND();

    while (1) {
        Token token = peek();

        if (token.type != T_CIRCUMFLEX) {
            break;
        }

        AST* ast = createAST(AST_BINARY);
        ast->binary.leftExpr = expr;
        ast->binary.operator = token;
        consume(token.type);

        AST* rightExpr = bitwiseAND();
        if (rightExpr->type == AST_NONE) {
            tokenError();
        }

        ast->binary.rightExpr = rightExpr;
        expr = ast;
    }

    return expr;
}

static AST* bitwiseOR()
{
    AST* expr = bitwiseXOR();

    while (1) {
        Token token = peek();

        if (token.type != T_PIPE) {
            break;
        }

        AST* ast = createAST(AST_BINARY);
        ast->binary.leftExpr = expr;
        ast->binary.operator = token;
        consume(token.type);

        AST* rightExpr = bitwiseXOR();
        if (rightExpr->type == AST_NONE) {
            tokenError();
        }

        ast->binary.rightExpr = rightExpr;
        expr = ast;
    }

    return expr;
}

static AST* booleanAND()
{
    AST* expr = bitwiseOR();

    while (1) {
        Token token = peek();

        if (token.type != T_BOOLEAN_AND) {
            break;
        }

        AST* ast = createAST(AST_BINARY);
        ast->binary.leftExpr = expr;
        ast->binary.operator = token;
        consume(token.type);

        AST* rightExpr = bitwiseOR();
        if (rightExpr->type == AST_NONE) {
            tokenError();
        }

        ast->binary.rightExpr = rightExpr;
        expr = ast;
    }

    return expr;
}

static AST* booleanOR()
{
    AST* expr = booleanAND();

    while (1) {
        Token token = peek();

        if (token.type != T_BOOLEAN_OR) {
            break;
        }

        AST* ast = createAST(AST_BINARY);
        ast->binary.leftExpr = expr;
        ast->binary.operator = token;
        consume(token.type);

        AST* rightExpr = booleanAND();
        if (rightExpr->type == AST_NONE) {
            tokenError();
        }

        ast->binary.rightExpr = rightExpr;
        expr = ast;
    }

    return expr;
}

static AST* expression()
{
    return booleanOR();
}

static AST* returnStmt()
{
    consume(T_RETURN);
    
    AST* ast = createAST(AST_RETURN);
    ast->expr = expression();

    return ast;
}

static AST* argument()
{
    AST* expr = expression();

    if (expr->type == AST_NONE) {
        tokenError();
    }

    return expr;
}

static AST* parameter()
{
    Token token = peek();
    StringObject* id = copyString(token.chars, token.length);
    AST* symbol = getLocalSymbol(parser.scope, id);

    if (symbol) {
        error("Error: Redefinition of '%.*s' on line %d:%d\n", token);
    }
    
    consume(T_IDENTIFIER);

    AST* ast = createAST(AST_PARAMETER);
    ast->param.scope = parser.scope;
    ast->param.id = id;
    ast->param.typeId = T_NONE;

    if (isType(peek().type)) {
        ast->param.typeId = peek().type;
        consumeType();
    }

    setLocalSymbol(parser.scope, id, ast);

    return ast;
}

static AST* assignment()
{
    Token operator = peek();
    Token token = prev();
    StringObject* id = copyString(token.chars, token.length);
    AST* symbol = getSymbol(parser.scope, id);

    if (!symbol) {
        error("Error: '%.*s' is undefined on line %d:%d\n", token);
    }

    consume(operator.type);

    AST* ast = createAST(AST_ASSIGNMENT);
    ast->assignment.scope = parser.scope;
    ast->assignment.id = id;
    ast->assignment.operator = operator;

    AST* expr = expression();
    if (expr->type == AST_NONE) {
        tokenError();
    }

    ast->assignment.expr = expr;

    return ast;
}

static AST* variable()
{
    Token token = prev();
    StringObject* id = copyString(token.chars, token.length);
    AST* symbol = getSymbol(parser.scope, id);

    if (!symbol) {
        error("Error: '%.*s' is undefined on line %d:%d\n", token);
    }
    
    if (symbol->type == AST_VARIABLE_DEFINITION && symbol->varDef.expr->type == AST_NONE) {
        error("Error: '%.*s' is uninitialized on line %d:%d\n", token);
    }

    AST* ast = createAST(AST_VARIABLE);
    ast->var.scope = parser.scope;
    ast->var.id = id;

    return ast;
}

static AST* variableDefinition()
{
    consume(T_VAR);

    Token token = peek();
    StringObject* id = copyString(token.chars, token.length);
    AST* symbol = getLocalSymbol(parser.scope, id);

    if (symbol) {
        error("Error: Redefinition of '%.*s' on line %d:%d\n", token);
    }

    consume(T_IDENTIFIER);

    AST* ast = createAST(AST_VARIABLE_DEFINITION);
    ast->varDef.scope = parser.scope;
    ast->varDef.id = id;
    ast->varDef.typeId = T_NONE;

    if (isType(peek().type)) {
        ast->varDef.typeId = peek().type;
        consumeType();
    }
    
    if (peek().type != T_EQUAL) {
        ast->varDef.expr = createAST(AST_NONE);
    } else {
        consume(T_EQUAL);
        ast->varDef.expr = expression();
    }

    setLocalVariable(parser.scope, id, ast);

    return ast;
}

static void compareFunctionSignature(AST* caller, AST* callee, Token token)
{
    size_t argCount = countVector(&caller->funcCall.args);
    size_t paramCount = countVector(&callee->funcDef.params);

    if (argCount != paramCount) {
        error("Error: Incorrect arguments to function '%.*s' on line %d:%d\n", token);
    }
}

static void compareSyscallSignature(AST* caller, Syscall* syscall, Token token)
{
    size_t argCount = countVector(&caller->syscall.args);

    if (argCount != syscall->paramCount) {
        error("Error: Incorrect arguments to function '%.*s' on line %d:%d\n", token);
    }
}

static AST* systemCall(Syscall* syscall, Token token)
{
    AST* ast = createAST(AST_SYSCALL);
    ast->syscall.opcode = syscall->opcode;

    consume(T_LPAREN);

    if (peek().type != T_RPAREN) {
        AST* expr = argument();
        pushVector(&ast->syscall.args, expr);

        while (peek().type == T_COMMA) {
            consume(T_COMMA);
            AST* expr = argument();
            pushVector(&ast->syscall.args, expr);
        }
    }

    consume(T_RPAREN);
    compareSyscallSignature(ast, syscall, token);

    return ast;
}

static AST* functionCall()
{
    Token token = prev();
    StringObject* id = copyString(token.chars, token.length);
    Syscall* syscall = getSyscallByName(id->chars);
    
    if (syscall) {
        return systemCall(syscall, token);
    }

    AST* symbol = getSymbol(parser.scope, id);
    if (!symbol) {
        error("Error: '%.*s' is undefined on line %d:%d\n", token);
    }

    AST* ast = createAST(AST_FUNCTION_CALL);
    ast->funcCall.id = id;
    ast->funcCall.scope = parser.scope;

    consume(T_LPAREN);

    if (peek().type != T_RPAREN) {
        AST* expr = argument();
        pushVector(&ast->funcCall.args, expr);

        while (peek().type == T_COMMA) {
            consume(T_COMMA);
            AST* expr = argument();
            pushVector(&ast->funcCall.args, expr);
        }
    }

    consume(T_RPAREN);
    compareFunctionSignature(ast, symbol, token);

    return ast;
}

static AST* functionDefinition()
{
    consume(T_FUNC);

    Token token = peek();
    StringObject* id = copyString(token.chars, token.length);
    AST* symbol = getLocalSymbol(parser.scope, id);

    if (symbol) {
        error("Error: Redefinition of '%.*s' on line %d:%d\n", token);
    }

    AST* ast = createAST(AST_FUNCTION_DEFINITION);
    ast->funcDef.scope = parser.scope;
    ast->funcDef.id = id;
    ast->funcDef.typeId = T_NONE;

    consume(T_IDENTIFIER);
    consume(T_LPAREN);

    parser.scope = createScope(parser.scope);
    int paramCount = 0;

    if (peek().type != T_RPAREN) {
        AST* expr = parameter();
        expr->param.position = paramCount++;
        pushVector(&ast->funcDef.params, expr);

        while (peek().type == T_COMMA) {
            consume(T_COMMA);
            AST* expr = parameter();
            expr->param.position = paramCount++;
            pushVector(&ast->funcDef.params, expr);
        }
    }
    
    consume(T_RPAREN);

    if (isType(peek().type)) {
        ast->funcDef.typeId = peek().type;
        consumeType();
    }

    consume(T_LBRACE);
    ast->funcDef.body = statements(T_RBRACE);
    consume(T_RBRACE);
    parser.scope = parser.scope->parent;
    setLocalSymbol(parser.scope, id, ast);

    return ast;
}

static AST* identifier()
{
    consume(T_IDENTIFIER);

    if (isAssignment(peek().type)) {
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
        case T_UNKNOWN:
            tokenError();
        case T_VAR:
            return variableDefinition();
        case T_FUNC:
            return functionDefinition();
        case T_RETURN:
            return returnStmt();
        default:
            return expression();
    }
}

static AST* statements(TokenType type)
{
    Token token = peek();
    AST* ast = createAST(AST_COMPOUND);

    ast->compound.scope = parser.scope;

    while (token.type != type) {
        AST* stmt = statement();
        
        if (peek().line == token.line &&
            peek().type != type &&
            prev().type != T_RBRACE) {
            consume(T_SEMICOLON);
        }

        pushVector(&ast->compound.statements, stmt);
        token = peek();
    }

    return ast;
}

AST* parse(char* source)
{
    initLexer(source);
    parser.scope = createScope(NULL);
    advance();
    
    return statements(T_EOF);
}
