#ifndef PW_ASSEMBLER_HPP
#define PW_ASSEMBLER_HPP

#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/pw_symbols.hpp"

// Builds `pw(branch, ..., otherwise)`. A piecewise with no branches left is just its otherwise, so that is
// returned instead: this is the step that makes the piecewise disappear.
template<typename IMakeFunctor>
struct pw_assembler {
    pw_assembler(IMakeFunctor& make_functor, pw_symbols symbols);
    const expr* assemble(const std::vector<const expr*>& branches, const expr* otherwise);
private:
    IMakeFunctor& make_functor_;
    pw_symbols symbols_;
};

#define PW_ASSEMBLER_TEMPLATE template<typename IMakeFunctor>
#define PW_ASSEMBLER          pw_assembler<IMakeFunctor>

PW_ASSEMBLER_TEMPLATE PW_ASSEMBLER::pw_assembler(IMakeFunctor& make_functor, pw_symbols symbols)
    : make_functor_(make_functor)
    , symbols_(symbols) {}

PW_ASSEMBLER_TEMPLATE const expr* PW_ASSEMBLER::assemble(
    const std::vector<const expr*>& branches, const expr* otherwise) {
    if (branches.empty())
        return otherwise;
    std::vector<const expr*> args = branches;
    args.push_back(otherwise);
    return make_functor_.make_functor(symbols_.piecewise, args);
}

#undef PW_ASSEMBLER
#undef PW_ASSEMBLER_TEMPLATE

#endif
