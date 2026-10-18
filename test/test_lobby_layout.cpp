#include "lobby_layout.h"

#include <array>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string_view>

namespace {
using mt2mp::game::lobby_ui::Widget;
using Layout = std::map<std::string, Widget>;

void require(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error(reason);
}

bool interactive(const Widget& widget) {
    return widget.type == "button" || widget.type == "input";
}

bool intersects(const Widget& a, const Widget& b) {
    return a.x < b.x + b.width && b.x < a.x + a.width &&
           a.y < b.y + b.height && b.y < a.y + a.height;
}

bool contains(const Widget& parent, const Widget& child) {
    return parent.x <= child.x && parent.y <= child.y &&
           child.x + child.width <= parent.x + parent.width &&
           child.y + child.height <= parent.y + parent.height;
}

void check_native_contract(const Layout& layout) {
    require(!layout.empty() && layout.size() <= 80, "Native declaration exceeds the 80-widget budget");
    const std::set<std::string> types{"text", "button", "input", "image", "panel"};
    const std::set<std::string> styles{"button", "tab", "row", "quiet"};
    for (const auto& [id, widget] : layout) {
        require(id == widget.id && !id.empty() && id.size() <= 48,
                "Native widget identity is invalid: " + id);
        require(id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") == std::string::npos,
                "Native widget identity contains an unsupported character: " + id);
        require(id.size() < 6 || id.substr(id.size() - 6) != "_label",
                "Native widget identity uses the reserved label suffix: " + id);
        require(types.count(widget.type) != 0, "Unsupported native type: " + id);
        require(widget.width > 0 && widget.height > 0 && widget.x >= 0 && widget.y >= 0 &&
                    widget.width <= 820 && widget.height <= 466 &&
                    widget.x <= 820 - widget.width && widget.y <= 466 - widget.height,
                "Native widget leaves the 820 x 466 content canvas: " + id);
        require(widget.size >= 10 && widget.size <= 30, "Native text size is unsupported: " + id);
        require(widget.text.size() <= 1024, "Native widget text exceeds its byte budget: " + id);
        if (widget.type == "button")
            require(styles.count(widget.style) != 0, "Unsupported native button style: " + id);
        if (widget.type == "input")
            require(widget.max_length >= 1 && widget.max_length <= 1024,
                    "Native input byte limit is unsupported: " + id);
    }
    for (const auto& [id, limit] : std::array<std::pair<const char*, int>, 3>{{
             {"chat_input", 240}, {"report_comment", 1000}, {"lobby_id", 20}}}) {
        const auto& input = layout.at(id);
        require(input.type == "input" && input.max_length == limit,
                std::string("Domain/native input byte limits differ: ") + id);
    }
}

struct View {
    std::string name;
    std::set<std::string> groups;
    std::set<std::string> excluded;
};

void check_visible_interactions(const Layout& layout) {
    // Shell Leave is meaningful only for an admitted session. Find has its own Back.
    const std::array<View, 5> views{{
        {"entry", {"entry", "shell"}, {"leave"}},
        {"session", {"session", "party", "shell"}, {}},
        {"world picker", {"browser", "picker", "party", "shell"}, {}},
        {"public discovery", {"browser", "find", "shell"}, {"leave"}},
        {"report", {"report"}, {}},
    }};
    std::set<std::string> exposed;
    for (const auto& view : views) {
        std::vector<const Widget*> controls;
        std::vector<const Widget*> panels;
        for (const auto& [id, widget] : layout) {
            if (!widget.visible || view.groups.count(widget.group) == 0 || view.excluded.count(id)) continue;
            require(widget.group != "legacy", "Legacy Start is exposed in " + view.name);
            if (interactive(widget)) { controls.push_back(&widget); exposed.insert(id); }
            if (widget.type == "panel") panels.push_back(&widget);
        }
        for (size_t i = 0; i < controls.size(); ++i) {
            for (size_t j = i + 1; j < controls.size(); ++j)
                require(!intersects(*controls[i], *controls[j]),
                        view.name + " has overlapping interactive controls: " +
                        controls[i]->id + " / " + controls[j]->id);
            // A background may contain a control; partial intersections can hide a hit region.
            // Rendering order and actual hit dispatch still require native acceptance.
            for (const auto* panel : panels)
                require(!intersects(*panel, *controls[i]) || contains(*panel, *controls[i]),
                        view.name + " has a panel cutting through an interactive control: " +
                        panel->id + " / " + controls[i]->id);
        }
    }
    require(layout.at("start").group == "legacy" && exposed.count("start") == 0,
            "The redundant legacy Start action is visible");
    require(layout.at("new_world").group == "picker" && exposed.count("new_world") &&
                layout.at("new_world").text == "Neue Welt erstellen",
            "The native New Game action is not confined to the Host world picker");
    require(mt2mp::game::lobby_ui::page_size == 2, "Compact native list does not use two rows per page");
    for (size_t row = 0; row < mt2mp::game::lobby_ui::page_size; ++row) {
        const auto suffix = std::to_string(row);
        const auto& load = layout.at("list" + suffix);
        const auto& join = layout.at("find" + suffix);
        require(load.type == "button" && load.group == "picker" &&
                    load.text == "Gemeinsam laden" && !load.disabled && exposed.count(load.id),
                "Saved-world row lost its direct shared-load action: " + load.id);
        require(join.type == "button" && join.group == "find" &&
                    join.text == "Beitreten" && !join.disabled && exposed.count(join.id),
                "Public row lost its distinct direct-join action: " + join.id);
        require(load.id != join.id, "Saved-world and public join actions share an identity");
    }
}

void check_roster_order() {
    using mt2mp::game::lobby_ui::ordered_members;
    std::vector<uint64_t> members{7, 19, 42, 88};
    const std::vector<uint64_t> expected{42, 7, 19, 88};
    do {
        require(ordered_members(members, 42) == expected,
                "Roster slot order changes when Steam membership enumeration changes");
    } while (std::next_permutation(members.begin(), members.end()));
    require(ordered_members({88, 7, 19}, 42) == std::vector<uint64_t>({7, 19, 88}),
            "Roster invented an absent Host or lost a real member");
    require(ordered_members({}, 42).empty(), "An empty roster invented a Host");
    require(ordered_members({42}, 42) == std::vector<uint64_t>({42}), "Single Host roster changed");
    require(ordered_members({88, 7}, 0) == std::vector<uint64_t>({7, 88}),
            "An unknown Host changed deterministic member ordering");
}
}

int main() {
    try {
        const auto layout = mt2mp::game::lobby_ui::make_layout();
        check_native_contract(layout);
        check_visible_interactions(layout);
        check_roster_order();
        std::cout << "PASS native layout bounds, byte limits, visible hit regions, direct actions and stable roster\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
