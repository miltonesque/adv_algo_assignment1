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

int main() {
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