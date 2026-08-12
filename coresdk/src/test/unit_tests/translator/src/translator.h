#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

using json = nlohmann::json;

class Translator
{
public:
    virtual void translateFiles(
        const std::vector<fs::path> &filepaths,
        const fs::path &outputDir,
        const std::string &projectName) = 0;
};

class CSharpTranslator : Translator
{
private:
    void writeProjectFiles(const fs::path &outputDir, const std::string &projectName);
    void translateAST(const json &AST, const std::string &projectName, fs::path &outputFilepath);
    void translateTestCase(const json &testCase, std::ofstream &file, std::string &category);
    void translateSection(const json &section, std::ofstream &file);
    void translateAssertion(const json &assertion, std::ofstream &file);
    void translateStatement(const json &statement, std::ofstream &file);
    void translateReturnStatement(const json &returnStmt, std::ofstream &file);
    void translateVarDeclStatement(const json &varDeclStmt, std::ofstream &file);
    void translateExprStatement(const json &exprStmt, std::ofstream &file);
    void translateFunctionDecl(const json &funcDecl, std::ofstream &file);
    void translateParameter(const json &param, std::ofstream &file);
    void translateVarDecl(const json &varDecl, std::ofstream &file);
    void translateExpression(const json &expr, std::ofstream &file);
    void translateBinaryExpr(const json &expr, std::ofstream &file);
    void translateCallExpr(const json &expr, std::ofstream &file);
    void translateRefExpr(const json &expr, std::ofstream &file);
    void translateLiteralExpr(const json &expr, std::ofstream &file);
    std::string toPascalCase(const std::string &name);

public:
    void translateFiles(
        const std::vector<fs::path> &filepaths,
        const fs::path &outputDir,
        const std::string &projectName = "SplashKit.CSharp.UnitTests") override;
};
