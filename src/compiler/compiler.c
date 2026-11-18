#include "compiler/compiler.h"
#include "common/errors.h"
#include "common/memory.h"
#include "common/vector.h"
#include "compiler/generator/emitter.h"
#include "compiler/generator/generator.h"
#include "compiler/analyzer/analyzer.h"
#include "compiler/analyzer/recovery.h"
#include "compiler/ast.h"
#include "compiler/ast_optimizer.h"
#include "compiler/bytecode_optimizer.h"
#include "compiler/parser.h"
#include "compiler/scope.h"
#include "runtime/code_object.h"
#include "runtime/function_object.h"
#include "runtime/module_object.h"
#include "runtime/string_object.h"
#include <stddef.h>

typedef void (*CompileStatements)(Compiler* compiler, Vector* nodes);

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

void init_compiler(Compiler* compiler, ModuleObject* module)
{
    init_vector(&compiler->function_references);
    init_vector(&compiler->omitted_statements);

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
    compiler->repl_mode = false;
    compiler->test_mode = false;
    compiler->test_index = 0;
}

void free_compiler(Compiler* compiler)
{
    free_vector(&compiler->function_references);

    for (size_t i = 0; i < compiler->omitted_statements.count; i++) {
        ASTNode* ast = compiler->omitted_statements.data[i];

        free_ast_node(ast);
    }

    free_vector(&compiler->omitted_statements);
    free_ast_node(compiler->ast);

    for (size_t i = 0; i < compiler->analyzer.array_types.count; i++) {
        free_memory(compiler->analyzer.array_types.data[i]);
    }

    free_vector(&compiler->analyzer.array_types);
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
    if (compiler->repl_mode) {
        return;
    }

    mark_reachable_functions(&compiler->ast->compound.statements);

    if (!has_unused_compiled_functions(compiler)) {
        return;
    }

    discard_compiled_functions(compiler);
    compile_statement_bytecode(compiler, compile_top_level_statements);
}

static void retain_test_statement(Compiler* compiler, ASTNode* ast, size_t* retained)
{
    bool declaration = ast->type == ASTFunctionDefinition || ast->type == ASTVariableDefinition;

    if (declaration) {
        compiler->ast->compound.statements.data[(*retained)++] = ast;

        return;
    }

    push_vector_item(&compiler->omitted_statements, ast);
}

static ASTNode* get_selected_test(Compiler* compiler)
{
    Vector* nodes = &compiler->ast->compound.statements;

    if (compiler->test_index >= nodes->count) {
        fatal_error("Test block index is out of range");
    }

    ASTNode* ast = nodes->data[compiler->test_index];

    if (ast->type != ASTFunctionDefinition || !ast->function_definition.is_test) {
        fatal_error("Selected statement is not a test block");
    }

    return ast;
}

static void select_test_statements(Compiler* compiler)
{
    Vector* nodes = &compiler->ast->compound.statements;
    ASTNode* test = get_selected_test(compiler);
    size_t retained = 0;
    size_t count = nodes->count;

    for (size_t i = 0; i < count; i++) {
        ASTNode* ast = nodes->data[i];

        retain_test_statement(compiler, ast, &retained);
    }

    nodes->count = retained;

    ASTNode* call = create_function_call_node(test->function_definition.token);
    StringObject* id = test->function_definition.id;

    call->function_call.id = copy_string_object(id->chars, id->length);
    call->function_call.symbol = test;
    call->function_call.scope = compiler->ast->compound.scope;

    push_vector_item(nodes, call);
}

static bool compile_source(Compiler* compiler, const char* source, CompileStatements compile_statements)
{
    if (!compiler->module) {
        return false;
    }

    compiler->repl_mode = compile_statements == compile_repl_statements;
    size_t start = count_vector(&compiler->ast->compound.statements);

    if (!parse(&compiler->parser, source)) {
        return false;
    }

    analyze(&compiler->analyzer, start);

    if (compiler->test_mode) {
        select_test_statements(compiler);
    }

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

bool compile_test(Compiler* compiler, const char* source, size_t test_index)
{
    compiler->test_mode = true;
    compiler->test_index = test_index;

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
