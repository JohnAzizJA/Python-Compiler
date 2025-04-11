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

        unordered_set<string> keywords = {
            "import", "from", "as",
            "if", "elif", "else", 
            "for", "while", "break", "continue", "pass",
            "and", "or", "not", "in", "is",
            "def", "class",
            "return", "yield",
            "True", "False", "None"
        };
        bool inBlockComment = false;
    
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
                return; // No need to tokenize further for function definitions
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
                        tokens.push_back({KEYWORD, word, lineNumber});
                    } else {
                        tokens.push_back({IDENTIFIER, word, lineNumber});
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

    for (const auto& token : lexer.getTokens()) {
        cout << "Line: " << token.line 
             << setw(5) << " Type: " << tokenTypeToString(token.type) << setw(10)
             << " Value: " << token.value << endl << endl;
    }
    return 0;
}