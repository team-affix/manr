#ifndef POLY_KERNEL_FIXTURE_HPP
#define POLY_KERNEL_FIXTURE_HPP

#include <sstream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/poly_kernel_manifest.hpp"

// A real kernel plus text helpers, shared by the polynomial integration tests.
struct PolyKernelFixture : ::testing::Test {
    poly_kernel_manifest kernel{256};

    const expr* parse(const std::string& text) { return kernel.parser_.parse(text); }

    std::string print(const expr* e) {
        std::ostringstream os;
        kernel.printer_.print(os, e);
        return os.str();
    }

    poly_polynomial normal_form(const std::string& text) { return kernel.expander_.find_polynomial(parse(text)).value(); }

    std::vector<std::string> print_all(const std::vector<const expr*>& exprs) {
        std::vector<std::string> printed;
        for (const expr* e : exprs)
            printed.push_back(print(e));
        return printed;
    }
};

#endif
