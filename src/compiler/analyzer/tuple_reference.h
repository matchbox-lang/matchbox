#ifndef ANALYZER_TUPLE_REFERENCE_H
#define ANALYZER_TUPLE_REFERENCE_H

#include "compiler/analyzer/analyzer.h"

typedef struct TupleReference
{
    ASTNode* origin;
    ReferenceType reference;
    size_t offset;
    size_t origin_offset;
} TupleReference;

const Vector* get_tuple_references(Analyzer* analyzer, ASTNode* ast);
void clear_tuple_references(Analyzer* analyzer, ASTNode* ast);
void copy_tuple_reference_vector(Vector* destination, const Vector* source);
void free_tuple_reference_vector(Vector* references);
void add_tuple_reference(Analyzer* analyzer, ASTNode* ast, TupleReference access);
void merge_tuple_reference_vectors(Vector* destination, const Vector* source);
void remove_tuple_references(Analyzer* analyzer, ASTNode* ast, size_t start, size_t slots);
void copy_tuple_references(Analyzer* analyzer, ASTNode* destination, ASTNode* source, size_t start, size_t offset,
    size_t slots);
void set_tuple_reference_vector(Analyzer* analyzer, ASTNode* ast, const Vector* source);
void free_tuple_reference_states(Analyzer* analyzer);
void discard_transient_tuple_references(Analyzer* analyzer);

#endif
