#ifndef ANALYZER_REFERENCE_H
#define ANALYZER_REFERENCE_H

#include "compiler/analyzer/analyzer.h"

void set_reference_origins_from_expression(ASTNode* destination, ASTNode* expression);
void release_reference_access(ASTNode* reference);
void release_inactive_reference_access(Vector* references);
void analyze_reference(Analyzer* analyzer, ASTNode* ast);
void validate_reference_binding_access(ASTNode* symbol, ReferenceType requested, Token token);
void validate_call_argument_access(Analyzer* analyzer, ASTNode* caller, ASTNode* callee, Token token);
void validate_argument_accesses(ASTNode* caller, ASTNode* callee);
void set_call_reference_origins(ASTNode* caller, ASTNode* callee);
void transfer_exclusive_reference(ASTNode* expression);
void activate_reference_access(ASTNode* ast);
void establish_reference_access(ASTNode* binding, ASTNode* expression);
void update_reference_binding(ASTNode* ast, ASTNode* symbol, bool initializes_binding);

#endif
