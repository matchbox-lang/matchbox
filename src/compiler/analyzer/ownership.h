#ifndef ANALYZER_OWNERSHIP_H
#define ANALYZER_OWNERSHIP_H

#include "compiler/analyzer/analyzer.h"

typedef struct OwnershipState
{
    ASTNode* symbol;
    Vector reference_origins;
    size_t shared_access_count;
    bool exclusive_access_active;
    bool initialized;
    bool possibly_initialized;
    bool moved;
    bool reference_access_active;
} OwnershipState;

bool is_moved(ASTNode* symbol);
void set_moved_state(ASTNode* symbol, bool moved);
void consume_array(ASTNode* ast);
void capture_ownership_states(Vector* states, Scope* scope);
void copy_ownership_states(Vector* destination, Vector* source);
void apply_ownership_states(Vector* states);
void update_ownership_states(Vector* states);
void free_ownership_states(Vector* states);
void release_references_dead_on_branch(Analyzer* analyzer, Vector* states, ASTNode* branch, ASTNode* binding);
void analyze_ownership_branch(Analyzer* analyzer, Vector* states, ASTNode* branch,
    Type expected_type, Type* type, bool moves_result);
void merge_ownership_states(Vector* then_states, Vector* else_states);
void validate_loop_ownership(Vector* entry_states, Token token);
void merge_ownership_outcome(Vector* merged_states, Vector* states, bool* initialized);
void record_function_array_moves(ASTNode* function, Vector* states);
void record_array_return_initializations(Analyzer* analyzer);

#endif
