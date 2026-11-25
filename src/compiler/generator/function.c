#include "compiler/generator/function.h"
#include "builtin/builtin.h"
#include "builtin/memory.h"
#include "common/array_type.h"
#include "common/errors.h"
#include "common/memory.h"
#include "common/vector.h"
#include "compiler/ast/ast.h"
#include "compiler/ast/generic.h"
#include "compiler/ast/predicate.h"
#include "compiler/ast/query.h"
#include "compiler/ast/reference.h"
#include "compiler/generator/array.h"
#include "compiler/generator/constant.h"
#include "compiler/generator/conversion.h"
#include "compiler/generator/emitter.h"
#include "compiler/generator/generator.h"
#include "compiler/generator/operand.h"
#include "compiler/generator/slice.h"
#include "compiler/optimizer/bytecode_optimizer.h"
#include "compiler/scope.h"
#include "runtime/argument_format.h"
#include "runtime/function_object.h"
#include "runtime/string_object.h"
#include <stddef.h>
#include <stdint.h>

static _Noreturn void function_index_overflow_error(void)
{
    fatal_error("Cannot create more than %u functions", UINT16_MAX + 1u);
}

static _Noreturn void function_not_found_error(ASTNode* ast)
{
    fatal_error("Could not find function %s", ast->function_definition.id->chars);
}

static int get_call_area_count(const FunctionObject* function)
{
    return function->param_count;
}

CallArea open_call_area(Compiler* compiler)
{
    CallArea call = { .result_register = compiler->register_count };

    allocate_register(compiler);
    allocate_register(compiler);
    call.frame_register = compiler->register_count;

    return call;
}

static void relocate_call_area(Compiler* compiler, CallArea* call, size_t argument_slots)
{
    int first_argument = call->frame_register;
    CallArea relocated = open_call_area(compiler);

    for (size_t i = 0; i < argument_slots; i++) {
        int destination = allocate_register(compiler);

        emit_mov(compiler, destination, first_argument + i);
    }

    call->frame_register = relocated.frame_register;
}

Operand close_call_area(
    Compiler* compiler, FunctionObject* function, uint16_t function_position, CallArea call, bool discard)
{
    int end = call.frame_register + get_call_area_count(function);

    while (compiler->register_count < end) {
        emit_ldi(compiler, 0);
    }

    emit_call_instruction(compiler, call.frame_register, function_position);

    if (!discard && function->return_count) {
        Operand result = {call.frame_register - 2, true, function->return_count, false};

        emit_operand_mov(compiler, call.result_register, result);
        result.reg = call.result_register;
        compiler->register_count = call.result_register + function->return_count;

        return result;
    }

    compiler->register_count = call.result_register;

    return no_operand();
}

static uint16_t make_function_position(size_t position)
{
    if (position > UINT16_MAX) {
        function_index_overflow_error();
    }

    return (uint16_t)position;
}

static FunctionObject* create_builtin_function_object(const Builtin* builtin)
{
    FunctionObject* function = create_function_object();
    function->type = FuncBuiltin;
    function->entry = builtin->entry;
    function->param_count = 0;
    function->return_count = get_type_slot_count(builtin->return_type_id);

    for (int i = 0; i < builtin->param_count; i++) {
        function->param_count += get_type_slot_count(builtin->params[i]);
    }

    if (builtin->return_type_id == TypeVoid) {
        function->return_count = 0;
    }

    function->max_stack_count = 2 + get_call_area_count(function);

    return function;
}

static bool matches_builtin_function(const FunctionObject* function, const Builtin* builtin)
{
    return function->type == FuncBuiltin && function->entry == builtin->entry;
}

size_t find_builtin_function_position(const Compiler* compiler, const Builtin* builtin)
{
    const Vector* functions = &compiler->module->functions;
    size_t position = 0;

    while (position < functions->count && !matches_builtin_function(functions->data[position], builtin)) {
        position++;
    }

    if (position == functions->count) {
        return SIZE_MAX;
    }

    return position;
}

uint16_t add_builtin_function(Compiler* compiler, const Builtin* builtin)
{
    size_t function_count = count_vector(&compiler->module->functions);
    uint16_t position = make_function_position(function_count);
    FunctionObject* function = create_builtin_function_object(builtin);

    push_vector_item(&compiler->module->functions, function);
    push_vector_item(&compiler->function_references, NULL);

    return position;
}

static bool matches_builtin_arguments(const FunctionObject* function, const Builtin* builtin,
    const Type* types, size_t count)
{
    if (function->type != FuncBuiltin || function->entry != builtin->entry
        || function->argument_count != count) {
        return false;
    }

    if (builtin->param_count == BUILTIN_TYPED_VARIADIC) {
        return true;
    }

    bool matches = true;

    for (size_t i = 0; i < count && matches; i++) {
        Type type = types[i];
        matches = matches_argument_format(&function->argument_formats[i], type);
    }

    return matches;
}

static Type get_builtin_argument_type(const Builtin* builtin, Type type)
{
    if (builtin->param_count == BUILTIN_TYPED_VARIADIC) {
        return (Type){.kind = builtin->params[0]};
    }

    return type;
}

static void set_builtin_argument_format(Compiler* compiler, FunctionObject* function, Type type, size_t index)
{
    if (!function->argument_formats) {
        return;
    }

    ArgumentFormat format = compile_argument_format(compiler->module, type);
    format.slot = function->param_count;
    function->argument_formats[index] = format;
}

static FunctionObject* create_variadic_builtin_function(Compiler* compiler, const Builtin* builtin,
    const Type* types, size_t count)
{
    FunctionObject* function = create_builtin_function_object(builtin);

    if (count > SIZE_MAX / sizeof(ArgumentFormat)) {
        out_of_memory_error();
    }

    if (builtin->param_count == BUILTIN_VARIADIC && count > 0) {
        function->argument_formats = allocate_memory(count * sizeof(ArgumentFormat));
    }

    function->argument_count = count;

    for (size_t i = 0; i < count; i++) {
        Type type = get_builtin_argument_type(builtin, types[i]);

        set_builtin_argument_format(compiler, function, type, i);

        function->param_count += get_type_slot_count(type.kind);
    }

    function->max_stack_count = 2 + get_call_area_count(function);

    return function;
}

static size_t find_typed_builtin_call_position(const Compiler* compiler, const Builtin* builtin,
    const Type* types, size_t argument_count)
{
    if (builtin->param_count >= 0) {
        return find_builtin_function_position(compiler, builtin);
    }

    const Vector* functions = &compiler->module->functions;
    size_t position = 0;

    while (position < functions->count
        && !matches_builtin_arguments(functions->data[position], builtin, types, argument_count)) {
        position++;
    }

    if (position == functions->count) {
        return SIZE_MAX;
    }

    return position;
}

static uint16_t add_typed_builtin_call(Compiler* compiler, const Builtin* builtin,
    const Type* types, size_t argument_count)
{
    if (builtin->param_count >= 0) {
        return add_builtin_function(compiler, builtin);
    }

    size_t count = compiler->module->functions.count;
    uint16_t position = make_function_position(count);
    FunctionObject* function = create_variadic_builtin_function(compiler, builtin, types, argument_count);

    push_vector_item(&compiler->module->functions, function);
    push_vector_item(&compiler->function_references, NULL);

    return position;
}

Operand emit_builtin_operands(Compiler* compiler, const Builtin* builtin,
    const Type* types, const Operand* arguments, size_t count, bool discard)
{
    size_t position = find_typed_builtin_call_position(compiler, builtin, types, count);

    if (position == SIZE_MAX) {
        position = add_typed_builtin_call(compiler, builtin, types, count);
    }

    FunctionObject* function = get_vector_at(&compiler->module->functions, position);
    CallArea call = open_call_area(compiler);

    for (size_t i = 0; i < count; i++) {
        Operand argument = arguments[i];
        int reg = allocate_registers(compiler, argument.slots);

        emit_operand_mov(compiler, reg, argument);
    }

    Operand result = close_call_area(compiler, function, position, call, discard);

    release_operand(compiler, result);

    for (size_t i = count; i > 0; i--) {
        release_operand(compiler, arguments[i - 1]);
    }

    if (!result.temporary) {
        return result;
    }

    int destination = allocate_registers(compiler, result.slots);

    emit_operand_mov(compiler, destination, result);

    result.reg = destination;

    return result;
}

Operand emit_binary_builtin(Compiler* compiler, TokenType operator_type,
    Type left_type, Type right_type, Operand left, Operand right)
{
    const char* name = operator_type == TokenPower ? "pow" : "string";
    Type types[] = {left_type, right_type};
    Operand arguments[] = {left, right};
    const Builtin* builtin = resolve_builtin(name, types, 2);

    return emit_builtin_operands(compiler, builtin, types, arguments, 2, false);
}

static bool requires_reserved_argument(Vector* args, Vector* params, size_t position)
{
    if (!params) {
        return false;
    }

    ASTNode* argument = get_vector_at(args, position);
    ASTNode* parameter = get_vector_at(params, position);

    return get_reference_type(parameter) != RefNone
        && get_reference_type(argument) == RefNone
        && argument->type != ASTVariable;
}

static bool requires_reserved_arguments(Vector* args, Vector* params)
{
    size_t count = count_vector(args);

    for (size_t i = 0; i < count; i++) {
        if (requires_reserved_argument(args, params, i)) {
            return true;
        }
    }

    return false;
}

static void reserve_argument_registers(Compiler* compiler, size_t count, bool reserved)
{
    for (size_t i = 0; reserved && i < count; i++) {
        allocate_register(compiler);
    }
}

static ASTNode* get_argument_expression(Vector* args, ASTNode* parameter, size_t position)
{
    if (position < args->count) {
        return args->data[position];
    }

    return parameter->parameter.default_value;
}

static Operand compile_argument_expression(Compiler* compiler, Vector* args, Vector* params, size_t position)
{
    ASTNode* parameter = NULL;

    if (params) {
        parameter = get_vector_at(params, position);
    }

    bool reference = parameter && get_reference_type(parameter) != RefNone;

    ASTNode* argument = get_argument_expression(args, parameter, position);
    Type destination = parameter ? get_node_data_type(parameter) : get_node_data_type(argument);
    Operand expression = compile_converted_expression(compiler, argument, destination, reference);

    return expression;
}

static void store_argument(Compiler* compiler, Operand argument,int destination, bool reserved)
{
    if (!reserved && argument.temporary) {
        return;
    }

    if (!reserved) {
        allocate_registers(compiler, argument.slots);
    }

    emit_operand_mov(compiler, destination, argument);
}

static void compile_argument(Compiler* compiler, Vector* args, Vector* params, size_t position,
    int first_register, size_t destination_offset, bool reserved)
{
    int destination = compiler->register_count;

    if (reserved) {
        destination = first_register + destination_offset;
    }

    Operand argument = compile_argument_expression(compiler, args, params, position);

    store_argument(compiler, argument, destination, reserved);
}

static void compile_variadic_element(Compiler* compiler, ASTNode* argument, Type type, Operand buffer, size_t offset)
{
    Operand value = compile_converted_expression(compiler, argument, type, false);

    store_array_value(compiler, buffer, value, offset, type);
    release_operand(compiler, value);
}

static size_t get_argument_slot_count(Vector* layout)
{
    size_t count = 0;

    for (size_t i = 0; i < layout->count; i++) {
        count += get_node_slot_count(layout->data[i]);
    }

    return count;
}

static void compile_argument_list(Compiler* compiler, Vector* args, Vector* params, size_t count,
    int first_register, bool reserved)
{
    Vector* layout = params ? params : args;

    for (size_t i = 0, offset = 0; i < count; i++) {
        compile_argument(compiler, args, params, i, first_register, offset, reserved);
        offset += get_node_slot_count(layout->data[i]);
    }
}

static size_t get_variadic_argument_count(Vector* args, size_t fixed_count)
{
    if (args->count <= fixed_count) {
        return 0;
    }

    return args->count - fixed_count;
}

static Operand reserve_variadic_buffer(Compiler* compiler, Type type, size_t count, int slice_register)
{
    size_t stride = get_type_storage_size(type);

    if (count && stride > ((size_t)UINT16_MAX + 1) * sizeof(Value) / count) {
        check_register_count(compiler, (int)UINT16_MAX + 2);
    }

    size_t slots = (count * stride + sizeof(Value) - 1) / sizeof(Value);
    int storage = allocate_registers(compiler, slots);
    Operand buffer = make_operand(slice_register, false);

    emit_instruction(compiler, OpRefl, slice_register, storage, 0);

    Operand length = compile_usize_literal(compiler, count);

    emit_mov(compiler, slice_register + 1, length.reg);
    release_operand(compiler, length);

    return buffer;
}

static void compile_variadic_elements(Compiler* compiler, Vector* args, size_t fixed_count, Type type, Operand buffer)
{
    size_t count = get_variadic_argument_count(args, fixed_count);
    size_t stride = get_type_storage_size(type);

    for (size_t i = 0; i < count; i++) {
        compile_variadic_element(compiler, args->data[fixed_count + i], type, buffer, i * stride);
    }
}

static void compile_variadic_arguments(Compiler* compiler, Vector* args, Vector* params)
{
    size_t fixed_count = params->count - 1;
    ASTNode* parameter = params->data[fixed_count];
    Type type = parameter->parameter.type_id.array->element_type;
    size_t count = get_variadic_argument_count(args, fixed_count);
    size_t offset = parameter->parameter.position;
    int first_register = compiler->register_count;
    int slice_register = first_register + offset;

    allocate_registers(compiler, offset + 2);

    Operand buffer = reserve_variadic_buffer(compiler, type, count, slice_register);

    compile_argument_list(compiler, args, params, fixed_count, first_register, true);
    compile_variadic_elements(compiler, args, fixed_count, type, buffer);
}

static bool has_variadic_parameter(Vector* params)
{
    if (!params || !params->count) {
        return false;
    }

    ASTNode* last = params->data[params->count - 1];

    return last->parameter.variadic;
}

static bool compile_fixed_arguments(Compiler* compiler, Vector* args, Vector* params)
{
    Vector* layout = params ? params : args;
    size_t slot_count = get_argument_slot_count(layout);
    bool reserved = requires_reserved_arguments(args, params);
    int first_register = compiler->register_count;

    reserve_argument_registers(compiler, slot_count, reserved);
    compile_argument_list(compiler, args, params, layout->count, first_register, reserved);

    return reserved;
}

static bool compile_arguments(Compiler* compiler, Vector* args, Vector* params)
{
    if (has_variadic_parameter(params)) {
        compile_variadic_arguments(compiler, args, params);

        return true;
    }

    return compile_fixed_arguments(compiler, args, params);
}

static FunctionObject* create_defined_function_object(ASTNode* ast)
{
    ASTNode* body = ast->function_definition.body;
    FunctionObject* function = create_function_object();
    function->param_count = 0;

    for (size_t i = 0; i < count_vector(&ast->function_definition.params); i++) {
        function->param_count += get_node_slot_count(
            get_vector_at(&ast->function_definition.params, i)
        );
    }

    function->return_count = get_type_slot_count(ast->function_definition.return_type_id.kind);

    if (ast->function_definition.return_type_id.kind == TypeVoid) {
        function->return_count = 0;
    }

    if (is_slice_type(ast->function_definition.return_type_id.kind)) {
        function->param_count++;
        function->return_count = 0;
    }

    function->local_count = get_max_local_count(body->compound.scope);
    int base = get_call_area_count(function);

    if (base > (int)UINT16_MAX + 1) {
        function_register_index_overflow_error(ast->function_definition.id->chars);
    }

    function->max_stack_count = base + 2;

    return function;
}

static Type* create_builtin_argument_types(const Vector* args)
{
    Type* types = allocate_memory(args->count * sizeof(Type));

    for (size_t i = 0; i < args->count; i++) {
        types[i] = get_node_data_type(args->data[i]);
    }

    return types;
}

size_t find_builtin_call_position(const Compiler* compiler, const Builtin* builtin, const Vector* args)
{
    if (builtin->param_count >= 0) {
        return find_builtin_function_position(compiler, builtin);
    }

    Type* types = create_builtin_argument_types(args);
    size_t position = find_typed_builtin_call_position(compiler, builtin, types, args->count);

    free_memory(types);

    return position;
}

uint16_t add_builtin_call(Compiler* compiler, const Builtin* builtin, const Vector* args)
{
    if (builtin->param_count >= 0) {
        return add_builtin_function(compiler, builtin);
    }

    Type* types = create_builtin_argument_types(args);
    uint16_t position = add_typed_builtin_call(compiler, builtin, types, args->count);

    free_memory(types);

    return position;
}

uint16_t get_function_position(Compiler* compiler, ASTNode* ast)
{
    size_t function_count = count_vector(&compiler->function_references);

    for (size_t i = 0; i < function_count; i++) {
        if (ast == compiler->function_references.data[i]) {
            return make_function_position(i);
        }
    }

    if (is_function_specialization(ast)) {
        compile_function_definition(compiler, ast);

        return get_function_position(compiler, ast);
    }

    function_not_found_error(ast);

    return 0;
}

static bool returns_slice(Compiler* compiler, uint16_t position)
{
    ASTNode* definition = compiler->function_references.data[position];

    return definition && is_slice_type(definition->function_definition.return_type_id.kind);
}

static Operand compile_slice_call(Compiler* compiler, FunctionObject* function, uint16_t position,
    Vector* args, Vector* params, bool discard)
{
    int result = allocate_registers(compiler, 2);
    CallArea call = open_call_area(compiler);
    bool reserved = compile_arguments(compiler, args, params);

    if (reserved) {
        relocate_call_area(compiler, &call, function->param_count - 1);
    }

    int output = allocate_register(compiler);

    emit_instruction(compiler, OpRefl, output, result, 0);
    close_call_area(compiler, function, position, call, true);

    if (discard) {
        compiler->register_count = result;

        return no_operand();
    }

    compiler->register_count = result + 2;

    return make_slice_operand(result, true);
}

Operand compile_call(Compiler* compiler, FunctionObject* function, uint16_t function_position,
    Vector* args, Vector* params, bool discard)
{
    if (returns_slice(compiler, function_position)) {
        return compile_slice_call(compiler, function, function_position, args, params, discard);
    }

    size_t argument_count = count_vector(args);

    if (function->type == FuncBuiltin && !function->return_count && argument_count == 1) {
        int previous_register_count = compiler->register_count;
        Operand argument = compile_argument_expression(compiler, args, params, 0);

        emit_call_instruction(compiler, argument.reg, function_position);
        release_operand(compiler, argument);
        compiler->register_count = previous_register_count;

        return no_operand();
    }

    CallArea call = open_call_area(compiler);
    bool reserved = compile_arguments(compiler, args, params);

    if (reserved) {
        relocate_call_area(compiler, &call, function->param_count);
    }

    return close_call_area(compiler, function, function_position, call, discard);
}

void compile_call_with_argument(Compiler* compiler, FunctionObject* function,
    uint16_t function_position, Operand argument, bool discard)
{
    if (function->type == FuncBuiltin && !function->return_count) {
        emit_call_instruction(compiler, argument.reg, function_position);
        release_operand(compiler, argument);

        return;
    }

    CallArea call = open_call_area(compiler);
    int call_argument_register = compiler->register_count;

    allocate_registers(compiler, argument.slots);
    emit_operand_mov(compiler, call_argument_register, argument);
    close_call_area(compiler, function, function_position, call, discard);
    release_operand(compiler, argument);
}

static Operand compile_typed_builtin_call(Compiler* compiler, ASTNode* ast, FunctionObject* function,
    uint16_t position, bool discard)
{
    const Builtin* builtin = ast->builtin_call.builtin;
    Vector* args = &ast->builtin_call.args;

    if (!function->return_count && args->count == 1) {
        Type type = {.kind = builtin->params[0]};
        Operand argument = compile_converted_expression(compiler, args->data[0], type, false);

        emit_call_instruction(compiler, argument.reg, position);
        release_operand(compiler, argument);

        return no_operand();
    }

    CallArea call = open_call_area(compiler);

    for (size_t i = 0; i < args->count; i++) {
        size_t parameter = builtin->param_count == BUILTIN_TYPED_VARIADIC ? 0 : i;
        Type type = {.kind = builtin->params[parameter]};
        int destination = compiler->register_count;
        Operand argument = compile_converted_expression(compiler, args->data[i], type, false);

        store_argument(compiler, argument, destination, false);
    }

    return close_call_area(compiler, function, position, call, discard);
}

static Operand compile_length(Compiler* compiler, ASTNode* ast, bool discard)
{
    ASTNode* argument = ast->builtin_call.args.data[0];
    Type type = get_node_data_type(argument);

    if (is_slice_type(type.kind) || type.kind == TypeString) {
        return compile_slice_length(compiler, ast, discard);
    }

    return compile_array_length(compiler, ast, discard);
}

Operand compile_builtin_call(Compiler* compiler, ASTNode* ast, bool discard)
{
    const Builtin* builtin = ast->builtin_call.builtin;

    if (!builtin->entry) {
        return compile_length(compiler, ast, discard);
    }

    if (builtin->entry == builtin_copy_array) {
        return compile_array_copy(compiler, ast, discard);
    }

    size_t position = find_builtin_call_position(compiler, builtin, &ast->builtin_call.args);

    if (position == SIZE_MAX) {
        position = add_builtin_call(compiler, builtin, &ast->builtin_call.args);
    }

    FunctionObject* function = get_vector_at(&compiler->module->functions, position);

    if (builtin->param_count == BUILTIN_VARIADIC) {
        return compile_call(compiler, function, position, &ast->builtin_call.args, NULL, discard);
    }

    return compile_typed_builtin_call(compiler, ast, function, position, discard);
}

Operand compile_function_call(Compiler* compiler, ASTNode* ast, bool discard)
{
    uint16_t position = get_function_position(compiler, ast->function_call.symbol);
    FunctionObject* function = get_vector_at(&compiler->module->functions, position);
    Vector* params = &ast->function_call.symbol->function_definition.params;
    Operand result = compile_call(compiler, function, position, &ast->function_call.args, params, discard);

    Type type = get_node_data_type(ast);

    if (discard || get_reference_type(ast) == RefNone || is_slice_type(type.kind)) {
        return result;
    }

    return dereference_operand(compiler, result, get_type_slot_count(get_node_data_type(ast).kind));
}

void compile_function_definition(Compiler* compiler, ASTNode* ast)
{
    if (is_generic_function(ast)) {
        return;
    }

    if (ast->function_definition.is_test && !compiler->test_mode) {
        return;
    }

    if (!compiler->repl_mode && !ast->function_definition.call_count) {
        return;
    }

    int previous_frame_base_count = compiler->frame_base_count;
    int previous_local_position_offset = compiler->local_position_offset;
    int previous_register_count = compiler->register_count;
    size_t previous_peephole_barrier = compiler->peephole_barrier;
    FunctionObject* previous_function = compiler->function;
    LoopJumps* previous_loop_jumps = compiler->loop_jumps;
    FunctionObject* function = create_defined_function_object(ast);
    ASTNode* body = ast->function_definition.body;

    compiler->frame_base_count = get_call_area_count(function);
    compiler->local_position_offset = 0;
    compiler->register_count = compiler->frame_base_count;
    compiler->function = function;
    compiler->loop_jumps = NULL;
    compiler->peephole_barrier = 0;

    push_vector_item(&compiler->module->functions, function);
    push_vector_item(&compiler->function_references, ast);
    compile_block_level_statements(compiler, &body->compound.statements);

    size_t statement_count = count_vector(&body->compound.statements);
    ASTNode* last = NULL;

    if (statement_count > 0) {
        last = get_vector_at(&body->compound.statements, statement_count - 1);
    }

    if (!last || last->type != ASTReturn) {
        compiler->register_count = compiler->frame_base_count;
        emit_ret(compiler);
    }

    optimize_bytecode(compiler->module, &function->code, 0);

    compiler->function = previous_function;
    compiler->loop_jumps = previous_loop_jumps;
    compiler->frame_base_count = previous_frame_base_count;
    compiler->local_position_offset = previous_local_position_offset;
    compiler->register_count = previous_register_count;
    compiler->peephole_barrier = previous_peephole_barrier;
}

void compile_return_statement(Compiler* compiler, ASTNode* ast)
{
    if (is_none(ast->return_statement.expr)) {
        compiler->register_count = compiler->frame_base_count;

        return emit_ret(compiler);
    }

    bool reference = get_reference_type(ast->return_statement.expr) != RefNone;
    ASTNode* function = get_current_function_definition(compiler);
    Type return_type = function->function_definition.return_type_id;
    Operand value = compile_converted_expression(compiler, ast->return_statement.expr, return_type, reference);

    if (is_slice_type(return_type.kind)) {
        int output = compiler->function->param_count - 1;
        Opcode store = sizeof(Value) == 8 ? OpStmi64 : OpStmi32;

        emit_instruction(compiler, OpStr, value.reg, output, 0);
        emit_instruction(compiler, store, value.reg + 1, output, sizeof(Value));
        emit_ret(compiler);
    } else {
        emit_retv(compiler, value);
    }

    release_operand(compiler, value);
    compiler->register_count = compiler->frame_base_count;
}
