#ifndef ARITH_SAMPLE_CONVERTER_HPP
#define ARITH_SAMPLE_CONVERTER_HPP

#include <vector>
#include "value_objects/arith_sample.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/pw_data_point.hpp"

// Interns each sample's input and output as literals.
template<typename IMakeLiteral>
struct arith_sample_converter {
    arith_sample_converter(IMakeLiteral& make_literal);
    std::vector<pw_data_point> convert(const std::vector<arith_sample>& samples);
private:
    IMakeLiteral& make_literal_;
};

#define ARITH_SAMPLE_CONVERTER_TEMPLATE template<typename IMakeLiteral>
#define ARITH_SAMPLE_CONVERTER          arith_sample_converter<IMakeLiteral>

ARITH_SAMPLE_CONVERTER_TEMPLATE ARITH_SAMPLE_CONVERTER::arith_sample_converter(IMakeLiteral& make_literal)
    : make_literal_(make_literal) {}

ARITH_SAMPLE_CONVERTER_TEMPLATE std::vector<pw_data_point> ARITH_SAMPLE_CONVERTER::convert(
    const std::vector<arith_sample>& samples) {
    std::vector<pw_data_point> points;
    points.reserve(samples.size());
    for (const arith_sample& sample : samples)
        points.push_back(pw_data_point{
            .input = make_literal_.make_literal(integer(sample.input)),
            .output = make_literal_.make_literal(integer(sample.output))});
    return points;
}

#undef ARITH_SAMPLE_CONVERTER
#undef ARITH_SAMPLE_CONVERTER_TEMPLATE

#endif
