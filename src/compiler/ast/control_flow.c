#include "compiler/ast/control_flow.h"
#include "common/vector.h"

static bool are_simple_match_values(const Vector* values)
{
    size_t index = 0;

    while (index < values->count && is_simple_match_value(values->data[index])) {
        index++;
    }

    return index == values->count;
}

static ASTNode* find_conditional_return(ASTNode* ast)
{
    ASTNode* result = find_node_return(ast->conditional.then_branch);

    if (result) {
        return result;
    }

    return find_node_return(ast->conditional.else_branch);
}

static ASTNode* find_function_return(Vector* statements)
{
    size_t count = count_vector(statements);
    size_t index = 0;
    ASTNode* result = NULL;

    while (index < count && !result) {
        ASTNode* statement = get_vector_at(statements, index);
        result = find_node_return(statement);
        index++;
    }

    return result;
}

static ASTNode* find_match_return(ASTNode* ast)
{
    size_t count = count_vector(&ast->match.arms);
    size_t index = 0;
    ASTNode* result = NULL;

    while (index < count && !result) {
        MatchArm* arm = get_vector_at(&ast->match.arms, index);
        result = find_node_return(arm->branch);
        index++;
    }

    return result;
}

ASTNode* find_node_return(ASTNode* ast)
{
    if (!ast) {
        return NULL;
    }

    switch (ast->type) {
        case ASTReturn:
            return ast;
        case ASTCompound:
            return find_function_return(&ast->compound.statements);
        case ASTFor:
            return find_node_return(ast->for_statement.body);
        case ASTWhile:
            return find_node_return(ast->while_statement.body);
        case ASTConditional:
            return find_conditional_return(ast);
        case ASTMatch:
            return find_match_return(ast);
        default:
            return NULL;
    }
}

ASTNode* get_branch_result(ASTNode* branch)
{
    if (branch->type == ASTConditional) {
        return branch;
    }

    Vector* statements = &branch->compound.statements;
    size_t count = count_vector(statements);

    if (!count) {
        return NULL;
    }

    return get_vector_at(statements, count - 1);
}

bool is_literal_match_pattern(ASTNode* pattern)
{
    if (pattern->type == ASTInteger || pattern->type == ASTBoolean) {
        return true;
    }

    if (!is_match_alternative(pattern)) {
        return false;
    }

    return is_literal_match_pattern(pattern->binary.left_expr) && is_literal_match_pattern(pattern->binary.right_expr);
}

bool is_match_alternative(ASTNode* pattern)
{
    return pattern->type == ASTBinary && pattern->binary.operator.type == TokenPipe;
}

bool is_simple_match_value(const ASTNode* ast)
{
    if (!ast) {
        return false;
    }

    switch (ast->type) {
        case ASTBoolean:
        case ASTCharacter:
        case ASTInteger:
        case ASTFloat:
        case ASTVariable:
            return true;
        case ASTBuiltinCall:
            return are_simple_match_values(&ast->builtin_call.args);
        default:
            return false;
    }
}

bool match_has_wildcard(ASTNode* ast)
{
    size_t count = count_vector(&ast->match.arms);
    size_t index = 0;
    bool wildcard = false;

    while (index < count && !wildcard) {
        MatchArm* arm = get_vector_at(&ast->match.arms, index);
        wildcard = !arm->pattern && !arm->guard;
        index++;
    }

    return wildcard;
}
