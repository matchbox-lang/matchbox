#ifndef ANALYZER_CONVERSION_H
#define ANALYZER_CONVERSION_H

#include "compiler/ast.h"

bool integer_literal_fits_type(uint64_t value, bool negative, Type type);
bool integer_literal_fits_float(uint64_t value, Type type);
bool apply_integer_literal_type(ASTNode* expression, Type type);
void apply_literal_type(ASTNode* expression, Type type);
void apply_declared_type(ASTNode* expression, Type type, ReferenceType reference_type);
void infer_conversion_literal(ASTNode* ast);
void analyze_type_conversion(ASTNode* ast);

#endif
