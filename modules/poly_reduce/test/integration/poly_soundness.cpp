#include <optional>
#include <random>
#include <string>
#include <vector>
#include "infrastructure/poly_rules_manifest.hpp"
#include "poly_kernel_fixture.hpp"

// Every proposal of every rule, at every position, must be the same polynomial as what it rewrites. Checked over
// random expressions against the normal form oracle.
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
        std::vector<const expr*> out;
        rules.rules_.propose(e, out);
        for (const expr* proposal : out) {
            ASSERT_EQ(kernel.expander_.find_polynomial(proposal), before) << print(e) << "  ->  " << print(proposal);
            ++checked;
        }
    }
    EXPECT_GT(checked, 1000u);
}
