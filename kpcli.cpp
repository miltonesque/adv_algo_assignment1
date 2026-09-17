/* this is the CLI implementation of the assignment
* SCAFFOLD
*  kpcli [options] [INPUTFILE]
* maybe paragraph separated by blank lines.
* 
* options:
* - width
* - alignment
* - compare
* - stats/ ratio / fitness
* - input file option
*  - helper
* 
* flowchart: CLI parsing >> read paragraphs >> words to box/glues/penalties >> knuthplass OR greedy >> rendering or alignment >> stats/output
*/

#include "knuth_plass.hpp"

#include <iostream>
#include <string>
#include <vector>

enum class Align {
    Left,
    Right,
    Center,
    Justify
};

struct Options {
    int width = 70;
    Align align = Align::Left;

    bool greedy = false;
    bool stats = false;

    std::string inputfile; //if empty it will read standard input
};

    const char* USAGE =
    "how to use: ./kpcli [options] [INPUTFILE] \n"
    "\n"
    "options:\n"
    "  -w N, --width N\n"
    "  -a MODE, --align MODE\n"
    "  --greedy\n"
    "  --stats\n"
    "  -h, --help\n";

    Options parse_argument(int arg_count, char** arg_vector) {

        Options options;

        for (int i = 1; i < arg_count; ++i) {
            std::string argument = arg_vector[i];

            if (argument == "-h" || argument == "--help") {
                std::cout << USAGE;
                std::exit(0);
            }
        }
        return options;
    }

int main(int arg_count, char** arg_vector) {

    Options options = parse_argument(arg_count, arg_vector);

	std::vector<kp::Item> items;

    items.push_back(kp::box(3, "The"));
    items.push_back(kp::glue(1, 1, 0));
    items.push_back(kp::box(6, "simple"));
    items.push_back(kp::glue(1, 1, 0));
    items.push_back(kp::box(5, "tests"));

    kp::end_paragraph(items);

    kp::Params params;
    params.line_widths = { 20 };

    kp::Result result =
        kp::break_lines(items, params);

    std::cout << "lines: "
        << result.lines.size()
        << '\n';

    std::cout << "demerits: "
        << result.demerits
        << '\n';

}