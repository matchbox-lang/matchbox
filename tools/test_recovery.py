from pathlib import Path
import argparse
import subprocess
import uuid


def run_checked(command, cwd):
    result = subprocess.run(command, cwd=cwd, capture_output=True, timeout=120)

    if result.returncode != 0:
        stdout = result.stdout.decode(errors="replace")
        stderr = result.stderr.decode(errors="replace")
        message = stdout + stderr

        raise AssertionError(message)

    return result


def check_repl(executable, source, values, errors=0, status=0, continuations=None):
    result = subprocess.run([executable], input=source, capture_output=True, timeout=30)
    output = result.stdout.replace(b">>> ", b"")
    output = output.replace(b"... ", b"")
    output = output.decode()
    actual = output.splitlines()
    nonempty = filter(None, actual)
    actual = list(nonempty)
    diagnostics = result.stderr.count(b"Error:")

    assert result.returncode == status, (source, result.returncode, result.stderr)
    assert actual == values, (source, actual, result.stderr)
    assert diagnostics == errors, (source, result.stderr)

    if continuations is not None:
        count = result.stdout.count(b"... ")

        assert count == continuations, result.stdout


def check_repl_cases(executable):
    check_repl(executable, b"let x\n=\n25\nx\n", ["25"], continuations=2)
    check_repl(executable, b"print(1); let x\n=\n25\nx\n", ["1", "25"], continuations=2)
    check_repl(executable, b"func value() {\nreturn 25\n}\nvalue()\n", ["25"], continuations=2)
    check_repl(executable, b"let x = 10; let y\n=\n25\nx + y\n", ["35"], continuations=2)
    check_repl(executable, b"let x\n=\n)\n25\n", ["25"], errors=1, continuations=2)
    check_repl(executable, b"let bad = '\\q'\n25\n", ["25"], errors=1)
    check_repl(executable, b"var x = 10\nlet bad = missing\nx\nlet bad = 20\nbad\n", ["10", "20"], errors=1)
    check_repl(executable, b"var x = 10\nx = 20; 1 / 0\nx\nvar x = 30\nx\n", ["30"], errors=2)
    check_repl(executable, b'var s = "hello"\ns = s + "!"; 1 / 0\ns\nvar s = "fresh"\ns\n',
               ["fresh"], errors=2)
    check_repl(executable, b"func fail() { return 1 / 0 }\nfail()\n25\n", ["25"], errors=1)
    check_repl(executable, b"func recurse() { recurse() }\nrecurse()\n25\n", ["25"], errors=1)
    check_repl(executable, b"let x\n=\n", [], errors=1, status=1, continuations=2)


def check_repl_session_reset(executable):
    check_repl(executable, b"func fail() { return 1 / 0 }\nfail()\nfail()\n"
                          b"func fail() { return 25 }\nfail()\n", ["25"], errors=2)
    check_repl(executable, b'func fail() -> string { print(1 / 0); return "bad" }\n'
                          b'let x = fail()\nx\nlet x = 25\nx\n', ["25"], errors=2)
    check_repl(executable, b"1 / 0\nlet x\n=\n25\nx\n", ["25"], errors=1, continuations=2)
    check_repl(executable, b"1 / 0\n2 / 0\n25\n", ["25"], errors=2)
    check_repl(executable, b"print(1); print(1 / 0); print(99)\n25\n", ["1", "25"], errors=1)
    check_repl(executable, b"1 / 0\n", [], errors=1)
    check_repl(executable, b"1 / 0\nlet x\n=\n", [], errors=2, status=1, continuations=2)
    check_repl(executable, b'var s: string\nif true { s = "x"; print(1 / 0) }\ns\nlet s = "new"\ns\n',
               ["new"], errors=2)


def check_repl_analysis_recovery(executable):
    check_repl(executable, b"let x = 10; let bad = missing\nlet x = 25\nx\n", ["25"], errors=1)
    check_repl(executable, b"var x = 10\nvar r = ^x\nlet lost = r; let bad = missing\nr\n", ["10"], errors=1)
    check_repl(executable, b"var x: i32\nx = 25; let bad = missing\nx\nx = 30\nx\n", ["30"], errors=2)
    check_repl(executable, b"var x = 10\nvar r: &i32\nr = &x; let bad = missing\nr\nr = &x\nr\n",
               ["10"], errors=2)
    check_repl(executable, b"var x = 10\nvar y = 20\nvar r = ^x\nr = ^y; let bad = missing\nr\ny\n",
               ["10", "20"], errors=1)
    check_repl(executable, b"var x: i32\nfunc bad() { x = 25; return missing }\nx\nx = 30\nx\n",
               ["30"], errors=2)
    check_repl(executable, b"func bad() { return missing }\nfunc bad() { return 25 }\nbad()\n",
               ["25"], errors=1)
    check_repl(executable, b"func keep() { return 7 }\nfunc bad(n: i32) -> char { return char(n) }\n"
                          b"func bad() { return 25 }\nkeep()\nbad()\n", ["7", "25"], errors=1)
    check_repl(executable, b"var x: i32\nx = 25\nlet bad = missing\nx\n", ["25"], errors=1)
    check_repl(executable, b"var x = 10\nvar source = ^x\nlet destination = source\n"
                          b"let bad = missing\nsource\ndestination\n", ["10"], errors=2)
    check_repl(executable, b"print(1)\nlet bad = missing\nprint(2)\n", ["1", "2"], errors=1)
    check_repl(executable, b"let x = 10\nlet x = 20\nx\n", ["10"], errors=1)
    check_repl(executable, b"let keep = 7\nlet lost: i64 = 4294967296; let bad = missing\n"
                          b"let first = 11\nlet second: i64 = 4294967297\nkeep\nfirst\nsecond\n",
               ["7", "11", "4294967297"], errors=1)
    check_repl(executable, b"func f(n) { if n == 0 { return 25 } return f(n - 1) }\n"
                          b"let bad = missing\nf(2)\n", ["25"], errors=1)


def check_retained_programs(executable, root):
    cases = [("functions/nested-recursion.mb", b"outer(3)\n", ["0", "0"]),
             ("references/returns.mb", b"print(returnedShared)\nprint(returnedExclusive)\n",
              ["10", "20", "10", "20"]),
             ("control/match-literal-alternatives.mb", b"25\n", ["20", "40", "60", "25"])]

    for filename, tail, values in cases:
        path = root / "tests" / "valid" / filename
        source = path.read_bytes()
        source += b"\nlet invalid = missing\n"
        source += tail

        check_repl(executable, source, values, errors=1)


def check_repl_growth(executable):
    source = b'var wide: i64 = 4294967296\nlet r = &wide\nlet message = "hello"\nlet s = &message\n'

    for index in range(32):
        declaration = f"let global{index} = {index}\n"
        encoded = declaration.encode()
        source += encoded

    source += b"r\ns\n"

    check_repl(executable, source, ["4294967296", "hello"])


def check_repl_stack_limit(executable):
    arguments = ["1"] * 1100
    arguments = ", ".join(arguments)
    command = "print(" + arguments + ")\n25\n"
    source = command.encode()

    check_repl(executable, source, ["25"], errors=1)


def check_reference_reassignment(executable, root):
    paths = ["shared-reassignment-releases-origin.mb", "exclusive-reassignment-releases-origin.mb"]

    for filename in paths:
        path = root / "tests" / "valid" / "references" / filename
        filename = str(path)
        result = run_checked([executable, filename], root)
        output = result.stdout.decode()
        lines = output.splitlines()

        assert lines == ["11", "20"], lines


def check_host(root):
    token = uuid.uuid4()
    filename = ".recovery-" + token.hex + ".exe"
    executable = root / filename
    executable_name = str(executable)
    object_file = executable.with_suffix(".o")
    object_name = str(object_file)
    compile_memory = ["gcc", "-Isrc", "-Wall", "-Wextra", "-O2", "-Dmalloc=testAllocate",
                      "-Dcalloc=testAllocateZeroed", "-Drealloc=testReallocate", "-Dfree=testFree",
                      "-c", "src/common/memory.c", "-o", object_name]
    command = ["gcc", "-Iinclude", "-Isrc", "-Wall", "-Wextra", "-O2",
               "tools/recovery_host.c", "src/program/repl.c", object_name,
               "build/libmatchbox.a", "-lm", "-o", executable_name]

    try:
        run_checked(compile_memory, root)
        run_checked(command, root)
        result = run_checked([executable_name], root)
        output = result.stdout.decode()
        output = output.strip()

        print(output)

        source = b'var s = "hello"\ns = s + "!"; print(1 / 0)\n' * 20
        source += b"25\n"
        result = subprocess.run([executable_name, "--repl"], input=source, cwd=root,
                                capture_output=True, timeout=30)

        assert result.returncode == 0, result.stderr
        assert result.stderr.count(b"Error:") == 20, result.stderr
        assert b"25" in result.stdout, result.stdout
    finally:
        executable.unlink(missing_ok=True)
        object_file.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument("--release", action="store_true")
    parser.add_argument("--known-issues", action="store_true")

    arguments = parser.parse_args()
    targets = ["debug", "lib-debug"]

    if arguments.release:
        targets = ["release", "lib-release"]

    script = Path(__file__)
    script = script.resolve()
    root = script.parents[1]
    executable = root / "build" / "matchbox.exe"
    executable_name = str(executable)

    command = ["make"] + targets

    run_checked(command, root)
    check_repl_cases(executable_name)
    check_repl_session_reset(executable_name)
    check_repl_stack_limit(executable_name)
    check_repl_analysis_recovery(executable_name)
    check_retained_programs(executable_name, root)
    check_repl_growth(executable_name)
    check_host(root)
    print("REPL recovery and incomplete-input checks passed")

    if arguments.known_issues:
        check_reference_reassignment(executable_name, root)


if __name__ == "__main__":
    main()
