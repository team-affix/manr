#include "composition/prog_fit_rules_manifest.hpp"

prog_fit_rules_manifest::prog_fit_rules_manifest(kernel_t& kernel, const std::vector<prog_sample>& samples)
    : poly_rules_(kernel)
    , pw_symbols_{.piecewise = kernel.symbols_.intern_symbol("pw"),
                  .branch = kernel.symbols_.intern_symbol("case"),
                  .region = kernel.symbols_.intern_symbol("when")}
    , abs_symbols_{.lambda = kernel.symbols_.intern_symbol("lam"),
                   .lambdas = kernel.symbols_.intern_symbol("lams"),
                   .application = kernel.symbols_.intern_symbol("app"),
                   .binding = kernel.symbols_.intern_symbol("let")}
    , prog_symbols_(intern_symbols(kernel))
    , indices_(kernel.symbols_, kernel.exprs_, k_abs_default_indices)
    , interpreter_(indices_, kernel.literals_, kernel.literals_, kernel.exprs_, prog_symbols_)
    , pw_evaluator_(interpreter_, interpreter_, pw_symbols_)
    , function_maker_(kernel.literals_, kernel.exprs_, abs_symbols_)
    , parser_(kernel.literals_, kernel.exprs_, kernel.symbols_, kernel.exprs_, prog_symbols_)
    , points_(parse_samples(parser_, samples))
    , constants_(kernel.sizes_)
    , programs_(interpreter_, kernel.literals_, kernel.literals_, indices_, kernel.exprs_, prog_symbols_)
    , hypotheses_(constants_, programs_)
    , piecewise_(hypotheses_, pw_evaluator_, function_maker_, kernel.literals_, kernel.exprs_, pw_symbols_,
                 prog_symbols_.point, kernel.poly_symbols_.add, kernel.poly_symbols_.sub, kernel.poly_symbols_.mul,
                 kernel.poly_symbols_.pow)
    , local_rules_(piecewise_.binder_, piecewise_.absorber_, poly_rules_.local_rules_)
    , root_rules_()
    , subterms_(local_rules_, kernel.exprs_)
    , rules_(subterms_, root_rules_)
    , initial_(piecewise_.builder_.build(points_, 0)) {}

prog_symbols prog_fit_rules_manifest::intern_symbols(kernel_t& kernel) {
    symbol_table& s = kernel.symbols_;
    return prog_symbols{.add = kernel.poly_symbols_.add,
                        .sub = kernel.poly_symbols_.sub,
                        .mul = kernel.poly_symbols_.mul,
                        .pow = kernel.poly_symbols_.pow,
                        .mod = s.intern_symbol("mod"),
                        .max = s.intern_symbol("max"),
                        .min = s.intern_symbol("min"),
                        .less = s.intern_symbol("lt"),
                        .less_equal = s.intern_symbol("le"),
                        .equal = s.intern_symbol("eq"),
                        .conjunction = s.intern_symbol("and"),
                        .disjunction = s.intern_symbol("or"),
                        .negation = s.intern_symbol("not"),
                        .piecewise = s.intern_symbol("pw"),
                        .region = s.intern_symbol("when"),
                        .nil = s.intern_symbol("nil"),
                        .cons = s.intern_symbol("cons"),
                        .head = s.intern_symbol("head"),
                        .tail = s.intern_symbol("tail"),
                        .length = s.intern_symbol("length"),
                        .reverse = s.intern_symbol("reverse"),
                        .append = s.intern_symbol("append"),
                        .sort = s.intern_symbol("sort"),
                        .unique = s.intern_symbol("unique"),
                        .member = s.intern_symbol("elem"),
                        .map = s.intern_symbol("map"),
                        .filter = s.intern_symbol("filter"),
                        .fold = s.intern_symbol("fold"),
                        .scan = s.intern_symbol("scan"),
                        .zip_with = s.intern_symbol("zipwith"),
                        .leaf = s.intern_symbol("leaf"),
                        .node = s.intern_symbol("node"),
                        .tree_fold = s.intern_symbol("tfold"),
                        .field = s.intern_symbol("field"),
                        .reachable = s.intern_symbol("reachable"),
                        .lambda = s.intern_symbol("lam"),
                        .lambdas = s.intern_symbol("lams"),
                        .application = s.intern_symbol("app"),
                        .binding = s.intern_symbol("let"),
                        .point = s.intern_symbol("point")};
}

std::vector<pw_data_point> prog_fit_rules_manifest::parse_samples(parser_t& parser,
                                                                  const std::vector<prog_sample>& samples) {
    std::vector<pw_data_point> points;
    points.reserve(samples.size());
    for (const prog_sample& sample : samples)
        points.push_back(pw_data_point{.input = parser.parse(sample.input), .output = parser.parse(sample.output)});
    return points;
}
