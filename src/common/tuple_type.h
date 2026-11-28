#ifndef TUPLE_TYPE_H
#define TUPLE_TYPE_H

#include "common/type.h"

typedef struct TupleElement
{
    Type type;
    ReferenceType reference;
    size_t offset;
} TupleElement;

typedef struct TupleType
{
    size_t count;
    size_t slots;
    bool resolved;
    TupleElement elements[];
} TupleType;

TupleType* create_tuple_type(size_t count);
TupleElement get_tuple_operand_element(Type type, size_t index);
void layout_tuple_type(TupleType* tuple);
size_t get_tuple_element_slot_count(TupleElement element);
bool tuple_types_equal(const TupleType* left, const TupleType* right);
bool tuple_shapes_equal(Type left, Type right);
bool is_owned_type(Type type);
bool has_exclusive_tuple_reference(Type type);

#endif
