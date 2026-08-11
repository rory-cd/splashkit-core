#include "clang/AST/Stmt.h"
#include "clang/Basic/SourceLocation.h"
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
#include <clang/AST/RecursiveASTVisitor.h>

#include <iostream>
#include <llvm-18/llvm/Support/Casting.h>
#include <memory>
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

struct ReferenceExpression : Expression
{
    std::string name;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(ReferenceExpression, name)
    void serialise(json &j) const override { j = *this; j["kind"] = "ReferenceExpression"; }
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
    unsigned location;
    std::string type;
    bool isConst;
    bool isPointer = false;
    bool isReference = false;
    std::unique_ptr<Expression> initializer;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(VariableDeclaration, name, location, type, isConst, isPointer, isReference, initializer)
};

struct Parameter
{
    std::string name;
    std::string type;
    std::unique_ptr<Expression> defaultValue;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(Parameter, name, type, defaultValue)
};

struct Statement
{
    virtual ~Statement() = default;             // Explicit destructor for safety (pointers in derived classes)
    virtual void serialise(json &j) const = 0;  // Virtual function for base classes to define how they should be serialised
};

// Defines how nlohmann/json converts this type (Statement) to json
void to_json(json &j, const Statement &e)
{
    e.serialise(j);
}

// Defines how nlohmann/json converts this type (Unique pointer to Statement) to json
void to_json(json &j, const std::unique_ptr<Statement> &e)
{
    if (e) e->serialise(j);
    else   j = nullptr;         // This converts to "null" with nlohmann/json
}

struct FunctionDeclaration
{
    std::string name;
    std::vector<Parameter> parameters;
    unsigned location;
    std::vector<std::unique_ptr<Statement>> body;
    std::string returnType;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(FunctionDeclaration, name, parameters, location, body, returnType)
};

struct ExpressionStatement : Statement
{
    std::unique_ptr<Expression> expression;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(ExpressionStatement, expression)
    void serialise(json &j) const override { j = *this; j["kind"] = "ExpressionStatement"; }
};

struct VariableDeclarationStatement : Statement
{
    VariableDeclaration variable;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(VariableDeclarationStatement, variable)
    void serialise(json &j) const override { j = *this; j["kind"] = "VariableDeclarationStatement"; }
};

struct ReturnStatement : Statement
{
    std::unique_ptr<Expression> value;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(ReturnStatement, value)
    void serialise(json &j) const override { j = *this; j["kind"] = "ReturnStatement"; }
};

struct Section : Statement
{
    std::string name;
    std::vector<std::unique_ptr<Statement>> body;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(Section, name, body)
    void serialise(json &j) const override { j = *this; j["kind"] = "Section"; }
};

enum class AssertionType
{
    Require,
    RequireFalse,
    Check,
    CheckFalse
};

struct AssertionStatement : Statement
{
    AssertionType type;
    std::unique_ptr<Expression> expression;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(AssertionStatement, type, expression)
    void serialise(json &j) const override { j = *this; j["kind"] = "AssertionStatement"; }
};

struct TestCase
{
    std::string name;
    std::vector<std::string> tags;
    unsigned location;
    std::vector<std::unique_ptr<Statement>> body;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(TestCase, name, tags, location, body)
};

struct TestFile
{
    std::string filename;
    std::vector<VariableDeclaration> globals;
    std::vector<FunctionDeclaration> functions;
    std::vector<TestCase> tests;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(TestFile, filename, globals, functions, tests)
};

enum class MacroKind
{
    TestCase,
    Section,
    Require,
    Check,
    RequireFalse,
    CheckFalse
};

struct MacroInfo
{
    MacroKind kind;
    clang::SourceLocation location;
    clang::SourceRange argumentRange;
    std::string name;
    std::vector<std::string> tags;
    // clang::MacroArgs *args;
};

std::unordered_map<unsigned, MacroInfo> macros;

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

    unsigned getLocationKey(clang::SourceLocation loc)
    {
        return sourceManager.getExpansionLoc(loc).getRawEncoding();
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

        // String literal
        if (auto *literal = llvm::dyn_cast<clang::StringLiteral>(expr))
        {
            result->value = literal->getString().str();
            result->type = "string";                        // This ensures the type isn't "const char[9]" - not useful for translation
        }
        else
        {
            result->value = getSourceText(expr);
            result->type = expr->getType().getAsString();
        }

        return result;
    }

    // Build a binary expression
    std::unique_ptr<Expression> buildBinaryExpression(clang::BinaryOperator *binary)
    {
        auto result = std::make_unique<BinaryExpression>();

        result->op = binary->getOpcodeStr().str();
        result->left = buildExpression(binary->getLHS());
        result->right = buildExpression(binary->getRHS());

        return result;
    }

    // Build a function call 
    std::unique_ptr<Expression> buildFunctionCall(clang::CallExpr *call, std::string name)
    {
        auto result = std::make_unique<CallExpression>();
        result->functionName = name;

        int numArgs = call->getNumArgs();

        // Add all arguments
        for (int i = 0; i < numArgs; i++)
        {
            auto currentArg = call->getArg(i);
            result->arguments.push_back(buildExpression(currentArg));
        }

        return result;
    }

    // Build a reference to something already declared
    std::unique_ptr<Expression> buildReference(clang::DeclRefExpr *ref)
    {
        auto result = std::make_unique<ReferenceExpression>();
        result->name = ref->getDecl()->getNameAsString();
        return result;
    }

    // Expression dispatcher
    std::unique_ptr<Expression> buildExpression(clang::Expr *expr)
    {
        llvm::outs() << "\nClass: " << expr->getStmtClassName() << "\n";
        llvm::outs() << "Type: " << expr->getType().getAsString() << "\n";

        // clang::Expr *cleanExpr = expr->IgnoreImplicit();
        // Literals
        if (llvm::isa<clang::IntegerLiteral>(expr) ||
            llvm::isa<clang::FloatingLiteral>(expr) ||
            llvm::isa<clang::StringLiteral>(expr) ||
            llvm::isa<clang::CharacterLiteral>(expr))
        {
            return buildLiteral(expr);
        }
        else if (auto *binary = llvm::dyn_cast<clang::BinaryOperator>(expr))
        {
            return buildBinaryExpression(binary);
        }
        // Reference to variable, function, enum, etc.
        else if (auto *ref = llvm::dyn_cast<clang::DeclRefExpr>(expr))
        {
            return buildReference(ref);
        }
        // Calls
        else if (auto *call = llvm::dyn_cast<clang::CallExpr>(expr))
        {
            // Function call
            if (auto *funcDecl = call->getDirectCallee())
            {
                return buildFunctionCall(call, funcDecl->getNameAsString());
            }
        }
        // Implicit casts (unwrap them)
        else if (auto *cast = llvm::dyn_cast<clang::ImplicitCastExpr>(expr))
        {
            return buildExpression(cast->getSubExpr());
        }
        // // Expressions with cleanups (cleang lifetime management - unwrap)
        // else if (auto *cleanups = llvm::dyn_cast<clang::ExprWithCleanups>(expr))
        // {
        //     return buildExpression(cleanups->getSubExpr());
        // }
        // // Constructor expressions (like C++ making a string() object for a string literal) - unwrap
        // else if (auto *construct = llvm::dyn_cast<clang::CXXConstructExpr>(expr))
        // {
        //     return buildExpression(construct->getArg(0));
        // }
        // // Another clang wrapper for memory management - unwrap
        // else if (auto *bind = llvm::dyn_cast<clang::CXXBindTemporaryExpr>(expr))
        // {
        //     return buildExpression(bind->getSubExpr());
        // }
        // // Another clang wrapper for memory management - unwrap
        // else if (auto *mat = llvm::dyn_cast<clang::MaterializeTemporaryExpr>(expr))
        // {
        //     return buildExpression(mat->getSubExpr());
        // }
        // else if (auto *cast = llvm::dyn_cast<clang::CStyleCastExpr>(expr))
        // {
        //     return buildExpression(cast->getSubExpr());
        // }

        std::cout << "Unsupported expression found at line " << sourceManager.getSpellingLineNumber(expr->getExprLoc()) << std::endl;
        throw std::runtime_error("Unsupported expression");

    }

    // Build a variable declaration
    VariableDeclaration buildVariableDeclaration(clang::VarDecl *var)
    {
        VariableDeclaration result;

        result.name = var->getNameAsString();
        // result.line = getLineNumber(var);
        result.location = getLocationKey(var->getLocation());

        // Split type into more detail
        clang::QualType type = var->getType();
        result.type = type.getUnqualifiedType().getAsString();
        result.isConst = type.isConstQualified();
        result.isPointer = type->isPointerType();
        result.isReference = type->isReferenceType();

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
        // result.line = getLineNumber(fn);
        result.location = getLocationKey(fn->getLocation());
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

    void checkMinAndMax(clang::SourceLocation loc, clang::SourceLocation &min, clang::SourceLocation &max)
    {
        if (loc.isInvalid()) return;

        clang::SourceLocation spelling = sourceManager.getSpellingLoc(loc);

        // Not defined in main file - ignore
        if (sourceManager.getFileID(spelling) != sourceManager.getMainFileID())
            return;

        // If this the earliest location yet?
        if (min.isInvalid() ||
            sourceManager.isBeforeInTranslationUnit(spelling, min))
        {
            min = spelling;
        }

        // Is this the latest location yet?
        if (max.isInvalid() ||
            sourceManager.isBeforeInTranslationUnit(max, spelling))
        {
            max = spelling;
        }
    }

    // Recurse through child statements until an expression is found
    clang::Expr* findExpressionInRange(clang::Stmt *stmt, clang::SourceRange targetRange, clang::SourceLocation &min, clang::SourceLocation &max)
    {
        if (!stmt) return nullptr;

        clang::SourceLocation childMin;
        clang::SourceLocation childMax;

        // Search children first, so we get the smallest matching expression.
        for (clang::Stmt *child : stmt->children())
        {
            // If we found a match in the children, return it
            if (auto *result = findExpressionInRange(child, targetRange, childMin, childMax))
                return result;
        }

        // Now we're at the bottom
        if (auto *expr = llvm::dyn_cast<clang::Expr>(stmt))
        {
            // Check if this expression is the new min or max of the range
            checkMinAndMax(expr->getBeginLoc(), min, max);
            checkMinAndMax(expr->getEndLoc(), min, max);

            std::cout << expr->getStmtClassName() << ": "
                << min.printToString(sourceManager)
                << " -> "
                << max.printToString(sourceManager)
                << '\n';

            // Normalise target locations in the same way.
            clang::SourceLocation targetBegin =
                sourceManager.getSpellingLoc(targetRange.getBegin());

            clang::SourceLocation targetEnd =
                sourceManager.getSpellingLoc(targetRange.getEnd());

            if (childMin.isValid() && childMax.isValid() && childMin == targetBegin && childMax == targetEnd)
            {
                std::cout << "MATCH: "
                        << stmt->getStmtClassName()
                        << '\n';

                return expr;
            }
        }

        return nullptr;
    }

    // Dispatcher for building macros
    std::unique_ptr<Statement> buildMacro(clang::Stmt *stmt, const MacroInfo &macroInfo)
    {
        std::cout << "Macro: "
            << macroInfo.name
            << '\n';

        stmt->dump();

        switch (macroInfo.kind)
        {
            case MacroKind::Section:
            {
                std::cout << "Section macro: " << macroInfo.name << std::endl;
                auto result = std::make_unique<Section>();
                result->name = macroInfo.name;

                // The SECTION macro becomes an if statement during compilation
                auto *ifStmt = llvm::dyn_cast<clang::IfStmt>(stmt);

                if (!ifStmt)
                    return result;

                // The block of the if statement becomes the "body" of the section
                auto *compound =
                    llvm::dyn_cast<clang::CompoundStmt>(ifStmt->getThen());

                if (!compound)
                    return result;

                // Add each statement to the section body
                for (clang::Stmt *child : compound->body())
                {
                    auto statement = buildStatement(child);
                    if (statement) result->body.push_back(std::move(statement));
                }

                return result;
            }
                
            case MacroKind::Require:
            {
                std::cout << "Require macro: " << macroInfo.name << std::endl;
                auto result = std::make_unique<AssertionStatement>();
                result->type = AssertionType::Require;

                clang::SourceLocation min;
                clang::SourceLocation max;
                auto argument = findExpressionInRange(stmt, macroInfo.argumentRange, min, max);
                
                if (!argument)
                {
                    std::cout << "findExpressionInRange returned nullptr!" << std::endl;
                    return result;
                }

                std::cout << "Found argument: "
                        << argument->getStmtClassName()
                        << std::endl;
                
                result->expression = buildExpression(argument);

                return result;
            }

            default:
            std::cout << "Macro: " << "DUNNO" << std::endl;
                auto result = std::make_unique<AssertionStatement>();
                return result;
        }
    }

    std::unique_ptr<Statement> buildStatement(clang::Stmt *stmt)
    {
        // Start by checking for macros
        // Get the key for this location
        unsigned key = getLocationKey(stmt->getBeginLoc());

        // Check for any macros at this location
        auto it = macros.find(key);

        // If there is one, build it
        if (it != macros.end())
        {
            return buildMacro(stmt, it->second);
        }
        // Declaration statement
        else if (auto *declStmt = llvm::dyn_cast<clang::DeclStmt>(stmt))
        {
            // Check declarations
            for (clang::Decl *decl : declStmt->decls())
            {
                // Variable declaration
                if (auto *var = llvm::dyn_cast<clang::VarDecl>(decl))
                {
                    auto result = std::make_unique<VariableDeclarationStatement>();;
                    result->variable = buildVariableDeclaration(var);
                    return result;
                }
            }
        }

        // if (auto *call = llvm::dyn_cast<clang::CallExpr>(stmt))
        // {
        //     return buildFunctionCall(call);
        // }

        // if (auto *ifStmt = llvm::dyn_cast<clang::IfStmt>(stmt))
        // {
        //     return buildIfStatement(ifStmt);
        // }

        return nullptr;
    }

    // Build a test case
    TestCase buildTestCase(clang::FunctionDecl *func, MacroInfo macroInfo)
    {
        TestCase testCase;

        // Utilise macro info obtained by the preprocessor
        testCase.name = macroInfo.name;
        // testCase.line = getLineNumber(func);
        testCase.location = getLocationKey(macroInfo.location);
        testCase.tags = macroInfo.tags;

        // Build body
        clang::Stmt *body = func->getBody();
        auto *compound = llvm::dyn_cast<clang::CompoundStmt>(body);
        if (!compound) return testCase;

        // For every statement
        for (clang::Stmt *stmt : compound->body())
        {
            // Build it, then add it to the test case body
            std::unique_ptr<Statement> result = buildStatement(stmt);
            testCase.body.push_back(std::move(result));
        }

        return testCase;
    }

    // Build a test file (top level)
    void buildTestFile(TestFile &file)
    {
        // Root node of the AST
        clang::TranslationUnitDecl *translationUnit = context.getTranslationUnitDecl();

        // Top-level declarations
        for (clang::Decl *decl : translationUnit->decls())
        {
            clang::SourceLocation loc = decl->getLocation();
            clang::SourceLocation expansionLoc =
                sourceManager.getExpansionLoc(loc);

            // If the declaration isn't written in the main file, ignore it
            if (!sourceManager.isWrittenInMainFile(expansionLoc))
            {
                continue;
            }

            // Try to cast as each type of declaration, and manage accordingly
            // Variables
            if (auto *var = llvm::dyn_cast<clang::VarDecl>(decl))
            {
                file.globals.push_back(buildVariableDeclaration(var));
            }
            else if (auto *func = llvm::dyn_cast<clang::FunctionDecl>(decl))
            {
                if (func != func->getCanonicalDecl())
                    continue;

                // Get the key for this location
                unsigned key = getLocationKey(func->getLocation());

                // Check for any macros at this location
                auto it = macros.find(key);

                // If there is one, and it's a test case, build it
                if (it != macros.end() && it->second.kind == MacroKind::TestCase)
                {
                    file.tests.push_back(buildTestCase(func, it->second));
                }
                // Otherwise it's a top-level function
                else
                {
                    file.functions.push_back(buildFunction(func));
                }
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
    ) override
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
            if (stem != "unit_test_test") continue;

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
    fs::path outputDir = "generated/json";
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

    // Generate C#
    // Ensure output directory exists
    fs::create_directories("generated/csharp");
    // Project files
    std::ofstream projFile("generated/csharp/SplashKit.CSharp.UnitTests.csproj");
    projFile << R"(<Project Sdk="Microsoft.NET.Sdk">

    <PropertyGroup>
        <TargetFramework>net6.0</TargetFramework>
        <ImplicitUsings>enable</ImplicitUsings>
        <Nullable>enable</Nullable>
        <IsPackable>false</IsPackable>
    </PropertyGroup>

    <ItemGroup>
        <PackageReference Include="coverlet.collector" Version="6.0.2" />
        <PackageReference Include="Microsoft.NET.Test.Sdk" Version="17.12.0" />
        <PackageReference Include="xunit" Version="2.9.2" />
        <PackageReference Include="xunit.runner.visualstudio" Version="2.8.2" />
    </ItemGroup>

    <!-- Include C# bindings in project -->
    <ItemGroup>
        <Compile Include="../../../../../generated/csharp/SplashKit.cs" />
    </ItemGroup>

</Project>
)";

    std::ofstream usingsFile("generated/csharp/GlobalUsings.cs");
    usingsFile << R"(global using Xunit;
global using SplashKitSDK;
global using static SplashKitSDK.SplashKit;)";
 
    return 0;
}
