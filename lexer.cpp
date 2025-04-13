#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <regex>
#include <iomanip>

using namespace std;

enum TokenType {
    IDENTIFIER, KEYWORD, OPERATOR, LITERAL, DELIMITER
};

struct Token{
    TokenType type;
    string value;
    int line;
};

struct Identifier {
    int ID;
    string name;
    string type;
    string Scope;
};

string tokenTypeToString(TokenType type) {
    switch (type) {
        case IDENTIFIER: return "IDENTIFIER";
        case KEYWORD: return "KEYWORD";
        case OPERATOR: return "OPERATOR";
        case LITERAL: return "LITERAL";
        case DELIMITER: return "DELIMITER";
        default: return "UNKNOWN";
    }
}


class Lexer {
    private:
        vector<Identifier> symbol_table;
        vector<Token> tokens;
        string CurrentScope = "global";
        bool inBlockComment = false;

        unordered_set<string> keywords = {
            "import", "from", "as",
            "if", "elif", "else", 
            "for", "while", "break", "continue", "pass",
            "and", "or", "not", "in", "is",
            "def", "class",
            "return", "yield",
            "True", "False", "None"
        };

        void addToSymbolTable(const string& name, const string& type, const string& scope) {
            for (const auto& id : symbol_table) {
                if (id.name == name && id.Scope == scope) return; // avoid duplicates in same scope
            }
            int newID = symbol_table.size() + 1;
            symbol_table.push_back({newID, name, type, scope});
        }

        string getVariableType(const string& name, const string& scope) {
            for (const auto& id : symbol_table) {
                if (id.name == name && id.Scope == scope) {
                    return id.type;
                }
            }
            return "unknown"; // Return "unknown" if the variable is not found
        }
        
    
    public:
        void readFile(const string& filename) {
            ifstream file(filename);
            if (!file.is_open()) {
                cerr << "Error: Could not open file " << filename << endl;
                return;
            }
        
            string line;
            int lineNumber = 1;
            while (getline(file, line)) {
                tokenizeLine(line, lineNumber);
                lineNumber++;
            }
        
            file.close();
        }

        void tokenizeLine(const string& line, int lineNumber) {
            string code = line;

            // Reset scope if the line is not indented and we're inside a function
             if (!line.empty() && !isspace(line[0]) && CurrentScope != "global") {
                CurrentScope = "global";
            }

            // Handle block comments or multiline string literals
            if (inBlockComment) {
                size_t endBlock = line.find("\"\"\"");
                if (endBlock != string::npos) {
                    inBlockComment = false;
                    code = line.substr(endBlock + 3); // Skip the block comment
                } else {
                    return; // Entire line is part of the block comment
                }
            }
        
            size_t startBlock = code.find("\"\"\"");
            if (startBlock != string::npos) {
                size_t equalsPos = code.find('=');
                if (equalsPos != string::npos && equalsPos < startBlock) {
                    size_t endBlock = code.find("\"\"\"", startBlock + 3);
                    if (endBlock != string::npos) {
                        string literal = code.substr(startBlock, endBlock - startBlock + 3);
                        tokens.push_back({LITERAL, literal, lineNumber});
                        code = code.substr(endBlock + 3);
                    } else {
                        inBlockComment = true;
                        return;
                    }
                } else {
                    inBlockComment = true;
                    code = code.substr(0, startBlock);
                }
            }
        
            size_t commentPos = code.find('#');
            code = (commentPos != string::npos) ? code.substr(0, commentPos) : code;
        
            // Split by semicolons and tokenize each statement
            vector<string> statements;
            size_t start = 0, end;
            while ((end = code.find(';', start)) != string::npos) {
                statements.push_back(code.substr(start, end - start));
                start = end + 1;
            }
            statements.push_back(code.substr(start));
        
            for (const string& statement : statements) {
                tokenizeStatement(statement, lineNumber);
            }
        }
        void tokenizeStatement(const string& code, int lineNumber) {
            // Regular expressions for different token types
            regex keywordRegex("[a-zA-Z_][a-zA-Z0-9_]*");
            regex numberRegex("\\b(0[xX][0-9a-fA-F]+|\\d+(\\.\\d+)?([eE][+-]?\\d+)?)\\b");
            regex operatorRegex("(==|!=|<=|>=|[+\\-*/%=<>!&|^~])");
            regex delimiterRegex("[(){}\\[\\],.:;]");
            regex stringLiteralRegex("\".*?\"|'.*?'");
            regex functionDefRegex("^\\s*def\\s+([a-zA-Z_][a-zA-Z0-9_]*)\\s*\\("); // Regex for function definitions

            smatch match;

            // Check for function definition
            if (regex_search(code, match, functionDefRegex)) {
                CurrentScope = match[1]; // Set the current scope to the function name
            }

            for (size_t i = 0; i < code.size();) {
                if (isspace(code[i])) {
                    i++;
                    continue;
                }
        
                string subCode = code.substr(i);
        
                // Match string literals
                if (regex_search(subCode, match, stringLiteralRegex) && match.position() == 0) {
                    tokens.push_back({LITERAL, match.str(), lineNumber});
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
        
                // Match keywords and identifiers
                if (regex_search(subCode, match, keywordRegex) && match.position() == 0) {
                    string word = match.str();
        
                    if (keywords.find(word) != keywords.end()) {
                        // Add keyword to tokens
                        tokens.push_back({KEYWORD, word, lineNumber});
                    } else {
                        // Add identifier to tokens
                        tokens.push_back({IDENTIFIER, word, lineNumber});
        
                        // Check if it's a variable assignment: identifier = something
                        size_t equalPos = code.find('=', i + word.length());
                        if (equalPos != string::npos && code[equalPos - 1] != '=' && code[equalPos + 1] != '=') {
                            string rhs = code.substr(equalPos + 1);
                            rhs = regex_replace(rhs, regex("^\\s+|\\s+$"), ""); // Trim spaces
        
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
                            } else if (regex_match(rhs, regex("^[+-]?\\d+\\s*[+\\-*/]\\s*\\d+$"))) {
                                // Handle simple arithmetic expressions
                                type = "int"; // Assume the result of arithmetic operations is an integer
                            } else {
                                // Handle expressions involving variables
                                vector<string> tokens;
                                stringstream ss(rhs);
                                string token;
                                while (ss >> token) {
                                    tokens.push_back(token);
                                }
        
                                // Infer type based on the first variable or literal in the expression
                                for (const string& tok : tokens) {
                                    if (regex_match(tok, keywordRegex)) {
                                        type = getVariableType(tok, CurrentScope);
                                        if (type != "unknown") break;
                                    } else if (regex_match(tok, regex("^[+-]?\\d+$"))) {
                                        type = "int";
                                        break;
                                    } else if (regex_match(tok, regex("^[+-]?(\\d*\\.\\d+|\\d+\\.\\d*)([eE][+-]?\\d+)?$"))) {
                                        type = "float";
                                        break;
                                    }
                                }
                            }
        
                            // Add to symbol table
                            addToSymbolTable(word, type, CurrentScope);
                        } else {
                            // Add identifier to symbol table with unknown type if not an assignment
                            addToSymbolTable(word, "unknown", CurrentScope);
                        }
                    }
        
                    i += match.length();
                    continue;
                }
        
                // Match numbers
                if (regex_search(subCode, match, numberRegex) && match.position() == 0) {
                    tokens.push_back({LITERAL, match.str(), lineNumber});
                    i += match.length();
                    continue;
                }
        
                // If no match, unrecognized token
                cerr << "Warning: Unrecognized token '" << code[i] << "' on line " << lineNumber << endl;
                i++;
            }
        }
        const vector<Token>& getTokens() const {
            return tokens;
        }
        const vector<Identifier>& getsymbols() const {
            return symbol_table;
        }
};

int main() {
    Lexer lexer;
    lexer.readFile("example.py");

    // Token Table Header
    cout << left << setw(8) << "Line"
         << setw(15) << "Type"
         << setw(20) << "Value" << endl;
    cout << string(45, '-') << endl;

    // Token Table Rows
    for (const auto& token : lexer.getTokens()) {
        cout << left << setw(8) << token.line
             << setw(15) << tokenTypeToString(token.type)
             << setw(20) << token.value << endl;
    }

    // Symbol Table Header
    cout << "\n--- Symbol Table ---\n";
    cout << left << setw(6) << "ID"
         << setw(20) << "Name"
         << setw(15) << "Type"
         << setw(15) << "Scope" << endl;
    cout << string(56, '-') << endl;

    // Symbol Table Rows
    for (const auto& id : lexer.getsymbols()) {
        cout << left << setw(6) << id.ID
             << setw(20) << id.name
             << setw(15) << id.type
             << setw(15) << id.Scope << endl;
    }
    return 0;
}