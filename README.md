# Python-Compiler

## Features and Validations

- [x] Space in identifiers (e.g., `w e` is invalid)
- [x] Identifiers starting with invalid characters (e.g., `1words` is invalid)
- [ ] Multiline strings and block comments handling
- [x] Unknowns in the symbol table (msh moshkla)
- [ ] Errors in the symbol table
- [x] Global indentation for control blocks (e.g., `if`, `for`, `while` should not be globally indented)
- [x] `f` for format strings shouldn't be treated as an identifier
- [x] Assignments like `variable = variable` type of first
- [x] Include delimiters (parentheses, dots, commas) in parse tree