#include "compiler.h"
#include "ast.h"
#include "bytecode.h"
#include "parser.h"
#include "reference.h"
#include "scope.h"

static void expression();
static void references();
static AST* statements(AST* ast);

static Chunk* currentChunk;
static Scope* currentScope;
static ReferenceArray functions;

static void write16(int16_t n)
{
    pushByte(currentChunk, (n >> 8) & 0xFF);
    pushByte(currentChunk, n & 0xFF);
}

static void write8(uint8_t n)
{
    pushByte(currentChunk, n);
}

static void patch16(size_t position, int16_t n)
{
    setByteAt(currentChunk, position, (n >> 8) & 0xFF);
    setByteAt(currentChunk, position + 1, n & 0xFF);
}

static void patch8(size_t position, int8_t n)
{
    setByteAt(currentChunk, position, n);
}

static void createFunctionReference(AST* ast, size_t position)
{
    Reference ref = { ast, position };
    pushReferenceArray(&functions, ref);
}

static Reference getFunctionReference(StringObject* id)
{
    size_t count = countReferenceArray(&functions);

    for (int i = 0; i < count; i++) {
        Reference ref = getReferenceAt(&functions, i);

        if (compareString(id, ref.ast->funcDef.id)) {
            return ref;
        }
    }

    Reference ref = { NULL };

    return ref;
}

static int getLocalPosition(AST* ast)
{
    if (ast->type == AST_PARAMETER) {
        return -(ast->param.position + 4);
    }
    
    return ast->varDef.position;
}

static void op_hlt()
{
    write8(OP_HLT);
}

static void op_syscall()
{
    write8(OP_SYSCALL);
}

static void op_ldc(uint8_t imm)
{
    write8(OP_LDC);
    write8(imm);
}

static void op_ldl(int8_t imm)
{
    if (imm == 0) return write8(OP_LDL_0);
    if (imm == 1) return write8(OP_LDL_1);
    if (imm == 2) return write8(OP_LDL_2);

    write8(OP_LDL);
    write8(imm);
}

static void op_stl(int8_t imm)
{
    if (imm == 0) return write8(OP_STL_0);
    if (imm == 1) return write8(OP_STL_1);
    if (imm == 2) return write8(OP_STL_2);

    write8(OP_STL);
    write8(imm);
}

static void op_push(int8_t imm)
{
    if (imm == 0) return write8(OP_PUSH_0);
    if (imm == 1) return write8(OP_PUSH_1);
    if (imm == 2) return write8(OP_PUSH_2);

    write8(OP_PUSH);
    write8(imm);
}

static void op_pop()
{
    write8(OP_POP);
}

static void op_inc(uint8_t imm)
{
    write8(OP_INC);
    write8(imm);
}

static void op_dec(uint8_t imm)
{
    write8(OP_DEC);
    write8(imm);
}

static void op_add()
{
    write8(OP_ADD);
}

static void op_sub()
{
    write8(OP_SUB);
}

static void op_mul()
{
    write8(OP_MUL);
}

static void op_div()
{
    write8(OP_DIV);
}

static void op_rem()
{
    write8(OP_REM);
}

static void op_pow()
{
    write8(OP_POW);
}

static void op_band()
{
    write8(OP_BAND);
}

static void op_bor()
{
    write8(OP_BOR);
}

static void op_bxor()
{
    write8(OP_BXOR);
}

static void op_bnot()
{
    write8(OP_BNOT);
}

static void op_lsl()
{
    write8(OP_LSL);
}

static void op_lsr()
{
    write8(OP_LSR);
}

static void op_asr()
{
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
    write8(OP_RET);
}

static void op_retv()
{
    write8(OP_RETV);
}

static void number(AST* ast)
{
    op_push(ast->intVal);
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
}

static void preIncrement(AST* ast)
{
    AST* expr = ast->postfix.expr;
    AST* symbol = getLocalSymbol(expr->var.scope, expr->var.id);
    int position = getLocalPosition(symbol);

    op_inc(position);
    expression(expr);
}

static void preDecrement(AST* ast)
{
    AST* expr = ast->postfix.expr;
    AST* symbol = getLocalSymbol(expr->var.scope, expr->var.id);
    int position = getLocalPosition(symbol);

    op_dec(position);
    expression(expr);
}

static void postIncrement(AST* ast)
{
    AST* expr = ast->postfix.expr;
    AST* symbol = getLocalSymbol(expr->var.scope, expr->var.id);
    int position = getLocalPosition(symbol);

    expression(expr);
    op_inc(position);
}

static void postDecrement(AST* ast)
{
    AST* expr = ast->postfix.expr;
    AST* symbol = getLocalSymbol(expr->var.scope, expr->var.id);
    int position = getLocalPosition(symbol);

    expression(expr);
    op_dec(position);
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
    AST *symbol = getLocalSymbol(ast->var.scope, ast->var.id);
    int position = getLocalPosition(symbol);

    op_ldl(position);
}

static void additionAssignment(AST* ast)
{
    AST* symbol = getLocalSymbol(ast->assignment.scope, ast->assignment.id);
    int position = getLocalPosition(symbol);

    op_ldl(position);
    expression(ast->assignment.expr);
    op_add();
    op_stl(position);
}

static void subtractionAssignment(AST* ast)
{
    AST* symbol = getLocalSymbol(ast->assignment.scope, ast->assignment.id);
    int position = getLocalPosition(symbol);

    op_ldl(position);
    expression(ast->assignment.expr);
    op_sub();
    op_stl(position);
}

static void muliplicationAssignment(AST* ast)
{
    AST* symbol = getLocalSymbol(ast->assignment.scope, ast->assignment.id);
    int position = getLocalPosition(symbol);

    op_ldl(position);
    expression(ast->assignment.expr);
    op_mul();
    op_stl(position);
}

static void divisionAssignment(AST* ast)
{
    AST* symbol = getLocalSymbol(ast->assignment.scope, ast->assignment.id);
    int position = getLocalPosition(symbol);

    op_ldl(position);
    expression(ast->assignment.expr);
    op_div();
    op_stl(position);
}

static void remainderAssignment(AST* ast)
{
    AST* symbol = getLocalSymbol(ast->assignment.scope, ast->assignment.id);
    int position = getLocalPosition(symbol);

    op_ldl(position);
    expression(ast->assignment.expr);
    op_rem();
    op_stl(position);
}

static void exponentiationAssignment(AST* ast)
{
    AST* symbol = getLocalSymbol(ast->assignment.scope, ast->assignment.id);
    int position = getLocalPosition(symbol);

    op_ldl(position);
    expression(ast->assignment.expr);
    op_pow();
    op_stl(position);
}

static void simpleAssignment(AST* ast)
{
    AST* symbol = getLocalSymbol(ast->assignment.scope, ast->assignment.id);
    int position = getLocalPosition(symbol);

    expression(ast->assignment.expr);
    op_stl(position);
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

static void arguments(Vector* args)
{
    int count = countVector(args);

    while (count--) {
        AST* arg = getVectorAt(args, count);
        expression(arg);
    }
}

static void functionCall(AST* ast)
{
    arguments(&ast->funcCall.args);

    size_t position = countChunk(currentChunk);
    Reference ref = { ast, position };
    pushReferenceArray(&currentScope->references, ref);
    op_call(0);
}

static void systemCall(AST* ast)
{
    arguments(&ast->syscall.args);
    op_push(ast->syscall.opcode);
    op_syscall();
}

static void functionBody(AST* ast)
{
    AST* last = statements(ast);

    if (last == NULL || last->type != AST_RETURN) {
        op_ret();
    }

    references();
}

static size_t functionDefinition(AST* ast)
{
    Reference ref = getFunctionReference(ast->funcDef.id);

    if (ref.ast) {
        return ref.position;
    }

    AST* body = ast->funcDef.body;
    size_t localCount = body->compound.scope->localCount;
    size_t paramCount = countVector(&ast->funcDef.params);
    size_t position = countChunk(currentChunk);
    Function func = { paramCount, localCount, position };
    size_t functionsIndex = countFunctionArray(&currentChunk->functions);
    
    pushFunctionArray(&currentChunk->functions, func);
    functionBody(body);
    createFunctionReference(ast, functionsIndex);
    currentScope = ast->funcDef.scope;

    return functionsIndex;
}

static void ret(AST* ast)
{
    if (ast->expr->type == AST_NONE) {
        return op_ret();
    }

    expression(ast->expr);
    op_retv();
}

static void variableDefinition(AST* ast)
{
    if (!ast->varDef.expr) {
        return op_push(0);
    }

    int position = getLocalPosition(ast);

    expression(ast->varDef.expr);
    op_stl(position);
}

static void references()
{
    size_t count = countReferenceArray(&currentScope->references);

    for (int i = 0; i < count; i++) {
        Reference ref = getReferenceAt(&currentScope->references, i);
        AST* symbol = getSymbol(ref.ast->funcCall.scope, ref.ast->funcCall.id);
        size_t position = functionDefinition(symbol);

        patch16(ref.position + 1, position);
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
    currentScope = ast->compound.scope;

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
        }
    }

    return statement;
}

static void topLevelStatements(AST* ast)
{
    statements(ast);
    op_hlt();
    references();
}

void compile(char* source, Chunk* chunk)
{
    AST* ast = parse(source);
    currentChunk = chunk;

    initReferenceArray(&functions);
    topLevelStatements(ast);
    freeReferenceArray(&functions);
    freeAST(ast);
}
