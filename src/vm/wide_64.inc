case OP_MOV_64:
    vm->fp[a] = vm->fp[b];
    break;
case OP_LDC_64:
    vm->fp[a] = vm->module->constants.data[OPERAND_BC(inst)];
    break;
case OP_LDG_64:
    vm->fp[a] = vm->globals.data[OPERAND_BC(inst)];
    break;
case OP_STG_64:
    vm->globals.data[OPERAND_BC(inst)] = vm->fp[a];
    break;
case OP_LDR_64:
    vm->fp[a] = *(Value*)AS_POINTER(vm->fp[b]);
    break;
case OP_STR_64:
    *(Value*)AS_POINTER(vm->fp[b]) = vm->fp[a];
    break;
case OP_F32_TO_F64:
    vm->fp[a] = F64_VALUE(AS_F32(vm->fp[b]));
    break;
case OP_F64_TO_F32:
    vm->fp[a] = F32_VALUE((float)AS_F64(vm->fp[b]));
    break;
case OP_F32_TO_I64:
    vm->fp[a] = SIGNED_VALUE((int64_t)AS_F32(vm->fp[b]));
    break;
case OP_F32_TO_U64:
    vm->fp[a] = UNSIGNED_VALUE((uint64_t)AS_F32(vm->fp[b]));
    break;
case OP_F64_TO_I32:
    vm->fp[a] = I32_VALUE((int32_t)AS_F64(vm->fp[b]));
    break;
case OP_F64_TO_U32:
    vm->fp[a] = U32_VALUE((uint32_t)AS_F64(vm->fp[b]));
    break;
case OP_F64_TO_I64:
    vm->fp[a] = SIGNED_VALUE((int64_t)AS_F64(vm->fp[b]));
    break;
case OP_F64_TO_U64:
    vm->fp[a] = UNSIGNED_VALUE((uint64_t)AS_F64(vm->fp[b]));
    break;
case OP_I64_TO_F32:
    vm->fp[a] = F32_VALUE((float)AS_SIGNED(vm->fp[b]));
    break;
case OP_I64_TO_F64:
    vm->fp[a] = F64_VALUE((double)AS_SIGNED(vm->fp[b]));
    break;
case OP_U64_TO_F32:
    vm->fp[a] = F32_VALUE((float)AS_UNSIGNED(vm->fp[b]));
    break;
case OP_U64_TO_F64:
    vm->fp[a] = F64_VALUE((double)AS_UNSIGNED(vm->fp[b]));
    break;
case OP_I32_TO_F64:
    vm->fp[a] = F64_VALUE((double)AS_SIGNED(vm->fp[b]));
    break;
case OP_U32_TO_F64:
    vm->fp[a] = F64_VALUE((double)AS_UNSIGNED(vm->fp[b]));
    break;
case OP_ADD_F64:
    vm->fp[a] = F64_VALUE(AS_F64(vm->fp[b]) + AS_F64(vm->fp[c]));
    break;
case OP_SUB_F64:
    vm->fp[a] = F64_VALUE(AS_F64(vm->fp[b]) - AS_F64(vm->fp[c]));
    break;
case OP_MUL_F64:
    vm->fp[a] = F64_VALUE(AS_F64(vm->fp[b]) * AS_F64(vm->fp[c]));
    break;
case OP_DIV_F64:
    vm->fp[a] = F64_VALUE(AS_F64(vm->fp[b]) / AS_F64(vm->fp[c]));
    break;
case OP_REM_F64: {
    double left = AS_F64(vm->fp[b]);
    double right = AS_F64(vm->fp[c]);
    double result = fmod(left, right);
    vm->fp[a] = F64_VALUE(result);
    break;
}
case OP_NEG_F64:
    vm->fp[a] = F64_VALUE(-AS_F64(vm->fp[b]));
    break;
case OP_ADD_I64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsignedValue + vm->fp[c].unsignedValue);
    break;
case OP_ADDI_I64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsignedValue + (int8_t)c);
    break;
case OP_SUB_I64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsignedValue - vm->fp[c].unsignedValue);
    break;
case OP_SUBI_I64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsignedValue - (int8_t)c);
    break;
case OP_MUL_I64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsignedValue * vm->fp[c].unsignedValue);
    break;
case OP_MULI_I64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsignedValue * (int8_t)c);
    break;
case OP_IDIV_I64: {
    if (!vm->fp[c].signedValue) {
        divisionByZeroError();
    }

    if (vm->fp[b].signedValue == INT64_MIN && vm->fp[c].signedValue == -1) {
        vm->fp[a] = SIGNED_VALUE(INT64_MIN);
        break;
    }

    vm->fp[a] = SIGNED_VALUE(vm->fp[b].signedValue / vm->fp[c].signedValue);
    break;
}
case OP_IDIV_U64: {
    if (!vm->fp[c].unsignedValue) {
        divisionByZeroError();
    }

    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsignedValue / vm->fp[c].unsignedValue);
    break;
}
case OP_REM_I64: {
    if (!vm->fp[c].signedValue) {
        divisionByZeroError();
    }

    if (vm->fp[b].signedValue == INT64_MIN && vm->fp[c].signedValue == -1) {
        vm->fp[a] = SIGNED_VALUE(0);
        break;
    }

    vm->fp[a] = SIGNED_VALUE(vm->fp[b].signedValue % vm->fp[c].signedValue);
    break;
}
case OP_REM_U64: {
    if (!vm->fp[c].unsignedValue) {
        divisionByZeroError();
    }

    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsignedValue % vm->fp[c].unsignedValue);
    break;
}
case OP_BAND_I64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsignedValue & vm->fp[c].unsignedValue);
    break;
case OP_BANDI_I64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsignedValue & c);
    break;
case OP_BOR_I64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsignedValue | vm->fp[c].unsignedValue);
    break;
case OP_BORI_I64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsignedValue | c);
    break;
case OP_BXOR_I64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsignedValue ^ vm->fp[c].unsignedValue);
    break;
case OP_BXORI_I64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsignedValue ^ c);
    break;
case OP_BNOT_I64:
    vm->fp[a] = UNSIGNED_VALUE(~vm->fp[b].unsignedValue);
    break;
case OP_LSL_I64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsignedValue << vm->fp[c].unsignedValue);
    break;
case OP_LSLI_I64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsignedValue << c);
    break;
case OP_LSR_I64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsignedValue >> vm->fp[c].unsignedValue);
    break;
case OP_LSRI_I64:
    vm->fp[a] = UNSIGNED_VALUE(vm->fp[b].unsignedValue >> c);
    break;
case OP_ASR_I64:
    vm->fp[a] = SIGNED_VALUE(vm->fp[b].signedValue >> vm->fp[c].signedValue);
    break;
case OP_ASRI_I64:
    vm->fp[a] = SIGNED_VALUE(vm->fp[b].signedValue >> c);
    break;
case OP_NEG_I64:
    vm->fp[a] = SIGNED_VALUE(-vm->fp[b].signedValue);
    break;
case OP_RET_64: {
    Value* frame = vm->fp;
    Instruction* returnIp = AS_POINTER(frame[-2]);
    Value* returnFp = AS_POINTER(frame[-1]);

    frame[-2] = frame[a];
    vm->ip = returnIp;
    vm->fp = returnFp;
    break;
}

case OP_CMP_I64: {
    int64_t left = AS_SIGNED(vm->fp[b]);
    int64_t right = AS_SIGNED(vm->fp[c]);
    int32_t result = (left > right) - (left < right);

    vm->fp[a] = I32_VALUE(result);
    break;
}
case OP_CMP_U64: {
    uint64_t left = AS_UNSIGNED(vm->fp[b]);
    uint64_t right = AS_UNSIGNED(vm->fp[c]);
    int32_t result = (left > right) - (left < right);

    vm->fp[a] = I32_VALUE(result);
    break;
}
case OP_CMPL_F64:
case OP_CMPG_F64: {
    double left = AS_F64(vm->fp[b]);
    double right = AS_F64(vm->fp[c]);
    int32_t result = (left > right) - (left < right);

    if (isnan(left) || isnan(right)) {
        result = opcode == OP_CMPL_F64 ? -1 : 1;
    }

    vm->fp[a] = I32_VALUE(result);
    break;
}
