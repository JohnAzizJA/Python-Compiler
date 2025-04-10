#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <regex>

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
            "for", "while", "break", "continue", "pass",
            "and", "or", "not", "in", "is",
            "def", "class",
            "return", "yield",
            "True", "False", "None"
        };
    
    public:
        void readFile (const string& filename){

        }

        void tokenizeLine(const string& line, int lineNumber){

        }
};

int main(){

}