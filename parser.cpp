#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include "definitions.h"
#include "lexer2.cpp"
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
            try {
                node->addChild(parseStatement());
            } catch (const runtime_error& e) {
                cerr << "Syntax Error " << e.what() << endl;
                exit(EXIT_FAILURE);
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
            // Look ahead to see if this is an assignment
            size_t savedPos = currentPos;
            consume(); // consume identifier
            
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

    shared_ptr<ParseTreeNode> parseIfStatement() {
        auto node = make_shared<ParseTreeNode>("IfStatement");
        
        // Parse 'if' keyword
        node->addChild(make_shared<ParseTreeNode>("Keyword", consume().value));
        
        // Parse condition
        node->addChild(parseTest());
        
        // Parse ':'
        expect(DELIMITER, ":", "Expected ':' after if condition");
        
        // Parse then-block
        node->addChild(parseSuite());
        
        // Parse optional elif blocks
        while (match(KEYWORD, "elif")) {
            auto elifNode = make_shared<ParseTreeNode>("ElifClause");
            elifNode->addChild(make_shared<ParseTreeNode>("Keyword", consume().value));
            elifNode->addChild(parseTest());
            expect(DELIMITER, ":", "Expected ':' after elif condition");
            elifNode->addChild(parseSuite());
            node->addChild(elifNode);
        }
        
        // Parse optional else-block
        if (match(KEYWORD, "else")) {
            auto elseNode = make_shared<ParseTreeNode>("ElseClause");
            elseNode->addChild(make_shared<ParseTreeNode>("Keyword", consume().value));
            expect(DELIMITER, ":", "Expected ':' after 'else'");
            elseNode->addChild(parseSuite());
            node->addChild(elseNode);
        }
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseWhileStatement() {
        auto node = make_shared<ParseTreeNode>("WhileStatement");
        
        // Parse 'while' keyword
        node->addChild(make_shared<ParseTreeNode>("Keyword", consume().value));
        
        // Parse condition
        node->addChild(parseTest());
        
        // Parse ':'
        expect(DELIMITER, ":", "Expected ':' after while condition");
        
        // Parse body
        node->addChild(parseSuite());
        
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
        node->addChild(parseTest());
        
        // Parse ':'
        expect(DELIMITER, ":", "Expected ':' after for statement");
        
        // Parse body
        node->addChild(parseSuite());
        
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
                    if (match(DELIMITER, ")")) break; // Handle trailing comma
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
        node->addChild(parseSuite());
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseClassDef() {
        auto node = make_shared<ParseTreeNode>("ClassDefinition");
        
        // Parse 'class' keyword
        node->addChild(make_shared<ParseTreeNode>("Keyword", consume().value));
        
        // Parse class name
        node->addChild(make_shared<ParseTreeNode>("Identifier", expect(IDENTIFIER, "Expected class name after 'class'").value));
        
        // Parse optional parent class
        if (match(DELIMITER, "(")) {
            consume(); // consume '('
            node->addChild(make_shared<ParseTreeNode>("Parent", expect(IDENTIFIER, "Expected parent class name").value));
            expect(DELIMITER, ")", "Expected ')' after parent class name");
        }
        
        // Parse ':'
        expect(DELIMITER, ":", "Expected ':' after class declaration");
        
        // Parse class body
        node->addChild(parseSuite());
        
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
            consume(); // consume '.'
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
        auto valueNode = make_shared<ParseTreeNode>("ExpressionList");
        valueNode->addChild(parseTest());
        
        while (match(DELIMITER, ",")) {
            consume(); // consume ','
            valueNode->addChild(parseTest());
        }
        
        node->addChild(valueNode);
        
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
        
        // Parse arguments
        expect(DELIMITER, "(", "Expected '(' after function name");
        
        auto argsNode = make_shared<ParseTreeNode>("Arguments");
        if (!match(DELIMITER, ")")) {
            argsNode->addChild(parseTest());
            
            while (match(DELIMITER, ",")) {
                consume(); // consume ','
                if (match(DELIMITER, ")")) break; // Handle trailing comma
                argsNode->addChild(parseTest());
            }
        }
        
        node->addChild(argsNode);
        expect(DELIMITER, ")", "Expected ')' after function arguments");
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseExpressionStatement() {
        auto node = make_shared<ParseTreeNode>("ExpressionStatement");
        node->addChild(parseTest());
        return node;
    }

    shared_ptr<ParseTreeNode> parseSuite() {
        auto node = make_shared<ParseTreeNode>("Suite");
        
        // In a real Python parser, you would handle indentation here
        // For simplicity, we'll just parse a single statement
        node->addChild(parseStatement());
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseTest() {
        return parseOrTest();
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
        auto node = parseArithExpr();
        
        while (match(OPERATOR, "<") || match(OPERATOR, ">") || match(OPERATOR, "==") || 
               match(OPERATOR, ">=") || match(OPERATOR, "<=") || match(OPERATOR, "!=")) {
            auto opNode = make_shared<ParseTreeNode>("ComparisonOp", consume().value);
            opNode->addChild(node);
            opNode->addChild(parseArithExpr());
            node = opNode;
        }
        
        return node;
    }

    shared_ptr<ParseTreeNode> parseArithExpr() {
        auto node = parseTerm();
        
        while (match(OPERATOR, "+") || match(OPERATOR, "-")) {
            auto opNode = make_shared<ParseTreeNode>("BinaryOp", consume().value);
            opNode->addChild(node);
            opNode->addChild(parseTerm());
            node = opNode;
        }
        
        return node;
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

    shared_ptr<ParseTreeNode> parseAtomExpr() {
        auto node = parseAtom();
        
        // Parse trailers (function calls, attribute access, etc.)
        while (match(DELIMITER, "(") || match(DELIMITER, ".")) {
            if (match(DELIMITER, "(")) {
                auto callNode = make_shared<ParseTreeNode>("FunctionCall");
                callNode->addChild(node);
                
                // Parse arguments
                consume(); // consume '('
                auto argsNode = make_shared<ParseTreeNode>("Arguments");
                
                if (!match(DELIMITER, ")")) {
                    argsNode->addChild(parseTest());
                    
                    while (match(DELIMITER, ",")) {
                        consume(); // consume ','
                        if (match(DELIMITER, ")")) break; // Handle trailing comma
                        argsNode->addChild(parseTest());
                    }
                }
                
                callNode->addChild(argsNode);
                expect(DELIMITER, ")", "Expected ')' after function arguments");
                
                node = callNode;
            } else if (match(DELIMITER, ".")) {
                // Handle attribute access (method calls)
                consume(); // consume '.'
                
                // Parse attribute name
                auto attrNode = make_shared<ParseTreeNode>("AttributeAccess");
                attrNode->addChild(node); // The object
                
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
            consume(); // consume '('
            
            // Empty tuple
            if (match(DELIMITER, ")")) {
                consume();
                return make_shared<ParseTreeNode>("Tuple", "()");
            }
            
            // Parse expression or tuple
            auto expr = parseTest();
            
            if (match(DELIMITER, ",")) {
                // It's a tuple
                auto tupleNode = make_shared<ParseTreeNode>("Tuple");
                tupleNode->addChild(expr);
                
                while (match(DELIMITER, ",")) {
                    consume(); // consume ','
                    if (match(DELIMITER, ")")) break; // Handle trailing comma
                    tupleNode->addChild(parseTest());
                }
                
                expect(DELIMITER, ")", "Expected ')' after tuple elements");
                return tupleNode;
            } else {
                // It's just a parenthesized expression
                expect(DELIMITER, ")", "Expected ')' after expression");
                return expr;
            }
        } else if (match(DELIMITER, "[")) {
            // List
            auto listNode = make_shared<ParseTreeNode>("List");
            
            consume(); // consume '['
            if (!match(DELIMITER, "]")) {
                listNode->addChild(parseTest());
                
                while (match(DELIMITER, ",")) {
                    consume(); // consume ','
                    if (match(DELIMITER, "]")) break; // Handle trailing comma
                    listNode->addChild(parseTest());
                }
            }
            
            expect(DELIMITER, "]", "Expected ']' after list elements");
            return listNode;
        } else if (match(DELIMITER, "{")) {
            // Dictionary
            auto dictNode = make_shared<ParseTreeNode>("Dict");
            
            consume(); // consume '{'
            if (!match(DELIMITER, "}")) {
                // Parse key-value pair
                auto key = parseTest();
                expect(DELIMITER, ":", "Expected ':' after dictionary key");
                auto value = parseTest();
                
                auto pairNode = make_shared<ParseTreeNode>("KeyValuePair");
                pairNode->addChild(key);
                pairNode->addChild(value);
                dictNode->addChild(pairNode);
                
                while (match(DELIMITER, ",")) {
                    consume(); // consume ','
                    if (match(DELIMITER, "}")) break; // Handle trailing comma
                    
                    // Parse key-value pair
                    key = parseTest();
                    expect(DELIMITER, ":", "Expected ':' after dictionary key");
                    value = parseTest();
                    
                    pairNode = make_shared<ParseTreeNode>("KeyValuePair");
                    pairNode->addChild(key);
                    pairNode->addChild(value);
                    dictNode->addChild(pairNode);
                }
            }
            
            expect(DELIMITER, "}", "Expected '}' after dictionary elements");
            return dictNode;
        } else if (match(IDENTIFIER)) {
            return make_shared<ParseTreeNode>("Identifier", consume().value);
        } else if (match(LITERAL)) {
            return make_shared<ParseTreeNode>("Literal", consume().value);
        } else if (match(KEYWORD, "None") || match(KEYWORD, "True") || match(KEYWORD, "False")) {
            return make_shared<ParseTreeNode>("Keyword", consume().value);
        } else {
            syntaxError("Expected expression");
            return nullptr; // This will never be reached due to the exception
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

int main() {
    Lexer lexer;
    lexer.parser("example.py");
    lexer.tokenizeLine(lexer.getcodelines());
    lexer.printTables();
    vector<Token> tokens = lexer.getTokens();

    Parser parser(tokens);
    auto parseTree = parser.parse();
    
    if (parseTree) {
        cout << "Parsing successful! Parse tree:" << endl;
        parser.printParseTree();
        
        // Save the parse tree to a DOT file
        parser.saveTreeToDot("tree.dot");
    }

    return 0;
}