#include "compiler/compiler.h"
#include "builtin/builtin.h"
#include "common/errors.h"
#include "common/memory.h"
#include "common/util.h"
#include "common/vector.h"
#include "compiler/ast.h"
#include "compiler/ast_optimizer.h"
#include "compiler/bytecode_optimizer.h"
#include "compiler/diagnostics.h"
#include "compiler/parser.h"
#include "compiler/scope.h"
#include "compiler/token.h"
#include "runtime/argument_format.h"
#include "runtime/code_object.h"
#include "runtime/function_object.h"
#include "runtime/module_object.h"
#include "runtime/string_object.h"
#include "runtime/value.h"
#include "vm/opcode.h"
#include "vm/vm.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void (*CompileStatements)(Compiler* compiler, Vector* nodes);

typedef struct Operand
{
    int reg;
    bool temporary;
    size_t slots;
} Operand;

typedef struct CallArea
{
    int result_register;
    int frame_register;
} CallArea;

typedef struct PreviousInstruction
{
    CodeObject* code;
    size_t start;
    Opcode opcode;
    uint8_t a;
    uint8_t b;
    uint8_t c;
} PreviousInstruction;

typedef enum IntegerOperation
{
    IntAdd,
    IntSub,
    IntMul,
    IntDiv,
    IntRem,
    IntBand,
    IntBor,
    IntBxor,
    IntBnot,
    IntLsl,
    IntLsr,
    IntNeg
} IntegerOperation;

static Operand compile_expression(Compiler* compiler, ASTNode* ast, bool discard);
static Operand compile_statement(Compiler* compiler, ASTNode* ast, bool discard);
static Operand compile_reference_expression(Compiler* compiler, ASTNode* ast);
static Operand compile_conditional(Compiler* compiler, ASTNode* ast, bool reference);
static Operand compile_match(Compiler* compiler, ASTNode* ast, bool reference);
static Operand compile_converted_expression(Compiler* compiler, ASTNode* ast, DataType destination, bool reference);
static void compile_blocklevel_statements(Compiler* compiler, Vector* nodes);
static void compile_top_level_statements(Compiler* compiler, Vector* nodes);
static size_t get_node_slot_count(ASTNode* ast);
static Operand compile_power(Compiler* compiler, ASTNode* left_expression, ASTNode* right_expression);
static Operand compile_integer_cast(Compiler* compiler, Operand operand, DataType source, DataType destination);
static Operand compile_concatenation(Compiler* compiler, ASTNode* ast);
static void compile_power_assignment(Compiler* compiler, ASTNode* ast);
static Operand emit_unary_operand(Compiler* compiler, Opcode opcode, Operand operand);
static bool compile_discarded_expression(Compiler* compiler, ASTNode* ast);
static ASTNode* get_current_function_definition(Compiler* compiler);
static bool is_match_alternative(ASTNode* pattern);
static bool is_simple_match_value(ASTNode* ast);
static bool get_literal_match_value(ASTNode* ast, uint64_t* value);
static size_t emit_jump(Compiler* compiler, Opcode opcode, int condition_register);
static void patch_jump(Compiler* compiler, size_t position);
static Operand compile_constant(Compiler* compiler, ASTNode* ast);

static _Noreturn void branch_offset_overflow_error(void)
{
    fatal_error("Branch offset exceeds instruction range");
}

static _Noreturn void constant_index_overflow_error(void)
{
    fatal_error("Cannot create more than %u constants", UINT16_MAX + 1u);
}

static _Noreturn void function_index_overflow_error(void)
{
    fatal_error("Cannot create more than %u functions", UINT16_MAX + 1u);
}

static _Noreturn void function_not_found_error(ASTNode* ast)
{
    fatal_error("Could not find function %s", ast->function_definition.id->chars);
}

static _Noreturn void function_register_index_overflow_error(const char* name)
{
    fatal_error("Function %s requires more than %u virtual registers", name, UINT16_MAX + 1u);
}

static _Noreturn void global_index_overflow_error(void)
{
    fatal_error("Cannot create more than %u globals", UINT16_MAX + 1u);
}

static _Noreturn void top_level_register_index_overflow_error(void)
{
    fatal_error("Top-level code requires more than %u virtual registers", UINT16_MAX + 1u);
}

static _Noreturn void unresolved_repl_expression_type_error(void)
{
    fatal_error("Cannot display expression with an unresolved type");
}

static void discard_repl_statements(Compiler* compiler, size_t start)
{
    Vector* statements = &compiler->ast->compound.statements;

    while (statements->count > start) {
        ASTNode* ast = pop_vector_item(statements);

        free_ast_node(ast);
    }

    compiler->statement_index = start;
}

static bool contains_function(Vector* functions, ASTNode* ast)
{
    if (!ast) {
        return true;
    }

    size_t i = 0;

    while (i < functions->count && functions->data[i] != ast) {
        i++;
    }

    return i < functions->count;
}

static void discard_abandoned_functions(Compiler* compiler, Vector* functions)
{
    Vector* references = &compiler->function_references;
    size_t count = 0;

    while (count < references->count && contains_function(functions, references->data[count])) {
        count++;
    }

    while (compiler->module->functions.count > count) {
        FunctionObject* function = pop_vector_item(&compiler->module->functions);

        free_function_object(function);
    }

    references->count = count;
}

static void recover_compiler(Compiler* compiler)
{
    Vector functions;

    rebuild_analyzer(&compiler->analyzer, &functions);
    discard_abandoned_functions(compiler, &functions);
    free_vector(&functions);

    compiler->module->global_count = compiler->ast->compound.scope->local_count;
    compiler->statement_index = compiler->ast->compound.statements.count;
    compiler->function = compiler->module->functions.data[0];
    compiler->register_count = 0;
    compiler->frame_base_count = 0;
    compiler->peephole_barrier = 0;
    compiler->local_position_offset = -(int)compiler->module->global_count;
    compiler->function->max_stack_count = 0;
}

static void check_register_count(Compiler* compiler, int count)
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

static CodeObject* current_code_object(Compiler* compiler)
{
    return &compiler->function->code;
}

static bool is_compiling_top_level(Compiler* compiler)
{
    return compiler->function == get_vector_at(&compiler->module->functions, 0);
}

static int allocate_register(Compiler* compiler)
{
    check_register_count(compiler, compiler->register_count + 1);

    int reg = compiler->register_count++;
    int extent = compiler->register_count + 2;

    compiler->function->max_stack_count = MAX(compiler->function->max_stack_count, extent);

    return reg;
}

static int release_register(Compiler* compiler)
{
    return --compiler->register_count;
}

static int allocate_registers(Compiler* compiler, size_t count)
{
    int first = compiler->register_count;

    for (size_t i = 0; i < count; i++) {
        allocate_register(compiler);
    }

    return first;
}

static void emit_instruction_word(Compiler* compiler, Opcode opcode, int a, int b, int c)
{
    CodeObject* code = current_code_object(compiler);

    push_byte(code, opcode);
    push_byte(code, (uint8_t)a);
    push_byte(code, (uint8_t)b);
    push_byte(code, (uint8_t)c);
}

static int get_register_high_byte(Compiler* compiler, int operand, OpcodeRegisterFlag flag, OpcodeRegisterFlag field)
{
    if (!(flag & field)) {
        return 0;
    }

    check_register_count(compiler, operand + 1);

    return operand >> 8;
}

static void emit_instruction(Compiler* compiler, Opcode opcode, int a, int b, int c)
{
    OpcodeRegisterFlag flag = get_opcode_register_flag(opcode);
    
    int high_a = get_register_high_byte(compiler, a, flag, OpRegisterA);
    int high_b = get_register_high_byte(compiler, b, flag, OpRegisterB);
    int high_c = get_register_high_byte(compiler, c, flag, OpRegisterC);

    if (high_a || high_b || high_c) {
        emit_instruction_word(compiler, OpWide, high_a, high_b, high_c);
    }

    emit_instruction_word(compiler, opcode, a, b, c);
}

static void emit_hlt(Compiler* compiler)
{
    emit_instruction(compiler, OpHlt, 0, 0, 0);
}

static bool get_instruction_before(Compiler* compiler, size_t count, PreviousInstruction* instruction)
{
    CodeObject* code = current_code_object(compiler);

    if (count < INSTRUCTION_SIZE) {
        return false;
    }

    size_t start = count - INSTRUCTION_SIZE;
    if (start < compiler->peephole_barrier) {
        return false;
    }

    if (start >= INSTRUCTION_SIZE && code->data[start - INSTRUCTION_SIZE] == OpWide) {
        return false;
    }

    instruction->code = code;
    instruction->start = start;
    instruction->opcode = (Opcode)code->data[start];
    instruction->a = code->data[start + 1];
    instruction->b = code->data[start + 2];
    instruction->c = code->data[start + 3];

    return true;
}

static bool get_previous_instruction(Compiler* compiler, PreviousInstruction* instruction)
{
    CodeObject* code = current_code_object(compiler);
    size_t count = count_code_object(code);

    return get_instruction_before(compiler, count, instruction);
}

static void emit_mov(Compiler* compiler, int destination, int source)
{
    if (destination == source) {
        return;
    }

    emit_instruction(compiler, OpMov, destination, source, 0);
}

static void emit_operand_mov(Compiler* compiler, int destination, Operand operand)
{
    if (operand.slots != 2) {
        emit_mov(compiler, destination, operand.reg);
        return;
    }

    if (destination == operand.reg) {
        return;
    }

    emit_instruction(compiler, OpMov64, destination, operand.reg, 0);
}

static Operand make_operand(int reg, bool temporary)
{
    Operand operand = {reg, temporary, 1};

    return operand;
}

static Operand make_wide_operand(int reg, bool temporary)
{
    Operand operand = {reg, temporary, 2};

    return operand;
}

static Operand no_operand(void)
{
    return make_operand(-1, false);
}

static void release_operand(Compiler* compiler, Operand operand)
{
    for (size_t i = 0; operand.temporary && i < operand.slots; i++) {
        release_register(compiler);
    }
}

static Operand materialize_operand(Compiler* compiler, Operand operand)
{
    if (operand.temporary) {
        return operand;
    }

    int reg = allocate_registers(compiler, operand.slots);
    emit_operand_mov(compiler, reg, operand);

    return operand.slots == 2 ? make_wide_operand(reg, true) : make_operand(reg, true);
}

static int emit_ldc(Compiler* compiler, uint16_t imm)
{
    int reg = allocate_register(compiler);

    emit_instruction(compiler, OpLdc, reg, imm >> 8, imm);

    return reg;
}

static int emit_ldc_i64(Compiler* compiler, uint16_t position)
{
    int reg = allocate_registers(compiler, 2);

    emit_instruction(compiler, OpLdc64, reg, position >> 8, position);

    return reg;
}

static int emit_ldi(Compiler* compiler, int16_t imm)
{
    int reg = allocate_register(compiler);

    emit_instruction(compiler, OpLdi, reg, imm >> 8, imm);

    return reg;
}

static void emit_global_instruction(Compiler* compiler, Opcode opcode, int reg, size_t position)
{
    if (position > UINT16_MAX) {
        global_index_overflow_error();
    }

    emit_instruction(compiler, opcode, reg, position >> 8, position);
}

static int emit_ldg(Compiler* compiler, size_t position)
{
    int reg = allocate_register(compiler);

    emit_global_instruction(compiler, OpLdg, reg, position);

    return reg;
}

static void emit_call_instruction(Compiler* compiler, int frame_register, uint16_t function_position)
{
    check_register_count(compiler, frame_register + 1);

    emit_instruction(compiler, OpCall, frame_register, function_position >> 8, function_position);
}

static void emit_ret(Compiler* compiler)
{
    emit_instruction(compiler, OpRet, 0, 0, 0);
}

static void emit_retv(Compiler* compiler, Operand operand)
{
    Opcode opcode = operand.slots == 2 ? OpRet64 : OpRetv;

    emit_instruction(compiler, opcode, operand.reg, 0, 0);
}

static int get_call_area_count(const FunctionObject* function)
{
    return function->param_count;
}

static size_t make_constant(Compiler* compiler, Value value)
{
    size_t position = count_value_array(&compiler->module->constants);

    if (position > UINT16_MAX) {
        constant_index_overflow_error();
    }

    return push_value(&compiler->module->constants, value) - 1;
}

static size_t make_i64_constant(Compiler* compiler, uint64_t value)
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

static CallArea open_call_area(Compiler* compiler)
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

static void allocate_call_area_register(Compiler* compiler)
{
    emit_ldi(compiler, 0);
}

static Operand close_call_area(Compiler* compiler, FunctionObject* function,
    uint16_t function_position, CallArea call, bool discard)
{
    int end = call.frame_register + get_call_area_count(function);

    while (compiler->register_count < end) {
        allocate_call_area_register(compiler);
    }

    emit_call_instruction(compiler, call.frame_register, function_position);

    if (!discard && function->return_count) {
        Operand result = {call.frame_register - 2, true, function->return_count};

        emit_operand_mov(compiler, call.result_register, result);

        result.reg = call.result_register;
        compiler->register_count = call.result_register + function->return_count;

        return result;
    }

    compiler->register_count = call.result_register;

    return no_operand();
}

static int get_local_position(Compiler* compiler, ASTNode* ast)
{
    int position;

    if (is_parameter(ast)) {
        position = ast->parameter.position;
    } else {
        position = ast->variable_definition.register_position;
    }

    size_t slots = get_node_slot_count(ast);
    int extent = position + slots;

    check_register_count(compiler, extent);

    return position;
}

static Operand load_global_with_store_forwarding(Compiler* compiler, size_t position)
{
    PreviousInstruction previous;
    uint16_t stored_position;

    if (!get_previous_instruction(compiler, &previous)) {
        return make_operand(emit_ldg(compiler, position), true);
    }

    if (previous.opcode == OpStg) {
        stored_position = previous.b << 8 | previous.c;
    } else if (previous.opcode == OpStgi) {
        stored_position = previous.c;
    } else {
        return make_operand(emit_ldg(compiler, position), true);
    }

    if (stored_position != position || previous.a > compiler->register_count) {
        return make_operand(emit_ldg(compiler, position), true);
    }

    if (previous.a == compiler->register_count) {
        int reg = allocate_register(compiler);

        return make_operand(reg, true);
    }

    return make_operand(previous.a, false);
}

static Operand load_global_variable(Compiler* compiler, ASTNode* ast)
{
    size_t position = ast->variable_definition.position;
    bool wide = get_node_slot_count(ast) == 2;

    if (wide) {
        int reg = allocate_registers(compiler, 2);
        emit_global_instruction(compiler, OpLdg64, reg, position);

        return make_wide_operand(reg, true);
    }

    return load_global_with_store_forwarding(compiler, position);
}

static Operand load_local_variable(Compiler* compiler, ASTNode* ast)
{
    int position = get_local_position(compiler, ast);

    return get_node_slot_count(ast) == 2
        ? make_wide_operand(position, false)
        : make_operand(position, false);
}

static Operand load_variable(Compiler* compiler, ASTNode* ast)
{
    if (is_top_level_scope(ast->variable_definition.scope)) {
        return load_global_variable(compiler, ast);
    }

    return load_local_variable(compiler, ast);
}

static Operand reference_variable(Compiler* compiler, ASTNode* ast)
{
    if (get_reference_type(ast) != RefNone) {
        return load_variable(compiler, ast);
    }

    int reg = allocate_register(compiler);

    if (is_top_level_scope(get_scope(ast))) {
        size_t position = ast->variable_definition.position;
        emit_global_instruction(compiler, OpRefg, reg, position);

        return make_operand(reg, true);
    }

    emit_instruction(compiler, OpRefl, reg, get_local_position(compiler, ast), 0);

    return make_operand(reg, true);
}

static Operand dereference_operand(Compiler* compiler, Operand operand, size_t slots)
{
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

static void store_global_variable(Compiler* compiler, ASTNode* ast, Operand value)
{
    size_t position = ast->variable_definition.position;

    if (value.slots == 2) {
        emit_global_instruction(compiler, OpStg64, value.reg, position);

        return;
    }

    emit_global_instruction(compiler, OpStg, value.reg, position);
}

static bool retarget_temporary(Compiler* compiler, Operand value, int destination)
{
    PreviousInstruction previous;

    if (!value.temporary || destination > (int)UINT8_MAX) {
        return false;
    }

    if (!get_previous_instruction(compiler, &previous)
        || previous.a != value.reg) {
        return false;
    }

    if (!(get_opcode_flags(previous.opcode) & OpFlagWritesA)) {
        return false;
    }

    set_byte_at(previous.code, previous.start + OPERAND_A_OFFSET, destination);

    return true;
}

static void store_local_variable(Compiler* compiler, ASTNode* ast, Operand value)
{
    int position = get_local_position(compiler, ast);

    if (retarget_temporary(compiler, value, position)) {
        return;
    }

    emit_operand_mov(compiler, position, value);
}

static void store_variable(Compiler* compiler, ASTNode* ast, Operand value)
{
    if (is_top_level_scope(ast->variable_definition.scope)) {
        store_global_variable(compiler, ast, value);
    } else {
        store_local_variable(compiler, ast, value);
    }

    release_operand(compiler, value);
}

static Operand compile_wide_number(Compiler* compiler, uint64_t value)
{
    size_t position = make_i64_constant(compiler, value);
    int reg = emit_ldc_i64(compiler, position);

    return make_wide_operand(reg, true);
}

static bool is_narrow_integer_immediate(uint64_t value, DataType type)
{
    bool signed_type = is_signed_integer_type(type);

    if (!signed_type) {
        return value <= INT16_MAX;
    }

    int64_t signed_value = (int64_t)value;

    return signed_value >= INT16_MIN && signed_value <= INT16_MAX;
}

static Operand compile_number(Compiler* compiler, ASTNode* ast)
{
    uint64_t value = ast->integer_literal.value;
    DataType type = get_node_data_type(ast);

    if (type == TypeF64) {
        size_t position = make_f64_constant(compiler, (double)value);

        return make_wide_operand(emit_ldc_i64(compiler, position), true);
    }

    if (type == TypeF32) {
        size_t position = make_f32_constant(compiler, (float)value);

        return make_operand(emit_ldc(compiler, position), true);
    }

    if (get_node_slot_count(ast) == 2) {
        return compile_wide_number(compiler, value);
    }

    bool immediate = is_narrow_integer_immediate(value, type);

    if (!immediate) {
        Value constant = is_signed_integer_type(type)
            ? SIGNED_VALUE((int64_t)value)
            : UNSIGNED_VALUE(value);
        size_t position = make_constant(compiler, constant);

        return make_operand(emit_ldc(compiler, position), true);
    }

    return make_operand(emit_ldi(compiler, (int16_t)value), true);
}

static Operand compile_float_value(Compiler* compiler, double value, DataType type)
{
    if (type == TypeF64) {
        size_t position = make_f64_constant(compiler, value);
        int reg = emit_ldc_i64(compiler, position);

        return make_wide_operand(reg, true);
    }

    size_t position = make_f32_constant(compiler, (float)value);
    int reg = emit_ldc(compiler, position);

    return make_operand(reg, true);
}

static Operand compile_float(Compiler* compiler, ASTNode* ast)
{
    double value = ast->float_literal.value;
    DataType type = get_node_data_type(ast);

    return compile_float_value(compiler, value, type);
}

static Operand compile_string(Compiler* compiler, ASTNode* ast)
{
    StringObject* literal = ast->string_literal.value;
    StringObject* string = copy_string_object(literal->chars, literal->length);
    Value value = POINTER_VALUE(string);

    push_vector_item(&compiler->module->objects, string);

    size_t position = make_constant(compiler, value);
    int reg = emit_ldc(compiler, position);

    return make_operand(reg, true);
}

static Operand compile_character(Compiler* compiler, ASTNode* ast)
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

static Operand compile_boolean(Compiler* compiler, ASTNode* ast)
{
    return make_operand(emit_ldi(compiler, ast->boolean_literal.value), true);
}

static size_t get_node_slot_count(ASTNode* ast)
{
    if (get_reference_type(ast) != RefNone) {
        return 1;
    }

    return get_type_slot_count(get_node_data_type(ast));
}

static bool is_direct_variable(Compiler* compiler, ASTNode* ast)
{
    if (!is_variable(ast)) {
        return false;
    }

    ASTNode* symbol = ast->variable.symbol;

    return !is_top_level_scope(symbol->variable_definition.scope)
        || is_compiling_top_level(compiler);
}

static Opcode get_width_opcode(DataType type, Opcode opcode64, Opcode opcode32, Opcode opcode16, Opcode opcode8)
{
    size_t size = get_integer_type_size(type);
    
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

static Opcode get_division_opcode(DataType type)
{
    if (is_signed_integer_type(type)) {
        return get_width_opcode(type, OpIdivI64, OpIdivI32, OpIdivI16, OpIdivI8);
    }

    return get_width_opcode(type, OpIdivU64, OpIdivU32, OpIdivU16, OpIdivU8);
}

static Opcode get_remainder_opcode(DataType type)
{
    if (is_signed_integer_type(type)) {
        return get_width_opcode(type, OpRemI64, OpRemI32, OpRemI16, OpRemI8);
    }

    return get_width_opcode(type, OpRemU64, OpRemU32, OpRemU16, OpRemU8);
}

static Opcode get_right_shift_opcode(DataType type)
{
    if (is_signed_integer_type(type)) {
        return get_width_opcode(type, OpAsrI64, OpAsrI32, OpAsrI16, OpAsrI8);
    }

    return get_width_opcode(type, OpLsrI64, OpLsrI32, OpLsrI16, OpLsrI8);
}

static Opcode get_integer_opcode(DataType type, IntegerOperation operation)
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

static int select_binary_destination(Compiler* compiler, Operand left, Operand right, size_t slots)
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

static Operand emit_binary_operands(
    Compiler* compiler, IntegerOperation operation, DataType type, Operand left, Operand right)
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

static Operand emit_immediate_comparison(Compiler* compiler, Opcode opcode, Operand left, Operand right)
{
    PreviousInstruction load;

    if (!prepare_immediate_comparison(compiler, opcode, &left, &right, &load)) {
        return no_operand();
    }

    resize_code_object(load.code, load.start);
    release_operand(compiler, right);

    return emit_immediate_comparison_value(compiler, opcode, left, load.c);
}

static Operand emit_comparison(Compiler* compiler, Opcode opcode, Operand left, Operand right)
{
    Operand immediate = emit_immediate_comparison(compiler, opcode, left, right);

    if (immediate.reg >= 0) {
        return immediate;
    }

    if (left.temporary) {
        release_operand(compiler, right);
        emit_instruction(compiler, opcode, left.reg, left.reg, right.reg);

        return make_operand(left.reg, true);
    }

    int destination = right.temporary ? right.reg : allocate_register(compiler);
    emit_instruction(compiler, opcode, destination, left.reg, right.reg);

    return make_operand(destination, true);
}

static Operand emit_reversed_comparison(Compiler* compiler, Opcode opcode, Operand left, Operand right)
{
    if (left.temporary) {
        release_operand(compiler, right);
        emit_instruction(compiler, opcode, left.reg, right.reg, left.reg);

        return make_operand(left.reg, true);
    }

    int destination = right.temporary ? right.reg : allocate_register(compiler);
    emit_instruction(compiler, opcode, destination, right.reg, left.reg);

    return make_operand(destination, true);
}

static Opcode get_comparison_opcode(DataType type, bool or_equal)
{
    if (is_signed_integer_type(type)) {
        return or_equal ? OpLeI32 : OpLtI32;
    }

    return or_equal ? OpLeU32 : OpLtU32;
}

static Opcode get_float_opcode(DataType type, TokenType operator_type)
{
    switch (operator_type) {
        case TokenPlus:
            return type == TypeF64 ? OpAddF64 : OpAddF32;
        case TokenMinus:
            return type == TypeF64 ? OpSubF64 : OpSubF32;
        case TokenStar:
            return type == TypeF64 ? OpMulF64 : OpMulF32;
        case TokenSlash:
            return type == TypeF64 ? OpDivF64 : OpDivF32;
        case TokenPercent:
            return type == TypeF64 ? OpRemF64 : OpRemF32;
        default:
            return OpHlt;
    }
}

static Opcode get_float_operation_opcode(DataType type, IntegerOperation operation)
{
    switch (operation) {
        case IntAdd:
            return type == TypeF64 ? OpAddF64 : OpAddF32;
        case IntSub:
            return type == TypeF64 ? OpSubF64 : OpSubF32;
        case IntMul:
            return type == TypeF64 ? OpMulF64 : OpMulF32;
        case IntDiv:
            return type == TypeF64 ? OpDivF64 : OpDivF32;
        case IntRem:
            return type == TypeF64 ? OpRemF64 : OpRemF32;
        default:
            return OpHlt;
    }
}

static Operand emit_float_operands(Compiler* compiler, Opcode opcode,
    DataType type, Operand left, Operand right)
{
    size_t slots = get_type_slot_count(type);
    int destination = select_binary_destination(compiler, left, right, slots);

    emit_instruction(compiler, opcode, destination, left.reg, right.reg);

    return type == TypeF64
        ? make_wide_operand(destination, true)
        : make_operand(destination, true);
}

static Operand compile_float_binary(Compiler* compiler, ASTNode* ast)
{
    DataType type = get_node_data_type(ast);
    Operand left = compile_converted_expression(compiler, ast->binary.left_expr, type, false);

    if (!left.temporary && !is_direct_variable(compiler, ast->binary.right_expr)) {
        left = materialize_operand(compiler, left);
    }

    Operand right = compile_converted_expression(compiler, ast->binary.right_expr, type, false);
    Opcode opcode = get_float_opcode(type, ast->binary.operator.type);

    return emit_float_operands(compiler, opcode, type, left, right);
}

static Operand emit_ordered_comparison(Compiler* compiler, DataType type,
    Operand left, Operand right, bool or_equal, bool reversed)
{
    Opcode opcode = get_comparison_opcode(type, or_equal);

    if (reversed) {
        return emit_reversed_comparison(compiler, opcode, left, right);
    }

    return emit_comparison(compiler, opcode, left, right);
}

static Opcode get_float32_three_way_opcode(TokenType operator_type)
{
    if (operator_type == TokenLess || operator_type == TokenLessEqual) {
        return OpCmpgF32;
    }

    return OpCmplF32;
}

static Opcode get_float64_three_way_opcode(TokenType operator_type)
{
    if (operator_type == TokenLess || operator_type == TokenLessEqual) {
        return OpCmpgF64;
    }

    return OpCmplF64;
}

static Opcode get_three_way_opcode(DataType type, TokenType operator_type)
{
    size_t size = get_integer_type_size(type);

    if (size == 8) {
        bool signed_type = is_signed_integer_type(type);

        return signed_type ? OpCmpI64 : OpCmpU64;
    }

    switch (type) {
        case TypeF32:
            return get_float32_three_way_opcode(operator_type);
        case TypeF64:
            return get_float64_three_way_opcode(operator_type);
        default:
            break;
    }

    bool signed_type = is_signed_integer_type(type);

    return signed_type ? OpCmpI32 : OpCmpU32;
}

static Operand emit_three_way_comparison(Compiler* compiler, Opcode opcode, Operand left, Operand right)
{
    release_operand(compiler, right);
    release_operand(compiler, left);

    int destination = allocate_register(compiler);

    emit_instruction(compiler, opcode, destination, left.reg, right.reg);

    return make_operand(destination, true);
}

static Operand emit_integer_comparison(Compiler* compiler, TokenType operator_type,
    DataType type, Operand left, Operand right)
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

static Operand emit_numeric_comparison(Compiler* compiler, TokenType operator_type,
    DataType type, Operand left, Operand right)
{
    bool floating = is_float_type(type);
    bool wide = get_integer_type_size(type) == 8;

    if (!floating && !wide && operator_type != TokenSpaceship) {
        return emit_integer_comparison(compiler, operator_type, type, left, right);
    }

    Opcode opcode = get_three_way_opcode(type, operator_type);
    Operand ordering = emit_three_way_comparison(compiler, opcode, left, right);

    if (operator_type == TokenSpaceship) {
        return ordering;
    }

    int zero_register = allocate_register(compiler);
    Operand zero = make_operand(zero_register, true);

    emit_instruction(compiler, OpLdi, zero_register, 0, 0);

    return emit_numeric_comparison(compiler, operator_type, TypeI32, ordering, zero);
}

static Operand normalize_integer_operand(Compiler* compiler, Operand operand, DataType type)
{
    size_t size = get_integer_type_size(type);

    if (size != 1 && size != 2) {
        return operand;
    }

    operand = materialize_operand(compiler, operand);

    Opcode opcode = OpZextI32;
    int width = size * 8;

    if (is_signed_integer_type(type)) {
        opcode = OpSextI32;
    }

    emit_instruction(compiler, opcode, operand.reg, operand.reg, width);

    return operand;
}

static Operand compile_comparison_operand(Compiler* compiler, ASTNode* ast, DataType type)
{
    Operand value = compile_converted_expression(compiler, ast, type, false);

    return normalize_integer_operand(compiler, value, type);
}

static Operand compile_numeric_comparison(Compiler* compiler, ASTNode* ast)
{
    DataType type = ast->binary.operand_type;
    Operand left = compile_comparison_operand(compiler, ast->binary.left_expr, type);
    bool direct = is_direct_variable(compiler, ast->binary.right_expr);

    if (!left.temporary && !direct) {
        left = materialize_operand(compiler, left);
    }

    Operand right = compile_comparison_operand(compiler, ast->binary.right_expr, type);

    return emit_numeric_comparison(compiler, ast->binary.operator.type, type, left, right);
}

static Operand emit_binary_operator(Compiler* compiler, TokenType operator_type,
    DataType type, Operand left, Operand right)
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
        case TokenAmpersand:
            return emit_binary_operands(compiler, IntBand, type, left, right);
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

static Operand compile_logical_binary(Compiler* compiler, ASTNode* ast)
{
    Operand result = compile_expression(compiler, ast->binary.left_expr, false);
    Opcode opcode = OpBz;

    result = materialize_operand(compiler, result);

    if (ast->binary.operator.type == TokenKeywordOr) {
        opcode = OpBnz;
    }

    size_t end_jump = emit_jump(compiler, opcode, result.reg);
    Operand right = compile_expression(compiler, ast->binary.right_expr, false);

    emit_operand_mov(compiler, result.reg, right);
    release_operand(compiler, right);
    patch_jump(compiler, end_jump);

    return result;
}

static Operand compile_binary(Compiler* compiler, ASTNode* ast)
{
    DataType result_type = get_node_data_type(ast);

    if (result_type == TypeString) {
        return compile_concatenation(compiler, ast);
    }

    if (ast->binary.operator.type == TokenPower) {
        return compile_power(compiler, ast->binary.left_expr, ast->binary.right_expr);
    }

    TokenType operator_type = ast->binary.operator.type;

    bool logical = is_logical_token(operator_type);
    if (logical) {
        return compile_logical_binary(compiler, ast);
    }

    bool comparison = is_comparison_token(operator_type) || is_equality_token(operator_type);
    if (comparison) {
        return compile_numeric_comparison(compiler, ast);
    }

    if (is_float_type(get_node_data_type(ast))) {
        return compile_float_binary(compiler, ast);
    }

    DataType type = ast->binary.operand_type;
    Operand left = compile_converted_expression(compiler, ast->binary.left_expr, type, false);

    if (!left.temporary && !is_direct_variable(compiler, ast->binary.right_expr)) {
        left = materialize_operand(compiler, left);
    }

    Operand right = compile_converted_expression(compiler, ast->binary.right_expr, type, false);

    return emit_binary_operator(compiler, ast->binary.operator.type, type, left, right);
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

static Opcode get_negation_opcode(DataType type)
{
    if (type == TypeF32) {
        return OpNegF32;
    }

    if (type == TypeF64) {
        return OpNegF64;
    }

    return get_integer_opcode(type, IntNeg);
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

static void evaluate_constant_expression(Compiler* compiler, ASTNode* ast)
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
    DataType type = get_node_data_type(ast);
    Value value = compiler->module->constants.data[position];
    uint64_t integer = AS_UNSIGNED(value);
    bool scalar = is_integer_type(type) || type == TypeBool || type == TypeChar;

    if (type == TypeBool) {
        integer = AS_BOOL(value);
    }

    if (scalar && is_narrow_integer_immediate(integer, type)) {
        return emit_ldi(compiler, (int16_t)integer);
    }

    return emit_ldc(compiler, position);
}

static Operand compile_constant(Compiler* compiler, ASTNode* ast)
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

static Operand compile_prefix(Compiler* compiler, ASTNode* ast)
{
    if (ast->prefix.operator.type == TokenKeywordConst) {
        return compile_constant(compiler, ast);
    }

    if (get_reference_type(ast) != RefNone) {
        return dereference_operand(
            compiler, compile_reference_expression(compiler, ast), get_type_slot_count(get_node_data_type(ast)));
    }

    Operand operand = compile_expression(compiler, ast->prefix.expr, false);

    switch (ast->prefix.operator.type) {
        case TokenKeywordNot:
            return emit_unary_operand(compiler, OpNot, operand);
        case TokenTilde:
            return emit_unary_operand(compiler, get_integer_opcode(get_node_data_type(ast), IntBnot), operand);
        case TokenMinus: {
            DataType type = get_node_data_type(ast);
            Opcode opcode = get_negation_opcode(type);

            return emit_unary_operand(compiler, opcode, operand);
        }
        default:
            return no_operand();
    }
}

static Operand compile_variable(Compiler* compiler, ASTNode* ast)
{
    ASTNode* symbol = ast->variable.symbol;

    if (is_variable_definition(symbol) && symbol->variable_definition.constant) {
        DataType type = get_node_data_type(symbol);

        return compile_converted_expression(compiler, symbol->variable_definition.expr, type, false);
    }

    Operand value = load_variable(compiler, ast->variable.symbol);

    if (get_reference_type(ast) == RefNone) {
        return value;
    }

    return dereference_operand(compiler, value, get_type_slot_count(get_node_data_type(ast)));
}

static void store_assignment_value(Compiler* compiler, ASTNode* symbol, Operand value)
{
    if (get_reference_type(symbol) == RefNone) {
        store_variable(compiler, symbol, value);

        return;
    }

    Operand reference = load_variable(compiler, symbol);
    Opcode opcode = value.slots == 2 ? OpStr64 : OpStr;

    emit_instruction(compiler, opcode, value.reg, reference.reg, 0);
    release_operand(compiler, value);
    release_operand(compiler, reference);
}

static void compile_compound_assignment(
    Compiler* compiler, ASTNode* ast, IntegerOperation operation)
{
    DataType type = get_node_data_type(ast->assignment.symbol);

    Operand left = load_variable(compiler, ast->assignment.symbol);

    if (get_reference_type(ast->assignment.symbol) != RefNone) {
        left = dereference_operand(
            compiler, left, get_type_slot_count(get_node_data_type(ast->assignment.symbol)));
    }

    if (!left.temporary && !is_direct_variable(compiler, ast->assignment.expr)) {
        left = materialize_operand(compiler, left);
    }

    Operand right = compile_converted_expression(compiler, ast->assignment.expr, type, false);
    Operand result;

    if (is_float_type(type)) {
        Opcode opcode = get_float_operation_opcode(type, operation);
        result = emit_float_operands(compiler, opcode, type, left, right);
    } else {
        result = emit_binary_operands(compiler, operation, type, left, right);
    }

    store_assignment_value(compiler, ast->assignment.symbol, result);
}

static void compile_simple_assignment(Compiler* compiler, ASTNode* ast)
{
    if (get_reference_type(ast->assignment.symbol) != RefNone) {
        Operand reference = compile_reference_expression(compiler, ast->assignment.expr);
        store_variable(compiler, ast->assignment.symbol, reference);

        return;
    }

    Operand value = compile_converted_expression(
        compiler, ast->assignment.expr, get_node_data_type(ast->assignment.symbol), false);

    store_assignment_value(compiler, ast->assignment.symbol, value);
}

static void compile_assignment(Compiler* compiler, ASTNode* ast)
{
    switch (ast->assignment.operator.type) {
        case TokenPlusEqual:
            return compile_compound_assignment(compiler, ast, IntAdd);
        case TokenMinusEqual:
            return compile_compound_assignment(compiler, ast, IntSub);
        case TokenStarEqual:
            return compile_compound_assignment(compiler, ast, IntMul);
        case TokenSlashEqual:
            return compile_compound_assignment(compiler, ast, IntDiv);
        case TokenPercentEqual:
            return compile_compound_assignment(compiler, ast, IntRem);
        case TokenAndEqual:
            return compile_compound_assignment(compiler, ast, IntBand);
        case TokenOrEqual:
            return compile_compound_assignment(compiler, ast, IntBor);
        case TokenCircumflexEqual:
            return compile_compound_assignment(compiler, ast, IntBxor);
        case TokenLshiftEqual:
            return compile_compound_assignment(compiler, ast, IntLsl);
        case TokenRshiftEqual:
            return compile_compound_assignment(compiler, ast, IntLsr);
        case TokenPowerEqual:
            return compile_power_assignment(compiler, ast);
        case TokenEqual:
            return compile_simple_assignment(compiler, ast);
        default:
            return;
    }
}

static Operand compile_reference_aware_expression(Compiler* compiler, ASTNode* ast, bool reference)
{
    if (reference) {
        return compile_reference_expression(compiler, ast);
    }

    return compile_expression(compiler, ast, false);
}

static Operand normalize_unsigned_operand(Compiler* compiler, Operand operand, DataType source)
{
#if UINTPTR_MAX == UINT64_MAX
    if (source == TypeU32) {
        operand = materialize_operand(compiler, operand);

        emit_instruction(compiler, OpLsriI32, operand.reg, operand.reg, 0);

        return operand;
    }
#endif

    if (source != TypeU8 && source != TypeU16) {
        return operand;
    }

    return normalize_integer_operand(compiler, operand, source);
}

static Operand extend_integer_operand(Compiler* compiler, Operand operand, DataType source)
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
    if (is_signed_integer_type(source)) {
        emit_instruction(compiler, OpAsriI32, high_register, destination_register, 31);

        return make_wide_operand(destination_register, true);
    }

    emit_instruction(compiler, OpLdi, high_register, 0, 0);

    return make_wide_operand(destination_register, true);
#endif
}

static Operand widen_integer_operand(Compiler* compiler, Operand operand,
    DataType source, DataType destination)
{
    if (!can_implicitly_widen_integer(source, destination)
        || get_type_slot_count(destination) == operand.slots) {
        return operand;
    }

    return compile_integer_cast(compiler, operand, source, destination);
}

static Operand widen_float_operand(Compiler* compiler, Operand operand,
    DataType source, DataType destination)
{
    if (source != TypeF32 || destination != TypeF64) {
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
    DataType source, DataType destination)
{
    if (!can_implicitly_convert_integer_to_float(source, destination)) {
        return operand;
    }

    bool wide = destination == TypeF64;
    int destination_register = operand.reg;

    if (!operand.temporary) {
        destination_register = allocate_registers(compiler, wide ? 2 : 1);
    } else if (wide) {
        allocate_register(compiler);
    }

    bool signed_integer = is_signed_integer_type(source);
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

static uint64_t get_widened_integer_literal_value(ASTNode* ast, DataType source)
{
    uint64_t value = ast->integer_literal.value;

    switch (source) {
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

static Operand compile_widened_integer_literal(Compiler* compiler, ASTNode* ast, DataType destination)
{
    DataType source = get_node_data_type(ast);
    ASTNode literal = *ast;

    literal.integer_literal.value = get_widened_integer_literal_value(ast, source);
    literal.integer_literal.type_id = destination;

    return compile_number(compiler, &literal);
}

static Operand compile_integer_literal_as_float(Compiler* compiler, ASTNode* ast, DataType destination)
{
    DataType source = get_node_data_type(ast);
    uint64_t integer = get_widened_integer_literal_value(ast, source);
    double number = integer;

    if (is_signed_integer_type(source)) {
        number = (int64_t)integer;
    }

    if (source == TypeF32) {
        number = (float)number;
    }

    return compile_float_value(compiler, number, destination);
}

static Operand compile_converted_expression(Compiler* compiler, ASTNode* ast,
    DataType destination, bool reference)
{
    DataType source = get_node_data_type(ast);
    bool integer_to_float = can_implicitly_convert_integer_to_float(source, destination);
    bool float_widening = can_implicitly_widen_float(source, destination);

    if (!reference && ast->type == ASTInteger && (integer_to_float || float_widening)) {
        return compile_integer_literal_as_float(compiler, ast, destination);
    }

    if (!reference && ast->type == ASTInteger && can_implicitly_widen_integer(source, destination)) {
        return compile_widened_integer_literal(compiler, ast, destination);
    }

    if (!reference && ast->type == ASTFloat && source == TypeF32 && destination == TypeF64) {
        double value = (float)ast->float_literal.value;

        return compile_float_value(compiler, value, destination);
    }

    Operand operand = compile_reference_aware_expression(compiler, ast, reference);

    if (reference) {
        return operand;
    }

    if (source != destination && can_implicitly_widen_type(source, destination)) {
        operand = normalize_unsigned_operand(compiler, operand, source);
    }

    operand = widen_integer_operand(compiler, operand, source, destination);
    operand = convert_integer_to_float_operand(compiler, operand, source, destination);

    return widen_float_operand(compiler, operand, source, destination);
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

static Operand compile_integer_cast(Compiler* compiler, Operand operand, DataType source, DataType destination)
{
    operand = normalize_unsigned_operand(compiler, operand, source);

    if (get_type_slot_count(destination) == 2 && operand.slots == 1) {
        return extend_integer_operand(compiler, operand, source);
    }

    if (get_type_slot_count(destination) == 2) {
        return operand;
    }

    operand = shrink_integer_operand(compiler, operand);
    operand = materialize_operand(compiler, operand);

    size_t size = get_integer_type_size(destination);

    if (size == 8) {
        return operand;
    }

    if (size < 4) {
        return normalize_integer_operand(compiler, operand, destination);
    }

    Opcode opcode = OpLsriI32;

    if (is_signed_integer_type(destination)) {
        opcode = OpAsriI32;
    }

    emit_instruction(compiler, opcode, operand.reg, operand.reg, 0);

    return operand;
}

static Opcode get_float_to_integer_opcode(DataType source, DataType destination)
{
    static const Opcode opcodes[2][4] = {
        {OpF32ToI32, OpF32ToU32, OpF32ToI64, OpF32ToU64},
        {OpF64ToI32, OpF64ToU32, OpF64ToI64, OpF64ToU64}
    };
    int index = 0;

    if (!is_signed_integer_type(destination)) {
        index++;
    }

    if (get_integer_type_size(destination) == 8) {
        index += 2;
    }

    return opcodes[source == TypeF64][index];
}

static Opcode get_integer_to_float_opcode(DataType source, DataType destination)
{
    static const Opcode signed_opcodes[2][2] = {
        {OpI32ToF32, OpI32ToF64},
        {OpI64ToF32, OpI64ToF64}
    };
    static const Opcode unsigned_opcodes[2][2] = {
        {OpU32ToF32, OpU32ToF64},
        {OpU64ToF32, OpU64ToF64}
    };
    bool wide_source = get_integer_type_size(source) == 8;
    bool wide_destination = destination == TypeF64;

    if (is_signed_integer_type(source)) {
        return signed_opcodes[wide_source][wide_destination];
    }

    return unsigned_opcodes[wide_source][wide_destination];
}

static Opcode get_numeric_conversion_opcode(DataType source, DataType destination)
{
    if (is_integer_type(destination)) {
        return get_float_to_integer_opcode(source, destination);
    }

    if (is_integer_type(source)) {
        return get_integer_to_float_opcode(source, destination);
    }

    return OpF64ToF32;
}

static Operand emit_conversion_operand(Compiler* compiler, Operand operand, DataType destination, Opcode opcode)
{
    size_t slots = get_type_slot_count(destination);

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
    DataType source = get_node_data_type(expression);
    DataType destination = ast->conversion.type_id;

    Operand operand = compile_expression(compiler, expression, false);

    if (destination == TypeChar) {
        return normalize_unsigned_operand(compiler, operand, source);
    }

    return compile_integer_cast(compiler, operand, TypeU32, destination);
}

static Operand compile_boolean_to_integer(Compiler* compiler, ASTNode* expression, DataType destination)
{
    Operand operand = compile_expression(compiler, expression, false);

    operand = materialize_operand(compiler, operand);

    emit_instruction(compiler, OpBandiI32, operand.reg, operand.reg, 1);

    if (get_type_slot_count(destination) == 2) {
        return extend_integer_operand(compiler, operand, TypeU32);
    }

    return operand;
}

static Operand compile_explicit_conversion(Compiler* compiler, ASTNode* ast)
{
    ASTNode* expression = ast->conversion.expr;
    DataType source = get_node_data_type(expression);
    DataType destination = ast->conversion.type_id;

    if (source == destination || can_implicitly_widen_type(source, destination)) {
        return compile_converted_expression(compiler, expression, destination, false);
    }

    if (source == TypeBool) {
        return compile_boolean_to_integer(compiler, expression, destination);
    }

    if (source == TypeChar || destination == TypeChar) {
        return compile_character_conversion(compiler, ast);
    }

    Operand operand = compile_expression(compiler, expression, false);

    if (is_integer_type(source) && is_integer_type(destination)) {
        return compile_integer_cast(compiler, operand, source, destination);
    }

    operand = normalize_unsigned_operand(compiler, operand, source);

    Opcode opcode = get_numeric_conversion_opcode(source, destination);

    return emit_conversion_operand(compiler, operand, destination, opcode);
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

static Operand compile_argument_expression(Compiler* compiler, Vector* args, Vector* params,
    size_t position)
{
    ASTNode* parameter = NULL;

    if (params) {
        parameter = get_vector_at(params, position);
    }

    bool reference = parameter && get_reference_type(parameter) != RefNone;

    ASTNode* argument = get_argument_expression(args, parameter, position);
    DataType destination = parameter ? get_node_data_type(parameter) : get_node_data_type(argument);
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

static bool compile_arguments(Compiler* compiler, Vector* args, Vector* params)
{
    size_t count = args->count;

    if (params) {
        count = params->count;
    }

    size_t slot_count = 0;
    bool reserved = requires_reserved_arguments(args, params);
    int first_register = compiler->register_count;

    for (size_t i = 0; i < count; i++) {
        ASTNode* node = params ? get_vector_at(params, i) : get_vector_at(args, i);
        slot_count += get_node_slot_count(node);
    }

    reserve_argument_registers(compiler, slot_count, reserved);

    for (size_t i = 0, offset = 0; i < count; i++) {
        ASTNode* layout = params ? get_vector_at(params, i) : get_vector_at(args, i);

        compile_argument(compiler, args, params, i, first_register, offset, reserved);
        offset += get_node_slot_count(layout);
    }

    return reserved;
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

static FunctionObject* create_defined_function_object(ASTNode* ast)
{
    ASTNode* body = ast->function_definition.body;
    FunctionObject* function = create_function_object();
    function->param_count = 0;

    for (size_t i = 0; i < count_vector(&ast->function_definition.params); i++) {
        function->param_count += get_node_slot_count(
            get_vector_at(&ast->function_definition.params, i));
    }
    function->return_count = get_type_slot_count(ast->function_definition.return_type_id);

    if (ast->function_definition.return_type_id == TypeVoid) {
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

static uint16_t get_builtin_function_position(Compiler* compiler, const Builtin* builtin)
{
    size_t function_count = count_vector(&compiler->module->functions);

    for (size_t i = 0; i < function_count; i++) {
        FunctionObject* function = get_vector_at(&compiler->module->functions, i);

        if (function->type == FuncBuiltin && function->entry == builtin->entry) {
            return make_function_position(i);
        }
    }

    uint16_t position = make_function_position(function_count);
    FunctionObject* function = create_builtin_function_object(builtin);

    push_vector_item(&compiler->module->functions, function);
    push_vector_item(&compiler->function_references, NULL);

    return position;
}

static DataType get_builtin_argument_type(const Builtin* builtin, const ASTNode* argument)
{
    if (builtin->param_count == BUILTIN_TYPED_VARIADIC) {
        return builtin->params[0];
    }

    return get_node_data_type(argument);
}

static bool matches_builtin_arguments(const FunctionObject* function, const Builtin* builtin, const Vector* args)
{
    if (function->type != FuncBuiltin || function->entry != builtin->entry
        || function->argument_count != args->count) {
        return false;
    }

    if (builtin->param_count == BUILTIN_TYPED_VARIADIC) {
        return true;
    }

    bool matches = true;

    for (size_t i = 0; i < args->count && matches; i++) {
        ASTNode* argument = args->data[i];
        DataType type = get_node_data_type(argument);
        ArgumentFormat format = get_argument_format(type);
        matches = format.formatter == function->argument_formats[i].formatter;
    }

    return matches;
}

static void set_builtin_argument_format(FunctionObject* function, DataType type, size_t index)
{
    if (!function->argument_formats) {
        return;
    }

    ArgumentFormat format = get_argument_format(type);
    format.slot = function->param_count;
    function->argument_formats[index] = format;
}

static FunctionObject* create_variadic_builtin_function(const Builtin* builtin, const Vector* args)
{
    FunctionObject* function = create_builtin_function_object(builtin);
    size_t count = args->count;

    if (count > SIZE_MAX / sizeof(ArgumentFormat)) {
        out_of_memory_error();
    }

    if (builtin->param_count == BUILTIN_VARIADIC && count > 0) {
        function->argument_formats = allocate_memory(count * sizeof(ArgumentFormat));
    }

    function->argument_count = count;

    for (size_t i = 0; i < count; i++) {
        ASTNode* argument = args->data[i];
        DataType type = get_builtin_argument_type(builtin, argument);

        set_builtin_argument_format(function, type, i);

        function->param_count += get_type_slot_count(type);
    }

    function->max_stack_count = 2 + get_call_area_count(function);

    return function;
}

static uint16_t get_builtin_call_position(Compiler* compiler, const Builtin* builtin, const Vector* args)
{
    if (builtin->param_count >= 0) {
        return get_builtin_function_position(compiler, builtin);
    }

    size_t count = compiler->module->functions.count;

    for (size_t i = 0; i < count; i++) {
        FunctionObject* function = compiler->module->functions.data[i];
        bool matches = matches_builtin_arguments(function, builtin, args);

        if (matches) {
            return make_function_position(i);
        }
    }

    uint16_t position = make_function_position(count);
    FunctionObject* function = create_variadic_builtin_function(builtin, args);

    push_vector_item(&compiler->module->functions, function);
    push_vector_item(&compiler->function_references, NULL);

    return position;
}

static uint16_t get_function_position(Compiler* compiler, ASTNode* ast)
{
    size_t function_count = count_vector(&compiler->function_references);

    for (size_t i = 0; i < function_count; i++) {
        if (ast == compiler->function_references.data[i]) {
            return make_function_position(i);
        }
    }

    function_not_found_error(ast);

    return 0;
}

static Operand compile_call(Compiler* compiler, FunctionObject* function, uint16_t function_position,
    Vector* args, Vector* params, bool discard)
{
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

static Operand compile_power(Compiler* compiler, ASTNode* left_expression, ASTNode* right_expression)
{
    void* items[] = {left_expression, right_expression};
    Vector args = {.data = items, .capacity = 2, .count = 2};
    DataType argument_types[] = {TypeI32, TypeI32};
    const Builtin* builtin = resolve_builtin("pow", argument_types, 2);
    uint16_t position = get_builtin_function_position(compiler, builtin);
    FunctionObject* function = get_vector_at(&compiler->module->functions, position);

    return compile_call(compiler, function, position, &args, NULL, false);
}

static Operand compile_concatenation(Compiler* compiler, ASTNode* ast)
{
    ASTNode* left = ast->binary.left_expr;
    ASTNode* right = ast->binary.right_expr;
    void* items[] = {left, right};
    Vector args = {.data = items, .capacity = 2, .count = 2};
    const Builtin* builtin = resolve_builtin("string", NULL, 0);
    uint16_t position = get_builtin_call_position(compiler, builtin, &args);
    FunctionObject* function = get_vector_at(&compiler->module->functions, position);

    return compile_call(compiler, function, position, &args, NULL, false);
}

static void compile_power_assignment(Compiler* compiler, ASTNode* ast)
{
    ASTNode left = {.type = ASTVariable};
    left.variable.symbol = ast->assignment.symbol;

    Operand result = compile_power(compiler, &left, ast->assignment.expr);
    store_assignment_value(compiler, ast->assignment.symbol, result);
}

static void compile_call_with_argument(Compiler* compiler, FunctionObject* function,
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

static Operand compile_typed_variadic_call(Compiler* compiler, ASTNode* ast, FunctionObject* function,
    uint16_t position, bool discard)
{
    ASTNode parameter = {.type = ASTParameter};
    parameter.parameter.type_id = ast->builtin_call.builtin->params[0];
    parameter.parameter.reference_type = RefNone;
    Vector params;

    init_vector(&params);

    for (size_t i = 0; i < ast->builtin_call.args.count; i++) {
        push_vector_item(&params, &parameter);
    }

    Operand result = compile_call(compiler, function, position, &ast->builtin_call.args, &params, discard);

    free_vector(&params);

    return result;
}

static Operand compile_builtin_call(Compiler* compiler, ASTNode* ast, bool discard)
{
    const Builtin* builtin = ast->builtin_call.builtin;
    uint16_t position = get_builtin_call_position(compiler, builtin, &ast->builtin_call.args);
    FunctionObject* function = get_vector_at(&compiler->module->functions, position);

    if (builtin->param_count == BUILTIN_VARIADIC) {
        return compile_call(compiler, function, position, &ast->builtin_call.args, NULL, discard);
    }

    if (builtin->param_count == BUILTIN_TYPED_VARIADIC) {
        return compile_typed_variadic_call(compiler, ast, function, position, discard);
    }

    ASTNode parameters[BUILTIN_PARAMS_MAX] = {0};
    void* items[BUILTIN_PARAMS_MAX];
    size_t count = builtin->param_count;
    Vector params = {.data = items, .capacity = count, .count = count};

    for (size_t i = 0; i < count; i++) {
        parameters[i].type = ASTParameter;
        parameters[i].parameter.type_id = builtin->params[i];
        parameters[i].parameter.reference_type = RefNone;
        items[i] = &parameters[i];
    }

    return compile_call(compiler, function, position, &ast->builtin_call.args, &params, discard);
}

static Operand compile_function_call(Compiler* compiler, ASTNode* ast, bool discard)
{
    uint16_t position = get_function_position(compiler, ast->function_call.symbol);
    FunctionObject* function = get_vector_at(&compiler->module->functions, position);
    Vector* params = &ast->function_call.symbol->function_definition.params;
    Operand result = compile_call(compiler, function, position, &ast->function_call.args, params, discard);

    if (discard || get_reference_type(ast) == RefNone) {
        return result;
    }

    return dereference_operand(compiler, result, get_type_slot_count(get_node_data_type(ast)));
}

static Operand compile_reference_expression(Compiler* compiler, ASTNode* ast)
{
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

static void compile_function_definition(Compiler* compiler, ASTNode* ast)
{
    if (!compiler->repl && !ast->function_definition.call_count) {
        return;
    }

    int previous_frame_base_count = compiler->frame_base_count;
    int previous_local_position_offset = compiler->local_position_offset;
    int previous_register_count = compiler->register_count;
    size_t previous_peephole_barrier = compiler->peephole_barrier;
    FunctionObject* previous_function = compiler->function;
    FunctionObject* function = create_defined_function_object(ast);
    ASTNode* body = ast->function_definition.body;

    compiler->frame_base_count = get_call_area_count(function);
    compiler->local_position_offset = 0;
    compiler->register_count = compiler->frame_base_count;
    compiler->function = function;
    compiler->peephole_barrier = 0;

    push_vector_item(&compiler->module->functions, function);
    push_vector_item(&compiler->function_references, ast);
    compile_blocklevel_statements(compiler, &body->compound.statements);

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
    compiler->frame_base_count = previous_frame_base_count;
    compiler->local_position_offset = previous_local_position_offset;
    compiler->register_count = previous_register_count;
    compiler->peephole_barrier = previous_peephole_barrier;
}

static void compile_return_statement(Compiler* compiler, ASTNode* ast)
{
    if (is_none(ast->return_statement.expr)) {
        compiler->register_count = compiler->frame_base_count;

        return emit_ret(compiler);
    }

    bool reference = get_reference_type(ast->return_statement.expr) != RefNone;
    ASTNode* function = get_current_function_definition(compiler);
    DataType return_type = function->function_definition.return_type_id;
    Operand value = compile_converted_expression(
        compiler, ast->return_statement.expr, return_type, reference);

    emit_retv(compiler, value);
    release_operand(compiler, value);
    compiler->register_count = compiler->frame_base_count;
}

static ASTNode* get_current_function_definition(Compiler* compiler)
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

static Operand compile_zero_value(Compiler* compiler, size_t slots)
{
    if (slots == 2) {
        size_t position = make_i64_constant(compiler, 0);

        return make_wide_operand(emit_ldc_i64(compiler, position), true);
    }

    return make_operand(emit_ldi(compiler, 0), true);
}

static bool compile_unused_variable_definition(
    Compiler* compiler, ASTNode* ast, size_t slots, bool unused, bool preserve)
{
    if (!unused || preserve) {
        return false;
    }

    compile_expression(compiler, ast->variable_definition.expr, true);

    if (is_top_level_scope(ast->variable_definition.scope)) {
        return true;
    }

    allocate_registers(compiler, slots);

    return true;
}

static void finish_global_definition(Compiler* compiler, ASTNode* ast, Operand value)
{
    if (!is_top_level_scope(ast->variable_definition.scope)) {
        return;
    }

    store_global_variable(compiler, ast, value);
    release_operand(compiler, value);
}

static void compile_variable_definition(Compiler* compiler, ASTNode* ast)
{
    ast->variable_definition.register_position = compiler->register_count;
    Operand value;
    size_t slots = get_node_slot_count(ast);
    bool unused = ast->variable_definition.read_count == ast->variable_definition.discarded_read_count;
    bool constant_value = ast->variable_definition.constant && !ast->variable_definition.address_taken;
    bool preserve = compiler->repl && is_compiling_top_level(compiler);

    if (compile_unused_variable_definition(compiler, ast, slots, unused || constant_value, preserve)) {
        return;
    }

    if (is_none(ast->variable_definition.expr)) {
        value = compile_zero_value(compiler, slots);
    } else {
        bool reference = get_reference_type(ast) != RefNone;
        value = compile_converted_expression(
            compiler, ast->variable_definition.expr, get_node_data_type(ast), reference);
    }

    if (!value.temporary) {
        int reg = allocate_registers(compiler, slots);

        emit_operand_mov(compiler, reg, value);

        value.reg = reg;
        value.temporary = true;
    }

    finish_global_definition(compiler, ast, value);
}

static size_t emit_jump(Compiler* compiler, Opcode opcode, int condition_register)
{
    size_t position = count_code_object(current_code_object(compiler));
    emit_instruction(compiler, opcode, condition_register, 0, 0);

    return position;
}

static Opcode get_condition_jump_opcode(Opcode comparison)
{
    switch (comparison) {
        case OpEq:     return OpBne;
        case OpNe:     return OpBeq;
        case OpEqi:    return OpBnei;
        case OpNei:    return OpBeqi;
        case OpLtI32: return OpBleI32;
        case OpLtU32: return OpBleU32;
        case OpLeI32: return OpBltI32;
        case OpLeU32: return OpBltU32;
        default:        return OpHlt;
    }
}

static size_t emit_condition_jump(Compiler* compiler, Operand condition)
{
    PreviousInstruction comparison;

    if (!condition.temporary || !get_previous_instruction(compiler, &comparison) || comparison.a != condition.reg) {
        return emit_jump(compiler, OpBz, condition.reg);
    }

    Opcode opcode = get_condition_jump_opcode(comparison.opcode);

    if (opcode == OpHlt) {
        return emit_jump(compiler, OpBz, condition.reg);
    }

    size_t position = comparison.start + INSTRUCTION_SIZE;
    int left = comparison.b;
    int right = comparison.c;

    if (opcode >= OpBltI32 && opcode <= OpBleU32) {
        left = comparison.c;
        right = comparison.b;
    }

    set_byte_at(comparison.code, comparison.start, OpNop);
    emit_instruction(compiler, opcode, left, right, 0);

    return position;
}

static Opcode get_jump_comparison_opcode(Opcode opcode)
{
    switch (opcode) {
        case OpBne:        return OpEq;
        case OpBeq:        return OpNe;
        case OpBnei:       return OpEqi;
        case OpBeqi:       return OpNei;
        case OpBleI32:    return OpLtI32;
        case OpBleU32:    return OpLtU32;
        case OpBltI32:    return OpLeI32;
        case OpBltU32:    return OpLeU32;
        default:            return OpHlt;
    }
}

static Opcode expand_condition_jump(CodeObject* code, size_t position, Opcode opcode, size_t offset)
{
    if (opcode < OpBeq || opcode > OpBleU32 || offset <= INT8_MAX) {
        return opcode;
    }

    size_t comparison = position - INSTRUCTION_SIZE;
    Opcode comparison_opcode = get_jump_comparison_opcode(opcode);

    set_byte_at(code, comparison, comparison_opcode);
    set_byte_at(code, position, OpBz);
    set_byte_at(code, position + 1, code->data[comparison + 1]);

    return OpBz;
}

static void check_jump_offset(Opcode opcode, size_t offset)
{
    size_t maximum = INT32_MAX >> 8;

    if (opcode == OpBz || opcode == OpBnz) {
        maximum = INT16_MAX;
    }

    if (opcode >= OpBeq && opcode <= OpBleU32) {
        maximum = INT8_MAX;
    }

    if (offset > maximum) {
        branch_offset_overflow_error();
    }
}

static void patch_jump(Compiler* compiler, size_t position)
{
    CodeObject* code = current_code_object(compiler);

    if (code->data[position] == OpWide) {
        position += INSTRUCTION_SIZE;
    }

    size_t count = count_code_object(code);
    size_t word_offset = (count - position - INSTRUCTION_SIZE) / INSTRUCTION_SIZE;
    Opcode opcode = (Opcode)code->data[position];
    compiler->peephole_barrier = count;
    opcode = expand_condition_jump(code, position, opcode, word_offset);

    check_jump_offset(opcode, word_offset);

    int offset = (int)word_offset;

    if (opcode == OpBz || opcode == OpBnz) {
        set_byte_at(code, position + 2, offset >> 8);
        set_byte_at(code, position + 3, offset);

        return;
    }

    if (opcode >= OpBeq && opcode <= OpBleU32) {
        set_byte_at(code, position + 3, offset);

        return;
    }

    set_byte_at(code, position + 1, offset >> 16);
    set_byte_at(code, position + 2, offset >> 8);
    set_byte_at(code, position + 3, offset);
}

static void store_conditional_value(
    Compiler* compiler, DataType type, int destination, Operand value)
{
    if (type == TypeVoid) {
        return;
    }

    if (retarget_temporary(compiler, value, destination)) {
        return;
    }

    emit_operand_mov(compiler, destination, value);
}

static Operand compile_conditional_branch(
    Compiler* compiler, ASTNode* branch, DataType type, int destination, bool reference)
{
    if (branch->type == ASTConditional) {
        Operand value = compile_converted_expression(compiler, branch, type, reference);

        store_conditional_value(compiler, type, destination, value);

        return value;
    }

    size_t count = count_vector(&branch->compound.statements);

    for (size_t i = 0; i < count; i++) {
        bool discard = type == TypeVoid || i + 1 < count;
        ASTNode* statement = get_vector_at(&branch->compound.statements, i);

        if (discard) {
            compile_statement(compiler, statement, true);

            continue;
        }

        Operand value = compile_converted_expression(compiler, statement, type, reference);

        store_conditional_value(compiler, type, destination, value);

        return value;
    }

    return no_operand();
}

static Operand allocate_branch_result(Compiler* compiler, DataType type)
{
    if (type == TypeVoid) {
        return no_operand();
    }

    size_t slots = get_type_slot_count(type);
    int reg = allocate_registers(compiler, slots);
    Operand result = {reg, true, slots};
    compiler->local_position_offset += slots;

    return result;
}

static void compile_scoped_conditional_branch(
    Compiler* compiler, ASTNode* branch, DataType type, int destination, bool reference)
{
    if (!branch) {
        return;
    }

    int register_count = compiler->register_count;

    compile_conditional_branch(compiler, branch, type, destination, reference);

    compiler->register_count = register_count;
}

static void compile_conditional_branches(
    Compiler* compiler, ASTNode* ast, int destination, bool reference, size_t else_jump)
{
    ASTNode* condition = ast->conditional.condition;
    DataType type = get_node_data_type(ast);

    if (condition->type == ASTBoolean) {
        ASTNode* branch = condition->boolean_literal.value ? ast->conditional.then_branch : ast->conditional.else_branch;

        compile_scoped_conditional_branch(compiler, branch, type, destination, reference);

        return;
    }

    compile_scoped_conditional_branch(compiler, ast->conditional.then_branch, type, destination, reference);

    if (!ast->conditional.else_branch) {
        patch_jump(compiler, else_jump);

        return;
    }

    size_t end_jump = emit_jump(compiler, OpJmp, 0);

    patch_jump(compiler, else_jump);
    compile_scoped_conditional_branch(compiler, ast->conditional.else_branch, type, destination, reference);
    patch_jump(compiler, end_jump);
}

static Operand compile_conditional(Compiler* compiler, ASTNode* ast, bool reference)
{
    int previous_local_position_offset = compiler->local_position_offset;
    ASTNode* condition_node = ast->conditional.condition;
    size_t else_jump = 0;

    if (condition_node->type != ASTBoolean) {
        Operand condition = compile_expression(compiler, condition_node, false);
        else_jump = emit_condition_jump(compiler, condition);

        release_operand(compiler, condition);
    }

    DataType type = get_node_data_type(ast);
    Operand result = allocate_branch_result(compiler, type);

    compile_conditional_branches(compiler, ast, result.reg, reference, else_jump);

    compiler->local_position_offset = previous_local_position_offset;

    return result;
}

static void emit_loop_jump(Compiler* compiler, size_t target)
{
    CodeObject* code = current_code_object(compiler);
    size_t position = count_code_object(code);
    size_t distance = (position + INSTRUCTION_SIZE - target) / INSTRUCTION_SIZE;

    check_jump_offset(OpJmp, distance);

    int offset = -(int)distance;

    emit_instruction(compiler, OpJmp, (offset >> 16) & 255, (offset >> 8) & 255, offset & 255);
}

static void compile_while(Compiler* compiler, ASTNode* ast)
{
    CodeObject* code = current_code_object(compiler);
    size_t start = count_code_object(code);
    int register_count = compiler->register_count;
    compiler->peephole_barrier = start;

    Operand condition = compile_expression(compiler, ast->while_statement.condition, false);
    size_t exit_jump = emit_condition_jump(compiler, condition);

    release_operand(compiler, condition);
    compile_scoped_conditional_branch(compiler, ast->while_statement.body, TypeVoid, 0, false);
    emit_loop_jump(compiler, start);
    patch_jump(compiler, exit_jump);

    compiler->register_count = register_count;
}

static Operand compile_match_subject(Compiler* compiler, ASTNode* subject)
{
    DataType type = get_node_data_type(subject);
    Operand value = compile_comparison_operand(compiler, subject, type);
    value.temporary = false;

    return value;
}

static bool is_match_alternative(ASTNode* pattern)
{
    return pattern->type == ASTBinary && pattern->binary.operator.type == TokenPipe;
}

static Operand compile_subject_match_condition(Compiler* compiler, ASTNode* pattern, Operand subject)
{
    if (!is_match_alternative(pattern)) {
        DataType type = get_node_data_type(pattern);
        Operand value = compile_comparison_operand(compiler, pattern, type);

        return emit_numeric_comparison(compiler, TokenEqualEqual, type, subject, value);
    }

    Operand left = compile_subject_match_condition(compiler, pattern->binary.left_expr, subject);
    Operand right = compile_subject_match_condition(compiler, pattern->binary.right_expr, subject);

    return emit_comparison(compiler, OpOr, left, right);
}

static Operand compile_match_condition(Compiler* compiler, ASTNode* ast, MatchArm* arm, Operand subject)
{
    if (!ast->match.subject) {
        return compile_expression(compiler, arm->pattern, false);
    }

    return compile_subject_match_condition(compiler, arm->pattern, subject);
}

static void patch_match_jumps(Compiler* compiler, Vector* jumps)
{
    size_t count = count_vector(jumps);

    for (size_t i = 0; i < count; i++) {
        void* jump = get_vector_at(jumps, i);
        patch_jump(compiler, (size_t)(uintptr_t)jump);
    }
}

static void mark_match_join(Compiler* compiler, Vector* jumps)
{
    if (!count_vector(jumps)) {
        return;
    }

    patch_match_jumps(compiler, jumps);
}

static bool are_simple_match_values(Vector* values)
{
    size_t count = count_vector(values);

    for (size_t i = 0; i < count; i++) {
        ASTNode* value = get_vector_at(values, i);
        if (!is_simple_match_value(value)) {
            return false;
        }
    }

    return true;
}

static bool is_simple_match_value(ASTNode* ast)
{
    switch (ast->type) {
        case ASTBoolean:
        case ASTCharacter:
        case ASTInteger:
        case ASTFloat:
        case ASTVariable:
            return true;
        case ASTBuiltinCall:
            return are_simple_match_values(&ast->builtin_call.args);
        default:
            return false;
    }
}

static bool can_use_direct_match_branch(MatchArm* arm)
{
    size_t count = count_vector(&arm->branch->compound.statements);
    if (count != 1) {
        return false;
    }

    ASTNode* statement = get_vector_at(&arm->branch->compound.statements, 0);

    return is_simple_match_value(statement);
}

static bool is_literal_match_pattern(ASTNode* pattern)
{
    if (pattern->type == ASTInteger || pattern->type == ASTBoolean) {
        return true;
    }

    if (!is_match_alternative(pattern)) {
        return false;
    }

    return is_literal_match_pattern(pattern->binary.left_expr) && is_literal_match_pattern(pattern->binary.right_expr);
}

static size_t compile_match_alternative_jumps(
    Compiler* compiler, ASTNode* pattern, Operand subject, bool last, Vector* matches)
{
    if (is_match_alternative(pattern)) {
        compile_match_alternative_jumps(compiler, pattern->binary.left_expr, subject, false, matches);

        return compile_match_alternative_jumps(compiler, pattern->binary.right_expr, subject, last, matches);
    }

    Opcode opcode = OpNe;

    if (last) {
        opcode = OpEq;
    }

    Operand value = compile_expression(compiler, pattern, false);
    TokenType operator_type = TokenNotEqual;

    if (opcode == OpEq) {
        operator_type = TokenEqualEqual;
    }

    DataType type = get_node_data_type(pattern);
    Operand condition = emit_numeric_comparison(compiler, operator_type, type, subject, value);
    size_t jump = emit_condition_jump(compiler, condition);

    release_operand(compiler, condition);

    if (!last) {
        push_vector_item(matches, (void*)(uintptr_t)jump);
    }

    return jump;
}

static size_t compile_match_alternative_condition(Compiler* compiler, ASTNode* pattern, Operand subject)
{
    Vector matches;

    init_vector(&matches);

    size_t next_jump = compile_match_alternative_jumps(compiler, pattern, subject, true, &matches);

    patch_match_jumps(compiler, &matches);
    free_vector(&matches);

    return next_jump;
}

static size_t compile_match_arm_condition(Compiler* compiler, ASTNode* ast, MatchArm* arm, Operand subject)
{
    if (!arm->pattern) {
        return 0;
    }

    DataType subject_type = get_node_data_type(ast->match.subject);
    size_t subject_size = get_integer_type_size(subject_type);

    if (ast->match.subject && subject.slots == 1 && is_match_alternative(arm->pattern)
        && is_literal_match_pattern(arm->pattern) && subject_size != 8) {
        return compile_match_alternative_condition(compiler, arm->pattern, subject);
    }

    DataType pattern_type = get_node_data_type(arm->pattern);
    bool numeric_compare = get_integer_type_size(pattern_type) == 8 || is_float_type(pattern_type);

    if (!ast->match.subject || is_match_alternative(arm->pattern) || !can_use_direct_match_branch(arm) || numeric_compare) {
        Operand condition = compile_match_condition(compiler, ast, arm, subject);
        size_t next_jump = emit_condition_jump(compiler, condition);
        release_operand(compiler, condition);

        return next_jump;
    }

    uint64_t pattern_value;

    if (subject.slots == 1 && get_literal_match_value(arm->pattern, &pattern_value) && pattern_value == 0) {
        return emit_jump(compiler, OpBnz, subject.reg);
    }

    Operand pattern = compile_expression(compiler, arm->pattern, false);
    Operand condition = emit_immediate_comparison(compiler, OpEq, subject, pattern);

    if (condition.reg >= 0) {
        size_t next_jump = emit_condition_jump(compiler, condition);

        release_operand(compiler, condition);

        return next_jump;
    }

    size_t next_jump = count_code_object(current_code_object(compiler));
    emit_instruction(compiler, OpBne, subject.reg, pattern.reg, 0);
    release_operand(compiler, pattern);

    return next_jump;
}

static void store_match_binding(Compiler* compiler, ASTNode* binding, Operand subject)
{
    if (!binding) {
        return;
    }

    binding->variable_definition.register_position = compiler->frame_base_count + compiler->local_position_offset
        + binding->variable_definition.position;
    int position = get_local_position(compiler, binding);

    emit_operand_mov(compiler, position, subject);

    DataType binding_type = get_node_data_type(binding);
    int binding_register_count = position + get_type_slot_count(binding_type);

    compiler->register_count = MAX(compiler->register_count, binding_register_count);
}

static size_t compile_match_guard(Compiler* compiler, MatchArm* arm)
{
    if (!arm->guard) {
        return 0;
    }

    Operand condition = compile_expression(compiler, arm->guard, false);
    size_t next_jump = emit_condition_jump(compiler, condition);

    release_operand(compiler, condition);

    return next_jump;
}

static size_t compile_match_arm(Compiler* compiler, ASTNode* ast, MatchArm* arm, Operand subject, int destination,
    bool needs_end_jump, bool reference)
{
    int branch_register_count = compiler->register_count;
    size_t next_jump = compile_match_arm_condition(compiler, ast, arm, subject);
    DataType type = get_node_data_type(ast);

    store_match_binding(compiler, arm->binding, subject);

    size_t guard_jump = compile_match_guard(compiler, arm);

    compile_conditional_branch(compiler, arm->branch, type, destination, reference);
    compiler->register_count = branch_register_count;

    size_t end_jump = SIZE_MAX;

    if (needs_end_jump) {
        end_jump = emit_jump(compiler, OpJmp, 0);
    }

    if (arm->pattern) {
        patch_jump(compiler, next_jump);
    }

    if (arm->guard) {
        patch_jump(compiler, guard_jump);
    }

    return end_jump;
}

static bool compile_next_match_arm(Compiler* compiler, ASTNode* ast, MatchArm* arm, Operand subject, int destination,
    bool has_following_arm, Vector* end_jumps, bool reference)
{
    bool unconditional = !arm->pattern && !arm->guard;
    bool needs_end_jump = !unconditional && has_following_arm;
    size_t end_jump = compile_match_arm(compiler, ast, arm, subject, destination, needs_end_jump, reference);

    if (end_jump != SIZE_MAX) {
        push_vector_item(end_jumps, (void*)(uintptr_t)end_jump);
    }

    return unconditional;
}

static bool compile_match_arms(
    Compiler* compiler, ASTNode* ast, Operand subject, int destination, Vector* end_jumps, bool reference)
{
    size_t count = count_vector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = get_vector_at(&ast->match.arms, i);
        bool has_following_arm = i + 1 < count || ast->match.default_branch;
        bool unconditional = compile_next_match_arm(compiler, ast, arm, subject, destination, has_following_arm,
            end_jumps, reference);

        if (unconditional) {
            return true;
        }
    }

    return false;
}

static void compile_match_default(
    Compiler* compiler, ASTNode* ast, int destination, int branch_register_count, bool reference)
{
    if (!ast->match.default_branch) {
        return;
    }

    DataType type = get_node_data_type(ast);

    compile_conditional_branch(compiler, ast->match.default_branch, type, destination, reference);
    compiler->register_count = branch_register_count;
}

static Operand match_result_operand(DataType type, int destination)
{
    if (type == TypeVoid) {
        return no_operand();
    }

    if (get_type_slot_count(type) == 2) {
        return make_wide_operand(destination, true);
    }

    return make_operand(destination, true);
}

static bool get_literal_match_value(ASTNode* ast, uint64_t* value)
{
    if (ast->type == ASTBoolean) {
        *value = ast->boolean_literal.value;

        return true;
    }

    if (ast->type == ASTCharacter) {
        *value = ast->character_literal.value;

        return true;
    }

    if (ast->type != ASTInteger) {
        return false;
    }

    *value = ast->integer_literal.value;

    if (ast->integer_literal.type_id == TypeI32) {
        *value = (uint64_t)(int64_t)(int32_t)*value;
    }

    return true;
}

static bool get_literal_match_subject(ASTNode* ast, uint64_t* value)
{
    if (!ast->match.subject) {
        *value = true;

        return true;
    }

    return get_literal_match_value(ast->match.subject, value);
}

typedef enum LiteralMatchArmResult
{
    LiteralMatchArmUnavailable,
    LiteralMatchArmSkipped,
    LiteralMatchArmSelected
} LiteralMatchArmResult;

static bool evaluate_literal_match_pattern(ASTNode* pattern, uint64_t subject_value, bool* matches)
{
    uint64_t pattern_value;

    if (get_literal_match_value(pattern, &pattern_value)) {
        *matches = subject_value == pattern_value;

        return true;
    }

    if (!is_match_alternative(pattern)) {
        return false;
    }

    bool left;
    bool right;

    if (!evaluate_literal_match_pattern(pattern->binary.left_expr, subject_value, &left)
        || !evaluate_literal_match_pattern(pattern->binary.right_expr, subject_value, &right)) {
        return false;
    }

    *matches = left || right;

    return true;
}

static LiteralMatchArmResult select_literal_match_arm(
    MatchArm* arm, uint64_t subject_value, ASTNode** branch, ASTNode** binding)
{
    if (arm->guard) {
        return LiteralMatchArmUnavailable;
    }

    if (!arm->pattern) {
        *branch = arm->branch;
        *binding = arm->binding;

        return LiteralMatchArmSelected;
    }

    bool matches;

    if (!evaluate_literal_match_pattern(arm->pattern, subject_value, &matches)) {
        return LiteralMatchArmUnavailable;
    }

    if (!matches) {
        return LiteralMatchArmSkipped;
    }

    *branch = arm->branch;
    *binding = NULL;

    return LiteralMatchArmSelected;
}

static bool find_literal_match_branch(ASTNode* ast, ASTNode** branch, ASTNode** binding)
{
    uint64_t subject_value;
    if (!get_literal_match_subject(ast, &subject_value)) {
        return false;
    }

    size_t count = count_vector(&ast->match.arms);
    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = get_vector_at(&ast->match.arms, i);
        LiteralMatchArmResult result = select_literal_match_arm(arm, subject_value, branch, binding);

        switch (result) {
            case LiteralMatchArmUnavailable:
                return false;
            case LiteralMatchArmSelected:
                return true;
            case LiteralMatchArmSkipped:
                break;
        }
    }

    *branch = ast->match.default_branch;
    *binding = NULL;

    return true;
}

static bool compile_literal_match(
    Compiler* compiler, ASTNode* ast, DataType type, int destination, bool reference)
{
    ASTNode* branch;
    ASTNode* binding;
    if (!find_literal_match_branch(ast, &branch, &binding)) {
        return false;
    }

    if (!branch) {
        return true;
    }

    if (binding) {
        Operand subject = compile_match_subject(compiler, ast->match.subject);
        store_match_binding(compiler, binding, subject);
    }

    compile_conditional_branch(compiler, branch, type, destination, reference);

    return true;
}

static void restore_match_compiler_state(Compiler* compiler, DataType type, int previous_local_position_offset,
    int previous_register_count)
{
    compiler->local_position_offset = previous_local_position_offset;
    compiler->register_count = previous_register_count;

    if (type != TypeVoid) {
        compiler->register_count += get_type_slot_count(type);
    }
}

static Operand compile_match(Compiler* compiler, ASTNode* ast, bool reference)
{
    int previous_local_position_offset = compiler->local_position_offset;
    int previous_register_count = compiler->register_count;
    DataType type = get_node_data_type(ast);
    Operand result = allocate_branch_result(compiler, type);
    int destination = result.reg;

    if (compile_literal_match(compiler, ast, type, destination, reference)) {
        restore_match_compiler_state(compiler, type, previous_local_position_offset, previous_register_count);

        return match_result_operand(type, destination);
    }

    Operand subject = no_operand();

    if (ast->match.subject) {
        subject = compile_match_subject(compiler, ast->match.subject);
    }

    int branch_register_count = compiler->register_count;

    Vector end_jumps;
    init_vector(&end_jumps);

    bool unconditional_arm = compile_match_arms(compiler, ast, subject, destination, &end_jumps, reference);

    if (!unconditional_arm) {
        compile_match_default(compiler, ast, destination, branch_register_count, reference);
    }

    mark_match_join(compiler, &end_jumps);
    free_vector(&end_jumps);

    restore_match_compiler_state(compiler, type, previous_local_position_offset, previous_register_count);

    return match_result_operand(type, destination);
}

static bool compile_discarded_prefix(Compiler* compiler, ASTNode* ast)
{
    if (ast->prefix.operator.type == TokenKeywordConst) {
        return false;
    }

    compile_expression(compiler, ast->prefix.expr, true);

    return true;
}

static bool compile_discarded_expression(Compiler* compiler, ASTNode* ast)
{
    switch (ast->type) {
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

static Operand compile_expression(Compiler* compiler, ASTNode* ast, bool discard)
{
    if (discard && compile_discarded_expression(compiler, ast)) {
        return no_operand();
    }

    Operand result;

    switch (ast->type) {
        case ASTBinary:
            result = compile_binary(compiler, ast);
            break;
        case ASTString:
            result = compile_string(compiler, ast);
            break;
        case ASTCharacter:
            result = compile_character(compiler, ast);
            break;
        case ASTBoolean:
            result = compile_boolean(compiler, ast);
            break;
        case ASTBuiltinCall:
            return compile_builtin_call(compiler, ast, discard);
        case ASTConditional:
            result = compile_conditional(compiler, ast, false);
            break;
        case ASTMatch:
            result = compile_match(compiler, ast, false);
            break;
        case ASTConversion:
            result = compile_explicit_conversion(compiler, ast);
            break;
        case ASTFunctionCall:
            return compile_function_call(compiler, ast, discard);
        case ASTFloat:
            result = compile_float(compiler, ast);
            break;
        case ASTInteger:
            result = compile_number(compiler, ast);
            break;
        case ASTPrefix:
            result = compile_prefix(compiler, ast);
            break;
        case ASTVariable:
            result = compile_variable(compiler, ast);
            break;
        default:
            return no_operand();
    }

    if (discard) {
        release_operand(compiler, result);

        return no_operand();
    }

    return result;
}

static Operand compile_statement(Compiler* compiler, ASTNode* ast, bool discard)
{
    switch (ast->type) {
        case ASTAssignment:
            compile_assignment(compiler, ast);
            return no_operand();
        case ASTBuiltinCall:
            return compile_builtin_call(compiler, ast, discard);
        case ASTFunctionCall:
            return compile_function_call(compiler, ast, discard);
        case ASTFunctionDefinition:
            compile_function_definition(compiler, ast);
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

static void compile_blocklevel_statements(Compiler* compiler, Vector* nodes)
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

static void compile_top_level_statements(Compiler* compiler, Vector* nodes)
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
    bool display = is_last && is_expression_statement(ast) && get_node_data_type(ast) != TypeVoid;

    if (display && get_node_data_type(ast) == TypeUnknown) {
        unresolved_repl_expression_type_error();
    }

    Operand value = compile_statement(compiler, ast, !display);

    if (!display) {
        return;
    }

    DataType argument_type = get_node_data_type(ast);
    void* items[] = {ast};
    Vector args = {.data = items, .capacity = 1, .count = 1};
    const Builtin* builtin = resolve_builtin("print", &argument_type, 1);
    uint16_t position = get_builtin_call_position(compiler, builtin, &args);
    FunctionObject* function = get_vector_at(&compiler->module->functions, position);

    compile_call_with_argument(compiler, function, position, value, true);
}

static void compile_repl_statements(Compiler* compiler, Vector* nodes)
{
    size_t count = count_vector(nodes);

    while (compiler->statement_index < count) {
        ASTNode* ast = nodes->data[compiler->statement_index++];
        bool is_last = compiler->statement_index == count;

        compile_repl_statement(compiler, ast, is_last);
        finish_top_level_statement(compiler, ast);
    }
}

void init_compiler(Compiler* compiler, ModuleObject* module)
{
    init_vector(&compiler->function_references);

    for (size_t i = 0; i < count_vector(&module->functions); i++) {
        push_vector_item(&compiler->function_references, NULL);
    }

    Scope* scope = create_scope(NULL);
    ASTNode* ast = create_compound_node(scope);

    init_parser(&compiler->parser, ast);
    init_analyzer(&compiler->analyzer, ast);

    compiler->module = module;
    compiler->function = get_vector_at(&module->functions, 0);
    compiler->ast = ast;
    compiler->statement_index = 0;
    compiler->peephole_barrier = 0;
    compiler->register_count = 0;
    compiler->frame_base_count = 0;
    compiler->local_position_offset = 0;
    compiler->repl = false;
}

void free_compiler(Compiler* compiler)
{
    free_vector(&compiler->function_references);
    free_ast_node(compiler->ast);
}

static void compile_statement_bytecode(Compiler* compiler, CompileStatements compile_statements)
{
    CodeObject* code = current_code_object(compiler);
    clear_code_object(code);

    compiler->peephole_barrier = 0;
    compile_statements(compiler, &compiler->ast->compound.statements);
    emit_hlt(compiler);
    optimize_bytecode(compiler->module, code, compiler->register_count);
}

static bool has_unused_compiled_functions(Compiler* compiler)
{
    bool unused = false;

    for (size_t i = 1; i < compiler->function_references.count; i++) {
        ASTNode* ast = compiler->function_references.data[i];
        unused = unused || (ast && !ast->function_definition.call_count);
    }

    return unused;
}

static void discard_compiled_functions(Compiler* compiler)
{
    while (compiler->module->functions.count > 1) {
        FunctionObject* function = pop_vector_item(&compiler->module->functions);

        free_function_object(function);
    }

    compiler->function_references.count = 1;
    compiler->module->global_count = 0;
    compiler->function->max_stack_count = 0;
    compiler->statement_index = 0;
    compiler->register_count = 0;
    compiler->frame_base_count = 0;
    compiler->local_position_offset = 0;
}

static void eliminate_compile_time_functions(Compiler* compiler)
{
    if (compiler->repl) {
        return;
    }

    mark_reachable_functions(&compiler->ast->compound.statements);

    if (!has_unused_compiled_functions(compiler)) {
        return;
    }

    discard_compiled_functions(compiler);
    compile_statement_bytecode(compiler, compile_top_level_statements);
}

static bool compile_source(Compiler* compiler, const char* source, CompileStatements compile_statements)
{
    if (!compiler->module) {
        return false;
    }

    compiler->repl = compile_statements == compile_repl_statements;
    size_t start = count_vector(&compiler->ast->compound.statements);

    if (!parse(&compiler->parser, source)) {
        return false;
    }

    analyze(&compiler->analyzer, start);
    optimize_ast(compiler->ast, start);

    if (compile_statements == compile_top_level_statements) {
        remove_discarded_reads(&compiler->ast->compound.statements, start);
        mark_reachable_functions(&compiler->ast->compound.statements);
    }

    compile_statement_bytecode(compiler, compile_statements);
    eliminate_compile_time_functions(compiler);

    return true;
}

bool compile(Compiler* compiler, const char* source)
{
    return compile_source(compiler, source, compile_top_level_statements);
}

CompileResult compile_repl(Compiler* compiler, const char* source)
{
    ErrorContext context;
    size_t start = compiler->ast->compound.statements.count;

    begin_error_context(&context, NULL);

    if (setjmp(context.jump) != 0) {
        end_error_context(&context);
        discard_repl_statements(compiler, start);
        recover_compiler(compiler);

        return CompileError;
    }

    bool complete = compile_source(compiler, source, compile_repl_statements);

    end_error_context(&context);

    if (!complete) {
        discard_repl_statements(compiler, start);

        return CompileIncomplete;
    }

    return CompileSuccess;
}
