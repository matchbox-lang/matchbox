#ifndef GENERATOR_OPERAND_H
#define GENERATOR_OPERAND_H

#include "compiler/compiler.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct Operand
{
    int reg;
    bool temporary;
    size_t slots;
} Operand;

_Noreturn void function_register_index_overflow_error(const char* name);
ASTNode* get_current_function_definition(Compiler* compiler);
void check_register_count(Compiler* compiler, int count);
bool is_compiling_top_level(Compiler* compiler);
int allocate_register(Compiler* compiler);
int release_register(Compiler* compiler);
int allocate_registers(Compiler* compiler, size_t count);
Operand make_operand(int reg, bool temporary);
Operand make_wide_operand(int reg, bool temporary);
Operand no_operand(void);
void release_operand(Compiler* compiler, Operand operand);
Operand materialize_operand(Compiler* compiler, Operand operand);
size_t get_node_slot_count(ASTNode* ast);
Operand dereference_operand(Compiler* compiler, Operand operand, size_t slots);
int select_binary_destination(Compiler* compiler, Operand left, Operand right, size_t slots);

#endif
