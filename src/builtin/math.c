#include "builtin/math.h"
#include "common/util.h"
#include <math.h>

void builtin_abs_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float n = AS_F32(frame[0]);
    float result = fabsf(n);

    frame[-2] = F32_VALUE(result);
}

void builtin_abs_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double n = read_f64(frame, 0);
    double result = fabs(n);

    write_f64(frame, result);
}

void builtin_abs_i32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int32_t n = AS_SIGNED(frame[0]);
    int32_t result = ABS(n);
    
    frame[-2] = SIGNED_VALUE(result);
}

void builtin_abs_i64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int64_t n = read_i64(frame, 0);
    int64_t result = ABS(n);

    write_i64(frame, result);
}

void builtin_acos_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float result = acosf(a);

    frame[-2] = F32_VALUE(result);
}

void builtin_acos_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double result = acos(a);

    write_f64(frame, result);
}

void builtin_acosh_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float result = acoshf(a);

    frame[-2] = F32_VALUE(result);
}

void builtin_acosh_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double result = acosh(a);

    write_f64(frame, result);
}

void builtin_asin_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float result = asinf(a);

    frame[-2] = F32_VALUE(result);
}

void builtin_asin_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double result = asin(a);

    write_f64(frame, result);
}

void builtin_asinh_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float result = asinhf(a);

    frame[-2] = F32_VALUE(result);
}

void builtin_asinh_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double result = asinh(a);

    write_f64(frame, result);
}

void builtin_atan2_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float b = AS_F32(frame[1]);
    float result = atan2f(a, b);

    frame[-2] = F32_VALUE(result);
}

void builtin_atan2_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double b = read_f64(frame, 2);
    double result = atan2(a, b);

    write_f64(frame, result);
}

void builtin_atan_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float result = atanf(a);

    frame[-2] = F32_VALUE(result);
}

void builtin_atan_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double result = atan(a);

    write_f64(frame, result);
}

void builtin_atanh_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float result = atanhf(a);

    frame[-2] = F32_VALUE(result);
}

void builtin_atanh_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double result = atanh(a);

    write_f64(frame, result);
}

void builtin_avg_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    builtin_sum_f32(vm, function, frame);

    float result = AS_F32(frame[-2]) / (float)function->argument_count;

    frame[-2] = F32_VALUE(result);
}

void builtin_avg_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    builtin_sum_f64(vm, function, frame);

    double result = read_f64(frame - 2, 0);
    result /= (double)function->argument_count;

    write_f64(frame, result);
}

void builtin_cbrt_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float result = cbrtf(a);

    frame[-2] = F32_VALUE(result);
}

void builtin_cbrt_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double result = cbrt(a);

    write_f64(frame, result);
}

void builtin_ceil_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float result = ceilf(a);

    frame[-2] = F32_VALUE(result);
}

void builtin_ceil_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double result = ceil(a);

    write_f64(frame, result);
}

void builtin_clamp_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float b = AS_F32(frame[1]);
    float c = AS_F32(frame[2]);
    float result = a;

    if (a < b) {
        result = b;
    } else if (a > c) {
        result = c;
    }

    frame[-2] = F32_VALUE(result);
}

void builtin_clamp_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double b = read_f64(frame, 2);
    double c = read_f64(frame, 4);
    double result = a;

    if (a < b) {
        result = b;
    } else if (a > c) {
        result = c;
    }

    write_f64(frame, result);
}

void builtin_clamp_i32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int32_t num = AS_SIGNED(frame[0]);
    int32_t min = AS_SIGNED(frame[1]);
    int32_t max = AS_SIGNED(frame[2]);

    if (num < min) {
        num = min;
    } else if (num > max) {
        num = max;
    }

    frame[-2] = SIGNED_VALUE(num);
}

void builtin_clamp_i64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int64_t num = read_i64(frame, 0);
    int64_t min = read_i64(frame, 2);
    int64_t max = read_i64(frame, 4);

    if (num < min) {
        num = min;
    } else if (num > max) {
        num = max;
    }

    write_i64(frame, num);
}

void builtin_cos_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float result = cosf(a);

    frame[-2] = F32_VALUE(result);
}

void builtin_cos_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double result = cos(a);

    write_f64(frame, result);
}

void builtin_cosh_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float result = coshf(a);

    frame[-2] = F32_VALUE(result);
}

void builtin_cosh_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double result = cosh(a);

    write_f64(frame, result);
}

void builtin_deg_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float result = a * 57.2957795130823208768f;

    frame[-2] = F32_VALUE(result);
}

void builtin_deg_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double result = a * 57.2957795130823208768;

    write_f64(frame, result);
}

void builtin_exp_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float result = expf(a);

    frame[-2] = F32_VALUE(result);
}

void builtin_exp_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double result = exp(a);

    write_f64(frame, result);
}

void builtin_floor_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float result = floorf(a);

    frame[-2] = F32_VALUE(result);
}

void builtin_floor_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double result = floor(a);

    write_f64(frame, result);
}

void builtin_hypot_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float b = AS_F32(frame[1]);
    float result = hypotf(a, b);

    frame[-2] = F32_VALUE(result);
}

void builtin_hypot_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double b = read_f64(frame, 2);
    double result = hypot(a, b);

    write_f64(frame, result);
}

void builtin_ilerp_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float b = AS_F32(frame[1]);
    float v = AS_F32(frame[2]);

    if (a == b) {
        frame[-2] = F32_VALUE(0.0f);

        return;
    }

    float result = (v - a) / (b - a);

    frame[-2] = F32_VALUE(result);
}

void builtin_ilerp_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double b = read_f64(frame, 2);
    double v = read_f64(frame, 4);

    if (a == b) {
        write_f64(frame, 0.0);

        return;
    }

    double result = (v - a) / (b - a);

    write_f64(frame, result);
}

void builtin_ldexp_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    int32_t exponent = AS_I32(frame[1]);
    float result = ldexpf(a, exponent);

    frame[-2] = F32_VALUE(result);
}

void builtin_ldexp_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    int32_t exponent = AS_I32(frame[2]);
    double result = ldexp(a, exponent);

    write_f64(frame, result);
}

void builtin_lerp_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float b = AS_F32(frame[1]);
    float t = AS_F32(frame[2]);
    float result = a + (b - a) * t;

    frame[-2] = F32_VALUE(result);
}

void builtin_lerp_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double b = read_f64(frame, 2);
    double t = read_f64(frame, 4);
    double result = a + (b - a) * t;

    write_f64(frame, result);
}

void builtin_log10_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float result = log10f(a);

    frame[-2] = F32_VALUE(result);
}

void builtin_log10_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double result = log10(a);

    write_f64(frame, result);
}

void builtin_log2_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float result = log2f(a);

    frame[-2] = F32_VALUE(result);
}

void builtin_log2_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double result = log2(a);

    write_f64(frame, result);
}

void builtin_log_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float result = logf(a);

    frame[-2] = F32_VALUE(result);
}

void builtin_log_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double result = log(a);

    write_f64(frame, result);
}

void builtin_max_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;

    float result = AS_F32(frame[0]);

    for (size_t i = 1; i < function->argument_count; i++) {
        float value = AS_F32(frame[i]);
        result = fmaxf(result, value);
    }

    frame[-2] = F32_VALUE(result);
}

void builtin_max_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;

    double result = read_f64(frame, 0);

    for (size_t i = 1; i < function->argument_count; i++) {
        double value = read_f64(frame, i * 2);
        result = fmax(result, value);
    }

    write_f64(frame, result);
}

void builtin_max_i32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;

    int32_t result = AS_I32(frame[0]);

    for (size_t i = 1; i < function->argument_count; i++) {
        int32_t value = AS_I32(frame[i]);
        result = MAX(result, value);
    }

    frame[-2] = I32_VALUE(result);
}

void builtin_max_i64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;

    int64_t result = read_i64(frame, 0);

    for (size_t i = 1; i < function->argument_count; i++) {
        int64_t value = read_i64(frame, i * 2);
        result = MAX(result, value);
    }

    write_i64(frame, result);
}

void builtin_max_u32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;

    uint32_t result = AS_U32(frame[0]);

    for (size_t i = 1; i < function->argument_count; i++) {
        uint32_t value = AS_U32(frame[i]);
        result = MAX(result, value);
    }

    frame[-2] = U32_VALUE(result);
}

void builtin_max_u64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;

    uint64_t result = read_u64(frame, 0);

    for (size_t i = 1; i < function->argument_count; i++) {
        uint64_t value = read_u64(frame, i * 2);
        result = MAX(result, value);
    }

    write_i64(frame, (int64_t)result);
}

void builtin_min_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;

    float result = AS_F32(frame[0]);

    for (size_t i = 1; i < function->argument_count; i++) {
        float value = AS_F32(frame[i]);
        result = fminf(result, value);
    }

    frame[-2] = F32_VALUE(result);
}

void builtin_min_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;

    double result = read_f64(frame, 0);

    for (size_t i = 1; i < function->argument_count; i++) {
        double value = read_f64(frame, i * 2);
        result = fmin(result, value);
    }

    write_f64(frame, result);
}

void builtin_min_i32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;

    int32_t result = AS_I32(frame[0]);

    for (size_t i = 1; i < function->argument_count; i++) {
        int32_t value = AS_I32(frame[i]);
        result = MIN(result, value);
    }

    frame[-2] = I32_VALUE(result);
}

void builtin_min_i64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;

    int64_t result = read_i64(frame, 0);

    for (size_t i = 1; i < function->argument_count; i++) {
        int64_t value = read_i64(frame, i * 2);
        result = MIN(result, value);
    }

    write_i64(frame, result);
}

void builtin_min_u32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;

    uint32_t result = AS_U32(frame[0]);

    for (size_t i = 1; i < function->argument_count; i++) {
        uint32_t value = AS_U32(frame[i]);
        result = MIN(result, value);
    }

    frame[-2] = U32_VALUE(result);
}

void builtin_min_u64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;

    uint64_t result = read_u64(frame, 0);

    for (size_t i = 1; i < function->argument_count; i++) {
        uint64_t value = read_u64(frame, i * 2);
        result = MIN(result, value);
    }

    write_i64(frame, (int64_t)result);
}

void builtin_pow_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float b = AS_F32(frame[1]);
    float result = powf(a, b);

    frame[-2] = F32_VALUE(result);
}

void builtin_pow_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double b = read_f64(frame, 2);
    double result = pow(a, b);

    write_f64(frame, result);
}

void builtin_pow_i32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int32_t a = AS_SIGNED(frame[0]);
    int32_t b = AS_SIGNED(frame[1]);
    int32_t result = pow(a, b);

    frame[-2] = SIGNED_VALUE(result);
}

void builtin_pow_i64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int64_t a = read_i64(frame, 0);
    int64_t b = read_i64(frame, 2);
    int64_t result = pow(a, b);

    write_i64(frame, result);
}

void builtin_rad_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float result = a * 0.01745329251994329577f;

    frame[-2] = F32_VALUE(result);
}

void builtin_rad_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double result = a * 0.01745329251994329577;

    write_f64(frame, result);
}

void builtin_sign_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float result = (a > 0) - (a < 0);

    if (isnan(a)) {
        result = a;
    }

    frame[-2] = F32_VALUE(result);
}

void builtin_sign_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double result = (a > 0) - (a < 0);

    if (isnan(a)) {
        result = a;
    }

    write_f64(frame, result);
}

void builtin_sign_i32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int32_t a = AS_SIGNED(frame[0]);
    int32_t result = (a > 0) - (a < 0);

    frame[-2] = SIGNED_VALUE(result);
}

void builtin_sign_i64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int64_t a = read_i64(frame, 0);
    int64_t result = (a > 0) - (a < 0);

    write_i64(frame, result);
}

void builtin_sin_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float result = sinf(a);

    frame[-2] = F32_VALUE(result);
}

void builtin_sin_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double result = sin(a);

    write_f64(frame, result);
}

void builtin_sinh_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float result = sinhf(a);

    frame[-2] = F32_VALUE(result);
}

void builtin_sinh_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double result = sinh(a);

    write_f64(frame, result);
}

void builtin_sqrt_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float result = sqrtf(a);

    frame[-2] = F32_VALUE(result);
}

void builtin_sqrt_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double result = sqrt(a);

    write_f64(frame, result);
}

void builtin_sum_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;

    float result = 0.0f;

    for (size_t i = 0; i < function->argument_count; i++) {
        result += AS_F32(frame[i]);
    }

    frame[-2] = F32_VALUE(result);
}

void builtin_sum_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;

    double result = 0.0;

    for (size_t i = 0; i < function->argument_count; i++) {
        result += read_f64(frame, i * 2);
    }

    write_f64(frame, result);
}

void builtin_sum_i32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;

    uint32_t result = 0;

    for (size_t i = 0; i < function->argument_count; i++) {
        result += AS_U32(frame[i]);
    }

    frame[-2] = I32_VALUE(result);
}

void builtin_sum_i64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;

    uint64_t result = 0;

    for (size_t i = 0; i < function->argument_count; i++) {
        result += read_u64(frame, i * 2);
    }

    write_i64(frame, (int64_t)result);
}

void builtin_sum_u32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;

    uint32_t result = 0;

    for (size_t i = 0; i < function->argument_count; i++) {
        result += AS_U32(frame[i]);
    }

    frame[-2] = U32_VALUE(result);
}

void builtin_sum_u64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;

    uint64_t result = 0;

    for (size_t i = 0; i < function->argument_count; i++) {
        result += read_u64(frame, i * 2);
    }

    write_i64(frame, (int64_t)result);
}

void builtin_tan_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float result = tanf(a);

    frame[-2] = F32_VALUE(result);
}

void builtin_tan_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double result = tan(a);

    write_f64(frame, result);
}

void builtin_tanh_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float result = tanhf(a);

    frame[-2] = F32_VALUE(result);
}

void builtin_tanh_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double result = tanh(a);

    write_f64(frame, result);
}
