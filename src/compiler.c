#include "compiler.h"
#include "ast.h"
#include "opcode.h"
#include "parser.h"
#include "scope.h"
#include "vector.h"
#include <stdlib.h>
#include <stdio.h>

static void expression();

typedef struct Compiler
{
    Chunk* chunk;
} Compiler;

Compiler compiler;

void initCompiler(Chunk* chunk)
{
    compiler.chunk = chunk;
}

static void emit16(int16_t n)
{
    writeChunk(compiler.chunk, (n >> 8) & 0xFF);
    writeChunk(compiler.chunk, n & 0xFF);
}

static void emit8(uint8_t n)
{
    writeChunk(compiler.chunk, n);
}

static void op_hlt()
{
    emit8(OP_HLT);
    printf("hlt\n");
}

static void op_syscall()
{
    emit8(OP_SYSCALL);
    printf("syscall\n");
}

static void op_ldc(uint8_t imm)
{
    emit8(OP_LDC);
    emit8(imm);
    printf("ldc\t%d\n", imm);
}

static void op_ldl_0()
{
    emit8(OP_LDL_0);
    printf("ldl_0\n");
}

static void op_ldl_1()
{
    emit8(OP_LDL_1);
    printf("ldl_1\n");
}

static void op_ldl_2()
{
    emit8(OP_LDL_2);
    printf("ldl_2\n");
}

static void op_ldl(int8_t imm)
{
    if (imm == 0) return op_ldl_0();
    if (imm == 1) return op_ldl_1();
    if (imm == 2) return op_ldl_2();

    emit8(OP_LDL);
    emit8(imm);
    printf("ldl\t%d\n", imm);
}

static void op_stl_0()
{
    emit8(OP_STL_0);
    printf("stl_0\n");
}

static void op_stl_1()
{
    emit8(OP_STL_1);
    printf("stl_1\n");
}

static void op_stl_2()
{
    emit8(OP_STL_2);
    printf("stl_2\n");
}

static void op_stl(int8_t imm)
{
    if (imm == 0) return op_stl_0();
    if (imm == 1) return op_stl_1();
    if (imm == 2) return op_stl_2();

    emit8(OP_STL);
    emit8(imm);
    printf("stl\t%d\n", imm);
}

static void op_push_0()
{
    emit8(OP_PUSH_0);
    printf("push_0\n");
}

static void op_push_1()
{
    emit8(OP_PUSH_1);
    printf("push_1\n");
}

static void op_push(int8_t imm)
{
    if (imm == 0) return op_push_0();
    if (imm == 1) return op_push_1();

    emit8(OP_PUSH);
    emit8(imm);
    printf("push\t%d\n", imm);
}

static void op_pop()
{
    emit8(OP_POP);
    printf("pop\n");
}

static void op_add()
{
    emit8(OP_ADD);
    printf("add\n");
}

static void op_sub()
{
    emit8(OP_SUB);
    printf("sub\n");
}

static void op_mul()
{
    emit8(OP_MUL);
    printf("mul\n");
}

static void op_div()
{
    emit8(OP_DIV);
    printf("div\n");
}

static void op_rem()
{
    emit8(OP_REM);
    printf("rem\n");
}

static void op_pow()
{
    emit8(OP_POW);
    printf("pow\n");
}

static void op_band()
{
    emit8(OP_BAND);
    printf("band\n");
}

static void op_bor()
{
    emit8(OP_BOR);
    printf("bor\n");
}

static void op_bxor()
{
    emit8(OP_BXOR);
    printf("bxor\n");
}

static void op_bnot()
{
    emit8(OP_BNOT);
    printf("bnot\n");
}

static void op_lsl()
{
    emit8(OP_LSL);
    printf("lsl\n");
}

static void op_lsr()
{
    emit8(OP_LSR);
    printf("lsr\n");
}

static void op_asr()
{
    emit8(OP_ASR);
    printf("asr\n");
}

static void op_abs()
{
    emit8(OP_ABS);
    printf("abs\n");
}

static void op_neg()
{
    emit8(OP_NEG);
    printf("neg\n");
}

static void op_not()
{
    emit8(OP_NOT);
    printf("not\n");
}

static void op_inc()
{
    emit8(OP_INC);
    printf("inc\n");
}

static void op_dec()
{
    emit8(OP_DEC);
    printf("dec\n");
}

static void op_beq(uint16_t imm)
{
    emit8(OP_BEQ);
    emit16(imm);
    printf("beq\t%d\n", imm);
}

static void op_blt(uint16_t imm)
{
    emit8(OP_BLT);
    emit16(imm);
    printf("blt\t%d\n", imm);
}

static void op_ble(uint16_t imm)
{
    emit8(OP_BLE);
    emit16(imm);
    printf("ble\t%d\n", imm);
}

static void op_jmp(uint16_t imm)
{
    emit8(OP_JMP);
    emit16(imm);
    printf("jmp\t%d\n", imm);
}

static void op_jsr(uint16_t imm)
{
    emit8(OP_JSR);
    emit16(imm);
    printf("jsr\t%d\n", imm);
}

static void op_ret()
{
    emit8(OP_RET);
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
    for (size_t i = 0; i < countVector(statements); i++) {
        AST* ast = vectorGet(statements, i);

        switch (ast->type) {
            case AST_ASSIGNMENT:
                assignment(ast);
                break;
            case AST_VARIABLE_DEFINITION:
                varDef(ast);
                break;
            default:
                expression(ast);
        }
    }
}

void compile(char* source)
{
    AST* ast = parse(source);
    statements(&ast->statements);
    op_hlt();
    freeAST(ast);
}
