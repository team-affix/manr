#ifndef PW_HYPOTHESES_UNION_HPP
#define PW_HYPOTHESES_UNION_HPP

#include <cstddef>
#include <tuple>
#include <utility>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/pw_data_point.hpp"

// Fans `propose_hypotheses` out to every injected hypothesis source in order. Mirrors `proposer_union`.
template<typename... IHypothesisSources>
struct pw_hypotheses_union {
    pw_hypotheses_union(IHypothesisSources&... sources);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    template<size_t... Indices>
    void propose_each(std::index_sequence<Indices...>,
                      const std::vector<pw_data_point>& points,
                      std::vector<const expr*>& out);

    std::tuple<IHypothesisSources&...> sources_;
};

#define PW_HYPOTHESES_UNION_TEMPLATE template<typename... IHypothesisSources>
#define PW_HYPOTHESES_UNION          pw_hypotheses_union<IHypothesisSources...>

PW_HYPOTHESES_UNION_TEMPLATE PW_HYPOTHESES_UNION::pw_hypotheses_union(IHypothesisSources&... sources)
    : sources_(sources...) {}

PW_HYPOTHESES_UNION_TEMPLATE void PW_HYPOTHESES_UNION::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    propose_each(std::index_sequence_for<IHypothesisSources...>{}, points, out);
}

PW_HYPOTHESES_UNION_TEMPLATE
template<size_t... Indices>
void PW_HYPOTHESES_UNION::propose_each(
    std::index_sequence<Indices...>, const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    (std::get<Indices>(sources_).propose_hypotheses(points, out), ...);
}

#undef PW_HYPOTHESES_UNION
#undef PW_HYPOTHESES_UNION_TEMPLATE

#endif
