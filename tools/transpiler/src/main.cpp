#include <filesystem>
#include <string>
#include <iostream>
#include <vector>

#include "ast.h"
#include "parser.h"
#include "translator.h"

namespace fs = std::filesystem;

int main(int argc, char** argv) {

    // Set scan directory (Default to unit test directory)
    fs::path scanDir = fs::path(SPLASHKIT_TESTS) / "unit_tests";
    if (argc > 1) scanDir = argv[1];

    std::cout << "Scanning directory: " << scanDir << "\n\n";
 
    // Check the chosen directory exists, and is actually a directory
    std::error_code ec;
    if (!fs::exists(scanDir, ec) || !fs::is_directory(scanDir, ec))
    {
        std::cerr << "Error: Directory does not exist or is inaccessible: " << scanDir << "\n";
        return 1;
    }
 
    // Add every .cpp file in the directory 
    std::vector<std::string> cppFiles;
    for (const auto& entry : fs::directory_iterator(scanDir))
    {
        if (entry.path().extension() == ".cpp")
        {
            std::string stem = entry.path().stem().string();                       // Filename without extension
            if (stem == "unit_test_main" || stem == "logging_handling") continue;  // Skip "main" file
            if (stem != "unit_test_color"
                && stem != "unit_test_test"
                && stem != "unit_test_music") continue;

            cppFiles.push_back(entry.path());                          // Add file to list
        }
    }
 
    // No valid files found
    if (cppFiles.empty())
    {
        std::cerr << "No .cpp files found in " << scanDir << "\n";
        return 1;
    }
 
    std::cout << "Found " << cppFiles.size() << " .cpp files\n\n";

    // Parse files
    fs::path debugOutputDir = fs::path(SPLASHKIT_TESTS) / "unit_tests" / "generated" / "json";
    std::vector<CustomAST> *allASTs = parseTestFiles(cppFiles, debugOutputDir);
    
    // Translate the files to C#
    CSharpTranslator translator;
    fs::path outputDir = fs::path(SPLASHKIT_TESTS) / "unit_tests" / "generated" / "csharp";
    translator.translateASTSet(*allASTs, outputDir);
 
    return 0;
}
