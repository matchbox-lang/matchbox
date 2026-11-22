#include "compiler/generator/slice.h"
#include "builtin/string.h"
#include "common/array_type.h"
#include "compiler/generator/array.h"
#include "compiler/generator/constant.h"
#include "compiler/generator/conversion.h"
#include "compiler/generator/emitter.h"
#include "compiler/generator/expression.h"
#include "compiler/generator/function.h"
#include "compiler/generator/generator.h"
#include "compiler/constant.h"
#include "runtime/string_object.h"
#include <stddef.h>
#include <stdint.h>

static Operand compile_string_slice_source(Compiler* compiler, ASTNode* expression)
{
    Operand buffer = compile_expression(compiler, expression, false);
    buffer = materialize_operand(compiler, buffer);
    int length = allocate_register(compiler);
    Opcode load = sizeof(size_t) == sizeof(uint64_t) ? OpLdmi64 : OpLdmi32;

    emit_instruction(compiler, load, length, buffer.reg, offsetof(StringObject, length));

    Operand offset = compile_usize_literal(compiler, offsetof(StringObject, chars));

    emit_buffer_offset(compiler, buffer, offset);

    return make_slice_operand(buffer.reg, true);
}

static Operand compile_slice_source(Compiler* compiler, ASTNode* expression)
{
    Type source = get_node_data_type(expression);

    if (source.kind == TypeString) {
        return compile_string_slice_source(compiler, expression);
    }

    Operand buffer = compile_expression(compiler, expression, false);
    buffer = materialize_operand(compiler, buffer);

    if (is_slice_type(source.kind)) {
        return buffer;
    }

    compile_usize_literal(compiler, source.array->count);

    return make_slice_operand(buffer.reg, true);
}

Operand compile_slice_length(Compiler* compiler, ASTNode* ast, bool discard)
{
    ASTNode* argument = ast->builtin_call.args.data[0];
    Operand slice = compile_slice_source(compiler, argument);
    int reg = slice.reg;

    release_operand(compiler, slice);

    if (discard) {
        return no_operand();
    }

    int destination = allocate_register(compiler);

    emit_mov(compiler, destination, reg + 1);

    return make_operand(destination, true);
}

static void compile_slice_bounds(Compiler* compiler, Operand start, Operand end, Operand length)
{
    Type type = {.kind = TypeUsize};
    start.temporary = false;
    end.temporary = false;
    length.temporary = false;
    Operand reversed = emit_numeric_comparison(compiler, TokenGreater, type, start, end);
    size_t ordered = emit_condition_jump(compiler, reversed);

    release_operand(compiler, reversed);
    compile_index_panic(compiler);
    patch_jump(compiler, ordered);

    Operand outside = emit_numeric_comparison(compiler, TokenGreater, type, end, length);
    size_t bounded = emit_condition_jump(compiler, outside);

    release_operand(compiler, outside);
    compile_index_panic(compiler);
    patch_jump(compiler, bounded);
}

static void compile_string_slice_bounds(Compiler* compiler, ASTNode* ast, Operand slice, Operand start, Operand end)
{
    if (ast->slice.type_id.kind != TypeStringSlice) {
        return;
    }

    static const Builtin builtin = {
        .name = "validate_string_slice",
        .entry = builtin_validate_string_slice,
        .param_count = 4,
        .params = {TypeUsize, TypeUsize, TypeUsize, TypeUsize},
        .return_type_id = TypeVoid
    };

    Type type = {.kind = TypeUsize};
    Type types[] = {type, type, type, type};

    Operand arguments[4];
    arguments[0] = make_operand(slice.reg, false);
    arguments[1] = start;
    arguments[1].temporary = false;
    arguments[2] = end;
    arguments[2].temporary = false;
    arguments[3] = make_operand(slice.reg + 1, false);

    emit_builtin_operands(compiler, &builtin, types, arguments, 4, true);
}

static void compile_slice_checks(Compiler* compiler, ASTNode* ast, Operand slice, Operand start, Operand end)
{
    bool constant = validate_string_slice_constant(ast);

    if (constant) {
        return;
    }

    Operand length = make_operand(slice.reg + 1, false);

    compile_slice_bounds(compiler, start, end, length);
    compile_string_slice_bounds(compiler, ast, slice, start, end);
}

Operand compile_slice(Compiler* compiler, ASTNode* ast)
{
    Operand slice = compile_slice_source(compiler, ast->slice.expr);

    if (!ast->slice.start) {
        return slice;
    }

    Type type = {.kind = TypeUsize};
    Operand start = compile_converted_expression(compiler, ast->slice.start, type, false);
    start = materialize_operand(compiler, start);
    Operand end = compile_converted_expression(compiler, ast->slice.end, type, false);
    end = materialize_operand(compiler, end);

    compile_slice_checks(compiler, ast, slice, start, end);

    Opcode subtract = get_integer_opcode(type, IntSub);
    int length = slice.reg + 1;

    emit_instruction(compiler, subtract, length, end.reg, start.reg);
    release_operand(compiler, end);

    size_t stride = get_array_storage_size(ast->slice.type_id.array->element_type);
    Operand pointer = make_operand(slice.reg, false);

    scale_array_index(compiler, start, stride);
    emit_buffer_offset(compiler, pointer, start);

    return slice;
}

Operand compile_string_slice_comparison(Compiler* compiler, ASTNode* ast)
{
    static const Builtin builtin = {
        .name = "string_slices_equal",
        .entry = builtin_string_slices_equal,
        .param_count = 2,
        .params = {TypeStringSlice, TypeStringSlice},
        .return_type_id = TypeBool
    };

    Type type = {.kind = TypeStringSlice};
    Type types[] = {type, type};
    
    Operand left = compile_slice_source(compiler, ast->binary.left_expr);
    Operand right = compile_slice_source(compiler, ast->binary.right_expr);
    Operand arguments[] = {left, right};
    Operand result = emit_builtin_operands(compiler, &builtin, types, arguments, 2, false);

    if (ast->binary.operator.type == TokenNotEqual) {
        emit_instruction(compiler, OpNot, result.reg, result.reg, 0);
    }

    return result;
}
