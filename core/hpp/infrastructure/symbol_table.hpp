#ifndef SYMBOL_TABLE_HPP
#define SYMBOL_TABLE_HPP

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Maps functor names to dense functor ids and back. Modules intern the names of the functors they define.
struct symbol_table {
    uint32_t intern_symbol(const std::string& name);
    const std::string& name(uint32_t id) const;
private:
    std::unordered_map<std::string, uint32_t> ids_;
    std::vector<std::string> names_;
};

#endif
