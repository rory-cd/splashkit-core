#include <clang/Tooling/Tooling.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/Expr.h>
#include <clang/AST/Decl.h>
#include <clang/Lex/Lexer.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Lex/MacroArgs.h>

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

struct Section
{
    std::string name;
    int line;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(Section, name, line)
};

struct TestCase
{
    std::string name;
    std::vector<std::string> tags;
    int line;
    std::vector<Section> sections;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(TestCase, name, tags, line, sections)
};

struct TestFile
{
    std::string filename;
    std::vector<VariableDeclaration> globals;
    std::vector<FunctionDeclaration> functions;
    std::vector<TestCase> tests;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(TestFile, filename, globals, functions, tests)
};

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

    // Build a test file (top level)
    void buildTestFile(TestFile &file)
    {
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
    }
};

// This is the full set of ASTs generated by this program
std::vector<TestFile> allASTs;

std::string getMacroArgumentString(
    const clang::MacroArgs *args,
    unsigned tokenIndex,
    clang::Preprocessor &pp)
{
    const clang::Token *argTokens = args->getUnexpArgument(0);  // Get a pointer to the first token
    std::string result = pp.getSpelling(argTokens[tokenIndex]); // Get the source text for the selected token

    // Strip out the quotes
    if (result.size() >= 2 &&
        result.front() == '"' &&
        result.back() == '"')
    {
        result = result.substr(1, result.size() - 2);
    }

    return result;
}

// Takes a set of tags in the form "[tag1][tag2][tag3]"
// Returns each tag as a string in a vector
std::vector<std::string> parseTags(std::string tagString)
{
    std::vector<std::string> result;

    int start = 0;

    // Extract substrings between "[]"
    while ((start = tagString.find('[', start)) != std::string::npos)
    {
        int end = tagString.find(']', start);

        if (end == std::string::npos) break;

        result.push_back(tagString.substr(start + 1, end - start - 1));
        start = end + 1;
    }

    return result;
}

// PPCallbacks is a pre-processor interface. MacroExpands() is called when the pre-processor encounters a Macro
class TestFinder : public clang::PPCallbacks
{
private:
    TestFile &file;
    clang::SourceManager &sourceManager;
    clang::Preprocessor &pp;
    TestCase *currentTest = nullptr;

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
    ) override
    {
        std::string name = macroName.getIdentifierInfo()->getName().str();

        if (name == "TEST_CASE")
        {
            TestCase test;
            unsigned numArgs = args->getNumMacroArguments();
            test.name = getMacroArgumentString(args, 0, pp);

            // Add tags
            std::string allTagsString = getMacroArgumentString(args, 2, pp);
            test.tags = parseTags(allTagsString);

            test.line = sourceManager.getSpellingLineNumber(range.getBegin());
            file.tests.push_back(test);
            currentTest = &file.tests.back();
        }

        else if (name == "SECTION")
        {
            if (!currentTest) return;
            Section section;
            section.name = getMacroArgumentString(args, 0, pp);
            section.line = sourceManager.getSpellingLineNumber(range.getBegin());
            currentTest->sections.push_back(section);
        }
    }
};

// ASTConsumer is an interface used to write generic actions on an AST, regardless of how the AST was produced
class TopLevelConsumer : public clang::ASTConsumer
{
private:
    TestFile &file;

public:
    TopLevelConsumer(TestFile &file) : file(file) { }

    void HandleTranslationUnit(clang::ASTContext &context) override
    {
        TranslationBuilder builder(context);
        builder.buildTestFile(file);
        allASTs.push_back(std::move(file));
    }
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
    ) override {
        // Set filename
        currentFile.filename = fs::path(filename.str()).stem().string();

        // Create a new "TestFinder" and point the pre-processor at it
        // The pre-processor will populate the source file with all the tests it finds
        compiler.getPreprocessor().addPPCallbacks(
            std::make_unique<TestFinder>(
                currentFile,
                compiler.getSourceManager(),
                compiler.getPreprocessor()
            )
        );

        // Create a new consumer to send the AST to, and return a unique pointer to it
        // This consumer will build out the representation of the test file, and add it to the global list
        return std::make_unique<TopLevelConsumer>(currentFile);
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
            std::string stem = entry.path().stem().string();                        // Filename without extension
            if (stem == "unit_test_main" || stem == "logging_handling") continue;   // Skip "main" file

            cppFiles.push_back(entry.path().string());                              // Add file to list
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
    for (const auto &test : allASTs)
    {
        json j = test;
        fs::path outputFile = outputDir / (test.filename + ".json");    // Set path
        std::ofstream file(outputFile);
        file << j.dump(4);
    }

    std::cout << "\nSaved " << cppFiles.size() << " tests to " << outputDir << "\n";
 
    return 0;
}
