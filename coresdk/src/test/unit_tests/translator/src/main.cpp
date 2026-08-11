#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/Expr.h>
#include <clang/AST/Decl.h>
#include <clang/Lex/Lexer.h>
#include <clang/AST/RecursiveASTVisitor.h>

#include <iostream>
#include <llvm-18/llvm/Support/Casting.h>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

#include "parser.h"

namespace fs = std::filesystem;

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
