#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace mt2mp::subscriber_lifecycle {
class Memory {
public:
    virtual ~Memory() = default;
    virtual bool read(uintptr_t, void*, size_t, std::string&) = 0;
};
struct Budget { size_t records = 0, tokens = 0, text_bytes = 0, links = 0; };
struct Token {
    int32_t type = 0;
    uint32_t bits = 0;
    std::string text;
};
struct Record {
    size_t parent = SIZE_MAX, child_index = 0;
    Token label;
    std::vector<Token> tokens;
};
struct PrivateLink {
    int32_t saved_link_id = 0;
    uintptr_t object = 0;
    bool has_inline_definition = false;
};
struct Graph {
    std::vector<Record> records;
    std::vector<PrivateLink> private_links;
};
// Caller supplies a resource budget, not a gameplay population limit. Failure
// leaves partial diagnostics but cannot authorize a Client append.
bool read_graph(Memory&, uintptr_t record, uintptr_t save_context, const Budget&, Graph&, std::string&);
}
