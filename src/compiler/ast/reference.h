#ifndef AST_REFERENCE_H
#define AST_REFERENCE_H

#include "compiler/ast/ast.h"

void add_reference_origin(Vector* origins, ASTNode* origin);
void apply_slice_type(ASTNode* ast, Type type, ReferenceType reference_type);
bool contains_reference_origin(Vector* origins, ASTNode* origin);
void convert_string_reference_to_slice(ASTNode* ast, Type type);
void copy_reference_origins(Vector* destination, Vector* source);
ASTNode* get_argument_reference_origin(ASTNode* argument);
ASTNode* get_reference_origin(const ASTNode* ast);
ASTNode* get_reference_origin_from_expression(ASTNode* ast);
ASTNode* get_reference_origin_or_self(ASTNode* symbol);
Vector* get_reference_origins(ASTNode* ast);
Vector* get_reference_origins_from_expression(ASTNode* ast);
ReferenceType get_reference_type(const ASTNode* ast);
size_t* get_shared_access_count(ASTNode* symbol);
bool has_exclusive_access(ASTNode* symbol);
bool has_value_references(ASTNode* ast);
void merge_reference_origins(Vector* destination, Vector* source);
void set_exclusive_access(ASTNode* symbol, bool active);
void set_expression_reference_type(ASTNode* ast, ReferenceType type);
Vector* get_tuple_references(ASTNode* ast);
void clear_tuple_references(ASTNode* ast);
void copy_tuple_reference_vector(Vector* destination, const Vector* source);
void free_tuple_reference_vector(Vector* references);
void add_tuple_reference(ASTNode* ast, TupleReference access);
void merge_tuple_reference_vectors(Vector* destination, const Vector* source);
void remove_tuple_references(ASTNode* ast, size_t start, size_t slots);
void copy_tuple_references(ASTNode* destination, ASTNode* source, size_t start, size_t offset, size_t slots);

#endif
