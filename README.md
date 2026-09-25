Advanced Algorithms Assignment 1
Knuth-Plass Line Breaking Algorithm

kpcli is a command-line text formatter that breaks paragraphs into lines using the Knuth-Plass line-breaking algorithm.

The CLI also includes a greedy line-breaking algorithm, allowing the two approaches to be compared.

Files

knuth_plass.hpp — Implementation of the Knuth-Plass line-breaking algorithm.

kpcli.cpp — Command-line tool containing argument parsing, paragraph reading, statistics, line rendering, and greedy line-breaking comparison.

CMakeLists.txt — CMake build configuration.

sample.txt — Sample input containing paragraphs for testing the CLI.

Requirements

A C++17 compatible compiler

CMake 3.15 or later

Building with CMake

Configure the project:

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release


Build the project:

cmake --build build --config Release


The executable will be located in:

build/Release/kpcli


Change into the Release directory before running the CLI:

cd build/Release

Usage
./kpcli [options] [INPUTFILE]


If INPUTFILE is not provided, text is read from standard input.

Paragraphs are separated by one or more blank lines.

Formatted text is written to standard output.

Statistics and error messages are written to standard error.

Options

Use -h or --help to display all available options and their descriptions.

Available options include:

-w — Set the line width.

-a — Set the text alignment.

--greedy — Compare the Knuth-Plass algorithm with the greedy line-breaking algorithm.

--stats — Display per-line statistics.

Exit Codes
Code
0	Successful execution.
1	Input file could not be opened, or line breaking failed.
2	Invalid CLI usage or an incorrect option was selected.