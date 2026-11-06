#include "group/group_model.h"

#include <algorithm>

namespace quickky {

std::optional<TabId> GroupModel::Add(const WindowIdentity& window, bool activate)
{
    if (!window.valid()) {
        return std::nullopt;
    }
    if (const auto existing = Find(window)) {
        return existing->id;
    }
    if (std::any_of(tabs_.begin(), tabs_.end(), [&](const Tab& tab) {
        return tab.window.window.handle == window.window.handle;
    })) {
        return std::nullopt;
    }

    const auto id = next_id_++;
    tabs_.push_back({id, window});
    if (activate || !active_) {
        active_ = id;
    }
    return id;
}

bool GroupModel::Remove(const WindowIdentity& window)
{
    const auto found = std::find_if(tabs_.begin(), tabs_.end(), [&](const Tab& tab) {
        return tab.window == window;
    });
    if (found == tabs_.end()) {
        return false;
    }

    const auto index = static_cast<std::size_t>(found - tabs_.begin());
    const bool was_active = active_ == found->id;
    tabs_.erase(found);
    if (tabs_.empty()) {
        active_.reset();
    } else if (was_active) {
        active_ = tabs_[std::min(index, tabs_.size() - 1)].id;
    }
    return true;
}

bool GroupModel::Activate(TabId id)
{
    if (std::none_of(tabs_.begin(), tabs_.end(), [id](const Tab& tab) { return tab.id == id; })) {
        return false;
    }
    active_ = id;
    return true;
}

bool GroupModel::Move(TabId id, std::size_t index)
{
    if (index >= tabs_.size()) {
        return false;
    }
    const auto found = std::find_if(tabs_.begin(), tabs_.end(), [id](const Tab& tab) {
        return tab.id == id;
    });
    if (found == tabs_.end()) {
        return false;
    }
    const auto old_index = static_cast<std::size_t>(found - tabs_.begin());
    if (old_index < index) {
        std::rotate(found, found + 1, tabs_.begin() + index + 1);
    } else if (old_index > index) {
        std::rotate(tabs_.begin() + index, found, found + 1);
    }
    return true;
}

const Tab* GroupModel::Find(const WindowIdentity& window) const
{
    const auto found = std::find_if(tabs_.begin(), tabs_.end(), [&](const Tab& tab) {
        return tab.window == window;
    });
    return found == tabs_.end() ? nullptr : &*found;
}

} // namespace quickky
