# Matchbox

Matchbox is a general-purpose programming language designed for speed, portability, expressive syntax, and a unified toolchain. It compiles `.mb` source files into bytecode, which runs on the Matchbox virtual machine.

> Warning: Matchbox is under active development. APIs and language behavior may change between releases.

## Status

Current version: `Matchbox 0.2.1`

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

This creates the executable at `build/matchbox`.

For the commands below, `matchbox` refers to either an installed binary or a locally built binary made available on your `PATH`.

To remove generated build artifacts:

```sh
make clean
```

## Getting Started

Create a file named `main.mb` and add the following code:

```mb
func add(a, b) {
    return a + b
}

var result = add(10, 20)
print(result)
```

Compile and run the program:

```sh
matchbox main.mb
```

## Development

Build the project before running local examples:

```sh
make
```

Run a known-working example from the test suite:

```sh
matchbox tests/Function.mb
```

There is not currently a dedicated automated test runner in this repository. Some files in `tests/` are working examples, while others document intended language behavior that may not be implemented yet.

## License

This project is licensed under the Apache License, Version 2.0.

See the [LICENSE](LICENSE) file for details.

## Copyright

Copyright © 2023-2026 Matchbox Labs
