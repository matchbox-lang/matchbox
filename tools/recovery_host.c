#include "matchbox.h"
#include "compiler.h"
#include "common/memory.h"
#include "program/repl.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void* testAllocate(size_t size);
void* testAllocateZeroed(size_t count, size_t size);
void* testReallocate(void* pointer, size_t size);
void testFree(void* pointer);

static size_t liveAllocations;
static size_t allocationsUntilFailure = SIZE_MAX;

static bool shouldFailAllocation(void)
{
    if (allocationsUntilFailure == SIZE_MAX) {
        return false;
    }

    if (allocationsUntilFailure == 0) {
        return true;
    }

    allocationsUntilFailure--;

    return false;
}

static void recordAllocation(void* pointer)
{
    if (pointer) {
        liveAllocations++;
    }
}

void* testAllocate(size_t size)
{
    if (shouldFailAllocation()) {
        return NULL;
    }

    void* pointer = malloc(size);

    recordAllocation(pointer);

    return pointer;
}

void* testAllocateZeroed(size_t count, size_t size)
{
    if (shouldFailAllocation()) {
        return NULL;
    }

    void* pointer = calloc(count, size);

    recordAllocation(pointer);

    return pointer;
}

void* testReallocate(void* pointer, size_t size)
{
    if (shouldFailAllocation()) {
        return NULL;
    }

    bool newAllocation = pointer == NULL;
    void* result = realloc(pointer, size);

    if (newAllocation) {
        recordAllocation(result);
    }

    return result;
}

void testFree(void* pointer)
{
    if (pointer) {
        assert(liveAllocations > 0);

        liveAllocations--;
    }

    free(pointer);
}

static void expectCompilation(Compiler* compiler, char* source, CompileResult expected)
{
    CompileResult result = compileRepl(compiler, source);

    assert(result == expected);
}

static void testCompilerRecovery(void)
{
    MemoryGroup memory = {0};
    MemoryGroup* previous = useMemoryGroup(&memory);
    ModuleObject* module = createModuleObject();
    Compiler compiler;
    char valid[] = "let x = 25";
    char incomplete[] = "let y";
    char invalid[] = "let lost = 10; let bad = missing";
    char corrected[] = "let y = 30";
    char read[] = "print(x, y)";

    initCompiler(&compiler, module);
    expectCompilation(&compiler, valid, COMPILE_SUCCESS);
    expectCompilation(&compiler, incomplete, COMPILE_INCOMPLETE);
    expectCompilation(&compiler, invalid, COMPILE_ERROR);
    expectCompilation(&compiler, corrected, COMPILE_SUCCESS);
    expectCompilation(&compiler, read, COMPILE_SUCCESS);
    useMemoryGroup(previous);
    freeMemoryGroup(&memory);

    assert(liveAllocations == 0);
}

static void expectRun(Matchbox* mb, const char* source, bool expected)
{
    bool result = matchbox_run(mb, source);

    assert(result == expected);

    if (!result) {
        assert(liveAllocations == 0);
    }
}

static void testErrorRecovery(Matchbox* mb)
{
    expectRun(mb, "let x = '\\q'", false);
    expectRun(mb, "let x = )", false);
    expectRun(mb, "let x = missing", false);
    expectRun(mb, "let x: u8 = 999", false);
    expectRun(mb, "let x", false);
    expectRun(mb, "func fail() { return 1 / 0 } fail()", false);
    expectRun(mb, "func recurse() { recurse() } recurse()", false);
    expectRun(mb, "let x = 25", true);
    expectRun(mb, "print(1 / 0)", false);
    expectRun(mb, "let x = 50", true);
}

static void testIndependentInstances(Matchbox* mb)
{
    expectRun(mb, "let x = 25", true);

    size_t originalAllocations = liveAllocations;
    Matchbox* other = matchbox_create();

    assert(other != NULL);

    bool complete = matchbox_run(other, "let bad = missing");

    assert(!complete);
    assert(liveAllocations == originalAllocations);

    complete = matchbox_run(other, "let x = 50");

    assert(complete);

    matchbox_destroy(other);

    assert(liveAllocations == originalAllocations);

    expectRun(mb, "let bad = missing", false);
}

static bool tryAllocationLimit(Matchbox* mb, size_t limit)
{
    const char* source = "func text(x: string) -> string { return x + \"!\" } let result = text(\"hello\")";
    allocationsUntilFailure = limit;

    bool complete = matchbox_run(mb, source);

    allocationsUntilFailure = SIZE_MAX;

    if (!complete) {
        assert(liveAllocations == 0);
    }

    return complete;
}

static void testAllocationFailures(Matchbox* mb)
{
    size_t limit = 0;

    while (!tryAllocationLimit(mb, limit)) {
        limit++;

        assert(limit < 1000);
    }

    expectRun(mb, "let x = )", false);
    expectRun(mb, "let x = 25", true);
}

int main(int argc, char** argv)
{
    (void)argv;

    if (argc > 1) {
        bool complete = runRepl();

        assert(liveAllocations == 0);

        return !complete;
    }

    testCompilerRecovery();

    Matchbox* mb = matchbox_create();

    assert(mb != NULL);

    testErrorRecovery(mb);
    testAllocationFailures(mb);
    testIndependentInstances(mb);
    matchbox_destroy(mb);

    assert(liveAllocations == 0);

    puts("Host recovery and allocation cleanup passed");

    return 0;
}
