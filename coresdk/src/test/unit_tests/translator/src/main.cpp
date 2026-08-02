#include <clang/Tooling/Tooling.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/AST/ASTConsumer.h>

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

// ASTConsumer is an interface used to write generic actions on an AST, regardless of how the AST was produced
class TopLevelConsumer : public clang::ASTConsumer
{
public:
    void HandleTranslationUnit(clang::ASTContext &context) override
    {
        // Root node of the AST
        clang::TranslationUnitDecl *translationUnit = context.getTranslationUnitDecl();

        std::cout << "Top-level declarations:\n";

        for (clang::Decl *decl : translationUnit->decls())
        {
            // If the declaration isn't written in the main file, ignore it
            if (!context.getSourceManager().isWrittenInMainFile(decl->getLocation()))
            {
                continue;
            }

            // Print the kind of decl
            std::cout << decl->getDeclKindName();

            // If it has a name, print it
            if (auto *namedDecl = llvm::dyn_cast<clang::NamedDecl>(decl))
            {
                std::cout << ": " << namedDecl->getNameAsString();
            }

            std::cout << "\n";
        }
    }
};

// FrontEndAction is something you want Clang to do after it creates the AST after processing a source file
// It has default ones, this is a custom one
class TopLevelAction : public clang::ASTFrontendAction
{
public:
    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
        clang::CompilerInstance &compiler,              // Reference to Clang's internal compiler state
        llvm::StringRef file                            // Filename being processed
    ) override {
        return std::make_unique<TopLevelConsumer>();    // Create a new consumer to send the AST to, and return a unique reference to it
    }
};

// 1. Tell Clang how this file should be compiled.
// 2. Tell Clang which file to compile.
// 3. Tell Clang what to do once it's parsed.
// 4. Run it.
void parseTestFiles(const std::vector<std::string>& filepaths)
{
    // Use these arguments when compiling ("Fixed" because it doesn't change per file)
     clang::tooling::FixedCompilationDatabase compilations(
        ".",    // Paths relative to this dir
        {
            "-I../../../backend",
            "-I../../../../external/catch",
            "-I../../../../external/easyloggingpp",
            "-I../../../coresdk"
        });

    // Create a tool
     clang::tooling::ClangTool tool(compilations, filepaths);

    // For every source file, build an AST, create a TopLevelAction, and execute it.
    // Run this ClangTool. For each source file, ask this factory to create a new TopLevelAction, then execute that action.
    tool.run(clang::tooling::newFrontendActionFactory<TopLevelAction>().get());
}

// Parses every .cpp file in the specified directory
int main(int argc, char** argv) {
    std::string testDir = "./../";        // Default to parent directory
    if (argc > 1) testDir = argv[1];

    std::cout << "Scanning directory: " << testDir << "\n\n";
 
    // Check the chosen directory exists, and is actually a directory
    std::error_code ec;
    if (!fs::exists(testDir, ec) || !fs::is_directory(testDir, ec)) {
        std::cerr << "Error: Directory does not exist or is inaccessible: " << testDir << "\n";
        return 1;
    }
 
    // Add every .cpp file in the directory 
    std::vector<std::string> cppFiles;
    for (const auto& entry : fs::directory_iterator(testDir)) {
        if (entry.path().extension() == ".cpp") {
            std::string stem = entry.path().stem().string();    // Filename without extension
            if (stem == "unit_test_main" || stem == "logging_handling") continue;             // Skip "main" file

            cppFiles.push_back(entry.path().string());          // Add file to list
        }
    }
 
    // No valid files found
    if (cppFiles.empty()) {
        std::cerr << "No .cpp files found in " << testDir << "\n";
        return 1;
    }
 
    std::cout << "Found " << cppFiles.size() << " .cpp files\n\n";

    // Use LibTooling to parse files
    parseTestFiles(cppFiles);
 
    return 0;
}
