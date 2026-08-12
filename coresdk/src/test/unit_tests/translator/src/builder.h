#pragma once

#include <clang/AST/RecursiveASTVisitor.h>
#include "ast.h"
#include "macro.h"

// Builds a data structure for a custom AST
class ASTBuilder
{
private:
    // Need source manager for info about the source file (like line numbers)
    clang::ASTContext &context;
    clang::SourceManager &sourceManager;
    std::unordered_map<unsigned, MacroInfo> &macros;

public:
    ASTBuilder(clang::ASTContext &context, std::unordered_map<unsigned, MacroInfo> &macros)
        : context(context),
          sourceManager(context.getSourceManager()),
          macros(macros)
    {
    }

    // Gets the line number of a declaration from the source file
    int getLineNumber(const clang::Decl *decl);

    unsigned getLocationKey(clang::SourceLocation loc);

    // Gets the source text of a given expression (e.g. "x + 5")  
    std::string getSourceText(clang::Expr *expr);

    // Build a literal with a string value 
    std::unique_ptr<Expression> buildLiteral(clang::Expr *expr);

    // Build a binary expression
    std::unique_ptr<Expression> buildBinaryExpression(clang::BinaryOperator *binary);

    // Build a function call 
    std::unique_ptr<Expression> buildFunctionCall(clang::CallExpr *call, std::string name);

    // Build a reference to something already declared
    std::unique_ptr<Expression> buildReference(clang::DeclRefExpr *ref);

    // Expression dispatcher
    std::unique_ptr<Expression> buildExpression(clang::Expr *expr);

    // Build a variable declaration
    std::unique_ptr<VariableDeclaration> buildVariableDecl(clang::VarDecl *var);

    std::unique_ptr<VariableDeclarationStatement> buildVariableDeclStmt(clang::VarDecl *var);

    // Build a parameter
    std::unique_ptr<Parameter> buildParameter(clang::ParmVarDecl *param);

    // Build a function declaration
    std::unique_ptr<FunctionDeclaration> buildFunctionDecl(clang::FunctionDecl *fn);

    void checkMinAndMax(clang::SourceLocation loc, clang::SourceLocation &min, clang::SourceLocation &max);

    // Recurse through child statements until an expression is found
    clang::Expr* findExpressionInRange(clang::Stmt *stmt, clang::SourceRange targetRange, clang::SourceLocation &min, clang::SourceLocation &max);

    // Dispatcher for building macros
    std::unique_ptr<Statement> buildMacro(clang::Stmt *stmt, const MacroInfo &macroInfo);

    std::unique_ptr<Statement> buildStatement(clang::Stmt *stmt);

    // Build a test case
    std::unique_ptr<TestCase> buildTestCase(clang::FunctionDecl *func, MacroInfo macroInfo);

    // Build a custom AST
    void buildAST(CustomAST &AST);
};
