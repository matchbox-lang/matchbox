# Matchbox

Matchbox is a general-purpose programming language designed for speed, portability, expressive syntax, and a unified toolchain. It compiles `.mb` source files into bytecode, which runs on the Matchbox virtual machine.

> Warning: Matchbox is under active development. APIs and language behavior may change between releases.

## Status

Current version: `Matchbox 0.3.0`

Matchbox currently provides a compiler pipeline, bytecode VM, and a small standard/native runtime. The language is still evolving, so the `tests/` directory contains a mix of working examples and planned language behavior.

## Supported Platforms

- Windows

The current build uses `make` and `gcc`. On Windows, use an environment that provides those tools, such as MSYS2/MinGW, Git Bash with GCC, or WSL.

## Installation

Pre-built binaries can be found at:

https://www.matchbox-lang.org/downloads

To build from source:

```sh
make
```

This builds the debug executable at `build/matchbox.exe`, with optimizations disabled and debug symbols enabled. Use `make debug` to select this target explicitly.

To build the release executable:

```sh
make release
```

The release target optimizes for size (`-Os`), removes unused sections, and strips symbols. It writes to the same `build/matchbox.exe` path as the debug target.

To build the static library for embedding Matchbox:

```sh
make lib-release
```

This creates `build/libmatchbox.a` using the release compilation flags. Use `make lib` or `make lib-debug` for a library with debug symbols and optimizations disabled. The public API is declared in `include/matchbox.h`.

Debug and release targets keep their object files in separate directories, so switching targets does not require `make clean`. Executable targets share one output path, and library targets share another; the most recently built target determines the configuration at each path.

For the commands below, `matchbox` refers to either an installed binary or a locally built binary made available on your `PATH`.

To remove generated build artifacts:

```sh
make clean
```

## Getting Started

Create a file named `hello.mb` and add the following code:

```mb
print("Hello World!")
```

Compile and run the program:

```sh
matchbox hello.mb
```

## Development

Build the project before running local examples:

```sh
make
```

Run a working example from the test suite:

```sh
matchbox tests/valid/functions/basic.mb
```

Run test blocks or test programs:

```sh
matchbox test
matchbox test --program
```

`matchbox test` runs top-level `test` blocks.

```
let count = 0

test {
    const EXPECTED = 4
    let result = 2 + 2

    assert(result == EXPECTED)
    assert(count == 0)
}
```

Use `--program` to run entire .mb programs instead, treating `.fail.mb` files as expected failures. Specify a path to narrow the search. By default, tests are discovered in the current directory. Only failed results are displayed by default. Use `-a` to show all results or `-p` to show passed results.

## License

This project is licensed under the Apache License, Version 2.0.

See the [LICENSE](LICENSE) file for details.

## Copyright

Copyright © 2023-2026 Matchbox Labs
