#include "common/type.h"
#include "common/array_type.h"
#include <float.h>
#include <limits.h>

static bool accepts_numeric_type(TypeKind source, TypeKind destination);
static bool accepts_both_numeric_types(TypeKind left, TypeKind right, TypeKind destination);

const char* get_data_type_name(Type type)
{
    if (is_array_type(type.kind)) {
        const ArrayType* info = type.array;

        return info->name;
    }

    switch (type.kind) {
        case TypeI8:       return "i8";
        case TypeI16:      return "i16";
        case TypeI32:      return "i32";
        case TypeI64:      return "i64";
        case TypeU8:       return "u8";
        case TypeU16:      return "u16";
        case TypeU32:      return "u32";
        case TypeU64:      return "u64";
        case TypeIsize:    return "isize";
        case TypeUsize:    return "usize";
        case TypeF32:      return "f32";
        case TypeF64:      return "f64";
        case TypeChar:     return "char";
        case TypeString:   return "string";
        case TypeBool:     return "bool";
        case TypeVoid:     return "void";
        default:
            return "unknown token";
    }
}

bool is_float_type(TypeKind kind)
{
    return kind == TypeF32 || kind == TypeF64;
}

bool is_integer_type(TypeKind kind)
{
    return get_integer_type_size(kind) != 0;
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
    return kind == TypeI64 || kind == TypeU64 || kind == TypeF64 ? 2 : 1;
}

bool is_printable_type(TypeKind kind)
{
    return is_integer_type(kind) || is_float_type(kind) || kind == TypeBool || kind == TypeChar
        || kind == TypeString || is_array_type(kind);
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

bool can_implicitly_widen_float(TypeKind source, TypeKind destination)
{
    return source == TypeF32 && destination == TypeF64;
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
