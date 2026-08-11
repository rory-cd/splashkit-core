#pragma once

#include <clang/Tooling/Tooling.h>
#include <clang/Lex/MacroArgs.h>
#include <clang/Frontend/CompilerInstance.h>
#include "clang/AST/Stmt.h"
#include "clang/Basic/SourceLocation.h"
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/Expr.h>
#include <clang/AST/Decl.h>
#include <clang/Lex/Lexer.h>
#include <clang/AST/RecursiveASTVisitor.h>

#include "ast.h"
#include "macro.h"

// Builds a data structure for a custom AST
class TranslationBuilder
{
private:
    // Need source manager for info about the source file (like line numbers)
    clang::ASTContext &context;
    clang::SourceManager &sourceManager;
    std::unordered_map<unsigned, MacroInfo> &macros;

public:
    TranslationBuilder(clang::ASTContext &context, std::unordered_map<unsigned, MacroInfo> &macros)
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
    VariableDeclaration buildVariableDeclaration(clang::VarDecl *var);

    // Build a parameter
    Parameter buildParameter(clang::ParmVarDecl *param);

    // Build a function declaration
    FunctionDeclaration buildFunction(clang::FunctionDecl *fn);

    void checkMinAndMax(clang::SourceLocation loc, clang::SourceLocation &min, clang::SourceLocation &max);

    // Recurse through child statements until an expression is found
    clang::Expr* findExpressionInRange(clang::Stmt *stmt, clang::SourceRange targetRange, clang::SourceLocation &min, clang::SourceLocation &max);

    // Dispatcher for building macros
    std::unique_ptr<Statement> buildMacro(clang::Stmt *stmt, const MacroInfo &macroInfo);

    std::unique_ptr<Statement> buildStatement(clang::Stmt *stmt);

    // Build a test case
    TestCase buildTestCase(clang::FunctionDecl *func, MacroInfo macroInfo);

    // Build a test file (top level)
    void buildTestFile(TestFile &file);
};
