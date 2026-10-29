#include "builtin/math.h"
#include "common/util.h"
#include <math.h>

void builtinAbsF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float n = AS_F32(frame[0]);
    float x = fabsf(n);

    frame[-2] = F32_VALUE(x);
}

void builtinAbsF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double n = readF64(frame, 0);
    double x = fabs(n);

    writeF64(frame, x);
}

void builtinAbsI32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int32_t n = AS_SIGNED(frame[0]);
    int32_t x = ABS(n);
    
    frame[-2] = SIGNED_VALUE(x);
}

void builtinAbsI64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int64_t n = readI64(frame, 0);
    int64_t x = ABS(n);

    writeI64(frame, x);
}

void builtinAcosF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = acosf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinAcosF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = acos(a);

    writeF64(frame, x);
}

void builtinAcoshF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = acoshf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinAcoshF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = acosh(a);

    writeF64(frame, x);
}

void builtinAsinF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = asinf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinAsinF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = asin(a);

    writeF64(frame, x);
}

void builtinAsinhF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = asinhf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinAsinhF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = asinh(a);

    writeF64(frame, x);
}

void builtinAtan2F32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float b = AS_F32(frame[1]);
    float x = atan2f(a, b);

    frame[-2] = F32_VALUE(x);
}

void builtinAtan2F64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double b = readF64(frame, 2);
    double x = atan2(a, b);

    writeF64(frame, x);
}

void builtinAtanF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = atanf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinAtanF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = atan(a);

    writeF64(frame, x);
}

void builtinAtanhF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = atanhf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinAtanhF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = atanh(a);

    writeF64(frame, x);
}

void builtinAvgF32(VM* vm, FunctionObject* function, Value* frame)
{
    builtinSumF32(vm, function, frame);

    float result = AS_F32(frame[-2]) / (float)function->argumentCount;

    frame[-2] = F32_VALUE(result);
}

void builtinAvgF64(VM* vm, FunctionObject* function, Value* frame)
{
    builtinSumF64(vm, function, frame);

    double result = readF64(frame - 2, 0);
    result /= (double)function->argumentCount;

    writeF64(frame, result);
}

void builtinCbrtF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = cbrtf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinCbrtF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = cbrt(a);

    writeF64(frame, x);
}

void builtinCeilF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = ceilf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinCeilF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = ceil(a);

    writeF64(frame, x);
}

void builtinClampF32(VM* vm, FunctionObject* function, Value* frame)
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

void builtinClampF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double b = readF64(frame, 2);
    double c = readF64(frame, 4);
    double x = a;

    if (a < b) {
        x = b;
    } else if (a > c) {
        x = c;
    }

    writeF64(frame, x);
}

void builtinClampI32(VM* vm, FunctionObject* function, Value* frame)
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

void builtinClampI64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int64_t num = readI64(frame, 0);
    int64_t min = readI64(frame, 2);
    int64_t max = readI64(frame, 4);

    if (num < min) {
        num = min;
    } else if (num > max) {
        num = max;
    }

    writeI64(frame, num);
}

void builtinCosF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = cosf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinCosF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = cos(a);

    writeF64(frame, x);
}

void builtinCoshF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = coshf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinCoshF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = cosh(a);

    writeF64(frame, x);
}

void builtinDegF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = a * 57.2957795130823208768f;

    frame[-2] = F32_VALUE(x);
}

void builtinDegF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = a * 57.2957795130823208768;

    writeF64(frame, x);
}

void builtinExpF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = expf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinExpF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = exp(a);

    writeF64(frame, x);
}

void builtinFloorF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = floorf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinFloorF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = floor(a);

    writeF64(frame, x);
}

void builtinHypotF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float b = AS_F32(frame[1]);
    float x = hypotf(a, b);

    frame[-2] = F32_VALUE(x);
}

void builtinHypotF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double b = readF64(frame, 2);
    double x = hypot(a, b);

    writeF64(frame, x);
}

void builtinLdexpF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    int32_t exponent = AS_I32(frame[1]);
    float x = ldexpf(a, exponent);

    frame[-2] = F32_VALUE(x);
}

void builtinLdexpF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    int32_t exponent = AS_I32(frame[2]);
    double x = ldexp(a, exponent);

    writeF64(frame, x);
}

void builtinLog10F32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = log10f(a);

    frame[-2] = F32_VALUE(x);
}

void builtinLog10F64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = log10(a);

    writeF64(frame, x);
}

void builtinLog2F32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = log2f(a);

    frame[-2] = F32_VALUE(x);
}

void builtinLog2F64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = log2(a);

    writeF64(frame, x);
}

void builtinLogF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = logf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinLogF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = log(a);

    writeF64(frame, x);
}

void builtinMaxF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    float result = AS_F32(frame[0]);

    for (size_t i = 1; i < function->argumentCount; i++) {
        float value = AS_F32(frame[i]);
        result = fmaxf(result, value);
    }

    frame[-2] = F32_VALUE(result);
}

void builtinMaxF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    double result = readF64(frame, 0);

    for (size_t i = 1; i < function->argumentCount; i++) {
        double value = readF64(frame, i * 2);
        result = fmax(result, value);
    }

    writeF64(frame, result);
}

void builtinMaxI32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    int32_t result = AS_I32(frame[0]);

    for (size_t i = 1; i < function->argumentCount; i++) {
        int32_t value = AS_I32(frame[i]);
        result = MAX(result, value);
    }

    frame[-2] = I32_VALUE(result);
}

void builtinMaxI64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    int64_t result = readI64(frame, 0);

    for (size_t i = 1; i < function->argumentCount; i++) {
        int64_t value = readI64(frame, i * 2);
        result = MAX(result, value);
    }

    writeI64(frame, result);
}

void builtinMaxU32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    uint32_t result = AS_U32(frame[0]);

    for (size_t i = 1; i < function->argumentCount; i++) {
        uint32_t value = AS_U32(frame[i]);
        result = MAX(result, value);
    }

    frame[-2] = U32_VALUE(result);
}

void builtinMaxU64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    uint64_t result = readU64(frame, 0);

    for (size_t i = 1; i < function->argumentCount; i++) {
        uint64_t value = readU64(frame, i * 2);
        result = MAX(result, value);
    }

    writeI64(frame, (int64_t)result);
}

void builtinMinF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    float result = AS_F32(frame[0]);

    for (size_t i = 1; i < function->argumentCount; i++) {
        float value = AS_F32(frame[i]);
        result = fminf(result, value);
    }

    frame[-2] = F32_VALUE(result);
}

void builtinMinF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    double result = readF64(frame, 0);

    for (size_t i = 1; i < function->argumentCount; i++) {
        double value = readF64(frame, i * 2);
        result = fmin(result, value);
    }

    writeF64(frame, result);
}

void builtinMinI32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    int32_t result = AS_I32(frame[0]);

    for (size_t i = 1; i < function->argumentCount; i++) {
        int32_t value = AS_I32(frame[i]);
        result = MIN(result, value);
    }

    frame[-2] = I32_VALUE(result);
}

void builtinMinI64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    int64_t result = readI64(frame, 0);

    for (size_t i = 1; i < function->argumentCount; i++) {
        int64_t value = readI64(frame, i * 2);
        result = MIN(result, value);
    }

    writeI64(frame, result);
}

void builtinMinU32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    uint32_t result = AS_U32(frame[0]);

    for (size_t i = 1; i < function->argumentCount; i++) {
        uint32_t value = AS_U32(frame[i]);
        result = MIN(result, value);
    }

    frame[-2] = U32_VALUE(result);
}

void builtinMinU64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    uint64_t result = readU64(frame, 0);

    for (size_t i = 1; i < function->argumentCount; i++) {
        uint64_t value = readU64(frame, i * 2);
        result = MIN(result, value);
    }

    writeI64(frame, (int64_t)result);
}

void builtinPowF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float b = AS_F32(frame[1]);
    float x = powf(a, b);

    frame[-2] = F32_VALUE(x);
}

void builtinPowF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double b = readF64(frame, 2);
    double x = pow(a, b);

    writeF64(frame, x);
}

void builtinPowI32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int32_t a = AS_SIGNED(frame[0]);
    int32_t b = AS_SIGNED(frame[1]);
    int32_t x = pow(a, b);

    frame[-2] = SIGNED_VALUE(x);
}

void builtinPowI64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    int64_t a = readI64(frame, 0);
    int64_t b = readI64(frame, 2);
    int64_t x = pow(a, b);

    writeI64(frame, x);
}

void builtinRadF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = a * 0.01745329251994329577f;

    frame[-2] = F32_VALUE(x);
}

void builtinRadF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = a * 0.01745329251994329577;

    writeF64(frame, x);
}

void builtinSinF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = sinf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinSinF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = sin(a);

    writeF64(frame, x);
}

void builtinSinhF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = sinhf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinSinhF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = sinh(a);

    writeF64(frame, x);
}

void builtinSqrtF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = sqrtf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinSqrtF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = sqrt(a);

    writeF64(frame, x);
}

void builtinSumF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    float result = 0.0f;

    for (size_t i = 0; i < function->argumentCount; i++) {
        result += AS_F32(frame[i]);
    }

    frame[-2] = F32_VALUE(result);
}

void builtinSumF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    double result = 0.0;

    for (size_t i = 0; i < function->argumentCount; i++) {
        result += readF64(frame, i * 2);
    }

    writeF64(frame, result);
}

void builtinSumI32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    uint32_t result = 0;

    for (size_t i = 0; i < function->argumentCount; i++) {
        result += AS_U32(frame[i]);
    }

    frame[-2] = I32_VALUE(result);
}

void builtinSumI64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    uint64_t result = 0;

    for (size_t i = 0; i < function->argumentCount; i++) {
        result += readU64(frame, i * 2);
    }

    writeI64(frame, (int64_t)result);
}

void builtinSumU32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    uint32_t result = 0;

    for (size_t i = 0; i < function->argumentCount; i++) {
        result += AS_U32(frame[i]);
    }

    frame[-2] = U32_VALUE(result);
}

void builtinSumU64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;

    uint64_t result = 0;

    for (size_t i = 0; i < function->argumentCount; i++) {
        result += readU64(frame, i * 2);
    }

    writeI64(frame, (int64_t)result);
}

void builtinTanF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = tanf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinTanF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = tan(a);

    writeF64(frame, x);
}

void builtinTanhF32(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    float a = AS_F32(frame[0]);
    float x = tanhf(a);

    frame[-2] = F32_VALUE(x);
}

void builtinTanhF64(VM* vm, FunctionObject* function, Value* frame)
{
    (void)vm;
    (void)function;

    double a = readF64(frame, 0);
    double x = tanh(a);

    writeF64(frame, x);
}
