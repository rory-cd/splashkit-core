#include <clang/Tooling/Tooling.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/Expr.h>
#include <clang/AST/Decl.h>

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace fs = std::filesystem;

struct Expression
{
    virtual ~Expression() = default;            // Explicit destructor for safety (pointers in derived classes)
    virtual void serialise(json &j) const = 0;  // Virtual function for base classes to define how they should be serialised
};

// Defines how nlohmann/json converts this type (Expression) to json
void to_json(json &j, const Expression &e)
{
    e.serialise(j);
}

// Defines how nlohmann/json converts this type (Unique pointer to Expression) to json
void to_json(json &j, const std::unique_ptr<Expression> &e)
{
    if (e) e->serialise(j);
    else   j = nullptr;         // This converts to "null" with nlohmann/json
}

struct LiteralExpression : Expression
{
    std::string value;
    std::string type;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(LiteralExpression, value, type)
    // Convert the current object (the derived type) to json, and assign a "kind"
    void serialise(json &j) const override { j = *this; j["kind"] = "LiteralExpression"; }
};

struct VariableReferenceExpression : Expression
{
    std::string name;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(VariableReferenceExpression, name)
    void serialise(json &j) const override { j = *this; j["kind"] = "VariableReferenceExpression"; }
};

struct CallExpression : Expression
{
    std::string functionName;
    std::vector<std::unique_ptr<Expression>> arguments;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(CallExpression, functionName, arguments)
    void serialise(json &j) const override { j = *this; j["kind"] = "CallExpression"; }
};

struct BinaryExpression : Expression
{
    std::string op;
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(BinaryExpression, op, left, right)
    void serialise(json &j) const override { j = *this; j["kind"] = "BinaryExpression"; }
};

struct VariableDeclaration
{
    std::string name;
    int line;
    std::string type;
    std::unique_ptr<Expression> initializer;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(VariableDeclaration, name, line, type, initializer)
};

struct Parameter
{
    std::string name;
    std::string type;
    std::unique_ptr<Expression> defaultValue;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(Parameter, name, type, defaultValue)
};

struct FunctionDeclaration
{
    std::string name;
    int line;
    // std::vector<Statement> body;
    std::string returnType;
    std::vector<Parameter> parameters;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(FunctionDeclaration, name, line, returnType, parameters)
};

struct TestCase
{
    std::string name;
    int line;
    std::vector<std::string> tags;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(TestCase, name, line, tags)
};

struct SourceFile
{
    std::string filename;
    std::vector<VariableDeclaration> globals;
    std::vector<FunctionDeclaration> functions;
    std::vector<TestCase> tests;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(SourceFile, filename, globals, functions, tests)
};

std::vector<SourceFile> allTests;

// Builds a data structure for a custom AST
class TranslationBuilder
{
private:
    // Need source manager for info about the source file (like line numbers)
    clang::ASTContext &context;
    clang::SourceManager &sourceManager;

public:
    TranslationBuilder(clang::ASTContext &context)
        : context(context),
          sourceManager(context.getSourceManager())
    {
    }

    // Gets the line number of a declaration from the source file
    int getLineNumber(const clang::Decl *decl)
    {
        return sourceManager.getSpellingLineNumber(decl->getLocation());
    }

    // Gets the source text of a given expression (e.g. "x + 5")  
    std::string getSourceText(clang::Expr *expr)
    {
        // Token range uses the entire range of the expression
        auto range = clang::CharSourceRange::getTokenRange(expr->getSourceRange());

        clang::LangOptions options; // Default options (language rules)

        // Use the lexer to ask for the source text at the given location
        return clang::Lexer::getSourceText(range, sourceManager, options).str();
    }

    // Build a literal with a string value 
    std::unique_ptr<Expression> buildLiteral(clang::Expr *expr)
    {
        auto result = std::make_unique<LiteralExpression>();

        result->type = expr->getType().getAsString();
        result->value = getSourceText(expr);

        return result;
    }

    // Expression dispatcher
    std::unique_ptr<Expression> buildExpression(clang::Expr *expr)
    {
        // Literals
        if (llvm::isa<clang::IntegerLiteral>(expr) ||
            llvm::isa<clang::FloatingLiteral>(expr) ||
            llvm::isa<clang::StringLiteral>(expr) ||
            llvm::isa<clang::CharacterLiteral>(expr))
        {
            return buildLiteral(expr);
        }

        throw std::runtime_error("Unsupported expression");
    }

    // Build a variable declaration
    VariableDeclaration buildVariableDeclaration(clang::VarDecl *var)
    {
        VariableDeclaration result;

        result.name = var->getNameAsString();
        result.line = getLineNumber(var);
        result.type = var->getType().getAsString();

        // Is it initialised?
        if (var->hasInit())
        {
            result.initializer = buildExpression(var->getInit());
        }

        return result;
    }

    // Build a parameter
    Parameter buildParameter(clang::ParmVarDecl *param)
    {
        Parameter result;

        result.name = param->getNameAsString();
        result.type = param->getType().getAsString();

        // Is there a default?
        if (param->hasDefaultArg())
        {
            result.defaultValue = buildExpression(param->getDefaultArg());
        }

        return result;
    }

    // Build a function declaration
    FunctionDeclaration buildFunction(clang::FunctionDecl *fn)
    {
        FunctionDeclaration result;

        // Use getQualifiedNameAsString(); to check for SplashKit functions "splashkit_lib::draw_bitmap"

        result.name = fn->getNameAsString();
        result.line = getLineNumber(fn);
        result.returnType = fn->getReturnType().getAsString();

        // Get params
        for (auto *param : fn->parameters())
        {
            result.parameters.push_back(buildParameter(param));
        }

        // Build the function body
        // auto *body = llvm::cast<clang::CompoundStmt>(fn->getBody());
        // result.body = buildBlock(body);

        return result;
    }

    // Build a source file (top level)
    SourceFile buildSourceFile()
    {
        SourceFile file;
        
        // Assign the file name (assuming there's a valid file entry)
        if (auto fileEntry = sourceManager.getFileEntryRefForID(sourceManager.getMainFileID()))
        {
            file.filename = fs::path(fileEntry->getName().str()).stem().string();
        }

        // Root node of the AST
        clang::TranslationUnitDecl *translationUnit = context.getTranslationUnitDecl();

        // Top-level declarations
        for (clang::Decl *decl : translationUnit->decls())
        {
            // If the declaration isn't written in the main file, ignore it
            if (!sourceManager.isWrittenInMainFile(decl->getLocation()))
            {
                continue;
            }

            // Try to cast as each type of declaration, and manage accordingly
            // Variables
            if (auto *var = llvm::dyn_cast<clang::VarDecl>(decl))
            {
                file.globals.push_back(buildVariableDeclaration(var));
            }

            // Functions
            if (auto *var = llvm::dyn_cast<clang::FunctionDecl>(decl))
            {
                file.functions.push_back(buildFunction(var));
            }
        }

        return file;
    }
};

// ASTConsumer is an interface used to write generic actions on an AST, regardless of how the AST was produced
class TopLevelConsumer : public clang::ASTConsumer
{
public:
    void HandleTranslationUnit(clang::ASTContext &context) override
    {
        TranslationBuilder builder(context);
        SourceFile file = builder.buildSourceFile();
        allTests.push_back(std::move(file));
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

// Parses all files with the ClangTool
// Gives clang the compilation instructions, files, and action it needs to build the AST
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

    // Create a tool and assign the filepaths
    clang::tooling::ClangTool tool(compilations, filepaths);

    // For every source file, build an AST, create a TopLevelAction, and execute it.
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

    // Ensure output directory exists
    fs::path outputDir = "json";
    fs::create_directories(outputDir);

    // Convert and save as JSON
    for (const auto &test : allTests)
    {
        json j = test;
        fs::path outputFile = outputDir / (test.filename + ".json");    // Set path
        std::ofstream file(outputFile);
        file << j.dump(4);
    }
 
    return 0;
}
