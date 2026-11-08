#ifndef TYPE_H
#define TYPE_H

#include <stdbool.h>
#include <stddef.h>

typedef enum TypeKind
{
    TypeUnknown,
    TypeI8,
    TypeI16,
    TypeI32,
    TypeI64,
    TypeU8,
    TypeU16,
    TypeU32,
    TypeU64,
    TypeIsize,
    TypeUsize,
    TypeF32,
    TypeF64,
    TypeChar,
    TypeString,
    TypeBool,
    TypeVoid,
    TypeArray
} TypeKind;

typedef struct ArrayType ArrayType;

typedef struct Type
{
    TypeKind kind;

    union {
        const ArrayType* array;
    };
} Type;

const char* get_data_type_name(Type type);
bool is_integer_type(TypeKind kind);
bool is_float_type(TypeKind kind);
bool is_signed_integer_type(TypeKind kind);
size_t get_integer_type_size(TypeKind kind);
size_t get_type_slot_count(TypeKind kind);
bool is_printable_type(TypeKind kind);
bool can_implicitly_widen_integer(TypeKind source, TypeKind destination);
bool can_implicitly_convert_integer_to_float(TypeKind source, TypeKind destination);
bool can_implicitly_widen_float(TypeKind source, TypeKind destination);
bool can_implicitly_widen_type(TypeKind source, TypeKind destination);
bool can_explicitly_convert_type(TypeKind source, TypeKind destination);
TypeKind get_common_numeric_type(TypeKind left, TypeKind right);
bool types_equal(Type left, Type right);

#endif
