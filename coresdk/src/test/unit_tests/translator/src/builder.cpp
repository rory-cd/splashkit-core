#include <clang/Tooling/Tooling.h>
#include <clang/AST/RecursiveASTVisitor.h>

#include <iostream>

#include "builder.h"
#include "ast.h"
#include "macro.h"
#include "clang/AST/Stmt.h"

// Gets the line number of a declaration from the source file
// int ASTBuilder::getLineNumber(const clang::Decl *decl)
// {
//     return sourceManager.getSpellingLineNumber(decl->getLocation());
// }

unsigned ASTBuilder::getLocationKey(clang::SourceLocation loc)
{
    return sourceManager.getExpansionLoc(loc).getRawEncoding();
}

// Gets the source text of a given expression (e.g. "x + 5")  
std::string ASTBuilder::getSourceText(const clang::Expr &expr)
{
    // Token range uses the entire range of the expression
    auto range = clang::CharSourceRange::getTokenRange(expr.getSourceRange());

    clang::LangOptions options; // Default options (language rules)

    // Use the lexer to ask for the source text at the given location
    return clang::Lexer::getSourceText(range, sourceManager, options).str();
}

// Build a literal with a string value 
std::unique_ptr<Expression> ASTBuilder::buildLiteral(const clang::Expr &expr)
{
    auto result = std::make_unique<LiteralExpression>();

    // String literal
    if (auto *literal = llvm::dyn_cast<const clang::StringLiteral>(&expr))
    {
        result->value = literal->getString().str();
        result->type = "string";                        // This ensures the type isn't "const char[9]" - not useful for translation
    }
    else
    {
        result->value = getSourceText(expr);
        result->type = expr.getType().getAsString();
    }

    return result;
}

// Build a binary expression
std::unique_ptr<Expression> ASTBuilder::buildBinaryExpression(const clang::BinaryOperator &binary)
{
    auto result = std::make_unique<BinaryExpression>();

    result->op = binary.getOpcodeStr().str();
    result->left = buildExpression(*binary.getLHS());
    result->right = buildExpression(*binary.getRHS());

    return result;
}

// Build a function call 
std::unique_ptr<Expression> ASTBuilder::buildFunctionCall(const clang::CallExpr &call, std::string name)
{
    auto result = std::make_unique<CallExpression>();
    result->functionName = name;

    unsigned numArgs = call.getNumArgs();

    // Add all arguments
    for (int i = 0; i < numArgs; i++)
    {
        auto *currentArg = call.getArg(i);
        result->arguments.push_back(buildExpression(*currentArg));
    }

    return result;
}

// Build a reference to something already declared
std::unique_ptr<Expression> ASTBuilder::buildReference(const clang::DeclRefExpr &ref)
{
    auto result = std::make_unique<ReferenceExpression>();
    result->name = ref.getDecl()->getNameAsString();
    return result;
}

// Expression dispatcher
std::unique_ptr<Expression> ASTBuilder::buildExpression(const clang::Expr &expr)
{
    llvm::outs() << "\nClass: " << expr.getStmtClassName() << "\n";
    llvm::outs() << "Type: " << expr.getType().getAsString() << "\n";

    // clang::Expr *cleanExpr = expr->IgnoreImplicit();
    // Literals
    if (llvm::isa<clang::IntegerLiteral>(expr) ||
        llvm::isa<clang::FloatingLiteral>(expr) ||
        llvm::isa<clang::StringLiteral>(expr) ||
        llvm::isa<clang::CharacterLiteral>(expr))
    {
        return buildLiteral(expr);
    }
    else if (auto *binary = llvm::dyn_cast<const clang::BinaryOperator>(&expr))
    {
        return buildBinaryExpression(*binary);
    }
    // Reference to variable, function, enum, etc.
    else if (auto *ref = llvm::dyn_cast<const clang::DeclRefExpr>(&expr))
    {
        return buildReference(*ref);
    }
    // Calls
    else if (auto *call = llvm::dyn_cast<const clang::CallExpr>(&expr))
    {
        // Function call
        if (auto *funcDecl = call->getDirectCallee())
        {
            return buildFunctionCall(*call, funcDecl->getNameAsString());
        }
    }
    // Implicit casts (unwrap them)
    else if (auto *cast = llvm::dyn_cast<const clang::ImplicitCastExpr>(&expr))
    {
        return buildExpression(*cast->getSubExpr());
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

    std::cout << "Unsupported expression found at line " << sourceManager.getSpellingLineNumber(expr.getExprLoc()) << std::endl;
    throw std::runtime_error("Unsupported expression");

}

// Build a variable declaration
VariableDeclaration ASTBuilder::buildVariableDecl(const clang::VarDecl &var)
{
    VariableDeclaration result;

    result.name = var.getNameAsString();
    // result.line = getLineNumber(var);
    // result.location = getLocationKey(var->getLocation());

    // Split type into more detail
    clang::QualType type = var.getType();
    result.type = type.getUnqualifiedType().getAsString();
    result.isConst = type.isConstQualified();
    result.isPointer = type->isPointerType();

    // Is it initialised?
    if (var.hasInit())
    {
        result.initializer = buildExpression(*var.getInit());
    }

    return result;
}

// Build a parameter
Parameter ASTBuilder::buildParameter(const clang::ParmVarDecl &param)
{
    Parameter result;

    result.name = param.getNameAsString();
    result.type = param.getType().getAsString();

    // Is there a default?
    if (param.hasDefaultArg())
    {
        result.defaultValue = buildExpression(*param.getDefaultArg());
    }

    return result;
}

// Build a function declaration
FunctionDeclaration ASTBuilder::buildFunctionDecl(const clang::FunctionDecl &fn)
{
    FunctionDeclaration result;

    // Use getQualifiedNameAsString(); to check for SplashKit functions "splashkit_lib::draw_bitmap"

    result.name = fn.getNameAsString();
    // result.line = getLineNumber(fn);
    // result.location = getLocationKey(fn->getLocation());
    result.returnType = fn.getReturnType().getAsString();

    // Get params
    for (auto *param : fn.parameters())
    {
        result.parameters.push_back(buildParameter(*param));
    }

    // Build the function body
    // auto *body = llvm::cast<clang::CompoundStmt>(fn->getBody());
    // result.body = buildBlock(body);

    return result;
}

void ASTBuilder::checkMinAndMax(
    clang::SourceLocation loc,
    clang::SourceLocation &min,
    clang::SourceLocation &max)
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
const clang::Expr* ASTBuilder::findExpressionInRange(
    const clang::Stmt *stmt,
    const clang::SourceRange targetRange,
    clang::SourceLocation &min,
    clang::SourceLocation &max)
{
    if (!stmt) return nullptr;

    clang::SourceLocation childMin;
    clang::SourceLocation childMax;

    // Search children first, so we get the smallest matching expression.
    for (const clang::Stmt *child : stmt->children())
    {
        // If we found a match in the children, return it
        if (auto *result = findExpressionInRange(child, targetRange, childMin, childMax))
            return result;
    }

    // Now we're at the bottom
    if (auto *expr = llvm::dyn_cast<const clang::Expr>(stmt))
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
std::unique_ptr<Statement> ASTBuilder::buildMacro(
    const clang::Stmt &stmt,
    const MacroInfo &macroInfo)
{
    std::cout << "Macro: "
        << macroInfo.name
        << '\n';

    stmt.dump();

    switch (macroInfo.kind)
    {
        case MacroKind::Section:
        {
            std::cout << "Section macro: " << macroInfo.name << std::endl;
            auto result = std::make_unique<Section>();
            result->name = macroInfo.name;

            // The SECTION macro becomes an if statement during compilation
            auto *ifStmt = llvm::dyn_cast<clang::IfStmt>(&stmt);

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
                auto statement = buildStatement(*child);
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
            auto argument = findExpressionInRange(&stmt, macroInfo.argumentRange, min, max);
            
            if (!argument)
            {
                std::cout << "findExpressionInRange returned nullptr!" << std::endl;
                return result;
            }

            std::cout << "Found argument: "
                    << argument->getStmtClassName()
                    << std::endl;
            
            result->expression = buildExpression(*argument);

            return result;
        }

        default:
        std::cout << "Macro: " << "DUNNO" << std::endl;
            auto result = std::make_unique<AssertionStatement>();
            return result;
    }
}

VariableDeclarationStatement ASTBuilder::buildVariableDeclStmt(const clang::VarDecl &var)
{
    VariableDeclarationStatement result;
    result.variable = buildVariableDecl(var);
    return result;
}

std::unique_ptr<Statement> ASTBuilder::buildStatement(const clang::Stmt &stmt)
{
    // Start by checking for macros
    // Get the key for this location
    unsigned key = getLocationKey(stmt.getBeginLoc());

    // Check for any macros at this location
    auto it = macros.find(key);

    // If there is one, build it
    if (it != macros.end())
    {
        return buildMacro(stmt, it->second);
    }
    // Declaration statement
    else if (auto *declStmt = llvm::dyn_cast<clang::DeclStmt>(&stmt))
    {
        // Check declarations
        for (clang::Decl *decl : declStmt->decls())
        {
            // Variable declaration
            if (auto *var = llvm::dyn_cast<clang::VarDecl>(decl))
            {
                return std::make_unique<VariableDeclarationStatement>(buildVariableDeclStmt(*var));
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
TestCase ASTBuilder::buildTestCase(
    const clang::FunctionDecl &func,
    const MacroInfo &macroInfo)
{
    TestCase result;

    // Utilise macro info obtained by the preprocessor
    result.name = macroInfo.name;
    // testCase.line = getLineNumber(func);
    // testCase.location = getLocationKey(macroInfo.location);
    result.tags = macroInfo.tags;

    // Build body
    clang::Stmt *body = func.getBody();
    auto *compound = llvm::dyn_cast<clang::CompoundStmt>(body);
    if (!compound) return result;

    // For every statement
    for (clang::Stmt *stmt : compound->body())
    {
        // Build it, then add it to the test case body
        std::unique_ptr<Statement> childStmt = buildStatement(*stmt);
        result.body.push_back(std::move(childStmt));
    }

    return result;
}

// Build a test file (top level)
void ASTBuilder::buildAST(CustomAST &AST)
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
            AST.globals.push_back(buildVariableDeclStmt(*var));
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
                AST.tests.push_back(buildTestCase(*func, it->second));
            }
            // Otherwise it's a top-level function
            else
            {
                AST.functions.push_back(buildFunctionDecl(*func));
            }
        }
    }
}
