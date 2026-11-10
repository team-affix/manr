#ifndef POLY_SAMPLE_CONVERTER_HPP
#define POLY_SAMPLE_CONVERTER_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/poly_sample.hpp"
#include "value_objects/pw_data_point.hpp"

// Interns each sample as a data point: the input as the tuple `point(v1, ..., vk)` of literals, the output as a
// literal. `find_samples` reads them back: the points whose input is such a tuple of `arity` literals and whose output
// is a literal.
template<typename IMakeLiteral, typename IMakeFunctor, typename IFindLiteralValue>
struct poly_sample_converter {
    poly_sample_converter(IMakeLiteral& make_literal,
                          IMakeFunctor& make_functor,
                          IFindLiteralValue& find_literal_value,
                          uint32_t point_symbol,
                          size_t arity);
    std::vector<pw_data_point> convert(const std::vector<poly_sample>& samples);
    std::vector<poly_sample> find_samples(const std::vector<pw_data_point>& points);
private:
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    IFindLiteralValue& find_literal_value_;
    uint32_t point_symbol_;
    size_t arity_;
};

#define POLY_SAMPLE_CONVERTER_TEMPLATE template<typename IMakeLiteral, typename IMakeFunctor, typename IFindLiteralValue>
#define POLY_SAMPLE_CONVERTER          poly_sample_converter<IMakeLiteral, IMakeFunctor, IFindLiteralValue>

POLY_SAMPLE_CONVERTER_TEMPLATE POLY_SAMPLE_CONVERTER::poly_sample_converter(
    IMakeLiteral& make_literal,
    IMakeFunctor& make_functor,
    IFindLiteralValue& find_literal_value,
    uint32_t point_symbol,
    size_t arity)
    : make_literal_(make_literal)
    , make_functor_(make_functor)
    , find_literal_value_(find_literal_value)
    , point_symbol_(point_symbol)
    , arity_(arity) {}

POLY_SAMPLE_CONVERTER_TEMPLATE std::vector<pw_data_point> POLY_SAMPLE_CONVERTER::convert(
    const std::vector<poly_sample>& samples) {
    std::vector<pw_data_point> points;
    points.reserve(samples.size());
    for (const poly_sample& sample : samples) {
        std::vector<const expr*> coordinates;
        coordinates.reserve(sample.inputs.size());
        for (const integer& input : sample.inputs)
            coordinates.push_back(make_literal_.make_literal(input));
        points.push_back(pw_data_point{
            .input = make_functor_.make_functor(point_symbol_, coordinates),
            .output = make_literal_.make_literal(sample.output)});
    }
    return points;
}

POLY_SAMPLE_CONVERTER_TEMPLATE std::vector<poly_sample> POLY_SAMPLE_CONVERTER::find_samples(
    const std::vector<pw_data_point>& points) {
    std::vector<poly_sample> samples;
    samples.reserve(points.size());
    for (const pw_data_point& point : points) {
        const expr::functor* tuple = std::get_if<expr::functor>(&point.input->content);
        if (tuple == nullptr || tuple->id != point_symbol_ || tuple->args.size() != arity_)
            continue;
        const std::optional<integer> output = find_literal_value_.find_value(point.output);
        if (!output.has_value())
            continue;
        poly_sample sample{.inputs = {}, .output = output.value()};
        for (const expr* coordinate : tuple->args) {
            const std::optional<integer> value = find_literal_value_.find_value(coordinate);
            if (!value.has_value())
                break;
            sample.inputs.push_back(value.value());
        }
        if (sample.inputs.size() != arity_)
            continue;
        samples.push_back(std::move(sample));
    }
    return samples;
}

#undef POLY_SAMPLE_CONVERTER
#undef POLY_SAMPLE_CONVERTER_TEMPLATE

#endif
