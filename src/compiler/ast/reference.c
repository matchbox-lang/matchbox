#include "compiler/ast/reference.h"
#include "common/memory.h"
#include "common/vector.h"
#include "compiler/ast/ast.h"
#include "compiler/ast/predicate.h"

static ReferenceType get_prefix_reference_type(TokenType type)
{
    if (type == TokenAmpersand) {
        return RefShared;
    }

    if (type == TokenCircumflex) {
        return RefExclusive;
    }

    return RefNone;
}

void add_reference_origin(Vector* origins, ASTNode* origin)
{
    if (!origin || contains_reference_origin(origins, origin)) {
        return;
    }

    push_vector_item(origins, origin);
}

void apply_slice_type(ASTNode* ast, Type type, ReferenceType reference_type)
{
    if (ast->type == ASTSlice) {
        ast->slice.type_id = type;
        ast->slice.reference_type = reference_type;

        return;
    }

    ASTNode* expression = allocate_memory(sizeof(ASTNode));
    *expression = *ast;

    *ast = (ASTNode){.type = ASTSlice};
    ast->slice.expr = expression;
    ast->slice.type_id = type;

    if (expression->type == ASTVariable) {
        ast->slice.token = expression->variable.token;
    }

    if (expression->type == ASTFunctionCall) {
        ast->slice.token = expression->function_call.token;
    }

    if (expression->type == ASTString) {
        ast->slice.token = expression->string_literal.token;
    }

    if (expression->type == ASTPrefix) {
        ast->slice.token = expression->prefix.operator;
    }

    ast->slice.reference_type = reference_type;

    init_vector(&ast->slice.reference_origins);
}

bool contains_reference_origin(Vector* origins, ASTNode* origin)
{
    size_t count = count_vector(origins);
    size_t index = 0;

    while (index < count && origins->data[index] != origin) {
        index++;
    }

    return index < count;
}

void convert_string_reference_to_slice(ASTNode* ast, Type type)
{
    ReferenceType reference = get_reference_type(ast);
    ASTNode* slice = create_slice_node(ast->prefix.operator, ast->prefix.expr, NULL);

    slice->slice.type_id = type;
    slice->slice.reference_type = reference;
    slice->slice.reference_origins = ast->prefix.reference_origins;

    replace_ast_node(ast, slice);
}

void copy_reference_origins(Vector* destination, Vector* source)
{
    if (destination == source) {
        return;
    }

    destination->count = 0;

    merge_reference_origins(destination, source);
}

ASTNode* get_argument_reference_origin(ASTNode* argument)
{
    ASTNode* origin = get_reference_origin_from_expression(argument);

    if (origin || !is_variable(argument)) {
        return origin;
    }

    return get_reference_origin_or_self(argument->variable.symbol);
}

ASTNode* get_reference_origin(const ASTNode* ast)
{
    if (!ast) {
        return NULL;
    }

    Vector* origins = get_reference_origins((ASTNode*)ast);
    if (origins && origins->count) {
        return origins->data[0];
    }

    if (ast->type == ASTVariable) {
        return get_reference_origin(ast->variable.symbol);
    }

    return NULL;
}

ASTNode* get_reference_origin_from_expression(ASTNode* ast)
{
    if (ast->type == ASTPrefix && is_variable(ast->prefix.expr)) {
        ASTNode* symbol = ast->prefix.expr->variable.symbol;

        return get_reference_origin_or_self(symbol);
    }

    return get_reference_origin(ast);
}

ASTNode* get_reference_origin_or_self(ASTNode* symbol)
{
    ASTNode* origin = get_reference_origin(symbol);
    if (origin) {
        return origin;
    }

    return symbol;
}

Vector* get_reference_origins(ASTNode* ast)
{
    if (!ast) {
        return NULL;
    }

    switch (ast->type) {
        case ASTSlice:
            return &ast->slice.reference_origins;
        case ASTConditional:
            return &ast->conditional.reference_origins;
        case ASTFunctionCall:
            return &ast->function_call.reference_origins;
        case ASTMatch:
            return &ast->match.reference_origins;
        case ASTParameter:
            return &ast->parameter.reference_origins;
        case ASTPrefix:
            return &ast->prefix.reference_origins;
        case ASTVariable:
            return get_reference_origins(ast->variable.symbol);
        case ASTVariableDefinition:
            return &ast->variable_definition.reference_origins;
        default:
            return NULL;
    }
}

Vector* get_reference_origins_from_expression(ASTNode* ast)
{
    Vector* origins = get_reference_origins(ast);
    if (origins) {
        return origins;
    }

    static Vector empty;

    return &empty;
}

ReferenceType get_reference_type(const ASTNode* ast)
{
    if (!ast) {
        return RefNone;
    }

    switch (ast->type) {
        case ASTSlice:
            return ast->slice.reference_type;
        case ASTConditional:
            return ast->conditional.reference_type;
        case ASTFunctionCall:
            return ast->function_call.symbol->function_definition.return_reference_type;
        case ASTFunctionDefinition:
            return ast->function_definition.return_reference_type;
        case ASTMatch:
            return ast->match.reference_type;
        case ASTParameter:
            return ast->parameter.reference_type;
        case ASTPrefix:
            return get_prefix_reference_type(ast->prefix.operator.type);
        case ASTVariable:
            return get_reference_type(ast->variable.symbol);
        case ASTVariableDefinition:
            return ast->variable_definition.reference_type;
        default:
            return RefNone;
    }
}

size_t* get_shared_access_count(ASTNode* symbol)
{
    if (is_parameter(symbol)) {
        return &symbol->parameter.shared_access_count;
    }

    return &symbol->variable_definition.shared_access_count;
}

bool has_exclusive_access(ASTNode* symbol)
{
    if (is_parameter(symbol)) {
        return symbol->parameter.exclusive_access_active;
    }

    return symbol->variable_definition.exclusive_access_active;
}

void merge_reference_origins(Vector* destination, Vector* source)
{
    size_t count = count_vector(source);

    for (size_t i = 0; i < count; i++) {
        ASTNode* origin = get_vector_at(source, i);

        add_reference_origin(destination, origin);
    }
}

void set_exclusive_access(ASTNode* symbol, bool active)
{
    if (is_parameter(symbol)) {
        symbol->parameter.exclusive_access_active = active;

        return;
    }

    symbol->variable_definition.exclusive_access_active = active;
}

void set_expression_reference_type(ASTNode* ast, ReferenceType type)
{
    if (ast->type == ASTConditional) {
        ast->conditional.reference_type = type;

        return;
    }

    ast->match.reference_type = type;
}
