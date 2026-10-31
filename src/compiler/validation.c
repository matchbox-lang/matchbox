#include "compiler/validation.h"
#include "compiler/constant.h"
#include "compiler/diagnostics.h"

static void validateNode(ASTNode* ast, bool checkZero);

static void validateNodes(Vector* nodes, size_t start, bool checkZero)
{
    for (size_t i = start; i < nodes->count; i++) {
        ASTNode* node = nodes->data[i];

        validateNode(node, checkZero);
    }
}

static void validateZeroDivisor(ASTNode* divisor, Token operator, DataType type, bool checkZero)
{
    bool division = operator.type == TOKEN_SLASH || operator.type == TOKEN_SLASH_EQUAL;
    bool remainder = operator.type == TOKEN_PERCENT || operator.type == TOKEN_PERCENT_EQUAL;

    if (!checkZero || !isIntegerType(type) || (!division && !remainder)) {
        return;
    }

    ConstantValue value = {0};
    bool constant = evaluateConstant(divisor, &value);

    if (!constant || value.integer != 0) {
        return;
    }

    fatalSourceError(operator.line, operator.column, "Division by zero");
}

static bool shouldCheckRightDivisor(ASTNode* ast, bool checkZero)
{
    if (!checkZero) {
        return false;
    }

    if (!isLogicalToken(ast->binary.operator.type)) {
        return true;
    }

    ConstantValue left = {0};
    bool constant = evaluateConstant(ast->binary.leftExpr, &left);

    if (!constant) {
        return false;
    }

    return (left.integer != 0) == (ast->binary.operator.type == TOKEN_AND);
}

static void validateBinary(ASTNode* ast, bool checkZero)
{
    validateNode(ast->binary.leftExpr, checkZero);

    bool checkRight = shouldCheckRightDivisor(ast, checkZero);

    validateNode(ast->binary.rightExpr, checkRight);
    validateZeroDivisor(ast->binary.rightExpr, ast->binary.operator, ast->binary.operandType, checkZero);
}

static void validateConversion(ASTNode* ast, bool checkZero)
{
    validateNode(ast->conversion.expr, checkZero);

    ConstantValue value = {0};
    bool constant = evaluateConstant(ast, &value);
    DataType source = getNodeDataType(ast->conversion.expr);

    if (constant || ast->conversion.typeId != TYPE_CHAR || source == TYPE_U8 || source == TYPE_CHAR) {
        return;
    }

    Token token = ast->conversion.token;

    fatalSourceError(token.line, token.column, "Integer-to-char conversion requires a constant or u8 source");
}

static void validateAssignment(ASTNode* ast, bool checkZero)
{
    validateNode(ast->assignment.expr, checkZero);

    DataType type = getNodeDataType(ast->assignment.symbol);

    validateZeroDivisor(ast->assignment.expr, ast->assignment.operator, type, checkZero);
}

static void validateConditional(ASTNode* ast, bool checkZero)
{
    validateNode(ast->conditional.condition, checkZero);
    validateNode(ast->conditional.thenBranch, checkZero);
    validateNode(ast->conditional.elseBranch, checkZero);
}

static void validateMatchArm(MatchArm* arm, bool checkZero)
{
    validateNode(arm->pattern, checkZero);
    validateNode(arm->guard, checkZero);
    validateNode(arm->branch, checkZero);
}

static void validateMatch(ASTNode* ast, bool checkZero)
{
    validateNode(ast->match.subject, checkZero);

    for (size_t i = 0; i < ast->match.arms.count; i++) {
        MatchArm* arm = ast->match.arms.data[i];

        validateMatchArm(arm, checkZero);
    }

    validateNode(ast->match.defaultBranch, checkZero);
}

static void validateNode(ASTNode* ast, bool checkZero)
{
    if (!ast) {
        return;
    }

    switch (ast->type) {
        case AST_ASSIGNMENT:
            validateAssignment(ast, checkZero);
            break;
        case AST_BINARY:
            validateBinary(ast, checkZero);
            break;
        case AST_BUILTIN_CALL:
            validateNodes(&ast->builtinCall.args, 0, checkZero);
            break;
        case AST_COMPOUND:
            validateNodes(&ast->compound.statements, 0, checkZero);
            break;
        case AST_CONDITIONAL:
            validateConditional(ast, checkZero);
            break;
        case AST_CONVERSION:
            validateConversion(ast, checkZero);
            break;
        case AST_FUNCTION_CALL:
            validateNodes(&ast->functionCall.args, 0, checkZero);
            break;
        case AST_FUNCTION_DEFINITION:
            validateNode(ast->functionDefinition.body, checkZero);
            break;
        case AST_MATCH:
            validateMatch(ast, checkZero);
            break;
        case AST_PREFIX:
            validateNode(ast->prefix.expr, checkZero);
            break;
        case AST_RETURN:
            validateNode(ast->returnStatement.expr, checkZero);
            break;
        case AST_VARIABLE_DEFINITION:
            validateNode(ast->variableDefinition.expr, checkZero);
            break;
        case AST_WHILE:
            validateNode(ast->whileStatement.condition, checkZero);
            validateNode(ast->whileStatement.body, checkZero);
            break;
        default:
            break;
    }
}

void validateConstantExpressions(ASTNode* ast, size_t start)
{
    validateNodes(&ast->compound.statements, start, true);
}
