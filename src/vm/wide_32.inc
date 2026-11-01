#define READ_U64(values, index) \
    ((values)[(index)].unsigned_value \
        | ((uint64_t)(values)[(index) + 1].unsigned_value << 32))

#define WRITE_U64(values, index, value) { \
    uint64_t wide_value = (value); \
    (values)[(index)].unsigned_value = wide_value; \
    (values)[(index) + 1].unsigned_value = wide_value >> 32; \
}

#define READ_F64(values, index, result) { \
    uint64_t bits = READ_U64(values, index); \
    memcpy(&(result), &bits, sizeof(result)); \
}

#define WRITE_F64(values, index, value) { \
    double double_value = (value); \
    uint64_t bits; \
    memcpy(&bits, &double_value, sizeof(bits)); \
    WRITE_U64(values, index, bits); \
}

case OpMov64:
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b));
    break;
case OpLdc64:
    WRITE_U64(vm->fp, a, READ_U64(vm->module->constants.data, OPERAND_BC(inst)));
    break;
case OpLdg64:
    WRITE_U64(vm->fp, a, READ_U64(vm->globals.data, OPERAND_BC(inst)));
    break;
case OpLdc64Call: {
    uint16_t operands = OPERAND_BC(inst);
    uint16_t position = LOAD_CALL_OPERAND(operands);

    vm->fp[a] = vm->module->constants.data[position];
    vm->fp[a + 1] = vm->module->constants.data[position + 1];

    function_position = LOAD_CALL_FUNCTION(operands);
    goto call;
}
case OpLdg64Call: {
    uint16_t operands = OPERAND_BC(inst);
    uint16_t position = LOAD_CALL_OPERAND(operands);

    vm->fp[a] = vm->globals.data[position];
    vm->fp[a + 1] = vm->globals.data[position + 1];

    function_position = LOAD_CALL_FUNCTION(operands);
    goto call;
}
case OpStg64:
    WRITE_U64(vm->globals.data, OPERAND_BC(inst), READ_U64(vm->fp, a));
    break;
case OpLdr64: {
    Value* reference = AS_POINTER(vm->fp[b]);
    vm->fp[a] = reference[0];
    vm->fp[a + 1] = reference[1];
    break;
}
case OpStr64: {
    Value* reference = AS_POINTER(vm->fp[b]);
    reference[0] = vm->fp[a];
    reference[1] = vm->fp[a + 1];
    break;
}
case OpF32ToF64:
    WRITE_F64(vm->fp, a, AS_F32(vm->fp[b]));
    break;
case OpF64ToF32: {
    double value;

    READ_F64(vm->fp, b, value);

    vm->fp[a] = F32_VALUE((float)value);
    break;
}
case OpF32ToI64:
    WRITE_U64(vm->fp, a, (uint64_t)(int64_t)AS_F32(vm->fp[b]));
    break;
case OpF32ToU64:
    WRITE_U64(vm->fp, a, (uint64_t)AS_F32(vm->fp[b]));
    break;
case OpF64ToI32: {
    double value;

    READ_F64(vm->fp, b, value);

    vm->fp[a] = I32_VALUE((int32_t)value);
    break;
}
case OpF64ToU32: {
    double value;

    READ_F64(vm->fp, b, value);

    vm->fp[a] = U32_VALUE((uint32_t)value);
    break;
}
case OpF64ToI64: {
    double value;

    READ_F64(vm->fp, b, value);
    WRITE_U64(vm->fp, a, (uint64_t)(int64_t)value);
    break;
}
case OpF64ToU64: {
    double value;

    READ_F64(vm->fp, b, value);
    WRITE_U64(vm->fp, a, (uint64_t)value);
    break;
}
case OpI64ToF32: {
    int64_t value = (int64_t)READ_U64(vm->fp, b);

    vm->fp[a] = F32_VALUE((float)value);
    break;
}
case OpI64ToF64: {
    int64_t value = (int64_t)READ_U64(vm->fp, b);

    WRITE_F64(vm->fp, a, (double)value);
    break;
}
case OpU64ToF32: {
    uint64_t value = READ_U64(vm->fp, b);

    vm->fp[a] = F32_VALUE((float)value);
    break;
}
case OpU64ToF64: {
    uint64_t value = READ_U64(vm->fp, b);

    WRITE_F64(vm->fp, a, (double)value);
    break;
}
case OpI32ToF64:
    WRITE_F64(vm->fp, a, AS_SIGNED(vm->fp[b]));
    break;
case OpU32ToF64:
    WRITE_F64(vm->fp, a, AS_UNSIGNED(vm->fp[b]));
    break;
case OpAddF64: {
    double left;
    double right;
    READ_F64(vm->fp, b, left);
    READ_F64(vm->fp, c, right);
    WRITE_F64(vm->fp, a, left + right);
    break;
}
case OpSubF64: {
    double left;
    double right;
    READ_F64(vm->fp, b, left);
    READ_F64(vm->fp, c, right);
    WRITE_F64(vm->fp, a, left - right);
    break;
}
case OpMulF64: {
    double left;
    double right;
    READ_F64(vm->fp, b, left);
    READ_F64(vm->fp, c, right);
    WRITE_F64(vm->fp, a, left * right);
    break;
}
case OpDivF64: {
    double left;
    double right;
    READ_F64(vm->fp, b, left);
    READ_F64(vm->fp, c, right);
    WRITE_F64(vm->fp, a, left / right);
    break;
}
case OpRemF64: {
    double left;
    double right;
    READ_F64(vm->fp, b, left);
    READ_F64(vm->fp, c, right);
    double result = fmod(left, right);
    WRITE_F64(vm->fp, a, result);
    break;
}
case OpNegF64: {
    double value;
    READ_F64(vm->fp, b, value);
    WRITE_F64(vm->fp, a, -value);
    break;
}
case OpAddI64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) + READ_U64(vm->fp, c));
    break;
}
case OpAddiI64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) + (int8_t)c);
    break;
}
case OpSubI64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) - READ_U64(vm->fp, c));
    break;
}
case OpSubiI64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) - (int8_t)c);
    break;
}
case OpMulI64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) * READ_U64(vm->fp, c));
    break;
}
case OpMuliI64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) * (int8_t)c);
    break;
}
case OpIdivI64: {
    if (!READ_U64(vm->fp, c)) {
        division_by_zero_error();
    }

    if (READ_U64(vm->fp, b) == (UINT64_C(1) << 63) && READ_U64(vm->fp, c) == UINT64_MAX) {
        WRITE_U64(vm->fp, a, UINT64_C(1) << 63);
        break;
    }

    WRITE_U64(vm->fp, a, (int64_t)READ_U64(vm->fp, b) / (int64_t)READ_U64(vm->fp, c));
    break;
}
case OpIdivU64: {
    if (!READ_U64(vm->fp, c)) {
        division_by_zero_error();
    }

    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) / READ_U64(vm->fp, c));
    break;
}
case OpRemI64: {
    if (!READ_U64(vm->fp, c)) {
        division_by_zero_error();
    }

    if (READ_U64(vm->fp, b) == (UINT64_C(1) << 63) && READ_U64(vm->fp, c) == UINT64_MAX) {
        WRITE_U64(vm->fp, a, 0);
        break;
    }

    WRITE_U64(vm->fp, a, (int64_t)READ_U64(vm->fp, b) % (int64_t)READ_U64(vm->fp, c));
    break;
}
case OpRemU64: {
    if (!READ_U64(vm->fp, c)) {
        division_by_zero_error();
    }

    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) % READ_U64(vm->fp, c));
    break;
}
case OpBandI64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) & READ_U64(vm->fp, c));
    break;
}
case OpBandiI64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) & c);
    break;
}
case OpBorI64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) | READ_U64(vm->fp, c));
    break;
}
case OpBoriI64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) | c);
    break;
}
case OpBxorI64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) ^ READ_U64(vm->fp, c));
    break;
}
case OpBxoriI64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) ^ c);
    break;
}
case OpBnotI64: {
    WRITE_U64(vm->fp, a, ~READ_U64(vm->fp, b));
    break;
}
case OpLslI64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) << READ_U64(vm->fp, c));
    break;
}
case OpLsliI64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) << c);
    break;
}
case OpLsrI64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) >> READ_U64(vm->fp, c));
    break;
}
case OpLsriI64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) >> c);
    break;
}
case OpAsrI64: {
    WRITE_U64(vm->fp, a, (int64_t)READ_U64(vm->fp, b) >> READ_U64(vm->fp, c));
    break;
}
case OpAsriI64: {
    WRITE_U64(vm->fp, a, (int64_t)READ_U64(vm->fp, b) >> c);
    break;
}
case OpNegI64: {
    WRITE_U64(vm->fp, a, -READ_U64(vm->fp, b));
    break;
}
case OpRet64: {
    Value* frame = vm->fp;
    Instruction* return_ip = AS_POINTER(frame[-2]);
    Value* return_fp = AS_POINTER(frame[-1]);

    frame[-2] = frame[a];
    frame[-1] = frame[a + 1];
    vm->ip = return_ip;
    vm->fp = return_fp;
    break;
}

case OpCmpI64: {
    int64_t left = (int64_t)READ_U64(vm->fp, b);
    int64_t right = (int64_t)READ_U64(vm->fp, c);
    int32_t result = (left > right) - (left < right);

    vm->fp[a] = I32_VALUE(result);
    break;
}
case OpCmpU64: {
    uint64_t left = (uint64_t)READ_U64(vm->fp, b);
    uint64_t right = (uint64_t)READ_U64(vm->fp, c);
    int32_t result = (left > right) - (left < right);

    vm->fp[a] = I32_VALUE(result);
    break;
}
case OpCmplF64:
case OpCmpgF64: {
    double left;
    double right;

    READ_F64(vm->fp, b, left);
    READ_F64(vm->fp, c, right);

    int32_t result = (left > right) - (left < right);

    if (isnan(left) || isnan(right)) {
        result = opcode == OpCmplF64 ? -1 : 1;
    }

    vm->fp[a] = I32_VALUE(result);
    break;
}

#undef READ_U64
#undef WRITE_U64
#undef READ_F64
#undef WRITE_F64
