#pragma once

#include <string>
#include <unordered_map>
#include <vector>

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
    static const std::unordered_map<std::string, std::string> typeMap;

    int indentLevel = 0;
    std::string indt() {
        std::string result;
        for (int i = 0; i < indentLevel; i++)
            result += "    ";
        return result;
    }
    void increaseIndent() { indentLevel++; }
    void decreaseIndent() { indentLevel--; }

    void writeProjectFiles(const fs::path &outputDir, const std::string &projectName);
    void translateAST(const CustomAST &AST, const std::string &projectName, const fs::path &outputFilepath);
    void writeTestCase(const TestCase &testCase, std::ofstream &file, std::string &category, const std::vector<std::unique_ptr<VariableDeclarationStatement>> &globals, const std::vector<std::unique_ptr<FunctionDeclaration>> &functions);
    void translateSection(const Section &section, std::ofstream &file);
    void translateAssertion(const AssertionStatement &assertion, std::ofstream &file);
    void translateStatement(const Statement &statement, std::ofstream &file);
    void translateReturnStatement(const ReturnStatement &returnStmt, std::ofstream &file);
    void writeVarDeclStmt(const VariableDeclarationStatement &varDeclStmt, std::ofstream &file);
    void translateExprStatement(const ExpressionStatement &exprStmt, std::ofstream &file);
    void writeFunctionDecl(const FunctionDeclaration &funcDecl, std::ofstream &file);
    std::string translateParameter(const Parameter &parameter);
    std::string translateVarDecl(const VariableDeclaration &varDecl, std::ofstream &file);
    std::string translateExpression(const Expression &expr);
    std::string translateBinaryExpr(const BinaryExpression &expr);
    std::string translateCallExpr(const CallExpression &expr);
    std::string translateRefExpr(const ReferenceExpression &expr);
    std::string translateLiteralExpr(const LiteralExpression &expr);

    std::string toPascalCase(const std::string &name);
    std::string translateType(const std::string &cppType);

public:
    void translateASTSet(
        const std::vector<CustomAST> &ASTs,
        const fs::path &outputDir,
        const std::string &projectName = "SplashKit.CSharp.UnitTests") override;
};
