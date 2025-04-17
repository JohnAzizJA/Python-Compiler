# Python-Compiler

## Features and Validations

- [ ] Space in identifiers (e.g., `w e` is invalid)
- [x] Identifiers starting with invalid characters (e.g., `1words` is invalid)
- [ ] Multiline strings and block comments handling
- [x] Unknowns in the symbol table
- [ ] Errors in the symbol table
- [ ] Global indentation for control blocks (e.g., `if`, `for`, `while` should not be globally indented)
- [ ] `f` for format strings shouldn't be treated as an identifier
- [x] Assignments like `variable = variable` type of first 