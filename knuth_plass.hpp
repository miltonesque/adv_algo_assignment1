//Knuth-Plass Line Breaking algorithm

/*
* A paragraph is designed as a sequence of:
* - boxes: these are fixed width text
* - glues: these are spaces that can stretch or shrink
* - penalities: these are possible places where a line can end
*/

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace kp {

    // A penalty this large means that a break is FORBIDDEN
    // a penalty this negative means that a break is REQUIRED
    constexpr double INF_PENALTY = 10000.0;

    // Badness is capped here, so extremely bad lines do not complicate the calculation
    constexpr double MAX_BADNESS = 10000.0;

    //the stretch of the "fill" glue that will end a paragraph so the last line can be short without being penalised.
    constexpr double FILL_STRETCH = 1e9;

    enum class Type { 
        Box, 
        Glue, 
        Penalty 
    };

    struct Item {
        Type type;
        double width = 0;      // box: width of the text; glue: natural width of the space; penalty: extra width that is used if we break here
        double stretch = 0;    // glue only
        double shrink = 0;     // glue only
        double penalty = 0;    // penalty only     
        std::string text;      // this is text that is printed out when the penalty is used as a break
    };

    //below are helper functions to construct items
    inline Item box(double width, const std::string& text) { 
        return { Type::Box, width, 0, 0, 0, text };
    }
    inline Item glue(double width, double stretch, double shrink) { 
        return { Type::Glue, width, stretch, shrink, 0, "" }; 
    }
    inline Item penalty(double width, double value, std::string text = "") {
        return { Type::Penalty, width, 0, 0, value, text };
    }

    //appending the standard paragraph ending:
    inline void end_paragraph(std::vector<Item>& items) {
        items.push_back(penalty(0, INF_PENALTY)); //no break before the fill glue
        items.push_back(glue(0, FILL_STRETCH, 0)); //lets last line be as short as it needs
        items.push_back(penalty(0, -INF_PENALTY)); //a forced final break
    }

    struct Params {
        std::vector<double> line_widths{ 72 };  // line i uses line_widths[0], the second uses line_widths[1], the final width is then repeated
        double tolerance = 1.0;               // max adjustment ratio accepted in the first normal pass thorough
        double line_penalty = 10;             // added to every line's badness
        double flagged_demerits = 10000;       // added when two consecutive lines both end at a flagged
        double fitness_demerits = 10000;       // adjacent lines whose fitness classes differ by more than one
        // is there is no solution within the 'tolerance' limit, it will
        // run with unlimited stretch and over fill lines instead of failing
        bool allow_emergency = true; 
    };

    struct Line {
        /* the line contains items [start, end]
        * item end is the penalty/glue where the break occurs
        */
        std::size_t start;
        std::size_t end;  

        double ratio;       // adjustment ratio: <0 shrink, >0 stretch. how much the glue had to stretch/shrink
        bool overfull;      // true if the line is wider than its target width and can be overfilled.
    };

    struct Result {
        std::vector<Line> lines;
        double demerits = 0;     // total demerits of the chosen breaks
        bool emergency = false;
    };

    /*
    * return the first item belonging to the next line.
    * after a break, spaces, glues and penalties items are skipped to the next box
    */
    inline std::size_t line_start_after(const std::vector<Item>& items, std::size_t break_position) {
        std::size_t i = break_position + 1;
        while (i < items.size() && items[i].type != Type::Box) {
            ++i;
        }
        return i;
    }

    namespace detail {

        /* this is a DP state that represents the possible way of reaching a specific line break
        * int prev will point to the state that produced THIS state. allows us to reconstruct optima
        * sequence of line breaks back at the end.
        */
        struct State {
            std::size_t break_position;
            std::size_t start;  // index of first item of the line that follows this break
            int line_number;           // number of lines ended at this break
            int fitness;        // this describes how loose/tight the lie is: 0 tight, 1 decent, 2 loose, 3 very loose
            bool flagged;       // was this break a flagged penalty?
            double ratio;       // adjustment ratio of the line that ends here
            double total;       // total demerits from the paragraph start to here
            int prev;           // index of the previous break
            bool overfull;
        };

        /* using prefix sums here to calculate the width of any interval in O(1)
        */
        struct PrefixSums {
            /*
            * width[i] = total box/glue width in [0,i)
            * stretch[i] = total stretchability in [0, i)
            * shrink[i]  = total shrinkability in [0, i)
            */
            std::vector<double> width;
            std::vector<double> stretch;
            std::vector<double> shrink;

            explicit PrefixSums(const std::vector<Item>& items)
                : width(items.size() + 1, 0),
                stretch(items.size() + 1, 0),
                shrink(items.size() + 1, 0)
            {
                for (std::size_t i = 0; i < items.size(); ++i) {
                    width[i + 1] = width[i];
                    stretch[i + 1] = stretch[i];
                    shrink[i + 1] = shrink[i];

                    if (items[i].type != Type::Penalty) {
                        width[i + 1] += items[i].width;
                        stretch[i + 1] += items[i].stretch;
                        shrink[i + 1] += items[i].shrink;
                    }
                }
            }
        };

        double natural_width(const std::vector<Item>& items, const PrefixSums& sums, std::size_t start, std::size_t end) {
            double result = sums.width[end] - sums.width[start];

            // a penalty only has a width if it is used as a break
            if (items[end].type == Type::Penalty) {
                result += items[end].width;
            }
            return result;
        }

        // ratio > 0 : glue must stretch
        // ratio < 0 : glue must shrink
        inline double adjustment_ratio(
            const std::vector<Item>& items, const PrefixSums& sums, std::size_t start, std::size_t end, double target_width)
        {
            double width = natural_width(items, sums, start, end);

            if (width < target_width) {
                double available_stretch =
                    sums.stretch[end] - sums.stretch[start];

                if (available_stretch <= 0) {
                    return std::numeric_limits<double>::infinity();
                }

                return (target_width - width) / available_stretch;
            }

            if (width > target_width) {
                double available_shrink =
                    sums.shrink[end] - sums.shrink[start];

                if (available_shrink <= 0) {
                    return -std::numeric_limits<double>::infinity();
                }

                return (target_width - width) / available_shrink;
            }

            return 0;
        }

        //determining the fitness class of a line
        inline int fitness_class(double ratio)
        {
            if (ratio < -0.5) 
                return 0;
            if (ratio <= 0.5) 
                return 1;
            if (ratio <= 1.0) 
                return 2;
            return 3;
        }

        //calculating the cost of adding one line.

        // this is the objective function being minimised by the dynamic program.
        inline double line_demerits(
            double ratio,
            const Item& break_item,
            const Params& params)
        {
            double badness =
                std::min(100.0 * std::pow(std::abs(ratio), 3), MAX_BADNESS);

            double value = params.line_penalty + badness;
            double demerits = value * value;

            if (break_item.type == Type::Penalty) {
                double p = break_item.penalty;

                if (p >= 0) {
                    demerits += p * p;
                }
                else if (p > -INF_PENALTY) {
                    demerits -= p * p;
                }
            }

            return demerits;
        }

        //finding out whether item (i) is a possible line break

        bool is_breakpoint(const std::vector<Item>& items, std::size_t i)
        {
            const Item& item = items[i];

            // a penalty is a ALWAYS a possible break unless forbidden
            if (item.type == Type::Penalty) {
                return item.penalty < INF_PENALTY;
            }

            //glue reps space, you can break after space if previous item is a box (word)
            return item.type == Type::Glue &&
                i > 0 &&
                items[i - 1].type == Type::Box;
        }

        bool is_forced(const Item& item)
        {
            return item.type == Type::Penalty &&
                item.penalty <= -INF_PENALTY;
        }


        inline bool is_flagged(const Item& item)
        {
            return item.type == Type::Penalty && item.width > 0;
        }

        //return the target width for a particular line
        inline double line_width(const Params& params, int line_number)
        {
            std::size_t index =
                std::min<std::size_t>(
                    static_cast<std::size_t>(line_number),
                    params.line_widths.size() - 1);

            return params.line_widths[index];
        }

        // Two states at the same break with different line numbers are only
        // interchangeable once both are past the last distinct line width.
        inline std::size_t width_class (const Params& params, int line_number)
        {
            return std::min<std::size_t>(
                static_cast<std::size_t>(line_number),
                params.line_widths.size() - 1);
        }
        
    }

}  // namespace kp