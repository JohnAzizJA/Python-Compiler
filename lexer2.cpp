#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <regex>
#include <iomanip>
#include <algorithm>

using namespace std;

enum TokenType {
    IDENTIFIER, KEYWORD, OPERATOR, LITERAL, DELIMITER, ERROR
};

struct Token {
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
        case ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

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

        unordered_set<string> builtInFunctions = {
            "print", "input", "lower", "upper", "len", "range", "str", "int", "float", "bool", "list", "dict", "set", "tuple"
        };

        unordered_set<string> keywords = {
            "import", "from", "as",
            "if", "elif", "else",
            "for", "while", "break", "continue", "pass",
            "def", "class", 
            "return", "yield",
            "True", "False", "None"
        };
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
            for (auto& id : symbol_table) {
                // Check if the variable already exists in the same scope
                if (id.name == name && id.Scope == scope) {
                    id.type = type; // Update the type
                    return;         // Exit after updating
                }
            }
        
            // If the variable doesn't exist, add it as a new entry
            int newID = symbol_table.size() + 1;
            symbol_table.push_back({newID, name, type, scope});
        }

        string getVariableType(const string& name, const string& scope) {
            for (const auto& id : symbol_table) {
                if (id.name == name && id.Scope == scope) {
                    return id.type;
                }
            }
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
                if (CurrentScope == "global" && indentation > 0) {
                
                    cerr << "Error: Indentation error on line " << lineNumber << endl;
                    tokens.push_back({ERROR, "IndentationError", lineNumber});
                    continue;
                }
                    
        
                // Handle scope changes
                if (indentation > previousIndentation) {
                    if (expectingIndentedBlock) {
                        scopeStack.push_back(CurrentScope);
                        expectingIndentedBlock = false;
                    }
                } else if (indentation < previousIndentation) {
                    if (!scopeStack.empty()) {
                        scopeStack.pop_back();
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
            }
        }
        
        void tokenizeStatement(const string& code, int lineNumber) {
            // Regular expressions for different token types
            regex keywordRegex("[a-zA-Z_][a-zA-Z0-9_]*");
            regex numberRegex("\\b(0[xX][0-9a-fA-F]+|\\d+(\\.\\d+)?([eE][+-]?\\d+)?)\\b");
            regex operatorRegex("(==|!=|<=|>=|[+\\-*/%=<>!&|^~])");
            regex delimiterRegex("[(){}\\[\\],.:;]");
            regex formattedStringRegex(R"([fF]\".*?\"|[fF]\'.*?\')");
            regex stringLiteralRegex("\".*?\"|'.*?'");
            regex functionDefRegex("^\\s*def\\s+([a-zA-Z_][a-zA-Z0-9_]*)\\s*\\("); 
            regex listRegex("\\[([^\\]]*)\\]");
            regex tupleRegex("\\(([^\\)]*)\\)");
        
            // ERROR regexes
            regex malformedNumberRegex(R"(\b\d+(\.\d+){2,}|\d+\.\d+\.\d+|[+-]?\d*\.?\d*[eE]$|[+-]?\d*\.?\d*[eE][+-]?$)");
            regex unterminatedStringRegex("\"[^\"]*$|'[^']*$");
            regex invalidAttributeRegex(R"(\b([a-zA-Z_][a-zA-Z0-9_]*)\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*=)");
            regex lhsNoRhsRegex(R"(^\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*=\s*$)");


        
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
                    tokens.push_back({ERROR, strLiteral, lineNumber});
                    i += match.length();
                    continue;
                }

                if (regex_search(subCode, match, lhsNoRhsRegex)) {
                    string lhs = match[1]; // Extract the LHS variable name
                    cerr << "Error: Missing RHS for assignment to '" << lhs << "' on line " << lineNumber << endl;
                    tokens.push_back({ERROR, "MissingRHS", lineNumber});
                    i += match.length();
                    continue;
                }

                if (regex_search(subCode, match, invalidAttributeRegex) && subCode.find(':') == string::npos) {
                    string strLiteral = match.str();
                    cerr << "Error: Invalid attribute name with space on line " << lineNumber << endl;
                    tokens.push_back({ERROR, strLiteral, lineNumber});
                    i += match.length();
                    continue;
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
                            // Look ahead to see if there's no expression followings
                            smatch lookahead;
                            string afterWord = code.substr(i + word.length());
                            if (regex_match(afterWord, regex(R"(^\s*(:|\s*$))"))) {
                                cerr << "Error: Expected condition/expression after '" << word << "' on line " << lineNumber << endl;
                                tokens.push_back({ERROR, "ExpectedCondition", lineNumber});
                                i += match.length();
                                continue;
                            } 
                            else if (subCode.find(':') == string::npos) {
                                cerr << "Error: Expected ':' after keyword '" << word << "' on line " << lineNumber << endl;
                                tokens.push_back({ERROR, "ExpectedColon", lineNumber});
                                i += match.length();
                                continue;
                            }
                            CurrentScope = word + " line number " + to_string(lineNumber); 
                            scopeStack.push_back( word + " line number " + to_string(lineNumber));
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
                            } else if (regex_match(rhs, regex("^[a-zA-Z_][a-zA-Z0-9_]*$"))) {
                                // RHS is a variable, get its type from the symbol table
                                type = getVariableType(rhs, CurrentScope);
                                if (type == "unknown") {
                                    cerr << "Error: Variable '" << rhs << "' used before declaration on line " << lineNumber << endl;
                                    tokens.push_back({ERROR, rhs, lineNumber});
                                }
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
                    tokens.push_back({ERROR, badNum, lineNumber});
                    i += match.length();
                    continue;
                }
        
                if (regex_search(subCode, match, numberRegex) && match.position() == 0) {
                    tokens.push_back({LITERAL, match.str(), lineNumber});
                    i += match.length();
                    continue;
                }
        
                // If no match, unrecognized token
                cerr << "Error: Invalid character '" << code[i] << "' on line " << lineNumber << endl;
                tokens.push_back({ERROR, string(1, code[i]), lineNumber});
                i++;
            }
        
            // Match function definitions
            if (regex_search(code, match, functionDefRegex)) {
                string functionName = match[1];
                tokens.push_back({IDENTIFIER, functionName, lineNumber});
                addToSymbolTable(functionName, "function", CurrentScope);
        
                // Push the new function scope onto the stack
                scopeStack.push_back(functionName);
                CurrentScope = functionName; // Update the current scope
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

};

int main() {
    Lexer lexer;
    lexer.parser("example.py");
    lexer.tokenizeLine(lexer.getcodelines());

    cout << left << setw(8) << "Line"
         << setw(15) << "Type"
         << setw(20) << "Value" << endl;
    cout << string(45, '-') << endl;

    for (const auto& token : lexer.getTokens()) {
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
    for (const auto& id : lexer.getsymbols()) {
        cout << left << setw(6) << id.ID
             << setw(20) << id.name
             << setw(15) << id.type
             << setw(15) << id.Scope << endl;
    }

    return 0;
}

