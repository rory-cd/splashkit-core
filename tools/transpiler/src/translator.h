#pragma once

#include <string>
#include <vector>
#include <unordered_set>

#include "ast.h"

namespace fs = std::filesystem;

class Translator
{
public:
    virtual void translateASTSet(
        const std::vector<CustomAST> &ASTs,
        const fs::path &outputDir,
        const std::string &projectName) = 0;
};

class CSharpTranslator : Translator
{
private:
    // Global declaration management
    const std::string globalClassName = "GeneratedGlobals";  
    std::unordered_set<std::string> globals;

    // Indentation helpers
    int indentLevel = 0;
    std::string indt() {
        std::string result;
        for (int i = 0; i < indentLevel; i++)
            result += "    ";
        return result;
    }
    void increaseIndent() { indentLevel++; }
    void decreaseIndent() { indentLevel--; }

    // Write functions (write the AST elements to an external file)
    void writeProjectFiles(const fs::path &outputDir, const std::string &projectName);
    void writeAST(const CustomAST &AST, const std::string &projectName, const fs::path &outputFilepath);
    void writeGlobals(const CustomAST &AST, std::ofstream &file);
    void writeTestCase(const TestCase &testCase, std::ofstream &file, std::string category);
    void writeSection(const Section &section, std::ofstream &file);
    void writeAssertion(const AssertionStatement &assertion, std::ofstream &file);
    void writeStatement(const Statement &statement, std::ofstream &file);
    void writeReturnStmt(const ReturnStatement &returnStmt, std::ofstream &file);
    void writeVarDeclStmt(const VariableDeclarationStatement &varDeclStmt, std::ofstream &file);
    void writeBody(const std::vector<std::shared_ptr<Statement>> &body, std::ofstream &file);
    void writeExprStmt(const ExpressionStatement &exprStmt, std::ofstream &file);
    void writeFunctionDecl(const FunctionDeclaration &funcDecl, std::ofstream &file);

    // Translation functions (return a string to be written inline)
    std::string translateParameter(const Parameter &parameter);
    std::string translateVarDecl(const VariableDeclaration &varDecl, std::ofstream &file);
    std::string translateExpression(const Expression &expr);
    std::string translateBinaryExpr(const BinaryExpression &expr);
    std::string translateUnaryExpr(const UnaryExpression &expr);
    std::string translateCallExpr(const CallExpression &expr);
    std::string translateRefExpr(const ReferenceExpression &expr);
    std::string translateLiteralExpr(const LiteralExpression &expr);
    
    // String manipulation
    std::string toCamelCase(const std::string &name);
    std::string toPascalCase(const std::string &name);
    std::string translateType(const std::string &cppType);

public:
    void translateASTSet(
        const std::vector<CustomAST> &ASTs,
        const fs::path &outputDir,
        const std::string &projectName = "SplashKit.CSharp.UnitTests") override;
};
