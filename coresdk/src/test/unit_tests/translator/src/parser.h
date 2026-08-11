#pragma once

#include "parser.h"

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

#include <string>
#include "ast.h"

std::string getMacroArgumentString(
    const clang::MacroArgs *args,
    unsigned tokenIndex,
    clang::Preprocessor &pp);

// Takes a set of tags in the form "[tag1][tag2][tag3]"
// Returns each tag as a string in a vector
std::vector<std::string> parseTags(std::string tagString);

// PPCallbacks is a pre-processor interface. MacroExpands() is called when the pre-processor encounters a Macro
class TestFinder : public clang::PPCallbacks
{
private:
    TestFile &file;
    clang::SourceManager &sourceManager;
    clang::Preprocessor &pp;
    unsigned currentTestKey = 0;

public:
    TestFinder(TestFile &file, clang::SourceManager &sm, clang::Preprocessor &pp)
        : file(file), sourceManager(sm), pp(pp)
    {
    }

    void MacroExpands(
        const clang::Token &macroName,
        const clang::MacroDefinition &macroDefinition,
        clang::SourceRange range,
        const clang::MacroArgs *args
    ) override;
};

// ASTConsumer is an interface used to write generic actions on an AST, regardless of how the AST was produced
class TopLevelConsumer : public clang::ASTConsumer
{
private:
    TestFile &file;

public:
    TopLevelConsumer(TestFile &file) : file(file) { }

    void HandleTranslationUnit(clang::ASTContext &context) override;
};

// FrontEndAction is something you want Clang to do after it creates the AST after processing a source file
// It has default ones, this is a custom one
class TopLevelAction : public clang::ASTFrontendAction
{
private:
    TestFile currentFile;

public:
    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
        clang::CompilerInstance &compiler,                  // Reference to Clang's internal compiler state
        llvm::StringRef filename                            // Filename (full path) being processed
    ) override;
};

// Parses all files with the ClangTool
// Gives clang the compilation instructions, files, and action it needs to build the AST
void parseTestFiles(const std::vector<std::string>& filepaths);