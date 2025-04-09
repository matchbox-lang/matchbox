#include "compiler.h"
#include "ast.h"
#include "bytecode.h"
#include "functionobject.h"
#include "parser.h"
#include "scope.h"
#include "util.h"

static AST* statements(AST* ast);
static void expression();
static Vector functionReferences;
static FunctionObject* currentFunction;
static ModuleObject* currentModule;
static int stackCount = 0;

static CodeObject* currentCodeObject()
{
    return &currentFunction->code;
}

static void incStackCount()
{
    if (++stackCount > currentFunction->maxStackCount) {
        currentFunction->maxStackCount = stackCount;
    }
}

static void decStackCount()
{
    stackCount--;
}

static void patch8(size_t position, int8_t n)
{
    setByteAt(currentCodeObject(), position, n);
}

static void patch16(size_t position, int16_t n)
{
    setByteAt(currentCodeObject(), position, (n >> 8) & 0xFF);
    setByteAt(currentCodeObject(), position + 1, n & 0xFF);
}

static void write8(uint8_t n)
{
    pushByte(currentCodeObject(), n);
}

static void write16(int16_t n)
{
    pushByte(currentCodeObject(), (n >> 8) & 0xFF);
    pushByte(currentCodeObject(), n & 0xFF);
}

static void op_hlt()
{
    write8(OP_HLT);
}

static void op_syscall(uint8_t imm)
{
    write8(OP_SYSCALL);
    write8(imm);
}

static void op_ldc(uint8_t imm)
{
    incStackCount();
    write8(OP_LDC);
    write8(imm);
}

static void op_reg()
{
    decStackCount();
    write8(OP_REG);
}

static void op_ldg(uint8_t imm)
{
    incStackCount();
    write8(OP_LDG);
    write8(imm);
}

static void op_stg(uint8_t imm)
{
    decStackCount();
    write8(OP_STG);
    write8(imm);
}

static void op_ldl(int8_t imm)
{
    incStackCount();

    switch (imm) {
        case 0: return write8(OP_LDL_0);
        case 1: return write8(OP_LDL_1);
        case 2: return write8(OP_LDL_2);
        case 3: return write8(OP_LDL_3);
    }

    write8(OP_LDL);
    write8(imm);
}

static void op_stl(int8_t imm)
{
    decStackCount();

    switch (imm) {
        case 0: return write8(OP_STL_0);
        case 1: return write8(OP_STL_1);
        case 2: return write8(OP_STL_2);
        case 3: return write8(OP_STL_3);
    }

    write8(OP_STL);
    write8(imm);
}

static void op_pushb(int8_t imm)
{
    incStackCount();

    switch (imm) {
        case 0: return write8(OP_PUSH_0);
        case 1: return write8(OP_PUSH_1);
        case 2: return write8(OP_PUSH_2);
        case 3: return write8(OP_PUSH_3);
    }

    write8(OP_PUSHB);
    write8(imm);
}

static void op_pushh(int16_t imm)
{
    incStackCount();
    write8(OP_PUSHH);
    write16(imm);
}

static void op_pop()
{
    decStackCount();
    write8(OP_POP);
}

static void op_dup()
{
    incStackCount();
    write8(OP_DUP);
}

static void op_inc()
{
    write8(OP_INC);
}

static void op_dec()
{
    write8(OP_DEC);
}

static void op_add()
{
    decStackCount();
    write8(OP_ADD);
}

static void op_sub()
{
    decStackCount();
    write8(OP_SUB);
}

static void op_mul()
{
    decStackCount();
    write8(OP_MUL);
}

static void op_div()
{
    decStackCount();
    write8(OP_DIV);
}

static void op_rem()
{
    decStackCount();
    write8(OP_REM);
}

static void op_pow()
{
    decStackCount();
    write8(OP_POW);
}

static void op_band()
{
    decStackCount();
    write8(OP_BAND);
}

static void op_bor()
{
    decStackCount();
    write8(OP_BOR);
}

static void op_bxor()
{
    decStackCount();
    write8(OP_BXOR);
}

static void op_bnot()
{
    decStackCount();
    write8(OP_BNOT);
}

static void op_lsl()
{
    decStackCount();
    write8(OP_LSL);
}

static void op_lsr()
{
    decStackCount();
    write8(OP_LSR);
}

static void op_asr()
{
    decStackCount();
    write8(OP_ASR);
}

static void op_neg()
{
    write8(OP_NEG);
}

static void op_not()
{
    write8(OP_NOT);
}

static void op_beq(uint16_t imm)
{
    write8(OP_BEQ);
    write16(imm);
}

static void op_blt(uint16_t imm)
{
    write8(OP_BLT);
    write16(imm);
}

static void op_ble(uint16_t imm)
{
    write8(OP_BLE);
    write16(imm);
}

static void op_jmp(uint16_t imm)
{
    write8(OP_JMP);
    write16(imm);
}

static void op_call(uint16_t imm)
{
    write8(OP_CALL);
    write16(imm);
}

static void op_ret()
{
    incStackCount();
    write8(OP_RET);
}

static void op_retv()
{
    write8(OP_RETV);
}

static size_t makeConstant(Value value)
{
    return pushValue(&currentModule->constants, value) - 1;
}

static int getLocalPosition(AST* ast)
{
    if (isParameter(ast)) {
        return ast->param.position;
    }
    
    return ast->varDef.position;
}

static void loadGlobalVariable(AST* ast)
{
    int position = getLocalPosition(ast);

    op_ldg(position);
}

static void loadLocalVariable(AST* ast)
{
    int position = getLocalPosition(ast);

    op_ldl(position);
}

static void loadVariable(AST* ast)
{
    if (isTopLevel(ast->varDef.scope)) {
        loadGlobalVariable(ast);
    } else {
        loadLocalVariable(ast);
    }
}

static void storeGlobalVariable(AST* ast)
{
    int position = getLocalPosition(ast);

    op_stg(position);
}

static void storeLocalVariable(AST* ast)
{
    int position = getLocalPosition(ast);

    op_stl(position);
}

static void storeVariable(AST* ast)
{
    if (isTopLevel(ast->varDef.scope)) {
        storeGlobalVariable(ast);
    } else {
        storeLocalVariable(ast);
    }
}

static void number(AST* ast)
{
    if (isLargerThan16BitSigned(ast->intValue)) {
        size_t position = makeConstant(INT_VALUE(ast->intValue));
        op_ldc(position);
        pushVectorItem(&functionReferences, ast);
    } else if (isLargerThan8BitSigned(ast->intValue)) {
        op_pushh(ast->intValue);
    } else {
        op_pushb(ast->intValue);
    }
}

static void binary(AST* ast)
{
    expression(ast->binary.leftExpr);
    expression(ast->binary.rightExpr);

    switch (ast->binary.operator.type) {
        case T_PLUS:        return op_add();
        case T_MINUS:       return op_sub();
        case T_STAR:        return op_mul();
        case T_SLASH:       return op_div();
        case T_FLOOR:       return op_div();
        case T_PERCENT:     return op_rem();
        case T_POWER:       return op_pow();
        case T_AMPERSAND:   return op_band();
        case T_PIPE:        return op_bor();
        case T_CIRCUMFLEX:  return op_bxor();
        case T_LSHIFT:      return op_lsl();
        case T_RSHIFT:      return op_lsr();
    }
}

static void bitNot(AST* ast)
{
    expression(ast->prefix.expr);
    op_bnot();
}

static void not(AST* ast)
{
    expression(ast->prefix.expr);
    op_not();
}

static void negate(AST* ast)
{
    expression(ast->prefix.expr);
    op_neg();
}

static void preDecrement(AST* ast)
{
    AST* expr = ast->postfix.expr;

    expression(expr);
    op_dec();
    op_dup();
    storeVariable(expr->var.symbol);
}

static void preIncrement(AST* ast)
{
    AST* expr = ast->postfix.expr;

    expression(expr);
    op_inc();
    op_dup();
    storeVariable(expr->var.symbol);
}

static void postDecrement(AST* ast)
{
    AST* expr = ast->postfix.expr;
    int position = getLocalPosition(expr->var.symbol);

    expression(expr);
    op_dup();
    op_dec();
    storeVariable(expr->var.symbol);
}

static void postIncrement(AST* ast)
{
    AST* expr = ast->postfix.expr;
    int position = getLocalPosition(expr->var.symbol);

    expression(expr);
    op_dup();
    op_inc();
    storeVariable(expr->var.symbol);
}

static void postfix(AST* ast)
{
    switch (ast->postfix.operator.type) {
        case T_INCREMENT:
            return postIncrement(ast);
        case T_DECREMENT:
            return postDecrement(ast);
    }
}

static void prefix(AST* ast)
{
    switch (ast->postfix.operator.type) {
        case T_TILDE:
            return bitNot(ast);
        case T_INCREMENT:
            return preIncrement(ast);
        case T_DECREMENT:
            return preDecrement(ast);
        case T_EXCLAMATION:
            return not(ast);
        case T_MINUS:
            return negate(ast);
    }
}

static void variable(AST* ast)
{
    loadVariable(ast->var.symbol);
}

static void additionAssignment(AST* ast)
{
    loadVariable(ast->assignment.symbol);
    expression(ast->assignment.expr);
    op_add();
    storeVariable(ast->assignment.symbol);
}

static void subtractionAssignment(AST* ast)
{
    loadVariable(ast->assignment.symbol);
    expression(ast->assignment.expr);
    op_sub();
    storeVariable(ast->assignment.symbol);
}

static void muliplicationAssignment(AST* ast)
{
    loadVariable(ast->assignment.symbol);
    expression(ast->assignment.expr);
    op_mul();
    storeVariable(ast->assignment.symbol);
}

static void divisionAssignment(AST* ast)
{
    loadVariable(ast->assignment.symbol);
    expression(ast->assignment.expr);
    op_div();
    storeVariable(ast->assignment.symbol);
}

static void remainderAssignment(AST* ast)
{
    loadVariable(ast->assignment.symbol);
    expression(ast->assignment.expr);
    op_rem();
    storeVariable(ast->assignment.symbol);
}

static void exponentiationAssignment(AST* ast)
{
    loadVariable(ast->assignment.symbol);
    expression(ast->assignment.expr);
    op_pow();
    storeVariable(ast->assignment.symbol);
}

static void simpleAssignment(AST* ast)
{
    expression(ast->assignment.expr);
    storeVariable(ast->assignment.symbol);
}

static void assignment(AST* ast)
{
    switch (ast->assignment.operator.type) {
        case T_PLUS_EQUAL:      return additionAssignment(ast);
        case T_MINUS_EQUAL:     return subtractionAssignment(ast);
        case T_STAR_EQUAL:      return muliplicationAssignment(ast);
        case T_SLASH_EQUAL:     return divisionAssignment(ast);
        case T_FLOOR_EQUAL:     return divisionAssignment(ast);
        case T_PERCENT_EQUAL:   return remainderAssignment(ast);
        case T_POWER_EQUAL:     return exponentiationAssignment(ast);
        case T_EQUAL:           return simpleAssignment(ast);
    }
}

static size_t arguments(Vector* args)
{
    size_t count = countVector(args);

    for (int i = 0; i < count; i++) {
        AST* arg = getVectorAt(args, i);
        expression(arg);
    }

    return count;
}

static int getFunctionPosition(AST* ast)
{
    size_t functionCount = countVector(&functionReferences);
    
    for (int i = 0; i < functionCount; i++) {
        if (ast == functionReferences.data[i]) {
            return i;
        }
    }

    return -1;
}

static void functionCall(AST* ast)
{
    uint16_t position = getFunctionPosition(ast->funcCall.symbol);
    arguments(&ast->funcCall.args);
    op_call(position);
}

static void systemCall(AST* ast)
{
    arguments(&ast->syscall.args);
    op_syscall(ast->syscall.opcode);
}

static void functionBody(AST* ast)
{
    AST* last = statements(ast);

    if (!last || last->type != AST_RETURN) {
        op_ret();
    }
}

static void functionDefinition(AST* ast)
{
    AST* body = ast->funcDef.body;
    FunctionObject* previousFunction = currentFunction;
    FunctionObject* function = createFunctionObject();
    function->paramCount = countVector(&ast->funcDef.params);
    function->localCount = body->compound.scope->localCount;
    function->maxStackCount = function->localCount;
    
    stackCount = function->maxStackCount;
    currentFunction = function;
    makeConstant(POINTER_VALUE(function));
    pushVectorItem(&functionReferences, ast);
    functionBody(body);
    currentFunction = previousFunction;
}

static void ret(AST* ast)
{
    if (isNone(ast->expr)) {
        return op_ret();
    }

    expression(ast->expr);
    op_retv();
}

static void variableDefinition(AST* ast)
{
    if (isNone(ast->varDef.expr)) {
        return;
    }

    expression(ast->varDef.expr);

    if (isTopLevel(ast->varDef.scope)) {
        op_reg();
    }
}

static void expression(AST* ast)
{
    switch (ast->type) {
        case AST_VARIABLE:
            return variable(ast);
        case AST_BINARY:
            return binary(ast);
        case AST_FUNCTION_CALL:
            return functionCall(ast);
        case AST_INTEGER:
            return number(ast);
        case AST_POSTFIX:
            return postfix(ast);
        case AST_PREFIX:
            return prefix(ast);
        case AST_SYSCALL:
            return systemCall(ast);
    }
}

static AST* statements(AST* ast)
{
    size_t count = countVector(&ast->compound.statements);
    AST* statement = NULL;

    for (size_t i = 0; i < count; i++) {
        statement = getVectorAt(&ast->compound.statements, i);

        switch (statement->type) {
            case AST_ASSIGNMENT:
                assignment(statement);
                break;
            case AST_FUNCTION_CALL:
                functionCall(statement);
                op_pop();
                break;
            case AST_FUNCTION_DEFINITION:
                functionDefinition(statement);
                break;
            case AST_RETURN:
                ret(statement);
                break;
            case AST_SYSCALL:
                systemCall(statement);
                op_pop();
                break;
            case AST_VARIABLE_DEFINITION:
                variableDefinition(statement);
                break;
            default:
                expression(statement);
                op_pop();
        }
    }

    return statement;
}

static void topLevelStatements(AST* ast)
{
    statements(ast);
    op_hlt();
}

void compile(char* source, ModuleObject* module)
{
    AST* ast = parse(source);

    currentFunction = getValueAsPointer(&module->constants, 0);
    currentModule = module;

    initVector(&functionReferences);
    pushVectorItem(&functionReferences, NULL);
    topLevelStatements(ast);
    freeVector(&functionReferences);
    freeAST(ast);
}
