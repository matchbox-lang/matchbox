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

    switch (type)
    {
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

static void freeASTVector(Vector* statements)
{
    size_t count = countVector(statements);

    for (size_t i = 0; i < count; i++) {
        AST* ast = getVectorAt(statements, i);
        freeAST(ast);
    }

    freeVector(statements);
}

void freeAST(AST* ast)
{
    if (!ast) {
        return;
    }

    switch (ast->type) {
        case AST_ASSIGNMENT:
            freeString(ast->assignment.id);
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
            freeString(ast->funcCall.id);
            freeASTVector(&ast->funcCall.args);
            break;
        case AST_FUNCTION_DEFINITION:
            freeString(ast->funcDef.id);
            freeASTVector(&ast->funcDef.params);
            freeAST(ast->funcDef.body);
            break;
        case AST_PARAMETER:
            freeString(ast->param.id);
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
        case AST_VARIABLE:
            freeString(ast->var.id);
            break;
        case AST_VARIABLE_DEFINITION:
            freeString(ast->varDef.id);
            freeAST(ast->varDef.expr);
            break;
    }

    free(ast);
}

int getTypeId(AST* expr)
{
    switch (expr->type) {
        case AST_BINARY:
            return expr->binary.typeId;
        case AST_FUNCTION_CALL:
            return getTypeId(expr->funcCall.symbol);
        case AST_FUNCTION_DEFINITION:
            return expr->funcDef.typeId;
        case AST_PARAMETER:
            return expr->param.typeId;
        case AST_SYSCALL:
            return expr->syscall.service->typeId;
        case AST_VARIABLE:
            return getTypeId(expr->var.symbol);
        case AST_VARIABLE_DEFINITION:
            return expr->varDef.typeId;
        case AST_PREFIX:
            return getTypeId(expr->prefix.expr);
        case AST_POSTFIX:
            return getTypeId(expr->postfix.expr);
        case AST_INTEGER:
            return T_INT;
    }

    return -1;
}
