#include "parser.h"

#include <clang/Tooling/Tooling.h>
#include <clang/Lex/MacroArgs.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Tooling/CompilationDatabase.h>

#include <filesystem>
#include <string>
#include <iostream>
#include <fstream>

#include "builder.h"
#include "macro.h"

namespace fs = std::filesystem;

std::unordered_map<unsigned, MacroInfo> macros;

// Full set of ASTs generated
std::vector<CustomAST> allASTs;

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
void TestFinder::MacroExpands(
    const clang::Token &macroName,
    const clang::MacroDefinition &macroDefinition,
    clang::SourceRange range,
    const clang::MacroArgs *args)
{
    std::string name = macroName.getIdentifierInfo()->getName().str();

    // Record macro info
    MacroInfo macro;
    macro.location = sourceManager.getExpansionLoc(range.getBegin());
    // Get the raw version of location as a key
    unsigned key = macro.location.getRawEncoding();

    if (name == "TEST_CASE")
    {
        macro.kind = MacroKind::TestCase;
        macro.name = getMacroArgumentString(args, 0, pp);

        // Add tags
        std::string allTagsString = getMacroArgumentString(args, 2, pp);
        macro.tags = parseTags(allTagsString);
        macros[key] = macro;
        std::cout << "- Test case found at " << macro.location.getRawEncoding() << std::endl;
    }
    else if (name == "SECTION")
    {
        macro.kind = MacroKind::Section;
        macro.name = getMacroArgumentString(args, 0, pp);
        macros[key] = macro;
        std::cout << "- Section found at " << macro.location.getRawEncoding() << std::endl;
    }
    else if (name == "REQUIRE")
    {
        macro.kind = MacroKind::Require;
        const clang::Token *tokens = args->getUnexpArgument(0);
        unsigned numTokens = args->getArgLength(tokens);
        // unsigned numTokens = args->getArgLength(0);
        // Get the location of the first and last tokens in the unexpanded arguments
        clang::SourceLocation begin = tokens[0].getLocation();
        clang::SourceLocation end = tokens[numTokens - 1].getLocation();

        macro.argumentRange = clang::SourceRange(begin, end);
        macros[key] = macro;
        std::cout << "Inserted require macro" << std::endl;
        std::cout << "Require args between: " << macro.argumentRange.getBegin().getRawEncoding() << " and " << macro.argumentRange.getEnd().getRawEncoding() << std::endl;
    }
    else if (name == "REQUIRE_FALSE")
    {
        macro.kind = MacroKind::RequireFalse;
        macros[key] = macro;
        std::cout << "- Require false found at " << macro.location.getRawEncoding() << std::endl;
    }
    else if (name == "CHECK")
    {
        macro.kind = MacroKind::Check;
        macros[key] = macro;
        std::cout << "- Check found at " << macro.location.getRawEncoding() << std::endl;
    }
    else if (name == "CHECK_FALSE")
    {
        macro.kind = MacroKind::CheckFalse;
        macros[key] = macro;
        std::cout << "- Check false found at " << macro.location.getRawEncoding() << std::endl;
    }
    // Track the current test case
    if (name == "TEST_CASE") currentTestKey = key;
}

void TopLevelConsumer::HandleTranslationUnit(clang::ASTContext &context)
{
    ASTBuilder builder(context, macros);
    builder.buildAST(AST);
    allASTs.push_back(std::move(AST));
}

// FrontEndAction is something you want Clang to do after it creates the AST after processing a source file
// It has default ones, this is a custom one
std::unique_ptr<clang::ASTConsumer> TopLevelAction::CreateASTConsumer(
    clang::CompilerInstance &compiler,                  // Reference to Clang's internal compiler state
    llvm::StringRef filename)                            // Filename (full path) being processed
{
    // Set filename
    AST.filename = fs::path(filename.str()).stem().string();

    // Create a new "TestFinder" and point the pre-processor at it
    // The pre-processor will populate the source file with all the tests it finds
    compiler.getPreprocessor().addPPCallbacks(
        std::make_unique<TestFinder>(
            AST,
            compiler.getSourceManager(),
            compiler.getPreprocessor()
        )
    );

    // Create a new consumer to send the AST to, and return a unique pointer to it
    // This consumer will build out the representation of the test file, and add it to the global list
    return std::make_unique<TopLevelConsumer>(AST);
}

// Parses all files with the ClangTool
// Gives clang the compilation instructions, files, and action it needs to build the AST
std::vector<fs::path> parseTestFiles(const std::vector<std::string> &filepaths)
{
    fs::path pwd = fs::current_path();
    fs::path srcDir = fs::path("..") / ".." / "..";
    fs::path externalDir = srcDir / ".." / "external";

    // Use these arguments when compiling ("Fixed" because it doesn't change per file)
    clang::tooling::FixedCompilationDatabase compilations(
        pwd.string(),    // Paths relative to this dir
        {
            "-I" + (srcDir / "backend").string(),
            "-I" + (externalDir / "catch").string(),
            "-I" + (externalDir / "easyloggingpp").string(),
            "-I" + (srcDir / "coresdk").string()
        });

    // Create a tool and assign the filepaths
    clang::tooling::ClangTool tool(compilations, filepaths);

    // For every source file, build a clang AST, create a TopLevelAction, and execute it.
    tool.run(clang::tooling::newFrontendActionFactory<TopLevelAction>().get());

    // Ensure output directory exists
    fs::path outputDir = pwd / "generated" / "json";
    fs::create_directories(outputDir);

    std::vector<fs::path> jsonFiles;

    // Convert and save as JSON
    for (const auto &test : allASTs)
    {
        json j = test;
        fs::path outputFile = outputDir / (test.filename + ".json");    // Set path
        std::ofstream file(outputFile);
        file << j.dump(4);
        jsonFiles.push_back(outputFile);
    }

    std::cout << "\nSaved " << filepaths.size() << " tests to " << outputDir << "\n";
    return jsonFiles;
}
