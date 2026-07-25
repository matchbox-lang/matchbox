#include "token.h"
#include <stdbool.h>
#include <stdio.h>

void printTokenValue(Token token)
{
    if (token.type == TOKEN_EOF || token.length == 0) {
        fprintf(stderr, "end of input");
        
        return;
    }

    fprintf(stderr, "%.*s", token.length, token.chars);
}

const char* getTokenTypeName(TokenType type)
{
    switch (type) {
        case TOKEN_IF:                  return "if";
        case TOKEN_ELSE:                return "else";
        case TOKEN_IN:                  return "in";
        case TOKEN_IS:                  return "is";
        case TOKEN_SIZEOF:              return "sizeof";
        case TOKEN_ALIGNOF:             return "alignof";
        case TOKEN_TYPEOF:              return "typeof";
        case TOKEN_FOR:                 return "for";
        case TOKEN_WHILE:               return "while";
        case TOKEN_MATCH:               return "match";
        case TOKEN_BREAK:               return "break";
        case TOKEN_CONTINUE:            return "continue";
        case TOKEN_WHERE:               return "where";
        case TOKEN_RETURN:              return "return";
        case TOKEN_YIELD:               return "yield";
        case TOKEN_AS:                  return "as";
        case TOKEN_VAR:                 return "var";
        case TOKEN_LET:                 return "let";
        case TOKEN_STATIC:              return "static";
        case TOKEN_CONST:               return "const";
        case TOKEN_USE:                 return "use";
        case TOKEN_END:                 return "end";
        case TOKEN_CLASS:               return "class";
        case TOKEN_STRUCT:              return "struct";
        case TOKEN_PROTOCOL:            return "protocol";
        case TOKEN_ENUM:                return "enum";
        case TOKEN_GET:                 return "get";
        case TOKEN_SET:                 return "set";
        case TOKEN_FUNC:                return "func";
        case TOKEN_TYPE:                return "type";
        case TOKEN_INTERNAL:            return "internal";
        case TOKEN_PRIVATE:             return "private";
        case TOKEN_PUBLIC:              return "public";
        case TOKEN_SELF:                return "self";
        case TOKEN_SPAWN:               return "spawn";
        case TOKEN_AWAIT:               return "await";
        case TOKEN_DEFER:               return "defer";
        case TOKEN_TRY:                 return "try";
        case TOKEN_CATCH:               return "catch";
        case TOKEN_FINALLY:             return "finally";
        case TOKEN_THROW:               return "throw";
        case TOKEN_VOID:                return "void";
        case TOKEN_I8:                  return "i8";
        case TOKEN_I16:                 return "i16";
        case TOKEN_I32:                 return "i32";
        case TOKEN_I64:                 return "i64";
        case TOKEN_U8:                  return "u8";
        case TOKEN_U16:                 return "u16";
        case TOKEN_U32:                 return "u32";
        case TOKEN_U64:                 return "u64";
        case TOKEN_F32:                 return "f32";
        case TOKEN_F64:                 return "f64";
        case TOKEN_CHAR:                return "char";
        case TOKEN_STRING:              return "string";
        case TOKEN_BOOL:                return "bool";
        case TOKEN_TRUE:                return "true";
        case TOKEN_FALSE:               return "false";
        case TOKEN_PLUS:                return "+";
        case TOKEN_MINUS:               return "-";
        case TOKEN_STAR:                return "*";
        case TOKEN_SLASH:               return "/";
        case TOKEN_FLOOR:               return "//";
        case TOKEN_PERCENT:             return "%";
        case TOKEN_POWER:               return "**";
        case TOKEN_COALESCE:            return "??";
        case TOKEN_EQUAL:               return "=";
        case TOKEN_PLUS_EQUAL:          return "+=";
        case TOKEN_MINUS_EQUAL:         return "-=";
        case TOKEN_STAR_EQUAL:          return "*=";
        case TOKEN_SLASH_EQUAL:         return "/=";
        case TOKEN_FLOOR_EQUAL:         return "//=";
        case TOKEN_PERCENT_EQUAL:       return "%=";
        case TOKEN_POWER_EQUAL:         return "**=";
        case TOKEN_COALESCE_EQUAL:      return "??" "=";
        case TOKEN_EQUAL_EQUAL:         return "==";
        case TOKEN_NOT_EQUAL:           return "!=";
        case TOKEN_EQUAL_EQUAL_EQUAL:   return "===";
        case TOKEN_NOT_EQUAL_EQUAL:     return "!==";
        case TOKEN_EQUAL_TILDE:         return "=~";
        case TOKEN_NOT_TILDE:           return "!~";
        case TOKEN_GREATER:             return ">";
        case TOKEN_GREATER_EQUAL:       return ">=";
        case TOKEN_LESS:                return "<";
        case TOKEN_LESS_EQUAL:          return "<=";
        case TOKEN_SPACESHIP:           return "<=>";
        case TOKEN_AND:                 return "and";
        case TOKEN_OR:                  return "or";
        case TOKEN_NOT:                 return "not";
        case TOKEN_AMPERSAND:           return "&";
        case TOKEN_PIPE:                return "|";
        case TOKEN_CIRCUMFLEX:          return "^";
        case TOKEN_LSHIFT:              return "<<";
        case TOKEN_RSHIFT:              return ">>";
        case TOKEN_TILDE:               return "~";
        case TOKEN_QUESTION:            return "?";
        case TOKEN_TERNARY:             return "?:";
        case TOKEN_COLON:               return ":";
        case TOKEN_AND_EQUAL:           return "&=";
        case TOKEN_OR_EQUAL:            return "|=";
        case TOKEN_CIRCUMFLEX_EQUAL:    return "^=";
        case TOKEN_LSHIFT_EQUAL:        return "<<=";
        case TOKEN_RSHIFT_EQUAL:        return ">>=";
        case TOKEN_ARROW:               return "->";
        case TOKEN_SAFE_ACCESS:         return "?.";
        case TOKEN_PIPE_FORWARD:        return "|>";
        case TOKEN_PIPE_BACKWARD:       return "<|";
        case TOKEN_DOLLAR:              return "$";
        case TOKEN_RANGE:               return "..";
        case TOKEN_RANGE_FROM_END:      return "..^";
        case TOKEN_SPREAD:              return "...";
        case TOKEN_AT:                  return "@";
        case TOKEN_DOT:                 return ".";
        case TOKEN_LPAREN:              return "(";
        case TOKEN_RPAREN:              return ")";
        case TOKEN_LSQUARE:             return "[";
        case TOKEN_RSQUARE:             return "]";
        case TOKEN_LBRACE:              return "{";
        case TOKEN_RBRACE:              return "}";
        case TOKEN_SEMICOLON:           return ";";
        case TOKEN_COMMA:               return ",";
        case TOKEN_INTEGER_LITERAL:     return "integer literal";
        case TOKEN_F32_LITERAL:         return "f32 literal";
        case TOKEN_OCTAL_LITERAL:       return "octal literal";
        case TOKEN_HEXADECIMAL_LITERAL: return "hexadecimal literal";
        case TOKEN_BINARY_LITERAL:      return "binary literal";
        case TOKEN_CHARACTER_LITERAL:   return "character literal";
        case TOKEN_STRING_LITERAL:      return "string literal";
        case TOKEN_IDENTIFIER:          return "identifier";
        case TOKEN_EOF:                 return "end of input";
        case TOKEN_UNKNOWN:             return "unknown token";
    }

    return "unknown token";
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
        case TOKEN_AND_EQUAL:
        case TOKEN_OR_EQUAL:
        case TOKEN_CIRCUMFLEX_EQUAL:
        case TOKEN_LSHIFT_EQUAL:
        case TOKEN_RSHIFT_EQUAL:
            return true;
        default:
            return false;
    }
}

bool isTypeToken(TokenType type)
{
    return isIntegerTypeToken(type) || type == TOKEN_VOID;
}

bool isIntegerTypeToken(TokenType type)
{
    return type >= TOKEN_I8 && type <= TOKEN_U64;
}

bool isSignedIntegerTypeToken(TokenType type)
{
    return type >= TOKEN_I8 && type <= TOKEN_I64;
}

bool canImplicitlyWidenInteger(TokenType source, TokenType destination)
{
    if (!isIntegerTypeToken(source) || !isIntegerTypeToken(destination)) {
        return false;
    }

    if (isSignedIntegerTypeToken(source) != isSignedIntegerTypeToken(destination)) {
        return false;
    }

    return getIntegerTypeSize(source) < getIntegerTypeSize(destination);
}

size_t getIntegerTypeSize(TokenType type)
{
    switch (type) {
        case TOKEN_I8:
        case TOKEN_U8:
            return 1;
        case TOKEN_I16:
        case TOKEN_U16:
            return 2;
        case TOKEN_I32:
        case TOKEN_U32:
            return 4;
        case TOKEN_I64:
        case TOKEN_U64:
            return 8;
        default:
            return 0;
    }
}

size_t getTypeSlotCount(TokenType type)
{
    return type == TOKEN_I64 || type == TOKEN_U64 || type == TOKEN_F64 ? 2 : 1;
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
        case TOKEN_AMPERSAND:
        case TOKEN_CIRCUMFLEX:
        case TOKEN_NOT:
        case TOKEN_MINUS:
        case TOKEN_TILDE:
            return true;
        default:
            return false;
    }
}
