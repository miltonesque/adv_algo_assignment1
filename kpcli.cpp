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
#include <sstream>
#include <fstream>
#include <istream>
#include <exception>
#include <cstdlib>

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

    //parses command line arguments and converts them into an Option
    Options parse_argument(int arg_count, char** arg_vector) {

        Options options;

        for (int i = 1; i < arg_count; ++i) {
            std::string argument = arg_vector[i];

            if (argument == "-h" || argument == "--help") {
                std::cout << USAGE;
                std::exit(0);
            }

            //width requires a value in the next commad line argument
            if (argument == "-w" || argument == "--width") {

                if (i + 1 >= arg_count) {
                    usage_error("missing value for " + argument);
                }

                std::string value = arg_vector[++i];
                char* end = nullptr;

                //strtol give you the ability to detect values that are not integers
                long width = std::strtol(value.c_str(), &end, 10);

                if (*end != '\0' || width <= 0) {
                    usage_error("invalid width, width has to be a positive number");
                }

                options.width = static_cast<int>(width);

                continue;

            }

            if (argument == "--greedy") {
                options.greedy = true;
                continue;
            }

            if (argument == "--stats") {
                options.stats = true;
                continue;
            }

            if (argument == "-a" || argument == "--align") {
                std::string mode = arg_vector[++i];

                if (i + 1 >= arg_count) {
                    usage_error("missing alignment mode");
                }

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

    //one character occupies one terminal column
    //returns the number of terminal columns occupied by a string
    int text_width(const std::string& text)
    {
        return static_cast<int>(text.size());
    }

    //converts the words into boxes and glue
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

        //paragraph ending penalty --> forced breakpoint at the end
        kp::end_paragraph(items);
        return items;
    }

    //greedy implementation
    /*
    * at each step it will take the longest line that fits.
    */
    std::vector<kp::Line> greedy_break(const std::vector<kp::Item>& items, int width) {

        std::vector<kp::Line> lines;
        std::size_t start = 0;

        while (start < items.size()) {
            double current_width = 0;

            //if no break found yet
            std::size_t best_break = items.size();

            for (std::size_t i = start; i < items.size(); ++i) {

                const kp::Item& item = items[i];

                //if forcing a break, the paragraph is ended
                if (item.type == kp::Type::Penalty && item.penalty <= -kp::INF_PENALTY) {

                    //only taking the rbeak if the text fits so far or if there is no previous break to fall back on
                    if (current_width <= width || best_break == items.size()) {
                        best_break = i;
                    }
                    break;
                }

                //need  to check BEFORE adding the glue, a space at a break is thrown away and does not count
                if (item.type == kp::Type::Glue && i > 0 && items[i - 1].type == kp::Type::Box) {

                    if (current_width <= width || best_break == items.size()) {
                        // either it fits, or this is the first break
                        // after an over-long word: take the breakpoint!
                        best_break = i;
                    }

                    if (current_width > width) {
                        break;
                    }
                }

                if (item.type != kp::Type::Penalty) {
                    current_width += item.width;
                }
            }
            //store the slected range, break is excluded from the line and will be handled by render_line
            lines.push_back({
                start,
                best_break,
                0,
                false
                });

            //once paragraph ending has been chosen then the paragraph is complete
            if (items[best_break].type == kp::Type::Penalty && items[best_break].penalty <= -kp::INF_PENALTY) {
                break;
            }

            //you can move past the breakpoint and its associated glue
            start = kp::line_start_after(items, best_break);
        }

        return lines;
    }

    //converts one line from the box/glue representation into a final printable string
    //alignment is also handled here because rendering will decide how any available space is used.
    std::string render_line(
        const std::vector<kp::Item>& items,
        const kp::Line& line,
        bool last_line,
        Align alignment,
        int width) {

        std::vector<std::string> words;
        std::string current_word;

        //reconstructing the words in a line by joining consecutive box items and using the glue as a space
        for (std::size_t i = line.start; i < line.end; i++) {

            const kp::Item& item = items[i];

            if (item.type == kp::Type::Box) {
                current_word += item.text;
            }
            else if (item.type == kp::Type::Glue) {
                if (!current_word.empty()) {
                    words.push_back(current_word);
                    current_word.clear();
                }
            }
        }

        if (!current_word.empty()) {
            words.push_back(current_word);
        }

        const kp::Item& break_item = items[line.end]; //penalties can add text to the end of the line

        if (break_item.type == kp::Type::Penalty && !break_item.text.empty()) {

            if (words.empty()) {
                words.push_back(break_item.text);
            }
            else {
                words.back() += break_item.text;
            }
        }

        if (words.empty()) {
            return "";
        }

        //calculating the natural width before any alignment has been added
        int natural_width = 0;

        for (const std::string& word : words) {
            natural_width += text_width(word);
        }

        int gaps = static_cast<int>(words.size()) - 1;

        natural_width += gaps;

        //any amount of unused space that is available on the line
        //using max means that alignment will not try to remove any spaces if the line is already full
        int extra = std::max(0, width - natural_width);

        bool justify = alignment == Align::Justify && !last_line && gaps > 0;

        std::string result;

        //shifting entire alignment to the right
        if (alignment == Align::Right) {
            result.append(extra, ' ');
        }

        //center alignment is about half extra space before the line
        if (alignment == Align::Center) {
            result.append(extra / 2, ' ');
        }

        for (int i = 0; i < static_cast<int>(words.size()); ++i) {

            result += words[i];

            if (i == gaps) {
                break;
            }

            int spaces = 1;

            //need to dritribute columns as equally as possible. if extra space can't
            //be divided, remainder will go to gaps later
            if (justify) {
                spaces += ((i + 1) * extra) / gaps - (i * extra) / gaps;
            }

            result.append(spaces, ' ');
        }
        return result;
    }

    //count the columns that are used by a line
    int line_width(const std::string& text)
    {
        return text_width(text);
    }

    //formats a paragraph using either knuth plass or greedy
    //also includes stats that are passed by so that format(stream)can report the totals across all paragraphs
    void format_paragraph(
        const std::vector<std::string>& words,
        const Options& options,
        int paragraph_number,
        int& total_lines,
        int& total_overfull,
        double& total_slack)
    {
        if (words.empty()) {
            return;
        }

        //building the box, glue, penalty representation used
        std::vector<kp::Item> items = make_items(words);

        std::vector<kp::Line> lines;
        double demerits = 0;
        bool emergency = false;

        if (options.greedy) {
            lines = greedy_break(items, options.width);
        }
        else {
            kp::Params params;

            params.line_widths = {
                static_cast<double>(options.width)
            };

            kp::Result result = kp::break_lines(items, params);

            lines = result.lines;
            demerits = result.demerits;
            emergency = result.emergency;
        }

        if (options.stats) {
            std::cerr << "paragraph " << paragraph_number << ":\n";
        }

        //need to out each formatted line of text, test it against the specified width and maybe also print stats about each line

        //loop through every element in lines
        //check if the current line is the last line

        for (std::size_t i = 0; i < lines.size(); i++) {
            bool last_line = (i + 1) == lines.size();

            //call render lines to turn words into formatted string
            std::string output = render_line(items, lines[i], last_line, options.align, options.width);

            //calculate width uses and unused space / slack
            int actual_width = line_width(output);

            //print formatted line to terminal
            //should track overfull lines and how many lines used (maybe write which line is overfull in the line stats)
            //output line stats as well that keep all ratios and slack if this option is used
            int slack = options.width - actual_width;

            bool overfull = slack < 0;

            std::cout << output << '\n';

            ++total_lines;

            if (overfull) {
                ++total_overfull;
            }
            else if (!last_line) {
                total_slack += slack * slack;
            }

            if (options.stats) {
                std::cerr << "  line " << (i + 1) << ": " << actual_width << "/" << options.width;

                if (!options.greedy) {
                    std::cerr << "  ratio " << lines[i].ratio;
                }

                std::cerr << "  slack " << slack;

                if (overfull) {
                    std::cerr << "  OVERFULL";
                }

                std::cerr << '\n';
            }
        }

        if (options.stats && !options.greedy) {
            std::cerr << "  demerits: " << demerits << (emergency ? "  (emergency pass)" : "") << '\n';
        }

    }
}
    //reads the input stream and will group words
    void format_stream(std::istream& input, const Options& options) {

        std::vector<std::string> words;

        bool first_paragraph = true;
        int paragraph_number = 0;

        int total_lines = 0;
        int total_overfull = 0;

        double total_slack = 0;

        std::string line;

        while (std::getline(input, line)) {
            std::istringstream stream(line);

            std::string word;
            bool blank = true;

            //automatically treats consecutive whtiespaces are sepaators, multiple spaces in input are normalised
            while (stream >> word) {
                words.push_back(word);
                blank = false;
            }

            //if the input is blank line, terminates the current paragraph
            if (blank && !words.empty()) {
                if (!first_paragraph) {
                    std::cout << '\n';
                }
                ++paragraph_number;

                format_paragraph(words, options, paragraph_number, total_lines, total_overfull, total_slack);

                words.clear();
                first_paragraph = false;
            }
        }

        //processing the final paragraph even if the input does not end with a blank line
        if (!words.empty()) {
            if (!first_paragraph) {
                std::cout << '\n';
            }
            ++paragraph_number;
            format_paragraph(words, options, paragraph_number, total_lines, total_overfull, total_slack);

        }

        if (options.stats) {
            std::cerr << "total: " << paragraph_number << " paragraph(s), " << total_lines << " line(s)\n";

            std::cerr << "sum of slack: " << total_slack << '\n';

            if (total_overfull > 0) {
                
                std::cerr << "overfull lines: " << total_overfull << '\n';
            }

        }

    }

int main(int arg_count, char** arg_vector) {

    //parsing and validating all cli configuration before processing the input.
    Options options = parse_argument(arg_count, arg_vector);

    try {
        if (options.inputfile.empty()) {
            format_stream(std::cin, options);
            return 0;
        }

        std::ifstream input(options.inputfile);

        if (!input) {
            std::cerr << "kpcli: cannot open " << options.inputfile << '\n';

            return 1;
        }

        format_stream(input, options);
    }

    catch (const std::exception& error) {
        std::cerr << "kpcli: " << error.what() << '\n';

        return 1;
    }
    return 0;

}