#define READ_U64(values, index) \
    ((values)[(index)].unsignedValue \
        | ((uint64_t)(values)[(index) + 1].unsignedValue << 32))

#define WRITE_U64(values, index, value) { \
    uint64_t wideValue = (value); \
    (values)[(index)].unsignedValue = wideValue; \
    (values)[(index) + 1].unsignedValue = wideValue >> 32; \
}

case OP_MOV_I64:
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b));
    break;
case OP_LDC_I64:
    WRITE_U64(vm->fp, a, READ_U64(vm->module->constants.data, OPERAND_BC(inst)));
    break;
case OP_LDG_I64:
    WRITE_U64(vm->fp, a, READ_U64(vm->gp, OPERAND_BC(inst)));
    break;
case OP_STG_I64:
    WRITE_U64(vm->gp, OPERAND_BC(inst), READ_U64(vm->fp, a));
    break;
case OP_LDR_I64:
    vm->fp[a] = *(Value*)AS_POINTER(vm->fp[b]);
    vm->fp[a + 1] = *((Value*)AS_POINTER(vm->fp[b]) + 1);
    break;
case OP_STR_I64:
    *(Value*)AS_POINTER(vm->fp[b]) = vm->fp[a];
    *((Value*)AS_POINTER(vm->fp[b]) + 1) = vm->fp[a + 1];
    break;
case OP_ADD_I64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) + READ_U64(vm->fp, c));
    break;
}
case OP_ADDI_I64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) + (int8_t)c);
    break;
}
case OP_SUB_I64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) - READ_U64(vm->fp, c));
    break;
}
case OP_SUBI_I64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) - (int8_t)c);
    break;
}
case OP_MUL_I64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) * READ_U64(vm->fp, c));
    break;
}
case OP_MULI_I64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) * (int8_t)c);
    break;
}
case OP_IDIV_I64: {
    if (!READ_U64(vm->fp, c)) {
        divisionByZeroError();
    }

    if (READ_U64(vm->fp, b) == (UINT64_C(1) << 63) && READ_U64(vm->fp, c) == UINT64_MAX) {
        WRITE_U64(vm->fp, a, UINT64_C(1) << 63);
        break;
    }

    WRITE_U64(vm->fp, a, (int64_t)READ_U64(vm->fp, b) / (int64_t)READ_U64(vm->fp, c));
    break;
}
case OP_IDIV_U64: {
    if (!READ_U64(vm->fp, c)) {
        divisionByZeroError();
    }

    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) / READ_U64(vm->fp, c));
    break;
}
case OP_REM_I64: {
    if (!READ_U64(vm->fp, c)) {
        divisionByZeroError();
    }

    if (READ_U64(vm->fp, b) == (UINT64_C(1) << 63) && READ_U64(vm->fp, c) == UINT64_MAX) {
        WRITE_U64(vm->fp, a, 0);
        break;
    }

    WRITE_U64(vm->fp, a, (int64_t)READ_U64(vm->fp, b) % (int64_t)READ_U64(vm->fp, c));
    break;
}
case OP_REM_U64: {
    if (!READ_U64(vm->fp, c)) {
        divisionByZeroError();
    }

    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) % READ_U64(vm->fp, c));
    break;
}
case OP_BAND_I64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) & READ_U64(vm->fp, c));
    break;
}
case OP_BANDI_I64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) & c);
    break;
}
case OP_BOR_I64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) | READ_U64(vm->fp, c));
    break;
}
case OP_BORI_I64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) | c);
    break;
}
case OP_BXOR_I64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) ^ READ_U64(vm->fp, c));
    break;
}
case OP_BXORI_I64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) ^ c);
    break;
}
case OP_BNOT_I64: {
    WRITE_U64(vm->fp, a, ~READ_U64(vm->fp, b));
    break;
}
case OP_LSL_I64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) << READ_U64(vm->fp, c));
    break;
}
case OP_LSLI_I64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) << c);
    break;
}
case OP_LSR_I64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) >> READ_U64(vm->fp, c));
    break;
}
case OP_LSRI_I64: {
    WRITE_U64(vm->fp, a, READ_U64(vm->fp, b) >> c);
    break;
}
case OP_ASR_I64: {
    WRITE_U64(vm->fp, a, (int64_t)READ_U64(vm->fp, b) >> READ_U64(vm->fp, c));
    break;
}
case OP_ASRI_I64: {
    WRITE_U64(vm->fp, a, (int64_t)READ_U64(vm->fp, b) >> c);
    break;
}
case OP_NEG_I64: {
    WRITE_U64(vm->fp, a, -READ_U64(vm->fp, b));
    break;
}
case OP_RET_I64: {
    Value* frame = vm->fp;
    Instruction* returnIp = AS_POINTER(frame[-2]);
    Value* returnFp = AS_POINTER(frame[-1]);

    frame[-2] = frame[a];
    frame[-1] = frame[a + 1];
    vm->ip = returnIp;
    vm->fp = returnFp;
    break;
}

#undef READ_U64
#undef WRITE_U64
