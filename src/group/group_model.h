#pragma once

#include "common/window_identity.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace quickky {

struct Tab {
    TabId id;
    WindowIdentity window;
};

class GroupModel {
public:
    std::optional<TabId> Add(const WindowIdentity& window, bool activate);
    bool Remove(const WindowIdentity& window);
    bool Activate(TabId id);
    bool Move(TabId id, std::size_t index);

    const Tab* Find(const WindowIdentity& window) const;
    const std::vector<Tab>& tabs() const noexcept { return tabs_; }
    std::optional<TabId> active() const noexcept { return active_; }

private:
    std::vector<Tab> tabs_;
    std::optional<TabId> active_;
    TabId next_id_ = 1;
};

} // namespace quickky
