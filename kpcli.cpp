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

namespace {

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
        "  -w N, --width N        line width (default is 70)\n"
        "  -a MODE, --align MODE  left | right | center | justify \n"
        "  --greedy               use greedy line breaking \n"
        "  --stats                printing statistics to the stderr\n"
        "  -h, --help             showing the help!\n";

    void usage_error(const std::string& message)
    {
        std::cerr << "kpcli: " << message << '\n';
        std::cerr << USAGE;
        std::exit(2);
    }

    Options parse_argument(int arg_count, char** arg_vector) {

        Options options;

        for (int i = 1; i < arg_count; ++i) {
            std::string argument = arg_vector[i];

            if (argument == "-h" || argument == "--help") {
                std::cout << USAGE;
                std::exit(0);
            }

            if (argument == "-w" || argument == "--width") {
                std::string value = arg_vector[++i];
                char* end = nullptr;

                long width =
                    std::strtol(value.c_str(), &end, 10);

                if (*end != '\0' || width <= 0) {
                    usage_error("invalid width, width has to be a positive number");
                }
                continue;

            }

            if (argument == "-a" || argument == "--align") {
                std::string mode = arg_vector[++i];

                std::string mode = arg_vector[++i];

                if (mode == "left") {
                    options.align = Align::Left;
                }
                else if (mode == "right") {
                    options.align = Align::Right;
                }
                else if (mode == "center") {
                    options.align = Align::Center;
                }
                else if (mode == "justify") {
                    options.align = Align::Justify;
                }
                else {
                    usage_error("unknown alignment: " + mode);
                }
                continue;
            }

            if (argument.empty() || argument[0] != '-' || argument == "-") {

                if (!options.inputfile.empty()) {
                    usage_error("more than one input file given");
                }

                options.inputfile =
                    argument == "-" ? "" : argument;

                if (argument.empty()) {
                    usage_error("empty filename");
                }

                continue;
            }

            usage_error(
                "unknown option: " + argument);

        }
        return options;
    }

    int text_width(const std::string& text)
    {
        return static_cast<int>(text.size());
    }

    std::vector<kp::Item> make_items(const std::vector<std::string>& words) {

        std::vector<kp::Item> items;
        
        for (std::size_t i = 0;
            i < words.size();
            ++i) {

            if (i > 0) {
                // A normal space has width 1 and can stretch by 1.
                items.push_back(kp::glue(1, 1, 0));
            }

            items.push_back(
                kp::box(text_width(words[i]), words[i]));
        }

        kp::end_paragraph(items);
        return items;
    }


    void format_paragraph(
        const std::vector<std::string>& words,
        const Options& options,
        int paragraph_number,
        int& total_lines,
        int& total_overfull,
        double& total_slack_squared) 
    
    {
        if (words.empty()) {
            return;
        }

        std::vector<kp::Item> items = make_items(words);
        
        std::vector<kp::Line> lines;
        double demerits = 0;
        bool emergency = false;

        kp::Params params;

        params.line_widths = {
            static_cast<double>(options.width)
        };

        kp::Result result = kp::break_lines(items, params);

        lines = result.lines;
        demerits = result.demerits;
        emergency = result.emergency;

    }

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