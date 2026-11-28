#include "compiler/ast/predicate.h"

bool is_constant_index(const ASTNode* ast)
{
    return is_index(ast) && ast->index.constant;
}

bool is_expression_statement(const ASTNode* ast)
{
    if (!ast) {
        return false;
    }

    switch (ast->type) {
        case ASTArray:
        case ASTTuple:
        case ASTSlice:
        case ASTIndex:
        case ASTBinary:
        case ASTBoolean:
        case ASTBuiltinCall:
        case ASTCharacter:
        case ASTConditional:
        case ASTConversion:
        case ASTFloat:
        case ASTFunctionCall:
        case ASTInteger:
        case ASTMatch:
        case ASTPrefix:
        case ASTString:
        case ASTVariable:
            return true;
        default:
            return false;
    }
}

bool is_function_call(const ASTNode* ast)
{
    return ast && ast->type == ASTFunctionCall;
}

bool is_function_definition(const ASTNode* ast)
{
    return ast && ast->type == ASTFunctionDefinition;
}

bool is_index(const ASTNode* ast)
{
    return ast && (ast->type == ASTIndex || ast->type == ASTIndexAssignment);
}

bool is_none(const ASTNode* ast)
{
    return ast && ast->type == ASTNone;
}

bool is_parameter(const ASTNode* ast)
{
    return ast && ast->type == ASTParameter;
}

bool is_prefix(const ASTNode* ast)
{
    return ast && ast->type == ASTPrefix;
}

bool is_prefix_operand(const ASTNode* ast)
{
    if (!ast) {
        return false;
    }

    switch (ast->type) {
        case ASTAssignment:
        case ASTArray:
        case ASTTuple:
        case ASTIndex:
        case ASTBinary:
        case ASTBoolean:
        case ASTBuiltinCall:
        case ASTCharacter:
        case ASTConditional:
        case ASTConversion:
        case ASTFloat:
        case ASTFunctionCall:
        case ASTInteger:
        case ASTMatch:
        case ASTString:
        case ASTVariable:
            return true;
        default:
            return false;
    }
}

bool is_variable(const ASTNode* ast)
{
    return ast && ast->type == ASTVariable;
}

bool is_variable_definition(const ASTNode* ast)
{
    return ast && ast->type == ASTVariableDefinition;
}

bool is_variable_type(const ASTNode* ast)
{
    return ast && (ast->type == ASTParameter || ast->type == ASTVariableDefinition);
}
