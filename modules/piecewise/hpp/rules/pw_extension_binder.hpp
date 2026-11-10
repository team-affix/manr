#ifndef PW_EXTENSION_BINDER_HPP
#define PW_EXTENSION_BINDER_HPP

#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/pw_data_point.hpp"
#include "value_objects/pw_symbols.hpp"
#include "value_objects/rewrite.hpp"

// Local rule, and the macro step of the piecewise algebra: when a piecewise's otherwise is an extension term, it
// asks the injected hypothesis sources what that term could be resolved to, given the branches as data. For each
// hypothesis that reproduces at least one branch (checked in detection), it detects one rewrite: resolve the term to
// that hypothesis and drop every branch it reproduces, in one step. The rewrite only assembles the result.
//
// The extension term is resolved where it stands, so this assumes it occurs nowhere else (the builder makes
// exactly one occurrence). Only a piecewise whose branches are all data points (`case`) is bound.
template<typename IProposeHypotheses, typename IFindUncovered, typename IAssemble>
struct pw_extension_binder {
    pw_extension_binder(IProposeHypotheses& propose_hypotheses,
                        IFindUncovered& find_uncovered,
                        IAssemble& assemble,
                        pw_symbols symbols);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    IProposeHypotheses& propose_hypotheses_;
    IFindUncovered& find_uncovered_;
    IAssemble& assemble_;
    pw_symbols symbols_;
    std::vector<const expr*> hypotheses_;
};

#define PW_EXTENSION_BINDER_TEMPLATE template<typename IProposeHypotheses, typename IFindUncovered, typename IAssemble>
#define PW_EXTENSION_BINDER          pw_extension_binder<IProposeHypotheses, IFindUncovered, IAssemble>

PW_EXTENSION_BINDER_TEMPLATE PW_EXTENSION_BINDER::pw_extension_binder(
    IProposeHypotheses& propose_hypotheses, IFindUncovered& find_uncovered, IAssemble& assemble, pw_symbols symbols)
    : propose_hypotheses_(propose_hypotheses)
    , find_uncovered_(find_uncovered)
    , assemble_(assemble)
    , symbols_(symbols)
    , hypotheses_() {}

PW_EXTENSION_BINDER_TEMPLATE void PW_EXTENSION_BINDER::detect(const expr* e, std::vector<rewrite>& out) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->id != symbols_.piecewise)
        return;
    const bool open = std::holds_alternative<expr::var>(f->args.back()->content);
    if (!open)
        return;
    const std::vector<const expr*> branches(f->args.begin(), f->args.end() - 1);
    std::vector<pw_data_point> points;
    points.reserve(branches.size());
    for (const expr* branch : branches) {
        const expr::functor& b = std::get<expr::functor>(branch->content);
        if (b.id != symbols_.branch)
            return;
        points.push_back(pw_data_point{.input = b.args[0], .output = b.args[1]});
    }
    hypotheses_.clear();
    propose_hypotheses_.propose_hypotheses(points, hypotheses_);
    for (const expr* hypothesis : hypotheses_) {
        const std::vector<const expr*> uncovered = find_uncovered_.find_uncovered(branches, hypothesis);
        if (uncovered.size() == branches.size())
            continue;
        out.push_back([this, uncovered, hypothesis]() { return assemble_.assemble(uncovered, hypothesis); });
    }
}

#undef PW_EXTENSION_BINDER
#undef PW_EXTENSION_BINDER_TEMPLATE

#endif
