#include "compiler/parser/type.h"
#include "common/array_type.h"
#include "common/memory.h"
#include "common/tuple_type.h"
#include "common/vector.h"
#include "compiler/ast/type_alias.h"
#include "compiler/parser/array.h"
#include "compiler/parser/expression.h"
#include "compiler/parser/function.h"
#include "compiler/parser/generic.h"
#include "compiler/builtin_constant.h"
#include "compiler/diagnostics.h"
#include "runtime/string_object.h"

static _Noreturn void expected_type_error(Token token)
{
    fatal_token_error(token, "", "Expected type but found ");
}

void consume_type(Parser* parser)
{
    if (!is_type_token(parser->current_token.type)) {
        expected_type_error(parser->current_token);
    }

    consume(parser, parser->current_token.type);
}

ReferenceType parse_reference_type(Parser* parser)
{
    if (parser->current_token.type == TokenAmpersand) {
        consume(parser, TokenAmpersand);

        return RefShared;
    }

    if (parser->current_token.type == TokenCircumflex) {
        consume(parser, TokenCircumflex);

        return RefExclusive;
    }

    return RefNone;
}

ASTNode* parse_type_expression(Parser* parser)
{
    Token type_token = parser->current_token;
    TypeKind kind = get_token_data_type(type_token.type);

    consume(parser, type_token.type);

    if (is_end_of_file(parser)) {
        return NULL;
    }

    if (parser->current_token.type == TokenLsquare) {
        return parse_array_initializer(parser, type_token, kind);
    }

    if (parser->current_token.type != TokenDot) {
        return parse_function_call(parser);
    }

    consume(parser, TokenDot);

    if (is_end_of_file(parser)) {
        return NULL;
    }

    Token name = parser->current_token;

    consume(parser, TokenIdentifier);

    ASTNode* ast = resolve_builtin_constant(kind, name);
    if (!ast) {
        fatal_source_error(name.line, name.column, "Unknown built-in constant %.*s.%.*s",
            (int)type_token.length, type_token.chars, (int)name.length, name.chars);
    }

    return ast;
}

static bool parse_dimension(Parser* parser, Vector* dimensions)
{
    consume(parser, TokenLsquare);

    if (parser->current_token.type == TokenRsquare) {
        push_vector_item(dimensions, NULL);
        consume(parser, TokenRsquare);

        return true;
    }

    ASTNode* count = parse_expression(parser);

    if (!count) {
        return false;
    }

    push_vector_item(dimensions, count);

    if (is_end_of_file(parser)) {
        return false;
    }

    consume(parser, TokenRsquare);

    return true;
}

bool parse_dimensions(Parser* parser, Type* type)
{
    if (parser->current_token.type != TokenLsquare) {
        return true;
    }

    ArrayType* array_type = allocate_zeroed_memory(1, sizeof(ArrayType) + 1);
    Vector* dimensions = allocate_memory(sizeof(Vector));
    array_type->element_type = *type;
    array_type->dimensions = dimensions;

    init_vector(dimensions);

    *type = (Type){.kind = TypeArray, .array = array_type};
    bool complete = true;

    while (complete && parser->current_token.type == TokenLsquare) {
        complete = parse_dimension(parser, dimensions);
    }

    return complete;
}

static bool parse_tuple_element_type(Parser* parser, Vector* elements)
{
    TupleElement* element = allocate_zeroed_memory(1, sizeof(TupleElement));

    push_vector_item(elements, element);

    return parse_declared_type(parser, &element->type, &element->reference);
}

static bool parse_next_tuple_element_type(Parser* parser, Vector* elements)
{
    consume(parser, TokenComma);

    if (parser->current_token.type == TokenRparen) {
        return true;
    }

    return parse_tuple_element_type(parser, elements);
}

static bool parse_tuple_type(Parser* parser, Type* type)
{
    Vector elements;

    init_vector(&elements);
    consume(parser, TokenLparen);

    bool complete = true;

    if (parser->current_token.type != TokenRparen) {
        complete = parse_tuple_element_type(parser, &elements);
    }

    while (complete && parser->current_token.type == TokenComma) {
        complete = parse_next_tuple_element_type(parser, &elements);
    }

    TupleType* tuple = create_tuple_type(elements.count);

    for (size_t i = 0; i < elements.count; i++) {
        tuple->elements[i] = *(TupleElement*)elements.data[i];

        free_memory(elements.data[i]);
    }

    free_vector(&elements);

    *type = (Type){.kind = TypeTuple, .tuple = tuple};

    if (!complete || is_end_of_file(parser)) {
        return false;
    }

    consume(parser, TokenRparen);

    return true;
}

bool parse_declared_type(Parser* parser, Type* type, ReferenceType* reference_type)
{
    *reference_type = parse_reference_type(parser);

    if (is_end_of_file(parser)) {
        return false;
    }

    bool complete = true;
    TypeKind kind = get_token_data_type(parser->current_token.type);
    *type = (Type){.kind = kind};

    if (parser->current_token.type == TokenLparen) {
        complete = parse_tuple_type(parser, type);
    } else if (parser->current_token.type == TokenIdentifier) {
        *type = parse_named_type(parser, parser->current_token, *reference_type, &complete);
    } else {
        consume_type(parser);
    }

    complete = complete && parse_dimensions(parser, type);

    if (type->kind == TypeArray && *reference_type != RefNone
        && type->array->dimensions && !type->array->dimensions->data[0]) {
        type->kind = TypeArraySlice;
    }

    if (type->kind == TypeString && *reference_type != RefNone) {
        type->kind = TypeStringSlice;
    }

    return complete;
}

bool parse_function_return_type(Parser* parser, ASTNode* ast)
{
    if (parser->current_token.type != TokenArrow) {
        return true;
    }

    consume(parser, TokenArrow);

    bool complete = parse_declared_type(parser, &ast->function_definition.return_type_id,
        &ast->function_definition.return_reference_type);
    ast->function_definition.has_explicit_return_type = complete;

    return complete;
}

bool parse_variable_type(Parser* parser, ASTNode* ast)
{
    if (parser->current_token.type == TokenColon) {
        consume(parser, TokenColon);

        return parse_declared_type(parser, &ast->variable_definition.type_id, &ast->variable_definition.reference_type);
    }

    if (is_end_of_file(parser)) {
        return false;
    }

    if (parser->current_token.type != TokenEqual) {
        expected_token_error(TokenEqual, parser->current_token);
    }

    return true;
}

static bool parse_type_alias_target(Parser* parser, ASTNode* ast)
{
    if (is_end_of_file(parser)) {
        return false;
    }

    consume(parser, TokenEqual);

    return parse_declared_type(parser, &ast->type_alias.type_id, &ast->type_alias.reference_type);
}

static bool parse_type_alias_definition(Parser* parser, ASTNode* ast)
{
    consume(parser, TokenIdentifier);

    if (!parse_generic_type_names(parser, &ast->type_alias.bindings.names)) {
        return false;
    }

    GenericBindings* parent = parser->generic_bindings;
    ast->type_alias.bindings.parent = parent;
    parser->generic_bindings = &ast->type_alias.bindings;
    bool complete = parse_type_alias_target(parser, ast);
    parser->generic_bindings = parent;

    return complete;
}

ASTNode* parse_type_alias(Parser* parser)
{
    consume(parser, TokenKeywordType);

    if (is_end_of_file(parser)) {
        return NULL;
    }

    Token token = parser->current_token;
    StringObject* id = copy_string_object(token.chars, token.length);
    ASTNode* ast = create_type_alias_node(token, id);

    if (!parse_type_alias_definition(parser, ast)) {
        free_ast_node(ast);

        return NULL;
    }

    return ast;
}
