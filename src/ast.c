#include "ast.h"
#include "object.h"
#include "scope.h"
#include "service.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

AST* createAST(TokenType type)
{
    AST* ast = malloc(sizeof(AST));
    ast->type = type;

    switch (type) {
        case AST_COMPOUND:
            initVector(&ast->compound.statements);
            break;
        case AST_FUNCTION_CALL:
            initVector(&ast->funcCall.args);
            break;
        case AST_FUNCTION_DEFINITION:
            initVector(&ast->funcDef.params);
            break;
        case AST_SYSCALL:
            initVector(&ast->syscall.args);
            break;
    }
    
    return ast;
}

static void freeASTVector(Vector* nodes)
{
    size_t count = countVector(nodes);

    for (size_t i = 0; i < count; i++) {
        freeAST(nodes->data[i]);
    }

    freeVector(nodes);
}

void freeAST(AST* ast)
{
    if (!ast) {
        return;
    }

    switch (ast->type) {
        case AST_ASSIGNMENT:
            freeAST(ast->assignment.expr);
            break;
        case AST_BINARY:
            freeAST(ast->binary.leftExpr);
            freeAST(ast->binary.rightExpr);
            break;
        case AST_COMPOUND:
            freeScope(ast->compound.scope);
            freeASTVector(&ast->compound.statements);
            break;
        case AST_FUNCTION_CALL:
            freeASTVector(&ast->funcCall.args);
            break;
        case AST_FUNCTION_DEFINITION:
            freeStringObject(ast->funcDef.id);
            freeASTVector(&ast->funcDef.params);
            freeAST(ast->funcDef.body);
            break;
        case AST_PARAMETER:
            freeStringObject(ast->param.id);
            break;
        case AST_POSTFIX:
            freeAST(ast->postfix.expr);
            break;
        case AST_PREFIX:
            freeAST(ast->prefix.expr);
            break;
        case AST_RETURN:
            freeAST(ast->expr);
            break;
        case AST_VARIABLE_DEFINITION:
            freeStringObject(ast->varDef.id);
            freeAST(ast->varDef.expr);
            break;
    }

    free(ast);
}

Scope* getScope(AST* ast)
{
    switch (ast->type) {
        case AST_ASSIGNMENT:
            return ast->assignment.scope;
        case AST_COMPOUND:
            return ast->compound.scope;
        case AST_FUNCTION_CALL:
            return ast->funcCall.scope;
        case AST_FUNCTION_DEFINITION:
            return ast->funcDef.scope;
        case AST_PARAMETER:
            return ast->param.scope;
        case AST_VARIABLE:
            return ast->var.scope;
        case AST_VARIABLE_DEFINITION:
            return ast->varDef.scope;
    }

    return NULL;
}

int getTypeId(AST* ast)
{
    if (!ast) {
        return T_NONE;
    }

    switch (ast->type) {
        case AST_BINARY:
            return ast->binary.typeId;
        case AST_FUNCTION_CALL:
            return getTypeId(ast->funcCall.symbol);
        case AST_FUNCTION_DEFINITION:
            return ast->funcDef.typeId;
        case AST_PARAMETER:
            return ast->param.typeId;
        case AST_SYSCALL:
            return ast->syscall.service->typeId;
        case AST_VARIABLE:
            return getTypeId(ast->var.symbol);
        case AST_VARIABLE_DEFINITION:
            return ast->varDef.typeId;
        case AST_PREFIX:
            return getTypeId(ast->prefix.expr);
        case AST_POSTFIX:
            return getTypeId(ast->postfix.expr);
        case AST_INTEGER:
            return T_INT;
    }

    return T_NONE;
}

bool isFunctionCall(AST* ast)
{
    return ast->type == AST_FUNCTION_CALL;
}

bool isFunctionDefinition(AST* ast)
{
    return ast->type == AST_FUNCTION_DEFINITION;
}

bool isParameter(AST* ast)
{
    return ast->type == AST_PARAMETER;
}

bool isPrefix(AST* ast)
{
    return ast->type == AST_PREFIX;
}

bool isPrefixOnlyOperand(AST* ast)
{
    switch (ast->type) {
        case AST_ASSIGNMENT:
        case AST_BINARY:
        case AST_FUNCTION_CALL:
        case AST_INTEGER:
        case AST_SYSCALL:
        case AST_VARIABLE:
            return true;
    }

    return false;
}

bool isVariable(AST* ast)
{
    return ast->type == AST_VARIABLE;
}

bool isVariableDefinition(AST* ast)
{
    return ast->type == AST_VARIABLE_DEFINITION;
}

bool isVariableType(AST* ast)
{
    return ast->type == AST_PARAMETER || ast->type == AST_VARIABLE_DEFINITION;
}

bool isNone(AST* ast)
{
    return ast->type == AST_NONE;
}

bool isInitialized(AST* ast)
{
    if (ast->type == AST_VARIABLE_DEFINITION) {
        return ast->varDef.initialized;
    }

    return ast->type == AST_PARAMETER;
}

void initialize(AST* ast)
{
    if (isVariableDefinition(ast)) {
        ast->varDef.initialized = true;
    }
}
