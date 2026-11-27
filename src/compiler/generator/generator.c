#include "compiler/generator/generator.h"
#include "builtin/builtin.h"
#include "common/array_type.h"
#include "common/errors.h"
#include "common/vector.h"
#include "compiler/ast/ast.h"
#include "compiler/ast/predicate.h"
#include "compiler/ast/query.h"
#include "compiler/ast/reference.h"
#include "compiler/generator/array.h"
#include "compiler/generator/constant.h"
#include "compiler/generator/control_flow.h"
#include "compiler/generator/conversion.h"
#include "compiler/generator/emitter.h"
#include "compiler/generator/expression.h"
#include "compiler/generator/function.h"
#include "compiler/generator/index.h"
#include "compiler/generator/iteration.h"
#include "compiler/generator/match.h"
#include "compiler/generator/operand.h"
#include "compiler/generator/slice.h"
#include "compiler/generator/tuple.h"
#include "compiler/generator/variable.h"
#include "runtime/function_object.h"
#include <stddef.h>
#include <stdint.h>

_Noreturn void global_index_overflow_error(void)
{
    fatal_error("Cannot create more than %u globals", UINT16_MAX + 1u);
}

static _Noreturn void unresolved_repl_expression_type_error(void)
{
    fatal_error("Cannot display expression with an unresolved type");
}

Operand compile_reference_expression(Compiler* compiler, ASTNode* ast)
{
    Type type = get_node_data_type(ast);

    if (ast->type == ASTIndex && get_reference_type(ast) != RefNone) {
        return compile_tuple_index(compiler, ast, true);
    }

    if (is_slice_type(type.kind)) {
        return compile_expression(compiler, ast, false);
    }

    if (ast->type == ASTConditional) {
        return compile_conditional(compiler, ast, true);
    }

    if (ast->type == ASTMatch) {
        return compile_match(compiler, ast, true);
    }

    if (get_reference_type(ast) == RefNone && ast->type != ASTVariable) {
        Operand value = compile_expression(compiler, ast, false);
        int value_register = allocate_registers(compiler, value.slots);

        emit_operand_mov(compiler, value_register, value);
        emit_instruction(compiler, OpRefl, value.reg, value_register, 0);

        return make_operand(value.reg, true);
    }

    if (ast->type == ASTPrefix) {
        return reference_variable(compiler, ast->prefix.expr->variable.symbol);
    }

    if (ast->type == ASTVariable) {
        return reference_variable(compiler, ast->variable.symbol);
    }

    if (ast->type == ASTFunctionCall) {
        uint16_t position = get_function_position(compiler, ast->function_call.symbol);
        FunctionObject* function = get_vector_at(&compiler->module->functions, position);
        Vector* params = &ast->function_call.symbol->function_definition.params;

        return compile_call(compiler, function, position, &ast->function_call.args, params, false);
    }

    return no_operand();
}

static bool compile_discarded_array(Compiler* compiler, ASTNode* ast)
{
    for (size_t i = 0; i < ast->array.elements.count; i++) {
        compile_expression(compiler, ast->array.elements.data[i], true);
    }

    return true;
}

static bool compile_discarded_prefix(Compiler* compiler, ASTNode* ast)
{
    if (ast->prefix.operator.type == TokenKeywordConst) {
        return false;
    }

    compile_expression(compiler, ast->prefix.expr, true);

    return true;
}

static bool compile_discarded_array_binary(Compiler* compiler, ASTNode* ast)
{
    Type left_type = get_node_data_type(ast->binary.left_expr);
    TokenType operator_type = ast->binary.operator.type;
    bool division = operator_type == TokenSlash || operator_type == TokenPercent;
    Type operand_type = ast->binary.operand_type;

    if (!is_array_type(left_type.kind) || (division && is_integer_type(operand_type.kind))) {
        return false;
    }

    compile_expression(compiler, ast->binary.left_expr, true);
    compile_expression(compiler, ast->binary.right_expr, true);

    return true;
}

static bool compile_discarded_expression(Compiler* compiler, ASTNode* ast)
{
    switch (ast->type) {
        case ASTBinary:
            return compile_discarded_array_binary(compiler, ast);
        case ASTArray:
            return compile_discarded_array(compiler, ast);
        case ASTBoolean:
        case ASTCharacter:
        case ASTFloat:
        case ASTInteger:
        case ASTString:
        case ASTVariable:
            return true;
        case ASTPrefix:
            return compile_discarded_prefix(compiler, ast);
        default:
            return false;
    }
}

static Operand compile_value_expression(Compiler* compiler, ASTNode* ast)
{
    switch (ast->type) {
        case ASTSlice:
            return compile_slice(compiler, ast);
        case ASTArray:
            return compile_array(compiler, ast);
        case ASTIndex:
            return compile_index(compiler, ast);
        case ASTBinary:
            return compile_binary(compiler, ast);
        case ASTString:
            return compile_string(compiler, ast);
        case ASTCharacter:
            return compile_character(compiler, ast);
        case ASTBoolean:
            return compile_boolean(compiler, ast);
        case ASTConditional:
            return compile_conditional(compiler, ast, false);
        case ASTMatch:
            return compile_match(compiler, ast, false);
        case ASTConversion:
            return compile_explicit_conversion(compiler, ast);
        case ASTFloat:
            return compile_float(compiler, ast);
        case ASTInteger:
            return compile_integer_value(compiler, ast->integer_literal.value, ast->integer_literal.type_id);
        case ASTPrefix:
            return compile_prefix(compiler, ast);
        case ASTVariable:
            return compile_variable(compiler, ast);
        default:
            return no_operand();
    }
}

Operand compile_expression(Compiler* compiler, ASTNode* ast, bool discard)
{
    if (discard && compile_discarded_expression(compiler, ast)) {
        return no_operand();
    }

    if (ast->type == ASTBuiltinCall) {
        return compile_builtin_call(compiler, ast, discard);
    }

    if (ast->type == ASTFunctionCall) {
        return compile_function_call(compiler, ast, discard);
    }

    Operand result = compile_value_expression(compiler, ast);

    if (discard) {
        release_operand(compiler, result);

        return no_operand();
    }

    return result;
}

static void compile_test_block(Compiler* compiler, ASTNode* ast)
{
    if (!compiler->test_mode) {
        return;
    }

    compile_function_definition(compiler, ast->test_block.function);
}

Operand compile_statement(Compiler* compiler, ASTNode* ast, bool discard)
{
    switch (ast->type) {
        case ASTIndexAssignment:
            compile_index_assignment(compiler, ast);
            return no_operand();
        case ASTAssignment:
            compile_assignment(compiler, ast);
            return no_operand();
        case ASTBuiltinCall:
            return compile_builtin_call(compiler, ast, discard);
        case ASTFunctionCall:
            return compile_function_call(compiler, ast, discard);
        case ASTTestBlock:
            compile_test_block(compiler, ast);
            return no_operand();
        case ASTFunctionDefinition:
            compile_function_definition(compiler, ast);
            return no_operand();
        case ASTFor:
            compile_for(compiler, ast);
            return no_operand();
        case ASTBreak:
        case ASTContinue:
            compile_loop_control(compiler, ast);
            return no_operand();
        case ASTWhile:
            compile_while(compiler, ast);
            return no_operand();
        case ASTReturn:
            compile_return_statement(compiler, ast);
            return no_operand();
        case ASTVariableDefinition:
            compile_variable_definition(compiler, ast);
            return no_operand();
        default:
            return compile_expression(compiler, ast, discard);
    }
}

void compile_block_level_statements(Compiler* compiler, Vector* nodes)
{
    size_t count = count_vector(nodes);

    for (size_t i = 0; i < count; i++) {
        compile_statement(compiler, nodes->data[i], true);
    }
}

static void finish_top_level_statement(Compiler* compiler, ASTNode* ast)
{
    if (ast->type != ASTVariableDefinition) {
        return;
    }

    size_t slots = get_node_slot_count(ast);
    size_t position = ast->variable_definition.position;
    size_t count = position + slots;

    if (count > (size_t)UINT16_MAX + 1) {
        global_index_overflow_error();
    }

    compiler->module->global_count = count;
    compiler->local_position_offset = -(int)count;
}

void compile_top_level_statements(Compiler* compiler, Vector* nodes)
{
    size_t count = count_vector(nodes);

    while (compiler->statement_index < count) {
        ASTNode* ast = nodes->data[compiler->statement_index++];

        compile_statement(compiler, ast, true);
        finish_top_level_statement(compiler, ast);
    }
}

static void compile_repl_statement(Compiler* compiler, ASTNode* ast, bool is_last)
{
    bool display = is_last && is_expression_statement(ast) && get_node_data_type(ast).kind != TypeVoid;

    if (display && get_node_data_type(ast).kind == TypeUnknown) {
        unresolved_repl_expression_type_error();
    }

    Operand value = compile_statement(compiler, ast, !display);

    if (!display) {
        return;
    }

    Type argument_type = get_node_data_type(ast);
    void* items[] = {ast};
    Vector args = {.data = items, .capacity = 1, .count = 1};
    const Builtin* builtin = resolve_builtin("print", &argument_type, 1);
    size_t position = find_builtin_call_position(compiler, builtin, &args);

    if (position == SIZE_MAX) {
        position = add_builtin_call(compiler, builtin, &args);
    }

    FunctionObject* function = get_vector_at(&compiler->module->functions, position);

    compile_call_with_argument(compiler, function, position, value, true);
}

void compile_repl_statements(Compiler* compiler, Vector* nodes)
{
    size_t count = count_vector(nodes);

    while (compiler->statement_index < count) {
        ASTNode* ast = nodes->data[compiler->statement_index++];
        bool is_last = compiler->statement_index == count;

        compile_repl_statement(compiler, ast, is_last);
        finish_top_level_statement(compiler, ast);
    }
}
