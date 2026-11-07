#ifndef RATIONAL_HPP
#define RATIONAL_HPP

#include <boost/multiprecision/cpp_int.hpp>

// Exact arbitrary-precision fractions over `integer`, for work that must divide (e.g. solving linear systems) before
// checking that the answer is integral.
using rational = boost::multiprecision::cpp_rational;

#endif
