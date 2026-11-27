#include "compiler/generator/operand.h"
#include "common/errors.h"
#include "common/math.h"
#include "common/vector.h"
#include "compiler/ast/ast.h"
#include "compiler/ast/query.h"
#include "compiler/ast/reference.h"
#include "compiler/generator/emitter.h"
#include "compiler/generator/memory.h"
#include "runtime/string_object.h"
#include "runtime/value.h"
#include <stddef.h>
#include <stdint.h>

_Noreturn void function_register_index_overflow_error(const char* name)
{
    fatal_error("Function %s requires more than %u virtual registers", name, UINT16_MAX + 1u);
}

static _Noreturn void top_level_register_index_overflow_error(void)
{
    fatal_error("Top-level code requires more than %u virtual registers", UINT16_MAX + 1u);
}

ASTNode* get_current_function_definition(Compiler* compiler)
{
    size_t count = count_vector(&compiler->module->functions);

    for (size_t i = 0; i < count; i++) {
        FunctionObject* function = get_vector_at(&compiler->module->functions, i);

        if (function == compiler->function) {
            ASTNode* reference = get_vector_at(&compiler->function_references, i);

            return reference;
        }
    }

    return NULL;
}

void check_register_count(Compiler* compiler, int count)
{
    if (count <= (int)UINT16_MAX + 1) {
        return;
    }

    ASTNode* function = get_current_function_definition(compiler);
    if (!function) {
        top_level_register_index_overflow_error();
    }

    function_register_index_overflow_error(function->function_definition.id->chars);
}

bool is_compiling_top_level(Compiler* compiler)
{
    return compiler->function == get_vector_at(&compiler->module->functions, 0);
}

int allocate_register(Compiler* compiler)
{
    check_register_count(compiler, compiler->register_count + 1);

    int reg = compiler->register_count++;
    int extent = compiler->register_count + 2;

    compiler->function->max_stack_count = MAX(compiler->function->max_stack_count, extent);

    return reg;
}

int release_register(Compiler* compiler)
{
    return --compiler->register_count;
}

int allocate_registers(Compiler* compiler, size_t count)
{
    int first = compiler->register_count;

    for (size_t i = 0; i < count; i++) {
        allocate_register(compiler);
    }

    return first;
}

void reserve_pointer_padding(Compiler* compiler)
{
    if (sizeof(void*) == sizeof(uint64_t)) {
        allocate_register(compiler);
    }
}

void release_pointer_padding(Compiler* compiler)
{
    if (sizeof(void*) == sizeof(uint64_t)) {
        release_register(compiler);
    }
}

Operand make_operand(int reg, bool temporary)
{
    Operand operand = {reg, temporary, 1, false};

    return operand;
}

Operand make_wide_operand(int reg, bool temporary)
{
    Operand operand = {reg, temporary, 2, false};

    return operand;
}

Operand make_slice_operand(int reg, bool temporary)
{
    Operand operand = {reg, temporary, 2, true};

    return operand;
}

Operand make_typed_operand(int reg, bool temporary, Type type)
{
    size_t slots = get_value_slot_count(type);
    bool cells = type.kind == TypeTuple || is_slice_type(type.kind);
    Operand result = {reg, temporary, slots, cells};

    return result;
}

Operand no_operand(void)
{
    return make_operand(-1, false);
}

void release_operand(Compiler* compiler, Operand operand)
{
    for (size_t i = 0; operand.temporary && i < operand.slots; i++) {
        release_register(compiler);
    }
}

Operand materialize_operand(Compiler* compiler, Operand operand)
{
    if (operand.temporary) {
        return operand;
    }

    int reg = allocate_registers(compiler, operand.slots);

    emit_operand_mov(compiler, reg, operand);

    operand.reg = reg;
    operand.temporary = true;

    return operand;
}

size_t get_node_slot_count(ASTNode* ast)
{
    Type type = get_node_data_type(ast);

    if (get_reference_type(ast) != RefNone && !is_slice_type(type.kind)) {
        return 1;
    }

    return get_value_slot_count(type);
}

static Operand dereference_cells(Compiler* compiler, Operand operand, size_t slots)
{
    int reg = allocate_registers(compiler, slots);
    Opcode load = sizeof(Value) == 8 ? OpLdm64 : OpLdm32;

    for (size_t i = 0; i < slots; i++) {
        emit_memory_access(compiler, load, reg + i, operand.reg, i * sizeof(Value));
    }

    Operand result = {reg, true, slots, true};

    if (!operand.temporary) {
        return result;
    }

    emit_operand_mov(compiler, operand.reg, result);

    result.reg = operand.reg;
    compiler->register_count = result.reg + slots;

    return result;
}

Operand dereference_operand(Compiler* compiler, Operand operand, size_t slots)
{
    if (operand.pair || slots > 2) {
        return dereference_cells(compiler, operand, slots);
    }

    Opcode opcode = slots == 2 ? OpLdr64 : OpLdr;

    if (!operand.temporary) {
        int reg = allocate_registers(compiler, slots);

        emit_instruction(compiler, opcode, reg, operand.reg, 0);

        return slots == 2 ? make_wide_operand(reg, true) : make_operand(reg, true);
    }

    if (slots == 2) {
        allocate_register(compiler);
    }

    emit_instruction(compiler, opcode, operand.reg, operand.reg, 0);

    return slots == 2 ? make_wide_operand(operand.reg, true) : operand;
}

int select_binary_destination(Compiler* compiler, Operand left, Operand right, size_t slots)
{
    if (left.temporary) {
        release_operand(compiler, right);

        return left.reg;
    }

    if (right.temporary) {
        return right.reg;
    }

    return allocate_registers(compiler, slots);
}
