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

    enum class Kind { 
        Box, 
        Glue, 
        Penalty 
    };

    struct Item {
        Kind kind;
        double width = 0;      // box: width of the text; glue: natural width of the space; penalty: extra width that is used if we break here
        double stretch = 0;    // glue only
        double shrink = 0;     // glue only
        double penalty = 0;    // penalty only
        
        std::string text;      // this is text that is printed out when the penalty is used as a break
    };

    inline Item box(double w, std::string text) { return { Kind::Box, w, 0, 0, 0, false, std::move(text) }; }
    inline Item glue(double w, double stretch, double shrink) { return { Kind::Glue, w, stretch, shrink, 0, false, "" }; }
    inline Item penalty(double w, double p, bool flagged, std::string text = "") {
        return { Kind::Penalty, w, 0, 0, p, flagged, std::move(text) };
    }

    struct Params {
        std::vector<double> line_widths{ 72 };  // line i uses line_widths[min(i, size-1)]
        double tolerance = 1.0;               // largest adjustment ratio accepted in the first pass
        double line_penalty = 10;             // added to every line's badness; favours fewer lines
        double flagged_demerits = 3000;       // two flagged breaks in a row
        double fitness_demerits = 3000;       // adjacent lines whose fitness classes differ by more than one
    };

    struct Line {
        std::size_t start;  // first item of the line
        std::size_t end;    // index of the break item; the line holds items [start, end)
        double ratio;       // adjustment ratio: <0 shrink, >0 stretch
        bool overfull;      // could not be made to fit (emergency pass only)
    };

    struct Result {
        std::vector<Line> lines;
        double demerits = 0;     // total demerits of the chosen breaks
        bool emergency = false;  // true if the first pass (with `tolerance`) found no solution
    };

    // First item of the line that begins after a break at b: the break item itself
    // is consumed, then glue and ordinary penalties are discarded until the next
    // box or forced break. (Also used by other line breakers, e.g. a greedy one.)
    inline std::size_t line_start_after(const std::vector<Item>& items, std::size_t b) {
        std::size_t i = b + 1;
        while (i < items.size() && items[i].kind != Kind::Box &&
            !(items[i].kind == Kind::Penalty && items[i].penalty <= -INF_PENALTY))
            ++i;
        return i;
    }

    namespace detail {

        struct Node {
            std::size_t pos;    // index of the break item (root: 0, meaning "paragraph start")
            std::size_t start;  // first item of the line that follows this break
            int line;           // number of lines ended at this break
            int fitness;        // 0 tight, 1 decent, 2 loose, 3 very loose
            bool flagged;       // was this break a flagged penalty?
            double ratio;       // adjustment ratio of the line that ends here
            double total;       // total demerits from the paragraph start to here
            int prev;           // index of the previous break in the node pool (-1 for the root)
            bool overfull;
        };

        class Breaker {
        public:
            Breaker(const std::vector<Item>& items, const Params& p) : items_(items), p_(p) {
                // Prefix sums over boxes and glue: sum[i] covers items [0, i).
                // Penalty widths are deliberately excluded: they only count if we break there.
                std::size_t n = items_.size();
                w_.assign(n + 1, 0);
                y_.assign(n + 1, 0);
                z_.assign(n + 1, 0);
                for (std::size_t i = 0; i < n; ++i) {
                    const Item& it = items_[i];
                    bool counts = it.kind != Kind::Penalty;
                    w_[i + 1] = w_[i] + (counts ? it.width : 0);
                    y_[i + 1] = y_[i] + it.stretch;
                    z_[i + 1] = z_[i] + it.shrink;
                }
            }

            // Returns false if no solution exists within the tolerance (non-emergency pass).
            bool run(double tolerance, bool emergency, Result& out) {
                pool_.clear();
                pool_.push_back({ 0, 0, 0, 1, false, 0, 0, -1, false });  // root: decent, zero demerits
                std::vector<int> active{ 0 };

                for (std::size_t b = 0; b < items_.size(); ++b) {
                    if (!feasible(b)) continue;
                    const Item& it = items_[b];
                    bool forced = it.kind == Kind::Penalty && it.penalty <= -INF_PENALTY;
                    bool flagged = it.kind == Kind::Penalty && it.flagged;

                    std::vector<Node> cands;  // best new node per (line, fitness) at this b
                    std::vector<int> keep;    // active nodes that survive b
                    int rescue = -1;          // emergency fallback if every node dies here

                    for (int a : active) {
                        const Node& A = pool_[a];
                        if (A.start > b) { keep.push_back(a); continue; }  // line would be empty
                        double r = ratio(A, b);
                        if (r >= -1 && r <= tolerance)
                            offer(cands, make_node(a, b, r, flagged, false));
                        if (r < -1 || forced) {
                            // Deactivate: every later line from A is at least as overfull,
                            // and nothing may cross a forced break.
                            if (rescue < 0 || pool_[a].pos > pool_[rescue].pos ||
                                (pool_[a].pos == pool_[rescue].pos && pool_[a].total < pool_[rescue].total))
                                rescue = a;
                        }
                        else {
                            keep.push_back(a);
                        }
                    }

                    if (keep.empty() && cands.empty()) {
                        if (!emergency) return false;
                        // Last resort: an overfull line from the most recent break.
                        cands.push_back(make_node(rescue, b, -1, flagged, true));
                    }
                    for (Node& c : cands) {
                        pool_.push_back(c);
                        keep.push_back(static_cast<int>(pool_.size() - 1));
                    }
                    active.swap(keep);
                }

                // Every surviving node ends at the final forced break.
                int best = -1;
                for (int a : active)
                    if (best < 0 || pool_[a].total < pool_[best].total) best = a;
                if (best < 0) return false;

                out.lines.clear();
                out.demerits = pool_[best].total;
                for (int a = best; pool_[a].prev >= 0; a = pool_[a].prev) {
                    const Node& N = pool_[a];
                    out.lines.push_back({ pool_[N.prev].start, N.pos, N.ratio, N.overfull });
                }
                std::reverse(out.lines.begin(), out.lines.end());
                return true;
            }

        private:
            const std::vector<Item>& items_;
            const Params& p_;
            std::vector<double> w_, y_, z_;
            std::vector<Node> pool_;  // every node ever created; `prev` links index into it

            bool feasible(std::size_t b) const {
                const Item& it = items_[b];
                if (it.kind == Kind::Penalty) return it.penalty < INF_PENALTY;
                return it.kind == Kind::Glue && b > 0 && items_[b - 1].kind == Kind::Box;
            }

            double width_for(int line) const {
                const auto& lw = p_.line_widths;
                return lw[std::min<std::size_t>(static_cast<std::size_t>(line), lw.size() - 1)];
            }

            double ratio(const Node& A, std::size_t b) const {
                double natural = w_[b] - w_[A.start];
                if (items_[b].kind == Kind::Penalty) natural += items_[b].width;
                double target = width_for(A.line);
                if (natural < target) {
                    double y = y_[b] - y_[A.start];
                    return y > 0 ? (target - natural) / y : std::numeric_limits<double>::infinity();
                }
                if (natural > target) {
                    double z = z_[b] - z_[A.start];
                    return z > 0 ? (target - natural) / z : -std::numeric_limits<double>::infinity();
                }
                return 0;
            }

            Node make_node(int a, std::size_t b, double r, bool flagged, bool overfull) const {
                const Node& A = pool_[a];
                const Item& it = items_[b];
                double bad = std::min(100 * std::pow(std::fabs(r), 3), MAX_BADNESS);
                double d = (p_.line_penalty + bad) * (p_.line_penalty + bad);
                if (it.kind == Kind::Penalty) {
                    if (it.penalty >= 0) d += it.penalty * it.penalty;
                    else if (it.penalty > -INF_PENALTY) d -= it.penalty * it.penalty;
                }
                if (A.flagged && flagged) d += p_.flagged_demerits;
                int fit = r < -0.5 ? 0 : r <= 0.5 ? 1 : r <= 1 ? 2 : 3;
                if (std::abs(fit - A.fitness) > 1) d += p_.fitness_demerits;
                if (overfull) d += MAX_BADNESS * MAX_BADNESS;
                return { b, line_start_after(items_, b), A.line + 1, fit, flagged, r, A.total + d, a, overfull };
            }

            // Keep at most one candidate per (line, fitness): the one with fewest demerits.
            static void offer(std::vector<Node>& cands, const Node& n) {
                for (Node& c : cands)
                    if (c.line == n.line && c.fitness == n.fitness) {
                        if (n.total < c.total) c = n;
                        return;
                    }
                cands.push_back(n);
            }
        };

    }  // namespace detail

    // Break a paragraph into lines. Tries `tolerance` first; if no solution exists,
    // retries accepting any non-overfull line and, as a last resort, overfull lines.
    inline Result break_lines(const std::vector<Item>& items, const Params& params = {}) {
        if (items.empty() || items.back().kind != Kind::Penalty || items.back().penalty > -INF_PENALTY)
            throw std::invalid_argument("paragraph must end with a forced break");
        if (params.line_widths.empty())
            throw std::invalid_argument("need at least one line width");

        detail::Breaker br(items, params);
        Result res;
        if (br.run(params.tolerance, false, res)) return res;
        br.run(std::numeric_limits<double>::infinity(), true, res);
        res.emergency = true;
        return res;
    }

}  // namespace kp