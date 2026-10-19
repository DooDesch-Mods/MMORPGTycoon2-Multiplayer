#include "record_graph.h"
#include <set>
#include <utility>

namespace mt2mp::subscriber_lifecycle {
namespace {
bool fail(std::string& e, const char* cause) { e = cause; return false; }
template<class T> bool at(Memory& m, uintptr_t p, size_t offset, T& out, std::string& e) {
    if (!p || p > UINTPTR_MAX - offset || p + offset > UINTPTR_MAX - sizeof(T))
        return fail(e, "native record address overflow/null");
    return m.read(p + offset, &out, sizeof out, e);
}
bool token(Memory& m, uintptr_t p, const Budget& budget, size_t& bytes, Token& out, std::string& e) {
    if (!at(m, p, 0, out.type, e)) return false;
    if (out.type == 0) return true;
    if (out.type == 3 || out.type == 4) return at(m, p, 0x28, out.bits, e);
    if (out.type != 1 && out.type != 2) return fail(e, "native record token is not a payload scalar");
    uintptr_t data = 0; uint64_t size = 0;
    if (!at(m, p, 8, data, e) || !at(m, p, 0x10, size, e)) return false;
    if (!data || size > budget.text_bytes || bytes > budget.text_bytes - size || data > UINTPTR_MAX - size)
        return fail(e, "native record text exceeds supplied resource budget/address extent");
    out.text.resize(static_cast<size_t>(size));
    if (size && !m.read(data, out.text.data(), out.text.size(), e)) return false;
    char end = 1;
    if (!at(m, data, static_cast<size_t>(size), end, e)) return false;
    if (end) return fail(e, "native record GNU string terminator invalid");
    bytes += static_cast<size_t>(size); return true;
}
bool array(Memory& m, uintptr_t record, size_t offset, uintptr_t& data, int32_t& count, std::string& e) {
    int32_t capacity = -1;
    if (!at(m, record, offset + 8, data, e) || !at(m, record, offset + 0x10, count, e) ||
        !at(m, record, offset + 0x14, capacity, e)) return false;
    if (count < 0 || capacity < count || (count && !data)) return fail(e, "native record array count/capacity invalid");
    return true;
}
bool tree(Memory& m, uintptr_t root, const Budget& budget, std::vector<Record>& out,
          std::set<int32_t>& definitions, std::string& e) {
    out.clear(); definitions.clear();
    struct Pending { uintptr_t object; size_t parent, index; };
    std::vector<Pending> queue{{root, SIZE_MAX, 0}};
    std::set<uintptr_t> visited;
    size_t tokens = 0, text_bytes = 0;
    for (size_t i = 0; i < queue.size(); ++i) {
        if (queue.size() > budget.records) return fail(e, "native record graph exceeds supplied record budget");
        const auto next = queue[i];
        if (!visited.insert(next.object).second) return fail(e, "native record tree repeats a child/cycle");
        uint8_t stream = 1; uintptr_t data = 0, children = 0; int32_t count = -1, child_count = -1;
        if (!at(m, next.object, 0x70, stream, e)) return false;
        if (stream) return fail(e, "capture requires owned non-streaming native record");
        Record row; row.parent = next.parent; row.child_index = next.index;
        if (!token(m, next.object, budget, text_bytes, row.label, e) ||
            !array(m, next.object, 0x30, data, count, e) || !array(m, next.object, 0x48, children, child_count, e)) return false;
        if (row.label.type != 0 && row.label.type != 1 && row.label.type != 2)
            return fail(e, "native record label type invalid");
        if (static_cast<size_t>(count) > budget.tokens || tokens > budget.tokens - static_cast<size_t>(count))
            return fail(e, "native record graph exceeds supplied token budget");
        for (int32_t n = 0; n < count; ++n) {
            const size_t offset = static_cast<size_t>(n) * 0x30;
            if (data > UINTPTR_MAX - offset) return fail(e, "native record token offset overflow");
            Token item; if (!token(m, data + offset, budget, text_bytes, item, e)) return false;
            row.tokens.push_back(std::move(item));
        }
        tokens += static_cast<size_t>(count);
        if (row.label.text == "linkId") {
            if (row.tokens.size() != 1 || row.tokens[0].type != 4 || row.tokens[0].bits == 0 || row.tokens[0].bits > INT32_MAX ||
                !definitions.insert(static_cast<int32_t>(row.tokens[0].bits)).second)
                return fail(e, "native record linkId definition invalid/duplicate");
        }
        if (static_cast<size_t>(child_count) > budget.records || queue.size() > budget.records - static_cast<size_t>(child_count))
            return fail(e, "native record children exceed supplied record budget");
        out.push_back(std::move(row));
        for (int32_t n = 0; n < child_count; ++n) {
            uintptr_t child = 0;
            if (!at(m, children, static_cast<size_t>(n) * 8, child, e)) return false;
            queue.push_back({child, i, static_cast<size_t>(n)});
        }
    }
    return true;
}
}
bool read_owned_record(Memory& m, uintptr_t root, const Budget& budget, std::vector<Record>& out, std::string& e) {
    std::set<int32_t> definitions;
    if (!tree(m, root, budget, out, definitions, e)) return false;
    e.clear(); return true;
}
bool read_graph(Memory& m, uintptr_t root, uintptr_t context, const Budget& budget, Graph& out, std::string& e) {
    out = {};
    std::set<int32_t> definitions;
    if (!tree(m, root, budget, out.records, definitions, e)) return false;
    uintptr_t table = 0, node = 0; uint64_t count = 0;
    if (!at(m, context, 0, table, e) || !at(m, table, 0x10, node, e) || !at(m, table, 0x18, count, e)) return false;
    if (count > budget.links) return fail(e, "native SaveContext exceeds supplied link budget");
    std::set<int32_t> ids; std::set<uintptr_t> nodes, objects;
    for (uint64_t n = 0; n < count; ++n) {
        PrivateLink link;
        if (!node || !nodes.insert(node).second) return fail(e, "native SaveContext chain shorter than count/cyclic");
        if (!at(m, node, 8, link.object, e) || !at(m, node, 0x10, link.saved_link_id, e)) return false;
        if (!link.object || link.saved_link_id <= 0 || !ids.insert(link.saved_link_id).second || !objects.insert(link.object).second)
            return fail(e, "native SaveContext duplicate/missing object or linkID");
        link.has_inline_definition = definitions.contains(link.saved_link_id);
        out.private_links.push_back(link);
        if (!at(m, node, 0, node, e)) return false;
    }
    if (node) return fail(e, "native SaveContext chain exceeds declared count");
    for (auto id : definitions) if (!ids.contains(id)) return fail(e, "native inline linkId absent from its SaveContext");
    e.clear(); return true;
}
}
