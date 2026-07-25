case OP_MOV_I64:
    vm->fp[a] = vm->fp[b];
    vm->fp[a + 1] = vm->fp[b + 1];
    break;
case OP_LDC_I64:
    vm->fp[a] = vm->module->constants.data[OPERAND_BC(inst)];
    vm->fp[a + 1] = vm->module->constants.data[OPERAND_BC(inst) + 1];
    break;
case OP_LDG_I64:
    vm->fp[a] = vm->gp[OPERAND_BC(inst)];
    vm->fp[a + 1] = vm->gp[OPERAND_BC(inst) + 1];
    break;
case OP_STG_I64:
    vm->gp[OPERAND_BC(inst)] = vm->fp[a];
    vm->gp[OPERAND_BC(inst) + 1] = vm->fp[a + 1];
    break;
case OP_LDR_I64:
    vm->fp[a] = *(Value*)AS_POINTER(vm->fp[b]);
    vm->fp[a + 1] = *((Value*)AS_POINTER(vm->fp[b]) + 1);
    break;
case OP_STR_I64:
    *(Value*)AS_POINTER(vm->fp[b]) = vm->fp[a];
    *((Value*)AS_POINTER(vm->fp[b]) + 1) = vm->fp[a + 1];
    break;
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
case OP_ADD_I64: {
    uint32_t leftLow = vm->fp[b].unsignedValue;
    uint32_t leftHigh = vm->fp[b + 1].unsignedValue;
    uint32_t rightLow = vm->fp[c].unsignedValue;
    uint32_t rightHigh = vm->fp[c + 1].unsignedValue;

    uint32_t low = leftLow + rightLow;
    uint32_t high = leftHigh + rightHigh + (low < leftLow);

    vm->fp[a].unsignedValue = low;
    vm->fp[a + 1].unsignedValue = high;
    break;
}
case OP_ADDI_I64: {
    uint32_t leftLow = vm->fp[b].unsignedValue;
    uint32_t leftHigh = vm->fp[b + 1].unsignedValue;

    uint64_t left = leftLow | (uint64_t)leftHigh << 32;
    uint64_t result = left + (int8_t)c;
    uint32_t low = (uint32_t)result;
    uint32_t high = (uint32_t)(result >> 32);

    vm->fp[a].unsignedValue = low;
    vm->fp[a + 1].unsignedValue = high;
    break;
}
case OP_SUB_I64: {
    uint32_t leftLow = vm->fp[b].unsignedValue;
    uint32_t leftHigh = vm->fp[b + 1].unsignedValue;
    uint32_t rightLow = vm->fp[c].unsignedValue;
    uint32_t rightHigh = vm->fp[c + 1].unsignedValue;

    uint64_t left = leftLow | (uint64_t)leftHigh << 32;
    uint64_t right = rightLow | (uint64_t)rightHigh << 32;
    uint64_t result = left - right;
    uint32_t low = (uint32_t)result;
    uint32_t high = (uint32_t)(result >> 32);

    vm->fp[a].unsignedValue = low;
    vm->fp[a + 1].unsignedValue = high;
    break;
}
case OP_SUBI_I64: {
    uint32_t leftLow = vm->fp[b].unsignedValue;
    uint32_t leftHigh = vm->fp[b + 1].unsignedValue;

    uint64_t left = leftLow | (uint64_t)leftHigh << 32;
    uint64_t result = left - (int8_t)c;
    uint32_t low = (uint32_t)result;
    uint32_t high = (uint32_t)(result >> 32);

    vm->fp[a].unsignedValue = low;
    vm->fp[a + 1].unsignedValue = high;
    break;
}
case OP_MUL_I64: {
    uint32_t leftLow = vm->fp[b].unsignedValue;
    uint32_t leftHigh = vm->fp[b + 1].unsignedValue;
    uint32_t rightLow = vm->fp[c].unsignedValue;
    uint32_t rightHigh = vm->fp[c + 1].unsignedValue;

    uint64_t left = leftLow | (uint64_t)leftHigh << 32;
    uint64_t right = rightLow | (uint64_t)rightHigh << 32;
    uint64_t result = left * right;
    uint32_t low = (uint32_t)result;
    uint32_t high = (uint32_t)(result >> 32);

    vm->fp[a].unsignedValue = low;
    vm->fp[a + 1].unsignedValue = high;
    break;
}
case OP_IDIV_I64: {
    uint32_t leftLow = vm->fp[b].unsignedValue;
    uint32_t leftHigh = vm->fp[b + 1].unsignedValue;
    uint32_t rightLow = vm->fp[c].unsignedValue;
    uint32_t rightHigh = vm->fp[c + 1].unsignedValue;

    uint64_t left = leftLow | (uint64_t)leftHigh << 32;
    uint64_t right = rightLow | (uint64_t)rightHigh << 32;
    uint64_t result = (int64_t)left / (int64_t)right;
    uint32_t low = (uint32_t)result;
    uint32_t high = (uint32_t)(result >> 32);

    vm->fp[a].unsignedValue = low;
    vm->fp[a + 1].unsignedValue = high;
    break;
}
case OP_IDIV_U64: {
    uint32_t leftLow = vm->fp[b].unsignedValue;
    uint32_t leftHigh = vm->fp[b + 1].unsignedValue;
    uint32_t rightLow = vm->fp[c].unsignedValue;
    uint32_t rightHigh = vm->fp[c + 1].unsignedValue;

    uint64_t left = leftLow | (uint64_t)leftHigh << 32;
    uint64_t right = rightLow | (uint64_t)rightHigh << 32;
    uint64_t result = left / right;
    uint32_t low = (uint32_t)result;
    uint32_t high = (uint32_t)(result >> 32);

    vm->fp[a].unsignedValue = low;
    vm->fp[a + 1].unsignedValue = high;
    break;
}
case OP_REM_I64: {
    uint32_t leftLow = vm->fp[b].unsignedValue;
    uint32_t leftHigh = vm->fp[b + 1].unsignedValue;
    uint32_t rightLow = vm->fp[c].unsignedValue;
    uint32_t rightHigh = vm->fp[c + 1].unsignedValue;

    uint64_t left = leftLow | (uint64_t)leftHigh << 32;
    uint64_t right = rightLow | (uint64_t)rightHigh << 32;
    uint64_t result = (int64_t)left % (int64_t)right;
    uint32_t low = (uint32_t)result;
    uint32_t high = (uint32_t)(result >> 32);

    vm->fp[a].unsignedValue = low;
    vm->fp[a + 1].unsignedValue = high;
    break;
}
case OP_REM_U64: {
    uint32_t leftLow = vm->fp[b].unsignedValue;
    uint32_t leftHigh = vm->fp[b + 1].unsignedValue;
    uint32_t rightLow = vm->fp[c].unsignedValue;
    uint32_t rightHigh = vm->fp[c + 1].unsignedValue;

    uint64_t left = leftLow | (uint64_t)leftHigh << 32;
    uint64_t right = rightLow | (uint64_t)rightHigh << 32;
    uint64_t result = left % right;
    uint32_t low = (uint32_t)result;
    uint32_t high = (uint32_t)(result >> 32);

    vm->fp[a].unsignedValue = low;
    vm->fp[a + 1].unsignedValue = high;
    break;
}
case OP_BAND_I64: {
    uint32_t leftLow = vm->fp[b].unsignedValue;
    uint32_t leftHigh = vm->fp[b + 1].unsignedValue;
    uint32_t rightLow = vm->fp[c].unsignedValue;
    uint32_t rightHigh = vm->fp[c + 1].unsignedValue;
    uint32_t low = leftLow & rightLow;
    uint32_t high = leftHigh & rightHigh;

    vm->fp[a].unsignedValue = low;
    vm->fp[a + 1].unsignedValue = high;
    break;
}
case OP_BANDI_I64: {
    uint32_t low = vm->fp[b].unsignedValue & c;
    uint32_t high = 0;

    vm->fp[a].unsignedValue = low;
    vm->fp[a + 1].unsignedValue = high;
    break;
}
case OP_BOR_I64: {
    uint32_t leftLow = vm->fp[b].unsignedValue;
    uint32_t leftHigh = vm->fp[b + 1].unsignedValue;
    uint32_t rightLow = vm->fp[c].unsignedValue;
    uint32_t rightHigh = vm->fp[c + 1].unsignedValue;
    uint32_t low = leftLow | rightLow;
    uint32_t high = leftHigh | rightHigh;

    vm->fp[a].unsignedValue = low;
    vm->fp[a + 1].unsignedValue = high;
    break;
}
case OP_BORI_I64: {
    uint32_t low = vm->fp[b].unsignedValue | c;
    uint32_t high = vm->fp[b + 1].unsignedValue;

    vm->fp[a].unsignedValue = low;
    vm->fp[a + 1].unsignedValue = high;
    break;
}
case OP_BXOR_I64: {
    uint32_t leftLow = vm->fp[b].unsignedValue;
    uint32_t leftHigh = vm->fp[b + 1].unsignedValue;
    uint32_t rightLow = vm->fp[c].unsignedValue;
    uint32_t rightHigh = vm->fp[c + 1].unsignedValue;
    uint32_t low = leftLow ^ rightLow;
    uint32_t high = leftHigh ^ rightHigh;

    vm->fp[a].unsignedValue = low;
    vm->fp[a + 1].unsignedValue = high;
    break;
}
case OP_BXORI_I64: {
    uint32_t low = vm->fp[b].unsignedValue ^ c;
    uint32_t high = vm->fp[b + 1].unsignedValue;

    vm->fp[a].unsignedValue = low;
    vm->fp[a + 1].unsignedValue = high;
    break;
}
case OP_BNOT_I64: {
    uint32_t low = ~vm->fp[b].unsignedValue;
    uint32_t high = ~vm->fp[b + 1].unsignedValue;

    vm->fp[a].unsignedValue = low;
    vm->fp[a + 1].unsignedValue = high;
    break;
}
case OP_LSL_I64: {
    uint32_t leftLow = vm->fp[b].unsignedValue;
    uint32_t leftHigh = vm->fp[b + 1].unsignedValue;
    uint32_t rightLow = vm->fp[c].unsignedValue;
    uint32_t rightHigh = vm->fp[c + 1].unsignedValue;

    uint64_t left = leftLow | (uint64_t)leftHigh << 32;
    uint64_t right = rightLow | (uint64_t)rightHigh << 32;
    uint64_t result = left << right;
    uint32_t low = (uint32_t)result;
    uint32_t high = (uint32_t)(result >> 32);

    vm->fp[a].unsignedValue = low;
    vm->fp[a + 1].unsignedValue = high;
    break;
}
case OP_LSLI_I64: {
    uint32_t leftLow = vm->fp[b].unsignedValue;
    uint32_t leftHigh = vm->fp[b + 1].unsignedValue;

    uint64_t left = leftLow | (uint64_t)leftHigh << 32;
    uint64_t result = left << c;
    uint32_t low = (uint32_t)result;
    uint32_t high = (uint32_t)(result >> 32);

    vm->fp[a].unsignedValue = low;
    vm->fp[a + 1].unsignedValue = high;
    break;
}
case OP_LSR_I64: {
    uint32_t leftLow = vm->fp[b].unsignedValue;
    uint32_t leftHigh = vm->fp[b + 1].unsignedValue;
    uint32_t rightLow = vm->fp[c].unsignedValue;
    uint32_t rightHigh = vm->fp[c + 1].unsignedValue;

    uint64_t left = leftLow | (uint64_t)leftHigh << 32;
    uint64_t right = rightLow | (uint64_t)rightHigh << 32;
    uint64_t result = left >> right;
    uint32_t low = (uint32_t)result;
    uint32_t high = (uint32_t)(result >> 32);

    vm->fp[a].unsignedValue = low;
    vm->fp[a + 1].unsignedValue = high;
    break;
}
case OP_LSRI_I64: {
    uint32_t leftLow = vm->fp[b].unsignedValue;
    uint32_t leftHigh = vm->fp[b + 1].unsignedValue;

    uint64_t left = leftLow | (uint64_t)leftHigh << 32;
    uint64_t result = left >> c;
    uint32_t low = (uint32_t)result;
    uint32_t high = (uint32_t)(result >> 32);

    vm->fp[a].unsignedValue = low;
    vm->fp[a + 1].unsignedValue = high;
    break;
}
case OP_ASR_I64: {
    uint32_t leftLow = vm->fp[b].unsignedValue;
    uint32_t leftHigh = vm->fp[b + 1].unsignedValue;
    uint32_t rightLow = vm->fp[c].unsignedValue;
    uint32_t rightHigh = vm->fp[c + 1].unsignedValue;

    uint64_t left = leftLow | (uint64_t)leftHigh << 32;
    uint64_t right = rightLow | (uint64_t)rightHigh << 32;
    uint64_t result = (int64_t)left >> right;
    uint32_t low = (uint32_t)result;
    uint32_t high = (uint32_t)(result >> 32);

    vm->fp[a].unsignedValue = low;
    vm->fp[a + 1].unsignedValue = high;
    break;
}
case OP_ASRI_I64: {
    uint32_t leftLow = vm->fp[b].unsignedValue;
    uint32_t leftHigh = vm->fp[b + 1].unsignedValue;

    uint64_t left = leftLow | (uint64_t)leftHigh << 32;
    uint64_t result = (int64_t)left >> c;
    uint32_t low = (uint32_t)result;
    uint32_t high = (uint32_t)(result >> 32);

    vm->fp[a].unsignedValue = low;
    vm->fp[a + 1].unsignedValue = high;
    break;
}
case OP_NEG_I64: {
    uint32_t leftLow = vm->fp[b].unsignedValue;
    uint32_t leftHigh = vm->fp[b + 1].unsignedValue;

    uint64_t left = leftLow | (uint64_t)leftHigh << 32;
    uint64_t result = -(int64_t)left;
    uint32_t low = (uint32_t)result;
    uint32_t high = (uint32_t)(result >> 32);

    vm->fp[a].unsignedValue = low;
    vm->fp[a + 1].unsignedValue = high;
    break;
}
