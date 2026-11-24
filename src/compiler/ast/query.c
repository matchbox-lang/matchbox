#include "compiler/ast/query.h"
#include "builtin/builtin.h"
#include "common/array_type.h"
#include "common/vector.h"
#include "compiler/ast/predicate.h"
#include "compiler/ast/reference.h"

static Type get_builtin_return_type(const ASTNode* ast)
{
    TypeKind kind = ast->builtin_call.builtin->return_type_id;

    if (kind == TypeArray) {
        return get_node_data_type(ast->builtin_call.args.data[0]);
    }

    return (Type){.kind = kind};
}

ASTNode* get_call_parameter(Vector* parameters, size_t position)
{
    ASTNode* parameter = get_vector_at(parameters, position);

    if (parameter || !parameters->count) {
        return parameter;
    }

    parameter = parameters->data[parameters->count - 1];

    return parameter->parameter.variadic ? parameter : NULL;
}

ASTNode* get_index_constant_expression(ASTNode* ast)
{
    if (!ast) {
        return NULL;
    }

    if (ast->type == ASTPrefix && ast->prefix.operator.type == TokenKeywordConst) {
        return ast;
    }

    if (ast->type != ASTVariable) {
        return NULL;
    }

    ASTNode* symbol = ast->variable.symbol;

    if (!is_variable_definition(symbol)) {
        return NULL;
    }

    if (!symbol->variable_definition.fixed || symbol->variable_definition.address_taken
        || symbol->variable_definition.reference_type != RefNone) {
        return NULL;
    }

    return get_index_constant_expression(symbol->variable_definition.expr);
}

Type get_node_data_type(const ASTNode* ast)
{
    if (!ast) {
        return (Type){.kind = TypeVoid};
    }

    switch (ast->type) {
        case ASTArray:
            return ast->array.type_id;
        case ASTSlice:
            return ast->slice.type_id;
        case ASTIndex:
            return ast->index.type_id;
        case ASTConversion:
            return ast->conversion.type_id;
        case ASTBinary:
            return ast->binary.type_id;
        case ASTConditional:
            return ast->conditional.type_id;
        case ASTBoolean:
            return (Type){.kind = TypeBool};
        case ASTBuiltinCall:
            return get_builtin_return_type(ast);
        case ASTCharacter:
            return (Type){.kind = TypeChar};
        case ASTFloat:
            return ast->float_literal.type_id;
        case ASTFunctionCall:
            return get_node_data_type(ast->function_call.symbol);
        case ASTFunctionDefinition:
            return ast->function_definition.return_type_id;
        case ASTParameter:
            return ast->parameter.type_id;
        case ASTVariable:
            return get_node_data_type(ast->variable.symbol);
        case ASTVariableDefinition:
            return ast->variable_definition.type_id;
        case ASTPrefix:
            return get_node_data_type(ast->prefix.expr);
        case ASTInteger:
            return ast->integer_literal.type_id;
        case ASTMatch:
            return ast->match.type_id;
        case ASTString:
            return (Type){.kind = TypeString};
        default:
            return (Type){.kind = TypeVoid};
    }
}

ReferenceType get_parameter_argument_reference(ASTNode* parameter)
{
    if (!parameter || !parameter->parameter.variadic) {
        return get_reference_type(parameter);
    }

    Type type = get_parameter_argument_type(parameter);

    return is_contiguous_type(type.kind) ? RefShared : RefNone;
}

ASTNode* get_root_binding(const ASTNode* ast)
{
    if (ast->type == ASTIndex) {
        return get_root_binding(ast->index.array);
    }

    if (ast->type == ASTVariable) {
        return ast->variable.symbol;
    }

    return NULL;
}

Scope* get_scope(const ASTNode* ast)
{
    if (!ast) {
        return NULL;
    }

    switch (ast->type) {
        case ASTAssignment:
            return ast->assignment.scope;
        case ASTCompound:
            return ast->compound.scope;
        case ASTFunctionCall:
            return ast->function_call.scope;
        case ASTFunctionDefinition:
            return ast->function_definition.scope;
        case ASTParameter:
            return ast->parameter.scope;
        case ASTVariable:
            return ast->variable.scope;
        case ASTVariableDefinition:
            return ast->variable_definition.scope;
        default:
            return NULL;
    }
}
