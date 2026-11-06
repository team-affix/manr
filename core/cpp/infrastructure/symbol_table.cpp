#include "infrastructure/symbol_table.hpp"
#include "debug_assert.hpp"

uint32_t symbol_table::intern_symbol(const std::string& name) {
    const auto found = ids_.find(name);
    if (found != ids_.end())
        return found->second;
    const uint32_t id = static_cast<uint32_t>(names_.size());
    names_.push_back(name);
    ids_.emplace(name, id);
    return id;
}

const std::string& symbol_table::name(uint32_t id) const {
    DEBUG_ASSERT(id < names_.size());
    return names_[id];
}
