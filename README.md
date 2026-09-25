# adv_algo_assignment1
Advanced Algorithms - Knuth-Plass Line Breaking Algorithm

The kpcli is a command line text formatter that breaks paragraphs into lines using the Knuth-Plass
line-breaking algorithm. THE CLI also includes a greedy line breaker to compare the 2 types.

Files: 
knuth_plass.hpp --> this is the implementation of the Knuth Plass algorithm
kpcli.cpp --> the command line tool. it contains argument parsing, paragraph reading, stats and line rendering, and the greedy comparison
cmakelists.txt --> this is the CMake build file
sample.txt --> a sample input with paragraphs to test the CLI

What is required?
A C++ 17 compiler
CMake 3.15 or later

How to Build with CMake
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

Change directory to the \build\release [this is where the cli will be run from]
The executable is build\release\kpcli

How to Run
./kpcli [options] [INPUTFILE]
If INPUTFILE is not typed in, the text is read from standard input. Paragraphs are separated by one
or more blank lines. The formatted text goes to standard output. All statistics and error messanges
go to standard error.

What are the Options?
starting with "-h" OR "--help" will display all the usage options and what they do.
These include, -w for width, -a for alignment, --greedy for comparison, --stats for per line statistics

Exit codes:
0 = successful
1 = input file could not be opened or the line breaking fails
2 = invalid CLI usage, incorrect option selected


