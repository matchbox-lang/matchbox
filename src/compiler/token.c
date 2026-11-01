#include "compiler/token.h"
#include <stdbool.h>
#include <stdio.h>

void print_token_value(Token token)
{
    if (token.type == TokenIncompleteInput || token.type == TokenEof || token.length == 0) {
        fprintf(stderr, "end of input");
        
        return;
    }

    fprintf(stderr, "%.*s", token.length, token.chars);
}

const char* get_token_type_name(TokenType type)
{
    switch (type) {
        case TokenKeywordIf:                  return "if";
        case TokenKeywordElse:                return "else";
        case TokenKeywordIn:                  return "in";
        case TokenKeywordIs:                  return "is";
        case TokenKeywordSizeof:              return "sizeof";
        case TokenKeywordAlignof:             return "alignof";
        case TokenKeywordTypeof:              return "typeof";
        case TokenKeywordFor:                 return "for";
        case TokenKeywordWhile:               return "while";
        case TokenKeywordMatch:               return "match";
        case TokenKeywordCase:                return "case";
        case TokenKeywordDefault:             return "default";
        case TokenKeywordBreak:               return "break";
        case TokenKeywordContinue:            return "continue";
        case TokenKeywordReturn:              return "return";
        case TokenKeywordYield:               return "yield";
        case TokenKeywordAs:                  return "as";
        case TokenKeywordVar:                 return "var";
        case TokenKeywordLet:                 return "let";
        case TokenKeywordStatic:              return "static";
        case TokenKeywordConst:               return "const";
        case TokenKeywordUse:                 return "use";
        case TokenKeywordEnd:                 return "end";
        case TokenKeywordClass:               return "class";
        case TokenKeywordStruct:              return "struct";
        case TokenKeywordProtocol:            return "protocol";
        case TokenKeywordEnum:                return "enum";
        case TokenKeywordGet:                 return "get";
        case TokenKeywordSet:                 return "set";
        case TokenKeywordFunc:                return "func";
        case TokenKeywordType:                return "type";
        case TokenKeywordInternal:            return "internal";
        case TokenKeywordExternal:            return "external";
        case TokenKeywordPublic:              return "public";
        case TokenKeywordPrivate:             return "private";
        case TokenKeywordSelf:                return "self";
        case TokenKeywordSpawn:               return "spawn";
        case TokenKeywordAwait:               return "await";
        case TokenKeywordDefer:               return "defer";
        case TokenKeywordTry:                 return "try";
        case TokenKeywordCatch:               return "catch";
        case TokenKeywordFinally:             return "finally";
        case TokenKeywordThrow:               return "throw";
        case TokenKeywordVoid:                return "void";
        case TokenKeywordI8:                  return "i8";
        case TokenKeywordI16:                 return "i16";
        case TokenKeywordI32:                 return "i32";
        case TokenKeywordI64:                 return "i64";
        case TokenKeywordU8:                  return "u8";
        case TokenKeywordU16:                 return "u16";
        case TokenKeywordU32:                 return "u32";
        case TokenKeywordU64:                 return "u64";
        case TokenKeywordIsize:               return "isize";
        case TokenKeywordUsize:               return "usize";
        case TokenKeywordF32:                 return "f32";
        case TokenKeywordF64:                 return "f64";
        case TokenKeywordChar:                return "char";
        case TokenKeywordString:              return "string";
        case TokenKeywordBool:                return "bool";
        case TokenKeywordTrue:                return "true";
        case TokenKeywordFalse:               return "false";
        case TokenPlus:                return "+";
        case TokenMinus:               return "-";
        case TokenStar:                return "*";
        case TokenSlash:               return "/";
        case TokenPercent:             return "%";
        case TokenPower:               return "**";
        case TokenCoalesce:            return "??";
        case TokenEqual:               return "=";
        case TokenPlusEqual:          return "+=";
        case TokenMinusEqual:         return "-=";
        case TokenStarEqual:          return "*=";
        case TokenSlashEqual:         return "/=";
        case TokenPercentEqual:       return "%=";
        case TokenPowerEqual:         return "**=";
        case TokenCoalesceEqual:      return "??" "=";
        case TokenEqualEqual:         return "==";
        case TokenNotEqual:           return "!=";
        case TokenEqualEqualEqual:   return "===";
        case TokenNotEqualEqual:     return "!==";
        case TokenEqualTilde:         return "=~";
        case TokenNotTilde:           return "!~";
        case TokenGreater:             return ">";
        case TokenGreaterEqual:       return ">=";
        case TokenLess:                return "<";
        case TokenLessEqual:          return "<=";
        case TokenSpaceship:           return "<=>";
        case TokenKeywordAnd:                 return "and";
        case TokenKeywordOr:                  return "or";
        case TokenKeywordNot:                 return "not";
        case TokenAmpersand:           return "&";
        case TokenPipe:                return "|";
        case TokenCircumflex:          return "^";
        case TokenLshift:              return "<<";
        case TokenRshift:              return ">>";
        case TokenTilde:               return "~";
        case TokenQuestion:            return "?";
        case TokenColon:               return ":";
        case TokenAndEqual:           return "&=";
        case TokenOrEqual:            return "|=";
        case TokenCircumflexEqual:    return "^=";
        case TokenLshiftEqual:        return "<<=";
        case TokenRshiftEqual:        return ">>=";
        case TokenArrow:               return "->";
        case TokenSafeAccess:         return "?.";
        case TokenPipeForward:        return "|>";
        case TokenPipeBackward:       return "<|";
        case TokenDollar:              return "$";
        case TokenRange:               return "..";
        case TokenRangeFromEnd:      return "..^";
        case TokenSpread:              return "...";
        case TokenAt:                  return "@";
        case TokenDot:                 return ".";
        case TokenLparen:              return "(";
        case TokenRparen:              return ")";
        case TokenLsquare:             return "[";
        case TokenRsquare:             return "]";
        case TokenLbrace:              return "{";
        case TokenRbrace:              return "}";
        case TokenSemicolon:           return ";";
        case TokenComma:               return ",";
        case TokenIntegerLiteral:     return "integer literal";
        case TokenFloatLiteral:       return "float literal";
        case TokenOctalLiteral:       return "octal literal";
        case TokenHexadecimalLiteral: return "hexadecimal literal";
        case TokenBinaryLiteral:      return "binary literal";
        case TokenCharacterLiteral:   return "character literal";
        case TokenStringLiteral:      return "string literal";
        case TokenIdentifier:          return "identifier";
        case TokenIncompleteInput:    return "incomplete input";
        case TokenEof:                 return "end of input";
        case TokenUnknown:             return "unknown token";
    }

    return "unknown token";
}

bool is_float_assignment_token(TokenType type)
{
    return type == TokenEqual || type == TokenPlusEqual || type == TokenMinusEqual
        || type == TokenStarEqual || type == TokenSlashEqual || type == TokenPercentEqual;
}

bool is_assignment_token(TokenType type)
{
    switch (type) {
        case TokenEqual:
        case TokenPlusEqual:
        case TokenMinusEqual:
        case TokenStarEqual:
        case TokenSlashEqual:
        case TokenPercentEqual:
        case TokenPowerEqual:
        case TokenAndEqual:
        case TokenOrEqual:
        case TokenCircumflexEqual:
        case TokenLshiftEqual:
        case TokenRshiftEqual:
            return true;
        default:
            return false;
    }
}

DataType get_token_data_type(TokenType type)
{
    switch (type) {
        case TokenKeywordI8:
            return TypeI8;
        case TokenKeywordI16:
            return TypeI16;
        case TokenKeywordI32:
            return TypeI32;
        case TokenKeywordI64:
            return TypeI64;
        case TokenKeywordU8:
            return TypeU8;
        case TokenKeywordU16:
            return TypeU16;
        case TokenKeywordU32:
            return TypeU32;
        case TokenKeywordU64:
            return TypeU64;
        case TokenKeywordIsize:
            return TypeIsize;
        case TokenKeywordUsize:
            return TypeUsize;
        case TokenKeywordF32:
            return TypeF32;
        case TokenKeywordF64:
            return TypeF64;
        case TokenKeywordChar:
            return TypeChar;
        case TokenKeywordString:
            return TypeString;
        case TokenKeywordBool:
            return TypeBool;
        case TokenKeywordVoid:
            return TypeVoid;
        default:
            return TypeUnknown;
    }
}

bool is_type_token(TokenType type)
{
    DataType data_type = get_token_data_type(type);

    return data_type != TypeUnknown;
}

bool is_character_comparison_token(TokenType type)
{
    switch (type) {
        case TokenEqualEqual:
        case TokenNotEqual:
        case TokenLess:
        case TokenLessEqual:
        case TokenGreater:
        case TokenGreaterEqual:
            return true;
        default:
            return false;
    }
}

bool is_comparison_token(TokenType type)
{
    switch (type) {
        case TokenGreater:
        case TokenGreaterEqual:
        case TokenLess:
        case TokenLessEqual:
        case TokenSpaceship:
            return true;
        default:
            return false;
    }
}

bool is_equality_token(TokenType type)
{
    switch (type) {
        case TokenEqualEqual:
        case TokenEqualEqualEqual:
        case TokenNotEqual:
        case TokenNotEqualEqual:
            return true;
        default:
            return false;
    }
}

bool is_logical_token(TokenType type)
{
    return type == TokenKeywordAnd || type == TokenKeywordOr;
}

bool is_bool_operator_token(TokenType type)
{
    return (is_comparison_token(type) && type != TokenSpaceship) || is_equality_token(type) || is_logical_token(type);
}

bool is_shift_token(TokenType type)
{
    switch (type) {
        case TokenLshift:
        case TokenRshift:
            return true;
        default:
            return false;
    }
}

bool is_term_token(TokenType type)
{
    switch (type) {
        case TokenPlus:
        case TokenMinus:
            return true;
        default:
            return false;
    }
}

bool is_factor_token(TokenType type)
{
    switch (type) {
        case TokenStar:
        case TokenSlash:
        case TokenPercent:
            return true;
        default:
            return false;
    }
}

bool is_prefix_token(TokenType type)
{
    switch (type) {
        case TokenKeywordConst:
        case TokenAmpersand:
        case TokenCircumflex:
        case TokenKeywordNot:
        case TokenMinus:
        case TokenTilde:
            return true;
        default:
            return false;
    }
}

bool is_match_arm_end_token(TokenType type)
{
    return type == TokenKeywordCase || type == TokenKeywordDefault || type == TokenRbrace;
}

bool is_wildcard_token(Token token)
{
    return token.type == TokenIdentifier && token.length == 1 && token.chars[0] == '_';
}
