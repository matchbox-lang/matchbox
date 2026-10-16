#include "compiler.h"
#include "ast.h"
#include "builtin.h"
#include "code_object.h"
#include "function_object.h"
#include "module_object.h"
#include "opcode.h"
#include "parser.h"
#include "ast_optimizer.h"
#include "bytecode_optimizer.h"
#include "scope.h"
#include "token.h"
#include "util.h"
#include "value.h"
#include "vector.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
static Operand compileStatement(Compiler* compiler, ASTNode* ast, bool discard);
static Operand compileReferenceExpression(Compiler* compiler, ASTNode* ast);
static Operand compileConditional(Compiler* compiler, ASTNode* ast, bool reference);
static Operand compileMatch(Compiler* compiler, ASTNode* ast, bool reference);
static Operand compileConvertedExpression(Compiler* compiler, ASTNode* ast, TokenType destination, bool reference);
static void compileBlocklevelStatements(Compiler* compiler, Vector* nodes);
static void compileTopLevelStatements(Compiler* compiler, Vector* nodes);
static size_t getNodeSlotCount(ASTNode* ast);
static Operand compilePower(Compiler* compiler, ASTNode* leftExpression, ASTNode* rightExpression);
static void compilePowerAssignment(Compiler* compiler, ASTNode* ast);
static Operand emitUnaryOperand(Compiler* compiler, Opcode opcode, Operand operand);
static bool compileDiscardedExpression(Compiler* compiler, ASTNode* ast);
static void removeDiscardedExpressionReads(ASTNode* ast);
static void removeDiscardedStatementReads(ASTNode* ast);
static void markCalledFunctions(ASTNode* ast);
static void resetFunctionCallCounts(ASTNode* ast);
static bool isSimpleMatchValue(ASTNode* ast);
static ASTNode* getCurrentFunctionDefinition(Compiler* compiler);
static bool isMatchAlternative(ASTNode* pattern);
static bool getLiteralMatchValue(ASTNode* ast, uint64_t* value);

static void constantIndexOverflowError(void)
{
    fprintf(stderr, "Error: Cannot create more than %u constants\n", UINT16_MAX + 1u);
    exit(1);
}

static void globalIndexOverflowError(void)
{
    fprintf(stderr, "Error: Cannot create more than %u globals\n", UINT16_MAX + 1u);
    exit(1);
}

static void functionIndexOverflowError(void)
{
    fprintf(stderr, "Error: Cannot create more than %u functions\n", UINT16_MAX + 1u);
    exit(1);
}

static void functionNotFoundError(ASTNode* ast)
{
    fprintf(stderr, "Error: Could not find function %s\n", ast->functionDefinition.id->chars);
    exit(1);
}

static void unresolvedReplExpressionTypeError(void)
{
    fprintf(stderr, "Error: Cannot display expression with an unresolved type\n");
    exit(1);
}

static void registerIndexOverflowError(ASTNode* function)
{
    if (!function) {
        fprintf(stderr, "Error: Top-level code requires more than %u virtual registers\n", UINT16_MAX + 1u);
        exit(1);
    }

    fprintf(stderr, "Error: Function %s requires more than %u virtual registers\n",
        function->functionDefinition.id->chars, UINT16_MAX + 1u);
    exit(1);
}

static void branchOffsetOverflowError(void)
{
    fprintf(stderr, "Error: Branch offset exceeds instruction range\n");
    exit(1);
}

static void checkRegisterCount(Compiler* compiler, int count)
{
    if (count <= (int)UINT16_MAX + 1) {
        return;
    }

    ASTNode* function = getCurrentFunctionDefinition(compiler);

    registerIndexOverflowError(function);
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
    checkRegisterCount(compiler, compiler->registerCount + 1);

    int reg = compiler->registerCount++;
    int extent = compiler->registerCount + 2;

    compiler->function->maxStackCount = MAX(compiler->function->maxStackCount, extent);

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

static void emitInstructionWord(Compiler* compiler, Opcode opcode, int a, int b, int c)
{
    CodeObject* code = currentCodeObject(compiler);

    pushByte(code, opcode);
    pushByte(code, (uint8_t)a);
    pushByte(code, (uint8_t)b);
    pushByte(code, (uint8_t)c);
}

static int getRegisterHighByte(Compiler* compiler, int operand, OpcodeRegisterFlag flag, OpcodeRegisterFlag field)
{
    if (!(flag & field)) {
        return 0;
    }

    checkRegisterCount(compiler, operand + 1);

    return operand >> 8;
}

static void emitInstruction(Compiler* compiler, Opcode opcode, int a, int b, int c)
{
    OpcodeRegisterFlag flag = getOpcodeRegisterFlag(opcode);
    
    int highA = getRegisterHighByte(compiler, a, flag, OP_REGISTER_A);
    int highB = getRegisterHighByte(compiler, b, flag, OP_REGISTER_B);
    int highC = getRegisterHighByte(compiler, c, flag, OP_REGISTER_C);

    if (highA || highB || highC) {
        emitInstructionWord(compiler, OP_WIDE, highA, highB, highC);
    }

    emitInstructionWord(compiler, opcode, a, b, c);
}

static void emitHlt(Compiler* compiler)
{
    emitInstruction(compiler, OP_HLT, 0, 0, 0);
}

static bool getInstructionBefore(Compiler* compiler, size_t count, PreviousInstruction* instruction)
{
    CodeObject* code = currentCodeObject(compiler);

    if (count < INSTRUCTION_SIZE) {
        return false;
    }

    size_t start = count - INSTRUCTION_SIZE;
    if (start < compiler->peepholeBarrier) {
        return false;
    }

    // Byte-based peephole rewrites must not edit the body of a prefixed instruction
    if (start >= INSTRUCTION_SIZE && code->data[start - INSTRUCTION_SIZE] == OP_WIDE) {
        return false;
    }

    instruction->code = code;
    instruction->start = start;
    instruction->opcode = (Opcode)code->data[start];
    instruction->a = code->data[start + 1];
    instruction->b = code->data[start + 2];
    instruction->c = code->data[start + 3];

    return true;
}

static bool getPreviousInstruction(Compiler* compiler, PreviousInstruction* instruction)
{
    CodeObject* code = currentCodeObject(compiler);
    size_t count = countCodeObject(code);

    return getInstructionBefore(compiler, count, instruction);
}

static void emitMov(Compiler* compiler, int dst, int src)
{
    if (dst == src) {
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

    emitInstruction(compiler, OP_MOV_64, dst, operand.reg, 0);
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

    emitInstruction(compiler, OP_LDC_64, reg, position >> 8, position);

    return reg;
}

static int emitLdi(Compiler* compiler, int16_t imm)
{
    int reg = allocateRegister(compiler);

    emitInstruction(compiler, OP_LDI, reg, imm >> 8, imm);

    return reg;
}

static void emitGlobalInstruction(Compiler* compiler, Opcode opcode, int reg, size_t position)
{
    if (position > UINT16_MAX) {
        globalIndexOverflowError();
    }

    emitInstruction(compiler, opcode, reg, position >> 8, position);
}

static int emitLdg(Compiler* compiler, size_t position)
{
    int reg = allocateRegister(compiler);

    emitGlobalInstruction(compiler, OP_LDG, reg, position);

    return reg;
}

static void emitCallInstruction(Compiler* compiler, int frameRegister, uint16_t functionPosition)
{
    checkRegisterCount(compiler, frameRegister + 1);

    emitInstruction(compiler, OP_CALL, frameRegister, functionPosition >> 8, functionPosition);
}

static void emitRet(Compiler* compiler)
{
    emitInstruction(compiler, OP_RET, 0, 0, 0);
}

static void emitRetv(Compiler* compiler, Operand operand)
{
    Opcode opcode = operand.slots == 2 ? OP_RET_64 : OP_RETV;

    emitInstruction(compiler, opcode, operand.reg, 0, 0);
}

static int getCallAreaCount(FunctionObject* function)
{
    return function->paramCount;
}

static size_t makeConstant(Compiler* compiler, Value value)
{
    size_t position = countValueArray(&compiler->module->constants);

    if (position > UINT16_MAX) {
        constantIndexOverflowError();
    }

    return pushValue(&compiler->module->constants, value) - 1;
}

static size_t makeI64Constant(Compiler* compiler, uint64_t value)
{
    size_t position = countValueArray(&compiler->module->constants);

    if (position > (size_t)UINT16_MAX - 1) {
        constantIndexOverflowError();
    }

#if UINTPTR_MAX == UINT32_MAX
    pushValue(&compiler->module->constants, U32_VALUE(value));
    pushValue(&compiler->module->constants, U32_VALUE(value >> 32));
#else
    pushValue(&compiler->module->constants, UNSIGNED_VALUE(value));
    pushValue(&compiler->module->constants, UNSIGNED_VALUE(0));
#endif

    return position;
}

static size_t makeF32Constant(Compiler* compiler, float value)
{
    Value constant = F32_VALUE(value);

    return makeConstant(compiler, constant);
}

static size_t makeF64Constant(Compiler* compiler, double value)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));

    return makeI64Constant(compiler, bits);
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

    emitCallInstruction(compiler, call.frameRegister, functionPosition);

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
    int position;

    if (isParameter(ast)) {
        position = ast->parameter.position;
    } else {
        position = ast->variableDefinition.registerPosition;
    }

    size_t slots = getNodeSlotCount(ast);
    int extent = position + slots;

    checkRegisterCount(compiler, extent);

    return position;
}

static Operand loadGlobalWithStoreForwarding(Compiler* compiler, size_t position)
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

    if (storedPosition != position || previous.a > compiler->registerCount) {
        return makeOperand(emitLdg(compiler, position), true);
    }

    if (previous.a == compiler->registerCount) {
        // Reclaim the released store operand while its value is still in the register
        int reg = allocateRegister(compiler);

        return makeOperand(reg, true);
    }

    return makeOperand(previous.a, false);
}

static Operand loadGlobalVariable(Compiler* compiler, ASTNode* ast)
{
    size_t position = ast->variableDefinition.position;
    bool wide = getNodeSlotCount(ast) == 2;

    if (wide) {
        int reg = allocateRegisters(compiler, 2);
        emitGlobalInstruction(compiler, OP_LDG_64, reg, position);

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
        size_t position = ast->variableDefinition.position;
        emitGlobalInstruction(compiler, OP_REFG, reg, position);

        return makeOperand(reg, true);
    }

    emitInstruction(compiler, OP_REFL, reg, getLocalPosition(compiler, ast), 0);

    return makeOperand(reg, true);
}

static Operand dereferenceOperand(Compiler* compiler, Operand operand, size_t slots)
{
    Opcode opcode = slots == 2 ? OP_LDR_64 : OP_LDR;

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

static void storeGlobalVariable(Compiler* compiler, ASTNode* ast, Operand value)
{
    size_t position = ast->variableDefinition.position;

    if (value.slots == 2) {
        emitGlobalInstruction(compiler, OP_STG_64, value.reg, position);

        return;
    }

    emitGlobalInstruction(compiler, OP_STG, value.reg, position);
}

static bool retargetTemporary(Compiler* compiler, Operand value, int dst)
{
    PreviousInstruction previous;

    if (!value.temporary || dst > (int)UINT8_MAX) {
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

static bool isNarrowIntegerImmediate(uint64_t value, TokenType type)
{
    bool signedType = isSignedIntegerTypeToken(type);

    if (!signedType) {
        return value <= INT16_MAX;
    }

    // Folded i32 arithmetic may store its result as unsigned 32-bit bits
    int32_t signedValue = (int32_t)value;

    return signedValue >= INT16_MIN && signedValue <= INT16_MAX;
}

static Operand compileNumber(Compiler* compiler, ASTNode* ast)
{
    uint64_t value = ast->integerLiteral.value;
    TokenType type = getTypeId(ast);

    if (type == TOKEN_F64) {
        size_t position = makeF64Constant(compiler, (double)value);

        return makeWideOperand(emitLdcI64(compiler, position), true);
    }

    if (type == TOKEN_F32) {
        size_t position = makeF32Constant(compiler, (float)value);

        return makeOperand(emitLdc(compiler, position), true);
    }

    if (getNodeSlotCount(ast) == 2) {
        return compileWideNumber(compiler, value);
    }

    bool immediate = isNarrowIntegerImmediate(value, type);

    if (!immediate) {
        Value constant = isSignedIntegerTypeToken(type)
            ? SIGNED_VALUE((int64_t)value)
            : UNSIGNED_VALUE(value);
        size_t position = makeConstant(compiler, constant);

        return makeOperand(emitLdc(compiler, position), true);
    }

    return makeOperand(emitLdi(compiler, (int16_t)value), true);
}

static Operand compileFloat(Compiler* compiler, ASTNode* ast)
{
    double value = ast->floatLiteral.value;

    if (getTypeId(ast) == TOKEN_F64) {
        size_t position = makeF64Constant(compiler, value);
        int reg = emitLdcI64(compiler, position);

        return makeWideOperand(reg, true);
    }

    size_t position = makeF32Constant(compiler, (float)value);
    int reg = emitLdc(compiler, position);

    return makeOperand(reg, true);
}

static Operand compileCharacter(Compiler* compiler, ASTNode* ast)
{
    uint32_t value = ast->characterLiteral.value;
    int reg;

    if (value <= INT16_MAX) {
        reg = emitLdi(compiler, (int16_t)value);
    } else {
        Value constant = U32_VALUE(value);
        size_t position = makeConstant(compiler, constant);

        reg = emitLdc(compiler, position);
    }

    return makeOperand(reg, true);
}

static Operand compileBoolean(Compiler* compiler, ASTNode* ast)
{
    return makeOperand(emitLdi(compiler, ast->booleanLiteral.value), true);
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
            return getWidthOpcode(type, OP_NEG_I64, OP_NEG_I32, OP_NEG_I16, OP_NEG_I8);
    }

    return OP_HLT;
}

static Operand emitBinaryOperands(
    Compiler* compiler, IntegerOperation operation, TokenType type, Operand left, Operand right)
{
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

static bool prepareImmediateComparison(
    Compiler* compiler, Opcode opcode, Operand* left, Operand* right, PreviousInstruction* load)
{
    if ((opcode != OP_EQ && opcode != OP_NE) || !getPreviousInstruction(compiler, load)
        || load->opcode != OP_LDI) {
        return false;
    }

    int16_t value = load->b << 8 | load->c;

    if (left->temporary && !right->temporary && left->slots == 1 && load->a == left->reg) {
        Operand swap = *left;
        *left = *right;
        *right = swap;
    }

    return right->temporary && right->slots == 1 && load->a == right->reg
        && value >= INT8_MIN && value <= INT8_MAX;
}

static Operand emitImmediateComparisonValue(Compiler* compiler, Opcode opcode, Operand left, int8_t value)
{
    int destination = left.reg;

    if (!left.temporary) {
        destination = allocateRegister(compiler);
    }

    Opcode immediate = OP_EQI;

    if (opcode == OP_NE) {
        immediate = OP_NEI;
    }

    emitInstruction(compiler, immediate, destination, left.reg, value);

    return makeOperand(destination, true);
}

static Operand emitImmediateComparison(Compiler* compiler, Opcode opcode, Operand left, Operand right)
{
    PreviousInstruction load;

    if (!prepareImmediateComparison(compiler, opcode, &left, &right, &load)) {
        return noOperand();
    }

    resizeCodeObject(load.code, load.start);
    releaseOperand(compiler, right);

    return emitImmediateComparisonValue(compiler, opcode, left, load.c);
}

static Operand emitComparison(Compiler* compiler, Opcode opcode, Operand left, Operand right)
{
    Operand immediate = emitImmediateComparison(compiler, opcode, left, right);

    if (immediate.reg >= 0) {
        return immediate;
    }

    if (left.temporary) {
        releaseOperand(compiler, right);
        emitInstruction(compiler, opcode, left.reg, left.reg, right.reg);

        return makeOperand(left.reg, true);
    }

    int destination = right.temporary ? right.reg : allocateRegister(compiler);
    emitInstruction(compiler, opcode, destination, left.reg, right.reg);

    return makeOperand(destination, true);
}

static Operand emitReversedComparison(Compiler* compiler, Opcode opcode, Operand left, Operand right)
{
    if (left.temporary) {
        releaseOperand(compiler, right);
        emitInstruction(compiler, opcode, left.reg, right.reg, left.reg);

        return makeOperand(left.reg, true);
    }

    int destination = right.temporary ? right.reg : allocateRegister(compiler);
    emitInstruction(compiler, opcode, destination, right.reg, left.reg);

    return makeOperand(destination, true);
}

static Opcode getComparisonOpcode(TokenType type, bool orEqual)
{
    if (isSignedIntegerTypeToken(type)) {
        return orEqual ? OP_LE_INT : OP_LT_INT;
    }

    return orEqual ? OP_LE_UINT : OP_LT_UINT;
}

static Opcode getFloatOpcode(TokenType type, TokenType operatorType)
{
    switch (operatorType) {
        case TOKEN_PLUS:
            return type == TOKEN_F64 ? OP_ADD_F64 : OP_ADD_F32;
        case TOKEN_MINUS:
            return type == TOKEN_F64 ? OP_SUB_F64 : OP_SUB_F32;
        case TOKEN_STAR:
            return type == TOKEN_F64 ? OP_MUL_F64 : OP_MUL_F32;
        case TOKEN_SLASH:
            return type == TOKEN_F64 ? OP_DIV_F64 : OP_DIV_F32;
        case TOKEN_PERCENT:
            return type == TOKEN_F64 ? OP_REM_F64 : OP_REM_F32;
        default:
            return OP_HLT;
    }
}

static Opcode getFloatOperationOpcode(TokenType type, IntegerOperation operation)
{
    switch (operation) {
        case INTEGER_ADD:
            return type == TOKEN_F64 ? OP_ADD_F64 : OP_ADD_F32;
        case INTEGER_SUB:
            return type == TOKEN_F64 ? OP_SUB_F64 : OP_SUB_F32;
        case INTEGER_MUL:
            return type == TOKEN_F64 ? OP_MUL_F64 : OP_MUL_F32;
        case INTEGER_DIV:
            return type == TOKEN_F64 ? OP_DIV_F64 : OP_DIV_F32;
        case INTEGER_REM:
            return type == TOKEN_F64 ? OP_REM_F64 : OP_REM_F32;
        default:
            return OP_HLT;
    }
}

static Operand emitFloatOperands(Compiler* compiler, Opcode opcode,
    TokenType type, Operand left, Operand right)
{
    int destination;

    if (left.temporary) {
        destination = left.reg;
        releaseOperand(compiler, right);
    } else if (right.temporary) {
        destination = right.reg;
    } else {
        destination = allocateRegisters(compiler, getTypeSlotCount(type));
    }

    emitInstruction(compiler, opcode, destination, left.reg, right.reg);

    return type == TOKEN_F64
        ? makeWideOperand(destination, true)
        : makeOperand(destination, true);
}

static Operand compileFloatBinary(Compiler* compiler, ASTNode* ast)
{
    TokenType type = getTypeId(ast);
    Operand left = compileConvertedExpression(compiler, ast->binary.leftExpr, type, false);

    if (!left.temporary && !isDirectVariable(compiler, ast->binary.rightExpr)) {
        left = materializeOperand(compiler, left);
    }

    Operand right = compileConvertedExpression(compiler, ast->binary.rightExpr, type, false);
    Opcode opcode = getFloatOpcode(type, ast->binary.operator.type);

    return emitFloatOperands(compiler, opcode, type, left, right);
}

static Operand emitOrderedComparison(Compiler* compiler, TokenType type,
    Operand left, Operand right, bool orEqual, bool reversed)
{
    Opcode opcode = getComparisonOpcode(type, orEqual);

    if (reversed) {
        return emitReversedComparison(compiler, opcode, left, right);
    }

    return emitComparison(compiler, opcode, left, right);
}

static Operand emitBinaryOperator(Compiler* compiler, TokenType operatorType,
    TokenType type, Operand left, Operand right)
{
    switch (operatorType) {
        case TOKEN_EQUAL_EQUAL:
            return emitComparison(compiler, OP_EQ, left, right);
        case TOKEN_NOT_EQUAL:
            return emitComparison(compiler, OP_NE, left, right);
        case TOKEN_LESS:
            return emitOrderedComparison(compiler, type, left, right, false, false);
        case TOKEN_LESS_EQUAL:
            return emitOrderedComparison(compiler, type, left, right, true, false);
        case TOKEN_GREATER:
            return emitOrderedComparison(compiler, type, left, right, false, true);
        case TOKEN_GREATER_EQUAL:
            return emitOrderedComparison(compiler, type, left, right, true, true);
        case TOKEN_AND:
            return emitComparison(compiler, OP_AND, left, right);
        case TOKEN_OR:
            return emitComparison(compiler, OP_OR, left, right);
        case TOKEN_PLUS:
            return emitBinaryOperands(compiler, INTEGER_ADD, type, left, right);
        case TOKEN_MINUS:
            return emitBinaryOperands(compiler, INTEGER_SUB, type, left, right);
        case TOKEN_STAR:
            return emitBinaryOperands(compiler, INTEGER_MUL, type, left, right);
        case TOKEN_SLASH:
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

static Operand compileBinary(Compiler* compiler, ASTNode* ast)
{
    if (ast->binary.operator.type == TOKEN_POWER) {
        return compilePower(compiler, ast->binary.leftExpr, ast->binary.rightExpr);
    }

    if (isFloatTypeToken(getTypeId(ast))) {
        return compileFloatBinary(compiler, ast);
    }

    TokenType type = getTypeId(ast->binary.leftExpr);
    Operand left = compileExpression(compiler, ast->binary.leftExpr, false);

    if (!left.temporary && !isDirectVariable(compiler, ast->binary.rightExpr)) {
        left = materializeOperand(compiler, left);
    }

    Operand right = compileExpression(compiler, ast->binary.rightExpr, false);

    return emitBinaryOperator(compiler, ast->binary.operator.type, type, left, right);
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

static Opcode getNegationOpcode(TokenType type)
{
    if (type == TOKEN_F32) {
        return OP_NEG_F32;
    }

    if (type == TOKEN_F64) {
        return OP_NEG_F64;
    }

    return getIntegerOpcode(type, INTEGER_NEG);
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
        case TOKEN_MINUS: {
            TokenType type = getTypeId(ast);
            Opcode opcode = getNegationOpcode(type);

            return emitUnaryOperand(compiler, opcode, operand);
        }
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
    Opcode opcode = value.slots == 2 ? OP_STR_64 : OP_STR;

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

    Operand right = compileConvertedExpression(compiler, ast->assignment.expr, type, false);
    Operand result;

    if (isFloatTypeToken(type)) {
        Opcode opcode = getFloatOperationOpcode(type, operation);
        result = emitFloatOperands(compiler, opcode, type, left, right);
    } else {
        result = emitBinaryOperands(compiler, operation, type, left, right);
    }

    storeAssignmentValue(compiler, ast->assignment.symbol, result);
}

static void compileSimpleAssignment(Compiler* compiler, ASTNode* ast)
{
    if (ast->assignment.initializesBinding) {
        Operand reference = compileReferenceExpression(compiler, ast->assignment.expr);
        storeVariable(compiler, ast->assignment.symbol, reference);

        return;
    }

    Operand value = compileConvertedExpression(
        compiler, ast->assignment.expr, getTypeId(ast->assignment.symbol), false);

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

static Operand widenIntegerOperand(Compiler* compiler, Operand operand,
    TokenType source, TokenType destination)
{
    if (!canImplicitlyWidenInteger(source, destination)
        || getTypeSlotCount(destination) == operand.slots) {
        return operand;
    }

    int destinationRegister = operand.reg;

    if (!operand.temporary) {
        destinationRegister = allocateRegister(compiler);
        emitMov(compiler, destinationRegister, operand.reg);
    }

    int highRegister = allocateRegister(compiler);

    if (isSignedIntegerTypeToken(source)) {
        emitInstruction(compiler, OP_ASRI_I32, highRegister, destinationRegister, 31);

        return makeWideOperand(destinationRegister, true);
    }

    emitInstruction(compiler, OP_LDI, highRegister, 0, 0);
    return makeWideOperand(destinationRegister, true);
}

static Operand widenFloatOperand(Compiler* compiler, Operand operand,
    TokenType source, TokenType destination)
{
    if (source != TOKEN_F32 || destination != TOKEN_F64) {
        return operand;
    }

    int destinationRegister;

    if (operand.temporary) {
        destinationRegister = operand.reg;
        allocateRegister(compiler);
    } else {
        destinationRegister = allocateRegisters(compiler, 2);
    }

    emitInstruction(compiler, OP_F32_TO_F64, destinationRegister, operand.reg, 0);

    return makeWideOperand(destinationRegister, true);
}

static Operand convertIntegerToFloatOperand(Compiler* compiler, Operand operand,
    TokenType source, TokenType destination)
{
    if (!canImplicitlyConvertIntegerToFloat(source, destination)) {
        return operand;
    }

    bool wide = destination == TOKEN_F64;
    int destinationRegister = operand.reg;

    if (!operand.temporary) {
        destinationRegister = allocateRegisters(compiler, wide ? 2 : 1);
    } else if (wide) {
        allocateRegister(compiler);
    }

    bool signedInteger = isSignedIntegerTypeToken(source);
    Opcode opcode;

    if (wide) {
        opcode = signedInteger ? OP_INT_TO_F64 : OP_UINT_TO_F64;
    } else {
        opcode = signedInteger ? OP_INT_TO_F32 : OP_UINT_TO_F32;
    }

    emitInstruction(compiler, opcode, destinationRegister, operand.reg, 0);

    if (wide) {
        return makeWideOperand(destinationRegister, true);
    }

    return makeOperand(destinationRegister, true);
}

static Operand compileConvertedExpression(Compiler* compiler, ASTNode* ast,
    TokenType destination, bool reference)
{
    Operand operand = compileReferenceAwareExpression(compiler, ast, reference);

    if (reference) {
        return operand;
    }

    TokenType source = getTypeId(ast);
    operand = widenIntegerOperand(compiler, operand, source, destination);
    operand = convertIntegerToFloatOperand(compiler, operand, source, destination);

    return widenFloatOperand(compiler, operand, source, destination);
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

    ASTNode* argument = args->data[position];
    TokenType destination = parameter ? getTypeId(parameter) : getTypeId(argument);
    Operand expression = compileConvertedExpression(compiler, argument, destination, reference);

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
        ASTNode* node = params ? getVectorAt(params, i) : getVectorAt(args, i);
        slotCount += getNodeSlotCount(node);
    }

    reserveArgumentRegisters(compiler, slotCount, reserved);

    for (size_t i = 0, offset = 0; i < count; i++) {
        ASTNode* argument = getVectorAt(args, i);
        ASTNode* layout = params ? getVectorAt(params, i) : argument;
        compileArgument(compiler, args, params, i, firstRegister, offset, reserved);
        offset += getNodeSlotCount(layout);
    }
}

static uint16_t makeFunctionPosition(size_t position)
{
    if (position > UINT16_MAX) {
        functionIndexOverflowError();
    }

    return (uint16_t)position;
}

static FunctionObject* createBuiltinFunctionObject(Builtin* builtin)
{
    FunctionObject* function = createFunctionObject();
    function->type = FUNCTION_BUILTIN;
    function->entry = builtin->entry;
    function->paramCount = 0;
    function->returnCount = getTypeSlotCount(builtin->returnTypeId);

    for (int i = 0; i < builtin->paramCount; i++) {
        function->paramCount += getTypeSlotCount(builtin->params[i]);
    }

    if (builtin->returnTypeId == TOKEN_VOID) {
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
    function->returnCount = getTypeSlotCount(ast->functionDefinition.returnTypeId);

    if (ast->functionDefinition.returnTypeId == TOKEN_VOID) {
        function->returnCount = 0;
    }

    function->localCount = getMaxLocalCount(body->compound.scope);
    int base = getCallAreaCount(function);

    if (base > (int)UINT16_MAX + 1) {
        registerIndexOverflowError(ast);
    }

    function->maxStackCount = base + 2;

    return function;
}

static uint16_t getBuiltinFunctionPosition(Compiler* compiler, Builtin* builtin)
{
    size_t functionCount = countVector(&compiler->module->functions);

    for (size_t i = 0; i < functionCount; i++) {
        FunctionObject* function = getVectorAt(&compiler->module->functions, i);

        if (function->type == FUNCTION_BUILTIN && function->entry == builtin->entry) {
            return makeFunctionPosition(i);
        }
    }

    uint16_t position = makeFunctionPosition(functionCount);
    FunctionObject* function = createBuiltinFunctionObject(builtin);

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

static Operand compileCall(Compiler* compiler, FunctionObject* function, uint16_t functionPosition,
    Vector* args, Vector* params, bool discard)
{
    size_t argumentCount = countVector(args);

    if (function->type == FUNCTION_BUILTIN && !function->returnCount && argumentCount == 1) {
        int previousRegisterCount = compiler->registerCount;
        Operand argument = compileExpression(compiler, args->data[0], false);

        emitCallInstruction(compiler, argument.reg, functionPosition);
        releaseOperand(compiler, argument);
        compiler->registerCount = previousRegisterCount;

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
    TokenType argumentTypes[] = {TOKEN_I32, TOKEN_I32};
    Builtin* builtin = resolveBuiltin("pow", argumentTypes, 2);
    uint16_t position = getBuiltinFunctionPosition(compiler, builtin);
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
        emitCallInstruction(compiler, argument.reg, functionPosition);
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
    uint16_t position = getBuiltinFunctionPosition(compiler, ast->builtinCall.builtin);
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
    if (ast->type == AST_CONDITIONAL) {
        return compileConditional(compiler, ast, true);
    }

    if (ast->type == AST_MATCH) {
        return compileMatch(compiler, ast, true);
    }

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
    if (!compiler->repl && !ast->functionDefinition.callCount) {
        return;
    }

    int previousFrameBaseCount = compiler->frameBaseCount;
    int previousLocalPositionOffset = compiler->localPositionOffset;
    int previousRegisterCount = compiler->registerCount;
    size_t previousPeepholeBarrier = compiler->peepholeBarrier;
    FunctionObject* previousFunction = compiler->function;
    FunctionObject* function = createDefinedFunctionObject(ast);
    ASTNode* body = ast->functionDefinition.body;

    compiler->frameBaseCount = getCallAreaCount(function);
    compiler->localPositionOffset = 0;
    compiler->registerCount = compiler->frameBaseCount;
    compiler->function = function;
    compiler->peepholeBarrier = 0;

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

    optimizeBytecode(compiler->module, &function->code, 0);

    compiler->function = previousFunction;
    compiler->frameBaseCount = previousFrameBaseCount;
    compiler->localPositionOffset = previousLocalPositionOffset;
    compiler->registerCount = previousRegisterCount;
    compiler->peepholeBarrier = previousPeepholeBarrier;
}

static void compileReturnStatement(Compiler* compiler, ASTNode* ast)
{
    if (isNone(ast->returnStatement.expr)) {
        compiler->registerCount = compiler->frameBaseCount;

        return emitRet(compiler);
    }

    bool reference = getReferenceType(ast->returnStatement.expr) != REFERENCE_NONE;
    ASTNode* function = getCurrentFunctionDefinition(compiler);
    TokenType returnType = function->functionDefinition.returnTypeId;
    Operand value = compileConvertedExpression(
        compiler, ast->returnStatement.expr, returnType, reference);

    emitRetv(compiler, value);
    releaseOperand(compiler, value);
    compiler->registerCount = compiler->frameBaseCount;
}

static ASTNode* getCurrentFunctionDefinition(Compiler* compiler)
{
    size_t count = countVector(&compiler->module->functions);

    for (size_t i = 0; i < count; i++) {
        FunctionObject* function = getVectorAt(&compiler->module->functions, i);

        if (function == compiler->function) {
            ASTNode* reference = getVectorAt(&compiler->functionReferences, i);

            return reference;
        }
    }

    return NULL;
}

static Operand compileZeroValue(Compiler* compiler, size_t slots)
{
    if (slots == 2) {
        size_t position = makeI64Constant(compiler, 0);

        return makeWideOperand(emitLdcI64(compiler, position), true);
    }

    return makeOperand(emitLdi(compiler, 0), true);
}

static bool compileUnusedVariableDefinition(
    Compiler* compiler, ASTNode* ast, size_t slots, bool unused, bool preserve)
{
    if (!unused || preserve) {
        return false;
    }

    compileExpression(compiler, ast->variableDefinition.expr, true);

    if (isTopLevelScope(ast->variableDefinition.scope)) {
        return true;
    }

    // Preserve analyzed binding positions after discarding an unused initializer
    allocateRegisters(compiler, slots);

    return true;
}

static void finishGlobalDefinition(Compiler* compiler, ASTNode* ast, Operand value)
{
    if (!isTopLevelScope(ast->variableDefinition.scope)) {
        return;
    }

    storeGlobalVariable(compiler, ast, value);
    releaseOperand(compiler, value);
}

static void compileVariableDefinition(Compiler* compiler, ASTNode* ast)
{
    ast->variableDefinition.registerPosition = compiler->registerCount;
    Operand value;
    size_t slots = getNodeSlotCount(ast);
    bool unused = ast->variableDefinition.readCount == ast->variableDefinition.discardedReadCount;
    bool preserve = compiler->repl && isCompilingTopLevel(compiler);

    if (compileUnusedVariableDefinition(compiler, ast, slots, unused, preserve)) {
        return;
    }

    if (isNone(ast->variableDefinition.expr)) {
        value = compileZeroValue(compiler, slots);
    } else {
        bool reference = getReferenceType(ast) != REFERENCE_NONE;
        value = compileConvertedExpression(
            compiler, ast->variableDefinition.expr, getTypeId(ast), reference);
    }

    if (!value.temporary) {
        int reg = allocateRegisters(compiler, slots);

        emitOperandMov(compiler, reg, value);

        value.reg = reg;
        value.temporary = true;
    }

    finishGlobalDefinition(compiler, ast, value);
}

static size_t emitJump(Compiler* compiler, Opcode opcode, int conditionRegister)
{
    size_t position = countCodeObject(currentCodeObject(compiler));
    emitInstruction(compiler, opcode, conditionRegister, 0, 0);

    return position;
}

static size_t emitConditionJump(Compiler* compiler, Operand condition)
{
    PreviousInstruction comparison;

    if (!condition.temporary || !getPreviousInstruction(compiler, &comparison)
        || (comparison.opcode != OP_EQI && comparison.opcode != OP_NEI) || comparison.a != condition.reg) {
        return emitJump(compiler, OP_BZ, condition.reg);
    }

    Opcode opcode = OP_BNEI;
    size_t position = comparison.start + INSTRUCTION_SIZE;

    if (comparison.opcode == OP_NEI) {
        opcode = OP_BEQI;
    }

    // Keep the comparison operands in a placeholder for the long-branch fallback
    setByteAt(comparison.code, comparison.start, OP_NOP);
    emitInstruction(compiler, opcode, comparison.b, comparison.c, 0);

    return position;
}

static Opcode expandImmediateJump(CodeObject* code, size_t position, Opcode opcode, size_t offset)
{
    if ((opcode != OP_BEQI && opcode != OP_BNEI) || offset <= INT8_MAX) {
        return opcode;
    }

    size_t comparison = position - INSTRUCTION_SIZE;
    Opcode comparisonOpcode = OP_EQI;

    if (opcode == OP_BEQI) {
        comparisonOpcode = OP_NEI;
    }

    setByteAt(code, comparison, comparisonOpcode);
    setByteAt(code, position, OP_BZ);
    setByteAt(code, position + 1, code->data[comparison + 1]);

    return OP_BZ;
}

static void checkJumpOffset(Opcode opcode, size_t offset)
{
    size_t maximum = INT32_MAX >> 8;

    if (opcode == OP_BZ || opcode == OP_BNZ) {
        maximum = INT16_MAX;
    }

    if (opcode >= OP_BEQ && opcode <= OP_BLE_UINT) {
        maximum = INT8_MAX;
    }

    if (offset > maximum) {
        branchOffsetOverflowError();
    }
}

static void patchJump(Compiler* compiler, size_t position)
{
    CodeObject* code = currentCodeObject(compiler);

    // A branch offset starts after its body word, excluding the prefix
    if (code->data[position] == OP_WIDE) {
        position += INSTRUCTION_SIZE;
    }

    size_t count = countCodeObject(code);
    size_t wordOffset = (count - position - INSTRUCTION_SIZE) / INSTRUCTION_SIZE;
    Opcode opcode = (Opcode)code->data[position];
    compiler->peepholeBarrier = count;
    opcode = expandImmediateJump(code, position, opcode, wordOffset);

    checkJumpOffset(opcode, wordOffset);

    int offset = (int)wordOffset;

    if (opcode == OP_BZ || opcode == OP_BNZ) {
        setByteAt(code, position + 2, offset >> 8);
        setByteAt(code, position + 3, offset);

        return;
    }

    if (opcode >= OP_BEQ && opcode <= OP_BLE_UINT) {
        setByteAt(code, position + 3, offset);

        return;
    }

    setByteAt(code, position + 1, offset >> 16);
    setByteAt(code, position + 2, offset >> 8);
    setByteAt(code, position + 3, offset);
}

static void storeConditionalValue(
    Compiler* compiler, TokenType type, int destination, Operand value)
{
    if (type == TOKEN_VOID) {
        return;
    }

    if (retargetTemporary(compiler, value, destination)) {
        return;
    }

    emitOperandMov(compiler, destination, value);
}

static Operand compileConditionalBranch(
    Compiler* compiler, ASTNode* branch, TokenType type, int destination, bool reference)
{
    if (branch->type == AST_CONDITIONAL) {
        Operand value = compileConvertedExpression(compiler, branch, type, reference);

        storeConditionalValue(compiler, type, destination, value);

        return value;
    }

    size_t count = countVector(&branch->compound.statements);

    for (size_t i = 0; i < count; i++) {
        bool discard = type == TOKEN_VOID || i + 1 < count;
        ASTNode* statement = getVectorAt(&branch->compound.statements, i);

        if (discard) {
            compileStatement(compiler, statement, true);

            continue;
        }

        Operand value = compileConvertedExpression(compiler, statement, type, reference);

        storeConditionalValue(compiler, type, destination, value);

        return value;
    }

    return noOperand();
}

static Operand allocateConditionalResult(Compiler* compiler, TokenType type)
{
    if (type == TOKEN_VOID) {
        return noOperand();
    }

    size_t slots = getTypeSlotCount(type);
    int reg = allocateRegisters(compiler, slots);
    Operand result = {reg, true, slots};
    compiler->localPositionOffset += slots;

    return result;
}

static void compileScopedConditionalBranch(
    Compiler* compiler, ASTNode* branch, TokenType type, int destination, bool reference)
{
    if (!branch) {
        return;
    }

    int registerCount = compiler->registerCount;

    compileConditionalBranch(compiler, branch, type, destination, reference);

    compiler->registerCount = registerCount;
}

static void compileConditionalBranches(
    Compiler* compiler, ASTNode* ast, int destination, bool reference, size_t elseJump)
{
    ASTNode* condition = ast->conditional.condition;
    TokenType type = getTypeId(ast);

    if (condition->type == AST_BOOLEAN) {
        ASTNode* branch = condition->booleanLiteral.value ? ast->conditional.thenBranch : ast->conditional.elseBranch;

        compileScopedConditionalBranch(compiler, branch, type, destination, reference);

        return;
    }

    compileScopedConditionalBranch(compiler, ast->conditional.thenBranch, type, destination, reference);

    if (!ast->conditional.elseBranch) {
        patchJump(compiler, elseJump);

        return;
    }

    size_t endJump = emitJump(compiler, OP_JMP, 0);

    patchJump(compiler, elseJump);
    compileScopedConditionalBranch(compiler, ast->conditional.elseBranch, type, destination, reference);
    patchJump(compiler, endJump);
}

static Operand compileConditional(Compiler* compiler, ASTNode* ast, bool reference)
{
    int previousLocalPositionOffset = compiler->localPositionOffset;
    ASTNode* conditionNode = ast->conditional.condition;
    size_t elseJump = 0;

    if (conditionNode->type != AST_BOOLEAN) {
        Operand condition = compileExpression(compiler, conditionNode, false);
        elseJump = emitConditionJump(compiler, condition);

        releaseOperand(compiler, condition);
    }

    TokenType type = getTypeId(ast);
    Operand result = allocateConditionalResult(compiler, type);

    compileConditionalBranches(compiler, ast, result.reg, reference, elseJump);

    compiler->localPositionOffset = previousLocalPositionOffset;

    return result;
}

static Operand compileMatchSubject(Compiler* compiler, ASTNode* subject)
{
    Operand value = compileExpression(compiler, subject, false);
    value.temporary = false;

    return value;
}

static bool isMatchAlternative(ASTNode* pattern)
{
    return pattern->type == AST_BINARY && pattern->binary.operator.type == TOKEN_PIPE;
}

static Operand compileSubjectMatchCondition(Compiler* compiler, ASTNode* pattern, Operand subject)
{
    if (!isMatchAlternative(pattern)) {
        Operand value = compileExpression(compiler, pattern, false);

        return emitComparison(compiler, OP_EQ, subject, value);
    }

    Operand left = compileSubjectMatchCondition(compiler, pattern->binary.leftExpr, subject);
    Operand right = compileSubjectMatchCondition(compiler, pattern->binary.rightExpr, subject);

    return emitComparison(compiler, OP_OR, left, right);
}

static Operand compileMatchCondition(Compiler* compiler, ASTNode* ast, MatchArm* arm, Operand subject)
{
    if (!ast->match.subject) {
        return compileExpression(compiler, arm->pattern, false);
    }

    return compileSubjectMatchCondition(compiler, arm->pattern, subject);
}

static void patchMatchJumps(Compiler* compiler, Vector* jumps)
{
    size_t count = countVector(jumps);

    for (size_t i = 0; i < count; i++) {
        void* jump = getVectorAt(jumps, i);
        patchJump(compiler, (size_t)(uintptr_t)jump);
    }
}

static void markMatchJoin(Compiler* compiler, Vector* jumps)
{
    if (!countVector(jumps)) {
        return;
    }

    patchMatchJumps(compiler, jumps);
}

static int allocateMatchDestination(Compiler* compiler, TokenType type)
{
    if (type == TOKEN_VOID) {
        return -1;
    }

    size_t slots = getTypeSlotCount(type);
    int destination = allocateRegisters(compiler, slots);
    compiler->localPositionOffset += slots;

    return destination;
}

static bool areSimpleMatchValues(Vector* values)
{
    size_t count = countVector(values);

    for (size_t i = 0; i < count; i++) {
        ASTNode* value = getVectorAt(values, i);
        if (!isSimpleMatchValue(value)) {
            return false;
        }
    }

    return true;
}

static bool isSimpleMatchValue(ASTNode* ast)
{
    switch (ast->type) {
        case AST_BOOLEAN:
        case AST_CHARACTER:
        case AST_INTEGER:
        case AST_FLOAT:
        case AST_VARIABLE:
            return true;
        case AST_BUILTIN_CALL:
            return areSimpleMatchValues(&ast->builtinCall.args);
        default:
            return false;
    }
}

static bool canUseDirectMatchBranch(MatchArm* arm)
{
    size_t count = countVector(&arm->branch->compound.statements);
    if (count != 1) {
        return false;
    }

    ASTNode* statement = getVectorAt(&arm->branch->compound.statements, 0);

    return isSimpleMatchValue(statement);
}

static bool isLiteralMatchPattern(ASTNode* pattern)
{
    if (pattern->type == AST_INTEGER || pattern->type == AST_BOOLEAN) {
        return true;
    }

    if (!isMatchAlternative(pattern)) {
        return false;
    }

    return isLiteralMatchPattern(pattern->binary.leftExpr) && isLiteralMatchPattern(pattern->binary.rightExpr);
}

static size_t compileMatchAlternativeJumps(
    Compiler* compiler, ASTNode* pattern, Operand subject, bool last, Vector* matches)
{
    if (isMatchAlternative(pattern)) {
        compileMatchAlternativeJumps(compiler, pattern->binary.leftExpr, subject, false, matches);

        return compileMatchAlternativeJumps(compiler, pattern->binary.rightExpr, subject, last, matches);
    }

    Opcode opcode = OP_NE;

    if (last) {
        opcode = OP_EQ;
    }

    Operand value = compileExpression(compiler, pattern, false);
    Operand condition = emitComparison(compiler, opcode, subject, value);
    size_t jump = emitConditionJump(compiler, condition);

    releaseOperand(compiler, condition);

    if (!last) {
        pushVectorItem(matches, (void*)(uintptr_t)jump);
    }

    return jump;
}

static size_t compileMatchAlternativeCondition(Compiler* compiler, ASTNode* pattern, Operand subject)
{
    Vector matches;

    initVector(&matches);

    size_t nextJump = compileMatchAlternativeJumps(compiler, pattern, subject, true, &matches);

    patchMatchJumps(compiler, &matches);
    freeVector(&matches);

    return nextJump;
}

static size_t compileMatchArmCondition(Compiler* compiler, ASTNode* ast, MatchArm* arm, Operand subject)
{
    if (!arm->pattern) {
        return 0;
    }

    if (ast->match.subject && subject.slots == 1 && isMatchAlternative(arm->pattern)
        && isLiteralMatchPattern(arm->pattern)) {
        return compileMatchAlternativeCondition(compiler, arm->pattern, subject);
    }

    if (!ast->match.subject || isMatchAlternative(arm->pattern) || !canUseDirectMatchBranch(arm)) {
        Operand condition = compileMatchCondition(compiler, ast, arm, subject);
        size_t nextJump = emitConditionJump(compiler, condition);
        releaseOperand(compiler, condition);

        return nextJump;
    }

    uint64_t patternValue;

    if (subject.slots == 1 && getLiteralMatchValue(arm->pattern, &patternValue) && patternValue == 0) {
        return emitJump(compiler, OP_BNZ, subject.reg);
    }

    Operand pattern = compileExpression(compiler, arm->pattern, false);
    Operand condition = emitImmediateComparison(compiler, OP_EQ, subject, pattern);

    if (condition.reg >= 0) {
        size_t nextJump = emitConditionJump(compiler, condition);

        releaseOperand(compiler, condition);

        return nextJump;
    }

    size_t nextJump = countCodeObject(currentCodeObject(compiler));
    emitInstruction(compiler, OP_BNE, subject.reg, pattern.reg, 0);
    releaseOperand(compiler, pattern);

    return nextJump;
}

static void storeMatchBinding(Compiler* compiler, ASTNode* binding, Operand subject)
{
    if (!binding) {
        return;
    }

    binding->variableDefinition.registerPosition = compiler->frameBaseCount + compiler->localPositionOffset
        + binding->variableDefinition.position;
    int position = getLocalPosition(compiler, binding);

    emitOperandMov(compiler, position, subject);

    TokenType bindingType = getTypeId(binding);
    int bindingRegisterCount = position + getTypeSlotCount(bindingType);

    compiler->registerCount = MAX(compiler->registerCount, bindingRegisterCount);
}

static size_t compileMatchArm(Compiler* compiler, ASTNode* ast, MatchArm* arm, Operand subject, int destination,
    bool needsEndJump, bool reference)
{
    int branchRegisterCount = compiler->registerCount;
    size_t nextJump = compileMatchArmCondition(compiler, ast, arm, subject);
    TokenType type = getTypeId(ast);

    storeMatchBinding(compiler, arm->binding, subject);
    compileConditionalBranch(compiler, arm->branch, type, destination, reference);
    compiler->registerCount = branchRegisterCount;

    size_t endJump = SIZE_MAX;

    if (needsEndJump) {
        endJump = emitJump(compiler, OP_JMP, 0);
    }

    if (arm->pattern) {
        patchJump(compiler, nextJump);
    }

    return endJump;
}

static bool compileNextMatchArm(Compiler* compiler, ASTNode* ast, MatchArm* arm, Operand subject, int destination,
    bool hasFollowingArm, Vector* endJumps, bool reference)
{
    bool unconditional = !arm->pattern;
    bool needsEndJump = !unconditional && hasFollowingArm;
    size_t endJump = compileMatchArm(compiler, ast, arm, subject, destination, needsEndJump, reference);

    if (endJump != SIZE_MAX) {
        pushVectorItem(endJumps, (void*)(uintptr_t)endJump);
    }

    return unconditional;
}

static bool compileMatchArms(
    Compiler* compiler, ASTNode* ast, Operand subject, int destination, Vector* endJumps, bool reference)
{
    size_t count = countVector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = getVectorAt(&ast->match.arms, i);
        bool hasFollowingArm = i + 1 < count || ast->match.defaultBranch;
        bool unconditional = compileNextMatchArm(compiler, ast, arm, subject, destination, hasFollowingArm,
            endJumps, reference);

        if (unconditional) {
            return true;
        }
    }

    return false;
}

static void compileMatchDefault(
    Compiler* compiler, ASTNode* ast, int destination, int branchRegisterCount, bool reference)
{
    if (!ast->match.defaultBranch) {
        return;
    }

    TokenType type = getTypeId(ast);

    compileConditionalBranch(compiler, ast->match.defaultBranch, type, destination, reference);
    compiler->registerCount = branchRegisterCount;
}

static Operand matchResultOperand(TokenType type, int destination)
{
    if (type == TOKEN_VOID) {
        return noOperand();
    }

    if (getTypeSlotCount(type) == 2) {
        return makeWideOperand(destination, true);
    }

    return makeOperand(destination, true);
}

static bool getLiteralMatchValue(ASTNode* ast, uint64_t* value)
{
    if (ast->type == AST_BOOLEAN) {
        *value = ast->booleanLiteral.value;

        return true;
    }

    if (ast->type == AST_CHARACTER) {
        *value = ast->characterLiteral.value;

        return true;
    }

    if (ast->type != AST_INTEGER) {
        return false;
    }

    *value = ast->integerLiteral.value;

    if (ast->integerLiteral.typeId == TOKEN_I32) {
        *value = (uint64_t)(int64_t)(int32_t)*value;
    }

    return true;
}

static bool getLiteralMatchSubject(ASTNode* ast, uint64_t* value)
{
    if (!ast->match.subject) {
        *value = true;

        return true;
    }

    return getLiteralMatchValue(ast->match.subject, value);
}

typedef enum LiteralMatchArmResult
{
    LITERAL_MATCH_ARM_UNAVAILABLE,
    LITERAL_MATCH_ARM_SKIPPED,
    LITERAL_MATCH_ARM_SELECTED
} LiteralMatchArmResult;

static bool evaluateLiteralMatchPattern(ASTNode* pattern, uint64_t subjectValue, bool* matches)
{
    uint64_t patternValue;

    if (getLiteralMatchValue(pattern, &patternValue)) {
        *matches = subjectValue == patternValue;

        return true;
    }

    if (!isMatchAlternative(pattern)) {
        return false;
    }

    bool left;
    bool right;

    if (!evaluateLiteralMatchPattern(pattern->binary.leftExpr, subjectValue, &left)
        || !evaluateLiteralMatchPattern(pattern->binary.rightExpr, subjectValue, &right)) {
        return false;
    }

    *matches = left || right;

    return true;
}

static LiteralMatchArmResult selectLiteralMatchArm(
    MatchArm* arm, uint64_t subjectValue, ASTNode** branch, ASTNode** binding)
{
    if (!arm->pattern) {
        *branch = arm->branch;
        *binding = arm->binding;

        return LITERAL_MATCH_ARM_SELECTED;
    }

    bool matches;

    if (!evaluateLiteralMatchPattern(arm->pattern, subjectValue, &matches)) {
        return LITERAL_MATCH_ARM_UNAVAILABLE;
    }

    if (!matches) {
        return LITERAL_MATCH_ARM_SKIPPED;
    }

    *branch = arm->branch;
    *binding = NULL;

    return LITERAL_MATCH_ARM_SELECTED;
}

static bool findLiteralMatchBranch(ASTNode* ast, ASTNode** branch, ASTNode** binding)
{
    uint64_t subjectValue;
    if (!getLiteralMatchSubject(ast, &subjectValue)) {
        return false;
    }

    size_t count = countVector(&ast->match.arms);
    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = getVectorAt(&ast->match.arms, i);
        LiteralMatchArmResult result = selectLiteralMatchArm(arm, subjectValue, branch, binding);

        switch (result) {
            case LITERAL_MATCH_ARM_UNAVAILABLE:
                return false;
            case LITERAL_MATCH_ARM_SELECTED:
                return true;
            case LITERAL_MATCH_ARM_SKIPPED:
                break;
        }
    }

    *branch = ast->match.defaultBranch;
    *binding = NULL;

    return true;
}

static bool compileLiteralMatch(
    Compiler* compiler, ASTNode* ast, TokenType type, int destination, bool reference)
{
    ASTNode* branch;
    ASTNode* binding;
    if (!findLiteralMatchBranch(ast, &branch, &binding)) {
        return false;
    }

    if (!branch) {
        return true;
    }

    if (binding) {
        Operand subject = compileMatchSubject(compiler, ast->match.subject);
        storeMatchBinding(compiler, binding, subject);
    }

    compileConditionalBranch(compiler, branch, type, destination, reference);

    return true;
}

static void restoreMatchCompilerState(Compiler* compiler, TokenType type, int previousLocalPositionOffset,
    int previousRegisterCount)
{
    compiler->localPositionOffset = previousLocalPositionOffset;
    compiler->registerCount = previousRegisterCount;

    if (type != TOKEN_VOID) {
        compiler->registerCount += getTypeSlotCount(type);
    }
}

static Operand compileMatch(Compiler* compiler, ASTNode* ast, bool reference)
{
    int previousLocalPositionOffset = compiler->localPositionOffset;
    int previousRegisterCount = compiler->registerCount;
    TokenType type = getTypeId(ast);
    int destination = allocateMatchDestination(compiler, type);

    if (compileLiteralMatch(compiler, ast, type, destination, reference)) {
        restoreMatchCompilerState(compiler, type, previousLocalPositionOffset, previousRegisterCount);

        return matchResultOperand(type, destination);
    }

    Operand subject = noOperand();

    if (ast->match.subject) {
        subject = compileMatchSubject(compiler, ast->match.subject);
    }

    int branchRegisterCount = compiler->registerCount;

    Vector endJumps;
    initVector(&endJumps);

    bool unconditionalArm = compileMatchArms(compiler, ast, subject, destination, &endJumps, reference);

    if (!unconditionalArm) {
        compileMatchDefault(compiler, ast, destination, branchRegisterCount, reference);
    }

    markMatchJoin(compiler, &endJumps);
    freeVector(&endJumps);

    restoreMatchCompilerState(compiler, type, previousLocalPositionOffset, previousRegisterCount);

    return matchResultOperand(type, destination);
}

static bool compileDiscardedExpression(Compiler* compiler, ASTNode* ast)
{
    switch (ast->type) {
        case AST_BOOLEAN:
        case AST_CHARACTER:
        case AST_FLOAT:
        case AST_INTEGER:
        case AST_VARIABLE:
            return true;
        case AST_BINARY:
            compileExpression(compiler, ast->binary.leftExpr, true);
            compileExpression(compiler, ast->binary.rightExpr, true);
            return true;
        case AST_PREFIX:
            compileExpression(compiler, ast->prefix.expr, true);
            return true;
        default:
            return false;
    }
}

static bool hasNoValueReads(ASTNode* ast)
{
    return ast->variableDefinition.readCount == ast->variableDefinition.discardedReadCount;
}

static void removeUnreadInitializerReads(ASTNode* ast)
{
    if (!hasNoValueReads(ast) || ast->variableDefinition.discardedReadsAnalyzed) {
        return;
    }

    ast->variableDefinition.discardedReadsAnalyzed = true;
    removeDiscardedExpressionReads(ast->variableDefinition.expr);
}

static void removeDiscardedVariableRead(ASTNode* ast)
{
    ASTNode* symbol = ast->variable.symbol;

    if (!isVariableDefinition(symbol)) {
        return;
    }

    symbol->variableDefinition.discardedReadCount++;
    removeUnreadInitializerReads(symbol);
}

static void removeDiscardedExpressionReads(ASTNode* ast)
{
    switch (ast->type) {
        case AST_VARIABLE:
            removeDiscardedVariableRead(ast);
            break;
        case AST_BINARY:
            removeDiscardedExpressionReads(ast->binary.leftExpr);
            removeDiscardedExpressionReads(ast->binary.rightExpr);
            break;
        case AST_PREFIX:
            removeDiscardedExpressionReads(ast->prefix.expr);
            break;
        default:
            break;
    }
}

static void removeDiscardedStatementVectorReads(Vector* nodes)
{
    size_t count = countVector(nodes);

    for (size_t i = 0; i < count; i++) {
        removeDiscardedStatementReads(getVectorAt(nodes, i));
    }
}

static void removeDiscardedStatementReads(ASTNode* ast)
{
    if (ast->type == AST_FUNCTION_DEFINITION) {
        removeDiscardedStatementVectorReads(&ast->functionDefinition.body->compound.statements);

        return;
    }

    if (ast->type == AST_VARIABLE_DEFINITION) {
        removeUnreadInitializerReads(ast);

        return;
    }

    removeDiscardedExpressionReads(ast);
}

static void removeDiscardedReads(Vector* nodes, size_t start)
{
    size_t count = countVector(nodes);

    for (size_t i = start; i < count; i++) {
        removeDiscardedStatementReads(getVectorAt(nodes, i));
    }
}

static void markCalledFunctionsInVector(Vector* nodes)
{
    size_t count = countVector(nodes);

    for (size_t i = 0; i < count; i++) {
        markCalledFunctions(getVectorAt(nodes, i));
    }
}

static void markCalledFunction(ASTNode* ast)
{
    ASTNode* function = ast->functionCall.symbol;

    markCalledFunctionsInVector(&ast->functionCall.args);

    if (!function) {
        return;
    }

    function->functionDefinition.callCount++;

    if (function->functionDefinition.callCount > 1) {
        return;
    }

    markCalledFunctions(function->functionDefinition.body);
}

static void markCalledFunctions(ASTNode* ast)
{
    if (!ast) {
        return;
    }

    switch (ast->type) {
        case AST_ASSIGNMENT:
            markCalledFunctions(ast->assignment.expr);
            break;
        case AST_BINARY:
            markCalledFunctions(ast->binary.leftExpr);
            markCalledFunctions(ast->binary.rightExpr);
            break;
        case AST_BUILTIN_CALL:
            markCalledFunctionsInVector(&ast->builtinCall.args);
            break;
        case AST_COMPOUND:
            markCalledFunctionsInVector(&ast->compound.statements);
            break;
        case AST_CONDITIONAL:
            markCalledFunctions(ast->conditional.condition);
            markCalledFunctions(ast->conditional.thenBranch);
            markCalledFunctions(ast->conditional.elseBranch);
            break;
        case AST_FUNCTION_CALL:
            markCalledFunction(ast);
            break;
        case AST_MATCH: {
            markCalledFunctions(ast->match.subject);

            size_t count = countVector(&ast->match.arms);
            for (size_t i = 0; i < count; i++) {
                MatchArm* arm = getVectorAt(&ast->match.arms, i);
                markCalledFunctions(arm->pattern);
                markCalledFunctions(arm->branch);
            }
            
            markCalledFunctions(ast->match.defaultBranch);
            break;
        }
        case AST_PREFIX:
            markCalledFunctions(ast->prefix.expr);
            break;
        case AST_RETURN:
            markCalledFunctions(ast->returnStatement.expr);
            break;
        case AST_VARIABLE_DEFINITION:
            markCalledFunctions(ast->variableDefinition.expr);
            break;
        default:
            break;
    }
}

static void resetFunctionCallCountsInVector(Vector* nodes)
{
    size_t count = countVector(nodes);

    for (size_t i = 0; i < count; i++) {
        ASTNode* node = getVectorAt(nodes, i);
        resetFunctionCallCounts(node);
    }
}

static void resetMatchFunctionCallCounts(ASTNode* ast)
{
    size_t count = countVector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = getVectorAt(&ast->match.arms, i);
        resetFunctionCallCounts(arm->branch);
    }

    resetFunctionCallCounts(ast->match.defaultBranch);
}

static void resetFunctionCallCounts(ASTNode* ast)
{
    if (!ast) {
        return;
    }

    if (ast->type == AST_FUNCTION_DEFINITION) {
        ast->functionDefinition.callCount = 0;
        resetFunctionCallCounts(ast->functionDefinition.body);

        return;
    }

    if (ast->type == AST_COMPOUND) {
        resetFunctionCallCountsInVector(&ast->compound.statements);

        return;
    }

    if (ast->type == AST_CONDITIONAL) {
        resetFunctionCallCounts(ast->conditional.thenBranch);
        resetFunctionCallCounts(ast->conditional.elseBranch);

        return;
    }

    if (ast->type == AST_MATCH) {
        resetMatchFunctionCallCounts(ast);
    }
}

static void eliminateDeadFunctions(Vector* nodes)
{
    size_t count = countVector(nodes);

    resetFunctionCallCountsInVector(nodes);

    for (size_t i = 0; i < count; i++) {
        ASTNode* ast = getVectorAt(nodes, i);

        if (ast->type != AST_FUNCTION_DEFINITION) {
            markCalledFunctions(ast);
        }
    }
}

static Operand compileExpression(Compiler* compiler, ASTNode* ast, bool discard)
{
    if (discard && compileDiscardedExpression(compiler, ast)) {
        return noOperand();
    }

    Operand result;

    switch (ast->type) {
        case AST_BINARY:
            result = compileBinary(compiler, ast);
            break;
        case AST_CHARACTER:
            result = compileCharacter(compiler, ast);
            break;
        case AST_BOOLEAN:
            result = compileBoolean(compiler, ast);
            break;
        case AST_BUILTIN_CALL:
            return compileBuiltinCall(compiler, ast, discard);
        case AST_CONDITIONAL:
            result = compileConditional(compiler, ast, false);
            break;
        case AST_MATCH:
            result = compileMatch(compiler, ast, false);
            break;
        case AST_FUNCTION_CALL:
            return compileFunctionCall(compiler, ast, discard);
        case AST_FLOAT:
            result = compileFloat(compiler, ast);
            break;
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

static void finishTopLevelStatement(Compiler* compiler, ASTNode* ast)
{
    if (ast->type != AST_VARIABLE_DEFINITION) {
        return;
    }

    size_t slots = getNodeSlotCount(ast);
    size_t position = ast->variableDefinition.position;
    size_t count = position + slots;

    if (count > (size_t)UINT16_MAX + 1) {
        globalIndexOverflowError();
    }

    compiler->module->globalCount = count;
    compiler->localPositionOffset = -(int)count;
}

static void compileTopLevelStatements(Compiler* compiler, Vector* nodes)
{
    size_t count = countVector(nodes);

    while (compiler->statementIndex < count) {
        ASTNode* ast = nodes->data[compiler->statementIndex++];

        compileStatement(compiler, ast, true);
        finishTopLevelStatement(compiler, ast);
    }
}

static void compileReplStatement(Compiler* compiler, ASTNode* ast, bool isLast)
{
    bool display = isLast && isExpressionStatement(ast) && getTypeId(ast) != TOKEN_VOID;

    if (display && getTypeId(ast) == TOKEN_UNKNOWN) {
        unresolvedReplExpressionTypeError();
    }

    Operand value = compileStatement(compiler, ast, !display);

    if (!display) {
        return;
    }

    TokenType argumentType = getTypeId(ast);

    Builtin* builtin = resolveBuiltin("print", &argumentType, 1);
    uint16_t position = getBuiltinFunctionPosition(compiler, builtin);
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
        finishTopLevelStatement(compiler, ast);
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
    compiler->peepholeBarrier = 0;
    compiler->registerCount = 0;
    compiler->frameBaseCount = 0;
    compiler->localPositionOffset = 0;
    compiler->repl = false;
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

    compiler->repl = compileStatements == compileReplStatements;
    size_t start = countVector(&compiler->ast->compound.statements);

    if (!parse(&compiler->parser, source)) {
        return false;
    }

    analyze(&compiler->analyzer, start);
    optimize(compiler->ast, start);

    if (compileStatements == compileTopLevelStatements) {
        removeDiscardedReads(&compiler->ast->compound.statements, start);
        eliminateDeadFunctions(&compiler->ast->compound.statements);
    }

    clearCodeObject(currentCodeObject(compiler));
    compiler->peepholeBarrier = 0;
    compileStatements(compiler, &compiler->ast->compound.statements);
    emitHlt(compiler);

    CodeObject* code = currentCodeObject(compiler);

    optimizeBytecode(compiler->module, code, compiler->registerCount);

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
