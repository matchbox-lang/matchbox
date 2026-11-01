#ifndef TOKEN_H
#define TOKEN_H

#include "common/data_type.h"
#include <stdbool.h>
#include <stdio.h>

typedef enum TokenType
{
    TokenKeywordIf,
    TokenKeywordElse,
    TokenKeywordIn,
    TokenKeywordIs,
    TokenKeywordType,
    TokenKeywordTypeof,
    TokenKeywordSizeof,
    TokenKeywordAlignof,
    TokenKeywordFor,
    TokenKeywordWhile,
    TokenKeywordMatch,
    TokenKeywordCase,
    TokenKeywordDefault,
    TokenKeywordBreak,
    TokenKeywordContinue,
    TokenKeywordReturn,
    TokenKeywordYield,
    TokenKeywordAs,
    TokenKeywordVar,
    TokenKeywordLet,
    TokenKeywordStatic,
    TokenKeywordConst,
    TokenKeywordUse,
    TokenKeywordEnd,
    TokenKeywordClass,
    TokenKeywordStruct,
    TokenKeywordProtocol,
    TokenKeywordEnum,
    TokenKeywordGet,
    TokenKeywordSet,
    TokenKeywordFunc,
    TokenKeywordInternal,
    TokenKeywordExternal,
    TokenKeywordPublic,
    TokenKeywordPrivate,
    TokenKeywordSelf,
    TokenKeywordSpawn,
    TokenKeywordAwait,
    TokenKeywordDefer,
    TokenKeywordTry,
    TokenKeywordCatch,
    TokenKeywordFinally,
    TokenKeywordThrow,
    TokenKeywordVoid,
    TokenKeywordI8,
    TokenKeywordI16,
    TokenKeywordI32,
    TokenKeywordI64,
    TokenKeywordU8,
    TokenKeywordU16,
    TokenKeywordU32,
    TokenKeywordU64,
    TokenKeywordIsize,
    TokenKeywordUsize,
    TokenKeywordF32,
    TokenKeywordF64,
    TokenKeywordChar,
    TokenKeywordString,
    TokenKeywordBool,
    TokenKeywordTrue,
    TokenKeywordFalse,
    TokenKeywordAnd,
    TokenKeywordOr,
    TokenKeywordNot,
    TokenPlus,
    TokenMinus,
    TokenStar,
    TokenSlash,
    TokenPercent,
    TokenPower,
    TokenCoalesce,
    TokenEqual,
    TokenPlusEqual,
    TokenMinusEqual,
    TokenStarEqual,
    TokenSlashEqual,
    TokenPercentEqual,
    TokenPowerEqual,
    TokenCoalesceEqual,
    TokenEqualEqual,
    TokenNotEqual,
    TokenEqualEqualEqual,
    TokenNotEqualEqual,
    TokenEqualTilde,
    TokenNotTilde,
    TokenGreater,
    TokenGreaterEqual,
    TokenLess,
    TokenLessEqual,
    TokenSpaceship,
    TokenAmpersand,
    TokenPipe,
    TokenCircumflex,
    TokenLshift,
    TokenRshift,
    TokenTilde,
    TokenQuestion,
    TokenColon,
    TokenAndEqual,
    TokenOrEqual,
    TokenCircumflexEqual,
    TokenLshiftEqual,
    TokenRshiftEqual,
    TokenArrow,
    TokenSafeAccess,
    TokenPipeForward,
    TokenPipeBackward,
    TokenDollar,
    TokenRange,
    TokenRangeFromEnd,
    TokenSpread,
    TokenAt,
    TokenDot,
    TokenLparen,
    TokenRparen,
    TokenLsquare,
    TokenRsquare,
    TokenLbrace,
    TokenRbrace,
    TokenSemicolon,
    TokenComma,
    TokenIntegerLiteral,
    TokenFloatLiteral,
    TokenOctalLiteral,
    TokenHexadecimalLiteral,
    TokenBinaryLiteral,
    TokenCharacterLiteral,
    TokenStringLiteral,
    TokenIdentifier,
    TokenIncompleteInput,
    TokenEof,
    TokenUnknown
} TokenType;

typedef struct Token
{
    TokenType type;
    const char* chars;
    size_t length;
    int line;
    int column;
} Token;

void print_token_value(Token token);
const char* get_token_type_name(TokenType type);
bool is_float_assignment_token(TokenType type);
bool is_assignment_token(TokenType type);
bool is_type_token(TokenType type);
DataType get_token_data_type(TokenType type);
bool is_character_comparison_token(TokenType type);
bool is_comparison_token(TokenType type);
bool is_equality_token(TokenType type);
bool is_logical_token(TokenType type);
bool is_bool_operator_token(TokenType type);
bool is_shift_token(TokenType type);
bool is_term_token(TokenType type);
bool is_factor_token(TokenType type);
bool is_prefix_token(TokenType type);
bool is_match_arm_end_token(TokenType type);
bool is_wildcard_token(Token token);

#endif
