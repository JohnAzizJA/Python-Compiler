#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include <string>
#include <unordered_set>
using namespace std;

enum TokenType {
    IDENTIFIER, KEYWORD, OPERATOR, LITERAL, DELIMITER, ERROR, INDENT, DEDENT, NEWLINE
};

struct Token {
    TokenType type;
    std::string value;
    int line;
};

struct Identifier {
    int ID;
    std::string name;
    std::string type;
    std::string Scope;
};

inline std::string tokenTypeToString(TokenType type) {
    switch (type) {
        case IDENTIFIER: return "IDENTIFIER";
        case KEYWORD: return "KEYWORD";
        case OPERATOR: return "OPERATOR";
        case LITERAL: return "LITERAL";
        case DELIMITER: return "DELIMITER";
        case ERROR: return "ERROR";
        case INDENT: return "INDENT";
        case DEDENT: return "DEDENT";
        case NEWLINE: return "NEWLINE";
        default: return "UNKNOWN"s;
    }
}

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



#endif