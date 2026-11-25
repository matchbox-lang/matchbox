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

static bool visit_statement_returns(Vector* statements, VisitReturn visit, void* context)
{
    bool complete = true;

    for (size_t i = 0; i < statements->count && complete; i++) {
        complete = visit_node_returns(statements->data[i], visit, context);
    }

    return complete;
}

static bool visit_match_returns(ASTNode* ast, VisitReturn visit, void* context)
{
    Vector* arms = &ast->match.arms;
    bool complete = true;

    for (size_t i = 0; i < arms->count && complete; i++) {
        MatchArm* arm = arms->data[i];
        complete = visit_node_returns(arm->branch, visit, context);
    }

    return complete;
}

static bool record_first_return(ASTNode* ast, void* context)
{
    ASTNode** result = context;
    *result = ast;

    return false;
}

ASTNode* find_node_return(ASTNode* ast)
{
    ASTNode* result = NULL;

    visit_node_returns(ast, record_first_return, &result);

    return result;
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

bool visit_node_returns(ASTNode* ast, VisitReturn visit, void* context)
{
    if (!ast) {
        return true;
    }

    switch (ast->type) {
        case ASTReturn:
            return visit(ast, context);
        case ASTCompound:
            return visit_statement_returns(&ast->compound.statements, visit, context);
        case ASTFor:
            return visit_node_returns(ast->for_statement.body, visit, context);
        case ASTWhile:
            return visit_node_returns(ast->while_statement.body, visit, context);
        case ASTConditional:
            return visit_node_returns(ast->conditional.then_branch, visit, context)
                && visit_node_returns(ast->conditional.else_branch, visit, context);
        case ASTMatch:
            return visit_match_returns(ast, visit, context);
        default:
            return true;
    }
}
