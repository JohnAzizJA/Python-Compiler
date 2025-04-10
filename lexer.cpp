#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>

using namespace std;

enum TokenType {
    IDENTIFIER, KEYWORD, OPERATOR, LITERAL, DELIMITER
};

struct Token{
    TokenType type;
    string value;
    int line;
};

class Lexer {
    private:
        vector<Token> tokens;
        unordered_set<string> keywords = {
            "import", "from", "as",
            "if", "elif", "else", 
            "for", "while", "break", "continue", 
            "and", "or", "not", "in", "is",
            "def", "class",
            "return", "yield",
            "True", "False", "None"
        };
    
    public:
        
};

int main(){

}