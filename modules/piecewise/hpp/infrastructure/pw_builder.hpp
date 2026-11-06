#ifndef PW_BUILDER_HPP
#define PW_BUILDER_HPP

#include <cstdint>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/pw_data_point.hpp"
#include "value_objects/pw_symbols.hpp"

// Turns a data set into its piecewise: one `case(input, output)` per point, and the extension term
// `?extension_index` as the catch-all for every input the data does not cover.
template<typename IMakeFunctor, typename IMakeVar, typename IAssemble>
struct pw_builder {
    pw_builder(IMakeFunctor& make_functor, IMakeVar& make_var, IAssemble& assemble, pw_symbols symbols);
    const expr* build(const std::vector<pw_data_point>& points, uint32_t extension_index);
private:
    IMakeFunctor& make_functor_;
    IMakeVar& make_var_;
    IAssemble& assemble_;
    pw_symbols symbols_;
};

#define PW_BUILDER_TEMPLATE template<typename IMakeFunctor, typename IMakeVar, typename IAssemble>
#define PW_BUILDER          pw_builder<IMakeFunctor, IMakeVar, IAssemble>

PW_BUILDER_TEMPLATE PW_BUILDER::pw_builder(
    IMakeFunctor& make_functor, IMakeVar& make_var, IAssemble& assemble, pw_symbols symbols)
    : make_functor_(make_functor)
    , make_var_(make_var)
    , assemble_(assemble)
    , symbols_(symbols) {}

PW_BUILDER_TEMPLATE const expr* PW_BUILDER::build(const std::vector<pw_data_point>& points, uint32_t extension_index) {
    std::vector<const expr*> branches;
    branches.reserve(points.size());
    for (const pw_data_point& point : points)
        branches.push_back(make_functor_.make_functor(symbols_.branch, {point.input, point.output}));
    return assemble_.assemble(branches, make_var_.make_var(extension_index));
}

#undef PW_BUILDER
#undef PW_BUILDER_TEMPLATE

#endif
