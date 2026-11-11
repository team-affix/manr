#ifndef LIST_IDENTITY_MAP_REMOVER_HPP
#define LIST_IDENTITY_MAP_REMOVER_HPP

#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/list_symbols.hpp"
#include "value_objects/rewrite.hpp"

// Local rule: removes a map of the identity, `map(lam(#0), xs)` to `xs`.
struct list_identity_map_remover {
    list_identity_map_remover(list_symbols symbols);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    list_symbols symbols_;
};

#endif
