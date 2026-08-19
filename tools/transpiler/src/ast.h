#pragma once

#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct Expression
{
    virtual ~Expression() = default;            // Explicit destructor for safety (pointers in derived classes)
    virtual void serialise(json &j) const = 0;  // Virtual function for base classes to define how they should be serialised
};

// Defines how nlohmann/json converts this type (Expression) to json
inline void to_json(json &j, const Expression &e)
{
    e.serialise(j);
}

// Defines how nlohmann/json converts this type (Unique pointer to Expression) to json
inline void to_json(json &j, const std::unique_ptr<Expression> &e)
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

struct UnaryExpression : Expression
{
    std::string op;
    std::unique_ptr<Expression> operand;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(UnaryExpression, op, operand)
    void serialise(json &j) const override { j = *this; j["kind"] = "UnaryExpression"; }
};

struct VariableDeclaration
{
    std::string name;
    std::string type;
    bool isConst;
    bool isPointer = false;
    std::unique_ptr<Expression> initializer;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(VariableDeclaration, name, type, isConst, isPointer, initializer)
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
inline void to_json(json &j, const Statement &e)
{
    e.serialise(j);
}

// Defines how nlohmann/json converts this type (Shared pointer to Statement) to json
inline void to_json(json &j, const std::shared_ptr<Statement> &e)
{
    if (e) e->serialise(j);
    else   j = nullptr;         // This converts to "null" with nlohmann/json
}

struct FunctionDeclaration
{
    std::string name;
    std::vector<Parameter> parameters;
    std::vector<std::shared_ptr<Statement>> body;
    std::string returnType;
    bool isGlobal;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(FunctionDeclaration, name, parameters, body, returnType, isGlobal)
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
    bool isGlobal;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(VariableDeclarationStatement, variable, isGlobal)
    void serialise(json &j) const override { j = *this; j["kind"] = "VariableDeclarationStatement"; }
};

struct ReturnStatement : Statement
{
    std::unique_ptr<Expression> value;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(ReturnStatement, value)
    void serialise(json &j) const override { j = *this; j["kind"] = "ReturnStatement"; }
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

struct Section
{
    std::string name;
    std::vector<std::shared_ptr<Statement>> body;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(Section, name, body)
};

struct TestCase
{
    std::string name;
    std::vector<std::string> tags;
    std::vector<Section> sections;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(TestCase, name, tags, sections)
};

struct CustomAST
{
    std::string filename;
    std::vector<VariableDeclarationStatement> globals;
    std::vector<FunctionDeclaration> functions;
    std::vector<TestCase> tests;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_ONLY_SERIALIZE(CustomAST, filename, globals, functions, tests)
};
