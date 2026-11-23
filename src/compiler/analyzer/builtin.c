#include "compiler/analyzer/builtin.h"
#include "builtin/builtin.h"
#include "builtin/io.h"
#include "common/errors.h"
#include "common/memory.h"
#include "common/vector.h"
#include "compiler/analyzer/conversion.h"
#include "compiler/ast/literal.h"
#include "compiler/ast/query.h"
#include "compiler/diagnostics.h"
#include "runtime/string_object.h"
#include <float.h>

static _Noreturn void invalid_builtin_arguments_error(Token token)
{
    fatal_token_error(token, "", "Invalid arguments to builtin ");
}

static void validate_typed_variadic_argument(ASTNode* argument, Type type, Token token)
{
    apply_literal_type(argument, type);

    Type argument_type = get_node_data_type(argument);

    if (!types_equal(argument_type, type) && !can_implicitly_widen_type(argument_type.kind, type.kind)) {
        semantic_token_error("Variadic arguments must convert losslessly to the parameter type ", token);
    }
}

static void validate_typed_variadic_arguments(Vector* args, const Builtin* builtin, Token token)
{
    if (!args->count) {
        semantic_token_error("Numeric variadic builtins require at least one argument ", token);
    }

    for (size_t i = 0; i < args->count; i++) {
        validate_typed_variadic_argument(args->data[i], (Type){.kind = builtin->params[0]}, token);
    }
}

static void validate_variadic_arguments(Vector* args, const Builtin* builtin, Token token)
{
    if (builtin->param_count == BUILTIN_TYPED_VARIADIC) {
        validate_typed_variadic_arguments(args, builtin, token);

        return;
    }

    char* message = "Invalid value in string construction ";

    if (builtin->entry == builtin_print) {
        message = "Invalid value in print ";
    }

    for (size_t i = 0; i < args->count; i++) {
        ASTNode* argument = args->data[i];
        Type type = get_node_data_type(argument);
        bool valid = is_printable_type(type.kind);

        if (!valid) {
            semantic_token_error(message, token);
        }
    }
}

static void validate_builtin_call(ASTNode* ast, const Builtin* builtin, Token token)
{
    size_t count = count_vector(&ast->function_call.args);

    if (builtin->param_count < 0) {
        validate_variadic_arguments(&ast->function_call.args, builtin, token);

        return;
    }

    if (count != (size_t)builtin->param_count) {
        semantic_token_error("Invalid arguments to function ", token);
    }

    if (builtin->params[0] == TypeArray) {
        return;
    }

    for (size_t i = 0; i < count; i++) {
        ASTNode* arg = get_vector_at(&ast->function_call.args, i);
        apply_literal_type(arg, (Type){.kind = builtin->params[i]});

        Type argument_type = get_node_data_type(arg);
        Type parameter_type = {.kind = builtin->params[i]};

        if (!types_equal(argument_type, parameter_type)
            && !can_implicitly_widen_type(argument_type.kind, parameter_type.kind)) {
            semantic_token_error("Invalid arguments to function ", token);
        }
    }
}

static void convert_builtin_call(ASTNode* ast, const Builtin* builtin)
{
    Vector args = ast->function_call.args;
    Token token = ast->function_call.token;

    free_string_object(ast->function_call.id);

    ast->type = ASTBuiltinCall;
    ast->builtin_call.builtin = builtin;
    ast->builtin_call.args = args;
    ast->builtin_call.token = token;
}

static Type get_nonliteral_argument_type(ASTNode* argument)
{
    bool literal = is_numeric_literal(argument);

    if (literal) {
        return (Type){.kind = TypeUnknown};
    }

    return get_node_data_type(argument);
}

static bool variadic_argument_fits_type(ASTNode* argument, Type type)
{
    bool negative;
    ASTNode* integer = get_integer_literal(argument, &negative);

    if (integer) {
        bool fits_integer = integer_literal_fits_type(integer->integer_literal.value, negative, type);
        bool fits_float = integer_literal_fits_float(integer->integer_literal.value, type);

        return fits_integer || fits_float;
    }

    ASTNode* floating = get_float_literal(argument);

    if (floating) {
        bool numeric = is_float_type(type.kind);
        bool fits = type.kind == TypeF64 || floating->float_literal.value <= FLT_MAX;

        return numeric && fits;
    }

    Type argument_type = get_node_data_type(argument);

    return types_equal(argument_type, type) || can_implicitly_widen_type(argument_type.kind, type.kind);
}

static bool typed_variadic_call_matches(Vector* args, const Builtin* builtin)
{
    if (!builtin || builtin->param_count != BUILTIN_TYPED_VARIADIC) {
        return false;
    }

    bool matches = true;

    for (size_t i = 0; i < args->count && matches; i++) {
        matches = variadic_argument_fits_type(args->data[i], (Type){.kind = builtin->params[0]});
    }

    return matches;
}

static const Builtin* resolve_contextual_variadic_call(ASTNode* ast)
{
    Vector* args = &ast->function_call.args;
    Type type = (Type){.kind = TypeUnknown};

    for (size_t i = 0; i < args->count && type.kind == TypeUnknown; i++) {
        type = get_nonliteral_argument_type(args->data[i]);
    }

    if (type.kind == TypeUnknown) {
        return NULL;
    }

    const Builtin* builtin = resolve_builtin(ast->function_call.id->chars, &type, 1);
    bool matches = typed_variadic_call_matches(args, builtin);

    if (!matches) {
        return NULL;
    }

    return builtin;
}

static Type get_builtin_argument_type(ASTNode* argument)
{
    Type type = get_node_data_type(argument);

    if (type.kind == TypeUnknown) {
        return (Type){.kind = TypeU64};
    }

    return type;
}

static const Builtin* resolve_builtin_call(ASTNode* ast)
{
    const Builtin* contextual = resolve_contextual_variadic_call(ast);

    if (contextual) {
        return contextual;
    }

    size_t count = ast->function_call.args.count;
    Type* argument_types = NULL;

    if (count > SIZE_MAX / sizeof(Type)) {
        out_of_memory_error();
    }

    if (count) {
        argument_types = allocate_memory(count * sizeof(Type));
    }

    for (size_t i = 0; i < count; i++) {
        argument_types[i] = get_builtin_argument_type(ast->function_call.args.data[i]);
    }

    const Builtin* builtin = resolve_builtin(ast->function_call.id->chars, argument_types, count);

    free_memory(argument_types);

    return builtin;
}

static void validate_builtin_resolution(ASTNode* ast, const Builtin* builtin)
{
    if (builtin) {
        return;
    }

    if (is_builtin_name(ast->function_call.id->chars)) {
        invalid_builtin_arguments_error(ast->function_call.token);
    }

    symbol_error(" is undefined", ast->function_call.token);
}

void validate_constant_builtin(Analyzer* analyzer, const Builtin* builtin, Token token)
{
    if (!analyzer->constant_scope || builtin->constant_evaluable) {
        return;
    }

    fatal_token_error(token, " during constant evaluation", "Cannot call builtin ");
}

void analyze_builtin_call(Analyzer* analyzer, ASTNode* ast)
{
    const Builtin* builtin = resolve_builtin_call(ast);

    validate_builtin_resolution(ast, builtin);
    validate_constant_builtin(analyzer, builtin, ast->function_call.token);
    validate_builtin_call(ast, builtin, ast->function_call.token);
    convert_builtin_call(ast, builtin);
}

void analyze_resolved_builtin_call(Analyzer* analyzer, ASTNode* ast)
{
    analyze_expression_nodes(analyzer, &ast->builtin_call.args);

    if (ast->builtin_call.builtin->param_count >= 0) {
        return;
    }

    Token token = ast->builtin_call.token;

    validate_variadic_arguments(&ast->builtin_call.args, ast->builtin_call.builtin, token);
}
