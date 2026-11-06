#ifndef INTEGER_HPP
#define INTEGER_HPP

#include <boost/multiprecision/cpp_int.hpp>

// The arbitrary-precision integer behind every literal and coefficient, so integer arithmetic is exact and never
// overflows. Boost.Multiprecision's `cpp_int`, header-only and standalone (submodules `multiprecision`, `boost_config`);
// small values are stored inline with no allocation.
using integer = boost::multiprecision::cpp_int;

#endif
