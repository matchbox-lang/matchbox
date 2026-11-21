#include "compiler/generator/expression.h"
#include "common/array_type.h"
#include "compiler/generator/array.h"
#include "compiler/generator/constant.h"
#include "compiler/generator/control_flow.h"
#include "compiler/generator/conversion.h"
#include "compiler/generator/emitter.h"
#include "compiler/generator/function.h"
#include "compiler/generator/generator.h"
#include "compiler/generator/operand.h"
#include "compiler/generator/variable.h"
#include "compiler/lexer/token.h"
#include "compiler/ast.h"
#include "runtime/code_object.h"
#include <stddef.h>
#include <stdint.h>

Opcode get_width_opcode(Type type, Opcode opcode64, Opcode opcode32, Opcode opcode16, Opcode opcode8)
{
    size_t size = get_integer_type_size(type.kind);

    switch (size) {
        case 8:
            return opcode64;
        case 4:
            return opcode32;
        case 2:
            return opcode16;
        case 1:
            return opcode8;
        default:
            return OpHlt;
    }
}

static Opcode get_division_opcode(Type type)
{
    if (is_signed_integer_type(type.kind)) {
        return get_width_opcode(type, OpIdivI64, OpIdivI32, OpIdivI16, OpIdivI8);
    }

    return get_width_opcode(type, OpIdivU64, OpIdivU32, OpIdivU16, OpIdivU8);
}

static Opcode get_remainder_opcode(Type type)
{
    if (is_signed_integer_type(type.kind)) {
        return get_width_opcode(type, OpRemI64, OpRemI32, OpRemI16, OpRemI8);
    }

    return get_width_opcode(type, OpRemU64, OpRemU32, OpRemU16, OpRemU8);
}

static Opcode get_right_shift_opcode(Type type)
{
    if (is_signed_integer_type(type.kind)) {
        return get_width_opcode(type, OpAsrI64, OpAsrI32, OpAsrI16, OpAsrI8);
    }

    return get_width_opcode(type, OpLsrI64, OpLsrI32, OpLsrI16, OpLsrI8);
}

Opcode get_integer_opcode(Type type, IntegerOperation operation)
{
    switch (operation) {
        case IntAdd:
            return get_width_opcode(type, OpAddI64, OpAddI32, OpAddI16, OpAddI8);
        case IntSub:
            return get_width_opcode(type, OpSubI64, OpSubI32, OpSubI16, OpSubI8);
        case IntMul:
            return get_width_opcode(type, OpMulI64, OpMulI32, OpMulI16, OpMulI8);
        case IntDiv:
            return get_division_opcode(type);
        case IntRem:
            return get_remainder_opcode(type);
        case IntBand:
            return get_width_opcode(type, OpBandI64, OpBandI32, OpBandI16, OpBandI8);
        case IntBor:
            return get_width_opcode(type, OpBorI64, OpBorI32, OpBorI16, OpBorI8);
        case IntBxor:
            return get_width_opcode(type, OpBxorI64, OpBxorI32, OpBxorI16, OpBxorI8);
        case IntBnot:
            return get_width_opcode(type, OpBnotI64, OpBnotI32, OpBnotI16, OpBnotI8);
        case IntLsl:
            return get_width_opcode(type, OpLslI64, OpLslI32, OpLslI16, OpLslI8);
        case IntLsr:
            return get_right_shift_opcode(type);
        case IntNeg:
            return get_width_opcode(type, OpNegI64, OpNegI32, OpNegI16, OpNegI8);
    }

    return OpHlt;
}

Operand emit_binary_operands(Compiler* compiler, IntegerOperation operation, Type type, Operand left, Operand right)
{
    int destination = select_binary_destination(compiler, left, right, left.slots);
    Opcode opcode = get_integer_opcode(type, operation);

    emit_instruction(compiler, opcode, destination, left.reg, right.reg);

    return left.slots == 2 ? make_wide_operand(destination, true) : make_operand(destination, true);
}

static bool prepare_immediate_comparison(
    Compiler* compiler, Opcode opcode, Operand* left, Operand* right, PreviousInstruction* load)
{
    if ((opcode != OpEq && opcode != OpNe) || !get_previous_instruction(compiler, load)|| load->opcode != OpLdi) {
        return false;
    }

    int16_t value = load->b << 8 | load->c;

    if (left->temporary && !right->temporary && left->slots == 1 && load->a == left->reg) {
        Operand swap = *left;
        *left = *right;
        *right = swap;
    }

    return right->temporary && right->slots == 1 && load->a == right->reg
        && value >= INT8_MIN && value <= INT8_MAX;
}

static Operand emit_immediate_comparison_value(Compiler* compiler, Opcode opcode, Operand left, int8_t value)
{
    int destination = left.reg;

    if (!left.temporary) {
        destination = allocate_register(compiler);
    }

    Opcode immediate = OpEqi;

    if (opcode == OpNe) {
        immediate = OpNei;
    }

    emit_instruction(compiler, immediate, destination, left.reg, value);

    return make_operand(destination, true);
}

Operand emit_immediate_comparison(Compiler* compiler, Opcode opcode, Operand left, Operand right)
{
    PreviousInstruction load;

    if (!prepare_immediate_comparison(compiler, opcode, &left, &right, &load)) {
        return no_operand();
    }

    resize_code_object(load.code, load.start);
    release_operand(compiler, right);

    return emit_immediate_comparison_value(compiler, opcode, left, load.c);
}

static Operand emit_comparison_value(Compiler* compiler, Opcode opcode, Operand left, Operand right)
{
    release_operand(compiler, right);
    release_operand(compiler, left);

    int destination = allocate_register(compiler);

    emit_instruction(compiler, opcode, destination, left.reg, right.reg);

    return make_operand(destination, true);
}

Operand emit_comparison(Compiler* compiler, Opcode opcode, Operand left, Operand right)
{
    Operand immediate = emit_immediate_comparison(compiler, opcode, left, right);

    if (immediate.reg >= 0) {
        return immediate;
    }

    return emit_comparison_value(compiler, opcode, left, right);
}

static Operand emit_reversed_comparison(Compiler* compiler, Opcode opcode, Operand left, Operand right)
{
    return emit_comparison_value(compiler, opcode, right, left);
}

static Opcode get_comparison_opcode(Type type, bool or_equal)
{
    if (is_signed_integer_type(type.kind)) {
        return or_equal ? OpLeI32 : OpLtI32;
    }

    return or_equal ? OpLeU32 : OpLtU32;
}

Opcode get_float_opcode(Type type, TokenType operator_type)
{
    switch (operator_type) {
        case TokenPlus:
        case TokenPlusEqual:
            return type.kind == TypeF64 ? OpAddF64 : OpAddF32;
        case TokenMinus:
        case TokenMinusEqual:
            return type.kind == TypeF64 ? OpSubF64 : OpSubF32;
        case TokenStar:
        case TokenStarEqual:
            return type.kind == TypeF64 ? OpMulF64 : OpMulF32;
        case TokenSlash:
        case TokenSlashEqual:
            return type.kind == TypeF64 ? OpDivF64 : OpDivF32;
        case TokenPercent:
        case TokenPercentEqual:
            return type.kind == TypeF64 ? OpRemF64 : OpRemF32;
        default:
            return OpHlt;
    }
}

Operand emit_float_operands(Compiler* compiler, Opcode opcode, Type type, Operand left, Operand right)
{
    size_t slots = get_type_slot_count(type.kind);
    int destination = select_binary_destination(compiler, left, right, slots);

    emit_instruction(compiler, opcode, destination, left.reg, right.reg);

    return type.kind == TypeF64
        ? make_wide_operand(destination, true)
        : make_operand(destination, true);
}

static Operand emit_ordered_comparison(Compiler* compiler, Type type,
    Operand left, Operand right, bool or_equal, bool reversed)
{
    Opcode opcode = get_comparison_opcode(type, or_equal);

    if (reversed) {
        return emit_reversed_comparison(compiler, opcode, left, right);
    }

    return emit_comparison(compiler, opcode, left, right);
}

static Opcode get_f32_three_way_opcode(TokenType operator_type)
{
    if (operator_type == TokenLess || operator_type == TokenLessEqual) {
        return OpCmpgF32;
    }

    return OpCmplF32;
}

static Opcode get_f64_three_way_opcode(TokenType operator_type)
{
    if (operator_type == TokenLess || operator_type == TokenLessEqual) {
        return OpCmpgF64;
    }

    return OpCmplF64;
}

static Opcode get_integer_three_way_opcode(Type type)
{
    size_t size = get_integer_type_size(type.kind);
    bool signed_type = is_signed_integer_type(type.kind);

    if (size == 8) {
        return signed_type ? OpCmpI64 : OpCmpU64;
    }

    return signed_type ? OpCmpI32 : OpCmpU32;
}

Opcode get_three_way_opcode(Type type, TokenType operator_type)
{
    switch (type.kind) {
        case TypeF32:
            return get_f32_three_way_opcode(operator_type);
        case TypeF64:
            return get_f64_three_way_opcode(operator_type);
        default:
            return get_integer_three_way_opcode(type);
    }
}

static Operand emit_integer_comparison(Compiler* compiler, TokenType operator_type,
    Type type, Operand left, Operand right)
{
    switch (operator_type) {
        case TokenEqualEqual:
            return emit_comparison(compiler, OpEq, left, right);
        case TokenNotEqual:
            return emit_comparison(compiler, OpNe, left, right);
        case TokenLess:
            return emit_ordered_comparison(compiler, type, left, right, false, false);
        case TokenLessEqual:
            return emit_ordered_comparison(compiler, type, left, right, true, false);
        case TokenGreater:
            return emit_ordered_comparison(compiler, type, left, right, false, true);
        default:
            return emit_ordered_comparison(compiler, type, left, right, true, true);
    }
}

Operand emit_numeric_comparison(Compiler* compiler, TokenType operator_type, Type type, Operand left, Operand right)
{
    bool floating = is_float_type(type.kind);
    bool wide = get_integer_type_size(type.kind) == 8;

    if (!floating && !wide && operator_type != TokenSpaceship) {
        return emit_integer_comparison(compiler, operator_type, type, left, right);
    }

    Opcode opcode = get_three_way_opcode(type, operator_type);
    Operand ordering = emit_comparison_value(compiler, opcode, left, right);

    if (operator_type == TokenSpaceship) {
        return ordering;
    }

    int zero_register = allocate_register(compiler);
    Operand zero = make_operand(zero_register, true);

    emit_instruction(compiler, OpLdi, zero_register, 0, 0);

    return emit_numeric_comparison(compiler, operator_type, (Type){.kind = TypeI32}, ordering, zero);
}

Operand normalize_integer_operand(Compiler* compiler, Operand operand, Type type)
{
    size_t size = get_integer_type_size(type.kind);

    if (size != 1 && size != 2) {
        return operand;
    }

    operand = materialize_operand(compiler, operand);

    Opcode opcode = OpZextI32;
    int width = size * 8;

    if (is_signed_integer_type(type.kind)) {
        opcode = OpSextI32;
    }

    emit_instruction(compiler, opcode, operand.reg, operand.reg, width);

    return operand;
}

Operand compile_comparison_operand(Compiler* compiler, ASTNode* ast, Type type)
{
    Operand value = compile_converted_expression(compiler, ast, type, false);

    return normalize_integer_operand(compiler, value, type);
}

static Operand emit_integer_binary_operator(
    Compiler* compiler, TokenType operator_type, Type type, Operand left, Operand right)
{
    switch (operator_type) {
        case TokenPlus:
            return emit_binary_operands(compiler, IntAdd, type, left, right);
        case TokenMinus:
            return emit_binary_operands(compiler, IntSub, type, left, right);
        case TokenStar:
            return emit_binary_operands(compiler, IntMul, type, left, right);
        case TokenSlash:
            return emit_binary_operands(compiler, IntDiv, type, left, right);
        case TokenPercent:
            return emit_binary_operands(compiler, IntRem, type, left, right);
        case TokenKeywordAnd:
        case TokenAmpersand:
            return emit_binary_operands(compiler, IntBand, type, left, right);
        case TokenKeywordOr:
        case TokenPipe:
            return emit_binary_operands(compiler, IntBor, type, left, right);
        case TokenCircumflex:
            return emit_binary_operands(compiler, IntBxor, type, left, right);
        case TokenLshift:
            return emit_binary_operands(compiler, IntLsl, type, left, right);
        case TokenRshift:
            return emit_binary_operands(compiler, IntLsr, type, left, right);
        default:
            return no_operand();
    }
}

Operand emit_binary_operator(Compiler* compiler, TokenType operator_type, Type type, Operand left, Operand right)
{
    if (type.kind == TypeString) {
        Opcode opcode = operator_type == TokenEqualEqual ? OpEqString : OpNeString;

        return emit_comparison(compiler, opcode, left, right);
    }

    bool comparison = is_comparison_token(operator_type) || is_equality_token(operator_type);

    if (comparison) {
        left = normalize_integer_operand(compiler, left, type);
        right = normalize_integer_operand(compiler, right, type);

        return emit_numeric_comparison(compiler, operator_type, type, left, right);
    }

    if (is_float_type(type.kind)) {
        Opcode opcode = get_float_opcode(type, operator_type);

        return emit_float_operands(compiler, opcode, type, left, right);
    }

    return emit_integer_binary_operator(compiler, operator_type, type, left, right);
}

Operand compile_binary(Compiler* compiler, ASTNode* ast)
{
    Type left_type = get_node_data_type(ast->binary.left_expr);

    if (is_array_type(left_type.kind)) {
        return compile_array_binary(compiler, ast);
    }

    TokenType operator_type = ast->binary.operator.type;

    if (is_logical_token(operator_type)) {
        return compile_logical_binary(compiler, ast);
    }

    Type right_type = get_node_data_type(ast->binary.right_expr);
    Type type = ast->binary.operand_type;
    Type result_type = ast->binary.type_id;
    bool concatenation = result_type.kind == TypeString;
    Type left_destination = concatenation ? left_type : type;
    Type right_destination = concatenation ? right_type : type;
    Operand left = compile_converted_expression(compiler, ast->binary.left_expr, left_destination, false);

    if (!left.temporary && !is_direct_variable(compiler, ast->binary.right_expr)) {
        left = materialize_operand(compiler, left);
    }

    Operand right = compile_converted_expression(compiler, ast->binary.right_expr, right_destination, false);

    if (concatenation || operator_type == TokenPower) {
        return emit_binary_builtin(compiler, operator_type, left_type, right_type, left, right);
    }

    return emit_binary_operator(compiler, operator_type, type, left, right);
}

static Operand emit_unary_operand(Compiler* compiler, Opcode opcode, Operand operand)
{
    if (operand.temporary) {
        emit_instruction(compiler, opcode, operand.reg, operand.reg, 0);

        return operand;
    }

    int destination = allocate_registers(compiler, operand.slots);
    emit_instruction(compiler, opcode, destination, operand.reg, 0);

    return operand.slots == 2 ? make_wide_operand(destination, true) : make_operand(destination, true);
}

static Opcode get_negation_opcode(Type type)
{
    if (type.kind == TypeF32) {
        return OpNegF32;
    }

    if (type.kind == TypeF64) {
        return OpNegF64;
    }

    return get_integer_opcode(type, IntNeg);
}

static Opcode get_prefix_opcode(TokenType operator_type, Type type)
{
    switch (operator_type) {
        case TokenKeywordNot:
            return OpNot;
        case TokenTilde:
            return get_integer_opcode(type, IntBnot);
        case TokenMinus:
            return get_negation_opcode(type);
        default:
            return OpHlt;
    }
}

Operand emit_prefix_operand(Compiler* compiler, TokenType operator_type, Type type, Operand operand)
{
    Opcode opcode = get_prefix_opcode(operator_type, type);

    if (opcode == OpHlt) {
        return no_operand();
    }

    return emit_unary_operand(compiler, opcode, operand);
}

Operand compile_prefix(Compiler* compiler, ASTNode* ast)
{
    if (ast->prefix.operator.type == TokenKeywordConst) {
        return compile_constant(compiler, ast);
    }

    if (get_reference_type(ast) != RefNone) {
        return dereference_operand(
            compiler, compile_reference_expression(compiler, ast), get_type_slot_count(get_node_data_type(ast).kind));
    }

    Type type = get_node_data_type(ast);

    if (is_array_type(type.kind)) {
        return compile_array_prefix(compiler, ast);
    }

    Operand operand = compile_expression(compiler, ast->prefix.expr, false);

    return emit_prefix_operand(compiler, ast->prefix.operator.type, type, operand);
}
