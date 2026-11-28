#ifndef AST_BINDING_H
#define AST_BINDING_H

#include "common/vector.h"
#include <stdbool.h>

typedef struct ASTNode ASTNode;

typedef enum BindingType
{
    BindingMutable,
    BindingFixed,
    BindingConstant
} BindingType;

typedef enum BindingState
{
    BindingUninitialized,
    BindingPossiblyInitialized,
    BindingInitialized
} BindingState;

bool has_no_value_reads(ASTNode* ast);
void initialize_variable(ASTNode* ast);
BindingState get_binding_state(const ASTNode* ast);
bool is_initialized(const ASTNode* ast);
bool is_moved(ASTNode* symbol);
bool requires_variable_initializer(const ASTNode* ast);
void mark_address_taken(ASTNode* symbol);
void set_moved_state(ASTNode* symbol, bool moved);
void track_variable_read(ASTNode* symbol);
Vector* get_tuple_moves(ASTNode* binding);
bool tuple_moves_overlap(const Vector* moves, size_t start, size_t slots);
bool tuple_moves_include(const Vector* destination, const Vector* source);
void copy_tuple_moves(Vector* destination, const Vector* source);
void merge_tuple_moves(Vector* destination, const Vector* source);
void record_tuple_move(ASTNode* binding, size_t start, size_t slots);
void forget_tuple_moves(ASTNode* binding, size_t start, size_t slots);

#endif
