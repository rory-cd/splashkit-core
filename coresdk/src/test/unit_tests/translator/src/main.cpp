#include <filesystem>
#include <string>
#include <iostream>
#include <vector>

#include "ast.h"
#include "parser.h"
#include "translator.h"

namespace fs = std::filesystem;

int main(int argc, char** argv) {

    // Set test directory (Default to parent directory)
    fs::path pwd = fs::current_path();
    fs::path testDir = pwd.has_parent_path() ? pwd.parent_path() : pwd;
    if (argc > 1) testDir = argv[1];

    std::cout << "Scanning directory: " << testDir << "\n\n";
 
    // Check the chosen directory exists, and is actually a directory
    std::error_code ec;
    if (!fs::exists(testDir, ec) || !fs::is_directory(testDir, ec))
    {
        std::cerr << "Error: Directory does not exist or is inaccessible: " << testDir << "\n";
        return 1;
    }
 
    // Add every .cpp file in the directory 
    std::vector<std::string> cppFiles;
    for (const auto& entry : fs::directory_iterator(testDir))
    {
        if (entry.path().extension() == ".cpp")
        {
            std::string stem = entry.path().stem().string();                       // Filename without extension
            if (stem == "unit_test_main" || stem == "logging_handling") continue;  // Skip "main" file
            if (stem != "unit_test_test") continue;

            cppFiles.push_back(entry.path());                          // Add file to list
        }
    }
 
    // No valid files found
    if (cppFiles.empty())
    {
        std::cerr << "No .cpp files found in " << testDir << "\n";
        return 1;
    }
 
    std::cout << "Found " << cppFiles.size() << " .cpp files\n\n";

    // Parse files
    std::vector<CustomAST> *allASTs = parseTestFiles(cppFiles);

    // Translate the files to C#
    CSharpTranslator translator;
    fs::path outputDir = pwd / "generated" / "csharp";
    translator.translateASTSet(*allASTs, outputDir);
 
    return 0;
}
