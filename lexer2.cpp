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
                if (ch == ' ') count++;
                else if (ch == '\t') count += 4; // tab = 4 spaces
                else break;
            }
            return count;
        }

        void addToSymbolTable(const string& name, const string& type, const string& scope) {
            // Special handling for function parameters and class methods
            if (name == "self" || (scope.find("__init__") != string::npos && (name == "name" || name == "self"))) {
                int newID = symbol_table.size() + 1;
                symbol_table.push_back({newID, name, type, scope});
                return;
            }

            // Check if it's a function declaration
            if (type == "function") {
                for (auto& id : symbol_table) {
                    if (id.name == name && id.type == "function") {
                        return; // Function already declared
                    }
                }
                int newID = symbol_table.size() + 1;
                symbol_table.push_back({newID, name, type, "global"});
                return;
            }

            // For all other identifiers (variables)
            bool found = false;
            
            // First check if variable exists anywhere in the symbol table
            for (auto& id : symbol_table) {
                if (id.name == name) {
                    found = true;
                    // If found in any scope, update it to be global with the new type
                    id.Scope = "global";
                    if (type != "unknown") {
                        id.type = type;
                    }
                    break;
                }
            }

            // If not found anywhere, add as new entry in appropriate scope
            if (!found) {
                int newID = symbol_table.size() + 1;
                // Always add variables to global scope except function parameters
                if (scope.find("if") != string::npos || 
                    scope.find("else") != string::npos || 
                    scope.find("while") != string::npos || 
                    scope.find("for") != string::npos || 
                    scope == "global") {
                    symbol_table.push_back({newID, name, type, "global"});
                } else {
                    // For function parameters and local variables
                    symbol_table.push_back({newID, name, type, scope});
                }
            }
        }
        


        string getVariableType(const string& name, const string& scope) {
            // First check in current scope
            for (const auto& id : symbol_table) {
                if (id.name == name && id.Scope == scope) {
                    return id.type;
                }
            }

            // Then check in global scope
            for (const auto& id : symbol_table) {
                if (id.name == name && id.Scope == "global") {
                    return id.type;
                }
            }

            // If not found in any relevant scope
            return "unknown";
        }

    public:
        void parser(string filename){
            ifstream file(filename);
            if (!file.is_open()) {
                cerr << "Error: Could not open file " << filename << endl;
                return;
            }

            string line;
            int lineNumber = 1;

            while (getline(file, line)) {
                if (line.find("#") != string::npos) {
                    line = line.substr(0, line.find("#")); // Remove comments
                }
                CodeLines.push_back(make_tuple(line, lineNumber, getIndentationLevel(line))); // Store the line of code with its line number and indentation level
                lineNumber++;
            }
            file.close();
        }
        
        void tokenizeLine(const vector<tuple<string, int, int>>& lines) {
            string currentBlockCommentDelimiter = "";
        
            for (const auto& [line, lineNumber, indentation] : lines) {
                string currentLine = line;
        
                if (currentLine.empty() || all_of(currentLine.begin(), currentLine.end(), [](unsigned char ch) { return isspace(ch); })) {
                    continue; // Skip empty lines
                }
                        // --- ADD THIS BLOCK ---
                if (indentation % 4 != 0) {
                    cerr << "Error: Indentation error on line " << lineNumber << " (not a multiple of 4 spaces)" << endl;
                    throw runtime_error("Indentation error");
                }
        
                // Handle ongoing block comments
                if (inBlockComment) {
                    if (currentLine.find(currentBlockCommentDelimiter) != string::npos) {
                        inBlockComment = false;
                        currentBlockCommentDelimiter = "";
                    }
                    continue;
                }
        
                // Detect start of block comment
                if ((currentLine.find("\"\"\"") != string::npos || currentLine.find("'''") != string::npos)) {
                    size_t tripleQuoteCount = count(currentLine.begin(), currentLine.end(), '"');
                    size_t singleQuoteCount = count(currentLine.begin(), currentLine.end(), '\'');
        
                    bool startsAndEndsOnSameLine = 
                        (currentLine.find("\"\"\"") != string::npos && tripleQuoteCount >= 6) || 
                        (currentLine.find("'''") != string::npos && singleQuoteCount >= 6);
        
                    if (!startsAndEndsOnSameLine) {
                        if (currentLine.find("\"\"\"") != string::npos) {
                            currentBlockCommentDelimiter = "\"\"\"";
                        } else {
                            currentBlockCommentDelimiter = "'''";
                        }
                        inBlockComment = true;
                        continue;
                    }
                    // If it's a single-line block comment, skip it
                    continue;
                }
                
                if (CurrentScope == "global" && indentation > 0 && !expectingIndentedBlock) {
                    cerr << "Error: Indentation error on line " << lineNumber << endl;
                    throw runtime_error("Indentation error");
                }
        
                // Handle indentation changes and generate INDENT/DEDENT tokens
                if (indentation > previousIndentation) {
                    // Add INDENT token
                    tokens.push_back({INDENT, to_string(indentation), lineNumber});
                    
                    if (expectingIndentedBlock) {
                        scopeStack.push_back(CurrentScope);
                        expectingIndentedBlock = false;
                    }
                } else if (indentation < previousIndentation) {
                    // Add DEDENT tokens - might need multiple if we're going back multiple levels
                    int indentDiff = previousIndentation - indentation;
                    int dedentCount = indentDiff / 4; // Assuming each indentation level is 4 spaces
                    
                    for (int i = 0; i < dedentCount; i++) {
                        tokens.push_back({DEDENT, to_string(indentation), lineNumber});
                        if (!scopeStack.empty()) {
                            scopeStack.pop_back();
                        }
                    }
                }
                previousIndentation = indentation;
        
                // Update current scope
                CurrentScope = scopeStack.empty() ? "global" : scopeStack.back();
        
                // Split line by semicolon
                stringstream ss(currentLine);
                string segment;
                while (getline(ss, segment, ';')) {
                    if (!segment.empty()) {
                        tokenizeStatement(segment, lineNumber);
                    }
                }
                // Emit NEWLINE token after processing the line
                tokens.push_back({NEWLINE, "\\n", lineNumber});
            }
        }
        
        void tokenizeStatement(const string& code, int lineNumber) {
            // Regular expressions for different token types
            regex keywordRegex("[a-zA-Z_][a-zA-Z0-9_]*");
            regex numberRegex("\\b(0[xX][0-9a-fA-F]+|\\d+(\\.\\d+)?([eE][+-]?\\d+)?)\\b");
            regex operatorRegex("(==|!=|<=|>=|\\+=|-=|\\*=|/=|%=|//=|[+\\-*/%=<>!&|^~])");
            regex delimiterRegex("[(){}\\[\\],.:;]");
            regex formattedStringRegex(R"([fF]\".*?\"|[fF]\'.*?\')");
            regex stringLiteralRegex("\".*?\"|'.*?'");
            regex functionDefRegex("^\\s*def\\s+([a-zA-Z_][a-zA-Z0-9_]*)\\s*\\("); 
            regex classDefRegex("^\\s*class\\s+([a-zA-Z_][a-zA-Z0-9_]*)");
            regex listRegex("\\[([^\\]]*)\\]");
            regex tupleRegex("\\(([^\\)]*)\\)");
        
            // ERROR regexes
            regex malformedNumberRegex(R"(\b\d+(\.\d+){2,}|\d+\.\d+\.\d+|[+-]?\d*\.?\d*[eE]$|[+-]?\d*\.?\d*[eE][+-]?$)");
            regex unterminatedStringRegex("\"[^\"]*$|'[^']*$");
            regex invalidAttributeRegex(R"(\b([a-zA-Z_][a-zA-Z0-9_]*)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=)");

            smatch match;
        
            for (size_t i = 0; i < code.size();) {
                if (isspace(code[i])) {
                    i++;
                    continue;
                }
        
                string subCode = code.substr(i);
                // Match formatted string literals (f-strings)
                if (regex_search(subCode, match, formattedStringRegex) && match.position() == 0) {
                    string fStringLiteral = match.str();
                    tokens.push_back({LITERAL, fStringLiteral, lineNumber});
                    i += match.length();
                    continue;
                }

                // Match string literals
                if (regex_search(subCode, match, unterminatedStringRegex) && match.position() == 0) {
                    string strLiteral = match.str();
                    cerr << "Error: Unterminated string literal on line " << lineNumber << endl;
                    printTables();

                    throw runtime_error("Unterminated string literal");
                }


                if (regex_search(subCode, match, invalidAttributeRegex) && subCode.find(':') == string::npos) {
                    string strLiteral = match.str();
                    cerr << "Error: Invalid attribute name with space on line " << lineNumber << endl;
                    printTables();
                    throw runtime_error("Invalid attribute name with space");
                }
        
                if (regex_search(subCode, match, stringLiteralRegex) && match.position() == 0) {
                    string strLiteral = match.str();
                    tokens.push_back({LITERAL, strLiteral, lineNumber});
                    i += match.length();
                    continue;
                }
        
                // Match operators
                if (regex_search(subCode, match, operatorRegex) && match.position() == 0) {
                    tokens.push_back({OPERATOR, match.str(), lineNumber});
                    i += match.length();
                    continue;
                }
        
                // Match delimiters
                if (regex_search(subCode, match, delimiterRegex) && match.position() == 0) {
                    tokens.push_back({DELIMITER, match.str(), lineNumber});
                    i += match.length();
                    continue;
                }
        
                // Match list literals
                if (regex_search(subCode, match, listRegex) && match.position() == 0) {
                    tokens.push_back({LITERAL, match.str(), lineNumber});
                    i += match.length();
                    continue;
                }
        
                // Match tuple literals
                if (regex_search(subCode, match, tupleRegex) && match.position() == 0) {
                    tokens.push_back({LITERAL, match.str(), lineNumber});
                    i += match.length();
                    continue;
                }
        
                // Match keywords and identifiers
                if (regex_search(subCode, match, keywordRegex) && match.position() == 0) {
                    string word = match.str();
        
                    if (keywords.find(word) != keywords.end()) {
                        if (word == "if" || word == "elif" || word == "while" || word == "for") {
                            CurrentScope = word + " line number " + to_string(lineNumber);
                            scopeStack.push_back(word + " line number " + to_string(lineNumber));
                        }
                        else if (word == "else")
                        {
                            CurrentScope = word + " line number " + to_string(lineNumber); 
                            scopeStack.push_back( word + " line number " + to_string(lineNumber));
                        }
                        
                        tokens.push_back({KEYWORD, word, lineNumber});
                    } 
                    else {
                        if (builtInFunctions.find(word) != builtInFunctions.end()) {
                            tokens.push_back({IDENTIFIER, word, lineNumber});
                            i += match.length();
                            continue;
                        }
        
                        tokens.push_back({IDENTIFIER, word, lineNumber});
                        size_t equalPos = code.find('=', i + word.length());
                        if (equalPos != string::npos && code[equalPos - 1] != '=' && code[equalPos + 1] != '=') {
                            string rhs = code.substr(equalPos + 1);
                            rhs = regex_replace(rhs, regex("^\\s+|\\s+$"), "");
                            string type = "unknown";
                        
                            // Infer type from RHS
                            if (regex_match(rhs, regex("^0[xX][0-9a-fA-F]+$"))) {
                                type = "int"; // Hexadecimal integer
                            } else if (regex_match(rhs, regex("^[+-]?\\d+$"))) {
                                type = "int"; // Decimal integer
                            } else if (regex_match(rhs, regex("^[+-]?(\\d*\\.\\d+|\\d+\\.\\d*)([eE][+-]?\\d+)?$"))) {
                                type = "float"; // Float with optional exponent
                            } else if (regex_match(rhs, regex("^(\".*\"|'.*')$"))) {
                                type = "string";
                            } else if (rhs == "True" || rhs == "False") {
                                type = "bool";
                            } else if (regex_match(rhs, regex("^input\\s*\\(.*\\)$"))) {
                                type = "string"; // Input function returns a string
                            } else if (regex_match(rhs, regex("^[a-zA-Z_][a-zA-Z0-9_]*\\s*\\(.*\\)$"))) {
                                type = "func return"; // Function call
                            } else if (regex_match(rhs, regex("^[+-]?\\d+\\s*[+\\-*/]\\s*\\d+$"))) {
                                type = "int"; // Arithmetic expressions result in int
                            } else if (regex_match(rhs, listRegex)) {
                                type = "list"; // List literal
                            } else if (regex_match(rhs, tupleRegex)) {
                                type = "tuple"; // Tuple literal
                            } else {
                                // Handle expressions involving variables
                                vector<string> tokens;
                                stringstream ss(rhs);
                                string token;
                                while (ss >> token) {
                                    if (regex_match(token, keywordRegex)) {
                                        type = getVariableType(token, CurrentScope);
                                        if (type != "unknown") break;
                                    } else if (regex_match(token, regex("^[+-]?\\d+$"))) {
                                        type = "int"; break;
                                    } else if (regex_match(token, regex("^[+-]?(\\d*\\.\\d+|\\d+\\.\\d*)([eE][+-]?\\d+)?$"))) {
                                        type = "float"; break;
                                    }
                                }
                            }
                            addToSymbolTable(word, type, CurrentScope);
                        }
                    }
        
                    i += match.length();
                    continue;
                }
        
                // Match numbers
                if (regex_search(subCode, match, malformedNumberRegex) && match.position() == 0) {
                    string badNum = match.str();
                    cerr << "Error: Malformed number literal '" << badNum << "' on line " << lineNumber << endl;
                    printTables();
                    throw runtime_error("Malformed number literal");
                }
        
                if (regex_search(subCode, match, numberRegex) && match.position() == 0) {
                    tokens.push_back({LITERAL, match.str(), lineNumber});
                    i += match.length();
                    continue;
                }
        
                // If no match, unrecognized token
                cerr << "Error: Invalid character '" << code[i] << "' on line " << lineNumber << endl;
                tokens.push_back({ERROR, string(1, code[i]), lineNumber});
                printTables();
                throw runtime_error("Invalid character");
            }
        
            // Match function definitions
            if (regex_search(code, match, functionDefRegex)) {
                string functionName = match[1];
                addToSymbolTable(functionName, "function", CurrentScope);
        
                // Push the new function scope onto the stack
                scopeStack.push_back(functionName);
                CurrentScope = functionName; // Update the current scope
                
                // Set flag to expect an indented block after function definition
                expectingIndentedBlock = true;
            }

            if (regex_search(code, match, classDefRegex)) {
                string className = match[1];
                addToSymbolTable(className, "class", CurrentScope);

                // Push the new class scope onto the stack
                scopeStack.push_back(className);
                CurrentScope = className; // Update the current scope
                
                // Set flag to expect an indented block after class definition
                expectingIndentedBlock = true;
            }
        }

        const vector<Token>& getTokens() const {
            return tokens;
        }
        
        const vector<Identifier>& getsymbols() const {
            return symbol_table;
        }

        const vector<tuple<string, int, int>>& getcodelines() const {
            return CodeLines;
        }
        
        void printTables() const {
            cout << left << setw(8) << "Line"
                 << setw(15) << "Type"
                 << setw(20) << "Value" << endl;
            cout << string(45, '-') << endl;

            for (const auto& token : tokens) {
                if (token.type == TokenType::ERROR) continue;
                cout << left << setw(8) << token.line
                     << setw(15) << tokenTypeToString(token.type)
                     << setw(20) << token.value << endl;
            }

            cout << "\n--- Symbol Table ---\n";
            cout << left << setw(6) << "ID"
                 << setw(20) << "Name"
                 << setw(15) << "Type"
                 << setw(15) << "Scope" << endl;
            cout << string(56, '-') << endl;
            for (const auto& id : symbol_table) {
                cout << left << setw(6) << id.ID
                     << setw(20) << id.name
                     << setw(15) << id.type
                     << setw(15) << id.Scope << endl;
            }
        }
};

// int main() {
//     Lexer lexer;
//     lexer.parser("errors.py");
//     lexer.tokenizeLine(lexer.getcodelines());
//     lexer.printTables();


//     return 0;
// }