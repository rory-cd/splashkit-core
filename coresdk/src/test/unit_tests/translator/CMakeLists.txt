#
# CMake file for building the translator
#

cmake_minimum_required(VERSION 3.10)

project(UnitTestTranslator)             # Define project name

set(CMAKE_CXX_STANDARD 17)              # Set C++ required version

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}) # Put the executable in the main directory

# Add source files to executable
add_executable(UnitTestTranslator
    src/main.cpp
)

find_package(LLVM REQUIRED CONFIG)      # Find the LLVM installation and use its CMake config
find_package(Clang REQUIRED CONFIG)     # Find the clang installation and use its CMake config

# Add the header paths from the config to the include dirs (PRIVATE so only this executable uses these paths)
target_include_directories(UnitTestTranslator PRIVATE
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/external
)

# Add the libraries
target_link_libraries(UnitTestTranslator PRIVATE
    clangTooling
    clangAST
    clangFrontend
)
