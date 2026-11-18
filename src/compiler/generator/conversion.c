#include "compiler/generator/conversion.h"
#include "compiler/ast.h"
#include "compiler/generator/constant.h"
#include "compiler/generator/emitter.h"
#include "compiler/generator/expression.h"
#include "compiler/generator/generator.h"
#include "compiler/generator/operand.h"
#include <stddef.h>
#include <stdint.h>

static Operand compile_reference_aware_expression(Compiler* compiler, ASTNode* ast, bool reference)
{
    if (reference) {
        return compile_reference_expression(compiler, ast);
    }

    return compile_expression(compiler, ast, false);
}

static Operand normalize_unsigned_operand(Compiler* compiler, Operand operand, Type source)
{
#if UINTPTR_MAX == UINT64_MAX
    if (source.kind == TypeU32) {
        operand = materialize_operand(compiler, operand);

        emit_instruction(compiler, OpLsriI32, operand.reg, operand.reg, 0);

        return operand;
    }
#endif

    if (source.kind != TypeU8 && source.kind != TypeU16) {
        return operand;
    }

    return normalize_integer_operand(compiler, operand, source);
}

static Operand extend_integer_operand(Compiler* compiler, Operand operand, Type source)
{
    int destination_register = operand.reg;

    if (!operand.temporary) {
        destination_register = allocate_register(compiler);

        emit_mov(compiler, destination_register, operand.reg);
    }

    int high_register = allocate_register(compiler);

#if UINTPTR_MAX == UINT64_MAX
    (void)high_register;
    (void)source;

    return make_wide_operand(destination_register, true);
#else
    if (is_signed_integer_type(source.kind)) {
        emit_instruction(compiler, OpAsriI32, high_register, destination_register, 31);

        return make_wide_operand(destination_register, true);
    }

    emit_instruction(compiler, OpLdi, high_register, 0, 0);

    return make_wide_operand(destination_register, true);
#endif
}

static Operand shrink_integer_operand(Compiler* compiler, Operand operand)
{
    if (operand.slots == 1) {
        return operand;
    }

    if (!operand.temporary) {
        return make_operand(operand.reg, false);
    }

    release_operand(compiler, operand);

    int result = allocate_register(compiler);

    return make_operand(result, true);
}

static Operand compile_integer_cast(Compiler* compiler, Operand operand, Type source, Type destination)
{
    operand = normalize_unsigned_operand(compiler, operand, source);

    if (get_type_slot_count(destination.kind) == 2 && operand.slots == 1) {
        return extend_integer_operand(compiler, operand, source);
    }

    if (get_type_slot_count(destination.kind) == 2) {
        return operand;
    }

    operand = shrink_integer_operand(compiler, operand);
    operand = materialize_operand(compiler, operand);

    size_t size = get_integer_type_size(destination.kind);

    if (size == 8) {
        return operand;
    }

    if (size < 4) {
        return normalize_integer_operand(compiler, operand, destination);
    }

    Opcode opcode = OpLsriI32;

    if (is_signed_integer_type(destination.kind)) {
        opcode = OpAsriI32;
    }

    emit_instruction(compiler, opcode, operand.reg, operand.reg, 0);

    return operand;
}

static Operand widen_integer_operand(Compiler* compiler, Operand operand,
    Type source, Type destination)
{
    if (!can_implicitly_widen_integer(source.kind, destination.kind)
        || get_type_slot_count(destination.kind) == operand.slots) {
        return operand;
    }

    return compile_integer_cast(compiler, operand, source, destination);
}

static Operand widen_float_operand(Compiler* compiler, Operand operand,
    Type source, Type destination)
{
    if (source.kind != TypeF32 || destination.kind != TypeF64) {
        return operand;
    }

    int destination_register;

    if (operand.temporary) {
        destination_register = operand.reg;
        allocate_register(compiler);
    } else {
        destination_register = allocate_registers(compiler, 2);
    }

    emit_instruction(compiler, OpF32ToF64, destination_register, operand.reg, 0);

    return make_wide_operand(destination_register, true);
}

static Operand convert_integer_to_float_operand(Compiler* compiler, Operand operand,
    Type source, Type destination)
{
    if (!can_implicitly_convert_integer_to_float(source.kind, destination.kind)) {
        return operand;
    }

    bool wide = destination.kind == TypeF64;
    int destination_register = operand.reg;

    if (!operand.temporary) {
        destination_register = allocate_registers(compiler, wide ? 2 : 1);
    } else if (wide) {
        allocate_register(compiler);
    }

    bool signed_integer = is_signed_integer_type(source.kind);
    Opcode opcode;

    if (wide) {
        opcode = signed_integer ? OpI32ToF64 : OpU32ToF64;
    } else {
        opcode = signed_integer ? OpI32ToF32 : OpU32ToF32;
    }

    emit_instruction(compiler, opcode, destination_register, operand.reg, 0);

    if (wide) {
        return make_wide_operand(destination_register, true);
    }

    return make_operand(destination_register, true);
}

static uint64_t get_widened_integer_literal_value(ASTNode* ast, Type source)
{
    uint64_t value = ast->integer_literal.value;

    switch (source.kind) {
        case TypeI8:
            return (uint64_t)(int64_t)(int8_t)value;
        case TypeI16:
            return (uint64_t)(int64_t)(int16_t)value;
        case TypeI32:
            return (uint64_t)(int64_t)(int32_t)value;
        case TypeU8:
            return (uint8_t)value;
        case TypeU16:
            return (uint16_t)value;
        case TypeU32:
            return (uint32_t)value;
        default:
            return value;
    }
}

static Operand compile_widened_integer_literal(Compiler* compiler, ASTNode* ast, Type destination)
{
    Type source = get_node_data_type(ast);
    uint64_t value = get_widened_integer_literal_value(ast, source);

    return compile_integer_value(compiler, value, destination);
}

static Operand compile_integer_literal_as_float(Compiler* compiler, ASTNode* ast, Type destination)
{
    Type source = get_node_data_type(ast);
    uint64_t integer = get_widened_integer_literal_value(ast, source);
    double number = integer;

    if (is_signed_integer_type(source.kind)) {
        number = (int64_t)integer;
    }

    if (source.kind == TypeF32) {
        number = (float)number;
    }

    return compile_float_value(compiler, number, destination);
}

Operand convert_operand(Compiler* compiler, Operand operand, Type source, Type destination)
{
    if (!types_equal(source, destination) && can_implicitly_widen_type(source.kind, destination.kind)) {
        operand = normalize_unsigned_operand(compiler, operand, source);
    }

    operand = widen_integer_operand(compiler, operand, source, destination);
    operand = convert_integer_to_float_operand(compiler, operand, source, destination);

    return widen_float_operand(compiler, operand, source, destination);
}

Operand compile_converted_expression(Compiler* compiler, ASTNode* ast,
    Type destination, bool reference)
{
    Type source = get_node_data_type(ast);
    bool integer_to_float = can_implicitly_convert_integer_to_float(source.kind, destination.kind);
    bool float_widening = can_implicitly_widen_float(source.kind, destination.kind);

    if (!reference && ast->type == ASTInteger && (integer_to_float || float_widening)) {
        return compile_integer_literal_as_float(compiler, ast, destination);
    }

    if (!reference && ast->type == ASTInteger && can_implicitly_widen_integer(source.kind, destination.kind)) {
        return compile_widened_integer_literal(compiler, ast, destination);
    }

    if (!reference && ast->type == ASTFloat && source.kind == TypeF32 && destination.kind == TypeF64) {
        double value = (float)ast->float_literal.value;

        return compile_float_value(compiler, value, destination);
    }

    Operand operand = compile_reference_aware_expression(compiler, ast, reference);

    if (reference) {
        return operand;
    }

    return convert_operand(compiler, operand, source, destination);
}

static Opcode get_float_to_integer_opcode(Type source, Type destination)
{
    static const Opcode opcodes[2][4] = {
        {OpF32ToI32, OpF32ToU32, OpF32ToI64, OpF32ToU64},
        {OpF64ToI32, OpF64ToU32, OpF64ToI64, OpF64ToU64}
    };
    int index = 0;

    if (!is_signed_integer_type(destination.kind)) {
        index++;
    }

    if (get_integer_type_size(destination.kind) == 8) {
        index += 2;
    }

    return opcodes[source.kind == TypeF64][index];
}

static Opcode get_integer_to_float_opcode(Type source, Type destination)
{
    static const Opcode signed_opcodes[2][2] = {
        {OpI32ToF32, OpI32ToF64},
        {OpI64ToF32, OpI64ToF64}
    };
    static const Opcode unsigned_opcodes[2][2] = {
        {OpU32ToF32, OpU32ToF64},
        {OpU64ToF32, OpU64ToF64}
    };
    bool wide_source = get_integer_type_size(source.kind) == 8;
    bool wide_destination = destination.kind == TypeF64;

    if (is_signed_integer_type(source.kind)) {
        return signed_opcodes[wide_source][wide_destination];
    }

    return unsigned_opcodes[wide_source][wide_destination];
}

static Opcode get_numeric_conversion_opcode(Type source, Type destination)
{
    if (is_integer_type(destination.kind)) {
        return get_float_to_integer_opcode(source, destination);
    }

    if (is_integer_type(source.kind)) {
        return get_integer_to_float_opcode(source, destination);
    }

    return OpF64ToF32;
}

static Operand emit_conversion_operand(Compiler* compiler, Operand operand, Type destination, Opcode opcode)
{
    size_t slots = get_type_slot_count(destination.kind);

    release_operand(compiler, operand);

    int result = allocate_registers(compiler, slots);

    emit_instruction(compiler, opcode, result, operand.reg, 0);

    if (slots == 2) {
        return make_wide_operand(result, true);
    }

    return make_operand(result, true);
}

static Operand compile_character_conversion(Compiler* compiler, ASTNode* ast)
{
    ASTNode* expression = ast->conversion.expr;
    Type source = get_node_data_type(expression);
    Type destination = ast->conversion.type_id;

    Operand operand = compile_expression(compiler, expression, false);

    if (destination.kind == TypeChar) {
        return normalize_unsigned_operand(compiler, operand, source);
    }

    return compile_integer_cast(compiler, operand, (Type){.kind = TypeU32}, destination);
}

static Operand compile_boolean_to_integer(Compiler* compiler, ASTNode* expression, Type destination)
{
    Operand operand = compile_expression(compiler, expression, false);

    operand = materialize_operand(compiler, operand);

    emit_instruction(compiler, OpBandiI32, operand.reg, operand.reg, 1);

    if (get_type_slot_count(destination.kind) == 2) {
        return extend_integer_operand(compiler, operand, (Type){.kind = TypeU32});
    }

    return operand;
}

Operand compile_explicit_conversion(Compiler* compiler, ASTNode* ast)
{
    ASTNode* expression = ast->conversion.expr;
    Type source = get_node_data_type(expression);
    Type destination = ast->conversion.type_id;

    if (types_equal(source, destination) || can_implicitly_widen_type(source.kind, destination.kind)) {
        return compile_converted_expression(compiler, expression, destination, false);
    }

    if (source.kind == TypeBool) {
        return compile_boolean_to_integer(compiler, expression, destination);
    }

    if (source.kind == TypeChar || destination.kind == TypeChar) {
        return compile_character_conversion(compiler, ast);
    }

    Operand operand = compile_expression(compiler, expression, false);

    if (is_integer_type(source.kind) && is_integer_type(destination.kind)) {
        return compile_integer_cast(compiler, operand, source, destination);
    }

    operand = normalize_unsigned_operand(compiler, operand, source);

    Opcode opcode = get_numeric_conversion_opcode(source, destination);

    return emit_conversion_operand(compiler, operand, destination, opcode);
}
