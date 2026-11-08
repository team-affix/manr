#include <sstream>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "infrastructure/list_fit_manifest.hpp"
#include "infrastructure/list_fold_hypotheses.hpp"
#include "infrastructure/list_map_hypotheses.hpp"
#include "value_objects/poly_default_limits.hpp"
#include "value_objects/poly_fit_default_limits.hpp"
#include "list_fit_fixture.hpp"

using ::testing::_;
using ::testing::Contains;
using ::testing::Invoke;
using ::testing::NiceMock;

namespace {

struct MockProposeHypotheses {
    MOCK_METHOD(void, propose_hypotheses, ((const std::vector<pw_data_point>&), (std::vector<const expr*>&)));
};

using rules_t = list_fit_rules_manifest;
using poly_maps_t = list_map_hypotheses<rules_t::values_t, rules_t::fit_t::hypotheses_t, expr_pool>;
using poly_folds_t = list_fold_hypotheses<rules_t::fit_t::hypotheses_t, expr_pool>;
using mock_maps_t = list_map_hypotheses<rules_t::values_t, MockProposeHypotheses, expr_pool>;
using mock_folds_t = list_fold_hypotheses<MockProposeHypotheses, expr_pool>;

struct ListGenericDetectorsIntegrationTest : ::testing::Test {
    list_fit_manifest manifest{{}, k_poly_default_limits, k_poly_fit_default_limits, 1};
    poly_kernel_manifest& kernel = manifest.kernel_;
    rules_t& rules = manifest.rule_set_;
    NiceMock<MockProposeHypotheses> element_domain;
    std::vector<const expr*> out{};

    const expr* literal(int value) { return kernel.literals_.make_literal(value); }
    const expr* list(const std::vector<const expr*>& elements) { return rules.values_.make_list(elements); }
    const expr* point(const std::vector<const expr*>& coordinates) {
        return kernel.exprs_.make_functor(rules.point_symbol_, coordinates);
    }
    std::vector<std::string> printed() {
        std::vector<std::string> texts;
        for (const expr* e : out) {
            std::ostringstream os;
            kernel.printer_.print(os, e);
            texts.push_back(os.str());
        }
        return texts;
    }
};

}

TEST_F(ListGenericDetectorsIntegrationTest, TheMapDetectorFindsPolynomialBodiesOnIntegerData) {
    poly_maps_t maps{rules.values_, rules.element_fit_.hypotheses_, kernel.exprs_, rules.list_symbols_,
                     rules.point_symbol_};
    maps.propose_hypotheses(rules.converter_.convert(sample_listed(all_lists(-2, 2, 2), [](const std::vector<integer>& l) {
                                std::vector<integer> result;
                                for (const integer& value : l)
                                    result.push_back(3 * value - 1);
                                return result;
                            })),
                            out);
    EXPECT_THAT(printed(), Contains("map(3 * e - 1, xs)"));
}

TEST_F(ListGenericDetectorsIntegrationTest, TheFoldDetectorFindsPolynomialStepsOnIntegerData) {
    poly_folds_t folds{rules.step_fit_.hypotheses_, kernel.exprs_, rules.list_symbols_, rules.point_symbol_};
    folds.propose_hypotheses(rules.converter_.convert(sample_scalar(all_lists(0, 3, 2), [](const std::vector<integer>& l) {
                                 integer total = 0;
                                 for (const integer& value : l)
                                     total += value * value;
                                 return total;
                             })),
                             out);
    EXPECT_THAT(printed(), Contains("fold(h^2 + acc, 0, xs)"));
}

TEST_F(ListGenericDetectorsIntegrationTest, TheMapDetectorHandsElementsOfAnyDomainToTheElementDomain) {
    // elements are themselves lists
    const expr* one = list({literal(1)});
    const expr* two = list({literal(2), literal(2)});
    const expr* body = kernel.exprs_.make_functor(rules.list_symbols_.element, {});
    std::vector<pw_data_point> handed;
    EXPECT_CALL(element_domain, propose_hypotheses(_, _))
        .WillOnce(Invoke([&](const std::vector<pw_data_point>& points, std::vector<const expr*>& bodies) {
            handed = points;
            bodies.push_back(body);
        }));
    mock_maps_t maps{rules.values_, element_domain, kernel.exprs_, rules.list_symbols_, rules.point_symbol_};
    maps.propose_hypotheses({{.input = list({one, two, one}), .output = list({two, one, two})}}, out);
    EXPECT_EQ(handed, (std::vector<pw_data_point>{{.input = point({one}), .output = two},
                                                  {.input = point({two}), .output = one}}));
    EXPECT_EQ(printed(), std::vector<std::string>{"map(e, xs)"});
}

TEST_F(ListGenericDetectorsIntegrationTest, TheFoldDetectorHandsResultsOfAnyDomainToTheStepDomain) {
    // reversal: the results are lists, so the accumulator is a list
    const expr* nil = list({});
    const expr* reversed = list({literal(2), literal(1)});
    const expr* single = list({literal(2)});
    std::vector<pw_data_point> handed;
    EXPECT_CALL(element_domain, propose_hypotheses(_, _))
        .WillOnce(Invoke([&](const std::vector<pw_data_point>& points, std::vector<const expr*>&) { handed = points; }));
    mock_folds_t folds{element_domain, kernel.exprs_, rules.list_symbols_, rules.point_symbol_};
    folds.propose_hypotheses({{.input = nil, .output = nil},
                              {.input = list({literal(2)}), .output = single},
                              {.input = list({literal(1), literal(2)}), .output = reversed}},
                             out);
    EXPECT_EQ(handed, (std::vector<pw_data_point>{{.input = point({literal(2), nil}), .output = single},
                                                  {.input = point({literal(1), single}), .output = reversed}}));
}
