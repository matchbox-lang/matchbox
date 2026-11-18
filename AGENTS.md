# Coding Style

These rules apply to implementation code, tests, and verification scripts.

- Use `snake_case` for variables, functions, and filenames, including tests
- Use `PascalCase` for types and structs
- Use `PascalCase` for enums and enum constants
- Preserve AST and VM capitalization in `PascalCase` identifiers
- Prefix reserved-word token constants with `TokenKeyword`
- Use `UPPER_SNAKE_CASE` for macros
- Use `UPPER_SNAKE_CASE` for `const` bindings in Matchbox tests
- Prefix public API functions with `matchbox_`
- Include the matching header first in each `.c` file when one exists
- Place project headers (`"..."`) before system headers (`<...>`) after the matching header
- Sort includes by path within each group, placing directories before files at every path level and sorting names alphabetically within each category. For example, `compiler/lexer/token.h` comes before `compiler/ast.h` and `compiler/scope.h`
- Keep conditional includes in their existing preprocessor branches and apply include ordering within each branch
- Create, initialize, and copy `ASTNode` instances only through AST helpers in `ast.c` and `ast.h`. Do not use local struct initializers or fabricate AST nodes in the compiler, optimizer, or analyzer to reuse existing logic; extract shared helpers that operate directly on types or operands instead
- Do not use nested `if`, `while`, or `for` statements. Prefer guard clauses, early returns, or extracted helper functions
- Write conditionals across multiple lines, including conditional expressions used in assignments. Place each branch body on its own indented lines; never write an entire conditional on one line unless testing single line parsing
- Do not use nested function calls
- Do not wrap code until a line reaches more than 120 columns in length
- Place return statements on a new line unless the return is the only statement
- Separate assignment groups from standalone function calls with a blank line, both before and after the calls
- Prefer shorter functions. Extract helpers as needed to keep functions short, readable, and maintainable
- Don't use ternary expressions unless the logic is compact and fits on a single line
- Start comments with capital letters and not end with a period
- Add forward declarations only when required for a use before a function or type definition. Do not declare functions already defined before their first use or types already available through included headers. Remove declarations made redundant by changes to definition order or includes
- Place all required forward declarations at the top of the file
- Separate logical groups of statements with a blank line
- Place a blank line before and after control-flow blocks (`if`, `while`, `for`, `match`) when adjacent to other statements
- Do not add blank lines immediately inside a block's opening or closing brace
- Use CRLF line endings

# Documentation

- Keep each prose paragraph on a single line. Do not hard-wrap Markdown paragraphs
- Don't update the `README.md` file unless prompted to do so

# Testing

- Do not suppress Windows crash dialogs. Treat them as evidence of a defect that must be investigated and fixed
- Test changes in both file and interactive modes
- Tests should test language functionality and errors, not AST or bytecode optimizations
- Before adding tests, inspect existing coverage and identify the specific gap. Add tests only for coverage that does not already exist; do not add redundant tests for every small change
- Keep the `tests` directory focused on language syntax, features, and errors. Tests can and should use built-in functions where useful
- Put tests for each new feature in dedicated, focused test files instead of appending examples to existing feature files. Group related cases together and avoid separate files for each numeric overload
- Smaller, more focused tests are preferred than creating tests with dozens lines

# Temporary Files

- Remove every temporary file created during the work before finishing the task, including temporary scripts, verification files, profiling artifacts, and copied binaries
- Never leave temporary files in any directory, including build directories and system temporary directories

# Handoffs

- Include bugs discovered during the work in the final summary, even when they were not fixed
