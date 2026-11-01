#include "compiler/analyzer.h"
#include "builtin/builtin.h"
#include "builtin/io.h"
#include "common/errors.h"
#include "common/memory.h"
#include "common/table.h"
#include "common/util.h"
#include "common/vector.h"
#include "compiler/diagnostics.h"
#include "compiler/scope.h"
#include "compiler/token.h"
#include "compiler/validation.h"
#include "runtime/string_object.h"
#include <float.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef bool (*VisitId)(StringView id, void* context);

static bool visitNodeUses(ASTNode* ast, VisitId visit, void* context);
static void clearNodeAnalysis(ASTNode* ast, Vector* functions);
static void analyzeNode(Analyzer* analyzer, ASTNode* ast);
static void validateNodeGlobalAccess(Analyzer* analyzer, ASTNode* ast);
static void activateReferenceAccess(ASTNode* ast);
static void transferExclusiveReference(ASTNode* expression);
static void updateBooleanMatchCoverage(MatchArm* arm, bool* coversFalse, bool* coversTrue);
static ASTNode* findNodeReturn(ASTNode* ast);

typedef struct FunctionPath
{
    ASTNode* function;
    struct FunctionPath* previous;
} FunctionPath;

typedef struct LoopPath
{
    ASTNode* ast;
    struct LoopPath* previous;
} LoopPath;

typedef struct OwnershipState
{
    ASTNode* symbol;
    Vector referenceOrigins;
    size_t sharedAccessCount;
    bool exclusiveAccessActive;
    bool initialized;
    bool moved;
    bool referenceAccessActive;
} OwnershipState;

typedef struct ScopedUses
{
    VisitId visit;
    void* context;
    Vector shadowed;
} ScopedUses;

typedef struct ReferenceUse
{
    size_t last;
} ReferenceUse;

typedef struct ReferenceLifetimes
{
    size_t position;
    Table lastUses;
    Vector* releases;
} ReferenceLifetimes;

static _Noreturn void binaryTypeError(Token token, DataType left, DataType right)
{
    const char* leftName = getDataTypeName(left);
    const char* rightName = getDataTypeName(right);

    fatalSourceError(token.line, token.column, "No lossless common numeric type for %s and %s", leftName, rightName);
}

static _Noreturn void bindingAccessError(char* message, Token token, char* detail)
{
    fatalTokenError(token, detail, "%s", message);
}

static _Noreturn void floatLiteralOutOfRangeError(Token token, DataType type)
{
    const char* typeName = getDataTypeName(type);

    fatalSourceError(token.line, token.column, "Float literal is outside the range of %s", typeName);
}

static _Noreturn void floatLiteralRequiresTypeError(Token token)
{
    fatalSourceError(token.line, token.column, "Float literal requires an explicit type");
}

static _Noreturn void implicitConversionError(Token token, DataType source, DataType destination)
{
    const char* sourceName = getDataTypeName(source);
    const char* destinationName = getDataTypeName(destination);

    fatalTokenError(token, "", "Cannot implicitly convert %s to %s for binding ", sourceName, destinationName);
}

static _Noreturn void integerLiteralNotRepresentableError(Token token, DataType type)
{
    const char* typeName = getDataTypeName(type);

    fatalSourceError(token.line, token.column, "Integer literal cannot be represented as %s", typeName);
}

static _Noreturn void integerLiteralOutOfRangeError(Token token, DataType type)
{
    const char* typeName = getDataTypeName(type);

    fatalSourceError(token.line, token.column, "Integer literal is outside the range of %s", typeName);
}

static _Noreturn void integerLiteralRequiresTypeError(Token token)
{
    fatalSourceError(token.line, token.column, "Integer literal requires an explicit type");
}

static _Noreturn void invalidBuiltinArgumentsError(Token token)
{
    fatalTokenError(token, "", "Invalid arguments to builtin ");
}

static _Noreturn void negativeUnsignedIntegerError(Token token)
{
    fatalSourceError(token.line, token.column, "Negative integer literal cannot have an unsigned type");
}

static _Noreturn void redefinitionError(char* kind, Token token)
{
    fatalTokenError(token, " is already defined", "%s ", kind);
}

static _Noreturn void returnOutsideFunctionError(Token token)
{
    fatalSourceError(token.line, token.column, "Return statement is only valid inside a function");
}

static _Noreturn void semanticError(char* message, Token token)
{
    fatalSourceError(token.line, token.column, "%s", message);
}

static _Noreturn void semanticTokenError(char* message, Token token)
{
    fatalTokenError(token, "", "%s", message);
}

static _Noreturn void symbolError(const char* detail, Token token)
{
    fatalTokenError(token, detail, "");
}

static _Noreturn void unsupportedExplicitConversionError(Token token, DataType source, DataType destination)
{
    const char* sourceName = getDataTypeName(source);
    const char* destinationName = getDataTypeName(destination);

    fatalSourceError(token.line, token.column, "Unsupported explicit conversion from %s to %s",
        sourceName, destinationName);
}

static ASTNode* getIntegerLiteral(ASTNode* expression, bool* negative)
{
    *negative = false;

    if (expression->type == AST_INTEGER && isSignedIntegerType(expression->integerLiteral.typeId)
        && expression->integerLiteral.value > INT64_MAX) {
        return NULL;
    }

    if (expression->type == AST_INTEGER) {
        return expression;
    }

    if (expression->type != AST_PREFIX || expression->prefix.operator.type != TOKEN_MINUS
        || expression->prefix.expr->type != AST_INTEGER) {
        return NULL;
    }

    *negative = true;

    return expression->prefix.expr;
}

static bool integerLiteralFitsType(uint64_t value, bool negative, DataType type)
{
    size_t bits = getIntegerTypeSize(type) * CHAR_BIT;

    if (!bits || (!isSignedIntegerType(type) && negative)) {
        return false;
    }

    if (!isSignedIntegerType(type)) {
        return bits == 64 || value <= (UINT64_C(1) << bits) - 1;
    }

    uint64_t limit = UINT64_C(1) << (bits - 1);

    return negative ? value <= limit : value < limit;
}

static bool integerLiteralFitsFloat(uint64_t value, DataType type)
{
    size_t precision;

    if (type == TYPE_F32) {
        precision = FLT_MANT_DIG;
    } else if (type == TYPE_F64) {
        precision = DBL_MANT_DIG;
    } else {
        return false;
    }

    size_t bits = 0;
    uint64_t remaining = value;

    while (remaining) {
        bits++;
        remaining >>= 1;
    }

    if (bits <= precision) {
        return true;
    }

    size_t discardedBits = bits - precision;
    uint64_t mask = (UINT64_C(1) << discardedBits) - 1;

    return (value & mask) == 0;
}

static bool applyIntegerLiteralType(ASTNode* expression, DataType type)
{
    bool negative;
    ASTNode* literal = getIntegerLiteral(expression, &negative);

    if (!literal) {
        return false;
    }

    if (negative && isIntegerType(type) && !isSignedIntegerType(type)) {
        negativeUnsignedIntegerError(literal->integerLiteral.token);
    }

    bool fitsInteger = integerLiteralFitsType(literal->integerLiteral.value, negative, type);
    bool fitsFloat = integerLiteralFitsFloat(literal->integerLiteral.value, type);

    if (isFloatType(type) && !fitsFloat) {
        integerLiteralNotRepresentableError(literal->integerLiteral.token, type);
    }

    if (isIntegerType(type) && !fitsInteger) {
        integerLiteralOutOfRangeError(literal->integerLiteral.token, type);
    }

    literal->integerLiteral.typeId = type;

    return true;
}

static ASTNode* getFloatLiteral(ASTNode* expression)
{
    if (expression->type == AST_PREFIX && expression->prefix.operator.type == TOKEN_MINUS) {
        expression = expression->prefix.expr;
    }

    if (expression->type != AST_FLOAT) {
        return NULL;
    }

    return expression;
}

static void applyLiteralType(ASTNode* expression, DataType type)
{
    if (expression->type == AST_PREFIX && expression->prefix.operator.type == TOKEN_CONST) {
        applyLiteralType(expression->prefix.expr, type);

        return;
    }

    if (isIntegerType(type) || isFloatType(type)) {
        applyIntegerLiteralType(expression, type);
    }

    expression = getFloatLiteral(expression);

    if (!expression || !isFloatType(type)) {
        return;
    }

    if (type == TYPE_F32 && expression->floatLiteral.value > FLT_MAX) {
        floatLiteralOutOfRangeError(expression->floatLiteral.token, type);
    }

    expression->floatLiteral.typeId = type;
}

static ASTNode* findFrameSymbol(Scope* scope, StringView id)
{
    Scope* frame = scope->frameScope;
    ASTNode* symbol = NULL;

    for (Scope* current = scope;
        current && current->frameScope == frame && !symbol;
        current = current->parent) {
        symbol = getLocalSymbol(current, id);
    }

    return symbol;
}

static ASTNode* findSymbol(Analyzer* analyzer, StringView id)
{
    ASTNode* symbol = findFrameSymbol(analyzer->currentScope, id);
    if (symbol) {
        return symbol;
    }

    return getLocalSymbol(analyzer->topLevel->compound.scope, id);
}

static size_t* getSharedAccessCount(ASTNode* symbol)
{
    if (isParameter(symbol)) {
        return &symbol->parameter.sharedAccessCount;
    }

    return &symbol->variableDefinition.sharedAccessCount;
}

static bool hasExclusiveAccess(ASTNode* symbol)
{
    if (isParameter(symbol)) {
        return symbol->parameter.exclusiveAccessActive;
    }

    return symbol->variableDefinition.exclusiveAccessActive;
}

static void setExclusiveAccess(ASTNode* symbol, bool active)
{
    if (isParameter(symbol)) {
        symbol->parameter.exclusiveAccessActive = active;

        return;
    }

    symbol->variableDefinition.exclusiveAccessActive = active;
}

static bool isMoved(ASTNode* symbol)
{
    if (isParameter(symbol)) {
        return symbol->parameter.moved;
    }

    return symbol->variableDefinition.moved;
}

static void setMoved(ASTNode* symbol)
{
    if (isParameter(symbol)) {
        symbol->parameter.moved = true;

        return;
    }

    symbol->variableDefinition.moved = true;
}

static void setMovedState(ASTNode* symbol, bool moved)
{
    if (isParameter(symbol)) {
        symbol->parameter.moved = moved;

        return;
    }

    symbol->variableDefinition.moved = moved;
}

static ASTNode* getReferenceOriginOrSelf(ASTNode* symbol)
{
    ASTNode* origin = getReferenceOrigin(symbol);

    if (origin) {
        return origin;
    }

    return symbol;
}

static ASTNode* referenceOriginFromExpression(ASTNode* ast)
{
    if (ast->type == AST_PREFIX && isVariable(ast->prefix.expr)) {
        ASTNode* symbol = ast->prefix.expr->variable.symbol;

        return getReferenceOriginOrSelf(symbol);
    }

    return getReferenceOrigin(ast);
}

static Vector* referenceOriginsFromExpression(ASTNode* ast)
{
    Vector* origins = getReferenceOrigins(ast);

    if (origins) {
        return origins;
    }

    static Vector empty;

    return &empty;
}

static bool containsReferenceOrigin(Vector* origins, ASTNode* origin)
{
    size_t count = countVector(origins);

    for (size_t i = 0; i < count; i++) {
        if (getVectorAt(origins, i) == origin) {
            return true;
        }
    }

    return false;
}

static void addReferenceOrigin(Vector* origins, ASTNode* origin)
{
    if (!origin || containsReferenceOrigin(origins, origin)) {
        return;
    }

    pushVectorItem(origins, origin);
}

static void copyReferenceOrigins(Vector* destination, Vector* source)
{
    if (destination == source) {
        return;
    }

    destination->count = 0;
    size_t count = countVector(source);

    for (size_t i = 0; i < count; i++) {
        addReferenceOrigin(destination, getVectorAt(source, i));
    }
}

static void mergeReferenceOrigins(Vector* destination, Vector* source)
{
    size_t count = countVector(source);

    for (size_t i = 0; i < count; i++) {
        addReferenceOrigin(destination, getVectorAt(source, i));
    }
}

static void setReferenceOriginsFromExpression(ASTNode* destination, ASTNode* expression)
{
    Vector* destinationOrigins = getReferenceOrigins(destination);
    Vector* origins = referenceOriginsFromExpression(expression);
    copyReferenceOrigins(destinationOrigins, origins);
}

static bool matchesShadowedId(StringObject* shadowed, StringView id)
{
    StringView view = getStringView(shadowed);

    return compareStringView(view, id);
}

static bool visitUnshadowedUse(StringView id, void* context)
{
    ScopedUses* scope = context;
    size_t index = 0;

    while (index < scope->shadowed.count && !matchesShadowedId(scope->shadowed.data[index], id)) {
        index++;
    }

    if (index < scope->shadowed.count) {
        return false;
    }

    return scope->visit(id, scope->context);
}

static bool visitScopedStatementUses(ASTNode* ast, ScopedUses* scope)
{
    bool found = visitNodeUses(ast, visitUnshadowedUse, scope);

    if (ast->type == AST_VARIABLE_DEFINITION) {
        pushVectorItem(&scope->shadowed, ast->variableDefinition.id);
    }

    return found;
}

static bool visitScopedStatementListUses(Vector* nodes, ScopedUses* scope)
{
    size_t count = countVector(nodes);
    size_t index = 0;
    bool found = false;

    while (index < count && !found) {
        ASTNode* node = getVectorAt(nodes, index);

        found = visitScopedStatementUses(node, scope);
        index++;
    }

    return found;
}

static bool visitScopedBlockUses(Vector* nodes, VisitId visit, void* context)
{
    ScopedUses scope = {.visit = visit, .context = context};
    bool found = visitScopedStatementListUses(nodes, &scope);

    freeVector(&scope.shadowed);

    return found;
}

static bool visitFunctionUses(ASTNode* ast, VisitId visit, void* context)
{
    ScopedUses scope = {.visit = visit, .context = context};
    Vector* params = &ast->functionDefinition.params;

    for (size_t i = 0; i < params->count; i++) {
        ASTNode* parameter = getVectorAt(params, i);

        pushVectorItem(&scope.shadowed, parameter->parameter.id);
    }

    bool found = visitNodeUses(ast->functionDefinition.body, visitUnshadowedUse, &scope);

    freeVector(&scope.shadowed);

    return found;
}

static bool visitNodeListUses(Vector* nodes, VisitId visit, void* context)
{
    size_t count = countVector(nodes);

    for (size_t i = 0; i < count; i++) {
        ASTNode* node = getVectorAt(nodes, i);
        bool found = visitNodeUses(node, visit, context);

        if (found) {
            return true;
        }
    }

    return false;
}

static bool visitMatchArmUses(MatchArm* arm, VisitId visit, void* context)
{
    if (visitNodeUses(arm->pattern, visit, context)) {
        return true;
    }

    if (!arm->binding) {
        return visitNodeUses(arm->guard, visit, context) || visitNodeUses(arm->branch, visit, context);
    }

    void* ids[] = {arm->binding->variableDefinition.id};
    ScopedUses scope = {.visit = visit, .context = context, .shadowed = {.data = ids, .count = 1}};

    return visitNodeUses(arm->guard, visitUnshadowedUse, &scope)
        || visitNodeUses(arm->branch, visitUnshadowedUse, &scope);
}

static bool visitMatchUses(ASTNode* ast, VisitId visit, void* context)
{
    if (visitNodeUses(ast->match.subject, visit, context)) {
        return true;
    }

    size_t count = countVector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = getVectorAt(&ast->match.arms, i);
        bool found = visitMatchArmUses(arm, visit, context);

        if (found) {
            return true;
        }
    }

    return visitNodeUses(ast->match.defaultBranch, visit, context);
}

static bool visitAssignmentUses(ASTNode* ast, VisitId visit, void* context)
{
    Token token = ast->assignment.token;
    StringView id = createStringView(token.chars, token.length);

    return visit(id, context) || visitNodeUses(ast->assignment.expr, visit, context);
}

static bool visitNodeUses(ASTNode* ast, VisitId visit, void* context)
{
    if (!ast) {
        return false;
    }

    switch (ast->type) {
        case AST_ASSIGNMENT:
            return visitAssignmentUses(ast, visit, context);
        case AST_BINARY:
            return visitNodeUses(ast->binary.leftExpr, visit, context)
                || visitNodeUses(ast->binary.rightExpr, visit, context);
        case AST_BUILTIN_CALL:
            return visitNodeListUses(&ast->builtinCall.args, visit, context);
        case AST_COMPOUND:
            return visitScopedBlockUses(&ast->compound.statements, visit, context);
        case AST_WHILE:
            return visitNodeUses(ast->whileStatement.condition, visit, context)
                || visitNodeUses(ast->whileStatement.body, visit, context);
        case AST_CONDITIONAL:
            return visitNodeUses(ast->conditional.condition, visit, context)
                || visitNodeUses(ast->conditional.thenBranch, visit, context)
                || visitNodeUses(ast->conditional.elseBranch, visit, context);
        case AST_CONVERSION:
            return visitNodeUses(ast->conversion.expr, visit, context);
        case AST_FUNCTION_CALL:
            return visitNodeListUses(&ast->functionCall.args, visit, context);
        case AST_FUNCTION_DEFINITION:
            return visitFunctionUses(ast, visit, context);
        case AST_MATCH:
            return visitMatchUses(ast, visit, context);
        case AST_PREFIX:
            return visitNodeUses(ast->prefix.expr, visit, context);
        case AST_RETURN:
            return visitNodeUses(ast->returnStatement.expr, visit, context);
        case AST_VARIABLE: {
            StringView id = getStringView(ast->variable.id);

            return visit(id, context);
        }
        case AST_VARIABLE_DEFINITION:
            return visitNodeUses(ast->variableDefinition.expr, visit, context);
        default:
            return false;
    }
}

static bool matchesReferenceId(StringView id, void* context)
{
    return compareStringView(id, *(StringView*)context);
}

static bool recordReferenceUse(StringView id, void* context)
{
    ReferenceLifetimes* lifetimes = context;
    ReferenceUse* use = getTableAt(&lifetimes->lastUses, id);

    if (use) {
        use->last = lifetimes->position;

        return false;
    }

    use = allocateMemory(sizeof(ReferenceUse));
    use->last = lifetimes->position;
    setTableAt(&lifetimes->lastUses, id, use);

    return false;
}

static void initReferenceLifetimes(ReferenceLifetimes* lifetimes, Vector* nodes, size_t start)
{
    size_t count = countVector(nodes);

    initTable(&lifetimes->lastUses, count * 2);

    lifetimes->releases = allocateZeroedMemory(count, sizeof(Vector));

    for (size_t i = start; i < count; i++) {
        ASTNode* ast = getVectorAt(nodes, i);
        lifetimes->position = i;

        visitNodeUses(ast, recordReferenceUse, lifetimes);
    }
}

static void scheduleReferenceRelease(ReferenceLifetimes* lifetimes, ASTNode* ast, size_t position)
{
    if (!isVariableDefinition(ast) || getReferenceType(ast) == REFERENCE_NONE) {
        return;
    }

    StringView id = getStringView(ast->variableDefinition.id);
    ReferenceUse* use = getTableAt(&lifetimes->lastUses, id);

    if (use) {
        position = MAX(position, use->last);
    }

    pushVectorItem(&lifetimes->releases[position], ast);
}

static void freeReferenceLifetimes(ReferenceLifetimes* lifetimes, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        freeVector(&lifetimes->releases[i]);
    }

    for (size_t i = 0; i < lifetimes->lastUses.capacity; i++) {
        TableItem* item = lifetimes->lastUses.data[i];

        while (item) {
            freeMemory(item->value);
            item = item->next;
        }
    }

    freeMemory(lifetimes->releases);
    freeTable(&lifetimes->lastUses);
}

static void releaseReferenceAccess(ASTNode* reference)
{
    Vector* origins = getReferenceOrigins(reference);
    size_t count = countVector(origins);

    for (size_t i = 0; i < count; i++) {
        ASTNode* origin = getVectorAt(origins, i);

        if (reference->variableDefinition.referenceType == REFERENCE_SHARED) {
            (*getSharedAccessCount(origin))--;
        } else {
            setExclusiveAccess(origin, false);
        }
    }

    reference->variableDefinition.referenceAccessActive = false;
}

static void releaseInactiveReferenceAccess(Vector* references)
{
    size_t count = countVector(references);

    for (size_t i = 0; i < count; i++) {
        ASTNode* ast = getVectorAt(references, i);

        if (ast->variableDefinition.referenceAccessActive) {
            releaseReferenceAccess(ast);
        }
    }
}

static void analyzeNodes(Analyzer* analyzer, Vector* nodes, size_t start)
{
    size_t count = countVector(nodes);

    if (start >= count) {
        return;
    }

    ReferenceLifetimes lifetimes;
    ReferenceLifetimes* previousLifetimes = analyzer->referenceLifetimes;
    Vector* previousNodes = analyzer->currentNodes;
    size_t previousNext = analyzer->nextNode;

    initReferenceLifetimes(&lifetimes, nodes, start);

    analyzer->referenceLifetimes = &lifetimes;

    for (size_t i = 0; i < start; i++) {
        ASTNode* ast = getVectorAt(nodes, i);

        scheduleReferenceRelease(&lifetimes, ast, start);
    }

    for (size_t i = start; i < count; i++) {
        ASTNode* ast = getVectorAt(nodes, i);
        analyzer->currentNodes = nodes;
        analyzer->nextNode = i + 1;

        analyzeNode(analyzer, ast);
        scheduleReferenceRelease(&lifetimes, ast, i);
        releaseInactiveReferenceAccess(&lifetimes.releases[i]);
    }

    analyzer->referenceLifetimes = previousLifetimes;
    analyzer->currentNodes = previousNodes;
    analyzer->nextNode = previousNext;

    freeReferenceLifetimes(&lifetimes, count);
}

static void analyzeExpressionNodes(Analyzer* analyzer, Vector* nodes)
{
    analyzeNodes(analyzer, nodes, 0);
}

static ASTNode* getBranchResult(ASTNode* branch)
{
    if (branch->type == AST_CONDITIONAL) {
        return branch;
    }

    Vector* statements = &branch->compound.statements;
    size_t count = countVector(statements);

    if (!count) {
        return NULL;
    }

    return getVectorAt(statements, count - 1);
}

static void setExpressionReferenceType(ASTNode* ast, ReferenceType type)
{
    if (ast->type == AST_CONDITIONAL) {
        ast->conditional.referenceType = type;

        return;
    }

    ast->match.referenceType = type;
}

static void mergeBranchReference(
    ASTNode* ast, ASTNode* result, bool first, bool hasExpectedType, Token token)
{
    ReferenceType type = getReferenceType(result);

    if (first && !hasExpectedType) {
        setExpressionReferenceType(ast, type);
    } else if (getReferenceType(ast) != type) {
        semanticTokenError("Expression branches have incompatible reference types ", token);
    }

    Vector* origins = referenceOriginsFromExpression(result);
    Vector* expressionOrigins = getReferenceOrigins(ast);
    mergeReferenceOrigins(expressionOrigins, origins);
}

static Scope* enterBranchScope(Analyzer* analyzer, ASTNode* branch, ASTNode* binding)
{
    Scope* previousScope = analyzer->currentScope;
    branch->compound.scope = createScope(previousScope);
    analyzer->currentScope = branch->compound.scope;

    if (binding) {
        binding->variableDefinition.scope = analyzer->currentScope;
        binding->variableDefinition.position = getNextLocalPosition(analyzer->currentScope);
        
        StringView id = getStringView(binding->variableDefinition.id);

        setLocalVariableSymbol(analyzer->currentScope, id, binding);
    }

    return previousScope;
}

static DataType analyzeBranchContents(Analyzer* analyzer, ASTNode* branch, DataType type)
{
    size_t count = countVector(&branch->compound.statements);

    if (count && isIntegerType(type)) {
        ASTNode* result = getVectorAt(&branch->compound.statements, count - 1);
        applyIntegerLiteralType(result, type);
    }

    analyzeExpressionNodes(analyzer, &branch->compound.statements);

    if (!count) {
        return TYPE_VOID;
    }

    ASTNode* result = getVectorAt(&branch->compound.statements, count - 1);
    DataType resultType = getNodeDataType(result);

    if (resultType == type || canImplicitlyWidenType(resultType, type)) {
        return type;
    }

    return resultType;
}

static DataType analyzeScopedBranch(Analyzer* analyzer, ASTNode* branch, ASTNode* binding, DataType type)
{
    Scope* previousScope = enterBranchScope(analyzer, branch, binding);
    DataType resultType = analyzeBranchContents(analyzer, branch, type);
    analyzer->currentScope = previousScope;

    return resultType;
}

static DataType analyzeElseBranch(Analyzer* analyzer, ASTNode* branch, DataType type)
{
    if (!branch) {
        return TYPE_VOID;
    }

    if (branch->type == AST_CONDITIONAL) {
        branch->conditional.typeId = type;
        analyzeNode(analyzer, branch);

        return getNodeDataType(branch);
    }

    return analyzeScopedBranch(analyzer, branch, NULL, type);
}

static void captureSymbolOwnership(Vector* states, ASTNode* symbol)
{
    if (!isVariableType(symbol)) {
        return;
    }

    OwnershipState* state = allocateMemory(sizeof(OwnershipState));
    state->symbol = symbol;
    initVector(&state->referenceOrigins);
    Vector* origins = getReferenceOrigins(symbol);
    copyReferenceOrigins(&state->referenceOrigins, origins);
    state->sharedAccessCount = *getSharedAccessCount(symbol);
    state->exclusiveAccessActive = hasExclusiveAccess(symbol);
    state->initialized = isInitialized(symbol);
    state->moved = isMoved(symbol);
    state->referenceAccessActive = isVariableDefinition(symbol) && symbol->variableDefinition.referenceAccessActive;

    pushVectorItem(states, state);
}

static void captureScopeOwnership(Vector* states, Scope* scope)
{
    for (size_t i = 0; i < scope->symbols.capacity; i++) {
        TableItem* item = scope->symbols.data[i];

        while (item) {
            captureSymbolOwnership(states, item->value);
            item = item->next;
        }
    }
}

static void captureOwnershipState(Vector* states, Scope* scope)
{
    initVector(states);

    while (scope) {
        captureScopeOwnership(states, scope);
        scope = scope->parent;
    }
}

static void copyOwnershipState(Vector* destination, Vector* source)
{
    initVector(destination);

    for (size_t i = 0; i < countVector(source); i++) {
        OwnershipState* sourceState = getVectorAt(source, i);
        OwnershipState* destinationState = allocateMemory(sizeof(OwnershipState));

        *destinationState = *sourceState;
        initVector(&destinationState->referenceOrigins);
        copyReferenceOrigins(&destinationState->referenceOrigins, &sourceState->referenceOrigins);
        pushVectorItem(destination, destinationState);
    }
}

static void applyOwnershipState(OwnershipState* state)
{
    ASTNode* symbol = state->symbol;

    *getSharedAccessCount(symbol) = state->sharedAccessCount;
    setExclusiveAccess(symbol, state->exclusiveAccessActive);
    setMovedState(symbol, state->moved);

    if (!isVariableDefinition(symbol)) {
        return;
    }

    Vector* origins = getReferenceOrigins(symbol);
    copyReferenceOrigins(origins, &state->referenceOrigins);
    symbol->variableDefinition.initialized = state->initialized;
    symbol->variableDefinition.referenceAccessActive = state->referenceAccessActive;
}

static void applyOwnershipStates(Vector* states)
{
    for (size_t i = 0; i < countVector(states); i++) {
        applyOwnershipState(getVectorAt(states, i));
    }
}

static void updateOwnershipStates(Vector* states)
{
    for (size_t i = 0; i < countVector(states); i++) {
        OwnershipState* state = getVectorAt(states, i);
        ASTNode* symbol = state->symbol;

        Vector* origins = getReferenceOrigins(symbol);
        copyReferenceOrigins(&state->referenceOrigins, origins);
        state->sharedAccessCount = *getSharedAccessCount(symbol);
        state->exclusiveAccessActive = hasExclusiveAccess(symbol);
        state->initialized = isInitialized(symbol);
        state->moved = isMoved(symbol);
        state->referenceAccessActive = isVariableDefinition(symbol)
            && symbol->variableDefinition.referenceAccessActive;
    }
}

static void freeOwnershipStates(Vector* states)
{
    for (size_t i = 0; i < countVector(states); i++) {
        OwnershipState* state = getVectorAt(states, i);
        freeVector(&state->referenceOrigins);
        freeMemory(state);
    }

    freeVector(states);
}

static bool branchUsesReference(ASTNode* branch, StringObject* id, ASTNode* binding)
{
    if (!branch) {
        return false;
    }

    StringView view = getStringView(id);

    if (binding && matchesShadowedId(binding->variableDefinition.id, view)) {
        return false;
    }

    return visitNodeUses(branch, matchesReferenceId, &view);
}

static bool continuationUsesReference(Analyzer* analyzer, StringObject* id)
{
    LoopPath* loop = analyzer->loopPath;

    while (loop) {
        if (branchUsesReference(loop->ast, id, NULL)) {
            return true;
        }

        loop = loop->previous;
    }

    ReferenceLifetimes* lifetimes = analyzer->referenceLifetimes;

    if (!lifetimes) {
        return false;
    }

    StringView view = getStringView(id);
    ReferenceUse* use = getTableAt(&lifetimes->lastUses, view);

    return use && use->last >= analyzer->nextNode;
}

static void releaseReferencesDeadOnBranch(Analyzer* analyzer, Vector* states, ASTNode* branch, ASTNode* binding)
{
    for (size_t i = 0; i < countVector(states); i++) {
        OwnershipState* state = getVectorAt(states, i);
        ASTNode* symbol = state->symbol;

        if (!isVariableDefinition(symbol) || !symbol->variableDefinition.referenceAccessActive) {
            continue;
        }

        StringObject* id = symbol->variableDefinition.id;
        if (branchUsesReference(branch, id, binding) || continuationUsesReference(analyzer, id)) {
            continue;
        }

        releaseReferenceAccess(symbol);
    }
}

static void analyzeOwnershipBranch(
    Analyzer* analyzer,
    Vector* states,
    ASTNode* branch,
    DataType expectedType,
    DataType* type)
{
    applyOwnershipStates(states);
    releaseReferencesDeadOnBranch(analyzer, states, branch, NULL);
    *type = analyzeElseBranch(analyzer, branch, expectedType);
    updateOwnershipStates(states);
}

static void mergeOwnershipState(OwnershipState* thenState, OwnershipState* elseState)
{
    OwnershipState merged = *thenState;
    initVector(&merged.referenceOrigins);
    copyReferenceOrigins(&merged.referenceOrigins, &thenState->referenceOrigins);
    mergeReferenceOrigins(&merged.referenceOrigins, &elseState->referenceOrigins);
    merged.sharedAccessCount = MAX(thenState->sharedAccessCount, elseState->sharedAccessCount);
    merged.exclusiveAccessActive = thenState->exclusiveAccessActive || elseState->exclusiveAccessActive;
    merged.initialized = thenState->initialized && elseState->initialized;
    merged.moved = thenState->moved || elseState->moved;
    merged.referenceAccessActive = thenState->referenceAccessActive || elseState->referenceAccessActive;

    applyOwnershipState(&merged);
    freeVector(&merged.referenceOrigins);
}

static void mergeOwnershipStates(Vector* thenStates, Vector* elseStates)
{
    size_t count = countVector(thenStates);

    for (size_t i = 0; i < count; i++) {
        mergeOwnershipState(getVectorAt(thenStates, i), getVectorAt(elseStates, i));
    }
}

static void validateLoopReferenceOrigins(OwnershipState* entry, Token token)
{
    Vector* origins = getReferenceOrigins(entry->symbol);
    size_t count = countVector(origins);

    for (size_t i = 0; i < count; i++) {
        ASTNode* origin = getVectorAt(origins, i);
        bool existing = containsReferenceOrigin(&entry->referenceOrigins, origin);

        if (!existing) {
            semanticError("While loop cannot change the origin of an outer reference", token);
        }
    }
}

static void validateLoopOwnership(Vector* entryStates, Token token)
{
    size_t count = countVector(entryStates);

    for (size_t i = 0; i < count; i++) {
        OwnershipState* entry = getVectorAt(entryStates, i);
        ASTNode* symbol = entry->symbol;
        bool moved = isMoved(symbol);

        if (moved && !entry->moved) {
            semanticError("While loop cannot move an outer reference across iterations", token);
        }

        validateLoopReferenceOrigins(entry, token);
    }
}

static void analyzeWhile(Analyzer* analyzer, ASTNode* ast)
{
    Vector entryStates;
    Vector bodyStates;
    LoopPath path = {ast, analyzer->loopPath};
    analyzer->loopPath = &path;

    captureOwnershipState(&entryStates, analyzer->currentScope);
    copyOwnershipState(&bodyStates, &entryStates);
    analyzeNode(analyzer, ast->whileStatement.condition);

    DataType type = getNodeDataType(ast->whileStatement.condition);

    if (type != TYPE_BOOL) {
        semanticError("While condition must have type bool", ast->whileStatement.token);
    }

    analyzeScopedBranch(analyzer, ast->whileStatement.body, NULL, TYPE_VOID);
    validateLoopOwnership(&entryStates, ast->whileStatement.token);
    updateOwnershipStates(&bodyStates);

    analyzer->loopPath = path.previous;

    mergeOwnershipStates(&entryStates, &bodyStates);
    freeOwnershipStates(&entryStates);
    freeOwnershipStates(&bodyStates);
}

static void validateExpressionBranch(ASTNode* branch, Token token)
{
    if (branch->type == AST_CONDITIONAL) {
        return;
    }

    Vector* statements = &branch->compound.statements;
    if (countVector(statements) != 1) {
        semanticTokenError("Expression branch must contain exactly one expression ", token);
    }

    ASTNode* expression = getVectorAt(statements, 0);
    if (!isExpressionStatement(expression)) {
        semanticTokenError("Expression branch must contain exactly one expression ", token);
    }
}

static void validateConditionalExpression(ASTNode* ast)
{
    if (!ast->conditional.expression) {
        return;
    }

    validateExpressionBranch(ast->conditional.thenBranch, ast->conditional.token);

    if (ast->conditional.elseBranch) {
        validateExpressionBranch(ast->conditional.elseBranch, ast->conditional.token);
    }
}

static void mergeOwnershipOutcome(Vector* mergedStates, Vector* states, bool* initialized)
{
    if (!*initialized) {
        copyOwnershipState(mergedStates, states);
        *initialized = true;

        return;
    }

    mergeOwnershipStates(mergedStates, states);
    updateOwnershipStates(mergedStates);
}

static void analyzeConditional(Analyzer* analyzer, ASTNode* ast)
{
    Vector* continuationNodes = analyzer->currentNodes;
    size_t continuationStart = analyzer->nextNode;

    validateConditionalExpression(ast);
    analyzeNode(analyzer, ast->conditional.condition);

    if (getNodeDataType(ast->conditional.condition) != TYPE_BOOL) {
        semanticError("Condition must have type bool", ast->conditional.token);
    }

    Vector thenStates;
    Vector elseStates;

    captureOwnershipState(&thenStates, analyzer->currentScope);
    copyOwnershipState(&elseStates, &thenStates);

    analyzer->currentNodes = continuationNodes;
    analyzer->nextNode = continuationStart;
    releaseReferencesDeadOnBranch(analyzer, &thenStates, ast->conditional.thenBranch, NULL);

    DataType type = ast->conditional.typeId;
    DataType thenType = analyzeScopedBranch(analyzer, ast->conditional.thenBranch, NULL, type);

    updateOwnershipStates(&thenStates);

    analyzer->currentNodes = continuationNodes;
    analyzer->nextNode = continuationStart;

    DataType elseType;

    analyzeOwnershipBranch(analyzer, &elseStates, ast->conditional.elseBranch, type, &elseType);
    mergeOwnershipStates(&thenStates, &elseStates);
    freeOwnershipStates(&thenStates);
    freeOwnershipStates(&elseStates);

    if (!ast->conditional.expression) {
        ast->conditional.typeId = TYPE_VOID;

        return;
    }

    if (!ast->conditional.elseBranch) {
        semanticTokenError("If expression requires an else branch ", ast->conditional.token);
    }

    if (thenType == TYPE_VOID || elseType == TYPE_VOID) {
        semanticTokenError("Conditional expression branch must produce a value ", ast->conditional.token);
    }

    if (thenType != elseType) {
        semanticTokenError("Conditional expression branches have incompatible types ", ast->conditional.token);
    }

    bool hasExpectedType = ast->conditional.typeId != TYPE_UNKNOWN
        && ast->conditional.typeId != TYPE_VOID;
    ASTNode* thenResult = getBranchResult(ast->conditional.thenBranch);
    ASTNode* elseResult = getBranchResult(ast->conditional.elseBranch);
    mergeBranchReference(ast, thenResult, true, hasExpectedType, ast->conditional.token);
    mergeBranchReference(ast, elseResult, false, hasExpectedType, ast->conditional.token);

    ast->conditional.typeId = thenType;
}

static bool matchHasWildcard(ASTNode* ast)
{
    size_t count = countVector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = getVectorAt(&ast->match.arms, i);
        if (!arm->pattern && !arm->guard) {
            return true;
        }
    }

    return false;
}

static bool updateAlternativeBooleanCoverage(ASTNode* pattern, bool* coversFalse, bool* coversTrue)
{
    if (pattern->type != AST_BINARY || pattern->binary.operator.type != TOKEN_PIPE) {
        return false;
    }

    MatchArm leftArm = { .pattern = pattern->binary.leftExpr };
    MatchArm rightArm = { .pattern = pattern->binary.rightExpr };
    
    updateBooleanMatchCoverage(&leftArm, coversFalse, coversTrue);
    updateBooleanMatchCoverage(&rightArm, coversFalse, coversTrue);

    return true;
}

static void updateBooleanMatchCoverage(MatchArm* arm, bool* coversFalse, bool* coversTrue)
{
    if (arm->guard) {
        return;
    }

    ASTNode* pattern = arm->pattern;
    if (!pattern) {
        return;
    }

    if (updateAlternativeBooleanCoverage(pattern, coversFalse, coversTrue)) {
        return;
    }

    if (pattern->type != AST_BOOLEAN) {
        return;
    }

    if (pattern->booleanLiteral.value) {
        *coversTrue = true;

        return;
    }

    *coversFalse = true;
}

static bool matchHasBooleanSubject(ASTNode* ast)
{
    if (!ast->match.subject) {
        return true;
    }

    return getNodeDataType(ast->match.subject) == TYPE_BOOL;
}

static bool matchCoversBothBooleans(ASTNode* ast)
{
    if (!matchHasBooleanSubject(ast)) {
        return false;
    }

    bool coversFalse = false;
    bool coversTrue = false;
    size_t count = countVector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = getVectorAt(&ast->match.arms, i);
        updateBooleanMatchCoverage(arm, &coversFalse, &coversTrue);
    }

    return coversFalse && coversTrue;
}

static bool matchIsExhaustive(ASTNode* ast)
{
    return ast->match.defaultBranch || matchHasWildcard(ast) || matchCoversBothBooleans(ast);
}

static void mergeMatchBranchType(ASTNode* ast, DataType branchType, DataType* resultType)
{
    if (branchType == TYPE_VOID) {
        semanticTokenError("Match expression arm must produce a value ", ast->match.token);
    }

    if (*resultType == TYPE_UNKNOWN) {
        *resultType = branchType;

        return;
    }

    if (*resultType != branchType) {
        semanticTokenError("Match expression arms have incompatible types ", ast->match.token);
    }
}

static void analyzeMatchPatternNode(Analyzer* analyzer, ASTNode* ast, ASTNode* pattern)
{
    if (pattern->type == AST_BINARY && pattern->binary.operator.type == TOKEN_PIPE) {
        analyzeMatchPatternNode(analyzer, ast, pattern->binary.leftExpr);
        analyzeMatchPatternNode(analyzer, ast, pattern->binary.rightExpr);

        return;
    }

    analyzeNode(analyzer, pattern);
    DataType expected = ast->match.subject ? getNodeDataType(ast->match.subject) : TYPE_BOOL;

    if (isIntegerType(expected)) {
        applyIntegerLiteralType(pattern, expected);
    }

    if (expected == TYPE_STRING) {
        semanticTokenError("String match patterns are not supported ", ast->match.token);
    }

    if (getNodeDataType(pattern) != expected) {
        semanticTokenError("Match pattern type does not match subject ", ast->match.token);
    }
}

static void analyzeMatchPattern(Analyzer* analyzer, ASTNode* ast, MatchArm* arm)
{
    if (!arm->pattern) {
        return;
    }

    analyzeMatchPatternNode(analyzer, ast, arm->pattern);
}

static void restoreMatchContinuation(Analyzer* analyzer, Vector* continuationNodes, size_t continuationStart)
{
    analyzer->currentNodes = continuationNodes;
    analyzer->nextNode = continuationStart;
}

static void mergeMatchResult(ASTNode* ast, ASTNode* branch, DataType branchType, DataType* resultType)
{
    if (!ast->match.expression) {
        return;
    }

    bool first = *resultType == TYPE_UNKNOWN;
    bool hasExpectedType = ast->match.typeId != TYPE_UNKNOWN && ast->match.typeId != TYPE_VOID;
    ASTNode* result = getBranchResult(branch);
    
    mergeBranchReference(ast, result, first, hasExpectedType, ast->match.token);
    mergeMatchBranchType(ast, branchType, resultType);
}

static void analyzeMatchGuard(Analyzer* analyzer, MatchArm* arm, Vector* fallthroughStates)
{
    if (!arm->guard) {
        return;
    }

    analyzeNode(analyzer, arm->guard);

    if (getNodeDataType(arm->guard) != TYPE_BOOL) {
        semanticError("Match guard requires a bool operand", arm->guardToken);
    }

    Vector guardStates;

    copyOwnershipState(&guardStates, fallthroughStates);
    updateOwnershipStates(&guardStates);

    if (arm->pattern) {
        mergeOwnershipStates(fallthroughStates, &guardStates);
        updateOwnershipStates(fallthroughStates);
        applyOwnershipStates(&guardStates);
    } else {
        updateOwnershipStates(fallthroughStates);
    }

    releaseReferencesDeadOnBranch(analyzer, &guardStates, arm->branch, arm->binding);
    freeOwnershipStates(&guardStates);
}

static void analyzeMatchArm(Analyzer* analyzer, ASTNode* ast, MatchArm* arm, Vector* continuationNodes,
    size_t continuationStart, DataType* resultType, Vector* fallthroughStates)
{
    restoreMatchContinuation(analyzer, continuationNodes, continuationStart);
    analyzeMatchPattern(analyzer, ast, arm);

    if (arm->binding) {
        arm->binding->variableDefinition.typeId = getNodeDataType(ast->match.subject);
    }

    Scope* previousScope = enterBranchScope(analyzer, arm->branch, arm->binding);

    analyzeMatchGuard(analyzer, arm, fallthroughStates);

    DataType branchType = analyzeBranchContents(analyzer, arm->branch, ast->match.typeId);
    analyzer->currentScope = previousScope;

    mergeMatchResult(ast, arm->branch, branchType, resultType);
}

static void analyzeMatchArmOwnership(Analyzer* analyzer, ASTNode* ast, MatchArm* arm,
    Vector* continuationNodes, size_t continuationStart, DataType* resultType,
    Vector* initialStates, Vector* mergedStates, bool* mergedInitialized)
{
    Vector states;
    copyOwnershipState(&states, initialStates);
    applyOwnershipStates(&states);

    if (!arm->guard) {
        releaseReferencesDeadOnBranch(analyzer, &states, arm->branch, arm->binding);
    }

    analyzeMatchArm(analyzer, ast, arm, continuationNodes, continuationStart, resultType, initialStates);
    updateOwnershipStates(&states);
    mergeOwnershipOutcome(mergedStates, &states, mergedInitialized);
    freeOwnershipStates(&states);
}

static void analyzeMatchArms(Analyzer* analyzer, ASTNode* ast, Vector* continuationNodes, size_t continuationStart,
    DataType* resultType, Vector* initialStates, Vector* mergedStates, bool* mergedInitialized)
{
    size_t count = countVector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = getVectorAt(&ast->match.arms, i);
        analyzeMatchArmOwnership(analyzer, ast, arm, continuationNodes, continuationStart, resultType,
            initialStates, mergedStates, mergedInitialized);
    }
}

static void analyzeMatchSubject(Analyzer* analyzer, ASTNode* ast)
{
    if (!ast->match.subject) {
        return;
    }

    analyzeNode(analyzer, ast->match.subject);
}

static void analyzeMatchDefault(Analyzer* analyzer, ASTNode* ast, Vector* continuationNodes,
    size_t continuationStart, DataType* resultType, Vector* initialStates,
    Vector* mergedStates, bool* mergedInitialized)
{
    if (!ast->match.defaultBranch) {
        return;
    }

    Vector states;

    copyOwnershipState(&states, initialStates);
    applyOwnershipStates(&states);
    restoreMatchContinuation(analyzer, continuationNodes, continuationStart);
    releaseReferencesDeadOnBranch(analyzer, &states, ast->match.defaultBranch, NULL);

    DataType branchType = analyzeScopedBranch(analyzer, ast->match.defaultBranch, NULL, ast->match.typeId);
    
    mergeMatchResult(ast, ast->match.defaultBranch, branchType, resultType);
    updateOwnershipStates(&states);
    mergeOwnershipOutcome(mergedStates, &states, mergedInitialized);
    freeOwnershipStates(&states);
}

static void finalizeMatchType(ASTNode* ast, DataType resultType)
{
    if (!ast->match.expression) {
        ast->match.typeId = TYPE_VOID;

        return;
    }

    if (!matchIsExhaustive(ast)) {
        semanticTokenError("Match expression must be exhaustive ", ast->match.token);
    }

    ast->match.typeId = resultType;
}

static void finalizeMatchOwnership(ASTNode* ast, Vector* initialStates,
    Vector* mergedStates, bool* mergedInitialized)
{
    if (!matchIsExhaustive(ast)) {
        mergeOwnershipOutcome(mergedStates, initialStates, mergedInitialized);
    }

    applyOwnershipStates(mergedStates);
    freeOwnershipStates(mergedStates);
    freeOwnershipStates(initialStates);
}

static void validateMatchExpression(ASTNode* ast)
{
    if (!ast->match.expression) {
        return;
    }

    size_t count = countVector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = getVectorAt(&ast->match.arms, i);
        validateExpressionBranch(arm->branch, ast->match.token);
    }

    if (ast->match.defaultBranch) {
        validateExpressionBranch(ast->match.defaultBranch, ast->match.token);
    }
}

static void analyzeMatch(Analyzer* analyzer, ASTNode* ast)
{
    Vector* continuationNodes = analyzer->currentNodes;
    size_t continuationStart = analyzer->nextNode;
    DataType resultType = TYPE_UNKNOWN;
    Vector initialStates;
    Vector mergedStates;
    bool mergedInitialized = false;

    validateMatchExpression(ast);
    analyzeMatchSubject(analyzer, ast);
    captureOwnershipState(&initialStates, analyzer->currentScope);
    analyzeMatchArms(analyzer, ast, continuationNodes, continuationStart, &resultType,
        &initialStates, &mergedStates, &mergedInitialized);
    analyzeMatchDefault(analyzer, ast, continuationNodes, continuationStart, &resultType,
        &initialStates, &mergedStates, &mergedInitialized);
    restoreMatchContinuation(analyzer, continuationNodes, continuationStart);
    finalizeMatchOwnership(ast, &initialStates, &mergedStates, &mergedInitialized);
    finalizeMatchType(ast, resultType);
}

static DataType getFloatBinaryType(DataType leftType, DataType rightType)
{
    if (leftType == TYPE_F64 || rightType == TYPE_F64) {
        return TYPE_F64;
    }

    return TYPE_F32;
}

static void applyBinaryFloatLiteralTypes(ASTNode* left, ASTNode* right)
{
    DataType leftType = getNodeDataType(left);
    DataType rightType = getNodeDataType(right);

    if (!isFloatType(leftType) || !isFloatType(rightType)) {
        return;
    }

    DataType type = getFloatBinaryType(leftType, rightType);

    if (!getFloatLiteral(left)) {
        type = leftType;
    }

    if (!getFloatLiteral(right)) {
        type = rightType;
    }

    applyLiteralType(left, type);
    applyLiteralType(right, type);
}

static void applyBinaryIntegerLiteralTypes(ASTNode* left, ASTNode* right)
{
    DataType leftType = getNodeDataType(left);
    DataType rightType = getNodeDataType(right);
    bool negative;
    ASTNode* leftLiteral = getIntegerLiteral(left, &negative);
    ASTNode* rightLiteral = getIntegerLiteral(right, &negative);
    bool sameSignedness = isSignedIntegerType(leftType) == isSignedIntegerType(rightType);
    bool widenLeft = sameSignedness && canImplicitlyWidenInteger(leftType, rightType);

    if (leftLiteral && rightLiteral && widenLeft) {
        applyIntegerLiteralType(left, rightType);

        leftType = rightType;
    }

    if (isIntegerType(leftType)) {
        applyIntegerLiteralType(right, leftType);

        rightType = getNodeDataType(right);
    }

    if (isIntegerType(rightType)) {
        applyIntegerLiteralType(left, rightType);
    }
}

static void analyzeBinary(Analyzer* analyzer, ASTNode* ast)
{
    analyzeNode(analyzer, ast->binary.leftExpr);
    analyzeNode(analyzer, ast->binary.rightExpr);

    DataType leftType = getNodeDataType(ast->binary.leftExpr);
    DataType rightType = getNodeDataType(ast->binary.rightExpr);
    TokenType operatorType = ast->binary.operator.type;

    bool comparison = isComparisonToken(operatorType) || isEqualityToken(operatorType);

    if (comparison) {
        applyBinaryFloatLiteralTypes(ast->binary.leftExpr, ast->binary.rightExpr);
    }

    bool leftText = leftType == TYPE_CHAR || leftType == TYPE_STRING;
    bool rightText = rightType == TYPE_CHAR || rightType == TYPE_STRING;

    if (ast->binary.operator.type == TOKEN_PLUS && leftText && rightText) {
        ast->binary.typeId = TYPE_STRING;

        return;
    }

    applyBinaryIntegerLiteralTypes(ast->binary.leftExpr, ast->binary.rightExpr);

    leftType = getNodeDataType(ast->binary.leftExpr);
    rightType = getNodeDataType(ast->binary.rightExpr);
    DataType commonType = getCommonNumericType(leftType, rightType);
    bool floatOperands = isFloatType(commonType);

    if (leftType != rightType && commonType == TYPE_UNKNOWN) {
        binaryTypeError(ast->binary.operator, leftType, rightType);
    }

    if (commonType == TYPE_UNKNOWN) {
        commonType = leftType;
    }

    ast->binary.operandType = commonType;

    if (leftType == TYPE_STRING) {
        semanticError("Invalid operator for string operands", ast->binary.operator);
    }

    if (leftType == TYPE_CHAR && !isCharacterComparisonToken(ast->binary.operator.type)) {
        semanticError("Invalid operator for char operands", ast->binary.operator);
    }

    if (isLogicalToken(ast->binary.operator.type) && leftType != TYPE_BOOL) {
        semanticError("Logical operators require bool operands", ast->binary.operator);
    }

    if (isComparisonToken(ast->binary.operator.type) && !isIntegerType(leftType) && !floatOperands && leftType != TYPE_CHAR) {
        semanticError("Comparison operators require compatible operand types", ast->binary.operator);
    }

    if (ast->binary.operator.type == TOKEN_POWER && leftType != TYPE_I32) {
        semanticError("Invalid operands for binary operator", ast->binary.operator);
    }

    if (floatOperands && !isTermToken(ast->binary.operator.type)
        && !isFactorToken(ast->binary.operator.type) && !isComparisonToken(ast->binary.operator.type)
        && !isEqualityToken(ast->binary.operator.type)) {
        semanticError("Invalid operands for binary operator", ast->binary.operator);
    }

    ast->binary.typeId = commonType;

    if (ast->binary.operator.type == TOKEN_SPACESHIP) {
        ast->binary.typeId = TYPE_I32;
    }

    if (isBoolOperatorToken(ast->binary.operator.type)) {
        ast->binary.typeId = TYPE_BOOL;
    }
}

static void trackVariableRead(ASTNode* symbol)
{
    if (!isVariableDefinition(symbol)) {
        return;
    }

    symbol->variableDefinition.readCount++;
}

static void validateConstantBinding(Analyzer* analyzer, ASTNode* symbol, Token token)
{
    if (!analyzer->constantScope || (isVariableDefinition(symbol) && symbol->variableDefinition.constant)) {
        return;
    }

    Scope* scope = getScope(symbol);

    if (scope->level > analyzer->constantScope->level) {
        return;
    }

    semanticTokenError("Cannot access runtime binding in constant expression ", token);
}

static void validateConstantReference(Analyzer* analyzer, ASTNode* symbol, Token token)
{
    if (!analyzer->constantScope || !symbol) {
        return;
    }

    Scope* scope = getScope(symbol);
    bool calledFunction = analyzer->functionPath != analyzer->constantFunctions;

    if (scope->level > analyzer->constantScope->level || (calledFunction && !isTopLevelScope(scope))) {
        return;
    }

    semanticTokenError("Cannot reference runtime binding in constant expression ", token);
}

static void validateConstantMutation(ASTNode* symbol, ReferenceType type, Token token)
{
    if (type == REFERENCE_EXCLUSIVE && isVariableDefinition(symbol) && symbol->variableDefinition.constant) {
        semanticTokenError("Cannot mutate constant ", token);
    }
}

static void analyzeVariable(Analyzer* analyzer, ASTNode* ast)
{
    StringView id = getStringView(ast->variable.id);
    ASTNode* symbol = findSymbol(analyzer, id);

    if (!symbol || !isVariableType(symbol)) {
        symbolError(" is undefined", ast->variable.token);
    }

    validateConstantBinding(analyzer, symbol, ast->variable.token);

    if (!isInitialized(symbol)) {
        symbolError(" is uninitialized", ast->variable.token);
    }

    if (isMoved(symbol)) {
        symbolError(" is moved", ast->variable.token);
    }

    if (getReferenceType(symbol) == REFERENCE_NONE
        && hasExclusiveAccess(symbol)) {
        bindingAccessError("Cannot read binding ", ast->variable.token, " while exclusive access is active");
    }

    ast->variable.scope = analyzer->currentScope;
    ast->variable.symbol = symbol;
    trackVariableRead(symbol);
}

static ASTNode* getReferenceVariable(ASTNode* ast)
{
    if (!isVariable(ast->prefix.expr)) {
        semanticError("References require a binding", ast->prefix.operator);
    }

    return ast->prefix.expr;
}

static ASTNode* findReferenceSymbol(Analyzer* analyzer, ASTNode* variable)
{
    StringView id = getStringView(variable->variable.id);
    ASTNode* symbol = findSymbol(analyzer, id);

    if (!symbol || !isVariableType(symbol)) {
        symbolError(" is undefined", variable->variable.token);
    }

    if (!isInitialized(symbol)) {
        symbolError(" is uninitialized", variable->variable.token);
    }

    if (isMoved(symbol)) {
        symbolError(" is moved", variable->variable.token);
    }

    return symbol;
}

static void validateReferenceAccess(ASTNode* ast, ASTNode* variable, ASTNode* symbol)
{
    ReferenceType requested = getReferenceType(ast);
    ReferenceType existing = getReferenceType(symbol);

    validateConstantMutation(symbol, requested, variable->variable.token);

    if (existing != REFERENCE_NONE) {
        semanticError("Cannot create a reference to another reference", ast->prefix.operator);
    }

    if (requested == REFERENCE_SHARED && hasExclusiveAccess(symbol)) {
        bindingAccessError("Cannot share binding ", variable->variable.token, " while exclusive access is active");
    }

    if (requested == REFERENCE_EXCLUSIVE
        && (hasExclusiveAccess(symbol) || *getSharedAccessCount(symbol))) {
        bindingAccessError("Cannot take exclusive access to binding ", variable->variable.token, " while another access is active");
    }
}

static void markAddressTaken(ASTNode* symbol)
{
    if (!symbol || !isVariableDefinition(symbol)) {
        return;
    }

    symbol->variableDefinition.addressTaken = true;
}

static void analyzeReference(Analyzer* analyzer, ASTNode* ast)
{
    ASTNode* variable = getReferenceVariable(ast);
    ASTNode* symbol = findReferenceSymbol(analyzer, variable);

    validateConstantBinding(analyzer, symbol, variable->variable.token);
    validateConstantReference(analyzer, symbol, variable->variable.token);
    validateReferenceAccess(ast, variable, symbol);
    markAddressTaken(symbol);

    variable->variable.scope = analyzer->currentScope;
    variable->variable.symbol = symbol;
    Vector* origins = getReferenceOrigins(ast);
    
    addReferenceOrigin(origins, symbol);
    trackVariableRead(symbol);
}

static void analyzeParameter(Analyzer* analyzer, ASTNode* ast)
{
    StringView id = getStringView(ast->parameter.id);

    if (getLocalSymbol(analyzer->currentScope, id)) {
        redefinitionError("Parameter", ast->parameter.token);
    }

    ast->parameter.scope = analyzer->currentScope;

    if (getReferenceType(ast) != REFERENCE_NONE) {
        Vector* origins = getReferenceOrigins(ast);
        addReferenceOrigin(origins, ast);
    }

    setLocalSymbol(analyzer->currentScope, id, ast);
}

static void validateTypedVariadicArgument(ASTNode* argument, DataType type, Token token)
{
    applyLiteralType(argument, type);

    DataType argumentType = getNodeDataType(argument);

    if (argumentType != type && !canImplicitlyWidenType(argumentType, type)) {
        semanticTokenError("Variadic arguments must convert losslessly to the parameter type ", token);
    }
}

static void validateTypedVariadicArguments(Vector* args, const Builtin* builtin, Token token)
{
    if (!args->count) {
        semanticTokenError("Numeric variadic builtins require at least one argument ", token);
    }

    for (size_t i = 0; i < args->count; i++) {
        validateTypedVariadicArgument(args->data[i], builtin->params[0], token);
    }
}

static void validateVariadicArguments(Vector* args, const Builtin* builtin, Token token)
{
    if (builtin->paramCount == BUILTIN_TYPED_VARIADIC) {
        validateTypedVariadicArguments(args, builtin, token);

        return;
    }

    char* message = "Invalid value in string construction ";

    if (builtin->entry == builtinPrint) {
        message = "Invalid value in print ";
    }

    for (size_t i = 0; i < args->count; i++) {
        ASTNode* argument = args->data[i];
        DataType type = getNodeDataType(argument);
        bool valid = isPrintableType(type);

        if (!valid) {
            semanticTokenError(message, token);
        }
    }
}

static void validateBuiltinCall(ASTNode* ast, const Builtin* builtin, Token token)
{
    size_t count = countVector(&ast->functionCall.args);

    if (builtin->paramCount < 0) {
        validateVariadicArguments(&ast->functionCall.args, builtin, token);

        return;
    }

    if (count != (size_t)builtin->paramCount) {
        semanticTokenError("Invalid arguments to function ", token);
    }

    for (size_t i = 0; i < count; i++) {
        ASTNode* arg = getVectorAt(&ast->functionCall.args, i);
        applyLiteralType(arg, builtin->params[i]);

        DataType argumentType = getNodeDataType(arg);
        DataType parameterType = builtin->params[i];

        if (argumentType != parameterType
            && !canImplicitlyWidenType(argumentType, parameterType)) {
            semanticTokenError("Invalid arguments to function ", token);
        }
    }
}

static void convertBuiltinCall(ASTNode* ast, const Builtin* builtin)
{
    Vector args = ast->functionCall.args;
    Token token = ast->functionCall.token;

    freeStringObject(ast->functionCall.id);

    ast->type = AST_BUILTIN_CALL;
    ast->builtinCall.builtin = builtin;
    ast->builtinCall.args = args;
    ast->builtinCall.token = token;
}

static bool isNumericLiteralArgument(ASTNode* argument)
{
    bool negative;
    ASTNode* integer = getIntegerLiteral(argument, &negative);
    ASTNode* floating = getFloatLiteral(argument);

    return integer || floating;
}

static DataType getNonliteralArgumentType(ASTNode* argument)
{
    bool literal = isNumericLiteralArgument(argument);

    if (literal) {
        return TYPE_UNKNOWN;
    }

    return getNodeDataType(argument);
}

static bool variadicArgumentFitsType(ASTNode* argument, DataType type)
{
    bool negative;
    ASTNode* integer = getIntegerLiteral(argument, &negative);

    if (integer) {
        bool fitsInteger = integerLiteralFitsType(integer->integerLiteral.value, negative, type);
        bool fitsFloat = integerLiteralFitsFloat(integer->integerLiteral.value, type);

        return fitsInteger || fitsFloat;
    }

    ASTNode* floating = getFloatLiteral(argument);

    if (floating) {
        bool numeric = isFloatType(type);
        bool fits = type == TYPE_F64 || floating->floatLiteral.value <= FLT_MAX;

        return numeric && fits;
    }

    DataType argumentType = getNodeDataType(argument);

    return argumentType == type || canImplicitlyWidenType(argumentType, type);
}

static bool typedVariadicCallMatches(Vector* args, const Builtin* builtin)
{
    if (!builtin || builtin->paramCount != BUILTIN_TYPED_VARIADIC) {
        return false;
    }

    bool matches = true;

    for (size_t i = 0; i < args->count && matches; i++) {
        matches = variadicArgumentFitsType(args->data[i], builtin->params[0]);
    }

    return matches;
}

static const Builtin* resolveContextualVariadicCall(ASTNode* ast)
{
    Vector* args = &ast->functionCall.args;
    DataType type = TYPE_UNKNOWN;

    for (size_t i = 0; i < args->count && type == TYPE_UNKNOWN; i++) {
        type = getNonliteralArgumentType(args->data[i]);
    }

    if (type == TYPE_UNKNOWN) {
        return NULL;
    }

    const Builtin* builtin = resolveBuiltin(ast->functionCall.id->chars, &type, 1);
    bool matches = typedVariadicCallMatches(args, builtin);

    if (!matches) {
        return NULL;
    }

    return builtin;
}

static DataType getBuiltinArgumentType(ASTNode* argument)
{
    DataType type = getNodeDataType(argument);

    if (type == TYPE_UNKNOWN) {
        return TYPE_U64;
    }

    return type;
}

static const Builtin* resolveBuiltinCall(ASTNode* ast)
{
    const Builtin* contextual = resolveContextualVariadicCall(ast);

    if (contextual) {
        return contextual;
    }

    size_t count = ast->functionCall.args.count;
    DataType* argumentTypes = NULL;

    if (count > SIZE_MAX / sizeof(DataType)) {
        outOfMemoryError();
    }

    if (count) {
        argumentTypes = allocateMemory(count * sizeof(DataType));
    }

    for (size_t i = 0; i < count; i++) {
        argumentTypes[i] = getBuiltinArgumentType(ast->functionCall.args.data[i]);
    }

    const Builtin* builtin = resolveBuiltin(ast->functionCall.id->chars, argumentTypes, count);

    freeMemory(argumentTypes);

    return builtin;
}

static void validateBuiltinResolution(ASTNode* ast, const Builtin* builtin)
{
    if (builtin) {
        return;
    }

    if (isBuiltinName(ast->functionCall.id->chars)) {
        invalidBuiltinArgumentsError(ast->functionCall.token);
    }

    symbolError(" is undefined", ast->functionCall.token);
}

static void analyzeBuiltinCall(ASTNode* ast)
{
    const Builtin* builtin = resolveBuiltinCall(ast);

    validateBuiltinResolution(ast, builtin);
    validateBuiltinCall(ast, builtin, ast->functionCall.token);
    convertBuiltinCall(ast, builtin);
}

static void validateFunctionCall(ASTNode* caller, ASTNode* callee, Token token)
{
    size_t count = countVector(&caller->functionCall.args);
    size_t paramCount = countVector(&callee->functionDefinition.params);

    if (count != paramCount) {
        semanticTokenError("Invalid arguments to function ", token);
    }

    for (size_t i = 0; i < count; i++) {
        ASTNode* arg = getVectorAt(&caller->functionCall.args, i);
        ASTNode* param = getVectorAt(&callee->functionDefinition.params, i);
        applyLiteralType(arg, param->parameter.typeId);

        ReferenceType argumentType = getReferenceType(arg);
        ReferenceType parameterType = getReferenceType(param);
        bool valueParameter = parameterType == REFERENCE_NONE;
        bool implicitAccess = argumentType == REFERENCE_NONE;
        bool compatibleReference = valueParameter || implicitAccess || argumentType == parameterType
            || (argumentType == REFERENCE_EXCLUSIVE && parameterType == REFERENCE_SHARED);
        DataType argumentValueType = getNodeDataType(arg);
        DataType parameterValueType = param->parameter.typeId;
        bool compatibleValue = argumentValueType == parameterValueType
            || (argumentType == REFERENCE_NONE && parameterType == REFERENCE_NONE
                && canImplicitlyWidenType(argumentValueType, parameterValueType));

        if (!compatibleValue || !compatibleReference) {
            semanticTokenError("Invalid arguments to function ", token);
        }
    }
}

static void validateGlobalBindingAccess(Analyzer* analyzer, ASTNode* origin, ReferenceType type, Token token)
{
    validateConstantMutation(origin, type, token);

    if (!origin || !isTopLevelScope(getScope(origin))) {
        return;
    }

    validateConstantBinding(analyzer, origin, token);

    if (type == REFERENCE_SHARED && hasExclusiveAccess(origin)) {
        bindingAccessError("Function requires shared access to binding ", token, " while exclusive access is active");
    }

    if (type == REFERENCE_EXCLUSIVE
        && (hasExclusiveAccess(origin) || *getSharedAccessCount(origin))) {
        bindingAccessError("Function requires exclusive access to binding ", token, " while another access is active");
    }
}

static bool referenceArgumentRequiresBinding(ReferenceType type)
{
    return type == REFERENCE_EXCLUSIVE;
}

static void validateCallArgumentAccess(Analyzer* analyzer, ASTNode* caller, ASTNode* callee, Token token)
{
    size_t count = countVector(&caller->functionCall.args);

    for (size_t i = 0; i < count; i++) {
        ASTNode* param = getVectorAt(&callee->functionDefinition.params, i);
        ReferenceType type = getReferenceType(param);

        if (type == REFERENCE_NONE) {
            continue;
        }

        ASTNode* arg = getVectorAt(&caller->functionCall.args, i);

        if (getReferenceType(arg) != REFERENCE_NONE) {
            continue;
        }

        ASTNode* origin = referenceOriginFromExpression(arg);

        if (!origin && isVariable(arg)) {
            origin = getReferenceOriginOrSelf(arg->variable.symbol);
        }

        if (!origin && referenceArgumentRequiresBinding(type)) {
            semanticTokenError("Reference access requires a binding for ", token);
        }

        validateGlobalBindingAccess(analyzer, origin, type, token);
        validateConstantReference(analyzer, origin, token);
        markAddressTaken(origin);
    }
}

static bool functionInPath(FunctionPath* path, ASTNode* function)
{
    if (!path || path->function == function) {
        return path != NULL;
    }

    return functionInPath(path->previous, function);
}

static void validateGlobalAccessInNodes(Analyzer* analyzer, Vector* nodes)
{
    size_t count = countVector(nodes);

    for (size_t i = 0; i < count; i++) {
        ASTNode* node = getVectorAt(nodes, i);

        validateNodeGlobalAccess(analyzer, node);
    }
}

static void validateFunctionGlobalAccess(Analyzer* analyzer, ASTNode* function)
{
    if (analyzer->constantScope && functionInPath(analyzer->constantFunctions, function)) {
        semanticTokenError("Cannot evaluate incomplete function ", function->functionDefinition.token);
    }

    if (functionInPath(analyzer->functionPath, function)) {
        return;
    }

    FunctionPath path = {function, analyzer->functionPath};
    analyzer->functionPath = &path;

    validateGlobalAccessInNodes(analyzer, &function->functionDefinition.body->compound.statements);

    analyzer->functionPath = path.previous;
}

static void validateBranchGlobalAccess(Analyzer* analyzer, ASTNode* branch)
{
    if (!branch) {
        return;
    }

    if (branch->type == AST_CONDITIONAL) {
        validateNodeGlobalAccess(analyzer, branch);

        return;
    }

    validateGlobalAccessInNodes(analyzer, &branch->compound.statements);
}

static void validateMatchArmGlobalAccess(Analyzer* analyzer, MatchArm* arm)
{
    if (arm->pattern) {
        validateNodeGlobalAccess(analyzer, arm->pattern);
    }

    if (arm->guard) {
        validateNodeGlobalAccess(analyzer, arm->guard);
    }

    validateBranchGlobalAccess(analyzer, arm->branch);
}

static void validateMatchGlobalAccess(Analyzer* analyzer, ASTNode* ast)
{
    if (ast->match.subject) {
        validateNodeGlobalAccess(analyzer, ast->match.subject);
    }

    size_t count = countVector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = getVectorAt(&ast->match.arms, i);

        validateMatchArmGlobalAccess(analyzer, arm);
    }

    validateBranchGlobalAccess(analyzer, ast->match.defaultBranch);
}

static void validatePrefixGlobalAccess(Analyzer* analyzer, ASTNode* ast)
{
    ASTNode* origin = referenceOriginFromExpression(ast);
    ReferenceType type = getReferenceType(ast);

    if (type != REFERENCE_NONE) {
        validateConstantReference(analyzer, origin, ast->prefix.operator);
    }

    validateGlobalBindingAccess(analyzer, origin, type, ast->prefix.operator);
    validateNodeGlobalAccess(analyzer, ast->prefix.expr);
}

static void validateNodeGlobalAccess(Analyzer* analyzer, ASTNode* ast)
{
    switch (ast->type) {
        case AST_ASSIGNMENT: {
            ASTNode* origin = getReferenceOriginOrSelf(ast->assignment.symbol);
            validateGlobalBindingAccess(analyzer, origin, REFERENCE_EXCLUSIVE, ast->assignment.token);
            validateNodeGlobalAccess(analyzer, ast->assignment.expr);
            break;
        }
        case AST_BINARY:
            validateNodeGlobalAccess(analyzer, ast->binary.leftExpr);
            validateNodeGlobalAccess(analyzer, ast->binary.rightExpr);
            break;
        case AST_BUILTIN_CALL:
            validateGlobalAccessInNodes(analyzer, &ast->builtinCall.args);
            break;
        case AST_WHILE:
            validateNodeGlobalAccess(analyzer, ast->whileStatement.condition);
            validateBranchGlobalAccess(analyzer, ast->whileStatement.body);
            break;
        case AST_CONDITIONAL:
            validateNodeGlobalAccess(analyzer, ast->conditional.condition);
            validateBranchGlobalAccess(analyzer, ast->conditional.thenBranch);
            validateBranchGlobalAccess(analyzer, ast->conditional.elseBranch);
            break;
        case AST_MATCH:
            validateMatchGlobalAccess(analyzer, ast);
            break;
        case AST_CONVERSION:
            validateNodeGlobalAccess(analyzer, ast->conversion.expr);
            break;
        case AST_FUNCTION_CALL:
            validateCallArgumentAccess(analyzer, ast, ast->functionCall.symbol, ast->functionCall.token);
            validateGlobalAccessInNodes(analyzer, &ast->functionCall.args);
            validateFunctionGlobalAccess(analyzer, ast->functionCall.symbol);
            break;
        case AST_PREFIX:
            validatePrefixGlobalAccess(analyzer, ast);
            break;
        case AST_RETURN:
            validateNodeGlobalAccess(analyzer, ast->returnStatement.expr);
            break;
        case AST_VARIABLE: {
            ASTNode* origin = getReferenceOriginOrSelf(ast->variable.symbol);
            validateGlobalBindingAccess(analyzer, origin, REFERENCE_SHARED, ast->variable.token);
            break;
        }
        case AST_VARIABLE_DEFINITION:
            validateNodeGlobalAccess(analyzer, ast->variableDefinition.expr);
            break;
        default:
            break;
    }
}

static ReferenceType getArgumentReferenceType(ASTNode* argument, ASTNode* parameter)
{
    ReferenceType type = getReferenceType(argument);

    if (type != REFERENCE_NONE) {
        return type;
    }

    return getReferenceType(parameter);
}

static ASTNode* getArgumentReferenceOrigin(ASTNode* argument)
{
    ASTNode* origin = referenceOriginFromExpression(argument);

    if (origin || !isVariable(argument)) {
        return origin;
    }

    return getReferenceOriginOrSelf(argument->variable.symbol);
}

static bool referenceAccessConflicts(ASTNode* leftOrigin, ReferenceType leftType,
    ASTNode* right, ASTNode* rightParameter)
{
    ReferenceType rightType = getArgumentReferenceType(right, rightParameter);

    if (leftType != REFERENCE_EXCLUSIVE && rightType != REFERENCE_EXCLUSIVE) {
        return false;
    }

    Vector* rightOrigins = referenceOriginsFromExpression(right);
    if (containsReferenceOrigin(rightOrigins, leftOrigin)) {
        return true;
    }

    ASTNode* rightOrigin = getArgumentReferenceOrigin(right);

    return leftOrigin == rightOrigin;
}

static void validateArgumentOriginAccess(Vector* arguments, Vector* parameters,
    size_t position, ASTNode* leftOrigin, ReferenceType leftType, Token token)
{
    size_t count = countVector(arguments);

    for (size_t i = position + 1; i < count; i++) {
        ASTNode* right = getVectorAt(arguments, i);
        ASTNode* rightParameter = getVectorAt(parameters, i);

        if (!referenceAccessConflicts(leftOrigin, leftType, right, rightParameter)) {
            continue;
        }

        semanticTokenError("Conflicting reference access in call ", token);
    }
}

static void validateArgumentAccess(Vector* arguments, Vector* parameters,
    size_t position, Token token)
{
    ASTNode* left = getVectorAt(arguments, position);
    ASTNode* leftParameter = getVectorAt(parameters, position);
    ReferenceType leftType = getArgumentReferenceType(left, leftParameter);

    if (leftType == REFERENCE_NONE) {
        return;
    }

    Vector* origins = referenceOriginsFromExpression(left);
    size_t count = countVector(origins);

    for (size_t i = 0; i < count; i++) {
        ASTNode* origin = getVectorAt(origins, i);
        validateArgumentOriginAccess(arguments, parameters, position, origin, leftType, token);
    }

    if (count) {
        return;
    }

    ASTNode* origin = getArgumentReferenceOrigin(left);
    if (origin) {
        validateArgumentOriginAccess(arguments, parameters, position, origin, leftType, token);
    }
}

static void validateArgumentAccesses(ASTNode* caller, ASTNode* callee)
{
    Vector* arguments = &caller->functionCall.args;
    Vector* parameters = &callee->functionDefinition.params;
    size_t count = countVector(arguments);

    for (size_t i = 0; i < count; i++) {
        validateArgumentAccess(arguments, parameters, i, caller->functionCall.token);
    }
}

static void setCallReferenceOrigins(ASTNode* caller, ASTNode* callee)
{
    ASTNode* returned = callee->functionDefinition.returnReferenceOrigin;

    if (!returned || !isParameter(returned)) {
        Vector* origins = getReferenceOrigins(caller);
        addReferenceOrigin(origins, returned);

        return;
    }

    size_t count = countVector(&callee->functionDefinition.params);

    for (size_t i = 0; i < count; i++) {
        if (getVectorAt(&callee->functionDefinition.params, i) != returned) {
            continue;
        }

        ASTNode* argument = getVectorAt(&caller->functionCall.args, i);
        setReferenceOriginsFromExpression(caller, argument);

        return;
    }
}

static void inferConversionLiteral(ASTNode* ast)
{
    DataType type = getTokenDataType(ast->functionCall.token.type);
    size_t count = countVector(&ast->functionCall.args);

    if ((!isIntegerType(type) && !isFloatType(type) && type != TYPE_CHAR) || count != 1) {
        return;
    }

    ASTNode* expression = ast->functionCall.args.data[0];
    DataType source = getNodeDataType(expression);

    if (source == TYPE_UNKNOWN && expression->type == AST_INTEGER) {
        applyIntegerLiteralType(expression, TYPE_U64);
    }
}

static void analyzeTypeConversion(ASTNode* ast)
{
    Token token = ast->functionCall.token;
    DataType destination = getTokenDataType(token.type);
    size_t count = countVector(&ast->functionCall.args);

    if (count != 1) {
        semanticError("Explicit type conversion requires exactly one argument", token);
    }

    ASTNode* expression = ast->functionCall.args.data[0];
    DataType source = getNodeDataType(expression);
    bool supported = canExplicitlyConvertType(source, destination);
    ReferenceType reference = getReferenceType(expression);

    if (!supported) {
        unsupportedExplicitConversionError(token, source, destination);
    }

    if (reference != REFERENCE_NONE) {
        semanticError("Explicit type conversions require a value", token);
    }

    freeStringObject(ast->functionCall.id);
    freeVector(&ast->functionCall.args);
    freeVector(&ast->functionCall.referenceOrigins);

    ast->type = AST_CONVERSION;
    ast->conversion.expr = expression;
    ast->conversion.token = token;
    ast->conversion.typeId = destination;
}

static void analyzeFunctionCall(Analyzer* analyzer, ASTNode* ast)
{
    inferConversionLiteral(ast);
    analyzeExpressionNodes(analyzer, &ast->functionCall.args);

    DataType calledType = getTokenDataType(ast->functionCall.token.type);

    if (isIntegerType(calledType) || isFloatType(calledType) || calledType == TYPE_CHAR) {
        analyzeTypeConversion(ast);

        return;
    }

    StringView id = getStringView(ast->functionCall.id);
    ASTNode* symbol = findSymbol(analyzer, id);
    
    if (!symbol) {
        analyzeBuiltinCall(ast);
        return;
    }

    if (!isFunctionDefinition(symbol)) {
        symbolError(" is undefined", ast->functionCall.token);
    }

    ast->functionCall.scope = analyzer->currentScope;
    ast->functionCall.symbol = symbol;
    validateFunctionCall(ast, symbol, ast->functionCall.token);
    validateCallArgumentAccess(analyzer, ast, symbol, ast->functionCall.token);
    validateArgumentAccesses(ast, symbol);
    validateFunctionGlobalAccess(analyzer, symbol);
    setCallReferenceOrigins(ast, symbol);
}

static ASTNode* findFunctionReturn(Vector* statements)
{
    size_t count = countVector(statements);

    for (size_t i = 0; i < count; i++) {
        ASTNode* statement = getVectorAt(statements, i);
        ASTNode* result = findNodeReturn(statement);

        if (result) {
            return result;
        }
    }

    return NULL;
}

static ASTNode* findConditionalReturn(ASTNode* ast)
{
    ASTNode* result = findNodeReturn(ast->conditional.thenBranch);

    if (result) {
        return result;
    }

    return findNodeReturn(ast->conditional.elseBranch);
}

static ASTNode* findMatchReturn(ASTNode* ast)
{
    size_t count = countVector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = getVectorAt(&ast->match.arms, i);
        ASTNode* result = findNodeReturn(arm->branch);

        if (result) {
            return result;
        }
    }

    return findNodeReturn(ast->match.defaultBranch);
}

static ASTNode* findNodeReturn(ASTNode* ast)
{
    if (!ast) {
        return NULL;
    }

    switch (ast->type) {
        case AST_RETURN:
            return ast;
        case AST_COMPOUND:
            return findFunctionReturn(&ast->compound.statements);
        case AST_WHILE:
            return findNodeReturn(ast->whileStatement.body);
        case AST_CONDITIONAL:
            return findConditionalReturn(ast);
        case AST_MATCH:
            return findMatchReturn(ast);
        default:
            return NULL;
    }
}

static DataType getFunctionReturnValueType(Vector* statements)
{
    ASTNode* result = findFunctionReturn(statements);

    if (!result) {
        return TYPE_VOID;
    }

    return getNodeDataType(result->returnStatement.expr);
}

static void validateFunctionReturnValueType(ASTNode* ast, DataType type)
{
    DataType returnType = ast->functionDefinition.returnTypeId;

    bool valueReturn = ast->functionDefinition.returnReferenceType == REFERENCE_NONE;

    if (returnType != type
        && (!valueReturn || !canImplicitlyWidenType(type, returnType))) {
        semanticTokenError("Invalid return type for function ", ast->functionDefinition.token);
    }
}

static void setParameterPositions(ASTNode* ast)
{
    size_t count = countVector(&ast->functionDefinition.params);
    size_t position = 0;

    for (size_t i = 0; i < count; i++) {
        ASTNode* param = getVectorAt(&ast->functionDefinition.params, i);
        param->parameter.position = position;
        position += getReferenceType(param) == REFERENCE_NONE ? getTypeSlotCount(getNodeDataType(param)) : 1;
    }
}

static void resolveFunctionReturnValueType(ASTNode* ast)
{
    DataType type = getFunctionReturnValueType(&ast->functionDefinition.body->compound.statements);

    if (ast->functionDefinition.hasExplicitReturnType) {
        validateFunctionReturnValueType(ast, type);
        return;
    }

    ast->functionDefinition.returnTypeId = type;
}

static void analyzeFunction(Analyzer* analyzer, ASTNode* ast)
{
    StringView id = getStringView(ast->functionDefinition.id);

    if (getLocalSymbol(analyzer->currentScope, id)) {
        redefinitionError("Function", ast->functionDefinition.token);
    }

    Scope* parent = analyzer->currentScope;
    ASTNode* previousFunction = analyzer->function;
    LoopPath* previousLoop = analyzer->loopPath;
    Scope* scope = createScope(parent);
    scope->frameScope = scope;
    scope->localOffset = 0;
    scope->maxLocalCount = 0;
    ast->functionDefinition.scope = scope;
    ast->functionDefinition.body->compound.scope = scope;
    analyzer->currentScope = scope;
    analyzer->function = ast;
    analyzer->loopPath = NULL;

    setLocalSymbol(scope, id, ast);
    analyzeExpressionNodes(analyzer, &ast->functionDefinition.params);
    setParameterPositions(ast);
    setLocalSymbol(parent, id, ast);

    FunctionPath path = {ast, analyzer->functionPath};
    analyzer->functionPath = &path;

    analyzeExpressionNodes(analyzer, &ast->functionDefinition.body->compound.statements);

    analyzer->functionPath = path.previous;
    analyzer->currentScope = parent;
    analyzer->function = previousFunction;
    analyzer->loopPath = previousLoop;

    resolveFunctionReturnValueType(ast);
}

static ASTNode* findAssignmentSymbol(Analyzer* analyzer, ASTNode* ast)
{
    Token token = ast->assignment.token;
    StringView id = createStringView(token.chars, token.length);
    ASTNode* symbol = findSymbol(analyzer, id);

    if (!symbol || !isVariableType(symbol)) {
        symbolError(" is undefined", ast->assignment.token);
    }

    return symbol;
}

static void validateCompoundAssignmentTarget(ASTNode* ast, ASTNode* symbol)
{
    if (ast->assignment.operator.type == TOKEN_EQUAL) {
        return;
    }

    if (!isInitialized(symbol)) {
        symbolError(" is uninitialized", ast->assignment.token);
    }

    if (getReferenceType(symbol) == REFERENCE_SHARED) {
        semanticTokenError("Cannot mutate through shared reference ", ast->assignment.token);
    }
}

static void validateAssignmentTarget(ASTNode* ast, ASTNode* symbol)
{
    Token token = ast->assignment.token;

    if (isVariableDefinition(symbol) && symbol->variableDefinition.constant) {
        semanticTokenError("Cannot reassign constant ", token);
    }

    if (isMoved(symbol)) {
        symbolError(" is moved", token);
    }

    if (getReferenceType(symbol) == REFERENCE_NONE && hasExclusiveAccess(symbol)) {
        bindingAccessError("Cannot mutate binding ", token, " while exclusive access is active");
    }

    if (getReferenceType(symbol) == REFERENCE_NONE && *getSharedAccessCount(symbol)) {
        bindingAccessError("Cannot mutate binding ", token, " while shared access is active");
    }

    if (getReferenceType(symbol) == REFERENCE_NONE && isVariableDefinition(symbol)
        && symbol->variableDefinition.fixed && isInitialized(symbol)) {
        semanticTokenError("Cannot reassign fixed binding ", token);
    }
}

static void validateAssignmentType(ASTNode* ast, ASTNode* symbol)
{
    ASTNode* expression = ast->assignment.expr;
    DataType expressionType = getNodeDataType(expression);
    DataType bindingType = getNodeDataType(symbol);
    ReferenceType bindingReferenceType = getReferenceType(symbol);
    ReferenceType expressionReferenceType = getReferenceType(expression);

    if (ast->assignment.operator.type != TOKEN_EQUAL && bindingReferenceType == REFERENCE_EXCLUSIVE) {
        bindingReferenceType = REFERENCE_NONE;
    }

    bool compatibleValue = bindingType == expressionType || (bindingReferenceType == REFERENCE_NONE
        && expressionReferenceType == REFERENCE_NONE && canImplicitlyWidenType(expressionType, bindingType));

    if (compatibleValue && bindingReferenceType == expressionReferenceType) {
        return;
    }

    if (bindingReferenceType == REFERENCE_NONE && expressionReferenceType == REFERENCE_NONE) {
        implicitConversionError(ast->assignment.token, expressionType, bindingType);
    }

    semanticTokenError("Assignment type does not match binding ", ast->assignment.token);
}

static void updateReferenceBinding(ASTNode* ast, ASTNode* symbol, bool initializesBinding)
{
    if (ast->assignment.operator.type != TOKEN_EQUAL || getReferenceType(symbol) == REFERENCE_NONE) {
        return;
    }

    setReferenceOriginsFromExpression(symbol, ast->assignment.expr);

    if (getReferenceType(symbol) == REFERENCE_EXCLUSIVE && isVariable(ast->assignment.expr)) {
        transferExclusiveReference(ast->assignment.expr);
    } else {
        activateReferenceAccess(symbol);
    }

    symbol->variableDefinition.referenceAccessActive = true;
    ast->assignment.initializesBinding = initializesBinding;
}

static void analyzeAssignment(Analyzer* analyzer, ASTNode* ast)
{
    ASTNode* symbol = findAssignmentSymbol(analyzer, ast);
    bool initializesBinding = !isInitialized(symbol);

    validateConstantBinding(analyzer, symbol, ast->assignment.token);
    validateAssignmentTarget(ast, symbol);
    validateCompoundAssignmentTarget(ast, symbol);
    trackVariableRead(symbol);

    if (ast->assignment.operator.type == TOKEN_EQUAL && isVariableDefinition(symbol)
        && !initializesBinding && symbol->variableDefinition.referenceAccessActive) {
        releaseReferenceAccess(symbol);
    }

    applyLiteralType(ast->assignment.expr, getNodeDataType(symbol));
    analyzeNode(analyzer, ast->assignment.expr);
    validateAssignmentType(ast, symbol);

    DataType bindingType = getNodeDataType(symbol);

    if (bindingType == TYPE_STRING && ast->assignment.operator.type != TOKEN_EQUAL) {
        semanticError("Invalid operator for string assignment", ast->assignment.operator);
    }

    if (bindingType == TYPE_CHAR && ast->assignment.operator.type != TOKEN_EQUAL) {
        semanticError("Invalid operator for char assignment", ast->assignment.operator);
    }

    if (isFloatType(getNodeDataType(symbol))
        && !isFloatAssignmentToken(ast->assignment.operator.type)) {
        semanticError("Invalid operands for assignment", ast->assignment.operator);
    }

    if (ast->assignment.operator.type == TOKEN_POWER_EQUAL && getNodeDataType(symbol) != TYPE_I32) {
        semanticError("Invalid operands for assignment", ast->assignment.operator);
    }

    ast->assignment.scope = analyzer->currentScope;
    ast->assignment.symbol = symbol;

    updateReferenceBinding(ast, symbol, initializesBinding);
    initializeVariable(symbol);
}

static void transferExclusiveReference(ASTNode* expression)
{
    ASTNode* source = expression->variable.symbol;
    setMoved(source);

    if (!isVariableDefinition(source)) {
        return;
    }

    source->variableDefinition.referenceAccessActive = false;
}

static void activateReferenceAccess(ASTNode* ast)
{
    Vector* origins = getReferenceOrigins(ast);
    size_t count = countVector(origins);

    for (size_t i = 0; i < count; i++) {
        ASTNode* origin = getVectorAt(origins, i);

        if (ast->variableDefinition.referenceType == REFERENCE_SHARED) {
            (*getSharedAccessCount(origin))++;
        } else {
            setExclusiveAccess(origin, true);
        }
    }
}

static void initializeVariableDefinition(Analyzer* analyzer, ASTNode* ast)
{
    ast->variableDefinition.scope = analyzer->currentScope;
    ast->variableDefinition.position = getNextLocalPosition(analyzer->currentScope);
    ast->variableDefinition.initialized = false;
}

static void applyDeclaredType(ASTNode* expression, DataType type, ReferenceType referenceType)
{
    if (expression->type == AST_PREFIX && expression->prefix.operator.type == TOKEN_CONST) {
        applyDeclaredType(expression->prefix.expr, type, referenceType);

        return;
    }

    if (type == TYPE_UNKNOWN) {
        return;
    }

    applyLiteralType(expression, type);

    if (expression->type == AST_CONDITIONAL) {
        expression->conditional.typeId = type;
        expression->conditional.referenceType = referenceType;
    }

    if (expression->type == AST_MATCH) {
        expression->match.typeId = type;
        expression->match.referenceType = referenceType;
    }
}

static bool variableTypesMatch(ASTNode* ast, DataType type, ReferenceType referenceType)
{
    ReferenceType declaredReferenceType = ast->variableDefinition.referenceType;

    if (declaredReferenceType != referenceType) {
        return false;
    }

    DataType declaredType = ast->variableDefinition.typeId;
    if (declaredType == type) {
        return true;
    }

    if (referenceType != REFERENCE_NONE) {
        return false;
    }

    return canImplicitlyWidenType(type, declaredType);
}

static void validateVariableTypeMatch(ASTNode* ast, DataType type, ReferenceType referenceType)
{
    if (variableTypesMatch(ast, type, referenceType)) {
        return;
    }

    DataType declaredType = ast->variableDefinition.typeId;
    ReferenceType declaredReferenceType = ast->variableDefinition.referenceType;

    if (declaredReferenceType == REFERENCE_NONE && referenceType == REFERENCE_NONE) {
        implicitConversionError(ast->variableDefinition.token, type, declaredType);
    }

    semanticTokenError("Initializer type does not match variable ", ast->variableDefinition.token);
}

static void resolveVariableType(ASTNode* ast, ASTNode* expression)
{
    DataType type = getNodeDataType(expression);
    ReferenceType referenceType = getReferenceType(expression);

    if (ast->variableDefinition.typeId != TYPE_UNKNOWN) {
        validateVariableTypeMatch(ast, type, referenceType);

        return;
    }

    if (type == TYPE_UNKNOWN && expression->type == AST_FLOAT) {
        floatLiteralRequiresTypeError(expression->floatLiteral.token);
    }

    if (type == TYPE_UNKNOWN) {
        integerLiteralRequiresTypeError(ast->variableDefinition.token);
    }

    ast->variableDefinition.typeId = type;
    ast->variableDefinition.referenceType = referenceType;
}

static void analyzeVariableInitializer(Analyzer* analyzer, ASTNode* ast)
{
    ASTNode* expression = ast->variableDefinition.expr;

    if (isNone(expression)) {
        return;
    }

    applyDeclaredType(expression, ast->variableDefinition.typeId, ast->variableDefinition.referenceType);
    analyzeNode(analyzer, expression);
    resolveVariableType(ast, expression);

    setReferenceOriginsFromExpression(ast, expression);
    initializeVariable(ast);
}

static void validateVariableType(ASTNode* ast)
{
    if (ast->variableDefinition.typeId == TYPE_VOID) {
        semanticTokenError("Invalid type for variable ", ast->variableDefinition.token);
    }
}

static void trackVariableReference(ASTNode* ast)
{
    if (ast->variableDefinition.referenceType == REFERENCE_NONE || !ast->variableDefinition.initialized) {
        return;
    }

    ASTNode* expression = ast->variableDefinition.expr;
    bool transfersExclusive = ast->variableDefinition.referenceType == REFERENCE_EXCLUSIVE && isVariable(expression);

    if (transfersExclusive) {
        transferExclusiveReference(expression);
    } else {
        activateReferenceAccess(ast);
    }

    ast->variableDefinition.referenceAccessActive = true;
}

static void analyzeVariableDefinition(Analyzer* analyzer, ASTNode* ast)
{
    StringView id = getStringView(ast->variableDefinition.id);

    if (getLocalSymbol(analyzer->currentScope, id)) {
        redefinitionError("Variable", ast->variableDefinition.token);
    }

    initializeVariableDefinition(analyzer, ast);
    analyzeVariableInitializer(analyzer, ast);
    validateVariableType(ast);
    setLocalVariableSymbol(analyzer->currentScope, id, ast);
    trackVariableReference(ast);
}

static bool canReturnReferenceOrigin(ASTNode* origin)
{
    if (!origin) {
        return false;
    }

    if (isTopLevelScope(getScope(origin))) {
        return true;
    }

    return isParameter(origin) && getReferenceType(origin) != REFERENCE_NONE;
}

static ReferenceType getExpectedReturnReferenceType(Analyzer* analyzer)
{
    if (!analyzer->function) {
        return REFERENCE_NONE;
    }

    return analyzer->function->functionDefinition.returnReferenceType;
}

static void validateReturnScope(Analyzer* analyzer, ASTNode* ast)
{
    if (analyzer->currentScope->level < 2) {
        returnOutsideFunctionError(ast->returnStatement.token);
    }
}

static ReferenceType validateReturnReferenceType(Analyzer* analyzer, ASTNode* ast)
{
    ReferenceType expected = getExpectedReturnReferenceType(analyzer);

    if (getReferenceType(ast->returnStatement.expr) != expected) {
        semanticTokenError("Invalid reference return from function ", ast->returnStatement.token);
    }

    return expected;
}

static void trackReturnOrigin(Analyzer* analyzer, ASTNode* ast, ASTNode* origin)
{
    if (!canReturnReferenceOrigin(origin)) {
        semanticTokenError("Reference to local binding cannot escape function ", ast->returnStatement.token);
    }

    ASTNode* function = analyzer->function;
    ASTNode* previous = function->functionDefinition.returnReferenceOrigin;

    if (previous && previous != origin) {
        semanticTokenError("Reference returns must share one lifetime in function ", ast->returnStatement.token);
    }

    function->functionDefinition.returnReferenceOrigin = origin;
}

static void trackReturnReferenceOrigin(Analyzer* analyzer, ASTNode* ast)
{
    Vector* origins = referenceOriginsFromExpression(ast->returnStatement.expr);
    size_t count = countVector(origins);

    if (!count) {
        ASTNode* origin = referenceOriginFromExpression(ast->returnStatement.expr);
        trackReturnOrigin(analyzer, ast, origin);

        return;
    }

    for (size_t i = 0; i < count; i++) {
        trackReturnOrigin(analyzer, ast, getVectorAt(origins, i));
    }
}

static void analyzeReturn(Analyzer* analyzer, ASTNode* ast)
{
    validateReturnScope(analyzer, ast);

    ASTNode* function = analyzer->function;

    if (function->functionDefinition.hasExplicitReturnType) {
        applyLiteralType(ast->returnStatement.expr, function->functionDefinition.returnTypeId);
    }

    analyzeNode(analyzer, ast->returnStatement.expr);

    if (validateReturnReferenceType(analyzer, ast) == REFERENCE_NONE) {
        return;
    }

    trackReturnReferenceOrigin(analyzer, ast);
}

static void inferNegatedIntegerLiteralType(ASTNode* ast)
{
    bool negative;
    ASTNode* literal = getIntegerLiteral(ast, &negative);

    if (!literal || !negative) {
        return;
    }

    if (literal->integerLiteral.typeId != TYPE_UNKNOWN) {
        return;
    }

    applyIntegerLiteralType(ast, TYPE_I64);
}

static void analyzeConstantExpression(Analyzer* analyzer, ASTNode* ast)
{
    Scope* previous = analyzer->constantScope;
    FunctionPath* previousFunctions = analyzer->constantFunctions;
    analyzer->constantScope = analyzer->currentScope;
    analyzer->constantFunctions = analyzer->functionPath;

    analyzeNode(analyzer, ast->prefix.expr);

    analyzer->constantScope = previous;
    analyzer->constantFunctions = previousFunctions;

    if (getReferenceType(ast->prefix.expr) != REFERENCE_NONE || getNodeDataType(ast) == TYPE_VOID) {
        semanticError("Constant expression must produce a value", ast->prefix.operator);
    }
}

static void analyzePrefix(Analyzer* analyzer, ASTNode* ast)
{
    if (ast->prefix.operator.type == TOKEN_CONST) {
        analyzeConstantExpression(analyzer, ast);

        return;
    }

    if (getReferenceType(ast) != REFERENCE_NONE) {
        analyzeReference(analyzer, ast);

        return;
    }

    inferNegatedIntegerLiteralType(ast);
    analyzeNode(analyzer, ast->prefix.expr);

    if (ast->prefix.operator.type == TOKEN_NOT && getNodeDataType(ast->prefix.expr) != TYPE_BOOL) {
        semanticError("Logical not requires a bool operand", ast->prefix.operator);
    }

    DataType type = getNodeDataType(ast->prefix.expr);

    if (type == TYPE_STRING) {
        semanticError("Invalid prefix operator for string operands", ast->prefix.operator);
    }

    if (type == TYPE_CHAR) {
        semanticError("Invalid prefix operator for char operands", ast->prefix.operator);
    }

    bool numeric = isSignedIntegerType(type) || isFloatType(type);

    if (ast->prefix.operator.type == TOKEN_MINUS && !numeric) {
        semanticError("Negation requires a signed numeric operand", ast->prefix.operator);
    }
}

static void analyzeResolvedBuiltinCall(Analyzer* analyzer, ASTNode* ast)
{
    analyzeExpressionNodes(analyzer, &ast->builtinCall.args);

    if (ast->builtinCall.builtin->paramCount >= 0) {
        return;
    }

    Token token = ast->builtinCall.token;

    validateVariadicArguments(&ast->builtinCall.args, ast->builtinCall.builtin, token);
}

static void analyzeNode(Analyzer* analyzer, ASTNode* ast)
{
    switch (ast->type) {
        case AST_ASSIGNMENT:
            analyzeAssignment(analyzer, ast);
            break;
        case AST_BINARY:
            analyzeBinary(analyzer, ast);
            break;
        case AST_BUILTIN_CALL:
            analyzeResolvedBuiltinCall(analyzer, ast);
            break;
        case AST_WHILE:
            analyzeWhile(analyzer, ast);
            break;
        case AST_CONDITIONAL:
            analyzeConditional(analyzer, ast);
            break;
        case AST_CONVERSION:
            analyzeNode(analyzer, ast->conversion.expr);
            break;
        case AST_FUNCTION_CALL:
            analyzeFunctionCall(analyzer, ast);
            break;
        case AST_FUNCTION_DEFINITION:
            analyzeFunction(analyzer, ast);
            break;
        case AST_MATCH:
            analyzeMatch(analyzer, ast);
            break;
        case AST_PARAMETER:
            analyzeParameter(analyzer, ast);
            break;
        case AST_PREFIX:
            analyzePrefix(analyzer, ast);
            break;
        case AST_RETURN:
            analyzeReturn(analyzer, ast);
            break;
        case AST_VARIABLE_DEFINITION:
            analyzeVariableDefinition(analyzer, ast);
            break;
        case AST_VARIABLE:
            analyzeVariable(analyzer, ast);
            break;
        default:
            break;
    }
}

void initAnalyzer(Analyzer* analyzer, ASTNode* ast)
{
    analyzer->topLevel = ast;
    analyzer->function = NULL;
    analyzer->currentScope = ast->compound.scope;
    analyzer->constantScope = NULL;
    analyzer->currentNodes = NULL;
    analyzer->nextNode = 0;
    analyzer->referenceLifetimes = NULL;
    analyzer->functionPath = NULL;
    analyzer->constantFunctions = NULL;
    analyzer->loopPath = NULL;
}

bool analyze(Analyzer* analyzer, size_t start)
{
    analyzer->currentScope = analyzer->topLevel->compound.scope;
    analyzeNodes(analyzer, &analyzer->topLevel->compound.statements, start);
    validateConstantExpressions(analyzer->topLevel, start);

    return true;
}

static void clearAnalysisVector(Vector* nodes, Vector* functions)
{
    for (size_t i = 0; i < nodes->count; i++) {
        clearNodeAnalysis(nodes->data[i], functions);
    }
}

static void clearVariableAnalysis(ASTNode* ast, Vector* functions)
{
    ast->variableDefinition.scope = NULL;
    ast->variableDefinition.initialized = false;
    ast->variableDefinition.moved = false;
    ast->variableDefinition.sharedAccessCount = 0;
    ast->variableDefinition.exclusiveAccessActive = false;
    ast->variableDefinition.referenceAccessActive = false;
    ast->variableDefinition.referenceOrigins.count = 0;
    ast->variableDefinition.readCount = 0;
    ast->variableDefinition.discardedReadCount = 0;
    ast->variableDefinition.discardedReadsAnalyzed = false;
    ast->variableDefinition.addressTaken = false;

    clearNodeAnalysis(ast->variableDefinition.expr, functions);
}

static void clearFunctionAnalysis(ASTNode* ast, Vector* functions)
{
    ast->functionDefinition.scope = NULL;
    ast->functionDefinition.returnReferenceOrigin = NULL;
    ast->functionDefinition.callCount = 0;

    pushVectorItem(functions, ast);
    clearAnalysisVector(&ast->functionDefinition.params, functions);
    clearNodeAnalysis(ast->functionDefinition.body, functions);
}

static void clearMatchArmAnalysis(MatchArm* arm, Vector* functions)
{
    clearNodeAnalysis(arm->pattern, functions);
    clearNodeAnalysis(arm->binding, functions);
    clearNodeAnalysis(arm->guard, functions);
    clearNodeAnalysis(arm->branch, functions);
}

static void clearMatchAnalysis(ASTNode* ast, Vector* functions)
{
    ast->match.referenceOrigins.count = 0;

    clearNodeAnalysis(ast->match.subject, functions);
    clearNodeAnalysis(ast->match.defaultBranch, functions);

    for (size_t i = 0; i < ast->match.arms.count; i++) {
        clearMatchArmAnalysis(ast->match.arms.data[i], functions);
    }
}

static void clearNodeAnalysis(ASTNode* ast, Vector* functions)
{
    if (!ast) {
        return;
    }

    switch (ast->type) {
        case AST_COMPOUND:
            freeScope(ast->compound.scope);
            ast->compound.scope = NULL;
            clearAnalysisVector(&ast->compound.statements, functions);
            break;
        case AST_VARIABLE_DEFINITION:
            clearVariableAnalysis(ast, functions);
            break;
        case AST_FUNCTION_DEFINITION:
            clearFunctionAnalysis(ast, functions);
            break;
        case AST_PARAMETER:
            ast->parameter.scope = NULL;
            ast->parameter.sharedAccessCount = 0;
            ast->parameter.exclusiveAccessActive = false;
            ast->parameter.moved = false;
            ast->parameter.referenceOrigins.count = 0;
            break;
        case AST_VARIABLE:
            ast->variable.scope = NULL;
            ast->variable.symbol = NULL;
            break;
        case AST_ASSIGNMENT:
            ast->assignment.scope = NULL;
            ast->assignment.symbol = NULL;
            ast->assignment.initializesBinding = false;
            clearNodeAnalysis(ast->assignment.expr, functions);
            break;
        case AST_FUNCTION_CALL:
            ast->functionCall.scope = NULL;
            ast->functionCall.symbol = NULL;
            ast->functionCall.referenceOrigins.count = 0;
            clearAnalysisVector(&ast->functionCall.args, functions);
            break;
        case AST_BUILTIN_CALL:
            clearAnalysisVector(&ast->builtinCall.args, functions);
            break;
        case AST_CONDITIONAL:
            ast->conditional.referenceOrigins.count = 0;
            clearNodeAnalysis(ast->conditional.condition, functions);
            clearNodeAnalysis(ast->conditional.thenBranch, functions);
            clearNodeAnalysis(ast->conditional.elseBranch, functions);
            break;
        case AST_MATCH:
            clearMatchAnalysis(ast, functions);
            break;
        case AST_WHILE:
            clearNodeAnalysis(ast->whileStatement.condition, functions);
            clearNodeAnalysis(ast->whileStatement.body, functions);
            break;
        case AST_BINARY:
            clearNodeAnalysis(ast->binary.leftExpr, functions);
            clearNodeAnalysis(ast->binary.rightExpr, functions);
            break;
        case AST_PREFIX:
            ast->prefix.referenceOrigins.count = 0;
            clearNodeAnalysis(ast->prefix.expr, functions);
            break;
        case AST_CONVERSION:
            clearNodeAnalysis(ast->conversion.expr, functions);
            break;
        case AST_RETURN:
            clearNodeAnalysis(ast->returnStatement.expr, functions);
            break;
        default:
            break;
    }
}

void rebuildAnalyzer(Analyzer* analyzer, Vector* functions)
{
    ASTNode* ast = analyzer->topLevel;

    initVector(functions);
    clearNodeAnalysis(ast, functions);

    ast->compound.scope = createScope(NULL);

    initAnalyzer(analyzer, ast);
    analyze(analyzer, 0);
}
