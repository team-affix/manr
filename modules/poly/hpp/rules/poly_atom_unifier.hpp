#ifndef POLY_ATOM_UNIFIER_HPP
#define POLY_ATOM_UNIFIER_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <utility>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/poly_polynomial.hpp"
#include "value_objects/poly_symbols.hpp"
#include "value_objects/rewrite.hpp"

// Root rule (it binds holes, so it must see every occurrence: apply it through `hole_scope_proposer`): binds the holes
// of an atom (a term outside polynomial algebra, such as `exp(?0, n)` or `prime(n + ?0)`) so that it becomes another
// atom of the expression with the same head and no holes, and proposes the result when that makes the whole smaller.
// Arguments match when equal, when one is a hole (bound to the other), or when their difference is linear in one hole
// with a constant coefficient (`n + ?0` against `n` binds `?0 = 0`). The result is the expression with the bindings,
// each unified atom made the same as its partner, then read as a polynomial over its atoms (each atom stands for one
// of `atom_symbols`, each remaining hole for one of `hole_symbols`), so `exp(2, n) - exp(?0, n)` is 0 with `?0 = 2`.
// This is unification run on the atoms the polynomial rules cannot see into, generic over every domain's terms.
template<typename IFindPolynomial, typename ISubtractPolynomials, typename IFindQuotient, typename IRender,
         typename IMakeFunctor, typename IMakeVar, typename IFindCost>
struct poly_atom_unifier {
    poly_atom_unifier(IFindPolynomial& find_polynomial,
                      ISubtractPolynomials& subtract_polynomials,
                      IFindQuotient& find_quotient,
                      IRender& render,
                      IMakeFunctor& make_functor,
                      IMakeVar& make_var,
                      IFindCost& find_cost,
                      poly_symbols symbols,
                      std::vector<uint32_t> hole_symbols,
                      std::vector<uint32_t> atom_symbols);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    using bindings_t = std::map<uint32_t, const expr*>;

    void collect_atoms(const expr* e, std::vector<const expr*>& atoms) const;
    bool match(const expr* pattern, const expr* target, bindings_t& bindings);
    const expr* substitute(const expr* e, const bindings_t& bindings);
    const expr* replace(const expr* e, const expr* from, const expr* to);
    const expr* fill(const expr* e, const std::map<const expr*, uint32_t>& atoms, bool& known);
    const expr* unfill(const expr* e, const std::map<uint32_t, const expr*>& atoms);
    const expr* normalize(const expr* e);
    static bool holds_hole(const expr* e);

    IFindPolynomial& find_polynomial_;
    ISubtractPolynomials& subtract_polynomials_;
    IFindQuotient& find_quotient_;
    IRender& render_;
    IMakeFunctor& make_functor_;
    IMakeVar& make_var_;
    IFindCost& find_cost_;
    poly_symbols symbols_;
    std::vector<uint32_t> hole_symbols_;
    std::vector<uint32_t> atom_symbols_;
};

#define POLY_ATOM_UNIFIER_TEMPLATE                                                                              \
    template<typename IFindPolynomial, typename ISubtractPolynomials, typename IFindQuotient, typename IRender, \
             typename IMakeFunctor, typename IMakeVar, typename IFindCost>
#define POLY_ATOM_UNIFIER                                                                                 \
    poly_atom_unifier<IFindPolynomial, ISubtractPolynomials, IFindQuotient, IRender, IMakeFunctor, IMakeVar, \
                      IFindCost>

POLY_ATOM_UNIFIER_TEMPLATE POLY_ATOM_UNIFIER::poly_atom_unifier(IFindPolynomial& find_polynomial,
                                                                ISubtractPolynomials& subtract_polynomials,
                                                                IFindQuotient& find_quotient,
                                                                IRender& render,
                                                                IMakeFunctor& make_functor,
                                                                IMakeVar& make_var,
                                                                IFindCost& find_cost,
                                                                poly_symbols symbols,
                                                                std::vector<uint32_t> hole_symbols,
                                                                std::vector<uint32_t> atom_symbols)
    : find_polynomial_(find_polynomial)
    , subtract_polynomials_(subtract_polynomials)
    , find_quotient_(find_quotient)
    , render_(render)
    , make_functor_(make_functor)
    , make_var_(make_var)
    , find_cost_(find_cost)
    , symbols_(symbols)
    , hole_symbols_(std::move(hole_symbols))
    , atom_symbols_(std::move(atom_symbols)) {}

POLY_ATOM_UNIFIER_TEMPLATE void POLY_ATOM_UNIFIER::detect(const expr* e, std::vector<rewrite>& out) {
    if (!holds_hole(e))
        return;
    std::vector<const expr*> atoms;
    collect_atoms(e, atoms);
    if (atoms.size() < 2)
        return;
    const size_t cost = find_cost_.cost(e);
    std::set<const expr*> proposed;
    for (const expr* holed : atoms) {
        if (!holds_hole(holed))
            continue;
        const expr::functor& pattern = std::get<expr::functor>(holed->content);
        for (const expr* ground : atoms) {
            const expr::functor& target = std::get<expr::functor>(ground->content);
            if (holds_hole(ground) || target.id != pattern.id || target.args.size() != pattern.args.size())
                continue;
            bindings_t bindings;
            bool matched = true;
            for (size_t i = 0; i < pattern.args.size() && matched; ++i)
                matched = match(pattern.args[i], target.args[i], bindings);
            if (!matched)
                continue;
            const expr* bound = replace(substitute(e, bindings), substitute(holed, bindings), ground);
            const expr* result = normalize(bound);
            if (result == nullptr || find_cost_.cost(result) >= cost || !proposed.insert(result).second)
                continue;
            out.push_back(ready(result));
        }
    }
}

// The atoms of e: its subterms outside polynomial algebra (functors with arguments other than + - * ^), not entered.
POLY_ATOM_UNIFIER_TEMPLATE void POLY_ATOM_UNIFIER::collect_atoms(const expr* e, std::vector<const expr*>& atoms) const {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->args.empty())
        return;
    const bool algebra = f->id == symbols_.add || f->id == symbols_.sub || f->id == symbols_.mul
                         || f->id == symbols_.pow;
    if (!algebra) {
        if (std::find(atoms.begin(), atoms.end(), e) == atoms.end())
            atoms.push_back(e);
        return;
    }
    for (const expr* arg : f->args)
        collect_atoms(arg, atoms);
}

// Extends the bindings so the pattern means the target: equal, a hole bound to the target, or a difference linear in
// one hole with a constant coefficient, solved exactly.
POLY_ATOM_UNIFIER_TEMPLATE bool POLY_ATOM_UNIFIER::match(const expr* pattern, const expr* target,
                                                         bindings_t& bindings) {
    const expr* bound = substitute(pattern, bindings);
    if (bound == target)
        return true;
    const expr::var* hole = std::get_if<expr::var>(&bound->content);
    if (hole != nullptr) {
        bindings.emplace(hole->index, target);
        return true;
    }
    if (!holds_hole(bound))
        return false;
    bool known = true;
    const expr* filled = fill(bound, {}, known);
    const std::optional<poly_polynomial> left = known ? find_polynomial_.find_polynomial(filled) : std::nullopt;
    const std::optional<poly_polynomial> right = find_polynomial_.find_polynomial(target);
    if (!left.has_value() || !right.has_value())
        return false;
    const std::optional<poly_polynomial> difference = subtract_polynomials_.subtract(left.value(), right.value());
    if (!difference.has_value())
        return false;
    // difference = c1 * h + c0, c1 a constant, h the only hole
    std::optional<size_t> only;
    integer slope = 0;
    poly_polynomial rest;
    for (const auto& [monomial, coefficient] : difference.value().terms) {
        std::optional<size_t> found;
        for (const poly_power& power : monomial.powers)
            for (size_t index = 0; index < hole_symbols_.size(); ++index)
                if (power.variable == hole_symbols_[index])
                    found = index;
        if (!found.has_value()) {
            rest.terms.emplace(monomial, coefficient);
            continue;
        }
        const bool linear = monomial.powers.size() == 1 && monomial.powers[0].exponent == 1;
        if (!linear || (only.has_value() && only.value() != found.value()))
            return false;
        only = found;
        slope = coefficient;
    }
    if (!only.has_value())
        return false;
    poly_polynomial negated;
    for (const auto& [monomial, coefficient] : rest.terms)
        negated.terms.emplace(monomial, integer(-coefficient));
    const std::optional<poly_polynomial> value =
        find_quotient_.find_quotient(negated, poly_polynomial{.terms = {{poly_monomial{}, slope}}});
    if (!value.has_value())
        return false;
    bindings.emplace(static_cast<uint32_t>(only.value()), render_.render(value.value()));
    return true;
}

POLY_ATOM_UNIFIER_TEMPLATE const expr* POLY_ATOM_UNIFIER::substitute(const expr* e, const bindings_t& bindings) {
    const expr::var* hole = std::get_if<expr::var>(&e->content);
    if (hole != nullptr) {
        const auto bound = bindings.find(hole->index);
        return bound == bindings.end() ? e : bound->second;
    }
    const expr::functor& f = std::get<expr::functor>(e->content);
    if (f.args.empty() || bindings.empty())
        return e;
    std::vector<const expr*> args;
    args.reserve(f.args.size());
    for (const expr* arg : f.args)
        args.push_back(substitute(arg, bindings));
    return make_functor_.make_functor(f.id, args);
}

// e with every occurrence of `from` as `to`
POLY_ATOM_UNIFIER_TEMPLATE const expr* POLY_ATOM_UNIFIER::replace(const expr* e, const expr* from, const expr* to) {
    if (e == from)
        return to;
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->args.empty())
        return e;
    std::vector<const expr*> args;
    args.reserve(f->args.size());
    for (const expr* arg : f->args)
        args.push_back(replace(arg, from, to));
    return make_functor_.make_functor(f->id, args);
}

// e with each hole `?k` as the variable `hole_symbols[k]` and each given atom as its variable; `known` false for a hole
// past the hole symbols
POLY_ATOM_UNIFIER_TEMPLATE const expr* POLY_ATOM_UNIFIER::fill(const expr* e,
                                                               const std::map<const expr*, uint32_t>& atoms,
                                                               bool& known) {
    const auto atom = atoms.find(e);
    if (atom != atoms.end())
        return make_functor_.make_functor(atom->second, {});
    const expr::var* hole = std::get_if<expr::var>(&e->content);
    if (hole != nullptr) {
        known = known && hole->index < hole_symbols_.size();
        return known ? make_functor_.make_functor(hole_symbols_[hole->index], {}) : e;
    }
    const expr::functor& f = std::get<expr::functor>(e->content);
    if (f.args.empty())
        return e;
    std::vector<const expr*> args;
    args.reserve(f.args.size());
    for (const expr* arg : f.args)
        args.push_back(fill(arg, atoms, known));
    return make_functor_.make_functor(f.id, args);
}

// the mirror of fill: each atom variable back to its atom, each hole variable back to `?k`
POLY_ATOM_UNIFIER_TEMPLATE const expr* POLY_ATOM_UNIFIER::unfill(const expr* e,
                                                                 const std::map<uint32_t, const expr*>& atoms) {
    const expr::functor& f = std::get<expr::functor>(e->content);
    if (f.args.empty()) {
        const auto atom = atoms.find(f.id);
        if (atom != atoms.end())
            return atom->second;
        for (uint32_t index = 0; index < hole_symbols_.size(); ++index)
            if (f.id == hole_symbols_[index])
                return make_var_.make_var(index);
        return e;
    }
    std::vector<const expr*> args;
    args.reserve(f.args.size());
    for (const expr* arg : f.args)
        args.push_back(unfill(arg, atoms));
    return make_functor_.make_functor(f.id, args);
}

// e read as a polynomial over its atoms and holes and rendered back; nullptr when it is not one, or holds more atoms
// than there are atom symbols
POLY_ATOM_UNIFIER_TEMPLATE const expr* POLY_ATOM_UNIFIER::normalize(const expr* e) {
    std::vector<const expr*> atoms;
    collect_atoms(e, atoms);
    if (atoms.size() > atom_symbols_.size())
        return nullptr;
    std::map<const expr*, uint32_t> to_symbol;
    std::map<uint32_t, const expr*> to_atom;
    for (size_t index = 0; index < atoms.size(); ++index) {
        to_symbol.emplace(atoms[index], atom_symbols_[index]);
        to_atom.emplace(atom_symbols_[index], atoms[index]);
    }
    bool known = true;
    const expr* filled = fill(e, to_symbol, known);
    const std::optional<poly_polynomial> polynomial = known ? find_polynomial_.find_polynomial(filled) : std::nullopt;
    if (!polynomial.has_value())
        return nullptr;
    return unfill(render_.render(polynomial.value()), to_atom);
}

POLY_ATOM_UNIFIER_TEMPLATE bool POLY_ATOM_UNIFIER::holds_hole(const expr* e) {
    if (std::holds_alternative<expr::var>(e->content))
        return true;
    const expr::functor& f = std::get<expr::functor>(e->content);
    for (const expr* arg : f.args)
        if (holds_hole(arg))
            return true;
    return false;
}

#undef POLY_ATOM_UNIFIER
#undef POLY_ATOM_UNIFIER_TEMPLATE

#endif
