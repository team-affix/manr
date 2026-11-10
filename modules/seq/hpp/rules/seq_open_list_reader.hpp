#ifndef SEQ_OPEN_LIST_READER_HPP
#define SEQ_OPEN_LIST_READER_HPP

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/pw_symbols.hpp"
#include "value_objects/rewrite.hpp"

constexpr size_t k_seq_default_min_open_terms = 3;

// Detector: an open-ended list of integers, `cons(a0, cons(a1, ..., cons(a(k-1), ?n)))`, whose tail is a free
// variable, is the infinite list of a sequence's terms that begins with them: `tabulate(pw(case(point(0), a0), ...,
// case(point(k-1), a(k-1)), ?n))`, data over the index whose catch-all is the hole, so fitting the sequence binds it.
// The two are one list under a binding (the tail is the sequence from index k on, for any sequence the catch-all
// becomes), so this reads the whole expression and is applied at the root. A term may hold holes (an unknown term
// `?1`), which become labels holding holes that fitting binds; at least `min_terms` terms are literals.
template<typename IFindLiteralValue, typename IMakeLiteral, typename IMakeFunctor>
struct seq_open_list_reader {
    seq_open_list_reader(IFindLiteralValue& find_literal_value,
                         IMakeLiteral& make_literal,
                         IMakeFunctor& make_functor,
                         uint32_t cons_symbol,
                         uint32_t point_symbol,
                         uint32_t tabulation_symbol,
                         pw_symbols symbols,
                         size_t min_terms = k_seq_default_min_open_terms);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    static bool holds_hole(const expr* e);

    IFindLiteralValue& find_literal_value_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    uint32_t cons_symbol_;
    uint32_t point_symbol_;
    uint32_t tabulation_symbol_;
    pw_symbols symbols_;
    size_t min_terms_;
};

#define SEQ_OPEN_LIST_READER_TEMPLATE template<typename IFindLiteralValue, typename IMakeLiteral, typename IMakeFunctor>
#define SEQ_OPEN_LIST_READER          seq_open_list_reader<IFindLiteralValue, IMakeLiteral, IMakeFunctor>

SEQ_OPEN_LIST_READER_TEMPLATE SEQ_OPEN_LIST_READER::seq_open_list_reader(IFindLiteralValue& find_literal_value,
                                                                         IMakeLiteral& make_literal,
                                                                         IMakeFunctor& make_functor,
                                                                         uint32_t cons_symbol,
                                                                         uint32_t point_symbol,
                                                                         uint32_t tabulation_symbol,
                                                                         pw_symbols symbols,
                                                                         size_t min_terms)
    : find_literal_value_(find_literal_value)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , cons_symbol_(cons_symbol)
    , point_symbol_(point_symbol)
    , tabulation_symbol_(tabulation_symbol)
    , symbols_(symbols)
    , min_terms_(min_terms) {}

SEQ_OPEN_LIST_READER_TEMPLATE void SEQ_OPEN_LIST_READER::detect(const expr* e, std::vector<rewrite>& out) {
    std::vector<const expr*> terms;
    size_t known = 0;
    const expr* rest = e;
    while (true) {
        const expr::functor* f = std::get_if<expr::functor>(&rest->content);
        if (f == nullptr || f->id != cons_symbol_ || f->args.size() != 2)
            break;
        const bool literal = find_literal_value_.find_value(f->args[0]).has_value();
        if (!literal && !holds_hole(f->args[0]))
            return;
        known += literal ? 1 : 0;
        terms.push_back(f->args[0]);
        rest = f->args[1];
    }
    const bool open = std::holds_alternative<expr::var>(rest->content);
    if (!open || known < min_terms_)
        return;
    out.push_back([this, terms, rest]() {
        std::vector<const expr*> branches;
        for (size_t index = 0; index < terms.size(); ++index) {
            const expr* key = make_functor_.make_functor(point_symbol_, {make_literal_.make_literal(integer(index))});
            branches.push_back(make_functor_.make_functor(symbols_.branch, {key, terms[index]}));
        }
        branches.push_back(rest);
        const expr* data = make_functor_.make_functor(symbols_.piecewise, branches);
        return make_functor_.make_functor(tabulation_symbol_, {data});
    });
}

SEQ_OPEN_LIST_READER_TEMPLATE bool SEQ_OPEN_LIST_READER::holds_hole(const expr* e) {
    if (std::holds_alternative<expr::var>(e->content))
        return true;
    for (const expr* arg : std::get<expr::functor>(e->content).args)
        if (holds_hole(arg))
            return true;
    return false;
}

#undef SEQ_OPEN_LIST_READER
#undef SEQ_OPEN_LIST_READER_TEMPLATE

#endif
