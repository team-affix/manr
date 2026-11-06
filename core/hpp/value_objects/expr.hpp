#ifndef EXPR_HPP
#define EXPR_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <variant>

struct expr {
    struct functor {
        uint32_t id;
        std::vector<const expr*> args;
        auto operator<=>(const functor&) const = default;
    };
    struct var  { uint32_t index; auto operator<=>(const var&) const = default; };
    std::variant<functor, var> content;
    auto operator<=>(const expr&) const = default;
};

#endif
