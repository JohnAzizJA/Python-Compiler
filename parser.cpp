#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <stdexcept>
#include "definitions.h"

using namespace std;

// Forward declaration of ParseTreeNode
class ParseTreeNode;

// Parse Tree Node class
class ParseTreeNode {
public:
    string type;
    string value;
    vector<shared_ptr<ParseTreeNode>> children;

    ParseTreeNode(const string& t, const string& v = "") : type(t), value(v) {}

    void addChild(shared_ptr<ParseTreeNode> child) {
        children.push_back(child);
    }

    void print(int indent = 0) const {
        string indentation(indent * 2, ' ');
        cout << indentation << type;
        if (!value.empty()) {
            cout << ": " << value;
        }
        cout << endl;

        for (const auto& child : children) {
            child->print(indent + 1);
        }
    }
};

// Parser class for syntax analysis
class Parser {
private:
    vector<Token> tokens;
    size_t currentPos;
    shared_ptr<ParseTreeNode> parseTree;

    // Error handling
    void syntaxError(const string& message) {
        int line = currentPos < tokens.size() ? tokens[currentPos].line : -1;
        string tokenValue = currentPos < tokens.size() ? tokens[currentPos].value : "EOF";
        
        cerr << "Syntax Error at line " << line << " near '" << tokenValue << "': " << message << endl;
        throw runtime_error("Syntax Error: " + message);
    }

    // Helper methods
    Token& currentToken() {
        if (currentPos >= tokens.size()) {
            static Token eofToken = {ERROR, "EOF", -1};
            return eofToken;
        }
        return tokens[currentPos];
    }

    bool match(TokenType type) {
        if (currentPos >= tokens.size()) return false;
        return currentToken().type == type;
    }

    bool match(TokenType type, const string& value) {
        if (currentPos >= tokens.size()) return false;
        return currentToken().type == type && currentToken().value == value;
    }

    Token consume() {
        if (currentPos >= tokens.size()) {
            syntaxError("Unexpected end of input");
        }
        return tokens[currentPos++];
    }

    Token expect(TokenType type, const string& message) {
        if (!match(type)) {
            syntaxError(message);
        }
        return consume();
    }

    Token expect(TokenType type, const string& value, const string& message) {
        if (!match(type, value)) {
            syntaxError(message);
        }
        return consume();
    }

    // Grammar rules implementation
    shared_ptr<ParseTreeNode> parseProgram() {
        auto node = make_shared<ParseTreeNode>("Program");
        
        while (currentPos < tokens.size()) {
            try {
                node->addChild(parseStatement());
            } catch (const runtime_error& e) {
                // Skip to the next statement after error
                recoverFromError();
            }
        }
        
        return node;
    }

    void recoverFromError() {
        // Simple error recovery: skip tokens until we find a statement delimiter
        while (currentPos < tokens.size()) {
            if (match(DELIMITER, ";") || match(KEYWORD, "if") || 
                match(KEYWORD, "while") || match(KEYWORD, "for") || 
                match(KEYWORD, "def") || match(KEYWORD, "class")) {
                break;
            }
            currentPos++;
        }
    }

    shared_ptr<ParseTreeNode> parseStatement() {
        if (match(KEYWORD, "if")) {
            return parseIfStatement();
        } else if (match(KEYWORD, "while")) {
            return parseWhileStatement();
        } else if (match(KEYWORD, "for")) {
            return parseForStatement();
        } else if (match(KEYWORD, "def")) {
            return parseFunctionDef();
        } else if (match(KEYWORD, "return")) {
            return parseReturnStatement();
        } else if (match(IDENTIFIER)) {
            // Look ahead to see if this is an assignment
            size_t savedPos = currentPos;
            consume(); // consume identifier
            
            if (match(OPERATOR, "=")) {
                currentPos = savedPos; // rewind
                return parseAssignment();
            } else {
                currentPos = savedPos; // rewind
                return parseExpressionStatement();
            }
        } else {
            return parseExpressionStatement();
        }
    }

    shared_ptr<ParseTreeNode> parseIfStatement() {
        auto node = make_shared<ParseTreeNode>("IfStatement");
        
        // Parse 'if' keyword
        node->addChild(make_shared<ParseTreeNode>("Keyword", consume().value));
        
        // Parse condition
        node->addChild(parseExpression());
        
        // Parse ':'
        expect(DELIMITER, ":", "Expected ':' after if condition");
        
        // Parse then-block
        node->addChild(parseBlock());
        
        // Parse optional else-block
        if (match(KEYWORD, "else")) {
            node->addChild(make_shared<ParseTreeNode>("Keyword", consume().value));
            expect(DELIMITER, ":", "Expected ':' after 'else'");
            node->addChild(parseBlock());
        }
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseWhileStatement() {
        auto node = make_shared<ParseTreeNode>("WhileStatement");
        
        // Parse 'while' keyword
        node->addChild(make_shared<ParseTreeNode>("Keyword", consume().value));
        
        // Parse condition
        node->addChild(parseExpression());
        
        // Parse ':'
        expect(DELIMITER, ":", "Expected ':' after while condition");
        
        // Parse body
        node->addChild(parseBlock());
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseForStatement() {
        auto node = make_shared<ParseTreeNode>("ForStatement");
        
        // Parse 'for' keyword
        node->addChild(make_shared<ParseTreeNode>("Keyword", consume().value));
        
        // Parse target variable
        node->addChild(make_shared<ParseTreeNode>("Identifier", expect(IDENTIFIER, "Expected identifier after 'for'").value));
        
        // Parse 'in' keyword
        expect(KEYWORD, "in", "Expected 'in' after for variable");
        node->addChild(make_shared<ParseTreeNode>("Keyword", "in"));
        
        // Parse iterable expression
        node->addChild(parseExpression());
        
        // Parse ':'
        expect(DELIMITER, ":", "Expected ':' after for statement");
        
        // Parse body
        node->addChild(parseBlock());
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseFunctionDef() {
        auto node = make_shared<ParseTreeNode>("FunctionDefinition");
        
        // Parse 'def' keyword
        node->addChild(make_shared<ParseTreeNode>("Keyword", consume().value));
        
        // Parse function name
        node->addChild(make_shared<ParseTreeNode>("Identifier", expect(IDENTIFIER, "Expected function name after 'def'").value));
        
        // Parse parameter list
        expect(DELIMITER, "(", "Expected '(' after function name");
        auto paramsNode = make_shared<ParseTreeNode>("Parameters");
        
        if (!match(DELIMITER, ")")) {
            do {
                paramsNode->addChild(make_shared<ParseTreeNode>("Parameter", expect(IDENTIFIER, "Expected parameter name").value));
                if (match(DELIMITER, ",")) {
                    consume();
                } else {
                    break;
                }
            } while (true);
        }
        
        node->addChild(paramsNode);
        expect(DELIMITER, ")", "Expected ')' after parameters");
        
        // Parse ':'
        expect(DELIMITER, ":", "Expected ':' after function declaration");
        
        // Parse function body
        node->addChild(parseBlock());
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseReturnStatement() {
        auto node = make_shared<ParseTreeNode>("ReturnStatement");
        
        // Parse 'return' keyword
        node->addChild(make_shared<ParseTreeNode>("Keyword", consume().value));
        
        // Parse optional return value
        if (!match(DELIMITER, ";") && currentPos < tokens.size()) {
            node->addChild(parseExpression());
        }
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseAssignment() {
        auto node = make_shared<ParseTreeNode>("Assignment");
        
        // Parse target
        node->addChild(make_shared<ParseTreeNode>("Target", expect(IDENTIFIER, "Expected identifier").value));
        
        // Parse '='
        expect(OPERATOR, "=", "Expected '=' in assignment");
        
        // Parse expression
        node->addChild(parseExpression());
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseExpressionStatement() {
        auto node = make_shared<ParseTreeNode>("ExpressionStatement");
        node->addChild(parseExpression());
        return node;
    }

    shared_ptr<ParseTreeNode> parseBlock() {
        auto node = make_shared<ParseTreeNode>("Block");
        
        // In a real Python parser, you would handle indentation here
        // For simplicity, we'll just parse a single statement
        node->addChild(parseStatement());
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseExpression() {
        return parseLogicalOr();
    }

    shared_ptr<ParseTreeNode> parseLogicalOr() {
        auto node = parseLogicalAnd();
        
        while (match(KEYWORD, "or")) {
            auto opNode = make_shared<ParseTreeNode>("BinaryOperator", consume().value);
            opNode->addChild(node);
            opNode->addChild(parseLogicalAnd());
            node = opNode;
        }
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseLogicalAnd() {
        auto node = parseEquality();
        
        while (match(KEYWORD, "and")) {
            auto opNode = make_shared<ParseTreeNode>("BinaryOperator", consume().value);
            opNode->addChild(node);
            opNode->addChild(parseEquality());
            node = opNode;
        }
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseEquality() {
        auto node = parseComparison();
        
        while (match(OPERATOR, "==") || match(OPERATOR, "!=")) {
            auto opNode = make_shared<ParseTreeNode>("BinaryOperator", consume().value);
            opNode->addChild(node);
            opNode->addChild(parseComparison());
            node = opNode;
        }
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseComparison() {
        auto node = parseAdditive();
        
        while (match(OPERATOR, "<") || match(OPERATOR, ">") || 
               match(OPERATOR, "<=") || match(OPERATOR, ">=")) {
            auto opNode = make_shared<ParseTreeNode>("BinaryOperator", consume().value);
            opNode->addChild(node);
            opNode->addChild(parseAdditive());
            node = opNode;
        }
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseAdditive() {
        auto node = parseMultiplicative();
        
        while (match(OPERATOR, "+") || match(OPERATOR, "-")) {
            auto opNode = make_shared<ParseTreeNode>("BinaryOperator", consume().value);
            opNode->addChild(node);
            opNode->addChild(parseMultiplicative());
            node = opNode;
        }
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseMultiplicative() {
        auto node = parseUnary();
        
        while (match(OPERATOR, "*") || match(OPERATOR, "/") || match(OPERATOR, "%")) {
            auto opNode = make_shared<ParseTreeNode>("BinaryOperator", consume().value);
            opNode->addChild(node);
            opNode->addChild(parseUnary());
            node = opNode;
        }
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseUnary() {
        if (match(OPERATOR, "-") || match(OPERATOR, "+") || match(KEYWORD, "not")) {
            auto opNode = make_shared<ParseTreeNode>("UnaryOperator", consume().value);
            opNode->addChild(parseUnary());
            return opNode;
        }
        
        return parsePrimary();
    }

    shared_ptr<ParseTreeNode> parsePrimary() {
        if (match(LITERAL)) {
            return make_shared<ParseTreeNode>("Literal", consume().value);
        } else if (match(IDENTIFIER)) {
            auto idNode = make_shared<ParseTreeNode>("Identifier", consume().value);
            
            // Check for function call
            if (match(DELIMITER, "(")) {
                auto callNode = make_shared<ParseTreeNode>("FunctionCall");
                callNode->addChild(idNode);
                
                // Parse arguments
                consume(); // consume '('
                auto argsNode = make_shared<ParseTreeNode>("Arguments");
                
                if (!match(DELIMITER, ")")) {
                    do {
                        argsNode->addChild(parseExpression());
                        if (match(DELIMITER, ",")) {
                            consume();
                        } else {
                            break;
                        }
                    } while (true);
                }
                
                callNode->addChild(argsNode);
                expect(DELIMITER, ")", "Expected ')' after function arguments");
                
                return callNode;
            }
            
            return idNode;
        } else if (match(DELIMITER, "(")) {
            consume(); // consume '('
            auto expr = parseExpression();
            expect(DELIMITER, ")", "Expected ')' after expression");
            return expr;
        } else {
            syntaxError("Expected expression");
            return nullptr; // This line will never be reached due to exception
        }
    }

public:
    Parser(const vector<Token>& t) : tokens(t), currentPos(0) {}

    shared_ptr<ParseTreeNode> parse() {
        try {
            parseTree = parseProgram();
            return parseTree;
        } catch (const runtime_error& e) {
            cerr << "Parsing failed: " << e.what() << endl;
            return nullptr;
        }
    }

    void printParseTree() const {
        if (parseTree) {
            parseTree->print();
        } else {
            cout << "No parse tree available." << endl;
        }
    }
};

int main() {
    // Example usage with a simple Python-like program
    vector<Token> tokens = {
        {KEYWORD, "def", 1},
        {IDENTIFIER, "add", 1},
        {DELIMITER, "(", 1},
        {IDENTIFIER, "x", 1},
        {DELIMITER, ",", 1},
        {IDENTIFIER, "y", 1},
        {DELIMITER, ")", 1},
        {DELIMITER, ":", 1},
        {IDENTIFIER, "result", 2},
        {OPERATOR, "=", 2},
        {IDENTIFIER, "x", 2},
        {OPERATOR, "+", 2},
        {IDENTIFIER, "y", 2},
        {KEYWORD, "return", 3},
        {IDENTIFIER, "result", 3}
    };

    Parser parser(tokens);
    auto parseTree = parser.parse();
    
    if (parseTree) {
        cout << "Parsing successful! Parse tree:" << endl;
        parser.printParseTree();
    }

    return 0;
}