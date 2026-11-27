#include "compiler/analyzer/type_alias.h"
#include "common/array_type.h"
#include "common/errors.h"
#include "common/memory.h"
#include "common/slice.h"
#include "common/tuple_type.h"
#include "compiler/analyzer/array.h"
#include "compiler/ast/type_alias.h"
#include "compiler/parser/generic.h"
#include "compiler/parser/type.h"
#include "compiler/diagnostics.h"
#include "compiler/scope.h"
#include "runtime/string_object.h"

static Type substitute_alias_arguments(Analyzer* analyzer, Type type, Vector* arguments);

static void register_type_alias(Analyzer* analyzer, ASTNode* ast)
{
    if (ast->type != ASTTypeAlias) {
        return;
    }

    StringView id = get_string_view(ast->type_alias.id);
    ASTNode* previous = get_local_symbol(analyzer->current_scope, id);

    if (previous && previous != ast) {
        redefinition_error("Type alias", ast->type_alias.token);
    }

    ast->type_alias.scope = analyzer->current_scope;

    set_local_symbol(analyzer->current_scope, id, ast);
}

void register_type_aliases(Analyzer* analyzer, Vector* nodes, size_t start)
{
    for (size_t i = start; i < nodes->count; i++) {
        register_type_alias(analyzer, nodes->data[i]);
    }
}

void resolve_type_alias(Analyzer* analyzer, ASTNode* ast)
{
    if (ast->type_alias.resolved) {
        return;
    }

    if (ast->type_alias.resolving) {
        semantic_token_error("Circular type alias ", ast->type_alias.token);
    }

    Scope* previous = analyzer->current_scope;
    ast->type_alias.resolving = true;
    analyzer->current_scope = ast->type_alias.scope;

    resolve_named_type(analyzer, &ast->type_alias.type_id, &ast->type_alias.reference_type);
    resolve_dimensions(analyzer, &ast->type_alias.type_id);

    analyzer->current_scope = previous;
    ast->type_alias.resolving = false;
    ast->type_alias.resolved = true;
}

static Type substitute_tuple_alias_arguments(Analyzer* analyzer, Type type, Vector* arguments)
{
    TupleType* tuple = create_tuple_type(type.tuple->count);

    for (size_t i = 0; i < tuple->count; i++) {
        tuple->elements[i] = type.tuple->elements[i];
        tuple->elements[i].type = substitute_alias_arguments(analyzer, tuple->elements[i].type, arguments);
    }

    layout_tuple_type(tuple);
    push_vector_item(&analyzer->tuple_types, tuple);

    return (Type){.kind = TypeTuple, .tuple = tuple};
}

static Type substitute_alias_arguments(Analyzer* analyzer, Type type, Vector* arguments)
{
    if (type.kind == TypeGeneric && type.generic_index < arguments->count) {
        return *(Type*)arguments->data[type.generic_index];
    }

    if (arguments->count && type.kind == TypeTuple) {
        return substitute_tuple_alias_arguments(analyzer, type, arguments);
    }

    if (!arguments->count || !is_contiguous_type(type.kind) || type.kind == TypeStringSlice) {
        return type;
    }

    Type element = substitute_alias_arguments(analyzer, type.array->element_type, arguments);

    if (element.kind == TypeVoid) {
        fatal_error("Invalid array element type");
    }

    if (type.kind == TypeArraySlice) {
        return get_slice_type(&analyzer->array_types, element);
    }

    Type resolved = find_array_type(&analyzer->array_types, element, type.array->count);

    if (resolved.kind == TypeUnknown) {
        resolved = add_array_type(&analyzer->array_types, element, type.array->count);
    }

    return resolved;
}

static void parse_alias_arguments(Parser* parser, const NamedType* named, bool generic, Vector* arguments)
{
    init_vector(arguments);
    init_parser(parser, NULL);

    if (named->suffix.chars) {
        init_lexer(&parser->lexer, named->suffix.chars);

        parser->lexer.current.line = named->suffix.line;
        parser->lexer.current.column = named->suffix.column;
        parser->generic_bindings = named->bindings;

        advance(parser);
    }

    if (generic && named->suffix.chars) {
        parse_type_arguments(parser, arguments);
    }
}

static void resolve_alias_dimensions(Parser* parser, const NamedType* named, ASTNode* alias, Type* type)
{
    if (named->suffix.chars && alias->type_alias.reference_type != RefNone
        && parser->current_token.type == TokenLsquare) {
        semantic_token_error("Array element cannot be a reference alias: ", named->token);
    }

    if (named->suffix.chars) {
        parse_dimensions(parser, type);
    }

    if (type->kind == TypeArray && named->reference && type->array->dimensions
        && !type->array->dimensions->data[0]) {
        type->kind = TypeArraySlice;
    }

    if (type->kind == TypeString && named->reference) {
        type->kind = TypeStringSlice;
    }
}

static ASTNode* find_type_alias(Analyzer* analyzer, Token token)
{
    StringView id = create_string_view(token.chars, token.length);
    ASTNode* alias = get_symbol(analyzer->current_scope, id);

    if (!alias || alias->type != ASTTypeAlias) {
        semantic_token_error("Unknown type ", token);
    }

    return alias;
}

static void resolve_alias_arguments(Analyzer* analyzer, Parser* parser, const NamedType* named,
    ASTNode* alias, Vector* arguments)
{
    size_t count = alias->type_alias.bindings.names.count;

    parse_alias_arguments(parser, named, count != 0, arguments);

    if (arguments->count != count) {
        semantic_token_error("Incorrect type argument count for alias ", named->token);
    }

    for (size_t i = 0; i < arguments->count; i++) {
        resolve_dimensions(analyzer, arguments->data[i]);
    }
}

static void resolve_alias_reference(ASTNode* alias, ReferenceType* reference, Token token)
{
    if (alias->type_alias.reference_type == RefNone) {
        return;
    }

    if (!reference || *reference != RefNone) {
        semantic_token_error("Reference alias cannot be used in this type: ", token);
    }

    *reference = alias->type_alias.reference_type;
}

static void free_alias_arguments(Vector* arguments)
{
    for (size_t i = 0; i < arguments->count; i++) {
        free_memory(arguments->data[i]);
    }

    free_vector(arguments);
}

void resolve_named_type(Analyzer* analyzer, Type* type, ReferenceType* reference)
{
    if (type->kind != TypeNamed) {
        return;
    }

    const NamedType* named = type->named;
    ASTNode* alias = find_type_alias(analyzer, named->token);
    Parser parser = {0};
    Vector arguments;

    resolve_alias_arguments(analyzer, &parser, named, alias, &arguments);
    resolve_type_alias(analyzer, alias);
    resolve_alias_reference(alias, reference, named->token);

    *type = substitute_alias_arguments(analyzer, alias->type_alias.type_id, &arguments);

    resolve_alias_dimensions(&parser, named, alias, type);
    free_alias_arguments(&arguments);
    free_named_type(named);
}
