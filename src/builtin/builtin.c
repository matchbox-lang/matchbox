#include "builtin/builtin.h"
#include "builtin/io.h"
#include "builtin/math.h"
#include "builtin/string.h"
#include "builtin/system.h"
#include "common/type.h"
#include <string.h>

static const Builtin builtins[BUILTINS_MAX] = {
    // I/O
    {"input",       builtin_input,          0,  {},                                     TypeString, false},
    {"print",       builtin_print,         -1,  {TypeUnknown},                          TypeVoid,   false},

    // Math                                                                             
    {"abs",         builtin_abs_i32,        1,  {TypeI32},                              TypeI32,    true},
    {"abs",         builtin_abs_i64,        1,  {TypeI64},                              TypeI64,    true},
    {"abs",         builtin_abs_f32,        1,  {TypeF32},                              TypeF32,    true},
    {"abs",         builtin_abs_f64,        1,  {TypeF64},                              TypeF64,    true},
    {"acos",        builtin_acos_f32,       1,  {TypeF32},                              TypeF32,    true},
    {"acos",        builtin_acos_f64,       1,  {TypeF64},                              TypeF64,    true},
    {"acosh",       builtin_acosh_f32,      1,  {TypeF32},                              TypeF32,    true},
    {"acosh",       builtin_acosh_f64,      1,  {TypeF64},                              TypeF64,    true},
    {"asin",        builtin_asin_f32,       1,  {TypeF32},                              TypeF32,    true},
    {"asin",        builtin_asin_f64,       1,  {TypeF64},                              TypeF64,    true},
    {"asinh",       builtin_asinh_f32,      1,  {TypeF32},                              TypeF32,    true},
    {"asinh",       builtin_asinh_f64,      1,  {TypeF64},                              TypeF64,    true},
    {"atan",        builtin_atan_f32,       1,  {TypeF32},                              TypeF32,    true},
    {"atan",        builtin_atan_f64,       1,  {TypeF64},                              TypeF64,    true},
    {"atan2",       builtin_atan2_f32,      2,  {TypeF32, TypeF32},                     TypeF32,    true},
    {"atan2",       builtin_atan2_f64,      2,  {TypeF64, TypeF64},                     TypeF64,    true},
    {"atanh",       builtin_atanh_f32,      1,  {TypeF32},                              TypeF32,    true},
    {"atanh",       builtin_atanh_f64,      1,  {TypeF64},                              TypeF64,    true},
    {"avg",         builtin_avg_f32,       -2,  {TypeF32},                              TypeF32,    true},
    {"avg",         builtin_avg_f64,       -2,  {TypeF64},                              TypeF64,    true},
    {"cbrt",        builtin_cbrt_f32,       1,  {TypeF32},                              TypeF32,    true},
    {"cbrt",        builtin_cbrt_f64,       1,  {TypeF64},                              TypeF64,    true},
    {"ceil",        builtin_ceil_f32,       1,  {TypeF32},                              TypeF32,    true},
    {"ceil",        builtin_ceil_f64,       1,  {TypeF64},                              TypeF64,    true},
    {"clamp",       builtin_clamp_i32,      3,  {TypeI32, TypeI32, TypeI32},            TypeI32,    true},
    {"clamp",       builtin_clamp_i64,      3,  {TypeI64, TypeI64, TypeI64},            TypeI64,    true},
    {"clamp",       builtin_clamp_f32,      3,  {TypeF32, TypeF32, TypeF32},            TypeF32,    true},
    {"clamp",       builtin_clamp_f64,      3,  {TypeF64, TypeF64, TypeF64},            TypeF64,    true},
    {"cos",         builtin_cos_f32,        1,  {TypeF32},                              TypeF32,    true},
    {"cos",         builtin_cos_f64,        1,  {TypeF64},                              TypeF64,    true},
    {"cosh",        builtin_cosh_f32,       1,  {TypeF32},                              TypeF32,    true},
    {"cosh",        builtin_cosh_f64,       1,  {TypeF64},                              TypeF64,    true},
    {"deg",         builtin_deg_f32,        1,  {TypeF32},                              TypeF32,    true},
    {"deg",         builtin_deg_f64,        1,  {TypeF64},                              TypeF64,    true},
    {"exp",         builtin_exp_f32,        1,  {TypeF32},                              TypeF32,    true},
    {"exp",         builtin_exp_f64,        1,  {TypeF64},                              TypeF64,    true},
    {"floor",       builtin_floor_f32,      1,  {TypeF32},                              TypeF32,    true},
    {"floor",       builtin_floor_f64,      1,  {TypeF64},                              TypeF64,    true},
    {"hypot",       builtin_hypot_f32,      2,  {TypeF32, TypeF32},                     TypeF32,    true},
    {"hypot",       builtin_hypot_f64,      2,  {TypeF64, TypeF64},                     TypeF64,    true},
    {"ilerp",       builtin_ilerp_f32,      3,  {TypeF32, TypeF32, TypeF32},            TypeF32,    true},
    {"ilerp",       builtin_ilerp_f64,      3,  {TypeF64, TypeF64, TypeF64},            TypeF64,    true},
    {"ldexp",       builtin_ldexp_f32,      2,  {TypeF32, TypeI32},                     TypeF32,    true},
    {"ldexp",       builtin_ldexp_f64,      2,  {TypeF64, TypeI32},                     TypeF64,    true},
    {"lerp",        builtin_lerp_f32,       3,  {TypeF32, TypeF32, TypeF32},            TypeF32,    true},
    {"lerp",        builtin_lerp_f64,       3,  {TypeF64, TypeF64, TypeF64},            TypeF64,    true},
    {"log",         builtin_log_f32,        1,  {TypeF32},                              TypeF32,    true},
    {"log",         builtin_log_f64,        1,  {TypeF64},                              TypeF64,    true},
    {"log10",       builtin_log10_f32,      1,  {TypeF32},                              TypeF32,    true},
    {"log10",       builtin_log10_f64,      1,  {TypeF64},                              TypeF64,    true},
    {"log2",        builtin_log2_f32,       1,  {TypeF32},                              TypeF32,    true},
    {"log2",        builtin_log2_f64,       1,  {TypeF64},                              TypeF64,    true},
    {"max",         builtin_max_i32,       -2,  {TypeI32},                              TypeI32,    true},
    {"max",         builtin_max_i64,       -2,  {TypeI64},                              TypeI64,    true},
    {"max",         builtin_max_u32,       -2,  {TypeU32},                              TypeU32,    true},
    {"max",         builtin_max_u64,       -2,  {TypeU64},                              TypeU64,    true},
    {"max",         builtin_max_f32,       -2,  {TypeF32},                              TypeF32,    true},
    {"max",         builtin_max_f64,       -2,  {TypeF64},                              TypeF64,    true},
    {"min",         builtin_min_i32,       -2,  {TypeI32},                              TypeI32,    true},
    {"min",         builtin_min_i64,       -2,  {TypeI64},                              TypeI64,    true},
    {"min",         builtin_min_u32,       -2,  {TypeU32},                              TypeU32,    true},
    {"min",         builtin_min_u64,       -2,  {TypeU64},                              TypeU64,    true},
    {"min",         builtin_min_f32,       -2,  {TypeF32},                              TypeF32,    true},
    {"min",         builtin_min_f64,       -2,  {TypeF64},                              TypeF64,    true},
    {"pow",         builtin_pow_i32,        2,  {TypeI32, TypeI32},                     TypeI32,    true},
    {"pow",         builtin_pow_i64,        2,  {TypeI64, TypeI64},                     TypeI64,    true},
    {"pow",         builtin_pow_f32,        2,  {TypeF32, TypeF32},                     TypeF32,    true},
    {"pow",         builtin_pow_f64,        2,  {TypeF64, TypeF64},                     TypeF64,    true},
    {"rad",         builtin_rad_f32,        1,  {TypeF32},                              TypeF32,    true},
    {"rad",         builtin_rad_f64,        1,  {TypeF64},                              TypeF64,    true},
    {"sign",        builtin_sign_i32,       1,  {TypeI32},                              TypeI32,    true},
    {"sign",        builtin_sign_i64,       1,  {TypeI64},                              TypeI64,    true},
    {"sign",        builtin_sign_f32,       1,  {TypeF32},                              TypeF32,    true},
    {"sign",        builtin_sign_f64,       1,  {TypeF64},                              TypeF64,    true},
    {"sin",         builtin_sin_f32,        1,  {TypeF32},                              TypeF32,    true},
    {"sin",         builtin_sin_f64,        1,  {TypeF64},                              TypeF64,    true},
    {"sinh",        builtin_sinh_f32,       1,  {TypeF32},                              TypeF32,    true},
    {"sinh",        builtin_sinh_f64,       1,  {TypeF64},                              TypeF64,    true},
    {"sqrt",        builtin_sqrt_f32,       1,  {TypeF32},                              TypeF32,    true},
    {"sqrt",        builtin_sqrt_f64,       1,  {TypeF64},                              TypeF64,    true},
    {"sum",         builtin_sum_i32,       -2,  {TypeI32},                              TypeI32,    true},
    {"sum",         builtin_sum_i64,       -2,  {TypeI64},                              TypeI64,    true},
    {"sum",         builtin_sum_u32,       -2,  {TypeU32},                              TypeU32,    true},
    {"sum",         builtin_sum_u64,       -2,  {TypeU64},                              TypeU64,    true},
    {"sum",         builtin_sum_f32,       -2,  {TypeF32},                              TypeF32,    true},
    {"sum",         builtin_sum_f64,       -2,  {TypeF64},                              TypeF64,    true},
    {"tan",         builtin_tan_f32,        1,  {TypeF32},                              TypeF32,    true},
    {"tan",         builtin_tan_f64,        1,  {TypeF64},                              TypeF64,    true},
    {"tanh",        builtin_tanh_f32,       1,  {TypeF32},                              TypeF32,    true},
    {"tanh",        builtin_tanh_f64,       1,  {TypeF64},                              TypeF64,    true},

    // String
    {"string",      builtin_string,        -1,  {TypeUnknown},                          TypeString, true},

    // System
    {"assert",      builtin_assert,         1,  {TypeBool},                             TypeVoid,   false},
    {"assert",      builtin_assert_message, 2,  {TypeBool, TypeString},                 TypeVoid,   false},
    {"byteorder",   builtin_byteorder,      0,  {},                                     TypeI32,    true},
    {"exit",        builtin_exit,           0,  {},                                     TypeVoid,   false},
    {"exit",        builtin_exit_status,    1,  {TypeI32},                              TypeVoid,   false},
    {"panic",       builtin_panic,          1,  {TypeString},                           TypeVoid,   false}
};

static bool builtin_argument_matches(const Builtin* builtin, const Type* argument_types, size_t position)
{
    TypeKind argument_type = argument_types[position].kind;
    TypeKind parameter_type = builtin->params[position];

    return argument_type == TypeUnknown
        || argument_type == parameter_type
        || can_implicitly_widen_type(argument_type, parameter_type);
}

static bool typed_variadic_matches(
    const Builtin* builtin, const Type* argument_types, size_t argument_count, bool exact)
{
    if (!argument_count || !argument_types) {
        return false;
    }

    bool matches = true;

    for (size_t i = 0; i < argument_count && matches; i++) {
        TypeKind argument_type = argument_types[i].kind;
        TypeKind parameter_type = builtin->params[0];
        matches = argument_type == parameter_type
            || (!exact && can_implicitly_widen_type(argument_type, parameter_type));
    }

    return matches;
}

static bool builtin_matches(const Builtin* builtin, const Type* argument_types, size_t argument_count)
{
    if (builtin->param_count == BUILTIN_VARIADIC) {
        return true;
    }

    if (builtin->param_count == BUILTIN_TYPED_VARIADIC) {
        return typed_variadic_matches(builtin, argument_types, argument_count, false);
    }

    if ((size_t)builtin->param_count != argument_count) {
        return false;
    }

    for (size_t i = 0; i < argument_count; i++) {
        if (!builtin_argument_matches(builtin, argument_types, i)) {
            return false;
        }
    }

    return true;
}

static bool builtin_call_matches(
    const Builtin* builtin, const char* name, const Type* argument_types, size_t argument_count)
{
    return strcmp(name, builtin->name) == 0
        && builtin_matches(builtin, argument_types, argument_count);
}

static const Builtin* match_exact_typed_variadic_builtin(
    const Builtin* builtin, const Type* argument_types, size_t argument_count)
{
    bool matches = typed_variadic_matches(builtin, argument_types, argument_count, true);

    if (!matches) {
        return NULL;
    }

    return builtin;
}

static const Builtin* match_exact_builtin(
    const Builtin* builtin, const char* name, const Type* argument_types, size_t argument_count)
{
    if (strcmp(name, builtin->name) != 0) {
        return NULL;
    }

    if (builtin->param_count == BUILTIN_TYPED_VARIADIC) {
        return match_exact_typed_variadic_builtin(builtin, argument_types, argument_count);
    }

    if ((size_t)builtin->param_count != argument_count) {
        return NULL;
    }

    bool matches = true;

    for (size_t i = 0; i < argument_count && matches; i++) {
        matches = argument_types[i].kind == builtin->params[i];
    }

    if (!matches) {
        return NULL;
    }

    return builtin;
}

static const Builtin* resolve_exact_builtin(const char* name, const Type* argument_types, size_t argument_count)
{
    const Builtin* exact = NULL;

    for (int i = 0; i < BUILTINS_MAX && !exact; i++) {
        exact = match_exact_builtin(&builtins[i], name, argument_types, argument_count);
    }

    return exact;
}

const Builtin* resolve_builtin(const char* name, const Type* argument_types, size_t argument_count)
{
    const Builtin* exact = resolve_exact_builtin(name, argument_types, argument_count);

    if (exact) {
        return exact;
    }

    const Builtin* variadic = NULL;

    for (int i = 0; i < BUILTINS_MAX; i++) {
        const Builtin* builtin = &builtins[i];

        if (!builtin_call_matches(builtin, name, argument_types, argument_count)) {
            continue;
        }

        if (builtin->param_count == BUILTIN_VARIADIC) {
            variadic = builtin;

            continue;
        }

        return builtin;
    }

    return variadic;
}

bool is_builtin_name(const char* name)
{
    for (int i = 0; i < BUILTINS_MAX; i++) {
        if (strcmp(name, builtins[i].name) == 0) {
            return true;
        }
    }

    return false;
}
