#include "compiler/generator/slice.h"
#include "common/array_type.h"
#include "compiler/generator/array.h"
#include "compiler/generator/constant.h"
#include "compiler/generator/conversion.h"
#include "compiler/generator/emitter.h"
#include "compiler/generator/expression.h"
#include "compiler/generator/generator.h"

Operand compile_slice_length(Compiler* compiler, ASTNode* ast, bool discard)
{
    ASTNode* argument = ast->builtin_call.args.data[0];
    Operand slice = compile_expression(compiler, argument, false);
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

static Operand compile_slice_source(Compiler* compiler, ASTNode* expression)
{
    Type source = get_node_data_type(expression);
    Operand buffer = compile_expression(compiler, expression, false);
    buffer = materialize_operand(compiler, buffer);

    if (source.kind == TypeSlice) {
        return buffer;
    }

    compile_usize_literal(compiler, source.array->count);

    return make_slice_operand(buffer.reg, true);
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
    Operand length = make_operand(slice.reg + 1, false);

    compile_slice_bounds(compiler, start, end, length);

    Opcode subtract = get_integer_opcode(type, IntSub);

    emit_instruction(compiler, subtract, length.reg, end.reg, start.reg);
    release_operand(compiler, end);

    size_t stride = get_array_storage_size(ast->slice.type_id.array->element_type);
    Operand pointer = make_operand(slice.reg, false);

    scale_array_index(compiler, start, stride);
    emit_buffer_offset(compiler, pointer, start);

    return slice;
}
