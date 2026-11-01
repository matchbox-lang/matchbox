case OpMov64:
    vm->fp[a] = vm->fp[b];
    break;
case OpLdc64:
    vm->fp[a] = vm->module->constants.data[OPERAND_BC(inst)];
    break;
case OpLdg64:
    vm->fp[a] = vm->globals.data[OPERAND_BC(inst)];
    break;
case OpLdc64Call: {
    uint16_t operands = OPERAND_BC(inst);
    uint16_t position = LOAD_CALL_OPERAND(operands);

    vm->fp[a] = vm->module->constants.data[position];

    function_position = LOAD_CALL_FUNCTION(operands);
    goto call;
}
case OpLdg64Call: {
    uint16_t operands = OPERAND_BC(inst);
    uint16_t position = LOAD_CALL_OPERAND(operands);

    vm->fp[a] = vm->globals.data[position];

    function_position = LOAD_CALL_FUNCTION(operands);
    goto call;
}
case OpStg64:
    vm->globals.data[OPERAND_BC(inst)] = vm->fp[a];
    break;
case OpLdr64:
    vm->fp[a] = *(Value*)AS_POINTER(vm->fp[b]);
    break;
case OpStr64:
    *(Value*)AS_POINTER(vm->fp[b]) = vm->fp[a];
    break;
case OpF32ToF64:
    vm->fp[a] = F64_VALUE(AS_F32(vm->fp[b]));
    break;
case OpF64ToF32:
    vm->fp[a] = F32_VALUE((float)AS_F64(vm->fp[b]));
    break;
case OpF32ToI64:
    vm->fp[a] = SIGNED_VALUE((int64_t)AS_F32(vm->fp[b]));
    break;
case OpF32ToU64:
    vm->fp[a] = UNSIGNED_VALUE((uint64_t)AS_F32(vm->fp[b]));
    break;
case OpF64ToI32:
    vm->fp[a] = I32_VALUE((int32_t)AS_F64(vm->fp[b]));
    break;
case OpF64ToU32:
    vm->fp[a] = U32_VALUE((uint32_t)AS_F64(vm->fp[b]));
    break;
case OpF64ToI64:
    vm->fp[a] = SIGNED_VALUE((int64_t)AS_F64(vm->fp[b]));
    break;
case OpF64ToU64:
    vm->fp[a] = UNSIGNED_VALUE((uint64_t)AS_F64(vm->fp[b]));
    break;
case OpI64ToF32:
    vm->fp[a] = F32_VALUE((float)AS_SIGNED(vm->fp[b]));
    break;
case OpI64ToF64:
    vm->fp[a] = F64_VALUE((double)AS_SIGNED(vm->fp[b]));
    break;
case OpU64ToF32:
    vm->fp[a] = F32_VALUE((float)AS_UNSIGNED(vm->fp[b]));
    break;
case OpU64ToF64:
    vm->fp[a] = F64_VALUE((double)AS_UNSIGNED(vm->fp[b]));
    break;
case OpI32ToF64:
    vm->fp[a] = F64_VALUE((double)AS_SIGNED(vm->fp[b]));
    break;
case OpU32ToF64:
    vm->fp[a] = F64_VALUE((double)AS_UNSIGNED(vm->fp[b]));
    break;
case OpAddF64:
    vm->fp[a] = F64_VALUE(AS_F64(vm->fp[b]) + AS_F64(vm->fp[c]));
    break;
case OpSubF64:
    vm->fp[a] = F64_VALUE(AS_F64(vm->fp[b]) - AS_F64(vm->fp[c]));
    break;
case OpMulF64:
    vm->fp[a] = F64_VALUE(AS_F64(vm->fp[b]) * AS_F64(vm->fp[c]));
    break;
case OpDivF64:
    vm->fp[a] = F64_VALUE(AS_F64(vm->fp[b]) / AS_F64(vm->fp[c]));
    break;
case OpRemF64: {
    double left = AS_F64(vm->fp[b]);
    double right = AS_F64(vm->fp[c]);
    double result = fmod(left, right);
    vm->fp[a] = F64_VALUE(result);
    break;
}
case OpNegF64:
    vm->fp[a] = F64_VALUE(-AS_F64(vm->fp[b]));
    break;
case OpAddI64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsigned_value + vm->fp[c].unsigned_value);
    break;
case OpAddiI64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsigned_value + (int8_t)c);
    break;
case OpSubI64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsigned_value - vm->fp[c].unsigned_value);
    break;
case OpSubiI64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsigned_value - (int8_t)c);
    break;
case OpMulI64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsigned_value * vm->fp[c].unsigned_value);
    break;
case OpMuliI64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsigned_value * (int8_t)c);
    break;
case OpIdivI64: {
    if (!vm->fp[c].signed_value) {
        division_by_zero_error();
    }

    if (vm->fp[b].signed_value == INT64_MIN && vm->fp[c].signed_value == -1) {
        vm->fp[a] = SIGNED_VALUE(INT64_MIN);
        break;
    }

    vm->fp[a] = SIGNED_VALUE(vm->fp[b].signed_value / vm->fp[c].signed_value);
    break;
}
case OpIdivU64: {
    if (!vm->fp[c].unsigned_value) {
        division_by_zero_error();
    }

    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsigned_value / vm->fp[c].unsigned_value);
    break;
}
case OpRemI64: {
    if (!vm->fp[c].signed_value) {
        division_by_zero_error();
    }

    if (vm->fp[b].signed_value == INT64_MIN && vm->fp[c].signed_value == -1) {
        vm->fp[a] = SIGNED_VALUE(0);
        break;
    }

    vm->fp[a] = SIGNED_VALUE(vm->fp[b].signed_value % vm->fp[c].signed_value);
    break;
}
case OpRemU64: {
    if (!vm->fp[c].unsigned_value) {
        division_by_zero_error();
    }

    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsigned_value % vm->fp[c].unsigned_value);
    break;
}
case OpBandI64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsigned_value & vm->fp[c].unsigned_value);
    break;
case OpBandiI64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsigned_value & c);
    break;
case OpBorI64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsigned_value | vm->fp[c].unsigned_value);
    break;
case OpBoriI64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsigned_value | c);
    break;
case OpBxorI64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsigned_value ^ vm->fp[c].unsigned_value);
    break;
case OpBxoriI64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsigned_value ^ c);
    break;
case OpBnotI64:
    vm->fp[a] = UNSIGNED_VALUE(~vm->fp[b].unsigned_value);
    break;
case OpLslI64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsigned_value << vm->fp[c].unsigned_value);
    break;
case OpLsliI64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsigned_value << c);
    break;
case OpLsrI64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsigned_value >> vm->fp[c].unsigned_value);
    break;
case OpLsriI64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsigned_value >> c);
    break;
case OpAsrI64:
    vm->fp[a] = SIGNED_VALUE(vm->fp[b].signed_value >> vm->fp[c].signed_value);
    break;
case OpAsriI64:
    vm->fp[a] = SIGNED_VALUE(vm->fp[b].signed_value >> c);
    break;
case OpNegI64:
    vm->fp[a] = SIGNED_VALUE(-vm->fp[b].signed_value);
    break;
case OpRet64: {
    Value* frame = vm->fp;
    Instruction* return_ip = AS_POINTER(frame[-2]);
    Value* return_fp = AS_POINTER(frame[-1]);

    frame[-2] = frame[a];
    vm->ip = return_ip;
    vm->fp = return_fp;
    break;
}

case OpCmpI64: {
    int64_t left = AS_SIGNED(vm->fp[b]);
    int64_t right = AS_SIGNED(vm->fp[c]);
    int32_t result = (left > right) - (left < right);

    vm->fp[a] = I32_VALUE(result);
    break;
}
case OpCmpU64: {
    uint64_t left = AS_UNSIGNED(vm->fp[b]);
    uint64_t right = AS_UNSIGNED(vm->fp[c]);
    int32_t result = (left > right) - (left < right);

    vm->fp[a] = I32_VALUE(result);
    break;
}
case OpCmplF64:
case OpCmpgF64: {
    double left = AS_F64(vm->fp[b]);
    double right = AS_F64(vm->fp[c]);
    int32_t result = (left > right) - (left < right);

    if (isnan(left) || isnan(right)) {
        result = opcode == OpCmplF64 ? -1 : 1;
    }

    vm->fp[a] = I32_VALUE(result);
    break;
}
