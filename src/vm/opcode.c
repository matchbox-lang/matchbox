#include "vm/opcode.h"

static const uint8_t opcode_flags[OpCount] = {
    [OpMov] = OpFlagRegAB | OpFlagWritesA,
    [OpLdi] = OpFlagRegA | OpFlagWritesA,
    [OpLdc] = OpFlagRegA | OpFlagWritesA,
    [OpLdg] = OpFlagRegA | OpFlagWritesA,
    [OpStg] = OpFlagRegA,
    [OpStgi] = OpFlagRegA | OpFlagWritesA,
    [OpRefl] = OpFlagRegAB | OpFlagWritesA,
    [OpRefg] = OpFlagRegA | OpFlagWritesA,
    [OpLdr] = OpFlagRegAB | OpFlagWritesA,
    [OpStr] = OpFlagRegAB,
    [OpMov64] = OpFlagRegAB | OpFlagWritesA,
    [OpLdc64] = OpFlagRegA | OpFlagWritesA,
    [OpLdg64] = OpFlagRegA | OpFlagWritesA,
    [OpStg64] = OpFlagRegA,
    [OpLdr64] = OpFlagRegAB | OpFlagWritesA,
    [OpStr64] = OpFlagRegAB,
    [OpLdm8] = OpFlagRegABC | OpFlagWritesA,
    [OpLdmi8] = OpFlagRegAB | OpFlagWritesA,
    [OpLdm16] = OpFlagRegABC | OpFlagWritesA,
    [OpLdmi16] = OpFlagRegAB | OpFlagWritesA,
    [OpLdm32] = OpFlagRegABC | OpFlagWritesA,
    [OpLdmi32] = OpFlagRegAB | OpFlagWritesA,
    [OpLdm64] = OpFlagRegABC | OpFlagWritesA,
    [OpLdmi64] = OpFlagRegAB | OpFlagWritesA,
    [OpStm8] = OpFlagRegABC,
    [OpStmi8] = OpFlagRegAB,
    [OpStm16] = OpFlagRegABC,
    [OpStmi16] = OpFlagRegAB,
    [OpStm32] = OpFlagRegABC,
    [OpStmi32] = OpFlagRegAB,
    [OpStm64] = OpFlagRegABC,
    [OpStmi64] = OpFlagRegAB,
    [OpSextI32] = OpFlagRegAB | OpFlagWritesA,
    [OpZextI32] = OpFlagRegAB | OpFlagWritesA,
    [OpF32ToF64] = OpFlagRegAB | OpFlagWritesA,
    [OpF64ToF32] = OpFlagRegAB | OpFlagWritesA,
    [OpF32ToI32] = OpFlagRegAB | OpFlagWritesA,
    [OpF32ToU32] = OpFlagRegAB | OpFlagWritesA,
    [OpF32ToI64] = OpFlagRegAB | OpFlagWritesA,
    [OpF32ToU64] = OpFlagRegAB | OpFlagWritesA,
    [OpF64ToI32] = OpFlagRegAB | OpFlagWritesA,
    [OpF64ToU32] = OpFlagRegAB | OpFlagWritesA,
    [OpF64ToI64] = OpFlagRegAB | OpFlagWritesA,
    [OpF64ToU64] = OpFlagRegAB | OpFlagWritesA,
    [OpI64ToF32] = OpFlagRegAB | OpFlagWritesA,
    [OpI64ToF64] = OpFlagRegAB | OpFlagWritesA,
    [OpU64ToF32] = OpFlagRegAB | OpFlagWritesA,
    [OpU64ToF64] = OpFlagRegAB | OpFlagWritesA,
    [OpI32ToF32] = OpFlagRegAB | OpFlagWritesA,
    [OpI32ToF64] = OpFlagRegAB | OpFlagWritesA,
    [OpU32ToF32] = OpFlagRegAB | OpFlagWritesA,
    [OpU32ToF64] = OpFlagRegAB | OpFlagWritesA,
    [OpAddF32] = OpFlagRegABC | OpFlagWritesA,
    [OpAddF64] = OpFlagRegABC | OpFlagWritesA,
    [OpSubF32] = OpFlagRegABC | OpFlagWritesA,
    [OpSubF64] = OpFlagRegABC | OpFlagWritesA,
    [OpMulF32] = OpFlagRegABC | OpFlagWritesA,
    [OpMulF64] = OpFlagRegABC | OpFlagWritesA,
    [OpDivF32] = OpFlagRegABC | OpFlagWritesA,
    [OpDivF64] = OpFlagRegABC | OpFlagWritesA,
    [OpRemF32] = OpFlagRegABC | OpFlagWritesA,
    [OpRemF64] = OpFlagRegABC | OpFlagWritesA,
    [OpNegF32] = OpFlagRegAB | OpFlagWritesA,
    [OpNegF64] = OpFlagRegAB | OpFlagWritesA,
    [OpAddI64] = OpFlagRegABC | OpFlagWritesA,
    [OpAddiI64] = OpFlagRegAB | OpFlagWritesA,
    [OpAddI32] = OpFlagRegABC | OpFlagWritesA,
    [OpAddiI32] = OpFlagRegAB | OpFlagWritesA,
    [OpAddI16] = OpFlagRegABC | OpFlagWritesA,
    [OpAddiI16] = OpFlagRegAB | OpFlagWritesA,
    [OpAddI8] = OpFlagRegABC | OpFlagWritesA,
    [OpAddiI8] = OpFlagRegAB | OpFlagWritesA,
    [OpSubI64] = OpFlagRegABC | OpFlagWritesA,
    [OpSubiI64] = OpFlagRegAB | OpFlagWritesA,
    [OpSubI32] = OpFlagRegABC | OpFlagWritesA,
    [OpSubiI32] = OpFlagRegAB | OpFlagWritesA,
    [OpSubI16] = OpFlagRegABC | OpFlagWritesA,
    [OpSubiI16] = OpFlagRegAB | OpFlagWritesA,
    [OpSubI8] = OpFlagRegABC | OpFlagWritesA,
    [OpSubiI8] = OpFlagRegAB | OpFlagWritesA,
    [OpMulI64] = OpFlagRegABC | OpFlagWritesA,
    [OpMuliI64] = OpFlagRegAB | OpFlagWritesA,
    [OpMulI32] = OpFlagRegABC | OpFlagWritesA,
    [OpMuliI32] = OpFlagRegAB | OpFlagWritesA,
    [OpMulI16] = OpFlagRegABC | OpFlagWritesA,
    [OpMuliI16] = OpFlagRegAB | OpFlagWritesA,
    [OpMulI8] = OpFlagRegABC | OpFlagWritesA,
    [OpMuliI8] = OpFlagRegAB | OpFlagWritesA,
    [OpIdivI64] = OpFlagRegABC | OpFlagWritesA,
    [OpIdivU64] = OpFlagRegABC | OpFlagWritesA,
    [OpIdivI32] = OpFlagRegABC | OpFlagWritesA,
    [OpIdivU32] = OpFlagRegABC | OpFlagWritesA,
    [OpIdivI16] = OpFlagRegABC | OpFlagWritesA,
    [OpIdivU16] = OpFlagRegABC | OpFlagWritesA,
    [OpIdivI8] = OpFlagRegABC | OpFlagWritesA,
    [OpIdivU8] = OpFlagRegABC | OpFlagWritesA,
    [OpRemI64] = OpFlagRegABC | OpFlagWritesA,
    [OpRemU64] = OpFlagRegABC | OpFlagWritesA,
    [OpRemI32] = OpFlagRegABC | OpFlagWritesA,
    [OpRemU32] = OpFlagRegABC | OpFlagWritesA,
    [OpRemI16] = OpFlagRegABC | OpFlagWritesA,
    [OpRemU16] = OpFlagRegABC | OpFlagWritesA,
    [OpRemI8] = OpFlagRegABC | OpFlagWritesA,
    [OpRemU8] = OpFlagRegABC | OpFlagWritesA,
    [OpBandI64] = OpFlagRegABC | OpFlagWritesA,
    [OpBandiI64] = OpFlagRegAB | OpFlagWritesA,
    [OpBandI32] = OpFlagRegABC | OpFlagWritesA,
    [OpBandiI32] = OpFlagRegAB | OpFlagWritesA,
    [OpBandI16] = OpFlagRegABC | OpFlagWritesA,
    [OpBandiI16] = OpFlagRegAB | OpFlagWritesA,
    [OpBandI8] = OpFlagRegABC | OpFlagWritesA,
    [OpBandiI8] = OpFlagRegAB | OpFlagWritesA,
    [OpBorI64] = OpFlagRegABC | OpFlagWritesA,
    [OpBoriI64] = OpFlagRegAB | OpFlagWritesA,
    [OpBorI32] = OpFlagRegABC | OpFlagWritesA,
    [OpBoriI32] = OpFlagRegAB | OpFlagWritesA,
    [OpBorI16] = OpFlagRegABC | OpFlagWritesA,
    [OpBoriI16] = OpFlagRegAB | OpFlagWritesA,
    [OpBorI8] = OpFlagRegABC | OpFlagWritesA,
    [OpBoriI8] = OpFlagRegAB | OpFlagWritesA,
    [OpBxorI64] = OpFlagRegABC | OpFlagWritesA,
    [OpBxoriI64] = OpFlagRegAB | OpFlagWritesA,
    [OpBxorI32] = OpFlagRegABC | OpFlagWritesA,
    [OpBxoriI32] = OpFlagRegAB | OpFlagWritesA,
    [OpBxorI16] = OpFlagRegABC | OpFlagWritesA,
    [OpBxoriI16] = OpFlagRegAB | OpFlagWritesA,
    [OpBxorI8] = OpFlagRegABC | OpFlagWritesA,
    [OpBxoriI8] = OpFlagRegAB | OpFlagWritesA,
    [OpBnotI64] = OpFlagRegAB | OpFlagWritesA,
    [OpBnotI32] = OpFlagRegAB | OpFlagWritesA,
    [OpBnotI16] = OpFlagRegAB | OpFlagWritesA,
    [OpBnotI8] = OpFlagRegAB | OpFlagWritesA,
    [OpLslI64] = OpFlagRegABC | OpFlagWritesA,
    [OpLsliI64] = OpFlagRegAB | OpFlagWritesA,
    [OpLslI32] = OpFlagRegABC | OpFlagWritesA,
    [OpLsliI32] = OpFlagRegAB | OpFlagWritesA,
    [OpLslI16] = OpFlagRegABC | OpFlagWritesA,
    [OpLsliI16] = OpFlagRegAB | OpFlagWritesA,
    [OpLslI8] = OpFlagRegABC | OpFlagWritesA,
    [OpLsliI8] = OpFlagRegAB | OpFlagWritesA,
    [OpLsrI64] = OpFlagRegABC | OpFlagWritesA,
    [OpLsriI64] = OpFlagRegAB | OpFlagWritesA,
    [OpLsrI32] = OpFlagRegABC | OpFlagWritesA,
    [OpLsriI32] = OpFlagRegAB | OpFlagWritesA,
    [OpLsrI16] = OpFlagRegABC | OpFlagWritesA,
    [OpLsriI16] = OpFlagRegAB | OpFlagWritesA,
    [OpLsrI8] = OpFlagRegABC | OpFlagWritesA,
    [OpLsriI8] = OpFlagRegAB | OpFlagWritesA,
    [OpAsrI64] = OpFlagRegABC | OpFlagWritesA,
    [OpAsriI64] = OpFlagRegAB | OpFlagWritesA,
    [OpAsrI32] = OpFlagRegABC | OpFlagWritesA,
    [OpAsriI32] = OpFlagRegAB | OpFlagWritesA,
    [OpAsrI16] = OpFlagRegABC | OpFlagWritesA,
    [OpAsriI16] = OpFlagRegAB | OpFlagWritesA,
    [OpAsrI8] = OpFlagRegABC | OpFlagWritesA,
    [OpAsriI8] = OpFlagRegAB | OpFlagWritesA,
    [OpAnd] = OpFlagRegABC | OpFlagWritesA,
    [OpOr] = OpFlagRegABC | OpFlagWritesA,
    [OpNot] = OpFlagRegAB | OpFlagWritesA,
    [OpNegI64] = OpFlagRegAB | OpFlagWritesA,
    [OpNegI32] = OpFlagRegAB | OpFlagWritesA,
    [OpNegI16] = OpFlagRegAB | OpFlagWritesA,
    [OpNegI8] = OpFlagRegAB | OpFlagWritesA,
    [OpEq] = OpFlagRegABC | OpFlagWritesA,
    [OpEqi] = OpFlagRegAB | OpFlagWritesA,
    [OpNe] = OpFlagRegABC | OpFlagWritesA,
    [OpNei] = OpFlagRegAB | OpFlagWritesA,
    [OpEqString] = OpFlagRegABC | OpFlagWritesA,
    [OpNeString] = OpFlagRegABC | OpFlagWritesA,
    [OpLtI32] = OpFlagRegABC | OpFlagWritesA,
    [OpLtU32] = OpFlagRegABC | OpFlagWritesA,
    [OpLeI32] = OpFlagRegABC | OpFlagWritesA,
    [OpLeU32] = OpFlagRegABC | OpFlagWritesA,
    [OpCmpI32] = OpFlagRegABC | OpFlagWritesA,
    [OpCmpU32] = OpFlagRegABC | OpFlagWritesA,
    [OpCmpI64] = OpFlagRegABC | OpFlagWritesA,
    [OpCmpU64] = OpFlagRegABC | OpFlagWritesA,
    [OpCmplF32] = OpFlagRegABC | OpFlagWritesA,
    [OpCmpgF32] = OpFlagRegABC | OpFlagWritesA,
    [OpCmplF64] = OpFlagRegABC | OpFlagWritesA,
    [OpCmpgF64] = OpFlagRegABC | OpFlagWritesA,
    [OpBz] = OpFlagRegA,
    [OpBnz] = OpFlagRegA,
    [OpBeq] = OpFlagRegAB,
    [OpBeqi] = OpFlagRegA,
    [OpBne] = OpFlagRegAB,
    [OpBnei] = OpFlagRegA,
    [OpBltI32] = OpFlagRegAB,
    [OpBltU32] = OpFlagRegAB,
    [OpBleI32] = OpFlagRegAB,
    [OpBleU32] = OpFlagRegAB,
    [OpCall] = OpFlagRegA,
    [OpRet64] = OpFlagRegA,
    [OpRetv] = OpFlagRegA,
    [OpCallCall] = OpFlagRegA,
    [OpLdiLdi] = OpFlagRegA | OpFlagWritesA,
    [OpLdcLdc] = OpFlagRegA | OpFlagWritesA,
    [OpLdgLdg] = OpFlagRegA | OpFlagWritesA,
    [OpMovMov] = OpFlagRegABC | OpFlagWritesA,
    [OpLdiCall] = OpFlagRegA,
    [OpLdcCall] = OpFlagRegA,
    [OpLdgCall] = OpFlagRegA,
    [OpLdc64Call] = OpFlagRegA,
    [OpLdg64Call] = OpFlagRegA,
    [OpLdrCall] = OpFlagRegA,
    [OpStgCall] = OpFlagRegA,
    [OpLdiLdg] = OpFlagRegA | OpFlagWritesA,
    [OpLdgLdi] = OpFlagRegA | OpFlagWritesA,
    [OpMovLdi] = OpFlagRegAB,
    [OpMovLdc] = OpFlagRegAB,
    [OpMovLdg] = OpFlagRegAB
};

Opcode get_arithmetic_immediate_opcode(Opcode opcode)
{
    switch (opcode) {
        case OpAddI64:    return OpAddiI64;
        case OpAddI32:    return OpAddiI32;
        case OpAddI16:    return OpAddiI16;
        case OpAddI8:     return OpAddiI8;
        case OpSubI64:    return OpSubiI64;
        case OpSubI32:    return OpSubiI32;
        case OpSubI16:    return OpSubiI16;
        case OpSubI8:     return OpSubiI8;
        case OpMulI64:    return OpMuliI64;
        case OpMulI32:    return OpMuliI32;
        case OpMulI16:    return OpMuliI16;
        case OpMulI8:     return OpMuliI8;
        case OpBandI64:   return OpBandiI64;
        case OpBandI32:   return OpBandiI32;
        case OpBandI16:   return OpBandiI16;
        case OpBandI8:    return OpBandiI8;
        case OpBorI64:    return OpBoriI64;
        case OpBorI32:    return OpBoriI32;
        case OpBorI16:    return OpBoriI16;
        case OpBorI8:     return OpBoriI8;
        case OpBxorI64:   return OpBxoriI64;
        case OpBxorI32:   return OpBxoriI32;
        case OpBxorI16:   return OpBxoriI16;
        case OpBxorI8:    return OpBxoriI8;
        case OpLslI64:    return OpLsliI64;
        case OpLslI32:    return OpLsliI32;
        case OpLslI16:    return OpLsliI16;
        case OpLslI8:     return OpLsliI8;
        case OpLsrI64:    return OpLsriI64;
        case OpLsrI32:    return OpLsriI32;
        case OpLsrI16:    return OpLsriI16;
        case OpLsrI8:     return OpLsriI8;
        case OpAsrI64:    return OpAsriI64;
        case OpAsrI32:    return OpAsriI32;
        case OpAsrI16:    return OpAsriI16;
        case OpAsrI8:     return OpAsriI8;
        default:
            return OpHlt;
    }
}

Opcode get_immediate_opcode(Opcode opcode)
{
    switch (opcode) {
        case OpLdm8:    return OpLdmi8;
        case OpLdm16:   return OpLdmi16;
        case OpLdm32:   return OpLdmi32;
        case OpLdm64:   return OpLdmi64;
        case OpStm8:    return OpStmi8;
        case OpStm16:   return OpStmi16;
        case OpStm32:   return OpStmi32;
        case OpStm64:   return OpStmi64;
        default:
            return opcode;
    }
}

Opcode get_jump_comparison_opcode(Opcode opcode)
{
    switch (opcode) {
        case OpBeq:     return OpNe;
        case OpBeqi:    return OpNei;
        case OpBne:     return OpEq;
        case OpBnei:    return OpEqi;
        case OpBltI32:  return OpLeI32;
        case OpBltU32:  return OpLeU32;
        case OpBleI32:  return OpLtI32;
        case OpBleU32:  return OpLtU32;
        default:
            return OpHlt;
    }
}

uint8_t get_opcode_register_flags(Opcode opcode)
{
    if ((unsigned)opcode >= OpCount) {
        return 0;
    }

    return opcode_flags[opcode] & OpFlagRegABC;
}

Opcode load_call_opcode(Opcode opcode)
{
    switch (opcode) {
        case OpLdi:     return OpLdiCall;
        case OpLdc:     return OpLdcCall;
        case OpLdg:     return OpLdgCall;
        case OpLdr:     return OpLdrCall;
        case OpLdc64:   return OpLdc64Call;
        case OpLdg64:   return OpLdg64Call;
        default:
            return OpHlt;
    }
}

Opcode move_load_opcode(Opcode opcode)
{
    switch (opcode) {
        case OpLdi:
            return OpMovLdi;
        case OpLdc:
            return OpMovLdc;
        case OpLdg:
            return OpMovLdg;
        default:
            return OpHlt;
    }
}

bool opcode_writes_a(Opcode opcode)
{
    if ((unsigned)opcode >= OpCount) {
        return false;
    }

    return opcode_flags[opcode] & OpFlagWritesA;
}
