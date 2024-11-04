#include "token.h"

void printToken(Token* token)
{
    printf("Token\n");
    printf("{\n");
    printf("\ttype: %d\n", token->type);
    printf("\tvalue: ");
    printf("%.*s\n", token->length, token->chars);
    printf("\tline: %i:%i\n", token->line, token->column);
    printf("}\n");
}

bool isAssignmentToken(TokenType type)
{
    switch (type) {
        case T_EQUAL:
        case T_PLUS_EQUAL:
        case T_MINUS_EQUAL:
        case T_STAR_EQUAL:
        case T_SLASH_EQUAL:
        case T_FLOOR_EQUAL:
        case T_PERCENT_EQUAL:
        case T_POWER_EQUAL:
            return true;
    }

    return false;
}

bool isTypeToken(TokenType type)
{
    return type == T_INT;
}

bool isComparisonToken(TokenType type)
{
    switch (type) {
        case T_GREATER:
        case T_GREATER_EQUAL:
        case T_LESS:
        case T_LESS_EQUAL:
        case T_SPACESHIP:
            return true;
    }

    return false;
}

bool isEqualityToken(TokenType type)
{
    switch (type) {
        case T_EQUAL_EQUAL:
        case T_NOT_EQUAL:
            return true;
    }

    return false;
}

bool isBoolOperatorToken(TokenType type)
{
    return isComparisonToken(type) || isEqualityToken(type);
}

bool isShiftToken(TokenType type)
{
    switch (type) {
        case T_LSHIFT:
        case T_RSHIFT:
            return true;
    }

    return false;
}

bool isTermToken(TokenType type)
{
    switch (type) {
        case T_PLUS:
        case T_MINUS:
            return true;
    }

    return false;
}

bool isFactorToken(TokenType type)
{
    switch (type) {
        case T_STAR:
        case T_SLASH:
        case T_FLOOR:
        case T_PERCENT:
            return true;
    }

    return false;
}

bool isPrefixToken(TokenType type)
{
    switch (type) {
        case T_INCREMENT:
        case T_DECREMENT:
        case T_MINUS:
        case T_EXCLAMATION:
        case T_TILDE:
            return true;
    }

    return false;
}

bool isPostfixToken(TokenType type)
{
    switch (type) {
        case T_INCREMENT:
        case T_DECREMENT:
            return true;
    }

    return false;
}
