#ifndef TYPE_H
#define TYPE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_IMPLICIT_CONVERSION_COST 16

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
    TypeStringSlice,
    TypeBool,
    TypeVoid,
    TypeTuple,
    TypeArray,
    TypeArraySlice,
    TypeGeneric,
    TypeNamed,
} TypeKind;

typedef enum ReferenceType
{
    RefShared,
    RefExclusive,
    RefNone
} ReferenceType;

typedef struct TupleType TupleType;
typedef struct ArrayType ArrayType;
typedef struct NamedType NamedType;

typedef struct Type
{
    TypeKind kind;

    union {
        const ArrayType* array;
        const TupleType* tuple;
        const NamedType* named;
        size_t generic_index;
    };
} Type;

bool can_explicitly_convert_type(TypeKind source, TypeKind destination);
bool can_implicitly_convert_integer_to_float(TypeKind source, TypeKind destination);
bool can_implicitly_widen_float(TypeKind source, TypeKind destination);
bool can_implicitly_widen_integer(TypeKind source, TypeKind destination);
bool can_implicitly_widen_type(TypeKind source, TypeKind destination);
size_t get_implicit_conversion_cost(TypeKind source, TypeKind destination);
TypeKind get_common_numeric_type(TypeKind left, TypeKind right);
const char* get_data_type_name(Type type);
size_t get_integer_type_size(TypeKind kind);
size_t get_type_slot_count(TypeKind kind);
size_t get_value_slot_count(Type type);
size_t get_type_storage_size(Type type);
bool integer_literal_fits_float(uint64_t value, Type type);
bool integer_literal_fits_type(uint64_t value, bool negative, Type type);
bool is_array_type(TypeKind kind);
bool is_contiguous_type(TypeKind kind);
bool is_float_type(TypeKind kind);
bool is_integer_type(TypeKind kind);
bool is_printable_type(TypeKind kind);
bool is_signed_integer_type(TypeKind kind);
bool is_slice_type(TypeKind kind);
bool types_equal(Type left, Type right);

#endif
