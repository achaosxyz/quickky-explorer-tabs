#include "capture/capture_coordinator.h"

namespace quickky {

std::optional<WindowIdentity> CaptureCoordinator::Observe(const WindowCandidate& window)
{
    if (!window.valid()) {
        return std::nullopt;
    }
    if (const auto existing = Find(window.handle)) {
        if (existing->window.window == window) {
            return existing->window;
        }
        const auto previous = existing->window;
        Forget(previous);
    }

    const WindowIdentity identity{window, next_generation_++};
    records_.emplace(window.handle, CaptureRecord{identity});
    return identity;
}

bool CaptureCoordinator::Forget(const WindowIdentity& window)
{
    if (FindCurrent(window) == nullptr) {
        return false;
    }
    group_.Remove(window);
    records_.erase(window.window.handle);
    return true;
}

std::optional<AttachmentRequest> CaptureCoordinator::BeginAttach(const WindowIdentity& window)
{
    auto record = FindCurrent(window);
    if (record == nullptr || record->state != CaptureState::Discovered) {
        return std::nullopt;
    }
    record->state = CaptureState::Attaching;
    record->pending_request = next_request_++;
    return AttachmentRequest{window, record->pending_request};
}

bool CaptureCoordinator::CompleteAttach(
    const AttachmentRequest& request, bool succeeded, bool activate)
{
    auto record = FindCurrent(request.window);
    if (record == nullptr || record->state != CaptureState::Attaching
        || record->pending_request != request.token) {
        return false;
    }

    record->pending_request = 0;
    if (!succeeded || !group_.Add(request.window, activate)) {
        record->state = CaptureState::Discovered;
        return false;
    }
    record->state = CaptureState::Managed;
    return true;
}

std::optional<RestoreRequest> CaptureCoordinator::BeginRestore(const WindowIdentity& window)
{
    auto record = FindCurrent(window);
    if (record == nullptr || record->state != CaptureState::Managed) {
        return std::nullopt;
    }
    record->state = CaptureState::Restoring;
    record->pending_request = next_request_++;
    return RestoreRequest{window, record->pending_request};
}

bool CaptureCoordinator::CompleteRestore(const RestoreRequest& request, bool succeeded)
{
    auto record = FindCurrent(request.window);
    if (record == nullptr || record->state != CaptureState::Restoring
        || record->pending_request != request.token) {
        return false;
    }
    record->pending_request = 0;
    if (!succeeded) {
        record->state = CaptureState::Managed;
        return false;
    }
    return Forget(request.window);
}

bool CaptureCoordinator::OnForeground(const WindowIdentity& window)
{
    const auto record = FindCurrent(window);
    if (record == nullptr || record->state != CaptureState::Managed) {
        return false;
    }
    const auto tab = group_.Find(window);
    return tab != nullptr && group_.Activate(tab->id);
}

const CaptureRecord* CaptureCoordinator::Find(NativeWindow handle) const
{
    const auto found = records_.find(handle);
    return found == records_.end() ? nullptr : &found->second;
}

CaptureRecord* CaptureCoordinator::FindCurrent(const WindowIdentity& window)
{
    const auto found = records_.find(window.window.handle);
    if (found == records_.end() || found->second.window != window) {
        return nullptr;
    }
    return &found->second;
}

} // namespace quickky
