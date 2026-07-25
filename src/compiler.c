#include "compiler.h"
#include "ast.h"
#include "builtin.h"
#include "code_object.h"
#include "function_object.h"
#include "module_object.h"
#include "opcode.h"
#include "parser.h"
#include "optimizer.h"
#include "scope.h"
#include "token.h"
#include "util.h"
#include "value.h"
#include "vector.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef void (*CompileStatements)(Compiler* compiler, Vector* nodes);

typedef struct Operand
{
    int reg;
    bool temporary;
    size_t slots;
} Operand;

typedef struct CallArea
{
    int resultRegister;
    int frameRegister;
} CallArea;

typedef struct PreviousInstruction
{
    CodeObject* code;
    size_t start;
    Opcode opcode;
    uint8_t a;
    uint8_t b;
    uint8_t c;
} PreviousInstruction;

typedef enum IntegerOperation
{
    INTEGER_ADD,
    INTEGER_SUB,
    INTEGER_MUL,
    INTEGER_DIV,
    INTEGER_REM,
    INTEGER_BAND,
    INTEGER_BOR,
    INTEGER_BXOR,
    INTEGER_BNOT,
    INTEGER_LSL,
    INTEGER_LSR,
    INTEGER_NEG
} IntegerOperation;

static Operand compileExpression(Compiler* compiler, ASTNode* ast, bool discard);
static Operand compileReferenceExpression(Compiler* compiler, ASTNode* ast);
static void compileBlocklevelStatements(Compiler* compiler, Vector* nodes);
static void compileTopLevelStatements(Compiler* compiler, Vector* nodes);
static size_t getNodeSlotCount(ASTNode* ast);
static Operand compilePower(Compiler* compiler, ASTNode* leftExpression, ASTNode* rightExpression);
static void compilePowerAssignment(Compiler* compiler, ASTNode* ast);

static void functionPositionOverflowError()
{
    fprintf(stderr, "Error: Cannot create more than 65536 functions\n");
    exit(1);
}

static void functionNotFoundError(ASTNode* ast)
{
    fprintf(stderr, "Error: Could not find function %s\n", ast->functionDefinition.id->chars);
    exit(1);
}

static CodeObject* currentCodeObject(Compiler* compiler)
{
    return &compiler->function->code;
}

static bool isCompilingTopLevel(Compiler* compiler)
{
    return compiler->function == getVectorAt(&compiler->module->functions, 0);
}

static int allocateRegister(Compiler* compiler)
{
    int reg = compiler->registerCount++;
    int extent = compiler->registerCount + 2;

    if (extent > compiler->function->maxStackCount) {
        compiler->function->maxStackCount = extent;
    }

    return reg;
}

static int releaseRegister(Compiler* compiler)
{
    return --compiler->registerCount;
}

static int allocateRegisters(Compiler* compiler, size_t count)
{
    int first = compiler->registerCount;

    for (size_t i = 0; i < count; i++) {
        allocateRegister(compiler);
    }

    return first;
}

static void write8(Compiler* compiler, uint8_t n)
{
    pushByte(currentCodeObject(compiler), n);
}

static void emitInstruction(Compiler* compiler, Opcode opcode, uint8_t a, uint8_t b, uint8_t c)
{
    write8(compiler, opcode);
    write8(compiler, a);
    write8(compiler, b);
    write8(compiler, c);
}

static void emitHlt(Compiler* compiler)
{
    emitInstruction(compiler, OP_HLT, 0, 0, 0);
}

static bool getPreviousInstruction(Compiler* compiler, PreviousInstruction* instruction)
{
    CodeObject* code = currentCodeObject(compiler);
    size_t count = countCodeObject(code);

    if (count < INSTRUCTION_SIZE) {
        return false;
    }

    size_t start = count - INSTRUCTION_SIZE;

    instruction->code = code;
    instruction->start = start;
    instruction->opcode = (Opcode)code->data[start];
    instruction->a = code->data[start + 1];
    instruction->b = code->data[start + 2];
    instruction->c = code->data[start + 3];

    return true;
}

static bool fuseMovMov(Compiler* compiler, int dst, int src)
{
    PreviousInstruction previous;

    if (!getPreviousInstruction(compiler, &previous)
        || previous.a + 1 != dst
        || previous.opcode != OP_MOV) {
        return false;
    }

    setByteAt(previous.code, previous.start, OP_MOV2);
    setByteAt(previous.code, previous.start + 3, src);

    return true;
}

static void emitMov(Compiler* compiler, int dst, int src)
{
    if (dst == src) {
        return;
    }

    if (fuseMovMov(compiler, dst, src)) {
        return;
    }

    emitInstruction(compiler, OP_MOV, dst, src, 0);
}

static void emitOperandMov(Compiler* compiler, int dst, Operand operand)
{
    if (operand.slots != 2) {
        emitMov(compiler, dst, operand.reg);
        return;
    }

    if (dst == operand.reg) {
        return;
    }

    emitInstruction(compiler, OP_MOV_I64, dst, operand.reg, 0);
}

static Operand makeOperand(int reg, bool temporary)
{
    Operand operand = {reg, temporary, 1};

    return operand;
}

static Operand makeWideOperand(int reg, bool temporary)
{
    Operand operand = {reg, temporary, 2};

    return operand;
}

static Operand noOperand(void)
{
    return makeOperand(-1, false);
}

static void releaseOperand(Compiler* compiler, Operand operand)
{
    for (size_t i = 0; operand.temporary && i < operand.slots; i++) {
        releaseRegister(compiler);
    }
}

static Operand materializeOperand(Compiler* compiler, Operand operand)
{
    if (operand.temporary) {
        return operand;
    }

    int reg = allocateRegisters(compiler, operand.slots);
    emitOperandMov(compiler, reg, operand);
    
    return operand.slots == 2 ? makeWideOperand(reg, true) : makeOperand(reg, true);
}

static int emitLdc(Compiler* compiler, uint16_t imm)
{
    int reg = allocateRegister(compiler);

    emitInstruction(compiler, OP_LDC, reg, imm >> 8, imm);

    return reg;
}

static int emitLdcI64(Compiler* compiler, uint16_t position)
{
    int reg = allocateRegisters(compiler, 2);

    emitInstruction(compiler, OP_LDC_I64, reg, position >> 8, position);

    return reg;
}

static Opcode getLoadPairOpcode(Opcode first, Opcode second)
{
    if (first == OP_LDI && second == OP_LDI) {
        return OP_LDI2;
    }

    if (first == OP_LDI && second == OP_LDG) {
        return OP_LDI_LDG;
    }

    if (first == OP_LDG && second == OP_LDI) {
        return OP_LDG_LDI;
    }

    if (first == OP_LDG && second == OP_LDG) {
        return OP_LDG2;
    }

    return OP_HLT;
}

static bool isLoadPairOperandValid(Opcode opcode, uint16_t operand)
{
    if (opcode != OP_LDI) {
        return operand <= UINT8_MAX;
    }

    return !isLargerThan8BitSigned((int16_t)operand);
}

static bool fuseLoadPair(Compiler* compiler, Opcode opcode, int reg, uint16_t operand)
{
    PreviousInstruction previous;

    if (!isLoadPairOperandValid(opcode, operand)
        || !getPreviousInstruction(compiler, &previous)
        || previous.a + 1 != reg) {
        return false;
    }

    Opcode fusedOpcode = getLoadPairOpcode(previous.opcode, opcode);

    if (fusedOpcode == OP_HLT) {
        return false;
    }

    uint16_t previousOperand = (uint16_t)(previous.b << 8 | previous.c);

    if (!isLoadPairOperandValid(previous.opcode, previousOperand)) {
        return false;
    }

    setByteAt(previous.code, previous.start, fusedOpcode);
    setByteAt(previous.code, previous.start + 2, previousOperand);
    setByteAt(previous.code, previous.start + 3, operand);

    return true;
}

static int emitLdi(Compiler* compiler, int16_t imm)
{
    int reg = allocateRegister(compiler);

    if (fuseLoadPair(compiler, OP_LDI, reg, (uint16_t)imm)) {
        return reg;
    }

    emitInstruction(compiler, OP_LDI, reg, imm >> 8, imm);

    return reg;
}

static int emitLdg(Compiler* compiler, uint16_t imm)
{
    int reg = allocateRegister(compiler);

    if (fuseLoadPair(compiler, OP_LDG, reg, imm)) {
        return reg;
    }

    emitInstruction(compiler, OP_LDG, reg, imm >> 8, imm);

    return reg;
}

static Opcode getLoadCallOpcode(Opcode opcode)
{
    switch (opcode) {
        case OP_LDI:
            return OP_LDI_CALL;
        case OP_LDC:
            return OP_LDC_CALL;
        case OP_LDG:
            return OP_LDG_CALL;
        case OP_LDR:
            return OP_LDR_CALL;
        default:
            return OP_HLT;
    }
}

static bool isLoadCallOperandValid(Opcode opcode, uint16_t operands)
{
    if (opcode != OP_LDI) {
        return operands <= LOAD_CALL_OPERAND_MASK;
    }

    int16_t imm = (int16_t)operands;

    return imm >= LOAD_CALL_SIGNED_OPERAND_MIN
        && imm <= LOAD_CALL_SIGNED_OPERAND_MAX;
}

static bool fuseLoadCall(Compiler* compiler, uint8_t frameRegister, uint16_t functionPosition)
{
    PreviousInstruction previous;

    if (functionPosition > LOAD_CALL_FUNCTION_MASK
        || !getPreviousInstruction(compiler, &previous)) {
        return false;
    }

    Opcode fusedOpcode = getLoadCallOpcode(previous.opcode);

    if (fusedOpcode == OP_HLT) {
        return false;
    }

    if (previous.a != frameRegister) {
        return false;
    }

    uint16_t operands = previous.opcode == OP_LDR
        ? previous.b
        : previous.b << 8 | previous.c;

    if (!isLoadCallOperandValid(previous.opcode, operands)) {
        return false;
    }

    operands = ((operands & LOAD_CALL_OPERAND_MASK) << LOAD_CALL_FUNCTION_BITS)
        | functionPosition;

    setByteAt(previous.code, previous.start, fusedOpcode);
    setByteAt(previous.code, previous.start + 2, operands >> 8);
    setByteAt(previous.code, previous.start + 3, operands);

    return true;
}

static bool isBuiltinFunctionAtPosition(Compiler* compiler, uint16_t position)
{
    FunctionObject* function = getVectorAt(&compiler->module->functions, position);

    return function->type == FUNCTION_BUILTIN;
}

static bool fuseCallCall(Compiler* compiler, uint8_t frameRegister, uint16_t functionPosition)
{
    PreviousInstruction previous;

    if (functionPosition > UINT8_MAX
        || !getPreviousInstruction(compiler, &previous)) {
        return false;
    }

    if (previous.opcode != OP_CALL
        || previous.a + 1 != frameRegister
        || previous.b != 0) {
        return false;
    }

    uint8_t previousFunctionPosition = previous.c;

    if (!isBuiltinFunctionAtPosition(compiler, previousFunctionPosition)) {
        return false;
    }

    setByteAt(previous.code, previous.start, OP_CALL2);
    setByteAt(previous.code, previous.start + 2, previousFunctionPosition);
    setByteAt(previous.code, previous.start + 3, functionPosition);

    return true;
}

static void emitCallInstruction(Compiler* compiler, uint8_t frameRegister, uint16_t functionPosition)
{
    if (fuseCallCall(compiler, frameRegister, functionPosition)) {
        return;
    }

    if (fuseLoadCall(compiler, frameRegister, functionPosition)) {
        return;
    }

    emitInstruction(compiler, OP_CALL, frameRegister, functionPosition >> 8, functionPosition);
}

static void emitRet(Compiler* compiler)
{
    emitInstruction(compiler, OP_RET, 0, 0, 0);
}

static void emitRetv(Compiler* compiler, Operand operand)
{
    Opcode opcode = operand.slots == 2 ? OP_RET_I64 : OP_RETV;

    emitInstruction(compiler, opcode, operand.reg, 0, 0);
}

static int getCallAreaCount(FunctionObject* function)
{
    return function->paramCount;
}

static size_t makeConstant(Compiler* compiler, Value value)
{
    return pushValue(&compiler->module->constants, value) - 1;
}

static size_t makeI64Constant(Compiler* compiler, uint64_t value)
{
    size_t position = countValueArray(&compiler->module->constants);

#if UINTPTR_MAX == UINT32_MAX
    pushValue(&compiler->module->constants, U32_VALUE(value));
    pushValue(&compiler->module->constants, U32_VALUE(value >> 32));
#else
    pushValue(&compiler->module->constants, UNSIGNED_VALUE(value));
    pushValue(&compiler->module->constants, UNSIGNED_VALUE(0));
#endif

    return position;
}

static CallArea openCallArea(Compiler* compiler)
{
    CallArea call = { .resultRegister = compiler->registerCount };

    allocateRegister(compiler);
    allocateRegister(compiler);
    call.frameRegister = compiler->registerCount;

    return call;
}

static void allocateCallAreaRegister(Compiler* compiler)
{
    emitLdi(compiler, 0);
}

static Operand closeCallArea(Compiler* compiler, FunctionObject* function,
    uint16_t functionPosition, CallArea call, bool discard)
{
    int end = call.frameRegister + getCallAreaCount(function);

    while (compiler->registerCount < end) {
        allocateCallAreaRegister(compiler);
    }

    emitCallInstruction(compiler, (uint8_t)call.frameRegister, functionPosition);

    if (!discard && function->returnCount) {
        compiler->registerCount = call.resultRegister + function->returnCount;
        
        return function->returnCount == 2
            ? makeWideOperand(call.resultRegister, true)
            : makeOperand(call.resultRegister, true);
    }

    compiler->registerCount = call.resultRegister;

    return noOperand();
}

static int getLocalPosition(Compiler* compiler, ASTNode* ast)
{
    if (isParameter(ast)) {
        return ast->parameter.position;
    }
    
    return compiler->frameBaseCount + ast->variableDefinition.position;
}

static Operand loadGlobalWithStoreForwarding(Compiler* compiler, int position)
{
    PreviousInstruction previous;
    uint16_t storedPosition;

    if (!getPreviousInstruction(compiler, &previous)) {
        return makeOperand(emitLdg(compiler, position), true);
    }

    if (previous.opcode == OP_STG) {
        storedPosition = previous.b << 8 | previous.c;
    } else if (previous.opcode == OP_LDI_STG) {
        storedPosition = previous.c;
    } else {
        return makeOperand(emitLdg(compiler, position), true);
    }

    if (storedPosition != position) {
        return makeOperand(emitLdg(compiler, position), true);
    }

    return makeOperand(previous.a, false);
}

static Operand loadGlobalVariable(Compiler* compiler, ASTNode* ast)
{
    int position = ast->variableDefinition.position;
    bool wide = getNodeSlotCount(ast) == 2;

    if (isCompilingTopLevel(compiler)) {
        return wide ? makeWideOperand(position, false) : makeOperand(position, false);
    }

    if (wide) {
        int reg = allocateRegisters(compiler, 2);
        emitInstruction(compiler, OP_LDG_I64, reg, position >> 8, position);

        return makeWideOperand(reg, true);
    }

    return loadGlobalWithStoreForwarding(compiler, position);
}

static Operand loadLocalVariable(Compiler* compiler, ASTNode* ast)
{
    int position = getLocalPosition(compiler, ast);

    return getNodeSlotCount(ast) == 2
        ? makeWideOperand(position, false)
        : makeOperand(position, false);
}

static Operand loadVariable(Compiler* compiler, ASTNode* ast)
{
    if (isTopLevelScope(ast->variableDefinition.scope)) {
        return loadGlobalVariable(compiler, ast);
    }

    return loadLocalVariable(compiler, ast);
}

static Operand referenceVariable(Compiler* compiler, ASTNode* ast)
{
    if (getReferenceType(ast) != REFERENCE_NONE) {
        return loadVariable(compiler, ast);
    }

    int reg = allocateRegister(compiler);

    if (isTopLevelScope(getScope(ast))) {
        int position = ast->variableDefinition.position;
        emitInstruction(compiler, OP_REFG, reg, position >> 8, position);

        return makeOperand(reg, true);
    }

    emitInstruction(compiler, OP_REFL, reg, getLocalPosition(compiler, ast), 0);

    return makeOperand(reg, true);
}

static Operand dereferenceOperand(Compiler* compiler, Operand operand, size_t slots)
{
    Opcode opcode = slots == 2 ? OP_LDR_I64 : OP_LDR;

    if (!operand.temporary) {
        int reg = allocateRegisters(compiler, slots);

        emitInstruction(compiler, opcode, reg, operand.reg, 0);

        return slots == 2 ? makeWideOperand(reg, true) : makeOperand(reg, true);
    }

    if (slots == 2) {
        allocateRegister(compiler);
    }

    emitInstruction(compiler, opcode, operand.reg, operand.reg, 0);

    return slots == 2 ? makeWideOperand(operand.reg, true) : operand;
}

static bool fuseLdiStore(Compiler* compiler, Operand value, int position)
{
    PreviousInstruction previous;

    if (!value.temporary
        || position > (int)UINT8_MAX
        || !getPreviousInstruction(compiler, &previous)) {
        return false;
    }

    if (previous.opcode != OP_LDI
        || previous.a != value.reg) {
        return false;
    }

    int16_t imm = (int16_t)(previous.b << 8 | previous.c);

    if (isLargerThan8BitSigned(imm)) {
        return false;
    }

    setByteAt(previous.code, previous.start, OP_LDI_STG);
    setByteAt(previous.code, previous.start + 2, imm);
    setByteAt(previous.code, previous.start + 3, position);

    return true;
}

static void storeGlobalVariable(Compiler* compiler, ASTNode* ast, Operand value)
{
    int position = ast->variableDefinition.position;

    if (value.slots == 2) {
        emitInstruction(compiler, OP_STG_I64, value.reg, position >> 8, position);

        return;
    }

    if (fuseLdiStore(compiler, value, position)) {
        return;
    }

    emitInstruction(compiler, OP_STG, value.reg, position >> 8, position);
}

static bool retargetTemporary(Compiler* compiler, Operand value, int dst)
{
    PreviousInstruction previous;

    if (!value.temporary) {
        return false;
    }

    if (!getPreviousInstruction(compiler, &previous)
        || previous.a != value.reg) {
        return false;
    }

    if (!(getOpcodeFlags(previous.opcode) & OP_FLAG_WRITES_A)) {
        return false;
    }

    setByteAt(previous.code, previous.start + OPERAND_A_OFFSET, dst);

    return true;
}

static void storeLocalVariable(Compiler* compiler, ASTNode* ast, Operand value)
{
    int position = getLocalPosition(compiler, ast);

    if (retargetTemporary(compiler, value, position)) {
        return;
    }

    emitOperandMov(compiler, position, value);
}

static void storeVariable(Compiler* compiler, ASTNode* ast, Operand value)
{
    if (isTopLevelScope(ast->variableDefinition.scope)) {
        storeGlobalVariable(compiler, ast, value);
    } else {
        storeLocalVariable(compiler, ast, value);
    }

    releaseOperand(compiler, value);
}

static Operand compileWideNumber(Compiler* compiler, uint64_t value)
{
    size_t position = makeI64Constant(compiler, value);

    return makeWideOperand(emitLdcI64(compiler, position), true);
}

static Operand compileNumber(Compiler* compiler, ASTNode* ast)
{
    uint64_t value = ast->integerLiteral.value;

    if (getNodeSlotCount(ast) == 2) {
        return compileWideNumber(compiler, value);
    }

    if (value > INT16_MAX) {
        Value constant = isSignedIntegerTypeToken(getTypeId(ast))
            ? SIGNED_VALUE((int64_t)value)
            : UNSIGNED_VALUE(value);
        size_t position = makeConstant(compiler, constant);

        return makeOperand(emitLdc(compiler, position), true);
    }

    return makeOperand(emitLdi(compiler, (int16_t)value), true);
}

static size_t getNodeSlotCount(ASTNode* ast)
{
    if (getReferenceType(ast) != REFERENCE_NONE) {
        return 1;
    }

    return getTypeSlotCount(getTypeId(ast));
}

static bool isDirectVariable(Compiler* compiler, ASTNode* ast)
{
    if (!isVariable(ast)) {
        return false;
    }

    ASTNode* symbol = ast->variable.symbol;

    return !isTopLevelScope(symbol->variableDefinition.scope)
        || isCompilingTopLevel(compiler);
}

static Opcode getWidthOpcode(
    TokenType type, Opcode opcode64, Opcode opcode32, Opcode opcode16, Opcode opcode8)
{
    switch (getIntegerTypeSize(type)) {
        case 8:
            return opcode64;
        case 4:
            return opcode32;
        case 2:
            return opcode16;
        case 1:
            return opcode8;
        default:
            return OP_HLT;
    }
}

static Opcode getDivisionOpcode(TokenType type)
{
    if (isSignedIntegerTypeToken(type)) {
        return getWidthOpcode(type, OP_IDIV_I64, OP_IDIV_I32, OP_IDIV_I16, OP_IDIV_I8);
    }

    return getWidthOpcode(type, OP_IDIV_U64, OP_IDIV_U32, OP_IDIV_U16, OP_IDIV_U8);
}

static Opcode getRemainderOpcode(TokenType type)
{
    if (isSignedIntegerTypeToken(type)) {
        return getWidthOpcode(type, OP_REM_I64, OP_REM_I32, OP_REM_I16, OP_REM_I8);
    }

    return getWidthOpcode(type, OP_REM_U64, OP_REM_U32, OP_REM_U16, OP_REM_U8);
}

static Opcode getRightShiftOpcode(TokenType type)
{
    if (isSignedIntegerTypeToken(type)) {
        return getWidthOpcode(type, OP_ASR_I64, OP_ASR_I32, OP_ASR_I16, OP_ASR_I8);
    }

    return getWidthOpcode(type, OP_LSR_I64, OP_LSR_I32, OP_LSR_I16, OP_LSR_I8);
}

static Opcode getIntegerOpcode(TokenType type, IntegerOperation operation)
{
    switch (operation) {
        case INTEGER_ADD:
            return getWidthOpcode(type, OP_ADD_I64, OP_ADD_I32, OP_ADD_I16, OP_ADD_I8);
        case INTEGER_SUB:
            return getWidthOpcode(type, OP_SUB_I64, OP_SUB_I32, OP_SUB_I16, OP_SUB_I8);
        case INTEGER_MUL:
            return getWidthOpcode(type, OP_MUL_I64, OP_MUL_I32, OP_MUL_I16, OP_MUL_I8);
        case INTEGER_DIV:
            return getDivisionOpcode(type);
        case INTEGER_REM:
            return getRemainderOpcode(type);
        case INTEGER_BAND:
            return getWidthOpcode(type, OP_BAND_I64, OP_BAND_I32, OP_BAND_I16, OP_BAND_I8);
        case INTEGER_BOR:
            return getWidthOpcode(type, OP_BOR_I64, OP_BOR_I32, OP_BOR_I16, OP_BOR_I8);
        case INTEGER_BXOR:
            return getWidthOpcode(type, OP_BXOR_I64, OP_BXOR_I32, OP_BXOR_I16, OP_BXOR_I8);
        case INTEGER_BNOT:
            return getWidthOpcode(type, OP_BNOT_I64, OP_BNOT_I32, OP_BNOT_I16, OP_BNOT_I8);
        case INTEGER_LSL:
            return getWidthOpcode(type, OP_LSL_I64, OP_LSL_I32, OP_LSL_I16, OP_LSL_I8);
        case INTEGER_LSR:
            return getRightShiftOpcode(type);
        case INTEGER_NEG:
            return getIntegerTypeSize(type) == 8 ? OP_NEG_I64 : OP_NEG;
    }

    return OP_HLT;
}

static Opcode getIntegerImmediateOpcode(TokenType type, IntegerOperation operation)
{
    switch (operation) {
        case INTEGER_ADD:
            return getWidthOpcode(type, OP_ADDI_I64, OP_ADDI_I32, OP_ADDI_I16, OP_ADDI_I8);
        case INTEGER_SUB:
            return getWidthOpcode(type, OP_SUBI_I64, OP_SUBI_I32, OP_SUBI_I16, OP_SUBI_I8);
        case INTEGER_BAND:
            return getWidthOpcode(type, OP_BANDI_I64, OP_BANDI_I32, OP_BANDI_I16, OP_BANDI_I8);
        case INTEGER_BOR:
            return getWidthOpcode(type, OP_BORI_I64, OP_BORI_I32, OP_BORI_I16, OP_BORI_I8);
        case INTEGER_BXOR:
            return getWidthOpcode(type, OP_BXORI_I64, OP_BXORI_I32, OP_BXORI_I16, OP_BXORI_I8);
        case INTEGER_LSL:
            return getWidthOpcode(type, OP_LSLI_I64, OP_LSLI_I32, OP_LSLI_I16, OP_LSLI_I8);
        case INTEGER_LSR:
            if (isSignedIntegerTypeToken(type)) {
                return getWidthOpcode(type, OP_ASRI_I64, OP_ASRI_I32, OP_ASRI_I16, OP_ASRI_I8);
            }

            return getWidthOpcode(type, OP_LSRI_I64, OP_LSRI_I32, OP_LSRI_I16, OP_LSRI_I8);
        default:
            return OP_HLT;
    }
}

static bool isImmediateOperandValid(IntegerOperation operation, int16_t immediate)
{
    if (operation == INTEGER_ADD || operation == INTEGER_SUB) {
        return !isLargerThan8BitSigned(immediate);
    }

    return immediate >= 0 && immediate <= (int16_t)UINT8_MAX;
}

static bool emitBinaryImmediate(Compiler* compiler, IntegerOperation operation,
    TokenType type, Operand left, Operand right, Operand* result)
{
    PreviousInstruction previous;

    if (!right.temporary || !getPreviousInstruction(compiler, &previous)
        || previous.opcode != OP_LDI
        || previous.a != right.reg) {
        return false;
    }

    int16_t immediate = (int16_t)(previous.b << 8 | previous.c);
    Opcode opcode = getIntegerImmediateOpcode(type, operation);

    if (opcode == OP_HLT || !isImmediateOperandValid(operation, immediate)) {
        return false;
    }

    resizeCodeObject(previous.code, previous.start);
    releaseOperand(compiler, right);

    int dst = left.temporary ? left.reg : allocateRegisters(compiler, left.slots);
    emitInstruction(compiler, opcode, dst, left.reg, immediate);
    *result = left.slots == 2 ? makeWideOperand(dst, true) : makeOperand(dst, true);

    return true;
}

static Operand emitBinaryOperands(
    Compiler* compiler, IntegerOperation operation, TokenType type, Operand left, Operand right)
{
    Operand immediateResult;

    if (emitBinaryImmediate(compiler, operation, type, left, right, &immediateResult)) {
        return immediateResult;
    }

    int dst;

    if (left.temporary) {
        dst = left.reg;
        releaseOperand(compiler, right);
    } else if (right.temporary) {
        dst = right.reg;
    } else {
        dst = allocateRegisters(compiler, left.slots);
    }

    emitInstruction(compiler, getIntegerOpcode(type, operation), dst, left.reg, right.reg);
    
    return left.slots == 2 ? makeWideOperand(dst, true) : makeOperand(dst, true);
}

static Operand compileBinary(Compiler* compiler, ASTNode* ast)
{
    if (ast->binary.operator.type == TOKEN_POWER) {
        return compilePower(compiler, ast->binary.leftExpr, ast->binary.rightExpr);
    }

    TokenType type = getTypeId(ast->binary.leftExpr);
    Operand left = compileExpression(compiler, ast->binary.leftExpr, false);

    if (!left.temporary && !isDirectVariable(compiler, ast->binary.rightExpr)) {
        left = materializeOperand(compiler, left);
    }

    Operand right = compileExpression(compiler, ast->binary.rightExpr, false);

    switch (ast->binary.operator.type) {
        case TOKEN_PLUS:
            return emitBinaryOperands(compiler, INTEGER_ADD, type, left, right);
        case TOKEN_MINUS:
            return emitBinaryOperands(compiler, INTEGER_SUB, type, left, right);
        case TOKEN_STAR:
            return emitBinaryOperands(compiler, INTEGER_MUL, type, left, right);
        case TOKEN_SLASH:
        case TOKEN_FLOOR:
            return emitBinaryOperands(compiler, INTEGER_DIV, type, left, right);
        case TOKEN_PERCENT:
            return emitBinaryOperands(compiler, INTEGER_REM, type, left, right);
        case TOKEN_AMPERSAND:
            return emitBinaryOperands(compiler, INTEGER_BAND, type, left, right);
        case TOKEN_PIPE:
            return emitBinaryOperands(compiler, INTEGER_BOR, type, left, right);
        case TOKEN_CIRCUMFLEX:
            return emitBinaryOperands(compiler, INTEGER_BXOR, type, left, right);
        case TOKEN_LSHIFT:
            return emitBinaryOperands(compiler, INTEGER_LSL, type, left, right);
        case TOKEN_RSHIFT:
            return emitBinaryOperands(compiler, INTEGER_LSR, type, left, right);
        default:
            return noOperand();
    }
}

static Operand emitUnaryOperand(Compiler* compiler, Opcode opcode, Operand operand)
{
    if (operand.temporary) {
        emitInstruction(compiler, opcode, operand.reg, operand.reg, 0);

        return operand;
    }

    int dst = allocateRegisters(compiler, operand.slots);
    emitInstruction(compiler, opcode, dst, operand.reg, 0);

    return operand.slots == 2 ? makeWideOperand(dst, true) : makeOperand(dst, true);
}

static Operand compilePrefix(Compiler* compiler, ASTNode* ast)
{
    if (getReferenceType(ast) != REFERENCE_NONE) {
        return dereferenceOperand(
            compiler, compileReferenceExpression(compiler, ast), getTypeSlotCount(getTypeId(ast)));
    }

    Operand operand = compileExpression(compiler, ast->prefix.expr, false);

    switch (ast->prefix.operator.type) {
        case TOKEN_NOT:
            return emitUnaryOperand(compiler, OP_NOT, operand);
        case TOKEN_TILDE:
            return emitUnaryOperand(
                compiler, getIntegerOpcode(getTypeId(ast), INTEGER_BNOT), operand);
        case TOKEN_MINUS:
            return emitUnaryOperand(
                compiler, getIntegerOpcode(getTypeId(ast), INTEGER_NEG), operand);
        default:
            return noOperand();
    }
}

static Operand compileVariable(Compiler* compiler, ASTNode* ast)
{
    Operand value = loadVariable(compiler, ast->variable.symbol);

    if (getReferenceType(ast) == REFERENCE_NONE) {
        return value;
    }

    return dereferenceOperand(compiler, value, getTypeSlotCount(getTypeId(ast)));
}

static void storeAssignmentValue(Compiler* compiler, ASTNode* symbol, Operand value)
{
    if (getReferenceType(symbol) == REFERENCE_NONE) {
        storeVariable(compiler, symbol, value);

        return;
    }

    Operand reference = loadVariable(compiler, symbol);
    Opcode opcode = value.slots == 2 ? OP_STR_I64 : OP_STR;

    emitInstruction(compiler, opcode, value.reg, reference.reg, 0);
    releaseOperand(compiler, value);
    releaseOperand(compiler, reference);
}

static void compileCompoundAssignment(
    Compiler* compiler, ASTNode* ast, IntegerOperation operation)
{
    TokenType type = getTypeId(ast->assignment.symbol);

    Operand left = loadVariable(compiler, ast->assignment.symbol);

    if (getReferenceType(ast->assignment.symbol) != REFERENCE_NONE) {
        left = dereferenceOperand(
            compiler, left, getTypeSlotCount(getTypeId(ast->assignment.symbol)));
    }

    if (!left.temporary && !isDirectVariable(compiler, ast->assignment.expr)) {
        left = materializeOperand(compiler, left);
    }

    Operand right = compileExpression(compiler, ast->assignment.expr, false);
    Operand result = emitBinaryOperands(compiler, operation, type, left, right);

    storeAssignmentValue(compiler, ast->assignment.symbol, result);
}

static void compileSimpleAssignment(Compiler* compiler, ASTNode* ast)
{
    if (ast->assignment.initializesBinding) {
        Operand reference = compileReferenceExpression(compiler, ast->assignment.expr);
        storeVariable(compiler, ast->assignment.symbol, reference);

        return;
    }

    Operand value = compileExpression(compiler, ast->assignment.expr, false);

    storeAssignmentValue(compiler, ast->assignment.symbol, value);
}

static void compileAssignment(Compiler* compiler, ASTNode* ast)
{
    switch (ast->assignment.operator.type) {
        case TOKEN_PLUS_EQUAL:
            return compileCompoundAssignment(compiler, ast, INTEGER_ADD);
        case TOKEN_MINUS_EQUAL:
            return compileCompoundAssignment(compiler, ast, INTEGER_SUB);
        case TOKEN_STAR_EQUAL:
            return compileCompoundAssignment(compiler, ast, INTEGER_MUL);
        case TOKEN_FLOOR_EQUAL:
        case TOKEN_SLASH_EQUAL:
            return compileCompoundAssignment(compiler, ast, INTEGER_DIV);
        case TOKEN_PERCENT_EQUAL:
            return compileCompoundAssignment(compiler, ast, INTEGER_REM);
        case TOKEN_POWER_EQUAL:
            return compilePowerAssignment(compiler, ast);
        case TOKEN_AND_EQUAL:
            return compileCompoundAssignment(compiler, ast, INTEGER_BAND);
        case TOKEN_OR_EQUAL:
            return compileCompoundAssignment(compiler, ast, INTEGER_BOR);
        case TOKEN_CIRCUMFLEX_EQUAL:
            return compileCompoundAssignment(compiler, ast, INTEGER_BXOR);
        case TOKEN_LSHIFT_EQUAL:
            return compileCompoundAssignment(compiler, ast, INTEGER_LSL);
        case TOKEN_RSHIFT_EQUAL:
            return compileCompoundAssignment(compiler, ast, INTEGER_LSR);
        case TOKEN_EQUAL:
            return compileSimpleAssignment(compiler, ast);
        default:
            return;
    }
}

static Operand compileReferenceAwareExpression(Compiler* compiler, ASTNode* ast, bool reference)
{
    if (reference) {
        return compileReferenceExpression(compiler, ast);
    }

    return compileExpression(compiler, ast, false);
}

static bool requiresReservedArgument(Vector* args, Vector* params, size_t position)
{
    if (!params) {
        return false;
    }

    ASTNode* argument = getVectorAt(args, position);
    ASTNode* parameter = getVectorAt(params, position);

    return getReferenceType(parameter) != REFERENCE_NONE
        && getReferenceType(argument) == REFERENCE_NONE
        && argument->type != AST_VARIABLE;
}

static bool requiresReservedArguments(Vector* args, Vector* params)
{
    size_t count = countVector(args);

    for (size_t i = 0; i < count; i++) {
        if (requiresReservedArgument(args, params, i)) {
            return true;
        }
    }

    return false;
}

static void reserveArgumentRegisters(Compiler* compiler, size_t count, bool reserved)
{
    for (size_t i = 0; reserved && i < count; i++) {
        allocateRegister(compiler);
    }
}

static Operand compileArgumentExpression(Compiler* compiler, Vector* args, Vector* params,
    size_t position)
{
    ASTNode* parameter = NULL;

    if (params) {
        parameter = getVectorAt(params, position);
    }

    bool reference = parameter && getReferenceType(parameter) != REFERENCE_NONE;

    Operand expression = compileReferenceAwareExpression(
        compiler, args->data[position], reference);

    return expression;
}

static void storeArgument(Compiler* compiler, Operand argument,int destination, bool reserved)
{
    if (!reserved && argument.temporary) {
        return;
    }

    if (!reserved) {
        allocateRegisters(compiler, argument.slots);
    }

    emitOperandMov(compiler, destination, argument);
}

static void compileArgument(Compiler* compiler, Vector* args, Vector* params, size_t position,
    int firstRegister, size_t destinationOffset, bool reserved)
{
    int destination = compiler->registerCount;

    if (reserved) {
        destination = firstRegister + destinationOffset;
    }

    Operand argument = compileArgumentExpression(compiler, args, params, position);

    storeArgument(compiler, argument, destination, reserved);

}

static void compileArguments(Compiler* compiler, Vector* args, Vector* params)
{
    size_t count = countVector(args);
    size_t slotCount = 0;
    bool reserved = requiresReservedArguments(args, params);
    int firstRegister = compiler->registerCount;

    for (size_t i = 0; i < count; i++) {
        slotCount += getNodeSlotCount(getVectorAt(args, i));
    }

    reserveArgumentRegisters(compiler, slotCount, reserved);

    for (size_t i = 0, offset = 0; i < count; i++) {
        ASTNode* argument = getVectorAt(args, i);
        compileArgument(compiler, args, params, i, firstRegister, offset, reserved);
        offset += getNodeSlotCount(argument);
    }
}

static uint16_t makeFunctionPosition(size_t position)
{
    if (position > UINT16_MAX) {
        functionPositionOverflowError();
    }

    return (uint16_t)position;
}

static FunctionObject* createBuiltinFunctionObject(BuiltinId id)
{
    FunctionObject* function = createFunctionObject();
    function->type = FUNCTION_BUILTIN;
    function->entry = builtins[id].entry;
    function->paramCount = builtins[id].paramCount;
    function->returnCount = 1;

    if (builtins[id].typeId == TOKEN_VOID) {
        function->returnCount = 0;
    }

    function->maxStackCount = 2 + getCallAreaCount(function);

    return function;
}

static FunctionObject* createDefinedFunctionObject(ASTNode* ast)
{
    ASTNode* body = ast->functionDefinition.body;
    FunctionObject* function = createFunctionObject();
    function->paramCount = 0;

    for (size_t i = 0; i < countVector(&ast->functionDefinition.params); i++) {
        function->paramCount += getNodeSlotCount(
            getVectorAt(&ast->functionDefinition.params, i));
    }
    function->returnCount = getTypeSlotCount(ast->functionDefinition.typeId);

    if (ast->functionDefinition.typeId == TOKEN_VOID) {
        function->returnCount = 0;
    }

    function->localCount = body->compound.scope->localCount;
    int base = getCallAreaCount(function);
    function->maxStackCount = base + 2;

    return function;
}

static uint16_t getBuiltinFunctionPosition(Compiler* compiler, BuiltinId id)
{
    size_t functionCount = countVector(&compiler->module->functions);

    for (size_t i = 0; i < functionCount; i++) {
        FunctionObject* function = getVectorAt(&compiler->module->functions, i);

        if (function->type == FUNCTION_BUILTIN && function->entry == builtins[id].entry) {
            return makeFunctionPosition(i);
        }
    }

    uint16_t position = makeFunctionPosition(functionCount);
    FunctionObject* function = createBuiltinFunctionObject(id);

    pushVectorItem(&compiler->module->functions, function);
    pushVectorItem(&compiler->functionReferences, NULL);

    return position;
}

static uint16_t getFunctionPosition(Compiler* compiler, ASTNode* ast)
{
    size_t functionCount = countVector(&compiler->functionReferences);
    
    for (size_t i = 0; i < functionCount; i++) {
        if (ast == compiler->functionReferences.data[i]) {
            return makeFunctionPosition(i);
        }
    }

    functionNotFoundError(ast);

    return 0;
}

static Operand compileCall(Compiler* compiler, FunctionObject* function,
    uint16_t functionPosition, Vector* args, Vector* params, bool discard)
{
    if (function->type == FUNCTION_BUILTIN && !function->returnCount && function->paramCount == 1) {
        CallArea call = openCallArea(compiler);
        Operand argument = compileExpression(compiler, args->data[0], false);

        emitCallInstruction(compiler, (uint8_t)argument.reg, functionPosition);
        releaseOperand(compiler, argument);
        compiler->registerCount = call.resultRegister;

        return noOperand();
    }

    CallArea call = openCallArea(compiler);

    compileArguments(compiler, args, params);
    
    return closeCallArea(compiler, function, functionPosition, call, discard);
}

static Operand compilePower(Compiler* compiler, ASTNode* leftExpression, ASTNode* rightExpression)
{
    void* items[] = {leftExpression, rightExpression};
    Vector args = {.data = items, .capacity = 2, .count = 2};
    uint16_t position = getBuiltinFunctionPosition(compiler, BUILTIN_POW);
    FunctionObject* function = getVectorAt(&compiler->module->functions, position);

    return compileCall(compiler, function, position, &args, NULL, false);
}

static void compilePowerAssignment(Compiler* compiler, ASTNode* ast)
{
    ASTNode left = {.type = AST_VARIABLE};
    left.variable.symbol = ast->assignment.symbol;

    Operand result = compilePower(compiler, &left, ast->assignment.expr);
    storeAssignmentValue(compiler, ast->assignment.symbol, result);
}

static void compileCallWithArgument(Compiler* compiler, FunctionObject* function,
    uint16_t functionPosition, Operand argument, bool discard)
{
    if (function->type == FUNCTION_BUILTIN && !function->returnCount) {
        emitCallInstruction(compiler, (uint8_t)argument.reg, functionPosition);
        releaseOperand(compiler, argument);

        return;
    }

    CallArea call = openCallArea(compiler);
    int callArgumentRegister = compiler->registerCount;

    allocateRegisters(compiler, argument.slots);
    emitOperandMov(compiler, callArgumentRegister, argument);
    closeCallArea(compiler, function, functionPosition, call, discard);
    releaseOperand(compiler, argument);
}

static Operand compileBuiltinCall(Compiler* compiler, ASTNode* ast, bool discard)
{
    uint16_t position = getBuiltinFunctionPosition(compiler, ast->builtinCall.id);
    FunctionObject* function = getVectorAt(&compiler->module->functions, position);

    return compileCall(compiler, function, position, &ast->builtinCall.args, NULL, discard);
}

static Operand compileFunctionCall(Compiler* compiler, ASTNode* ast, bool discard)
{
    uint16_t position = getFunctionPosition(compiler, ast->functionCall.symbol);
    FunctionObject* function = getVectorAt(&compiler->module->functions, position);
    Vector* params = &ast->functionCall.symbol->functionDefinition.params;
    Operand result = compileCall(compiler, function, position, &ast->functionCall.args, params, discard);

    if (discard || getReferenceType(ast) == REFERENCE_NONE) {
        return result;
    }

    return dereferenceOperand(compiler, result, getTypeSlotCount(getTypeId(ast)));
}

static Operand compileReferenceExpression(Compiler* compiler, ASTNode* ast)
{
    if (getReferenceType(ast) == REFERENCE_NONE && ast->type != AST_VARIABLE) {
        Operand value = compileExpression(compiler, ast, false);
        int valueRegister = allocateRegister(compiler);

        emitMov(compiler, valueRegister, value.reg);
        emitInstruction(compiler, OP_REFL, value.reg, valueRegister, 0);

        return value;
    }

    if (ast->type == AST_PREFIX) {
        return referenceVariable(compiler, ast->prefix.expr->variable.symbol);
    }

    if (ast->type == AST_VARIABLE) {
        return referenceVariable(compiler, ast->variable.symbol);
    }

    if (ast->type == AST_FUNCTION_CALL) {
        uint16_t position = getFunctionPosition(compiler, ast->functionCall.symbol);
        FunctionObject* function = getVectorAt(&compiler->module->functions, position);
        Vector* params = &ast->functionCall.symbol->functionDefinition.params;

        return compileCall(compiler, function, position, &ast->functionCall.args, params, false);
    }

    return noOperand();
}

static void compileFunctionDefinition(Compiler* compiler, ASTNode* ast)
{
    int previousFrameBaseCount = compiler->frameBaseCount;
    int previousRegisterCount = compiler->registerCount;
    FunctionObject* previousFunction = compiler->function;
    FunctionObject* function = createDefinedFunctionObject(ast);
    ASTNode* body = ast->functionDefinition.body;
    
    compiler->frameBaseCount = getCallAreaCount(function);
    compiler->registerCount = compiler->frameBaseCount;
    compiler->function = function;
    
    pushVectorItem(&compiler->module->functions, function);
    pushVectorItem(&compiler->functionReferences, ast);
    compileBlocklevelStatements(compiler, &body->compound.statements);

    size_t statementCount = countVector(&body->compound.statements);
    ASTNode* last = NULL;

    if (statementCount > 0) {
        last = getVectorAt(&body->compound.statements, statementCount - 1);
    }

    if (!last || last->type != AST_RETURN) {
        compiler->registerCount = compiler->frameBaseCount;
        emitRet(compiler);
    }
    
    compiler->function = previousFunction;
    compiler->frameBaseCount = previousFrameBaseCount;
    compiler->registerCount = previousRegisterCount;
}

static void compileReturnStatement(Compiler* compiler, ASTNode* ast)
{
    if (isNone(ast->returnStatement.expr)) {
        compiler->registerCount = compiler->frameBaseCount;
        
        return emitRet(compiler);
    }

    bool reference = getReferenceType(ast->returnStatement.expr) != REFERENCE_NONE;
    Operand value = compileReferenceAwareExpression(compiler, ast->returnStatement.expr, reference);

    emitRetv(compiler, value);
    releaseOperand(compiler, value);
    compiler->registerCount = compiler->frameBaseCount;
}

static Operand compileZeroValue(Compiler* compiler, size_t slots)
{
    if (slots == 2) {
        size_t position = makeI64Constant(compiler, 0);

        return makeWideOperand(emitLdcI64(compiler, position), true);
    }

    return makeOperand(emitLdi(compiler, 0), true);
}

static void compileVariableDefinition(Compiler* compiler, ASTNode* ast)
{
    Operand value;
    size_t slots = getNodeSlotCount(ast);

    if (isNone(ast->variableDefinition.expr)) {
        value = compileZeroValue(compiler, slots);
    } else {
        bool reference = getReferenceType(ast) != REFERENCE_NONE;
        value = compileReferenceAwareExpression(compiler, ast->variableDefinition.expr, reference);
    }

    if (!value.temporary) {
        int reg = allocateRegisters(compiler, slots);

        emitOperandMov(compiler, reg, value);
    }
}

static Operand compileExpression(Compiler* compiler, ASTNode* ast, bool discard)
{
    Operand result;

    switch (ast->type) {
        case AST_BINARY:
            result = compileBinary(compiler, ast);
            break;
        case AST_BUILTIN_CALL:
            return compileBuiltinCall(compiler, ast, discard);
        case AST_FUNCTION_CALL:
            return compileFunctionCall(compiler, ast, discard);
        case AST_INTEGER:
            result = compileNumber(compiler, ast);
            break;
        case AST_PREFIX:
            result = compilePrefix(compiler, ast);
            break;
        case AST_VARIABLE:
            result = compileVariable(compiler, ast);
            break;
        default:
            return noOperand();
    }

    if (discard) {
        releaseOperand(compiler, result);

        return noOperand();
    }

    return result;
}

static Operand compileStatement(Compiler* compiler, ASTNode* ast, bool discard)
{
    switch (ast->type) {
        case AST_ASSIGNMENT:
            compileAssignment(compiler, ast);
            return noOperand();
        case AST_BUILTIN_CALL:
            return compileBuiltinCall(compiler, ast, discard);
        case AST_FUNCTION_CALL:
            return compileFunctionCall(compiler, ast, discard);
        case AST_FUNCTION_DEFINITION:
            compileFunctionDefinition(compiler, ast);
            return noOperand();
        case AST_RETURN:
            compileReturnStatement(compiler, ast);
            return noOperand();
        case AST_VARIABLE_DEFINITION:
            compileVariableDefinition(compiler, ast);
            return noOperand();
        default:
            return compileExpression(compiler, ast, discard);
    }
}

static void compileBlocklevelStatements(Compiler* compiler, Vector* nodes)
{
    size_t count = countVector(nodes);

    for (size_t i = 0; i < count; i++) {
        compileStatement(compiler, nodes->data[i], true);
    }
}

static void compileTopLevelStatements(Compiler* compiler, Vector* nodes)
{
    size_t count = countVector(nodes);

    while (compiler->statementIndex < count) {
        compileStatement(compiler, nodes->data[compiler->statementIndex], true);
        compiler->statementIndex++;
    }
}

static void compileReplStatement(Compiler* compiler, ASTNode* ast, bool isLast)
{
    bool display = isLast
        && isExpressionStatement(ast)
        && getTypeId(ast) != TOKEN_VOID;
        
    Operand value = compileStatement(compiler, ast, !display);

    if (!display) {
        return;
    }

    uint16_t position = getBuiltinFunctionPosition(compiler, BUILTIN_PRINT);
    FunctionObject* function = getVectorAt(&compiler->module->functions, position);

    compileCallWithArgument(compiler, function, position, value, true);
}

static void compileReplStatements(Compiler* compiler, Vector* nodes)
{
    size_t count = countVector(nodes);

    while (compiler->statementIndex < count) {
        ASTNode* ast = nodes->data[compiler->statementIndex++];
        bool isLast = compiler->statementIndex == count;

        compileReplStatement(compiler, ast, isLast);
    }
}

void initCompiler(Compiler* compiler, ModuleObject* module)
{
    initVector(&compiler->functionReferences);

    for (size_t i = 0; i < countVector(&module->functions); i++) {
        pushVectorItem(&compiler->functionReferences, NULL);
    }

    ASTNode* ast = createASTNode(AST_COMPOUND);
    ast->compound.scope = createScope(NULL);

    initParser(&compiler->parser, ast);
    initAnalyzer(&compiler->analyzer, ast);

    compiler->module = module;
    compiler->function = getVectorAt(&module->functions, 0);
    compiler->ast = ast;
    compiler->statementIndex = 0;
    compiler->registerCount = 0;
    compiler->frameBaseCount = 0;
}

void freeCompiler(Compiler* compiler)
{
    freeVector(&compiler->functionReferences);
    freeASTNode(compiler->ast);
}

static bool compileSource(Compiler* compiler, char* source, CompileStatements compileStatements)
{
    if (!compiler->module) {
        return false;
    }

    size_t start = countVector(&compiler->ast->compound.statements);

    if (!parse(&compiler->parser, source)) {
        return false;
    }

    analyze(&compiler->analyzer, start);
    optimize(compiler->ast, start);
    clearCodeObject(currentCodeObject(compiler));
    compileStatements(compiler, &compiler->ast->compound.statements);
    emitHlt(compiler);

    return true;
}

bool compile(Compiler* compiler, char* source)
{
    return compileSource(compiler, source, compileTopLevelStatements);
}

bool compileRepl(Compiler* compiler, char* source)
{
    return compileSource(compiler, source, compileReplStatements);
}
