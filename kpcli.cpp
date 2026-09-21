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

    //one character occupies one terminal column
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

            lines.push_back({
                start,
                best_break,
                0,
                false
                });

            if (items[best_break].type == kp::Type::Penalty && items[best_break].penalty <= -kp::INF_PENALTY) {
                break;
            }

            start = kp::line_start_after(items, best_break);
        }

        return lines;
    }

    std::string render_line(
        const std::vector<kp::Item>& items,
        const kp::Line& line,
        bool last_line,
        Align alignment,
        int width) {

        std::vector<std::string> words;
        std::string current_word;

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

        int natural_width = 0;

        for (const std::string& word : words) {
            natural_width += text_width(word);
        }

        int gaps = static_cast<int>(words.size()) - 1;

        natural_width += gaps;

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


    }

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

            while (stream >> word) {
                words.push_back(word);
                blank = false;
            }

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