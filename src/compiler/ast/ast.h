#ifndef AST_H
#define AST_H

#include "common/type.h"
#include "common/vector.h"
#include "compiler/lexer/token.h"
#include "compiler/generic_bindings.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct ASTNode ASTNode;
typedef struct Builtin Builtin;
typedef struct GenericFunction GenericFunction;
typedef struct Scope Scope;
typedef struct StringObject StringObject;

typedef struct MatchArm
{
    ASTNode* pattern;
    ASTNode* binding;
    ASTNode* guard;
    Token guard_token;
    ASTNode* branch;
} MatchArm;

typedef enum ReferenceType
{
    RefShared,
    RefExclusive,
    RefNone
} ReferenceType;

typedef enum ASTNodeType
{
    ASTArray,
    ASTSlice,
    ASTIndex,
    ASTIndexAssignment,
    ASTAssignment,
    ASTBinary,
    ASTBuiltinCall,
    ASTBoolean,
    ASTBreak,
    ASTCharacter,
    ASTCompound,
    ASTConditional,
    ASTContinue,
    ASTConversion,
    ASTFloat,
    ASTFor,
    ASTFunctionCall,
    ASTFunctionDefinition,
    ASTInteger,
    ASTMatch,
    ASTParameter,
    ASTPrefix,
    ASTReturn,
    ASTString,
    ASTVariable,
    ASTVariableDefinition,
    ASTWhile,
    ASTNone
} ASTNodeType;

typedef struct ASTNode
{
    ASTNodeType type;

    union {
        struct {
            Token token;
            Vector elements;
            Type type_id;
            bool flat;
            bool fill;
        } array;

        struct {
            Token token;
            ASTNode* expr;
            ASTNode* start;
            ASTNode* end;
            Type type_id;
            ReferenceType reference_type;
            Vector reference_origins;
        } slice;

        struct {
            Token token;
            ASTNode* array;
            ASTNode* index;
            ASTNode* value;
            Token operator;
            Type type_id;
            size_t position;
            bool constant;
        } index;

        struct {
            ASTNode* expr;
            Token token;
            Type type_id;
        } conversion;

        struct {
            Token token;
            ASTNode* condition;
            ASTNode* body;
        } while_statement;

        struct {
            Token token;
            ASTNode* array;
            ASTNode* end;
            ASTNode* index;
            ASTNode* value;
            ASTNode* body;
        } for_statement;

        struct {
            Token token;
        } loop_control;

        struct {
            Scope* scope;
            Token operator;
            Token token;
            ASTNode* expr;
            ASTNode* symbol;
            bool initializes_binding;
        } assignment;

        struct {
            Token operator;
            ASTNode* left_expr;
            ASTNode* right_expr;
            Type type_id;
            Type operand_type;
        } binary;

        struct {
            bool value;
            Token token;
        } boolean_literal;

        struct {
            Vector args;
            const Builtin* builtin;
            Token token;
        } builtin_call;

        struct {
            uint32_t value;
            Token token;
        } character_literal;

        struct {
            Scope* scope;
            Vector statements;
        } compound;

        struct {
            Token token;
            ASTNode* condition;
            ASTNode* then_branch;
            ASTNode* else_branch;
            Type type_id;
            ReferenceType reference_type;
            Vector reference_origins;
            bool expression;
        } conditional;

        struct {
            double value;
            Token token;
            Type type_id;
        } float_literal;

        struct {
            Scope* scope;
            Vector args;
            Vector type_args;
            StringObject* id;
            Token token;
            ASTNode* symbol;
            Vector reference_origins;
        } function_call;

        struct {
            Scope* scope;
            StringObject* id;
            Token token;
            Vector params;
            GenericFunction* generics;
            Vector array_moves;
            Vector array_initializations;
            Vector array_possible_initializations;
            Vector array_uninitialized_returns;
            bool is_test;
            bool is_test_skipped;
            bool has_explicit_return_type;
            bool signature_resolved;
            Token test_name;
            Type return_type_id;
            ReferenceType return_reference_type;
            ASTNode* return_reference_origin;
            ASTNode* body;
            ASTNode* next_overload;
            size_t call_count;
        } function_definition;

        struct {
            uint64_t value;
            Token token;
            Type type_id;
        } integer_literal;

        struct {
            Token token;
            ASTNode* subject;
            Vector arms;
            Type type_id;
            ReferenceType reference_type;
            Vector reference_origins;
            bool expression;
        } match;

        struct {
            Scope* scope;
            StringObject* id;
            Token token;
            Type type_id;
            ReferenceType reference_type;
            Vector reference_origins;
            size_t shared_access_count;
            bool exclusive_access_active;
            bool moved;
            int position;
            ASTNode* default_value;
            bool variadic;
        } parameter;

        struct {
            Token operator;
            ASTNode* expr;
            Vector reference_origins;
            size_t constant_position;
            bool evaluated;
        } prefix;

        struct {
            Token token;
            ASTNode* expr;
        } return_statement;

        struct {
            StringObject* value;
            Token token;
        } string_literal;

        struct {
            Scope* scope;
            StringObject* id;
            Token token;
            ASTNode* symbol;
        } variable;

        struct {
            Scope* scope;
            StringObject* id;
            Token token;
            bool fixed;
            bool constant;
            bool initialized;
            bool possibly_initialized;
            Type type_id;
            ReferenceType reference_type;
            Vector reference_origins;
            size_t shared_access_count;
            bool exclusive_access_active;
            bool moved;
            bool reference_access_active;
            size_t read_count;
            size_t discarded_read_count;
            bool discarded_reads_analyzed;
            bool address_taken;
            int register_position;
            int position;
            ASTNode* expr;
        } variable_definition;
    };
} ASTNode;

// Construction
void initialize_function_generics(ASTNode* function, GenericBindings* parent, Vector* types, Token source);
void finish_function_generics(ASTNode* function);
ASTNode* create_array_node(Token token, Type type, bool flat);
ASTNode* create_assignment_node(Token operator, Token token, ASTNode* expr);
ASTNode* create_ast_node(ASTNodeType type);
ASTNode* create_binary_node(ASTNode* left_expr, ASTNode* right_expr, Token token);
ASTNode* create_boolean_node(Token token, bool value);
ASTNode* create_builtin_call_node(Token token, const Builtin* builtin);
ASTNode* create_character_node(Token token, uint32_t value);
ASTNode* create_compound_node(Scope* scope);
ASTNode* create_conditional_node(Token token, ASTNode* condition, ASTNode* then_branch, ASTNode* else_branch);
ASTNode* create_conversion_node(Token token, ASTNode* expr, Type type_id);
ASTNode* create_float_node(Token token, double value, Type type_id);
ASTNode* create_for_node(Token token);
ASTNode* create_function_call_node(Token token);
ASTNode* create_function_definition_node(Token token, StringObject* id);
ASTNode* create_integer_node(Token token, uint64_t value, Type type_id);
ASTNode* create_loop_control_node(Token token);
ASTNode* create_match_node(Token token);
ASTNode* create_parameter_node(Token token, StringObject* id);
ASTNode* create_prefix_node(Token token, ASTNode* expr);
ASTNode* create_return_node(Token token, ASTNode* expr);
ASTNode* create_slice_node(Token token, ASTNode* expr, ASTNode* start);
ASTNode* create_string_node(Token token, StringObject* value);
ASTNode* create_typed_binary_node(ASTNode* left, ASTNode* right, Token token, Type operand_type, Type result_type);
ASTNode* create_variable_definition_node(Token token, StringObject* id, bool fixed);
ASTNode* create_variable_node(Token token);
ASTNode* create_while_node(Token token, ASTNode* condition, ASTNode* body);

// Queries
bool is_generic_function(const ASTNode* function);
bool is_function_specialization(const ASTNode* function);
bool function_signatures_equal(ASTNode* left, ASTNode* right);
ASTNode* get_function_analysis_body(const ASTNode* function);
Type add_implicit_generic_parameter(ASTNode* function, Token token);
Type get_parameter_argument_type(ASTNode* parameter);

// Lifetime
void free_ast_node(ASTNode* ast);
void replace_ast_node(ASTNode* ast, ASTNode* replacement);

#endif
