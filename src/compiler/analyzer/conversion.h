#ifndef ANALYZER_CONVERSION_H
#define ANALYZER_CONVERSION_H

#include "compiler/ast/ast.h"

bool apply_integer_literal_type(ASTNode* expression, Type type);
void apply_literal_type(ASTNode* expression, Type type);
void apply_declared_type(ASTNode* expression, Type type, ReferenceType reference_type);
void infer_conversion_literal(ASTNode* ast);
void analyze_type_conversion(ASTNode* ast);

#endif
