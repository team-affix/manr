#ifndef LIST_SAMPLE_CONVERTER_HPP
#define LIST_SAMPLE_CONVERTER_HPP

#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/list_sample.hpp"
#include "value_objects/pw_data_point.hpp"

// Interns each sample as a data point: the input as a list of literals, the output as a list of literals or a
// literal.
template<typename IMakeList, typename IMakeLiteral>
struct list_sample_converter {
    list_sample_converter(IMakeList& make_list, IMakeLiteral& make_literal);
    std::vector<pw_data_point> convert(const std::vector<list_sample>& samples);
private:
    const expr* make_literal_list(const std::vector<integer>& values);

    IMakeList& make_list_;
    IMakeLiteral& make_literal_;
};

#define LIST_SAMPLE_CONVERTER_TEMPLATE template<typename IMakeList, typename IMakeLiteral>
#define LIST_SAMPLE_CONVERTER          list_sample_converter<IMakeList, IMakeLiteral>

LIST_SAMPLE_CONVERTER_TEMPLATE LIST_SAMPLE_CONVERTER::list_sample_converter(IMakeList& make_list,
                                                                            IMakeLiteral& make_literal)
    : make_list_(make_list)
    , make_literal_(make_literal) {}

LIST_SAMPLE_CONVERTER_TEMPLATE std::vector<pw_data_point> LIST_SAMPLE_CONVERTER::convert(
    const std::vector<list_sample>& samples) {
    std::vector<pw_data_point> points;
    points.reserve(samples.size());
    for (const list_sample& sample : samples) {
        const expr* output =
            sample.output_is_list ? make_literal_list(sample.output) : make_literal_.make_literal(sample.output[0]);
        points.push_back(pw_data_point{.input = make_literal_list(sample.input), .output = output});
    }
    return points;
}

LIST_SAMPLE_CONVERTER_TEMPLATE const expr* LIST_SAMPLE_CONVERTER::make_literal_list(const std::vector<integer>& values) {
    std::vector<const expr*> elements;
    elements.reserve(values.size());
    for (const integer& value : values)
        elements.push_back(make_literal_.make_literal(value));
    return make_list_.make_list(elements);
}

#undef LIST_SAMPLE_CONVERTER
#undef LIST_SAMPLE_CONVERTER_TEMPLATE

#endif
