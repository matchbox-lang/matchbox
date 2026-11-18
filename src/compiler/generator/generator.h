#ifndef GENERATOR_H
#define GENERATOR_H

#include "compiler/generator/operand.h"

_Noreturn void global_index_overflow_error(void);
Operand compile_reference_expression(Compiler* compiler, ASTNode* ast);
Operand compile_expression(Compiler* compiler, ASTNode* ast, bool discard);
Operand compile_statement(Compiler* compiler, ASTNode* ast, bool discard);
void compile_blocklevel_statements(Compiler* compiler, Vector* nodes);
void compile_top_level_statements(Compiler* compiler, Vector* nodes);
void compile_repl_statements(Compiler* compiler, Vector* nodes);

#endif
