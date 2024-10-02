#include "compiler.h"
#include "ast.h"
#include "opcode.h"
#include "parser.h"
#include "reference.h"
#include "scope.h"
#include "vector.h"
#include <stdlib.h>
#include <stdio.h>

static void expression();
static void statements();

static Chunk* currentChunk;
static Vector references;

static void write16(int16_t n)
{
    writeChunk(currentChunk, (n >> 8) & 0xFF);
    writeChunk(currentChunk, n & 0xFF);
}

static void write8(uint8_t n)
{
    writeChunk(currentChunk, n);
}

static void patch16(size_t position, int16_t n)
{
    patchChunk(currentChunk, position, (n >> 8) & 0xFF);
    patchChunk(currentChunk, position + 1, n & 0xFF);
}

static void patch8(size_t position, int8_t n)
{
    patchChunk(currentChunk, position, n);
}

static void op_hlt()
{
    write8(OP_HLT);
    printf("hlt\n");
}

static void op_syscall()
{
    write8(OP_SYSCALL);
    printf("syscall\n");
}

static void op_ldc(uint8_t imm)
{
    write8(OP_LDC);
    write8(imm);
    printf("ldc\t%d\n", imm);
}

static void op_ldl_0()
{
    write8(OP_LDL_0);
    printf("ldl_0\n");
}

static void op_ldl_1()
{
    write8(OP_LDL_1);
    printf("ldl_1\n");
}

static void op_ldl_2()
{
    write8(OP_LDL_2);
    printf("ldl_2\n");
}

static void op_ldl(int8_t imm)
{
    if (imm == 0) return op_ldl_0();
    if (imm == 1) return op_ldl_1();
    if (imm == 2) return op_ldl_2();

    write8(OP_LDL);
    write8(imm);
    printf("ldl\t%d\n", imm);
}

static void op_stl_0()
{
    write8(OP_STL_0);
    printf("stl_0\n");
}

static void op_stl_1()
{
    write8(OP_STL_1);
    printf("stl_1\n");
}

static void op_stl_2()
{
    write8(OP_STL_2);
    printf("stl_2\n");
}

static void op_stl(int8_t imm)
{
    if (imm == 0) return op_stl_0();
    if (imm == 1) return op_stl_1();
    if (imm == 2) return op_stl_2();

    write8(OP_STL);
    write8(imm);
    printf("stl\t%d\n", imm);
}

static void op_push_0()
{
    write8(OP_PUSH_0);
    printf("push_0\n");
}

static void op_push_1()
{
    write8(OP_PUSH_1);
    printf("push_1\n");
}

static void op_push(int8_t imm)
{
    if (imm == 0) return op_push_0();
    if (imm == 1) return op_push_1();

    write8(OP_PUSH);
    write8(imm);
    printf("push\t%d\n", imm);
}

static void op_pop()
{
    write8(OP_POP);
    printf("pop\n");
}

static void op_add()
{
    write8(OP_ADD);
    printf("add\n");
}

static void op_sub()
{
    write8(OP_SUB);
    printf("sub\n");
}

static void op_mul()
{
    write8(OP_MUL);
    printf("mul\n");
}

static void op_div()
{
    write8(OP_DIV);
    printf("div\n");
}

static void op_rem()
{
    write8(OP_REM);
    printf("rem\n");
}

static void op_pow()
{
    write8(OP_POW);
    printf("pow\n");
}

static void op_band()
{
    write8(OP_BAND);
    printf("band\n");
}

static void op_bor()
{
    write8(OP_BOR);
    printf("bor\n");
}

static void op_bxor()
{
    write8(OP_BXOR);
    printf("bxor\n");
}

static void op_bnot()
{
    write8(OP_BNOT);
    printf("bnot\n");
}

static void op_lsl()
{
    write8(OP_LSL);
    printf("lsl\n");
}

static void op_lsr()
{
    write8(OP_LSR);
    printf("lsr\n");
}

static void op_asr()
{
    write8(OP_ASR);
    printf("asr\n");
}

static void op_abs()
{
    write8(OP_ABS);
    printf("abs\n");
}

static void op_neg()
{
    write8(OP_NEG);
    printf("neg\n");
}

static void op_not()
{
    write8(OP_NOT);
    printf("not\n");
}

static void op_inc()
{
    write8(OP_INC);
    printf("inc\n");
}

static void op_dec()
{
    write8(OP_DEC);
    printf("dec\n");
}

static void op_beq(uint16_t imm)
{
    write8(OP_BEQ);
    write16(imm);
    printf("beq\t%d\n", imm);
}

static void op_blt(uint16_t imm)
{
    write8(OP_BLT);
    write16(imm);
    printf("blt\t%d\n", imm);
}

static void op_ble(uint16_t imm)
{
    write8(OP_BLE);
    write16(imm);
    printf("ble\t%d\n", imm);
}

static void op_jmp(uint16_t imm)
{
    write8(OP_JMP);
    write16(imm);
    printf("jmp\t%d\n", imm);
}

static void op_jsr(uint16_t imm)
{
    write8(OP_JSR);
    write16(imm);
    printf("jsr\t%d\n", imm);
}

static void op_ret()
{
    write8(OP_RET);
    printf("ret\n");
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
    op_neg();
}

static void increment(AST* ast)
{
    AST* expr = ast->postfix.expr;
    AST* symbol = getLocalSymbol(expr->var.scope, expr->var.id);

    op_inc();
}

static void decrement(AST* ast)
{
    AST* expr = ast->postfix.expr;
    AST* symbol = getLocalSymbol(expr->var.scope, expr->var.id);

    op_dec();
}

static void postfix(AST* ast)
{
    switch (ast->postfix.operator.type) {
        case T_INCREMENT:
            return increment(ast);
        case T_DECREMENT:
            return decrement(ast);
    }
}

static void prefix(AST* ast)
{
    switch (ast->postfix.operator.type) {
        case T_TILDE:
            return bitNot(ast);
        case T_INCREMENT:
            return increment(ast);
        case T_DECREMENT:
            return decrement(ast);
        case T_EXCLAMATION:
            return not(ast);
        case T_MINUS:
            return negate(ast);
    }
}

static void variable(AST* ast)
{
    AST *symbol = getLocalSymbol(ast->var.scope, ast->var.id);

    if (symbol->type == AST_PARAMETER) {
        return op_ldl(symbol->varDef.position - 2);
    }

    op_ldl(symbol->varDef.position);
}

static void additionAssignment(AST* ast)
{
    AST* symbol = getLocalSymbol(ast->assignment.scope, ast->assignment.id);

    op_ldl(symbol->varDef.position);
    expression(ast->assignment.expr);
    op_add();
    op_stl(symbol->varDef.position);
}

static void subtractionAssignment(AST* ast)
{
    AST* symbol = getLocalSymbol(ast->assignment.scope, ast->assignment.id);

    op_ldl(symbol->varDef.position);
    expression(ast->assignment.expr);
    op_sub();
    op_stl(symbol->varDef.position);
}

static void muliplicationAssignment(AST* ast)
{
    AST* symbol = getLocalSymbol(ast->assignment.scope, ast->assignment.id);

    op_ldl(symbol->varDef.position);
    expression(ast->assignment.expr);
    op_mul();
    op_stl(symbol->varDef.position);
}

static void divisionAssignment(AST* ast)
{
    AST* symbol = getLocalSymbol(ast->assignment.scope, ast->assignment.id);

    op_ldl(symbol->varDef.position);
    expression(ast->assignment.expr);
    op_div();
    op_stl(symbol->varDef.position);
}

static void remainderAssignment(AST* ast)
{
    AST* symbol = getLocalSymbol(ast->assignment.scope, ast->assignment.id);

    op_ldl(symbol->varDef.position);
    expression(ast->assignment.expr);
    op_rem();
    op_stl(symbol->varDef.position);
}

static void exponentiationAssignment(AST* ast)
{
    AST* symbol = getLocalSymbol(ast->assignment.scope, ast->assignment.id);

    op_ldl(symbol->varDef.position);
    expression(ast->assignment.expr);
    op_pow();
    op_stl(symbol->varDef.position);
}

static void simpleAssignment(AST* ast)
{
    AST* symbol = getLocalSymbol(ast->assignment.scope, ast->assignment.id);

    expression(ast->assignment.expr);
    op_stl(symbol->varDef.position);
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

static void funcCall(AST* ast)
{
    int count = countVector(&ast->funcCall.args);

    while (count--) {
        AST* arg = vectorGet(&ast->funcCall.args, count);
        expression(arg);
    }

    Reference* ref = createReference(ast, countChunk(currentChunk));
    pushVector(&references, ref);

    op_jsr(0);
}

static int16_t funcDef(AST* ast)
{
    int16_t position = countChunk(currentChunk);
    statements(&ast->funcDef.body->statements);

    return position;
}

static void ret(AST* ast)
{
    expression(ast->expr);
    op_ret();
}

static void varDef(AST* ast)
{
    if (!ast->varDef.expr) {
        return op_push_0();
    }

    expression(ast->varDef.expr);
    op_stl(ast->varDef.position);
}

static void expression(AST* ast)
{
    switch (ast->type) {
        case AST_VARIABLE:
            return variable(ast);
        case AST_BINARY:
            return binary(ast);
        case AST_FUNCTION_CALL:
            return funcCall(ast);
        case AST_INTEGER:
            return number(ast);
        case AST_POSTFIX:
            return postfix(ast);
        case AST_PREFIX:
            return prefix(ast);
    }
}

static void statements(Vector* statements)
{
    size_t count = countVector(statements);

    for (size_t i = 0; i < count; i++) {
        AST* ast = vectorGet(statements, i);

        switch (ast->type) {
            case AST_ASSIGNMENT:
                assignment(ast);
                break;
            case AST_RETURN:
                ret(ast);
                break;
            case AST_VARIABLE_DEFINITION:
            case AST_PARAMETER:
                varDef(ast);
                break;
            default:
                expression(ast);
        }
    }
}

static void functions()
{
    size_t count = countVector(&references);

    for (int i = 0; i < count; i++) {
        Reference* ref = vectorGet(&references, i);
        AST* symbol = getSymbol(ref->ast->funcCall.scope, ref->ast->funcCall.id);
        int16_t offset = funcDef(symbol) - ref->position - 3;

        patch16(ref->position + 1, offset);
        freeReference(ref);
    }
}

void compile(char* source, Chunk* chunk)
{
    AST* ast = parse(source);
    currentChunk = chunk;
    initVector(&references);
    statements(&ast->statements);
    op_hlt();
    functions();
    freeVector(&references);
    freeAST(ast);
}
