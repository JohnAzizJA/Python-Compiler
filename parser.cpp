#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <memory>
#include <string>
#include <stdexcept>
#include "definitions.h"
#include "lexer2.cpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace std;


// Forward declaration of ParseTreeNode
class ParseTreeNode;

// Parse Tree Node class
class ParseTreeNode {
public:
    string type;
    string value;
    vector<shared_ptr<ParseTreeNode>> children;
    static int nodeCounter;

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
    
    // Generate DOT representation of the node and its children
    void toDot(ostream& out, int& nodeId) const {
        int myId = nodeId++;
        
        // Node label
        string label = type;
        if (!value.empty()) {
            label += ": " + value;
        }
        
        // Escape quotes in the label
        size_t pos = 0;
        while ((pos = label.find("\"", pos)) != string::npos) {
            label.replace(pos, 1, "\\\"");
            pos += 2;
        }
        
        out << "  node" << myId << " [label=\"" << label << "\"];" << endl;
        
        // Connect to children
        for (const auto& child : children) {
            int childId = nodeId;
            child->toDot(out, nodeId);
            out << "  node" << myId << " -> node" << childId << ";" << endl;
        }
    }
};

// Initialize static counter
int ParseTreeNode::nodeCounter = 0;

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
            // Skip NEWLINE tokens between statements
            while (match(NEWLINE)) consume();
            if (currentPos >= tokens.size()) break;
            node->addChild(parseStatement());
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
        while (match(NEWLINE)) consume();
        if (match(KEYWORD, "if")) {
            return parseIfStatement();
        } else if (match(KEYWORD, "while")) {
            return parseWhileStatement();
        } else if (match(KEYWORD, "for")) {
            return parseForStatement();
        } else if (match(KEYWORD, "def")) {
            return parseFunctionDef();
        } else if (match(KEYWORD, "class")) {
            return parseClassDef();
        } else if (match(KEYWORD, "return")) {
            return parseReturnStatement();
        } else if (match(KEYWORD, "pass")) {
            return parsePassStatement();
        } else if (match(KEYWORD, "break")) {
            return parseBreakStatement();
        } else if (match(KEYWORD, "continue")) {
            return parseContinueStatement();
        } else if (match(KEYWORD, "import") || match(KEYWORD, "from")) {
            return parseImportStatement();
        } else if (match(IDENTIFIER)) {
            // Look ahead to see if this is an assignment or attribute assignment
            size_t savedPos = currentPos;
            consume(); // consume identifier
            
            // Check for attribute access followed by assignment
            if (match(DELIMITER, ".")) {
                consume(); // consume '.'
                if (match(IDENTIFIER)) {
                    consume(); // consume attribute name
                    if (match(OPERATOR, "=") || match(OPERATOR, "+=") || match(OPERATOR, "-=") || 
                        match(OPERATOR, "*=") || match(OPERATOR, "/=") || match(OPERATOR, "%=") || 
                        match(OPERATOR, "//=")) {
                        currentPos = savedPos; // rewind
                        return parseAssignment();
                    }
                }
                currentPos = savedPos; // rewind if not an attribute assignment
            }
            
            // Check for direct assignment
            if (match(OPERATOR, "=") || match(OPERATOR, "+=") || match(OPERATOR, "-=") || 
                match(OPERATOR, "*=") || match(OPERATOR, "/=") || match(OPERATOR, "%=") || 
                match(OPERATOR, "//=")) {
                currentPos = savedPos; // rewind
                return parseAssignment();
            } else if (match(DELIMITER, "(")) {
                currentPos = savedPos; // rewind
                return parseFunctionCallStatement();
            } else {
                currentPos = savedPos; // rewind
                return parseExpressionStatement();
            }
        } else {
            return parseExpressionStatement();
        }
    }

    shared_ptr<ParseTreeNode> parseBlockOrSimpleSuite() {
        auto node = make_shared<ParseTreeNode>("Suite");
        if (match(NEWLINE)) {
            consume(); // consume NEWLINE
            if (match(INDENT)) {
                consume(); // consume INDENT
                while (!match(DEDENT) && currentPos < tokens.size()) {
                    // Skip extra NEWLINEs inside block
                    while (match(NEWLINE)) consume();
                    if (match(DEDENT) || currentPos >= tokens.size()) break;
                    node->addChild(parseStatement());
                }
                if (match(DEDENT)) {
                    consume(); // consume DEDENT
                } else if (currentPos >= tokens.size()) {
                    // Allow EOF as valid end of block
                } else {
                    syntaxError("Expected DEDENT at end of block");
                }
            } else {
                syntaxError("Expected INDENT after NEWLINE for block suite");
            }
        } else if (
            // Accept a simple statement (start of a statement)
            match(IDENTIFIER) || match(KEYWORD, "return") || match(KEYWORD, "pass") ||
            match(KEYWORD, "break") || match(KEYWORD, "continue") || match(KEYWORD, "import") ||
            match(KEYWORD, "from") || match(KEYWORD, "if") || match(KEYWORD, "while") ||
            match(KEYWORD, "for") || match(KEYWORD, "def") || match(KEYWORD, "class")
        ) {
            node->addChild(parseStatement());
        } else {
            syntaxError("Expected NEWLINE+INDENT for block or a simple statement after ':'");
        }
        return node;
    }

    shared_ptr<ParseTreeNode> parseIfStatement() {
        auto node = make_shared<ParseTreeNode>("IfStatement");
        node->addChild(make_shared<ParseTreeNode>("Keyword", consume().value)); // 'if'
        
        // Parse the condition - no need to flatten it anymore
        node->addChild(parseTest());
        
        expect(DELIMITER, ":", "Expected ':' after if condition");
        node->addChild(parseBlockOrSimpleSuite());

        // Parse optional elif blocks
        while (match(KEYWORD, "elif")) {
            auto elifNode = make_shared<ParseTreeNode>("ElifClause");
            elifNode->addChild(make_shared<ParseTreeNode>("Keyword", consume().value));
            
            // Parse the elif condition - no need to flatten it anymore
            elifNode->addChild(parseTest());
            
            expect(DELIMITER, ":", "Expected ':' after elif condition");
            elifNode->addChild(parseBlockOrSimpleSuite());
            node->addChild(elifNode);
        }

        // Parse optional else-block
        if (match(KEYWORD, "else")) {
            auto elseNode = make_shared<ParseTreeNode>("ElseClause");
            elseNode->addChild(make_shared<ParseTreeNode>("Keyword", consume().value));
            expect(DELIMITER, ":", "Expected ':' after 'else'");
            elseNode->addChild(parseBlockOrSimpleSuite());
            node->addChild(elseNode);
        }

        return node;
    }

    shared_ptr<ParseTreeNode> parseWhileStatement() {
        auto node = make_shared<ParseTreeNode>("WhileStatement");
        node->addChild(make_shared<ParseTreeNode>("Keyword", consume().value));
        node->addChild(parseTest());
        expect(DELIMITER, ":", "Expected ':' after while condition");
        node->addChild(parseBlockOrSimpleSuite());
        return node;
    }

    shared_ptr<ParseTreeNode> parseForStatement() {
        auto node = make_shared<ParseTreeNode>("ForStatement");
        node->addChild(make_shared<ParseTreeNode>("Keyword", consume().value));
        node->addChild(make_shared<ParseTreeNode>("Identifier", expect(IDENTIFIER, "Expected identifier after 'for'").value));
        expect(KEYWORD, "in", "Expected 'in' after for variable");
        node->addChild(make_shared<ParseTreeNode>("Keyword", "in"));
        node->addChild(parseTest());
        expect(DELIMITER, ":", "Expected ':' after for statement");
        node->addChild(parseBlockOrSimpleSuite());
        return node;
    }

    shared_ptr<ParseTreeNode> parseFunctionDef() {
        auto node = make_shared<ParseTreeNode>("FunctionDefinition");
        node->addChild(make_shared<ParseTreeNode>("Keyword", consume().value));
        node->addChild(make_shared<ParseTreeNode>("Identifier", expect(IDENTIFIER, "Expected function name after 'def'").value));

        // Add opening parenthesis node
        Token openParen = expect(DELIMITER, "(", "Expected '(' after function name");
        node->addChild(make_shared<ParseTreeNode>("Delimiter", openParen.value));

        auto paramsNode = make_shared<ParseTreeNode>("Parameters");
        if (!match(DELIMITER, ")")) {
            do {
                paramsNode->addChild(make_shared<ParseTreeNode>("Parameter", expect(IDENTIFIER, "Expected parameter name").value));
                if (match(DELIMITER, ",")) {
                    Token comma = consume();
                    paramsNode->addChild(make_shared<ParseTreeNode>("Delimiter", comma.value));
                    if (match(DELIMITER, ")")) break;
                } else {
                    break;
                }
            } while (true);
        }
        node->addChild(paramsNode);

        // Add closing parenthesis node
        Token closeParen = expect(DELIMITER, ")", "Expected ')' after parameters");
        node->addChild(make_shared<ParseTreeNode>("Delimiter", closeParen.value));

        // Add colon node
        Token colon = expect(DELIMITER, ":", "Expected ':' after function declaration");
        node->addChild(make_shared<ParseTreeNode>("Delimiter", colon.value));

        node->addChild(parseBlockOrSimpleSuite());
        return node;
    }

    shared_ptr<ParseTreeNode> parseClassDef() {
        auto node = make_shared<ParseTreeNode>("ClassDefinition");
        node->addChild(make_shared<ParseTreeNode>("Keyword", consume().value));
        node->addChild(make_shared<ParseTreeNode>("Identifier", expect(IDENTIFIER, "Expected class name after 'class'").value));
        
        if (match(DELIMITER, "(")) {
            // Add opening parenthesis to parse tree
            Token openParen = consume();
            node->addChild(make_shared<ParseTreeNode>("Delimiter", openParen.value));
            
            node->addChild(make_shared<ParseTreeNode>("Parent", expect(IDENTIFIER, "Expected parent class name").value));
            
            // Add closing parenthesis to parse tree
            Token closeParen = expect(DELIMITER, ")", "Expected ')' after parent class name");
            node->addChild(make_shared<ParseTreeNode>("Delimiter", closeParen.value));
        }
        
        // Add colon to parse tree
        Token colon = expect(DELIMITER, ":", "Expected ':' after class declaration");
        node->addChild(make_shared<ParseTreeNode>("Delimiter", colon.value));
        
        node->addChild(parseBlockOrSimpleSuite());
        return node;
    }

    shared_ptr<ParseTreeNode> parseReturnStatement() {
        auto node = make_shared<ParseTreeNode>("ReturnStatement");
        
        // Parse 'return' keyword
        node->addChild(make_shared<ParseTreeNode>("Keyword", consume().value));
        
        // Parse optional return value
        if (!match(DELIMITER, ";") && currentPos < tokens.size()) {
            node->addChild(parseTest());
        }
        
        return node;
    }

    shared_ptr<ParseTreeNode> parsePassStatement() {
        auto node = make_shared<ParseTreeNode>("PassStatement");
        node->addChild(make_shared<ParseTreeNode>("Keyword", consume().value)); // 'pass'
        return node;
    }

    shared_ptr<ParseTreeNode> parseBreakStatement() {
        auto node = make_shared<ParseTreeNode>("BreakStatement");
        node->addChild(make_shared<ParseTreeNode>("Keyword", consume().value)); // 'break'
        return node;
    }

    shared_ptr<ParseTreeNode> parseContinueStatement() {
        auto node = make_shared<ParseTreeNode>("ContinueStatement");
        node->addChild(make_shared<ParseTreeNode>("Keyword", consume().value)); // 'continue'
        return node;
    }

    shared_ptr<ParseTreeNode> parseImportStatement() {
        auto node = make_shared<ParseTreeNode>("ImportStatement");
        
        // Parse 'import' or 'from' keyword
        node->addChild(make_shared<ParseTreeNode>("Keyword", consume().value));
        
        if (node->children[0]->value == "import") {
            // Parse module name
            node->addChild(parseDottedName());
            
            // Parse optional 'as' clause
            if (match(KEYWORD, "as")) {
                consume(); // consume 'as'
                node->addChild(make_shared<ParseTreeNode>("Alias", expect(IDENTIFIER, "Expected identifier after 'as'").value));
            }
            
            // Parse additional imports
            while (match(DELIMITER, ",")) {
                consume(); // consume ','
                node->addChild(parseDottedName());
                
                // Parse optional 'as' clause
                if (match(KEYWORD, "as")) {
                    consume(); // consume 'as'
                    node->addChild(make_shared<ParseTreeNode>("Alias", expect(IDENTIFIER, "Expected identifier after 'as'").value));
                }
            }
        } else if (node->children[0]->value == "from") {
            // Parse module name
            node->addChild(parseDottedName());
            
            // Parse 'import' keyword
            expect(KEYWORD, "import", "Expected 'import' after module name");
            
            // Parse '*' or specific imports
            if (match(OPERATOR, "*")) {
                node->addChild(make_shared<ParseTreeNode>("ImportAll", consume().value));
            } else {
                // Parse name to import
                node->addChild(make_shared<ParseTreeNode>("ImportName", expect(IDENTIFIER, "Expected name to import").value));
                
                // Parse optional 'as' clause
                if (match(KEYWORD, "as")) {
                    consume(); // consume 'as'
                    node->addChild(make_shared<ParseTreeNode>("Alias", expect(IDENTIFIER, "Expected identifier after 'as'").value));
                }
            }
        }
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseDottedName() {
        auto node = make_shared<ParseTreeNode>("DottedName");
        
        // Parse first part of the name
        node->addChild(make_shared<ParseTreeNode>("NamePart", expect(IDENTIFIER, "Expected identifier").value));
        
        // Parse additional parts
        while (match(DELIMITER, ".")) {
            // Add dot to parse tree
            Token dot = consume();
            node->addChild(make_shared<ParseTreeNode>("Delimiter", dot.value));
            
            node->addChild(make_shared<ParseTreeNode>("NamePart", expect(IDENTIFIER, "Expected identifier after '.'").value));
        }
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseAssignment() {
        auto node = make_shared<ParseTreeNode>("Assignment");
        
        // Parse identifier list (target)
        auto targetNode = make_shared<ParseTreeNode>("IdentifierList");
        
        // Check if the target is a simple identifier or an attribute access
        if (match(IDENTIFIER)) {
            size_t savedPos = currentPos;
            string name = consume().value;
            
            if (match(DELIMITER, ".")) {
                // It's an attribute access
                currentPos = savedPos;
                targetNode->addChild(parseAtomExpr());
            } else {
                // It's a simple identifier
                currentPos = savedPos;
                targetNode->addChild(make_shared<ParseTreeNode>("Identifier", consume().value));
            }
        } else {
            syntaxError("Expected identifier or attribute access");
        }
        
        while (match(DELIMITER, ",")) {
            consume(); // consume ','
            targetNode->addChild(make_shared<ParseTreeNode>("Identifier", expect(IDENTIFIER, "Expected identifier after ','").value));
        }
        
        node->addChild(targetNode);
        
        // Parse assignment operator
        string op = consume().value; // =, +=, -=, etc.
        node->addChild(make_shared<ParseTreeNode>("AssignOp", op));
        
        // Parse expression list (value)
        auto firstExpr = parseTest();
        if (match(DELIMITER, ",")) {
            auto valueNode = make_shared<ParseTreeNode>("ExpressionList");
            valueNode->addChild(firstExpr);
            while (match(DELIMITER, ",")) {
                consume(); // consume ','
                valueNode->addChild(parseTest());
            }
            node->addChild(valueNode);
        } else {
            node->addChild(firstExpr);
        }
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseFunctionCallStatement() {
        auto node = make_shared<ParseTreeNode>("FunctionCallStatement");
        
        // Parse function name (could be dotted)
        if (match(IDENTIFIER)) {
            size_t savedPos = currentPos;
            string name = consume().value;
            
            if (match(DELIMITER, ".")) {
                // It's a dotted name
                currentPos = savedPos;
                node->addChild(parseDottedName());
            } else {
                // It's a simple name
                currentPos = savedPos;
                node->addChild(make_shared<ParseTreeNode>("Identifier", consume().value));
            }
        } else {
            syntaxError("Expected function name");
        }
        
        // Add opening parenthesis to parse tree
        Token openParen = expect(DELIMITER, "(", "Expected '(' after function name");
        node->addChild(make_shared<ParseTreeNode>("Delimiter", openParen.value));
        
        auto argsNode = make_shared<ParseTreeNode>("Arguments");
        if (!match(DELIMITER, ")")) {
            argsNode->addChild(parseTest());
            
            while (match(DELIMITER, ",")) {
                // Add comma to parse tree
                Token comma = consume();
                argsNode->addChild(make_shared<ParseTreeNode>("Delimiter", comma.value));
                
                if (match(DELIMITER, ")")) break; // Handle trailing comma
                argsNode->addChild(parseTest());
            }
        }
        
        node->addChild(argsNode);
        
        // Add closing parenthesis to parse tree
        Token closeParen = expect(DELIMITER, ")", "Expected ')' after function arguments");
        node->addChild(make_shared<ParseTreeNode>("Delimiter", closeParen.value));
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseExpressionStatement() {
        auto node = make_shared<ParseTreeNode>("ExpressionStatement");
        node->addChild(parseTest());
        return node;
    }

    shared_ptr<ParseTreeNode> parseSuite() {
        auto node = make_shared<ParseTreeNode>("Suite");
        
        // Handle INDENT for block
        if ( match(NEWLINE)) {
            consume(); // consume INDENT
            if (match(INDENT)) {
                consume(); // consume NEWLINE
            } else {
                syntaxError("Expected INDENT after newline");
            }
            // Parse multiple statements until DEDENT
            while (!match(DEDENT) && currentPos < tokens.size()) {
                node->addChild(parseStatement());
            }
            // Accept DEDENT or EOF as valid end of block
            if (match(DEDENT)) {
                consume(); // consume DEDENT
            } else if (currentPos >= tokens.size()) {
                // Allow EOF as a valid end of block
            } else {
                syntaxError("Expected DEDENT at end of block");
            }        
        } else {
            // Simple statement after ':'
            node->addChild(parseStatement());
        }
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseTernaryOp() {
        auto thenExpr = parseOrTest();
        
        if (match(KEYWORD, "if")) {
            auto node = make_shared<ParseTreeNode>("TernaryOp");
            node->addChild(thenExpr);  // Value if true
            node->addChild(make_shared<ParseTreeNode>("Keyword", consume().value));  // 'if'
            node->addChild(parseOrTest());  // Condition
            
            expect(KEYWORD, "else", "Expected 'else' in conditional expression");
            node->addChild(make_shared<ParseTreeNode>("Keyword", "else"));
            node->addChild(parseTest());  // Value if false
            
            return node;
        }
        
        return thenExpr;
    }

    shared_ptr<ParseTreeNode> parseTest() {
        return parseTernaryOp();
    }

    shared_ptr<ParseTreeNode> parseOrTest() {
        auto node = parseAndTest();
        
        while (match(KEYWORD, "or")) {
            auto opNode = make_shared<ParseTreeNode>("BinaryOp", consume().value);
            opNode->addChild(node);
            opNode->addChild(parseAndTest());
            node = opNode;
        }
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseAndTest() {
        auto node = parseNotTest();
        
        while (match(KEYWORD, "and")) {
            auto opNode = make_shared<ParseTreeNode>("BinaryOp", consume().value);
            opNode->addChild(node);
            opNode->addChild(parseNotTest());
            node = opNode;
        }
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseNotTest() {
        if (match(KEYWORD, "not")) {
            auto node = make_shared<ParseTreeNode>("UnaryOp", consume().value);
            node->addChild(parseNotTest());
            return node;
        }
        
        return parseComparison();
    }

    shared_ptr<ParseTreeNode> parseComparison() {
        auto leftExpr = parseArithExpr();
        
        if (match(OPERATOR, "<") || match(OPERATOR, ">") || match(OPERATOR, "==") || 
            match(OPERATOR, ">=") || match(OPERATOR, "<=") || match(OPERATOR, "!=")) {
            
            // Create a flattened comparison node
            auto node = make_shared<ParseTreeNode>("Comparison");
            
            // Add left operand
            node->addChild(leftExpr);
            
            // Add operator
            Token op = consume();
            node->addChild(make_shared<ParseTreeNode>("ComparisonOp", op.value));
            
            // Add right operand
            auto rightExpr = parseArithExpr();
            node->addChild(rightExpr);
            
            return node;
        }
        
        return leftExpr;
    }

    shared_ptr<ParseTreeNode> parseArithExpr() {
        auto exprList = make_shared<ParseTreeNode>("ExpressionList");
        exprList->addChild(parseTerm());
        while (match(OPERATOR, "+") || match(OPERATOR, "-")) {
            exprList->addChild(make_shared<ParseTreeNode>("BinaryOp", consume().value));
            exprList->addChild(parseTerm());
        }
        return exprList->children.size() == 1 ? exprList->children[0] : exprList;
    }

    shared_ptr<ParseTreeNode> parseTerm() {
        auto node = parseFactor();
        
        while (match(OPERATOR, "*") || match(OPERATOR, "/") || match(OPERATOR, "//")) {
            auto opNode = make_shared<ParseTreeNode>("BinaryOp", consume().value);
            opNode->addChild(node);
            opNode->addChild(parseFactor());
            node = opNode;
        }
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseFactor() {
        if (match(OPERATOR, "+") || match(OPERATOR, "-") || match(OPERATOR, "~")) {
            auto node = make_shared<ParseTreeNode>("UnaryOp", consume().value);
            node->addChild(parseFactor());
            return node;
        }
        
        return parseAtomExpr();
    }

    // Modified parseAtomExpr method to include parentheses and dots
    shared_ptr<ParseTreeNode> parseAtomExpr() {
        auto node = parseAtom();
        
        // Parse trailers (function calls, attribute access, etc.)
        while (match(DELIMITER, "(") || match(DELIMITER, ".")) {
            if (match(DELIMITER, "(")) {
                auto callNode = make_shared<ParseTreeNode>("FunctionCall");
                callNode->addChild(node);
                
                // Add opening parenthesis to parse tree
                Token openParen = consume();
                callNode->addChild(make_shared<ParseTreeNode>("Delimiter", openParen.value));
                
                auto argsNode = make_shared<ParseTreeNode>("Arguments");
                
                if (!match(DELIMITER, ")")) {
                    argsNode->addChild(parseTest());
                    
                    while (match(DELIMITER, ",")) {
                        // Add comma to parse tree
                        Token comma = consume();
                        argsNode->addChild(make_shared<ParseTreeNode>("Delimiter", comma.value));
                        
                        if (match(DELIMITER, ")")) break; // Handle trailing comma
                        argsNode->addChild(parseTest());
                    }
                }
                
                callNode->addChild(argsNode);
                
                // Add closing parenthesis to parse tree
                Token closeParen = expect(DELIMITER, ")", "Expected ')' after function arguments");
                callNode->addChild(make_shared<ParseTreeNode>("Delimiter", closeParen.value));
                
                node = callNode;
            } else if (match(DELIMITER, ".")) {
                // Handle attribute access (method calls)
                // Add dot to parse tree
                Token dot = consume();
                
                // Parse attribute name
                auto attrNode = make_shared<ParseTreeNode>("AttributeAccess");
                attrNode->addChild(node); // The object
                attrNode->addChild(make_shared<ParseTreeNode>("Delimiter", dot.value)); // The dot
                
                // Get the attribute name
                if (match(IDENTIFIER)) {
                    attrNode->addChild(make_shared<ParseTreeNode>("Identifier", consume().value));
                } else {
                    syntaxError("Expected attribute name after '.'");
                }
                
                node = attrNode;
            }
        }
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseAtom() {
        if (match(DELIMITER, "(")) {
            Token openParen = consume();
            // Empty tuple
            if (match(DELIMITER, ")")) {
                Token closeParen = consume();
                auto tupleNode = make_shared<ParseTreeNode>("Tuple");
                tupleNode->addChild(make_shared<ParseTreeNode>("Delimiter", openParen.value));
                tupleNode->addChild(make_shared<ParseTreeNode>("Delimiter", closeParen.value));
                return tupleNode;
            }
            auto expr = parseTest();
            if (match(DELIMITER, ",")) {
                auto tupleNode = make_shared<ParseTreeNode>("Tuple");
                tupleNode->addChild(make_shared<ParseTreeNode>("Delimiter", openParen.value));
                tupleNode->addChild(expr);
                while (match(DELIMITER, ",")) {
                    Token comma = consume();
                    tupleNode->addChild(make_shared<ParseTreeNode>("Delimiter", comma.value));
                    if (match(DELIMITER, ")")) break;
                    tupleNode->addChild(parseTest());
                }
                Token closeParen = expect(DELIMITER, ")", "Expected ')' after tuple elements");
                tupleNode->addChild(make_shared<ParseTreeNode>("Delimiter", closeParen.value));
                return tupleNode;
            } else {
                Token closeParen = expect(DELIMITER, ")", "Expected ')' after expression");
                auto exprNode = make_shared<ParseTreeNode>("ParenExpr");
                exprNode->addChild(make_shared<ParseTreeNode>("Delimiter", openParen.value));
                exprNode->addChild(expr);
                exprNode->addChild(make_shared<ParseTreeNode>("Delimiter", closeParen.value));
                return exprNode;
            }
        } else if (match(DELIMITER, "[")) {
            auto listNode = make_shared<ParseTreeNode>("List");

            // Add opening bracket node
            Token openBracket = consume();
            listNode->addChild(make_shared<ParseTreeNode>("Delimiter", openBracket.value));

            if (!match(DELIMITER, "]")) {
                listNode->addChild(parseTest());
                while (match(DELIMITER, ",")) {
                    Token comma = consume();
                    listNode->addChild(make_shared<ParseTreeNode>("Delimiter", comma.value));
                    if (match(DELIMITER, "]")) break;
                    listNode->addChild(parseTest());
                }
            }

            // Add closing bracket node
            Token closeBracket = expect(DELIMITER, "]", "Expected ']' after list elements");
            listNode->addChild(make_shared<ParseTreeNode>("Delimiter", closeBracket.value));

            return listNode;
        } else if (match(DELIMITER, "{")) {
            // Dictionary
            auto dictNode = make_shared<ParseTreeNode>("Dict");
            
            // Add opening brace to parse tree
            Token openBrace = consume();
            dictNode->addChild(make_shared<ParseTreeNode>("Delimiter", openBrace.value));
            
            if (!match(DELIMITER, "}")) {
                // Parse key-value pair
                auto key = parseTest();
                
                // Add colon to parse tree
                Token colon = expect(DELIMITER, ":", "Expected ':' after dictionary key");
                
                auto value = parseTest();
                
                auto pairNode = make_shared<ParseTreeNode>("KeyValuePair");
                pairNode->addChild(key);
                pairNode->addChild(make_shared<ParseTreeNode>("Delimiter", colon.value));
                pairNode->addChild(value);
                dictNode->addChild(pairNode);
                
                while (match(DELIMITER, ",")) {
                    // Add comma to parse tree
                    Token comma = consume();
                    dictNode->addChild(make_shared<ParseTreeNode>("Delimiter", comma.value));
                    
                    if (match(DELIMITER, "}")) break; // Handle trailing comma
                    
                    // Parse key-value pair
                    key = parseTest();
                    
                    // Add colon to parse tree
                    colon = expect(DELIMITER, ":", "Expected ':' after dictionary key");
                    
                    value = parseTest();
                    
                    pairNode = make_shared<ParseTreeNode>("KeyValuePair");
                    pairNode->addChild(key);
                    pairNode->addChild(make_shared<ParseTreeNode>("Delimiter", colon.value));
                    pairNode->addChild(value);
                    dictNode->addChild(pairNode);
                }
            }
            
            // Add closing brace to parse tree
            Token closeBrace = expect(DELIMITER, "}", "Expected '}' after dictionary elements");
            dictNode->addChild(make_shared<ParseTreeNode>("Delimiter", closeBrace.value));
            
            return dictNode;
        } else if (match(IDENTIFIER)) {
            return make_shared<ParseTreeNode>("Identifier", consume().value);
        } else if (match(LITERAL)) {
            return make_shared<ParseTreeNode>("Literal", consume().value);
        } else if (match(KEYWORD, "None") || match(KEYWORD, "True") || match(KEYWORD, "False")) {
            return make_shared<ParseTreeNode>("Keyword", consume().value);
        } else if (currentPos >= tokens.size()) {
            syntaxError("Unexpected end of input (EOF) while parsing expression");
        } else {
            syntaxError("Expected expression");
        }
        return nullptr;
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
    
    // Save parse tree to DOT file for visualization
    bool saveTreeToDot(const string& filename) const {
        if (!parseTree) {
            cerr << "No parse tree available to save." << endl;
            return false;
        }
        
        ofstream dotFile(filename);
        if (!dotFile) {
            cerr << "Failed to open file: " << filename << endl;
            return false;
        }
        
        // Write DOT file header
        dotFile << "digraph ParseTree {" << endl;
        dotFile << "  node [shape=box, fontname=\"Arial\", fontsize=10];" << endl;
        
        // Generate DOT representation of the tree
        int nodeId = 0;
        parseTree->toDot(dotFile, nodeId);
        
        // Write DOT file footer
        dotFile << "}" << endl;
        
        dotFile.close();
        cout << "Parse tree saved to " << filename << endl;
        return true;
    }
};

// Helper function to convert hex color to ImVec4
ImVec4 hexToImVec4(const std::string& hex, float darken = 1.0f) {
    std::string cleanHex = hex.substr(hex[0] == '#' ? 1 : 0);
    unsigned int rgb;
    std::stringstream ss;
    ss << std::hex << cleanHex;
    ss >> rgb;
    float r = ((rgb >> 16) & 0xFF) / 255.0f * darken;
    float g = ((rgb >> 8) & 0xFF) / 255.0f * darken;
    float b = (rgb & 0xFF) / 255.0f * darken;
    return ImVec4(r, g, b, 1.0f);
}
GLuint LoadTextureFromFile(const char* filename) {
    int width, height, channels;
    unsigned char* data = stbi_load(filename, &width, &height, &channels, 4); // Force RGBA
    if (!data) {
        std::cerr << "Failed to load image: " << filename << std::endl;
        return 0;
    }

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Upload pixels
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    // Texture settings
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return textureID;
}

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Create window with increased size
    GLFWwindow* window = glfwCreateWindow(3000, 3000,"Python Lexer and Parser Analyzer", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwMaximizeWindow(window); // Maximize window on startup

    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize OpenGL loader" << std::endl;
        return -1;
    }

    // Setup Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    // Load 20pt Times New Roman (regular and bold)
    io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/times.ttf", 20.0f);
    io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/timesbd.ttf", 20.0f);
    io.FontDefault = io.Fonts->Fonts[1]; // Set bold font as default

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(20, 20);
    style.ItemSpacing = ImVec2(15, 15);
    style.ScrollbarSize = 20.0f;

    // Theme toggle state
    bool isDarkMode = false;

    // Define colors from palette
    const ImVec4 color1 = hexToImVec4("#A53860"); // Rich Pink (text in light mode)
    const ImVec4 color2 = hexToImVec4("#3A0519"); // Deep Burgundy (headers, borders)
    const ImVec4 color3 = hexToImVec4("#FFFFFF"); // White (light mode background)
    const ImVec4 color4 = hexToImVec4("#F8D7E3"); // Very Light Pink (table rows)
    const ImVec4 color5 = hexToImVec4("#EF88AD"); // Light Pink (hovered/active, text in dark mode)
    const ImVec4 color6 = hexToImVec4("#333333"); // Dark Grey (dark mode background)

    // Function to apply light theme
    auto applyLightTheme = [&]() {
        ImVec4* colors = ImGui::GetStyle().Colors;
        colors[ImGuiCol_WindowBg] = color3; // #FFFFFF
        colors[ImGuiCol_ChildBg] = color3; // #FFFFFF
        colors[ImGuiCol_FrameBg] = color3; // #FFFFFF
        colors[ImGuiCol_FrameBgHovered] = color5; // #EF88AD
        colors[ImGuiCol_FrameBgActive] = color5; // #EF88AD
        colors[ImGuiCol_Text] = color1; // #A53860
        colors[ImGuiCol_Header] = color2; // #3A0519
        colors[ImGuiCol_TableBorderStrong] = color2; // #3A0519
        colors[ImGuiCol_TableBorderLight] = ImVec4(color2.x, color2.y, color2.z, 0.5f);
        colors[ImGuiCol_TableRowBg] = color3; // #FFFFFF
        colors[ImGuiCol_TableRowBgAlt] = color4; // #F8D7E3
        colors[ImGuiCol_Button] = color2; // #3A0519
        colors[ImGuiCol_ButtonHovered] = color5; // #EF88AD
        colors[ImGuiCol_ButtonActive] = color5; // #EF88AD
        colors[ImGuiCol_Tab] = color3; // #FFFFFF
        colors[ImGuiCol_TabHovered] = color5; // #EF88AD
        colors[ImGuiCol_TabActive] = color2; // #3A0519
        colors[ImGuiCol_TabUnfocused] = color3; // #FFFFFF
        colors[ImGuiCol_TabUnfocusedActive] = color2; // #3A0519
        colors[ImGuiCol_TitleBg] = color2; // #3A0519
        colors[ImGuiCol_TitleBgActive] = color2; // #3A0519
        colors[ImGuiCol_TitleBgCollapsed] = color2; // #3A0519
        std::cout << "Light mode applied: WindowBg = #FFFFFF" << std::endl;
    };

    // Function to apply dark theme
    auto applyDarkTheme = [&]() {
        ImVec4* colors = ImGui::GetStyle().Colors;
        colors[ImGuiCol_WindowBg] = color6; // #333333
        colors[ImGuiCol_ChildBg] = color6; // #333333
        colors[ImGuiCol_FrameBg] = hexToImVec4("#3C3C3C"); // Slightly lighter grey
        colors[ImGuiCol_FrameBgHovered] = color5; // #EF88AD
        colors[ImGuiCol_FrameBgActive] = color5; // #EF88AD
        colors[ImGuiCol_Text] = color5; // #EF88AD
        colors[ImGuiCol_Header] = color2; // #3A0519
        colors[ImGuiCol_TableBorderStrong] = color2; // #3A0519
        colors[ImGuiCol_TableBorderLight] = ImVec4(color2.x, color2.y, color2.z, 0.5f);
        colors[ImGuiCol_TableRowBg] = color6; // #333333
        colors[ImGuiCol_TableRowBgAlt] = hexToImVec4("#F8D7E3", 0.8f); // Darkened #F8D7E3
        colors[ImGuiCol_Button] = color2; // #3A0519
        colors[ImGuiCol_ButtonHovered] = color5; // #EF88AD
        colors[ImGuiCol_ButtonActive] = color5; // #EF88AD
        colors[ImGuiCol_Tab] = color6; // #333333
        colors[ImGuiCol_TabHovered] = color5; // #EF88AD
        colors[ImGuiCol_TabActive] = color2; // #3A0519
        colors[ImGuiCol_TabUnfocused] = color6; // #333333
        colors[ImGuiCol_TabUnfocusedActive] = color2; // #3A0519
        colors[ImGuiCol_TitleBg] = color2; // #3A0519
        colors[ImGuiCol_TitleBgActive] = color2; // #3A0519
        colors[ImGuiCol_TitleBgCollapsed] = color2; // #3A0519
        std::cout << "Dark mode applied: WindowBg = #333333" << std::endl;
    };

    // Apply initial theme (light)
    applyLightTheme();

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // User input variables
    char inputCode[8192] = "# Enter your Python code here\n\ndef main():\n    print('Hello world!')\n    x = 42\n\nif __name__ == '__main__':\n    main()";
    bool analyzeClicked = false;
    bool exporttree = false;
    Lexer lexer;
    Parser* parser = nullptr;
    std::string parseTreeText;
    std::string dotFileContent;
    std::vector<std::string> errorMessages;

    GLuint treeTexture = 0;
    int treeImageWidth = 0, treeImageHeight = 0;
    bool imageLoaded = false;


    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Menu Bar
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Load Code")) {
                    std::ifstream file("input.py");
                    if (file) {
                        std::stringstream buffer;
                        buffer << file.rdbuf();
                        std::string content = buffer.str();
                        if (content.size() < sizeof(inputCode)) {
                            strncpy(inputCode, content.c_str(), sizeof(inputCode) - 1);
                            inputCode[sizeof(inputCode) - 1] = '\0';
                        } else {
                            errorMessages.push_back("Error: Input file too large for buffer.");
                        }
                        file.close();
                    } else {
                        errorMessages.push_back("Error: Could not open input.py.");
                    }
                }
                if (ImGui::MenuItem("Save Code")) {
                    std::ofstream file("output.py");
                    if (file) {
                        file << inputCode;
                        file.close();
                    } else {
                        errorMessages.push_back("Error: Could not save to output.py.");
                    }
                }
                if (ImGui::MenuItem("Export DOT")) {
                    if (!dotFileContent.empty() && dotFileContent != "No parse tree available." && 
                        dotFileContent != "Failed to read tree.dot") {
                        std::ofstream file("exported_tree.dot");
                        if (file) {
                            file << dotFileContent;
                            file.close();
                        } else {
                            errorMessages.push_back("Error: Could not export to exported_tree.dot.");
                        }
                    } else {
                        errorMessages.push_back("Error: No DOT file available to export.");
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("Toggle Dark/Light Mode")) {
                    isDarkMode = !isDarkMode;
                    if (isDarkMode) {
                        applyDarkTheme();
                    } else {
                        applyLightTheme();
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // Main window (non-resizable)
        ImGui::Begin("Python Lexer and Parser Analyzer", nullptr, 
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize);
        
        // Code input section
        ImGui::TextColored(isDarkMode ? color5 : color1, "PYTHON CODE INPUT:");

        ImGui::InputTextMultiline("##source", inputCode, IM_ARRAYSIZE(inputCode), 
            ImVec2(-1, ImGui::GetTextLineHeight() * 15), 
            ImGuiInputTextFlags_AllowTabInput);
        
        if (ImGui::Button("ANALYZE CODE", ImVec2(-1, 40))) {
            lexer = Lexer();
            if (parser) delete parser;
            parseTreeText.clear();
            dotFileContent.clear();
            errorMessages.clear();

            
            std::stringstream errorStream;
            std::streambuf* oldCerr = std::cerr.rdbuf(errorStream.rdbuf());

            std::ofstream tmp("_temp_lexer_input.py");
            tmp << inputCode;
            tmp.close();
            
            lexer.parser("_temp_lexer_input.py");
            try
            {
                lexer.tokenizeLine(lexer.getcodelines());
                std::vector<Token> tokens = lexer.getTokens();
                parser = new Parser(tokens);
                auto parseTree = parser->parse();
                std::stringstream treeStream;
                std::streambuf* oldCout = std::cout.rdbuf(treeStream.rdbuf());
                if (parseTree) {
                    parser->printParseTree();
                }
                std::cout.rdbuf(oldCout);
                parseTreeText = treeStream.str();
    
                if (parseTree) {
                    parser->saveTreeToDot("tree.dot");
                    std::ifstream dotFile("tree.dot");
                    if (dotFile) {
                        std::stringstream dotStream;
                        dotStream << dotFile.rdbuf();
                        dotFileContent = dotStream.str();
                        dotFile.close();
                    } else {
                        dotFileContent = "Failed to read tree.dot";
                    }
                } else {
                    dotFileContent = "No parse tree available.";
                }
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
            }
            

            std::string errors = errorStream.str();
            std::stringstream errorLines(errors);
            std::string errorLine;
            while (std::getline(errorLines, errorLine)) {
                errorMessages.push_back(errorLine);
            }
            for (const auto& token : lexer.getTokens()) {
                if (token.type == ERROR) {
                    errorMessages.push_back("Line " + std::to_string(token.line) + " : " + token.value + " - " + tokenTypeToString(token.type));
                }
            }
            std::cerr.rdbuf(oldCerr);

            analyzeClicked = true;
            exporttree = true;
            
        }

      if (analyzeClicked) {
    if (exporttree) {
        std::ofstream file("exported_tree.dot");
#if defined(_WIN32)
        FILE* pipe = _popen("dot -Tpng tree.dot -o tree.png", "r");
#else
        FILE* pipe = popen("dot -Tpng tree.dot -o tree.png", "r");
#endif
        if (!pipe) {
            errorMessages.push_back("Error: Could not generate PNG from DOT file.");
        } else {
            std::cout << "Exported tree.png from tree.dot" << std::endl;
#if defined(_WIN32)
            _pclose(pipe);
#else
            pclose(pipe);
#endif
            if (!imageLoaded) {
                treeTexture = LoadTextureFromFile("tree.png");
                if (treeTexture != 0) {
                    imageLoaded = true;
                } else {
                    errorMessages.push_back("Error: Failed to load tree.png as texture.");
                }
            }
        }
        exporttree = false;
    }

    ImGui::BeginTabBar("ResultsTabs", ImGuiTabBarFlags_NoCloseWithMiddleMouseButton);

    if (ImGui::BeginTabItem("Tokens")) {
        ImGui::BeginChild("TokensChild", ImVec2(0, 0), true);
        for (const auto& token : lexer.getTokens()) {
            ImGui::Text("%-6d %-15s %-20s",
                token.line,
                tokenTypeToString(token.type).c_str(),
                token.value.c_str());
        }
        ImGui::EndChild();
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Symbol Table")) {
        ImGui::BeginChild("SymbolsChild", ImVec2(0, 0), true);
        if (ImGui::BeginTable("SymbolTable", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableHeadersRow();
            for (const auto& id : lexer.getsymbols()) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("%d", id.ID);
                ImGui::TableSetColumnIndex(1); ImGui::Text("%s", id.name.c_str());
                ImGui::TableSetColumnIndex(2); ImGui::Text("%s", id.type.c_str());
                ImGui::TableSetColumnIndex(3); ImGui::Text("%s", id.Scope.c_str());
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Errors")) {
        ImGui::BeginChild("ErrorsChild", ImVec2(0, 0), true);
        for (const auto& error : errorMessages) {
            ImGui::TextColored(isDarkMode ? color5 : color1, "%s", error.c_str());
        }
        ImGui::EndChild();
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Parse Tree")) {
    ImGui::BeginChild("ParseTreeChild", ImVec2(0, 0), true);
    if (imageLoaded && treeTexture) {
        ImGui::Text("Parse Tree Image:");
        // Make the image wider (e.g., 1200x700)
        ImGui::Image((ImTextureID)(intptr_t)treeTexture, ImVec2(1200, 700));
    } else {
        ImGui::TextWrapped("%s", parseTreeText.c_str());
    }
    ImGui::EndChild();
    ImGui::EndTabItem();
}


    if (ImGui::BeginTabItem("DOT File")) {
        ImGui::BeginChild("DotFileChild", ImVec2(0, 0), true);
        ImGui::Text("%s", dotFileContent.c_str());
        ImGui::EndChild();
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}


        ImGui::End();

        // Rendering
        ImGui::Render();
        glClearColor(isDarkMode ? color6.x : color3.x, 
                     isDarkMode ? color6.y : color3.y, 
                     isDarkMode ? color6.z : color3.z, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    
    // Cleanup
    if (parser) delete parser;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}