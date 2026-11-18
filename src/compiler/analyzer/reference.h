#ifndef ANALYZER_REFERENCE_H
#define ANALYZER_REFERENCE_H

#include "compiler/analyzer/analyzer.h"

size_t* get_shared_access_count(ASTNode* symbol);
bool has_exclusive_access(ASTNode* symbol);
void set_exclusive_access(ASTNode* symbol, bool active);
ASTNode* get_reference_origin_or_self(ASTNode* symbol);
ASTNode* reference_origin_from_expression(ASTNode* ast);
Vector* reference_origins_from_expression(ASTNode* ast);
bool contains_reference_origin(Vector* origins, ASTNode* origin);
void add_reference_origin(Vector* origins, ASTNode* origin);
void copy_reference_origins(Vector* destination, Vector* source);
void merge_reference_origins(Vector* destination, Vector* source);
void set_reference_origins_from_expression(ASTNode* destination, ASTNode* expression);
void release_reference_access(ASTNode* reference);
void release_inactive_reference_access(Vector* references);
void analyze_reference(Analyzer* analyzer, ASTNode* ast);
void validate_call_argument_access(Analyzer* analyzer, ASTNode* caller, ASTNode* callee, Token token);
void validate_argument_accesses(ASTNode* caller, ASTNode* callee);
void set_call_reference_origins(ASTNode* caller, ASTNode* callee);
void transfer_exclusive_reference(ASTNode* expression);
void activate_reference_access(ASTNode* ast);
void establish_reference_access(ASTNode* binding, ASTNode* expression);
void update_reference_binding(ASTNode* ast, ASTNode* symbol, bool initializes_binding);

#endif
