#ifndef GENERATOR_FUNCTION_H
#define GENERATOR_FUNCTION_H

#include "builtin/builtin.h"
#include "compiler/generator/operand.h"
#include "compiler/lexer/token.h"
#include "runtime/function_object.h"
#include <stddef.h>
#include <stdint.h>

typedef struct CallArea
{
    int result_register;
    int frame_register;
} CallArea;

CallArea open_call_area(Compiler* compiler);
Operand close_call_area(Compiler* compiler, FunctionObject* function,
    uint16_t function_position, CallArea call, bool discard);
// Returns SIZE_MAX when the builtin function is not registered
size_t find_builtin_function_position(const Compiler* compiler, const Builtin* builtin);
uint16_t add_builtin_function(Compiler* compiler, const Builtin* builtin);
Operand emit_builtin_operands(Compiler* compiler, const Builtin* builtin,
    const Type* types, const Operand* arguments, size_t count, bool discard);
Operand emit_binary_builtin(Compiler* compiler, TokenType operator_type,
    Type left_type, Type right_type, Operand left, Operand right);
// Returns SIZE_MAX when no registered builtin matches the call
size_t find_builtin_call_position(const Compiler* compiler, const Builtin* builtin, const Vector* args);
uint16_t add_builtin_call(Compiler* compiler, const Builtin* builtin, const Vector* args);
uint16_t get_function_position(Compiler* compiler, ASTNode* ast);
Operand compile_call(Compiler* compiler, FunctionObject* function, uint16_t function_position,
    Vector* args, Vector* params, bool discard);
void compile_call_with_argument(Compiler* compiler, FunctionObject* function,
    uint16_t function_position, Operand argument, bool discard);
Operand compile_builtin_call(Compiler* compiler, ASTNode* ast, bool discard);
Operand compile_function_call(Compiler* compiler, ASTNode* ast, bool discard);
void compile_function_definition(Compiler* compiler, ASTNode* ast);
void compile_return_statement(Compiler* compiler, ASTNode* ast);

#endif
