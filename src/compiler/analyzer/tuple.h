#ifndef ANALYZER_TUPLE_H
#define ANALYZER_TUPLE_H

#include "common/tuple_type.h"
#include "compiler/analyzer/analyzer.h"

void resolve_tuple_type(Analyzer* analyzer, Type* type);
void analyze_tuple(Analyzer* analyzer, ASTNode* ast);
void analyze_tuple_index(Analyzer* analyzer, ASTNode* ast, Type type, uint64_t value);
void analyze_tuple_index_value(Analyzer* analyzer, ASTNode* ast);
bool consume_tuple_element(Analyzer* analyzer, ASTNode* ast);
void validate_tuple_projection(ASTNode* ast);
void analyze_tuple_copy(Analyzer* analyzer, ASTNode* ast, ASTNode* argument, Token token);
bool apply_tuple_move_effects(Analyzer* analyzer, ASTNode* symbol, const FunctionArrayEffects* effects);
void collect_tuple_element_references(Analyzer* analyzer, ASTNode* tuple, ASTNode* expression, TupleElement element);
void set_tuple_binding_references(Analyzer* analyzer, ASTNode* binding, ASTNode* expression);
void activate_tuple_access(Analyzer* analyzer, ASTNode* binding);
void release_tuple_access(Analyzer* analyzer, ASTNode* binding);
void track_tuple_return(Analyzer* analyzer, ASTNode* ast);
void set_tuple_call_references(Analyzer* analyzer, ASTNode* caller, ASTNode* callee);
void initialize_tuple_parameter(Analyzer* analyzer, ASTNode* parameter, Type type, size_t offset);

#endif
