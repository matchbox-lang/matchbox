#include "builtin/builtin.h"
#include "builtin/io.h"
#include "builtin/math.h"
#include "builtin/string.h"
#include "builtin/system.h"
#include "common/data_type.h"
#include <string.h>

static const Builtin builtins[BUILTINS_MAX] = {
    // I/O
    {"print",       builtin_print,         -1,  {TypeUnknown},                          TypeVoid},

    // Math                                                                             
    {"abs",         builtin_abs_i32,        1,  {TypeI32},                              TypeI32},
    {"abs",         builtin_abs_i64,        1,  {TypeI64},                              TypeI64},
    {"abs",         builtin_abs_f32,        1,  {TypeF32},                              TypeF32},
    {"abs",         builtin_abs_f64,        1,  {TypeF64},                              TypeF64},
    {"acos",        builtin_acos_f32,       1,  {TypeF32},                              TypeF32},
    {"acos",        builtin_acos_f64,       1,  {TypeF64},                              TypeF64},
    {"acosh",       builtin_acosh_f32,      1,  {TypeF32},                              TypeF32},
    {"acosh",       builtin_acosh_f64,      1,  {TypeF64},                              TypeF64},
    {"asin",        builtin_asin_f32,       1,  {TypeF32},                              TypeF32},
    {"asin",        builtin_asin_f64,       1,  {TypeF64},                              TypeF64},
    {"asinh",       builtin_asinh_f32,      1,  {TypeF32},                              TypeF32},
    {"asinh",       builtin_asinh_f64,      1,  {TypeF64},                              TypeF64},
    {"atan",        builtin_atan_f32,       1,  {TypeF32},                              TypeF32},
    {"atan",        builtin_atan_f64,       1,  {TypeF64},                              TypeF64},
    {"atan2",       builtin_atan2_f32,      2,  {TypeF32, TypeF32},                     TypeF32},
    {"atan2",       builtin_atan2_f64,      2,  {TypeF64, TypeF64},                     TypeF64},
    {"atanh",       builtin_atanh_f32,      1,  {TypeF32},                              TypeF32},
    {"atanh",       builtin_atanh_f64,      1,  {TypeF64},                              TypeF64},
    {"avg",         builtin_avg_f32,       -2,  {TypeF32},                              TypeF32},
    {"avg",         builtin_avg_f64,       -2,  {TypeF64},                              TypeF64},
    {"cbrt",        builtin_cbrt_f32,       1,  {TypeF32},                              TypeF32},
    {"cbrt",        builtin_cbrt_f64,       1,  {TypeF64},                              TypeF64},
    {"ceil",        builtin_ceil_f32,       1,  {TypeF32},                              TypeF32},
    {"ceil",        builtin_ceil_f64,       1,  {TypeF64},                              TypeF64},
    {"clamp",       builtin_clamp_i32,      3,  {TypeI32, TypeI32, TypeI32},            TypeI32},
    {"clamp",       builtin_clamp_i64,      3,  {TypeI64, TypeI64, TypeI64},            TypeI64},
    {"clamp",       builtin_clamp_f32,      3,  {TypeF32, TypeF32, TypeF32},            TypeF32},
    {"clamp",       builtin_clamp_f64,      3,  {TypeF64, TypeF64, TypeF64},            TypeF64},
    {"cos",         builtin_cos_f32,        1,  {TypeF32},                              TypeF32},
    {"cos",         builtin_cos_f64,        1,  {TypeF64},                              TypeF64},
    {"cosh",        builtin_cosh_f32,       1,  {TypeF32},                              TypeF32},
    {"cosh",        builtin_cosh_f64,       1,  {TypeF64},                              TypeF64},
    {"deg",         builtin_deg_f32,        1,  {TypeF32},                              TypeF32},
    {"deg",         builtin_deg_f64,        1,  {TypeF64},                              TypeF64},
    {"exp",         builtin_exp_f32,        1,  {TypeF32},                              TypeF32},
    {"exp",         builtin_exp_f64,        1,  {TypeF64},                              TypeF64},
    {"floor",       builtin_floor_f32,      1,  {TypeF32},                              TypeF32},
    {"floor",       builtin_floor_f64,      1,  {TypeF64},                              TypeF64},
    {"hypot",       builtin_hypot_f32,      2,  {TypeF32, TypeF32},                     TypeF32},
    {"hypot",       builtin_hypot_f64,      2,  {TypeF64, TypeF64},                     TypeF64},
    {"ldexp",       builtin_ldexp_f32,      2,  {TypeF32, TypeI32},                     TypeF32},
    {"ldexp",       builtin_ldexp_f64,      2,  {TypeF64, TypeI32},                     TypeF64},
    {"log",         builtin_log_f32,        1,  {TypeF32},                              TypeF32},
    {"log",         builtin_log_f64,        1,  {TypeF64},                              TypeF64},
    {"log10",       builtin_log10_f32,      1,  {TypeF32},                              TypeF32},
    {"log10",       builtin_log10_f64,      1,  {TypeF64},                              TypeF64},
    {"log2",        builtin_log2_f32,       1,  {TypeF32},                              TypeF32},
    {"log2",        builtin_log2_f64,       1,  {TypeF64},                              TypeF64},
    {"max",         builtin_max_i32,       -2,  {TypeI32},                              TypeI32},
    {"max",         builtin_max_i64,       -2,  {TypeI64},                              TypeI64},
    {"max",         builtin_max_u32,       -2,  {TypeU32},                              TypeU32},
    {"max",         builtin_max_u64,       -2,  {TypeU64},                              TypeU64},
    {"max",         builtin_max_f32,       -2,  {TypeF32},                              TypeF32},
    {"max",         builtin_max_f64,       -2,  {TypeF64},                              TypeF64},
    {"min",         builtin_min_i32,       -2,  {TypeI32},                              TypeI32},
    {"min",         builtin_min_i64,       -2,  {TypeI64},                              TypeI64},
    {"min",         builtin_min_u32,       -2,  {TypeU32},                              TypeU32},
    {"min",         builtin_min_u64,       -2,  {TypeU64},                              TypeU64},
    {"min",         builtin_min_f32,       -2,  {TypeF32},                              TypeF32},
    {"min",         builtin_min_f64,       -2,  {TypeF64},                              TypeF64},
    {"pow",         builtin_pow_i32,        2,  {TypeI32, TypeI32},                     TypeI32},
    {"pow",         builtin_pow_i64,        2,  {TypeI64, TypeI64},                     TypeI64},
    {"pow",         builtin_pow_f32,        2,  {TypeF32, TypeF32},                     TypeF32},
    {"pow",         builtin_pow_f64,        2,  {TypeF64, TypeF64},                     TypeF64},
    {"rad",         builtin_rad_f32,        1,  {TypeF32},                              TypeF32},
    {"rad",         builtin_rad_f64,        1,  {TypeF64},                              TypeF64},
    {"sin",         builtin_sin_f32,        1,  {TypeF32},                              TypeF32},
    {"sin",         builtin_sin_f64,        1,  {TypeF64},                              TypeF64},
    {"sinh",        builtin_sinh_f32,       1,  {TypeF32},                              TypeF32},
    {"sinh",        builtin_sinh_f64,       1,  {TypeF64},                              TypeF64},
    {"sqrt",        builtin_sqrt_f32,       1,  {TypeF32},                              TypeF32},
    {"sqrt",        builtin_sqrt_f64,       1,  {TypeF64},                              TypeF64},
    {"sum",         builtin_sum_i32,       -2,  {TypeI32},                              TypeI32},
    {"sum",         builtin_sum_i64,       -2,  {TypeI64},                              TypeI64},
    {"sum",         builtin_sum_u32,       -2,  {TypeU32},                              TypeU32},
    {"sum",         builtin_sum_u64,       -2,  {TypeU64},                              TypeU64},
    {"sum",         builtin_sum_f32,       -2,  {TypeF32},                              TypeF32},
    {"sum",         builtin_sum_f64,       -2,  {TypeF64},                              TypeF64},
    {"tan",         builtin_tan_f32,        1,  {TypeF32},                              TypeF32},
    {"tan",         builtin_tan_f64,        1,  {TypeF64},                              TypeF64},
    {"tanh",        builtin_tanh_f32,       1,  {TypeF32},                              TypeF32},
    {"tanh",        builtin_tanh_f64,       1,  {TypeF64},                              TypeF64},

    // String
    {"string",      builtin_string,        -1,  {TypeUnknown},                          TypeString},

    // System
    {"assert",      builtin_assert,         1,  {TypeBool},                             TypeVoid},
    {"assert",      builtin_assert_message, 2,  {TypeBool, TypeString},                 TypeVoid},
    {"byteorder",   builtin_byteorder,      0,  {},                                     TypeI32},
    {"exit",        builtin_exit,           0,  {},                                     TypeVoid},
    {"exit",        builtin_exit_status,    1,  {TypeI32},                              TypeVoid},
    {"panic",       builtin_panic,          1,  {TypeString},                           TypeVoid}
};

static bool builtin_argument_matches(const Builtin* builtin, const DataType* argument_types, size_t position)
{
    DataType argument_type = argument_types[position];
    DataType parameter_type = builtin->params[position];

    return argument_type == TypeUnknown
        || argument_type == parameter_type
        || can_implicitly_widen_type(argument_type, parameter_type);
}

static bool typed_variadic_matches(
    const Builtin* builtin, const DataType* argument_types, size_t argument_count, bool exact)
{
    if (!argument_count || !argument_types) {
        return false;
    }

    bool matches = true;

    for (size_t i = 0; i < argument_count && matches; i++) {
        DataType argument_type = argument_types[i];
        DataType parameter_type = builtin->params[0];
        matches = argument_type == parameter_type || (!exact && can_implicitly_widen_type(argument_type, parameter_type));
    }

    return matches;
}

static bool builtin_matches(const Builtin* builtin, const DataType* argument_types, size_t argument_count)
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
    const Builtin* builtin, const char* name, const DataType* argument_types, size_t argument_count)
{
    return strcmp(name, builtin->name) == 0
        && builtin_matches(builtin, argument_types, argument_count);
}

static const Builtin* match_exact_typed_variadic_builtin(
    const Builtin* builtin, const DataType* argument_types, size_t argument_count)
{
    bool matches = typed_variadic_matches(builtin, argument_types, argument_count, true);

    if (!matches) {
        return NULL;
    }

    return builtin;
}

static const Builtin* match_exact_builtin(
    const Builtin* builtin, const char* name, const DataType* argument_types, size_t argument_count)
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
        matches = argument_types[i] == builtin->params[i];
    }

    if (!matches) {
        return NULL;
    }

    return builtin;
}

static const Builtin* resolve_exact_builtin(const char* name, const DataType* argument_types, size_t argument_count)
{
    const Builtin* exact = NULL;

    for (int i = 0; i < BUILTINS_MAX && !exact; i++) {
        exact = match_exact_builtin(&builtins[i], name, argument_types, argument_count);
    }

    return exact;
}

const Builtin* resolve_builtin(const char* name, const DataType* argument_types, size_t argument_count)
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
