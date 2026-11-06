#pragma once

#include "common/window_identity.h"
#include "group/group_model.h"

#include <optional>
#include <unordered_map>

namespace quickky {

enum class CaptureState { Discovered, Attaching, Managed, Restoring };

struct AttachmentRequest {
    WindowIdentity window;
    std::uint64_t token;
};

struct RestoreRequest {
    WindowIdentity window;
    std::uint64_t token;
};

struct CaptureRecord {
    WindowIdentity window;
    CaptureState state = CaptureState::Discovered;
    std::uint64_t pending_request = 0;
};

class CaptureCoordinator {
public:
    explicit CaptureCoordinator(GroupModel& group) : group_(group) {}

    std::optional<WindowIdentity> Observe(const WindowCandidate& window);
    bool Forget(const WindowIdentity& window);
    std::optional<AttachmentRequest> BeginAttach(const WindowIdentity& window);
    bool CompleteAttach(const AttachmentRequest& request, bool succeeded, bool activate);
    std::optional<RestoreRequest> BeginRestore(const WindowIdentity& window);
    bool CompleteRestore(const RestoreRequest& request, bool succeeded);
    bool OnForeground(const WindowIdentity& window);

    const CaptureRecord* Find(NativeWindow handle) const;
    std::size_t size() const noexcept { return records_.size(); }

private:
    CaptureRecord* FindCurrent(const WindowIdentity& window);

    GroupModel& group_;
    std::unordered_map<NativeWindow, CaptureRecord> records_;
    std::uint64_t next_generation_ = 1;
    std::uint64_t next_request_ = 1;
};

} // namespace quickky
