#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <regex>
#include <iomanip>
#include <algorithm>
#include "definitions.h"

using namespace std;

class Lexer {
    private:
        vector<tuple<string, int, int>> CodeLines; 
        vector<string> scopeStack; 
        vector<Identifier> symbol_table;
        vector<Token> tokens;
        string CurrentScope = "global";
        bool inBlockComment = false;
        int previousIndentation = 0;
        int expectedIndentation = 0;
        bool expectingIndentedBlock = false;

        int getIndentationLevel(const string& line) {
            int count = 0;
            for (char ch : line) {
                if (ch == ' ') count++…
[7:09 pm, 16/05/2025] Michael Maged: # 1. Indentation Error
if True:
print("This is indented incorrectly")

# 2. Unterminated String Literal
message = "Hello, this string never ends

# 3. Missing Right Hand Side of Assignment    
y = 

# 4. Invalid Attribute Name with Spaces
he llo = "Hello"

# 5. Malformed Number Literals
numb1 = 1e      # Incomplete exponent
numb2 = 1.1.0   # Multiple decimal points

# 6. Missing Conditions in Control Structures
if:
    print("No condition specified")

while:
    print("No condition specified")

for:
    print("No variable and iterable specified")

# 7. Missing Colons in Control Structures
if True
    print("Missing colon after condition")

# 8. Variables Used Before Declaration
print(undeclared_variable)  # Using before declaration

# 9. Invalid Characters
amount$ = 100  # $ is not valid in variable names

# 10. Identifier Starting with Invalid Character
1variable = 10  # Cannot start with a number

# 12. Block Comment Handling Issues
"""This is a multiline comment
that doesn't get closed properly"""

# 13.
X = 10 y=20