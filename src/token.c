#include "token.h"
#include <stdbool.h>
#include <stdio.h>

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
        case TOKEN_EQUAL:
        case TOKEN_PLUS_EQUAL:
        case TOKEN_MINUS_EQUAL:
        case TOKEN_STAR_EQUAL:
        case TOKEN_SLASH_EQUAL:
        case TOKEN_FLOOR_EQUAL:
        case TOKEN_PERCENT_EQUAL:
        case TOKEN_POWER_EQUAL:
            return true;
        default:
            return false;
    }
}

bool isTypeToken(TokenType type)
{
    return type == TOKEN_INT;
}

bool isComparisonToken(TokenType type)
{
    switch (type) {
        case TOKEN_GREATER:
        case TOKEN_GREATER_EQUAL:
        case TOKEN_LESS:
        case TOKEN_LESS_EQUAL:
        case TOKEN_SPACESHIP:
            return true;
        default:
            return false;
    }
}

bool isEqualityToken(TokenType type)
{
    switch (type) {
        case TOKEN_EQUAL_EQUAL:
        case TOKEN_EQUAL_EQUAL_EQUAL:
        case TOKEN_NOT_EQUAL:
        case TOKEN_NOT_EQUAL_EQUAL:
            return true;
        default:
            return false;
    }
}

bool isBoolOperatorToken(TokenType type)
{
    return isComparisonToken(type) || isEqualityToken(type);
}

bool isShiftToken(TokenType type)
{
    switch (type) {
        case TOKEN_LSHIFT:
        case TOKEN_RSHIFT:
            return true;
        default:
            return false;
    }
}

bool isTermToken(TokenType type)
{
    switch (type) {
        case TOKEN_PLUS:
        case TOKEN_MINUS:
            return true;
        default:
            return false;
    }
}

bool isFactorToken(TokenType type)
{
    switch (type) {
        case TOKEN_STAR:
        case TOKEN_SLASH:
        case TOKEN_FLOOR:
        case TOKEN_PERCENT:
            return true;
        default:
            return false;
    }
}

bool isPrefixToken(TokenType type)
{
    switch (type) {
        case TOKEN_EXCLAMATION:
        case TOKEN_MINUS:
        case TOKEN_TILDE:
            return true;
        default:
            return false;
    }
}
