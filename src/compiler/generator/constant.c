#include "compiler/generator/constant.h"
#include "common/errors.h"
#include "common/memory.h"
#include "common/vector.h"
#include "compiler/generator/emitter.h"
#include "compiler/generator/generator.h"
#include "compiler/generator/operand.h"
#include "compiler/ast.h"
#include "runtime/function_object.h"
#include "runtime/string_object.h"
#include "runtime/value.h"
#include "vm/vm.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static _Noreturn void constant_index_overflow_error(void)
{
    fatal_error("Cannot create more than %u constants", UINT16_MAX + 1u);
}

static size_t make_constant(Compiler* compiler, Value value)
{
    size_t position = count_value_array(&compiler->module->constants);

    if (position > UINT16_MAX) {
        constant_index_overflow_error();
    }

    return push_value(&compiler->module->constants, value) - 1;
}

size_t make_i64_constant(Compiler* compiler, uint64_t value)
{
    size_t position = count_value_array(&compiler->module->constants);

    if (position > (size_t)UINT16_MAX - 1) {
        constant_index_overflow_error();
    }

#if UINTPTR_MAX == UINT32_MAX
    push_value(&compiler->module->constants, U32_VALUE(value));
    push_value(&compiler->module->constants, U32_VALUE(value >> 32));
#else
    push_value(&compiler->module->constants, UNSIGNED_VALUE(value));
    push_value(&compiler->module->constants, UNSIGNED_VALUE(0));
#endif

    return position;
}

static size_t make_f32_constant(Compiler* compiler, float value)
{
    Value constant = F32_VALUE(value);

    return make_constant(compiler, constant);
}

static size_t make_f64_constant(Compiler* compiler, double value)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));

    return make_i64_constant(compiler, bits);
}

static Operand compile_wide_number(Compiler* compiler, uint64_t value)
{
    size_t position = make_i64_constant(compiler, value);
    int reg = emit_ldc_i64(compiler, position);

    return make_wide_operand(reg, true);
}

static bool is_narrow_integer_immediate(uint64_t value, Type type)
{
    bool signed_type = is_signed_integer_type(type.kind);

    if (!signed_type) {
        return value <= INT16_MAX;
    }

    int64_t signed_value = (int64_t)value;

    return signed_value >= INT16_MIN && signed_value <= INT16_MAX;
}

Operand compile_integer_value(Compiler* compiler, uint64_t value, Type type)
{
    if (type.kind == TypeF64) {
        size_t position = make_f64_constant(compiler, (double)value);

        return make_wide_operand(emit_ldc_i64(compiler, position), true);
    }

    if (type.kind == TypeF32) {
        size_t position = make_f32_constant(compiler, (float)value);

        return make_operand(emit_ldc(compiler, position), true);
    }

    if (get_type_slot_count(type.kind) == 2) {
        return compile_wide_number(compiler, value);
    }

    bool immediate = is_narrow_integer_immediate(value, type);

    if (!immediate) {
        Value constant = is_signed_integer_type(type.kind)
            ? SIGNED_VALUE((int64_t)value)
            : UNSIGNED_VALUE(value);
        size_t position = make_constant(compiler, constant);

        return make_operand(emit_ldc(compiler, position), true);
    }

    return make_operand(emit_ldi(compiler, (int16_t)value), true);
}

Operand compile_float_value(Compiler* compiler, double value, Type type)
{
    if (type.kind == TypeF64) {
        size_t position = make_f64_constant(compiler, value);
        int reg = emit_ldc_i64(compiler, position);

        return make_wide_operand(reg, true);
    }

    size_t position = make_f32_constant(compiler, (float)value);
    int reg = emit_ldc(compiler, position);

    return make_operand(reg, true);
}

Operand compile_float(Compiler* compiler, ASTNode* ast)
{
    double value = ast->float_literal.value;
    Type type = get_node_data_type(ast);

    return compile_float_value(compiler, value, type);
}

Operand compile_string(Compiler* compiler, ASTNode* ast)
{
    StringObject* literal = ast->string_literal.value;
    StringObject* string = copy_string_object(literal->chars, literal->length);
    Value value = POINTER_VALUE(string);

    push_vector_item(&compiler->module->objects, string);

    size_t position = make_constant(compiler, value);
    int reg = emit_ldc(compiler, position);

    return make_operand(reg, true);
}

Operand compile_character(Compiler* compiler, ASTNode* ast)
{
    uint32_t value = ast->character_literal.value;
    int reg;

    if (value <= INT16_MAX) {
        reg = emit_ldi(compiler, (int16_t)value);
    } else {
        Value constant = U32_VALUE(value);
        size_t position = make_constant(compiler, constant);

        reg = emit_ldc(compiler, position);
    }

    return make_operand(reg, true);
}

Operand compile_boolean(Compiler* compiler, ASTNode* ast)
{
    return make_operand(emit_ldi(compiler, ast->boolean_literal.value), true);
}

static void restore_constant_compiler(Compiler* compiler, const Compiler* previous, FunctionObject* entry)
{
    Vector references = compiler->function_references;

    *compiler = *previous;
    compiler->function_references = references;
    compiler->module->functions.data[0] = entry;
}

static void free_constant_evaluation(VM* vm, FunctionObject* function)
{
    free_vm(vm);
    free_memory(vm);
    free_function_object(function);
}

static Operand compile_constant_expression(Compiler* compiler, ASTNode* ast, FunctionObject* function)
{
    compiler->function = function;
    compiler->module->functions.data[0] = function;
    compiler->register_count = 0;
    compiler->frame_base_count = 0;
    compiler->local_position_offset = 0;
    compiler->peephole_barrier = 0;

    Operand result = compile_expression(compiler, ast->prefix.expr, false);

    emit_hlt(compiler);

    return result;
}

static void store_constant_result(Compiler* compiler, ASTNode* ast, const VM* vm, Operand result)
{
    if (vm->exit_requested) {
        fatal_error("Constant evaluation exited without a value");
    }

    size_t position = count_value_array(&compiler->module->constants);

    for (size_t i = 0; i < result.slots; i++) {
        make_constant(compiler, vm->stack[result.reg + i]);
    }

    ast->prefix.constant_position = position;
    ast->prefix.evaluated = true;
}

void evaluate_constant_expression(Compiler* compiler, ASTNode* ast)
{
    Compiler previous = *compiler;
    FunctionObject* entry = compiler->module->functions.data[0];
    FunctionObject* function = create_function_object();
    VM* vm = allocate_zeroed_memory(1, sizeof(VM));
    ErrorContext context;

    begin_error_context(&context, NULL);

    if (setjmp(context.jump) != 0) {
        end_error_context(&context);
        restore_constant_compiler(compiler, &previous, entry);
        free_constant_evaluation(vm, function);
        raise_error();
    }

    Operand result = compile_constant_expression(compiler, ast, function);

    init_vm(vm, compiler->module);
    interpret(vm);
    store_constant_result(compiler, ast, vm, result);
    end_error_context(&context);
    restore_constant_compiler(compiler, &previous, entry);
    free_constant_evaluation(vm, function);
}

static int emit_scalar_constant(Compiler* compiler, ASTNode* ast, size_t position)
{
    Type type = get_node_data_type(ast);
    Value value = compiler->module->constants.data[position];
    uint64_t integer = AS_UNSIGNED(value);
    bool scalar = is_integer_type(type.kind) || type.kind == TypeBool || type.kind == TypeChar;

    if (type.kind == TypeBool) {
        integer = AS_BOOL(value);
    }

    if (scalar && is_narrow_integer_immediate(integer, type)) {
        return emit_ldi(compiler, (int16_t)integer);
    }

    return emit_ldc(compiler, position);
}

Operand compile_constant(Compiler* compiler, ASTNode* ast)
{
    if (!ast->prefix.evaluated) {
        evaluate_constant_expression(compiler, ast);
    }

    size_t position = ast->prefix.constant_position;
    size_t slots = get_node_slot_count(ast);

    if (slots == 2) {
        int reg = emit_ldc_i64(compiler, position);

        return make_wide_operand(reg, true);
    }

    int reg = emit_scalar_constant(compiler, ast, position);

    return make_operand(reg, true);
}

Operand compile_usize_literal(Compiler* compiler, size_t value)
{
    Type type = {.kind = TypeUsize};

    return compile_integer_value(compiler, value, type);
}
