#ifndef ARITH_HOLE_SOLVER_HPP
#define ARITH_HOLE_SOLVER_HPP

#include <cstdint>
#include <optional>
#include <variant>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"

// Solves `pattern = value` over the integers for one extension term `?hole` of the pattern, when the pattern is linear
// in it: built from the hole and literals by `add`, `sub`, and `mul` with at most one side holding the hole, it is
// `slope * ?hole + offset`, and the solution is `(value - offset) / slope` when that divides exactly. Nothing for a
// value that is not a literal, any other leaf or functor, a slope of 0, or no integer solution.
template<typename IFindLiteralValue, typename IMakeLiteral>
struct arith_hole_solver {
    arith_hole_solver(IFindLiteralValue& find_literal_value,
                      IMakeLiteral& make_literal,
                      uint32_t add_symbol,
                      uint32_t sub_symbol,
                      uint32_t mul_symbol);
    std::optional<const expr*> solve_hole(const expr* pattern, uint32_t hole, const expr* value);
private:
    struct linear {
        integer slope;
        integer offset;
    };
    std::optional<linear> find_linear(const expr* e, uint32_t hole);

    IFindLiteralValue& find_literal_value_;
    IMakeLiteral& make_literal_;
    uint32_t add_symbol_;
    uint32_t sub_symbol_;
    uint32_t mul_symbol_;
};

#define ARITH_HOLE_SOLVER_TEMPLATE template<typename IFindLiteralValue, typename IMakeLiteral>
#define ARITH_HOLE_SOLVER          arith_hole_solver<IFindLiteralValue, IMakeLiteral>

ARITH_HOLE_SOLVER_TEMPLATE ARITH_HOLE_SOLVER::arith_hole_solver(IFindLiteralValue& find_literal_value,
                                                                 IMakeLiteral& make_literal,
                                                                 uint32_t add_symbol,
                                                                 uint32_t sub_symbol,
                                                                 uint32_t mul_symbol)
    : find_literal_value_(find_literal_value)
    , make_literal_(make_literal)
    , add_symbol_(add_symbol)
    , sub_symbol_(sub_symbol)
    , mul_symbol_(mul_symbol) {}

ARITH_HOLE_SOLVER_TEMPLATE std::optional<const expr*> ARITH_HOLE_SOLVER::solve_hole(
    const expr* pattern, uint32_t hole, const expr* value) {
    const std::optional<integer> target = find_literal_value_.find_value(value);
    if (!target.has_value())
        return std::nullopt;
    const std::optional<linear> form = find_linear(pattern, hole);
    if (!form.has_value() || form.value().slope == 0)
        return std::nullopt;
    const integer difference = target.value() - form.value().offset;
    const bool divides = difference % form.value().slope == 0;
    if (!divides)
        return std::nullopt;
    return make_literal_.make_literal(difference / form.value().slope);
}

ARITH_HOLE_SOLVER_TEMPLATE std::optional<typename ARITH_HOLE_SOLVER::linear> ARITH_HOLE_SOLVER::find_linear(
    const expr* e, uint32_t hole) {
    const expr::var* variable = std::get_if<expr::var>(&e->content);
    if (variable != nullptr) {
        if (variable->index != hole)
            return std::nullopt;
        return linear{.slope = 1, .offset = 0};
    }
    const std::optional<integer> literal = find_literal_value_.find_value(e);
    if (literal.has_value())
        return linear{.slope = 0, .offset = literal.value()};
    const expr::functor& f = std::get<expr::functor>(e->content);
    const bool binary = f.args.size() == 2 && (f.id == add_symbol_ || f.id == sub_symbol_ || f.id == mul_symbol_);
    if (!binary)
        return std::nullopt;
    const std::optional<linear> left = find_linear(f.args[0], hole);
    if (!left.has_value())
        return std::nullopt;
    const std::optional<linear> right = find_linear(f.args[1], hole);
    if (!right.has_value())
        return std::nullopt;
    const linear& l = left.value();
    const linear& r = right.value();
    if (f.id == add_symbol_)
        return linear{.slope = l.slope + r.slope, .offset = l.offset + r.offset};
    if (f.id == sub_symbol_)
        return linear{.slope = l.slope - r.slope, .offset = l.offset - r.offset};
    if (l.slope != 0 && r.slope != 0)
        return std::nullopt;
    return linear{.slope = l.slope * r.offset + r.slope * l.offset, .offset = l.offset * r.offset};
}

#undef ARITH_HOLE_SOLVER
#undef ARITH_HOLE_SOLVER_TEMPLATE

#endif
