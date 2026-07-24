#include "optimizer.h"
#include "token.h"
#include "vector.h"
#include <math.h>

static void optimizeNode(ASTNode* ast);

static bool foldDivision(int left, int right, int* value)
{
    if (!right) {
        return false;
    }

    *value = left / right;

    return true;
}

static bool foldRemainder(int left, int right, int* value)
{
    if (!right) {
        return false;
    }

    *value = left % right;
    
    return true;
}

static bool foldBinaryValue(ASTNode* ast, int* value)
{
    int left = ast->binary.leftExpr->integerLiteral.value;
    int right = ast->binary.rightExpr->integerLiteral.value;

    switch (ast->binary.operator.type) {
        case TOKEN_PLUS:
            *value = left + right;
            return true;
        case TOKEN_MINUS:
            *value = left - right;
            return true;
        case TOKEN_STAR:
            *value = left * right;
            return true;
        case TOKEN_SLASH:
            return foldDivision(left, right, value);
        case TOKEN_PERCENT:
            return foldRemainder(left, right, value);
        case TOKEN_POWER:
            *value = (int)pow(left, right);
            return true;
        case TOKEN_AMPERSAND:
            *value = left & right;
            return true;
        case TOKEN_PIPE:
            *value = left | right;
            return true;
        case TOKEN_CIRCUMFLEX:
            *value = left ^ right;
            return true;
        default: return false;
    }
}

static void foldBinary(ASTNode* ast)
{
    if (ast->binary.leftExpr->type != AST_INTEGER) {
        return;
    }

    if (ast->binary.rightExpr->type != AST_INTEGER) {
        return;
    }

    if (getTypeId(ast->binary.leftExpr) != TOKEN_I32
        || getTypeId(ast->binary.rightExpr) != TOKEN_I32) {
        return;
    }

    int value;
    Token token = ast->binary.operator;
    
    if (!foldBinaryValue(ast, &value)) {
        return;
    }

    freeASTNode(ast->binary.leftExpr);
    freeASTNode(ast->binary.rightExpr);

    ast->type = AST_INTEGER;
    ast->integerLiteral.value = value;
    ast->integerLiteral.token = token;
    ast->integerLiteral.typeId = TOKEN_I32;
}

static void optimizeNodes(Vector* nodes)
{
    size_t count = countVector(nodes);

    for (size_t i = 0; i < count; i++) {
        optimizeNode(getVectorAt(nodes, i));
    }
}

static void optimizeNode(ASTNode* ast)
{
    switch (ast->type) {
        case AST_ASSIGNMENT:
            optimizeNode(ast->assignment.expr);
            break;
        case AST_BINARY:
            optimizeNode(ast->binary.leftExpr);
            optimizeNode(ast->binary.rightExpr);
            foldBinary(ast);
            break;
        case AST_BUILTIN_CALL:
            optimizeNodes(&ast->builtinCall.args);
            break;
        case AST_FUNCTION_CALL:
            optimizeNodes(&ast->functionCall.args);
            break;
        case AST_FUNCTION_DEFINITION:
            optimizeNodes(&ast->functionDefinition.body->compound.statements);
            break;
        case AST_PREFIX:
            optimizeNode(ast->prefix.expr);
            break;
        case AST_RETURN:
            optimizeNode(ast->returnStatement.expr);
            break;
        case AST_VARIABLE_DEFINITION:
            optimizeNode(ast->variableDefinition.expr);
            break;
        default:
            break;
    }
}

void optimize(ASTNode* ast, size_t start)
{
    size_t count = countVector(&ast->compound.statements);

    for (size_t i = start; i < count; i++) {
        optimizeNode(getVectorAt(&ast->compound.statements, i));
    }
}
