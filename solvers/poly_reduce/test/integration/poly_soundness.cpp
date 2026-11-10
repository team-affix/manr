#include <optional>
#include <random>
#include <string>
#include <vector>
#include "composition/poly_rules_manifest.hpp"
#include "poly_kernel_fixture.hpp"

// Every rewrite every rule detects, at every position, must succeed when performed and give the same polynomial as
// what it rewrites. Checked over random expressions against the normal form oracle.
namespace {

struct PolySoundnessIntegrationTest : PolyKernelFixture {
    poly_rules_manifest rules{kernel};
    std::mt19937 rng{2026};

    std::string random_text(int depth) {
        const char* leaves[] = {"x", "y", "z", "0", "1", "2", "-1", "3"};
        if (depth == 0 || rng() % 4 == 0)
            return leaves[rng() % 8];
        switch (rng() % 5) {
            case 0: return "(" + random_text(depth - 1) + " + " + random_text(depth - 1) + ")";
            case 1: return "(" + random_text(depth - 1) + " - " + random_text(depth - 1) + ")";
            case 2: return "(" + random_text(depth - 1) + " * " + random_text(depth - 1) + ")";
            case 3: return "(" + random_text(depth - 1) + ")^" + std::to_string(rng() % 4);
            default: return "(" + random_text(depth - 1) + " * " + random_text(depth - 1) + " + 1)";
        }
    }
};

}

TEST_F(PolySoundnessIntegrationTest, EveryProposalPreservesThePolynomial) {
    size_t checked = 0;
    for (int sample = 0; sample < 300; ++sample) {
        const expr* e = parse(random_text(4));
        const std::optional<poly_polynomial> before = kernel.expander_.find_polynomial(e);
        ASSERT_TRUE(before.has_value());
        std::vector<rewrite> out;
        rules.rules_.detect(e, out);
        for (const rewrite& detected : out) {
            const expr* proposal = detected();
            ASSERT_NE(proposal, nullptr) << "a detected rewrite failed on " << print(e);
            ASSERT_EQ(kernel.expander_.find_polynomial(proposal), before) << print(e) << "  ->  " << print(proposal);
            ++checked;
        }
    }
    EXPECT_GT(checked, 1000u);
}
