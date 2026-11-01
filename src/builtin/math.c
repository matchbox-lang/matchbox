#include "builtin/math.h"
#include "common/util.h"
#include <math.h>

void builtin_abs_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float n = AS_F32(frame[0]);
    float x = fabsf(n);

    frame[-2] = F32_VALUE(x);
}

void builtin_abs_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double n = read_f64(frame, 0);
    double x = fabs(n);

    write_f64(frame, x);
}

void builtin_abs_i32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int32_t n = AS_SIGNED(frame[0]);
    int32_t x = ABS(n);
    
    frame[-2] = SIGNED_VALUE(x);
}

void builtin_abs_i64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int64_t n = read_i64(frame, 0);
    int64_t x = ABS(n);

    write_i64(frame, x);
}

void builtin_acos_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = acosf(a);

    frame[-2] = F32_VALUE(x);
}

void builtin_acos_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double x = acos(a);

    write_f64(frame, x);
}

void builtin_acosh_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = acoshf(a);

    frame[-2] = F32_VALUE(x);
}

void builtin_acosh_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double x = acosh(a);

    write_f64(frame, x);
}

void builtin_asin_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = asinf(a);

    frame[-2] = F32_VALUE(x);
}

void builtin_asin_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double x = asin(a);

    write_f64(frame, x);
}

void builtin_asinh_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = asinhf(a);

    frame[-2] = F32_VALUE(x);
}

void builtin_asinh_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double x = asinh(a);

    write_f64(frame, x);
}

void builtin_atan2_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float b = AS_F32(frame[1]);
    float x = atan2f(a, b);

    frame[-2] = F32_VALUE(x);
}

void builtin_atan2_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double b = read_f64(frame, 2);
    double x = atan2(a, b);

    write_f64(frame, x);
}

void builtin_atan_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = atanf(a);

    frame[-2] = F32_VALUE(x);
}

void builtin_atan_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double x = atan(a);

    write_f64(frame, x);
}

void builtin_atanh_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = atanhf(a);

    frame[-2] = F32_VALUE(x);
}

void builtin_atanh_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double x = atanh(a);

    write_f64(frame, x);
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
    float x = cbrtf(a);

    frame[-2] = F32_VALUE(x);
}

void builtin_cbrt_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double x = cbrt(a);

    write_f64(frame, x);
}

void builtin_ceil_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = ceilf(a);

    frame[-2] = F32_VALUE(x);
}

void builtin_ceil_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double x = ceil(a);

    write_f64(frame, x);
}

void builtin_clamp_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float b = AS_F32(frame[1]);
    float c = AS_F32(frame[2]);
    float x = a;

    if (a < b) {
        x = b;
    } else if (a > c) {
        x = c;
    }

    frame[-2] = F32_VALUE(x);
}

void builtin_clamp_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double b = read_f64(frame, 2);
    double c = read_f64(frame, 4);
    double x = a;

    if (a < b) {
        x = b;
    } else if (a > c) {
        x = c;
    }

    write_f64(frame, x);
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
    float x = cosf(a);

    frame[-2] = F32_VALUE(x);
}

void builtin_cos_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double x = cos(a);

    write_f64(frame, x);
}

void builtin_cosh_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = coshf(a);

    frame[-2] = F32_VALUE(x);
}

void builtin_cosh_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double x = cosh(a);

    write_f64(frame, x);
}

void builtin_deg_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = a * 57.2957795130823208768f;

    frame[-2] = F32_VALUE(x);
}

void builtin_deg_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double x = a * 57.2957795130823208768;

    write_f64(frame, x);
}

void builtin_exp_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = expf(a);

    frame[-2] = F32_VALUE(x);
}

void builtin_exp_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double x = exp(a);

    write_f64(frame, x);
}

void builtin_floor_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = floorf(a);

    frame[-2] = F32_VALUE(x);
}

void builtin_floor_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double x = floor(a);

    write_f64(frame, x);
}

void builtin_hypot_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float b = AS_F32(frame[1]);
    float x = hypotf(a, b);

    frame[-2] = F32_VALUE(x);
}

void builtin_hypot_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double b = read_f64(frame, 2);
    double x = hypot(a, b);

    write_f64(frame, x);
}

void builtin_ldexp_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    int32_t exponent = AS_I32(frame[1]);
    float x = ldexpf(a, exponent);

    frame[-2] = F32_VALUE(x);
}

void builtin_ldexp_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    int32_t exponent = AS_I32(frame[2]);
    double x = ldexp(a, exponent);

    write_f64(frame, x);
}

void builtin_log10_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = log10f(a);

    frame[-2] = F32_VALUE(x);
}

void builtin_log10_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double x = log10(a);

    write_f64(frame, x);
}

void builtin_log2_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = log2f(a);

    frame[-2] = F32_VALUE(x);
}

void builtin_log2_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double x = log2(a);

    write_f64(frame, x);
}

void builtin_log_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = logf(a);

    frame[-2] = F32_VALUE(x);
}

void builtin_log_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double x = log(a);

    write_f64(frame, x);
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
    float x = powf(a, b);

    frame[-2] = F32_VALUE(x);
}

void builtin_pow_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double b = read_f64(frame, 2);
    double x = pow(a, b);

    write_f64(frame, x);
}

void builtin_pow_i32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int32_t a = AS_SIGNED(frame[0]);
    int32_t b = AS_SIGNED(frame[1]);
    int32_t x = pow(a, b);

    frame[-2] = SIGNED_VALUE(x);
}

void builtin_pow_i64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int64_t a = read_i64(frame, 0);
    int64_t b = read_i64(frame, 2);
    int64_t x = pow(a, b);

    write_i64(frame, x);
}

void builtin_rad_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = a * 0.01745329251994329577f;

    frame[-2] = F32_VALUE(x);
}

void builtin_rad_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double x = a * 0.01745329251994329577;

    write_f64(frame, x);
}

void builtin_sin_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = sinf(a);

    frame[-2] = F32_VALUE(x);
}

void builtin_sin_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double x = sin(a);

    write_f64(frame, x);
}

void builtin_sinh_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = sinhf(a);

    frame[-2] = F32_VALUE(x);
}

void builtin_sinh_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double x = sinh(a);

    write_f64(frame, x);
}

void builtin_sqrt_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = sqrtf(a);

    frame[-2] = F32_VALUE(x);
}

void builtin_sqrt_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double x = sqrt(a);

    write_f64(frame, x);
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
    float x = tanf(a);

    frame[-2] = F32_VALUE(x);
}

void builtin_tan_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double x = tan(a);

    write_f64(frame, x);
}

void builtin_tanh_f32(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = tanhf(a);

    frame[-2] = F32_VALUE(x);
}

void builtin_tanh_f64(VM* vm, const FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = read_f64(frame, 0);
    double x = tanh(a);

    write_f64(frame, x);
}
