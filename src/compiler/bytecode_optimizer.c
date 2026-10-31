#include "compiler/bytecode_optimizer.h"
#include "common/memory.h"
#include "common/util.h"
#include "runtime/function_object.h"
#include "vm/opcode.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct OptimizationWord
{
    Opcode opcode;
    uint8_t a, b, c;
    bool removed;
    bool target;
    bool wide;
    size_t destination;
} OptimizationWord;

typedef struct KnownConstant
{
    bool known;
    bool wide;
    int64_t value;
    Opcode loadOpcode;
    uint16_t operand;
} KnownConstant;

static bool usesRegisterPairs(Opcode opcode)
{
    switch (opcode) {
        case OP_CMP_I64:
        case OP_CMP_U64:
        case OP_CMPL_F64:
        case OP_CMPG_F64:
        case OP_MOV_64:
        case OP_LDC_64:
        case OP_LDG_64:
        case OP_LDC_64_CALL:
        case OP_LDG_64_CALL:
        case OP_STG_64:
        case OP_LDR_64:
        case OP_STR_64:
        case OP_F32_TO_F64:
        case OP_F64_TO_F32:
        case OP_F32_TO_I64:
        case OP_F32_TO_U64:
        case OP_F64_TO_I32:
        case OP_F64_TO_U32:
        case OP_F64_TO_I64:
        case OP_F64_TO_U64:
        case OP_I64_TO_F32:
        case OP_I64_TO_F64:
        case OP_U64_TO_F32:
        case OP_U64_TO_F64:
        case OP_I32_TO_F64:
        case OP_U32_TO_F64:
        case OP_ADD_F64:
        case OP_SUB_F64:
        case OP_MUL_F64:
        case OP_DIV_F64:
        case OP_REM_F64:
        case OP_NEG_F64:
        case OP_RET_64:
        case OP_ADD_I64:
        case OP_ADDI_I64:
        case OP_SUB_I64:
        case OP_SUBI_I64:
        case OP_MUL_I64:
        case OP_MULI_I64:
        case OP_IDIV_I64:
        case OP_IDIV_U64:
        case OP_REM_I64:
        case OP_REM_U64:
        case OP_BAND_I64:
        case OP_BANDI_I64:
        case OP_BOR_I64:
        case OP_BORI_I64:
        case OP_BXOR_I64:
        case OP_BXORI_I64:
        case OP_BNOT_I64:
        case OP_LSL_I64:
        case OP_LSLI_I64:
        case OP_LSR_I64:
        case OP_LSRI_I64:
        case OP_ASR_I64:
        case OP_ASRI_I64:
        case OP_NEG_I64:
            return true;
        default: return false;
    }
}

static bool isBranch(Opcode opcode)
{
    return opcode >= OP_BZ && opcode <= OP_JMP;
}

static bool endsBlock(Opcode opcode)
{
    bool branch = isBranch(opcode);

    return branch || opcode == OP_HLT || opcode == OP_RET || opcode == OP_RETV || opcode == OP_RET_64;
}

static int branchOffset(OptimizationWord* word)
{
    if (word->opcode == OP_BZ || word->opcode == OP_BNZ) {
        return (int16_t)(word->b << 8 | word->c);
    }

    if (word->opcode != OP_JMP) {
        return (int8_t)word->c;
    }

    uint32_t offset = word->a << 16 | word->b << 8 | word->c;

    return (int32_t)(offset ^ 0x800000u) - 0x800000;
}

static void decodeWords(CodeObject* code, OptimizationWord* words, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        uint8_t* bytes = code->data + i * INSTRUCTION_SIZE;
        OptimizationWord* word = &words[i];

        word->opcode = bytes[0];
        word->a = bytes[1];
        word->b = bytes[2];
        word->c = bytes[3];
        word->wide = i > 0 && words[i - 1].opcode == OP_WIDE;
        word->destination = count;
    }

    for (size_t i = 0; i < count; i++) {
        OptimizationWord* word = &words[i];
        bool branch = isBranch(word->opcode);

        if (!branch) {
            continue;
        }

        int offset = branchOffset(word);
        size_t destination = i + 1 + offset;

        word->destination = destination;
        words[destination].target = true;
    }
}

static Opcode immediateOpcode(Opcode opcode)
{
    switch (opcode) {
        case OP_ADD_I64:    return OP_ADDI_I64;
        case OP_ADD_I32:    return OP_ADDI_I32;
        case OP_ADD_I16:    return OP_ADDI_I16;
        case OP_ADD_I8:     return OP_ADDI_I8;
        case OP_SUB_I64:    return OP_SUBI_I64;
        case OP_SUB_I32:    return OP_SUBI_I32;
        case OP_SUB_I16:    return OP_SUBI_I16;
        case OP_SUB_I8:     return OP_SUBI_I8;
        case OP_MUL_I64:    return OP_MULI_I64;
        case OP_MUL_I32:    return OP_MULI_I32;
        case OP_MUL_I16:    return OP_MULI_I16;
        case OP_MUL_I8:     return OP_MULI_I8;
        case OP_BAND_I64:   return OP_BANDI_I64;
        case OP_BAND_I32:   return OP_BANDI_I32;
        case OP_BAND_I16:   return OP_BANDI_I16;
        case OP_BAND_I8:    return OP_BANDI_I8;
        case OP_BOR_I64:    return OP_BORI_I64;
        case OP_BOR_I32:    return OP_BORI_I32;
        case OP_BOR_I16:    return OP_BORI_I16;
        case OP_BOR_I8:     return OP_BORI_I8;
        case OP_BXOR_I64:   return OP_BXORI_I64;
        case OP_BXOR_I32:   return OP_BXORI_I32;
        case OP_BXOR_I16:   return OP_BXORI_I16;
        case OP_BXOR_I8:    return OP_BXORI_I8;
        case OP_LSL_I64:    return OP_LSLI_I64;
        case OP_LSL_I32:    return OP_LSLI_I32;
        case OP_LSL_I16:    return OP_LSLI_I16;
        case OP_LSL_I8:     return OP_LSLI_I8;
        case OP_LSR_I64:    return OP_LSRI_I64;
        case OP_LSR_I32:    return OP_LSRI_I32;
        case OP_LSR_I16:    return OP_LSRI_I16;
        case OP_LSR_I8:     return OP_LSRI_I8;
        case OP_ASR_I64:    return OP_ASRI_I64;
        case OP_ASR_I32:    return OP_ASRI_I32;
        case OP_ASR_I16:    return OP_ASRI_I16;
        case OP_ASR_I8:     return OP_ASRI_I8;
        default:            return OP_HLT;
    }
}

static bool invalidatesConstants(Opcode opcode)
{
    bool end = endsBlock(opcode);

    return end || opcode == OP_CALL || opcode == OP_STG_CALL || opcode == OP_STG || opcode == OP_STG_64
        || opcode == OP_STR || opcode == OP_STR_64
        || opcode == OP_REFL || opcode == OP_REFG || opcode == OP_WIDE;
}

static void selectImmediateArithmetic(OptimizationWord* word, KnownConstant* known)
{
    KnownConstant right = known[word->c];
    Opcode immediate = immediateOpcode(word->opcode);
    bool signedOperand = word->opcode >= OP_ADD_I64 && word->opcode <= OP_MULI_I8;
    int64_t minimum = signedOperand ? INT8_MIN : 0;
    int64_t maximum = signedOperand ? INT8_MAX : UINT8_MAX;
    bool pairs = usesRegisterPairs(word->opcode);

    if (immediate == OP_HLT || !right.known || right.wide != pairs
        || right.value < minimum || right.value > maximum) {
        return;
    }

    word->opcode = immediate;
    word->c = right.value;
}

static void selectConstantMove(OptimizationWord* word, KnownConstant* known)
{
    KnownConstant value = known[word->b];
    bool move = word->opcode == OP_MOV || word->opcode == OP_MOV_64;
    bool wide = word->opcode == OP_MOV_64;

    if (!move || !value.known || value.wide != wide) {
        return;
    }

    word->opcode = value.loadOpcode;
    word->b = value.operand >> 8;
    word->c = value.operand;
}

static void invalidateKnownRegister(KnownConstant* known, size_t reg)
{
    if (reg >= 256) {
        return;
    }

    known[reg] = (KnownConstant){0};

    if (reg > 0 && known[reg - 1].wide) {
        known[reg - 1] = (KnownConstant){0};
    }
}

static int64_t readConstant(ModuleObject* module, Opcode opcode, size_t position)
{
    if (opcode == OP_LDC_64) {
        return readI64(module->constants.data, position);
    }

    return module->constants.data[position].signedValue;
}

static void trackConstant(ModuleObject* module, OptimizationWord* word, KnownConstant* known)
{
    KnownConstant value = {0};
    OpcodeFlag flags = getOpcodeFlags(word->opcode);
    bool pairs = usesRegisterPairs(word->opcode);

    if (word->opcode == OP_LDI) {
        value.known = true;
        value.value = (int16_t)(word->b << 8 | word->c);
    }

    if (word->opcode == OP_LDC || word->opcode == OP_LDC_64) {
        size_t position = word->b << 8 | word->c;

        value.known = true;
        value.wide = pairs;
        value.value = readConstant(module, word->opcode, position);
    }

    if (value.known) {
        value.loadOpcode = word->opcode;
        value.operand = word->b << 8 | word->c;
    }

    if (word->opcode == OP_MOV || word->opcode == OP_MOV_64) {
        value = known[word->b];
        value.known = value.known && value.wide == pairs;
    }

    if (!(flags & OP_FLAG_WRITES_A)) {
        return;
    }

    invalidateKnownRegister(known, word->a);

    if (pairs) {
        invalidateKnownRegister(known, word->a + 1);
    }

    if (pairs && word->a == UINT8_MAX) {
        return;
    }

    known[word->a] = value;
}

static bool supportsWideConstants(Opcode opcode)
{
    if (opcode == OP_LDC_64 || opcode == OP_MOV_64) {
        return true;
    }

    Opcode immediate = immediateOpcode(opcode);

    return immediate != OP_HLT;
}

static void optimizeConstantWord(ModuleObject* module, OptimizationWord* word, KnownConstant* known)
{
    bool invalidate = invalidatesConstants(word->opcode);
    bool pairs = usesRegisterPairs(word->opcode);
    bool supported = supportsWideConstants(word->opcode);
    bool barrier = word->wide || invalidate || (pairs && !supported);

    if (word->target || barrier) {
        memset(known, 0, 256 * sizeof(KnownConstant));
    }

    if (barrier) {
        return;
    }

    selectConstantMove(word, known);
    selectImmediateArithmetic(word, known);
    trackConstant(module, word, known);
}

static void optimizeConstants(ModuleObject* module, OptimizationWord* words, size_t count)
{
    KnownConstant known[256] = {0};

    for (size_t i = 0; i < count; i++) {
        optimizeConstantWord(module, &words[i], known);
    }
}

static bool isWideImmediateArithmetic(Opcode opcode)
{
    switch (opcode) {
        case OP_ADDI_I64:
        case OP_SUBI_I64:
        case OP_MULI_I64:
        case OP_BANDI_I64:
        case OP_BORI_I64:
        case OP_BXORI_I64:
        case OP_LSLI_I64:
        case OP_LSRI_I64:
        case OP_ASRI_I64:
            return true;
        default: return false;
    }
}

static bool updateWideLiveness(OptimizationWord* word, bool* live)
{
    bool immediate = isWideImmediateArithmetic(word->opcode);
    bool pairs = usesRegisterPairs(word->opcode);
    Opcode selected = immediateOpcode(word->opcode);
    bool binary = pairs && selected != OP_HLT;
    bool constant = word->opcode == OP_LDC_64;
    bool load = constant || word->opcode == OP_LDG_64;
    bool move = word->opcode == OP_MOV_64;

    if (word->wide || word->a == UINT8_MAX || (!immediate && !binary && !load && !move)) {
        return false;
    }

    if ((!load && word->b == UINT8_MAX) || (binary && word->c == UINT8_MAX)) {
        return false;
    }

    word->removed = constant && !word->target && !live[word->a] && !live[word->a + 1];
    live[word->a] = false;
    live[word->a + 1] = false;

    if (!load) {
        live[word->b] = true;
        live[word->b + 1] = true;
    }

    if (binary) {
        live[word->c] = true;
        live[word->c + 1] = true;
    }

    return true;
}

static bool updateBuiltinCallLiveness(ModuleObject* module, OptimizationWord* word, bool* live)
{
    if (word->opcode != OP_CALL || word->wide) {
        return false;
    }

    size_t position = word->b << 8 | word->c;
    FunctionObject* function = module->functions.data[position];
    int argumentEnd = word->a + function->paramCount;

    if (function->type != FUNCTION_BUILTIN || argumentEnd > 256) {
        return false;
    }

    for (int i = 0; i < function->returnCount; i++) {
        int result = (int)word->a - 2 + i;

        live[result] = false;
    }

    for (int reg = word->a; reg < argumentEnd; reg++) {
        live[reg] = true;
    }

    return true;
}

static bool updateGlobalStoreLiveness(OptimizationWord* word, bool* live)
{
    if (word->wide || (word->opcode != OP_STG && word->opcode != OP_STG_64)) {
        return false;
    }

    int slots = word->opcode == OP_STG_64 ? 2 : 1;

    if (word->a + slots > 256) {
        return false;
    }

    for (int i = 0; i < slots; i++) {
        live[word->a + i] = true;
    }

    return true;
}

static void updateLiveness(ModuleObject* module, OptimizationWord* word, bool* live,
    size_t persistentRegisters)
{
    bool wide = updateWideLiveness(word, live);

    if (wide) {
        return;
    }

    bool isBuiltinCall = updateBuiltinCallLiveness(module, word, live);

    if (isBuiltinCall) {
        return;
    }

    bool store = updateGlobalStoreLiveness(word, live);

    if (store) {
        return;
    }

    OpcodeRegisterFlag registers = getOpcodeRegisterFlag(word->opcode);
    OpcodeFlag flags = getOpcodeFlags(word->opcode);
    bool pairs = usesRegisterPairs(word->opcode);
    bool barrier = invalidatesConstants(word->opcode);
    bool terminal = word->opcode == OP_HLT || word->opcode == OP_RET || word->opcode == OP_RETV;

    if (terminal) {
        for (size_t reg = 0; reg < 256; reg++) {
            live[reg] = reg < persistentRegisters;
        }
    }

    if (word->wide || pairs || (barrier && !terminal)) {
        memset(live, 1, 256 * sizeof(bool));

        return;
    }

    bool load = word->opcode == OP_LDI || word->opcode == OP_LDC;

    word->removed = word->opcode == OP_NOP || (load && !word->target && !live[word->a]);

    if (flags & OP_FLAG_WRITES_A) {
        live[word->a] = false;
    }

    if ((registers & OP_REGISTER_A) && !(flags & OP_FLAG_WRITES_A)) {
        live[word->a] = true;
    }

    if (registers & OP_REGISTER_B) {
        live[word->b] = true;
    }

    if (registers & OP_REGISTER_C) {
        live[word->c] = true;
    }
}

static void removeDeadLoads(ModuleObject* module, OptimizationWord* words, size_t count,
    size_t persistentRegisters)
{
    bool live[256];

    memset(live, 1, sizeof(live));

    for (size_t end = count; end > 0; end--) {
        OptimizationWord* word = &words[end - 1];

        updateLiveness(module, word, live, persistentRegisters);

        if (word->target) {
            memset(live, 1, sizeof(live));
        }
    }
}

static bool isImmediateArithmetic(Opcode opcode)
{
    switch (opcode) {
        case OP_SEXT_I32:
        case OP_ZEXT_I32:
        case OP_ADDI_I32:
        case OP_ADDI_I16:
        case OP_ADDI_I8:
        case OP_SUBI_I32:
        case OP_SUBI_I16:
        case OP_SUBI_I8:
        case OP_MULI_I32:
        case OP_MULI_I16:
        case OP_MULI_I8:
        case OP_BANDI_I32:
        case OP_BANDI_I16:
        case OP_BANDI_I8:
        case OP_BORI_I32:
        case OP_BORI_I16:
        case OP_BORI_I8:
        case OP_BXORI_I32:
        case OP_BXORI_I16:
        case OP_BXORI_I8:
        case OP_LSLI_I32:
        case OP_LSLI_I16:
        case OP_LSLI_I8:
        case OP_LSRI_I32:
        case OP_LSRI_I16:
        case OP_LSRI_I8:
        case OP_ASRI_I32:
        case OP_ASRI_I16:
        case OP_ASRI_I8:
            return true;
        default: return false;
    }
}

static void removeImmediateOperandMove(OptimizationWord* move, OptimizationWord* arithmetic)
{
    bool wide = isWideImmediateArithmetic(arithmetic->opcode);
    bool immediate = isImmediateArithmetic(arithmetic->opcode) || wide;
    Opcode moveOpcode = OP_MOV;

    if (wide) {
        moveOpcode = OP_MOV_64;
    }

    if (!immediate || move->opcode != moveOpcode || move->target || move->wide
        || arithmetic->target || arithmetic->wide || move->a != arithmetic->a
        || move->a != arithmetic->b) {
        return;
    }

    arithmetic->b = move->b;
    move->removed = true;
}

static void removeImmediateOperandMoves(OptimizationWord* words, size_t count)
{
    size_t previous = count;

    for (size_t i = 0; i < count; i++) {
        OptimizationWord* word = &words[i];

        if (word->removed) {
            continue;
        }

        if (previous != count) {
            removeImmediateOperandMove(&words[previous], word);
        }

        previous = i;
    }
}

static bool smallLoad(OptimizationWord* word)
{
    int operand = word->b << 8 | word->c;

    if (word->opcode == OP_LDG || word->opcode == OP_LDC) {
        return operand <= (int)UINT8_MAX;
    }

    if (word->opcode != OP_LDI) {
        return false;
    }

    int16_t immediate = operand;

    return immediate >= INT8_MIN && immediate <= INT8_MAX;
}

static Opcode loadPairOpcode(Opcode first, Opcode second)
{
    if (first == OP_LDI && second == OP_LDI) {
        return OP_LDI_LDI;
    }

    if (first == OP_LDC && second == OP_LDC) {
        return OP_LDC_LDC;
    }

    if (first == OP_LDG && second == OP_LDG) {
        return OP_LDG_LDG;
    }

    if (first == OP_LDI && second == OP_LDG) {
        return OP_LDI_LDG;
    }

    if (first == OP_LDG && second == OP_LDI) {
        return OP_LDG_LDI;
    }

    return OP_NOP;
}

static bool fuseLoads(OptimizationWord* first, OptimizationWord* second)
{
    bool firstSmall = smallLoad(first);
    bool secondSmall = smallLoad(second);
    Opcode opcode = loadPairOpcode(first->opcode, second->opcode);

    if (first->a + 1 != second->a || !firstSmall || !secondSmall || opcode == OP_NOP) {
        return false;
    }

    first->opcode = opcode;
    first->b = first->c;
    first->c = second->c;

    return true;
}

static bool fuseMoveLoad(OptimizationWord* first, OptimizationWord* second)
{
    bool small = smallLoad(second);

    if (first->opcode != OP_MOV || first->a + 1 != second->a || !small) {
        return false;
    }

    switch (second->opcode) {
        case OP_LDI:
            first->opcode = OP_MOV_LDI;
            break;
        case OP_LDC:
            first->opcode = OP_MOV_LDC;
            break;
        case OP_LDG:
            first->opcode = OP_MOV_LDG;
            break;
        default:
            return false;
    }

    first->c = second->c;

    return true;
}

static Opcode loadCallOpcode(Opcode opcode)
{
    switch (opcode) {
        case OP_LDI:    return OP_LDI_CALL;
        case OP_LDC:    return OP_LDC_CALL;
        case OP_LDG:    return OP_LDG_CALL;
        case OP_LDC_64: return OP_LDC_64_CALL;
        case OP_LDG_64: return OP_LDG_64_CALL;
        case OP_LDR:    return OP_LDR_CALL;
        default:        return OP_HLT;
    }
}

static bool fuseLoadCall(OptimizationWord* first, OptimizationWord* second)
{
    Opcode fused = loadCallOpcode(first->opcode);
    uint16_t function = second->b << 8 | second->c;
    int operand = first->b << 8 | first->c;

    if (first->opcode == OP_LDR) {
        operand = first->b;
    }

    if (first->opcode == OP_LDI) {
        operand = (int16_t)operand;
    }

    int minimum = first->opcode == OP_LDI ? LOAD_CALL_SIGNED_OPERAND_MIN : 0;
    int maximum = first->opcode == OP_LDI ? LOAD_CALL_SIGNED_OPERAND_MAX : LOAD_CALL_OPERAND_MASK;

    if (second->opcode != OP_CALL || fused == OP_HLT || first->a != second->a
        || function > LOAD_CALL_FUNCTION_MASK || operand < minimum || operand > maximum) {
        return false;
    }

    uint16_t packed = (operand & LOAD_CALL_OPERAND_MASK) << LOAD_CALL_FUNCTION_BITS | function;

    first->opcode = fused;
    first->b = packed >> 8;
    first->c = packed;

    return true;
}

static bool fuseStoreCall(OptimizationWord* first, OptimizationWord* second)
{
    if (first->opcode != OP_STG || second->opcode != OP_CALL || first->a != second->a
        || first->b != 0 || second->b != 0) {
        return false;
    }

    first->opcode = OP_STG_CALL;
    first->b = first->c;
    first->c = second->c;

    return true;
}

static bool fusePair(ModuleObject* module, OptimizationWord* first, OptimizationWord* second)
{
    bool loads = fuseLoads(first, second);

    if (loads) {
        return true;
    }

    bool call = fuseLoadCall(first, second);

    if (call) {
        return true;
    }

    bool storeCall = fuseStoreCall(first, second);

    if (storeCall) {
        return true;
    }

    bool moveLoad = fuseMoveLoad(first, second);

    if (moveLoad) {
        return true;
    }

    if (first->opcode == OP_MOV && second->opcode == OP_MOV && first->a + 1 == second->a) {
        first->opcode = OP_MOV_MOV;
        first->c = second->b;

        return true;
    }

    bool small = smallLoad(first);

    if (first->opcode == OP_LDI && second->opcode == OP_STG && first->a == second->a
        && second->b == 0 && small) {
        first->opcode = OP_STGI;
        first->b = first->c;
        first->c = second->c;

        return true;
    }

    if (first->opcode != OP_CALL || second->opcode != OP_CALL || first->a + 1 != second->a
        || first->b != 0 || second->b != 0) {
        return false;
    }

    FunctionObject* function = module->functions.data[first->c];

    if (function->type != FUNCTION_BUILTIN) {
        return false;
    }

    first->opcode = OP_CALL_CALL;
    first->b = first->c;
    first->c = second->c;

    return true;
}

static void fuseInstructions(ModuleObject* module, OptimizationWord* words, size_t count)
{
    size_t previous = count;

    for (size_t i = 0; i < count; i++) {
        OptimizationWord* word = &words[i];

        if (word->removed) {
            continue;
        }

        if (word->target || word->wide || word->opcode == OP_WIDE) {
            previous = count;
        }

        if (previous != count) {
            word->removed = fusePair(module, &words[previous], word);
        }

        previous = word->removed ? count : i;

        if (word->wide || word->opcode == OP_WIDE) {
            previous = count;
        }
    }
}

static bool isDuplicateInstruction(const OptimizationWord* previous, const OptimizationWord* word)
{
    OpcodeFlag flags = getOpcodeFlags(word->opcode);
    OpcodeRegisterFlag registers = getOpcodeRegisterFlag(word->opcode);
    bool independent = (flags & OP_FLAG_WRITES_A) && !(registers & (OP_REGISTER_B | OP_REGISTER_C));

    return independent && previous && previous->opcode == word->opcode
        && previous->a == word->a && previous->b == word->b && previous->c == word->c
        && !previous->wide && !word->wide && !word->target;
}

static bool isRedundantGlobalLoad(const OptimizationWord* previous, const OptimizationWord* word)
{
    bool matchingOpcodes = previous && ((previous->opcode == OP_STG && word->opcode == OP_LDG)
        || (previous->opcode == OP_STG_64 && word->opcode == OP_LDG_64));

    return matchingOpcodes && previous->a == word->a && previous->b == word->b && previous->c == word->c
        && !previous->wide && !word->wide && !word->target;
}

static OptimizationWord* removeRedundantInstruction(OptimizationWord* previous, OptimizationWord* word)
{
    if (word->removed) {
        return previous;
    }

    if (isDuplicateInstruction(previous, word) || isRedundantGlobalLoad(previous, word)) {
        word->removed = true;

        return previous;
    }

    return word;
}

static void removeRedundantInstructions(OptimizationWord* words, size_t count)
{
    OptimizationWord* previous = NULL;

    for (size_t i = 0; i < count; i++) {
        previous = removeRedundantInstruction(previous, &words[i]);
    }
}

static void relocateBranch(OptimizationWord* word, size_t position, size_t* positions)
{
    int offset = (int)positions[word->destination] - (int)position - 1;

    if (word->opcode == OP_JMP) {
        word->a = offset >> 16;
    }

    if (word->opcode == OP_JMP || word->opcode == OP_BZ || word->opcode == OP_BNZ) {
        word->b = offset >> 8;
    }

    word->c = offset;
}

static void writeWords(CodeObject* code, OptimizationWord* words, size_t count, size_t* positions)
{
    size_t output = 0;

    for (size_t i = 0; i <= count; i++) {
        positions[i] = output;
        output += i < count && !words[i].removed;
    }

    clearCodeObject(code);

    for (size_t i = 0; i < count; i++) {
        OptimizationWord* word = &words[i];
        bool branch = isBranch(word->opcode);

        if (word->removed) {
            continue;
        }

        if (branch) {
            relocateBranch(word, positions[i], positions);
        }

        pushByte(code, word->opcode);
        pushByte(code, word->a);
        pushByte(code, word->b);
        pushByte(code, word->c);
    }
}

void optimizeBytecode(ModuleObject* module, CodeObject* code, size_t persistentRegisters)
{
    size_t count = code->count / INSTRUCTION_SIZE;

    if (!count) {
        return;
    }

    OptimizationWord* words = allocateZeroedMemory(count + 1, sizeof(OptimizationWord));
    size_t* positions = allocateMemory((count + 1) * sizeof(size_t));

    decodeWords(code, words, count);
    optimizeConstants(module, words, count);
    removeDeadLoads(module, words, count, persistentRegisters);
    removeImmediateOperandMoves(words, count);
    fuseInstructions(module, words, count);
    removeRedundantInstructions(words, count);
    writeWords(code, words, count, positions);
    freeMemory(words);
    freeMemory(positions);
}
