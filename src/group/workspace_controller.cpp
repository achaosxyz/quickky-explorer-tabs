#include "group/workspace_controller.h"

#include "explorer/folder_location.h"
#include "explorer/native_tab_guard.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <utility>

namespace quickky {
namespace {
bool UsesNativeTabs()
{
    // This chooses a backend, not an ABI capability. NativeFrame separately
    // validates a built-in map and accessor contract on the target thread.
    OSVERSIONINFOEXW version{sizeof(version)};
    version.dwMajorVersion = 10;
    version.dwBuildNumber = 22000;
    DWORDLONG mask = 0;
    for (const DWORD field : {VER_MAJORVERSION, VER_MINORVERSION, VER_BUILDNUMBER}) {
        mask = VerSetConditionMask(mask, field, VER_GREATER_EQUAL);
    }
    return VerifyVersionInfoW(&version, VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER, mask) != FALSE;
}
}

WorkspaceController::WorkspaceController(HWND sender, CaptureCoordinator& capture,
    GroupModel& group, ExplorerWindowProbe& probe, HookLibrary& library, std::wstring root)
    : sender_(sender), capture_(capture), group_(group), probe_(probe), root_(std::move(root)), library_(library) {}

WorkspaceController::~WorkspaceController() { Stop(); }
bool WorkspaceController::Initialize() { native_ = UsesNativeTabs(); return library_.Load(); }

std::vector<HWND> WorkspaceController::FolderRoots(
    const std::map<NativeWindow, WindowCandidate>& visible) const
{
    std::vector<HWND> roots;
    for (const auto& entry : entries_) roots.push_back(reinterpret_cast<HWND>(entry.first));
    for (const auto& item : visible) {
        if (entries_.count(item.first) || entries_.size() >= kMaxWorkspaceTabs) continue;
        const auto target = reinterpret_cast<HWND>(item.first);
        const auto blocked = quarantined_.find(item.first);
        if (native_ ? (blocked != quarantined_.end() && blocked->second == item.second) :
            NativeTabGuard::CountTabs(target) != 1) continue;
        roots.push_back(target);
    }
    return roots;
}

std::map<NativeWindow, WindowIdentity> WorkspaceController::SnapshotAttachments() const
{
    std::map<NativeWindow, WindowIdentity> attachments;
    for (const auto& entry : entries_) attachments.emplace(entry.first, entry.second.client->window());
    return attachments;
}

void WorkspaceController::ReconcileEntries(const FolderLocations& locations,
    const std::map<NativeWindow, WindowIdentity>& attachments)
{
    std::vector<std::pair<NativeWindow, WindowIdentity>> removed;
    for (const auto& attachment : attachments) {
        const auto target = reinterpret_cast<HWND>(attachment.first);
        const auto folders = locations.find(target);
        if (folders == locations.end()) continue; // Missing coverage is not a scope rejection.
        const auto current = entries_.find(attachment.first);
        if (current == entries_.end() || current->second.client->window() != attachment.second) continue;
        const auto candidate = probe_.Inspect(target, !native_);
        // Inspect may pump too. Never apply this batch to a replacement attachment.
        const auto entry = entries_.find(attachment.first);
        if (entry == entries_.end() || entry->second.client->window() != attachment.second) continue;
        const bool identity_valid = candidate && *candidate == attachment.second.window && entry->second.client->Healthy();
        const bool valid = identity_valid &&
            (native_ ? InNativeScope(target, locations) :
             (folders->second.size() == 1 && (root_.empty() ||
              IsWithinDirectory(folders->second.front().parsing_name, root_))));
        // A newly added native tab may not yet have a public Shell view. Keep
        // the original leases for known pending work; mutations remain paused
        // until full scope is visible again. Never defer a proven outside path.
        const bool pending_root = native_ && native_pending_ &&
            (native_pending_->source == attachment.second || native_pending_->destination == attachment.second);
        const bool outside = !root_.empty() && std::any_of(folders->second.begin(), folders->second.end(),
            [this](const FolderLocation& folder) { return !IsWithinDirectory(folder.parsing_name, root_); });
        if (!valid && pending_root && !outside && identity_valid) continue;
        const auto retained = entries_.find(attachment.first);
        if (retained == entries_.end() || retained->second.client->window() != attachment.second) continue;
        if (!valid) {
            removed.push_back(attachment);
        } else if (!native_) retained->second.label = folders->second.front().display_name;
    }
    for (const auto& attachment : removed) ReleaseIfCurrent(attachment.first, attachment.second);
}

const Tab* WorkspaceController::FindTab(TabId id) const
{
    const auto& tabs = group_.tabs();
    const auto found = std::find_if(tabs.begin(), tabs.end(),
        [id](const Tab& tab) { return tab.id == id; });
    return found == tabs.end() ? nullptr : &*found;
}

void WorkspaceController::SavePlacement()
{
    const auto active = group_.active();
    const auto tab = active ? FindTab(*active) : nullptr;
    if (tab && IsWindowVisible(reinterpret_cast<HWND>(tab->window.window.handle))) {
        WINDOWPLACEMENT current{sizeof(current)};
        if (GetWindowPlacement(reinterpret_cast<HWND>(tab->window.window.handle), &current)) {
            placement_ = current;
            placed_ = true;
        }
    }
}

void WorkspaceController::Release(NativeWindow window)
{
    const auto found = entries_.find(window);
    if (found == entries_.end()) return;
    const auto identity = found->second.client->window();
    found->second.client->Cancel();
    const auto restoring = capture_.BeginRestore(identity);
    const bool restored = found->second.client->Detach();
    if (restoring && restored) capture_.CompleteRestore(*restoring, true);
    // Terminal release: the canceled lease remains open in the DLL, which owns
    // delayed restoration if the target cannot acknowledge. Do not claim success.
    else capture_.Forget(identity);
    entries_.erase(found);
}

void WorkspaceController::ReleaseIfCurrent(NativeWindow window, const WindowIdentity& identity)
{
    const auto current = entries_.find(window);
    if (current != entries_.end() && current->second.client->window() == identity) Release(window);
}

void WorkspaceController::Stop()
{
    native_transferred_ = false;
    for (auto& entry : entries_) entry.second.client->Cancel();
    native_pending_.reset();
    while (!entries_.empty()) Release(entries_.begin()->first);
    inputs_.clear();
    placed_ = false;
    main_ = 0;
    quarantined_.clear();
}

bool WorkspaceController::Refresh(const std::map<NativeWindow, WindowCandidate>& visible)
{
    native_transferred_ = false;
    if (native_) return RefreshNative(visible);
    SavePlacement();
    // Initial snapshots never authorize commit or native mutation.
    const auto candidates = visible;
    const auto attachments = SnapshotAttachments();
    const auto locations = ResolveFolderLocations(FolderRoots(candidates));
    ReconcileEntries(locations, attachments);

    for (const auto& item : candidates) {
        if (entries_.count(item.first) || entries_.size() >= kMaxWorkspaceTabs) continue;
        const HWND target = reinterpret_cast<HWND>(item.first);
        if (NativeTabGuard::CountTabs(target) != 1) continue;
        const auto found = locations.find(target);
        if (found == locations.end()) continue; // It became ready after this pass's snapshot.
        const auto& folders = found->second;
        if (folders.size() != 1 || (!root_.empty() && !IsWithinDirectory(folders.front().parsing_name, root_))) continue;
        const auto current = probe_.Inspect(target);
        if (!current || !(*current == item.second)) continue;
        const auto identity = capture_.Observe(*current);
        const auto request = identity ? capture_.BeginAttach(*identity) : std::nullopt;
        if (!request) continue;
        auto client = std::make_shared<HookClient>();
        bool attached = client->Prepare(sender_, library_.module(), *identity);
        if (attached) {
            const auto verified = probe_.Inspect(target);
            const auto folder = verified && *verified == *current ? ResolveFolder(target) : std::nullopt;
            attached = folder && (root_.empty() || IsWithinDirectory(folder->parsing_name, root_)) &&
                client->Commit();
        }
        if (!capture_.CompleteAttach(*request, attached, true)) continue;
        if (!placed_) {
            placed_ = GetWindowPlacement(target, &placement_) != FALSE;
        }
        entries_.emplace(item.first, Entry{std::move(client), folders.front().display_name});
    }
    const auto foreground = reinterpret_cast<NativeWindow>(GetForegroundWindow());
    if (const auto entry = entries_.find(foreground); entry != entries_.end() &&
        IsWindowVisible(reinterpret_cast<HWND>(foreground))) {
        capture_.OnForeground(entry->second.client->window());
    }
    return Publish();
}

bool WorkspaceController::InNativeScope(HWND target) const
{
    return InNativeScope(target, ResolveFolderLocations({target}));
}

bool WorkspaceController::InNativeScope(HWND target, const FolderLocations& locations) const
{
    const auto found = locations.find(target);
    if (found == locations.end()) return false;
    const auto& folders = found->second;
    if (folders.empty() || folders.size() != NativeTabGuard::CountTabs(target)) return false;
    return root_.empty() || std::all_of(folders.begin(), folders.end(), [this](const FolderLocation& folder) {
        return IsWithinDirectory(folder.parsing_name, root_);
    });
}

bool WorkspaceController::RefreshNative(const std::map<NativeWindow, WindowCandidate>& visible)
{
    const auto candidates = visible;
    const auto attachments = SnapshotAttachments();
    const auto locations = ResolveFolderLocations(FolderRoots(candidates));
    ReconcileEntries(locations, attachments);
    for (const auto& item : candidates) {
        if (entries_.count(item.first) || entries_.size() >= kMaxWorkspaceTabs) continue;
        const auto blocked = quarantined_.find(item.first);
        if (blocked != quarantined_.end() && blocked->second == item.second) continue;
        const auto target = reinterpret_cast<HWND>(item.first);
        if (!InNativeScope(target, locations)) continue;
        const auto candidate = probe_.Inspect(target);
        if (!candidate || !(*candidate == item.second)) continue;
        const auto identity = capture_.Observe(*candidate);
        const auto request = identity ? capture_.BeginAttach(*identity) : std::nullopt;
        if (!request) continue;
        auto client = std::make_shared<HookClient>();
        bool attached = client->Prepare(sender_, library_.module(), *identity, WorkspaceMode::Native);
        if (attached) {
            const auto verified = probe_.Inspect(target);
            attached = verified && *verified == *candidate && InNativeScope(target) && client->Commit();
        }
        if (!capture_.CompleteAttach(*request, attached, true)) continue;
        entries_.emplace(item.first, Entry{std::move(client), L""});
    }
    if (native_pending_) {
        ContinueNativeTransfer(locations);
        return true;
    }
    if (!entries_.count(main_)) {
        const auto foreground = reinterpret_cast<NativeWindow>(GetForegroundWindow());
        main_ = entries_.count(foreground) ? foreground : entries_.empty() ? 0 : entries_.begin()->first;
    }
    if (!main_) return true;
    const auto participants = entries_; // Pin clients while COM can pump Stop/recovery.
    const auto destination_client = participants.at(main_).client;
    auto& destination = *destination_client;
    for (const auto& item : participants) {
        if (item.first == main_) continue;
        const auto source_window = reinterpret_cast<HWND>(item.first);
        auto& source = *item.second.client;
        const auto current = [&] {
            const auto from = entries_.find(item.first), to = entries_.find(destination.window().window.handle);
            return !native_pending_ && from != entries_.end() && to != entries_.end() &&
                from->second.client == item.second.client && to->second.client == destination_client;
        };
        const auto blocked = quarantined_.find(item.first);
        if (blocked != quarantined_.end() && blocked->second == source.window().window) continue;
        std::vector<GUID> tabs;
        GUID active{};
        OwnedPidl folder;
        FolderSelection selection;
        std::uint64_t source_input = 0;
        const bool captured = source.CaptureNative(tabs, active, folder, selection, source_input);
        if (!current()) return true;
        if (!captured) continue;
        // COM may pump messages. Check both identities/scope immediately before
        // mutation; final source deletion also rechecks selection and input epoch.
        const auto source_identity = probe_.Inspect(source_window);
        const auto destination_identity = probe_.Inspect(reinterpret_cast<HWND>(main_));
        if (!current()) return true;
        if (!source_identity || !(*source_identity == source.window().window) ||
            !destination_identity || !(*destination_identity == destination.window().window)) continue;
        const auto transfer_locations = ResolveFolderLocations({source_window, reinterpret_cast<HWND>(main_)});
        if (!current()) return true;
        if (!InNativeScope(source_window, transfer_locations) ||
            !InNativeScope(reinterpret_cast<HWND>(main_), transfer_locations)) continue;
        GUID created{};
        std::uint64_t destination_input = 0;
        // Record the mutation barrier before a callout can invalidate its
        // entries. Acknowledged pending work resumes; an uncertain add cannot
        // be repeated by recovery within this enablement session.
        quarantined_[item.first] = source.window().window;
        const bool added = destination.AddNative(folder.get(), created, destination_input);
        if (!current()) return true;
        if (added) {
            auto pending = std::make_shared<NativeTransfer>();
            pending->source = source.window(); pending->destination = destination.window();
            pending->source_tab = active; pending->created = created;
            pending->folder = std::move(folder); pending->selection = std::move(selection);
            pending->source_input = source_input; pending->destination_input = destination_input;
            native_pending_ = std::move(pending);
            // The restore poll must run before a final folder confirmation:
            // initial navigation is allowed to finish on subsequent passes.
            ContinueNativeTransfer(transfer_locations);
        }
        // One source-tab transfer per reconciliation bounds work and lets native
        // lifecycle settle before handling another tab in a multi-tab source frame.
        break;
    }
    return true;
}

void WorkspaceController::ContinueNativeTransfer(const FolderLocations& locations)
{
    const auto pending = native_pending_;
    if (!pending) return;
    const auto source_key = pending->source.window.handle;
    const auto destination_key = pending->destination.window.handle;
    const auto source = entries_.find(source_key), destination = entries_.find(destination_key);
    const auto abort = [&] {
        if (native_pending_ != pending) return;
        quarantined_[source_key] = pending->source.window;
        native_pending_.reset();
    };
    if (source == entries_.end() || destination == entries_.end() ||
        source->second.client->window() != pending->source ||
        destination->second.client->window() != pending->destination) { abort(); return; }
    const auto main = destination->second.client, incoming = source->second.client;
    // Observe cancellation even when a loading Shell view lacks metadata.
    if (!main->native_input_unchanged() || !incoming->native_input_unchanged()) { abort(); return; }
    if (!InNativeScope(reinterpret_cast<HWND>(source_key), locations) ||
        !InNativeScope(reinterpret_cast<HWND>(destination_key), locations)) return;
    const auto current = [&] {
        const auto from = entries_.find(source_key), to = entries_.find(destination_key);
        return native_pending_ == pending && from != entries_.end() && to != entries_.end() &&
            from->second.client == incoming && to->second.client == main &&
            incoming->window() == pending->source && main->window() == pending->destination;
    };
    const bool restored = main->RestoreNative(pending->created, pending->folder.get(), pending->selection,
        pending->destination_input, pending->restore_progress);
    if (!current()) return;
    if (!restored) {
        if (!main->native_waiting()) abort();
        return;
    }
    const auto final_locations = ResolveFolderLocations({reinterpret_cast<HWND>(source_key),
        reinterpret_cast<HWND>(destination_key)});
    // Shell automation can pump stop/recovery. Do not reuse attachment
    // references across that callout or apply a batch to their replacements.
    if (!current()) return;
    if (!InNativeScope(reinterpret_cast<HWND>(source_key), final_locations) ||
        !InNativeScope(reinterpret_cast<HWND>(destination_key), final_locations)) return;
    std::vector<GUID> tabs;
    GUID selected{};
    OwnedPidl folder;
    const bool confirmed = main->ReadNative(tabs, selected, folder);
    if (!current()) return;
    if (!confirmed || !InlineIsEqualGUID(selected, pending->created) ||
        !ILIsEqual(folder.get(), pending->folder.get())) { abort(); return; }
    const bool removed = incoming->RemoveNative(pending->source_tab, pending->folder.get(), pending->selection,
        pending->source_input, *main);
    if (!current()) return;
    if (!removed) {
        if (!incoming->native_waiting()) abort();
        return;
    }
    const auto blocked = quarantined_.find(source_key);
    if (blocked != quarantined_.end() && blocked->second == pending->source.window) quarantined_.erase(blocked);
    native_pending_.reset();
    native_transferred_ = true;
    // Presentation cannot repeat an acknowledged source removal.
    if (!main->RevealNative(pending->created)) {
        char diagnostic[160]{};
        std::snprintf(diagnostic, sizeof(diagnostic),
            "QTabs: transferred tab confirmed, destination activation/focus failed hr=0x%08lX target=%p foreground=%p\n",
            static_cast<unsigned long>(main->last_native_result()),
            reinterpret_cast<void*>(destination_key), static_cast<void*>(GetForegroundWindow()));
        OutputDebugStringA(diagnostic);
    }
}

bool WorkspaceController::Publish()
{
    if (entries_.empty()) { placed_ = false; return true; }
    GuardRequest view;
    view.count = static_cast<std::uint32_t>(group_.tabs().size());
    view.active = group_.active().value_or(0);
    unsigned index = 0;
    for (const auto& tab : group_.tabs()) {
        const auto entry = entries_.find(tab.window.window.handle);
        if (entry == entries_.end() || index >= kMaxWorkspaceTabs) return false;
        view.tabs[index].id = tab.id;
        wcsncpy_s(view.tabs[index].title, entry->second.label.c_str(), _TRUNCATE);
        ++index;
    }
    for (auto& entry : entries_) {
        if (!entry.second.publication.NeedsConfigure(view)) continue;
        if (!entry.second.client->Configure(view)) {
            OutputDebugStringA("Quickky: workspace Configure failed\n");
            return false;
        }
        entry.second.publication.Configured(view);
    }
    const auto active = FindTab(view.active);
    if (!active || !placed_) { OutputDebugStringA("Quickky: workspace active placement missing\n"); return false; }
    const auto present = [this](Entry& entry, bool visible) {
        const auto window = reinterpret_cast<HWND>(entry.client->window().window.handle);
        if (!entry.publication.NeedsPresent(placement_, visible, IsWindowVisible(window) != FALSE)) return true;
        if (!entry.client->Present(placement_, visible)) return false;
        entry.publication.Presented(placement_, visible);
        return true;
    };
    // Show the destination before hiding the source: a failed switch leaves a usable root.
    if (!present(entries_.at(active->window.window.handle), true)) {
        OutputDebugStringA("Quickky: workspace active Present failed\n");
        return false;
    }
    for (auto& entry : entries_) {
        if (entry.first != active->window.window.handle && !present(entry.second, false)) {
            OutputDebugStringA("Quickky: workspace inactive Present failed\n");
            return false;
        }
    }
    return true;
}

bool WorkspaceController::QueueInput(WPARAM sender, LPARAM packet)
{
    const auto data = reinterpret_cast<const COPYDATASTRUCT*>(packet);
    if (!data || data->dwData != kGuardProtocol || data->cbData < sizeof(GuardRequest) ||
        data->cbData > sizeof(GuardRequest) + kMaxFolderSnapshotBytes ||
        !data->lpData || inputs_.size() >= 64) return false;
    GuardRequest request;
    std::memcpy(&request, data->lpData, sizeof(request));
    if (request.size != data->cbData || request.version != kGuardVersion) return false;
    const auto found = entries_.find(request.target);
    if (found == entries_.end() || !found->second.client->AcceptInput(
        reinterpret_cast<HWND>(sender), request)) return false;
    OwnedPidl folder;
    if (request.input == TabInput::New) {
        folder = CloneFolderSnapshot(static_cast<const unsigned char*>(data->lpData) + sizeof(request),
            data->cbData - sizeof(request));
        if (!folder) return false;
    } else if (data->cbData != sizeof(request)) return false;
    inputs_.push_back(Input{found->second.client->window(), request.input, request.input_tab, std::move(folder)});
    return true;
}

bool WorkspaceController::ProcessInput()
{
    while (!inputs_.empty()) {
        auto input = std::move(inputs_.front()); inputs_.pop_front();
        const auto source = entries_.find(input.source.window.handle);
        if (source == entries_.end() || source->second.client->window() != input.source) continue;
        const auto active = group_.active();
        const auto active_tab = active ? FindTab(*active) : nullptr;
        if (!active_tab || active_tab->window != input.source) continue;
        if (!source->second.client->Healthy()) continue;
        SavePlacement();
        if (input.action == TabInput::Select) group_.Activate(input.tab);
        else if (input.action == TabInput::Next || input.action == TabInput::Previous) {
            const auto& tabs = group_.tabs();
            const auto current = std::find_if(tabs.begin(), tabs.end(),
                [active](const Tab& tab) { return tab.id == *active; });
            const auto index = static_cast<std::size_t>(current - tabs.begin());
            const auto next = (index + (input.action == TabInput::Next ? 1 : tabs.size() - 1)) % tabs.size();
            group_.Activate(tabs[next].id);
        } else if (input.action == TabInput::Close) {
            const auto closing = FindTab(input.tab ? input.tab : *active);
            if (closing) {
                const auto target = entries_.find(closing->window.window.handle);
                if (target != entries_.end() && (!target->second.client->RequestClose() ||
                    !target->second.client->Healthy())) Release(target->first);
            }
        } else if (input.action == TabInput::New) {
            const auto location = DescribeFolder(std::move(input.folder));
            if (location && (root_.empty() || IsWithinDirectory(location->parsing_name, root_))) {
                OpenFolderWindow(*location);
            }
        }
        if (!Publish()) return false;
    }
    return true;
}

} // namespace quickky
