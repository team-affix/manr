#ifndef POLY_KERNEL_FIXTURE_HPP
#define POLY_KERNEL_FIXTURE_HPP

#include <sstream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/poly_kernel_manifest.hpp"
#include "value_objects/rewrite.hpp"
#include "value_objects/poly_default_limits.hpp"

// A real kernel plus text helpers, shared by the polynomial integration tests. The default limits, but with a small
// term limit so tests can cross it cheaply.
struct PolyKernelFixture : ::testing::Test {
    static poly_limits make_limits() {
        poly_limits limits = k_poly_default_limits;
        limits.max_terms = 256;
        return limits;
    }

    poly_kernel_manifest kernel{make_limits()};

    const expr* parse(const std::string& text) { return kernel.parser_.parse(text); }

    std::string print(const expr* e) {
        std::ostringstream os;
        kernel.printer_.print(os, e);
        return os.str();
    }

    poly_polynomial normal_form(const std::string& text) { return kernel.expander_.find_polynomial(parse(text)).value(); }

    // Performs every detected rewrite and keeps the results of those that found something.
    std::vector<const expr*> perform(const std::vector<rewrite>& rewrites) {
        std::vector<const expr*> results;
        for (const rewrite& detected : rewrites) {
            const expr* result = detected();
            if (result != nullptr)
                results.push_back(result);
        }
        return results;
    }

    std::vector<std::string> print_all(const std::vector<const expr*>& exprs) {
        std::vector<std::string> printed;
        for (const expr* e : exprs)
            printed.push_back(print(e));
        return printed;
    }
};

#endif
