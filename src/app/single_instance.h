#pragma once

#include <windows.h>
#include <memory>
#include <string>

namespace quickky {

enum class InstanceState { Failed, Primary, Duplicate };

// Resident-thread lifetime guard. Mutex existence (not thread ownership) is the
// exclusion boundary. Keep this alive until all controller teardown completes.
// Explicit names let process tests exercise the same guard in an isolated scope.
class SingleInstance final {
    using Handle = std::unique_ptr<void, decltype(&CloseHandle)>;

    // Serialize publication of the singleton/endpoint pair, not the entire
    // resident lifetime. Stop cannot inspect a half-created pair or signal a
    // colliding event while construction is deciding whether it can be primary.
    class StartupGate final {
    public:
        StartupGate(const wchar_t* mutex_name, DWORD timeout)
        {
            const auto name = std::wstring(mutex_name) + L".Startup";
            handle_.reset(CreateMutexW(nullptr, FALSE, name.c_str()));
            if (!handle_) return;
            const auto wait = WaitForSingleObject(handle_.get(), timeout);
            locked_ = wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED;
        }
        ~StartupGate() { if (locked_) ReleaseMutex(handle_.get()); }
        explicit operator bool() const { return locked_; }
    private:
        Handle handle_{nullptr, CloseHandle};
        bool locked_ = false;
    };

public:
    SingleInstance(const wchar_t* mutex_name, const wchar_t* stop_name)
    {
        const StartupGate gate(mutex_name, 1000);
        if (!gate) return;
        mutex_.reset(CreateMutexW(nullptr, FALSE, mutex_name));
        const auto mutex_error = GetLastError();
        if (!mutex_) return;
        if (mutex_error == ERROR_ALREADY_EXISTS) {
            mutex_.reset();
            state_ = InstanceState::Duplicate;
            return;
        }

        stop_.reset(CreateEventW(nullptr, TRUE, FALSE, stop_name));
        const auto stop_error = GetLastError();
        if (!stop_ || stop_error == ERROR_ALREADY_EXISTS) {
            // Never reset a colliding event or hold the singleton after failure.
            stop_.reset();
            mutex_.reset();
            return;
        }
        state_ = InstanceState::Primary;
    }

    InstanceState state() const { return state_; }
    HANDLE stop_event() const { return stop_.get(); }

    static bool RequestStop(const wchar_t* mutex_name, const wchar_t* stop_name)
    {
        const StartupGate gate(mutex_name, 0);
        if (!gate) return false;
        // Pin the existing instance before opening its endpoint. This excludes
        // a replacement generation and leaves orphaned same-name events alone.
        const Handle mutex(OpenMutexW(SYNCHRONIZE, FALSE, mutex_name), CloseHandle);
        if (!mutex) return GetLastError() == ERROR_FILE_NOT_FOUND;
        const Handle stop(OpenEventW(EVENT_MODIFY_STATE, FALSE, stop_name), CloseHandle);
        if (!stop) return false;
        return SetEvent(stop.get()) != FALSE;
    }

private:
    Handle mutex_{nullptr, CloseHandle};
    Handle stop_{nullptr, CloseHandle};
    InstanceState state_ = InstanceState::Failed;
};

} // namespace quickky
