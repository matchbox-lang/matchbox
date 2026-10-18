#include "bytecode_optimizer.h"
#include "function_object.h"
#include "opcode.h"
#include "util.h"
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
    int64_t value;
    Opcode loadOpcode;
    uint16_t operand;
} KnownConstant;

static bool usesRegisterPairs(Opcode opcode)
{
    switch (opcode) {
        case OP_MOV_64:
        case OP_LDC_64:
        case OP_LDG_64:
        case OP_STG_64:
        case OP_LDR_64:
        case OP_STR_64:
        case OP_F32_TO_F64:
        case OP_INT_TO_F64:
        case OP_UINT_TO_F64:
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

    return end || opcode == OP_CALL || opcode == OP_STG || opcode == OP_STG_64
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

    if (immediate == OP_HLT || !right.known || right.value < minimum || right.value > maximum) {
        return;
    }

    word->opcode = immediate;
    word->c = right.value;
}

static void selectConstantMove(OptimizationWord* word, KnownConstant* known)
{
    KnownConstant value = known[word->b];

    if (word->opcode != OP_MOV || !value.known) {
        return;
    }

    word->opcode = value.loadOpcode;
    word->b = value.operand >> 8;
    word->c = value.operand;
}

static void trackConstant(ModuleObject* module, OptimizationWord* word, KnownConstant* known)
{
    KnownConstant value = {0};
    OpcodeFlag flags = getOpcodeFlags(word->opcode);

    if (word->opcode == OP_LDI) {
        value.known = true;
        value.value = (int16_t)(word->b << 8 | word->c);
    }

    if (word->opcode == OP_LDC) {
        size_t position = word->b << 8 | word->c;

        value.known = true;
        value.value = module->constants.data[position].signedValue;
    }

    if (value.known) {
        value.loadOpcode = word->opcode;
        value.operand = word->b << 8 | word->c;
    }

    if (word->opcode == OP_MOV) {
        value = known[word->b];
    }

    if (flags & OP_FLAG_WRITES_A) {
        known[word->a] = value;
    }
}

static void optimizeConstants(ModuleObject* module, OptimizationWord* words, size_t count)
{
    KnownConstant known[256] = {0};

    for (size_t i = 0; i < count; i++) {
        OptimizationWord* word = &words[i];

        if (word->target) {
            memset(known, 0, sizeof(known));
        }

        bool invalidate = invalidatesConstants(word->opcode);
        bool pairs = usesRegisterPairs(word->opcode);

        if (word->target || word->wide || invalidate || pairs) {
            memset(known, 0, sizeof(known));
        }

        if (word->wide || invalidate || pairs) {
            continue;
        }

        selectConstantMove(word, known);
        selectImmediateArithmetic(word, known);
        trackConstant(module, word, known);
    }
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

    // Built-in functions only read their argument slots and write their result slots
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
    bool immediate = isImmediateArithmetic(arithmetic->opcode);

    if (!immediate || move->opcode != OP_MOV || move->target || move->wide
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
        return OP_LDI2;
    }

    if (first == OP_LDC && second == OP_LDC) {
        return OP_LDC2;
    }

    if (first == OP_LDG && second == OP_LDG) {
        return OP_LDG2;
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
        case OP_LDI: first->opcode = OP_MOV_LDI; break;
        case OP_LDC: first->opcode = OP_MOV_LDC; break;
        case OP_LDG: first->opcode = OP_MOV_LDG; break;
        default: return false;
    }

    first->c = second->c;

    return true;
}

static Opcode loadCallOpcode(Opcode opcode)
{
    switch (opcode) {
        case OP_LDI: return OP_LDI_CALL;
        case OP_LDC: return OP_LDC_CALL;
        case OP_LDG: return OP_LDG_CALL;
        case OP_LDR: return OP_LDR_CALL;
        default: return OP_HLT;
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

    bool moveLoad = fuseMoveLoad(first, second);

    if (moveLoad) {
        return true;
    }

    if (first->opcode == OP_MOV && second->opcode == OP_MOV && first->a + 1 == second->a) {
        first->opcode = OP_MOV2;
        first->c = second->b;

        return true;
    }

    bool small = smallLoad(first);

    if (first->opcode == OP_LDI && second->opcode == OP_STG && first->a == second->a
        && second->b == 0 && small) {
        first->opcode = OP_LDI_STG;
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

    first->opcode = OP_CALL2;
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

    OptimizationWord* words = calloc(count + 1, sizeof(OptimizationWord));
    size_t* positions = malloc((count + 1) * sizeof(size_t));

    if (!words || !positions) {
        outOfMemoryError();
    }

    decodeWords(code, words, count);
    optimizeConstants(module, words, count);
    removeDeadLoads(module, words, count, persistentRegisters);
    removeImmediateOperandMoves(words, count);
    fuseInstructions(module, words, count);
    writeWords(code, words, count, positions);
    free(words);
    free(positions);
}
