#include "common/type.h"
#include "common/array_type.h"
#include "common/tuple_type.h"
#include <float.h>
#include <limits.h>

static bool accepts_numeric_type(TypeKind source, TypeKind destination)
{
    return source == destination || can_implicitly_widen_type(source, destination);
}

static bool accepts_both_numeric_types(TypeKind left, TypeKind right, TypeKind destination)
{
    bool accepts_left = accepts_numeric_type(left, destination);
    bool accepts_right = accepts_numeric_type(right, destination);

    return accepts_left && accepts_right;
}

static size_t get_scalar_storage_size(TypeKind kind)
{
    switch (kind) {
        case TypeBool:
            return sizeof(uint8_t);
        case TypeChar:
        case TypeF32:
            return sizeof(uint32_t);
        case TypeF64:
            return sizeof(uint64_t);
        case TypeString:
            return sizeof(void*);
        default:
            return get_integer_type_size(kind);
    }
}

static const char* get_type_kind_name(TypeKind kind)
{
    switch (kind) {
        case TypeI8:            return "i8";
        case TypeI16:           return "i16";
        case TypeI32:           return "i32";
        case TypeI64:           return "i64";
        case TypeU8:            return "u8";
        case TypeU16:           return "u16";
        case TypeU32:           return "u32";
        case TypeU64:           return "u64";
        case TypeIsize:         return "isize";
        case TypeUsize:         return "usize";
        case TypeF32:           return "f32";
        case TypeF64:           return "f64";
        case TypeChar:          return "char";
        case TypeBool:          return "bool";
        case TypeVoid:          return "void";
        case TypeString:        return "string";
        case TypeStringSlice:   return "string view";
        case TypeTuple:          return "tuple";
        case TypeGeneric:       return "generic";
        default:
            return "unknown token";
    }
}

bool can_assign_type(Type source, Type destination, ReferenceType source_ref, ReferenceType dest_ref)
{
    if (types_equal(source, destination)) {
        return true;
    }

    if (source_ref != RefNone || dest_ref != RefNone) {
        return false;
    }

    return can_implicitly_widen_type(source.kind, destination.kind);
}

bool can_explicitly_convert_type(TypeKind source, TypeKind destination)
{
    bool source_numeric = is_integer_type(source) || is_float_type(source);
    bool destination_numeric = is_integer_type(destination) || is_float_type(destination);

    if (source == TypeBool && is_integer_type(destination)) {
        return true;
    }

    if (source_numeric && destination_numeric) {
        return true;
    }

    if (source == TypeChar) {
        return destination == TypeChar || is_integer_type(destination);
    }

    return is_integer_type(source) && destination == TypeChar;
}

bool can_implicitly_convert_integer_to_float(TypeKind source, TypeKind destination)
{
    if (!is_integer_type(source)) {
        return false;
    }

    size_t bits = get_integer_type_size(source) * CHAR_BIT;
    bool signed_type = is_signed_integer_type(source);
    size_t precision = 0;

    if (destination == TypeF32) {
        precision = FLT_MANT_DIG;
    }

    if (destination == TypeF64) {
        precision = DBL_MANT_DIG;
    }

    return bits - signed_type <= precision;
}

bool can_implicitly_widen_float(TypeKind source, TypeKind destination)
{
    return source == TypeF32 && destination == TypeF64;
}

bool can_implicitly_widen_integer(TypeKind source, TypeKind destination)
{
    if (!is_integer_type(source) || !is_integer_type(destination)) {
        return false;
    }

    if (is_signed_integer_type(source) && !is_signed_integer_type(destination)) {
        return false;
    }

    size_t source_size = get_integer_type_size(source);
    size_t destination_size = get_integer_type_size(destination);
    bool same_sign = is_signed_integer_type(source) == is_signed_integer_type(destination);

    return source_size < destination_size || (source_size == destination_size && same_sign);
}

bool can_implicitly_widen_type(TypeKind source, TypeKind destination)
{
    if (can_implicitly_widen_integer(source, destination)) {
        return true;
    }

    if (can_implicitly_convert_integer_to_float(source, destination)) {
        return true;
    }

    return can_implicitly_widen_float(source, destination);
}

size_t get_implicit_conversion_cost(TypeKind source, TypeKind destination)
{
    if (source == destination) {
        return 0;
    }

    if (!can_implicitly_widen_type(source, destination)) {
        return SIZE_MAX;
    }

    if (is_integer_type(source) && is_integer_type(destination)) {
        return get_integer_type_size(destination);
    }

    if (is_float_type(source)) {
        return 1;
    }

    return MAX_IMPLICIT_CONVERSION_COST;
}

TypeKind get_common_numeric_type(TypeKind left, TypeKind right)
{
    bool left_numeric = is_integer_type(left) || is_float_type(left);
    bool right_numeric = is_integer_type(right) || is_float_type(right);

    if (!left_numeric || !right_numeric) {
        return TypeUnknown;
    }

    if (accepts_numeric_type(left, right)) {
        return right;
    }

    if (accepts_numeric_type(right, left)) {
        return left;
    }

    static const TypeKind candidates[] = {
        TypeI8, TypeI16, TypeI32, TypeI64, TypeU8, TypeU16, TypeU32, TypeU64, TypeF32, TypeF64
    };

    size_t count = sizeof(candidates) / sizeof(*candidates);
    size_t index = 0;

    while (index < count && !accepts_both_numeric_types(left, right, candidates[index])) {
        index++;
    }

    if (index == count) {
        return TypeUnknown;
    }

    return candidates[index];
}

const char* get_data_type_name(Type type)
{
    if (is_contiguous_type(type.kind) && type.kind != TypeStringSlice) {
        const ArrayType* info = type.array;

        return info->name;
    }

    return get_type_kind_name(type.kind);
}

size_t get_integer_type_size(TypeKind kind)
{
    switch (kind) {
        case TypeI8:
        case TypeU8:
            return 1;
        case TypeI16:
        case TypeU16:
            return 2;
        case TypeI32:
        case TypeU32:
            return 4;
        case TypeI64:
        case TypeU64:
            return 8;
        case TypeIsize:
        case TypeUsize:
            return sizeof(void*);
        default:
            return 0;
    }
}

size_t get_type_slot_count(TypeKind kind)
{
    return kind == TypeI64 || kind == TypeU64 || kind == TypeF64 || is_slice_type(kind) ? 2 : 1;
}

size_t get_value_slot_count(Type type)
{
    if (type.kind == TypeTuple) {
        return type.tuple->slots;
    }

    return get_type_slot_count(type.kind);
}

size_t get_type_storage_size(Type type)
{
    if (type.kind == TypeTuple) {
        return type.tuple->slots * sizeof(void*);
    }

    if (!is_contiguous_type(type.kind)) {
        return get_scalar_storage_size(type.kind);
    }

    const ArrayType* info = type.array;

    return info->size;
}

bool integer_literal_fits_float(uint64_t value, Type type)
{
    size_t precision;

    if (type.kind == TypeF32) {
        precision = FLT_MANT_DIG;
    } else if (type.kind == TypeF64) {
        precision = DBL_MANT_DIG;
    } else {
        return false;
    }

    size_t bits = 0;
    uint64_t remaining = value;

    while (remaining) {
        bits++;
        remaining >>= 1;
    }

    if (bits <= precision) {
        return true;
    }

    size_t discarded_bits = bits - precision;
    uint64_t mask = (UINT64_C(1) << discarded_bits) - 1;

    return (value & mask) == 0;
}

bool integer_literal_fits_type(uint64_t value, bool negative, Type type)
{
    size_t bits = get_integer_type_size(type.kind) * CHAR_BIT;

    if (!bits || (!is_signed_integer_type(type.kind) && negative)) {
        return false;
    }

    if (!is_signed_integer_type(type.kind)) {
        return bits == 64 || value <= (UINT64_C(1) << bits) - 1;
    }

    uint64_t limit = UINT64_C(1) << (bits - 1);

    return negative ? value <= limit : value < limit;
}

bool is_array_type(TypeKind kind)
{
    return kind == TypeArray;
}

bool is_contiguous_type(TypeKind kind)
{
    return is_array_type(kind) || is_slice_type(kind);
}

bool is_float_type(TypeKind kind)
{
    return kind == TypeF32 || kind == TypeF64;
}

bool is_integer_type(TypeKind kind)
{
    return get_integer_type_size(kind) != 0;
}

bool is_printable_type(TypeKind kind)
{
    switch (kind) {
        case TypeUnknown:
        case TypeVoid:
        case TypeGeneric:
        case TypeNamed:
            return false;
        default:
            return true;
    }
}

bool is_signed_integer_type(TypeKind kind)
{
    switch (kind) {
        case TypeI8:
        case TypeI16:
        case TypeI32:
        case TypeI64:
        case TypeIsize:
            return true;
        default:
            return false;
    }
}

bool is_slice_type(TypeKind kind)
{
    return kind == TypeArraySlice || kind == TypeStringSlice;
}

bool types_equal(Type left, Type right)
{
    if (left.kind != right.kind) {
        return false;
    }

    if (left.kind == TypeTuple) {
        return tuple_types_equal(left.tuple, right.tuple);
    }

    if (is_contiguous_type(left.kind)) {
        return left.array == right.array;
    }

    if (left.kind == TypeGeneric) {
        return left.generic_index == right.generic_index;
    }

    return true;
}
