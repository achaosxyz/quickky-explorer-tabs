#include "app/single_instance.h"
#include "settings/preferences.h"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Handle = std::unique_ptr<void, decltype(&CloseHandle)>;

void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

std::wstring UniqueName(const wchar_t* suffix)
{
    return L"QTabs.Test." + std::to_wstring(GetCurrentProcessId()) + L"." +
        std::to_wstring(GetTickCount64()) + L"." + suffix;
}

// Every fixture uses private object names, never the resident controller's names.
struct Scope {
    std::wstring name = L"Local\\" + UniqueName(L"Instance");
    std::wstring mutex = name + L".Mutex";
    std::wstring stop = name + L".Stop";
    std::wstring ready_name = name + L".Ready";
    std::wstring gate_name = name + L".Gate";
    Handle ready{CreateEventW(nullptr, TRUE, FALSE, ready_name.c_str()), CloseHandle};
    Handle gate{CreateEventW(nullptr, TRUE, FALSE, gate_name.c_str()), CloseHandle};

    Scope() { Check(ready && gate, "Create isolated fixture events"); }
};

// Own only a test-created process; failure cleanup cannot stop a daily controller.
class Child final {
public:
    Child(const std::wstring& executable, const Scope& scope, bool initializing = false)
    {
        const auto mode = initializing ? L"--initializing-fixture" : L"--resident-fixture";
        std::wstring command = L"\"" + executable + L"\" " + mode + L" \"" +
            scope.mutex + L"\" \"" + scope.stop + L"\" \"" + scope.ready_name +
            L"\" \"" + scope.gate_name + L"\"";
        STARTUPINFOW startup{sizeof(startup)};
        PROCESS_INFORMATION info{};
        Check(CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr,
            FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &info) != FALSE,
            "Launch isolated child");
        process_.reset(info.hProcess);
        CloseHandle(info.hThread);
    }
    ~Child()
    {
        if (WaitForSingleObject(process_.get(), 0) == WAIT_TIMEOUT) {
            TerminateProcess(process_.get(), 99);
            WaitForSingleObject(process_.get(), 5000);
        }
    }
    bool running() const
    {
        const auto wait = WaitForSingleObject(process_.get(), 0);
        Check(wait == WAIT_TIMEOUT || wait == WAIT_OBJECT_0, "Wait for fixture process");
        return wait == WAIT_TIMEOUT;
    }
    HANDLE handle() const { return process_.get(); }
    void ExpectExit(DWORD expected = 0) const
    {
        Check(WaitForSingleObject(process_.get(), 5000) == WAIT_OBJECT_0, "Fixture exits on time");
        DWORD code = 0;
        Check(GetExitCodeProcess(process_.get(), &code) && code == expected, "Fixture exit status");
    }
    void Crash() const
    {
        Check(TerminateProcess(process_.get(), 98) != FALSE, "Terminate only the fixture");
        ExpectExit(98);
    }
private:
    Handle process_{nullptr, CloseHandle};
};

// A copied fixture proves exclusion is independent of the executable's directory.
struct CopiedImage {
    std::wstring directory;
    std::wstring file;
    CopiedImage()
    {
        wchar_t temporary[MAX_PATH + 1]{};
        const auto length = GetTempPathW(MAX_PATH + 1, temporary);
        Check(length && length <= MAX_PATH, "Locate fixture staging directory");
        directory = std::wstring(temporary) + UniqueName(L"Image");
        Check(CreateDirectoryW(directory.c_str(), nullptr) != FALSE, "Create owned fixture directory");
        file = directory + L"\\QTabs-instance-fixture.exe";
        if (!CopyFileW(quickky::ExecutablePath().c_str(), file.c_str(), TRUE)) {
            RemoveDirectoryW(directory.c_str());
            throw std::runtime_error("Copy fixture executable");
        }
    }
    ~CopiedImage() { DeleteFileW(file.c_str()); RemoveDirectoryW(directory.c_str()); }
};

int RunChild(wchar_t** arguments)
{
    Handle gate(OpenEventW(SYNCHRONIZE, FALSE, arguments[5]), CloseHandle);
    if (!gate) return 1;
    if (std::wstring(arguments[1]) == L"--initializing-fixture") {
        // Freeze the real publication protocol between claiming the singleton
        // and validating its endpoint, using only this fixture's unique names.
        const auto name = std::wstring(arguments[2]) + L".Startup";
        Handle startup(CreateMutexW(nullptr, TRUE, name.c_str()), CloseHandle);
        if (!startup || GetLastError() == ERROR_ALREADY_EXISTS) return 1;
        Handle singleton(CreateMutexW(nullptr, FALSE, arguments[2]), CloseHandle);
        if (!singleton || GetLastError() == ERROR_ALREADY_EXISTS) return 1;
        Handle ready(OpenEventW(EVENT_MODIFY_STATE, FALSE, arguments[4]), CloseHandle);
        if (!ready || !SetEvent(ready.get())) return 1;
        const auto wait = WaitForSingleObject(gate.get(), 10000);
        ReleaseMutex(startup.get());
        return wait == WAIT_OBJECT_0 ? 0 : 1;
    }
    if (WaitForSingleObject(gate.get(), 5000) != WAIT_OBJECT_0) return 1;
    quickky::SingleInstance instance(arguments[2], arguments[3]);
    if (instance.state() == quickky::InstanceState::Duplicate) return 0;
    if (instance.state() != quickky::InstanceState::Primary) return 1;
    Handle ready(OpenEventW(EVENT_MODIFY_STATE, FALSE, arguments[4]), CloseHandle);
    if (!ready || !SetEvent(ready.get())) return 1;
    return WaitForSingleObject(instance.stop_event(), 10000) == WAIT_OBJECT_0 ? 0 : 1;
}

void SignalStop(const Scope& scope)
{
    Check(quickky::SingleInstance::RequestStop(scope.mutex.c_str(), scope.stop.c_str()),
        "Request stop from another fixture process");
}

void TestConcurrentLaunchAndRestart()
{
    Scope scope;
    CopiedImage copy;
    for (unsigned round = 0; round < 2; ++round) {
        Check(ResetEvent(scope.ready.get()) && ResetEvent(scope.gate.get()), "Reset fixture barriers");
        std::vector<std::unique_ptr<Child>> children;
        for (unsigned index = 0; index < 8; ++index) {
            children.push_back(std::make_unique<Child>(
                index % 2 ? copy.file : quickky::ExecutablePath(), scope));
        }
        Check(SetEvent(scope.gate.get()) != FALSE, "Release concurrent launches");
        Check(WaitForSingleObject(scope.ready.get(), 5000) == WAIT_OBJECT_0, "Primary becomes ready");
        // A ready primary does not imply that every other image has finished
        // loading. Wait for duplicates to exit instead of classifying a slow
        // loader as a second resident after an arbitrary short delay.
        const auto deadline = GetTickCount64() + 5000;
        for (;;) {
            std::vector<HANDLE> pending;
            for (const auto& child : children) {
                if (child->running()) pending.push_back(child->handle());
                else child->ExpectExit();
            }
            Check(!pending.empty(), "Primary remains alive");
            if (pending.size() == 1) break;
            const auto now = GetTickCount64();
            Check(now < deadline, "Duplicate launches exit on time");
            const auto wait = WaitForMultipleObjects(static_cast<DWORD>(pending.size()),
                pending.data(), FALSE, static_cast<DWORD>(deadline - now));
            Check(wait < WAIT_OBJECT_0 + pending.size(),
                "Wait for concurrent duplicates");
        }
        SignalStop(scope);
        for (const auto& child : children) child->ExpectExit();
    }
}

void TestCrashRecovery()
{
    Scope scope;
    Check(SetEvent(scope.gate.get()) != FALSE, "Release crash fixture");
    {
        Child primary(quickky::ExecutablePath(), scope);
        Check(WaitForSingleObject(scope.ready.get(), 5000) == WAIT_OBJECT_0, "Crash fixture ready");
        primary.Crash();
    }
    Check(ResetEvent(scope.ready.get()) != FALSE, "Reset recovery barrier");
    Child restarted(quickky::ExecutablePath(), scope);
    Check(WaitForSingleObject(scope.ready.get(), 5000) == WAIT_OBJECT_0, "Restart after process loss");
    SignalStop(scope);
    restarted.ExpectExit();
}

void TestDuplicateDoesNotResetStop()
{
    Scope scope;
    quickky::SingleInstance primary(scope.mutex.c_str(), scope.stop.c_str());
    Check(primary.state() == quickky::InstanceState::Primary, "Acquire local fixture singleton");
    Check(SetEvent(primary.stop_event()) != FALSE, "Begin fixture shutdown");
    quickky::SingleInstance duplicate(scope.mutex.c_str(), scope.stop.c_str());
    Check(duplicate.state() == quickky::InstanceState::Duplicate && !duplicate.stop_event(),
        "Duplicate never acquires the stop endpoint");
    Check(WaitForSingleObject(primary.stop_event(), 0) == WAIT_OBJECT_0,
        "Duplicate cannot cancel primary shutdown");
}

void TestObjectCollisions()
{
    Scope scope;
    {
        const auto name = scope.mutex + L".Startup";
        Handle collision(CreateEventW(nullptr, TRUE, FALSE, name.c_str()), CloseHandle);
        Check(collision != nullptr, "Create wrong-kind startup gate");
        quickky::SingleInstance instance(scope.mutex.c_str(), scope.stop.c_str());
        Check(instance.state() == quickky::InstanceState::Failed, "Reject wrong-kind startup gate");
        Check(!quickky::SingleInstance::RequestStop(scope.mutex.c_str(), scope.stop.c_str()),
            "Stop rejects wrong-kind startup gate");
    }
    {
        Handle collision(CreateEventW(nullptr, TRUE, FALSE, scope.mutex.c_str()), CloseHandle);
        Check(collision != nullptr, "Create wrong-kind singleton collision");
        quickky::SingleInstance instance(scope.mutex.c_str(), scope.stop.c_str());
        Check(instance.state() == quickky::InstanceState::Failed, "Reject wrong-kind singleton");
    }
    {
        Handle collision(CreateMutexW(nullptr, FALSE, scope.stop.c_str()), CloseHandle);
        Check(collision != nullptr, "Create wrong-kind stop collision");
        quickky::SingleInstance instance(scope.mutex.c_str(), scope.stop.c_str());
        Check(instance.state() == quickky::InstanceState::Failed, "Reject wrong-kind stop endpoint");
    }
    {
        Handle collision(CreateEventW(nullptr, TRUE, TRUE, scope.stop.c_str()), CloseHandle);
        Check(collision != nullptr, "Create existing stop event");
        quickky::SingleInstance instance(scope.mutex.c_str(), scope.stop.c_str());
        Check(instance.state() == quickky::InstanceState::Failed, "Reject pre-existing stop event");
        Check(WaitForSingleObject(collision.get(), 0) == WAIT_OBJECT_0, "Do not reset colliding event");
    }
    quickky::SingleInstance recovered(scope.mutex.c_str(), scope.stop.c_str());
    Check(recovered.state() == quickky::InstanceState::Primary, "Failed acquisition releases handles");
}

void TestStopRequest()
{
    Scope scope;
    Check(quickky::SingleInstance::RequestStop(scope.mutex.c_str(), scope.stop.c_str()),
        "Stopping an absent resident is idempotent");
    {
        Handle orphan(CreateEventW(nullptr, TRUE, FALSE, scope.stop.c_str()), CloseHandle);
        Check(orphan != nullptr, "Create orphaned stop-event collision");
        Check(quickky::SingleInstance::RequestStop(scope.mutex.c_str(), scope.stop.c_str()),
            "No singleton means no stop request");
        Check(WaitForSingleObject(orphan.get(), 0) == WAIT_TIMEOUT,
            "Stop must not signal an orphaned same-name event");
    }
    {
        Handle unpublished(CreateMutexW(nullptr, FALSE, scope.mutex.c_str()), CloseHandle);
        Check(unpublished != nullptr, "Create singleton without a stop endpoint");
        Check(!quickky::SingleInstance::RequestStop(scope.mutex.c_str(), scope.stop.c_str()),
            "An unavailable stop endpoint must not report successful delivery");
    }
    {
        Handle wrong_kind(CreateEventW(nullptr, TRUE, FALSE, scope.mutex.c_str()), CloseHandle);
        Check(wrong_kind != nullptr, "Create wrong-kind singleton for stop request");
        Check(!quickky::SingleInstance::RequestStop(scope.mutex.c_str(), scope.stop.c_str()),
            "Stop rejects a wrong-kind singleton");
    }
    quickky::SingleInstance primary(scope.mutex.c_str(), scope.stop.c_str());
    Check(primary.state() == quickky::InstanceState::Primary, "Acquire stop-request fixture");
    Check(quickky::SingleInstance::RequestStop(scope.mutex.c_str(), scope.stop.c_str()) &&
        WaitForSingleObject(primary.stop_event(), 0) == WAIT_OBJECT_0,
        "Stop signals an existing resident");
}

void TestStopDuringStartup()
{
    for (const bool with_orphan : {false, true}) {
        Scope scope;
        Handle orphan(nullptr, CloseHandle);
        if (with_orphan) {
            orphan.reset(CreateEventW(nullptr, TRUE, FALSE, scope.stop.c_str()));
            Check(orphan != nullptr, "Create startup-collision event");
        }
        Child initializing(quickky::ExecutablePath(), scope, true);
        Check(WaitForSingleObject(scope.ready.get(), 5000) == WAIT_OBJECT_0,
            "Freeze fixture during singleton/endpoint publication");
        Check(!quickky::SingleInstance::RequestStop(scope.mutex.c_str(), scope.stop.c_str()),
            "Stop must not report success for a half-initialized resident");
        if (orphan) Check(WaitForSingleObject(orphan.get(), 0) == WAIT_TIMEOUT,
            "Stop during startup must not signal an orphaned endpoint");
        Check(SetEvent(scope.gate.get()) != FALSE, "Release initializing fixture");
        initializing.ExpectExit();
    }
}

// Redirect HKCU only within this test process. Real startup/preferences are never
// modified; the sole real key created is a unique, test-owned temporary subtree.
class RegistryScope final {
public:
    RegistryScope() : path_(L"Software\\" + UniqueName(L"Registry"))
    {
        DWORD disposition = 0;
        Check(RegCreateKeyExW(HKEY_CURRENT_USER, path_.c_str(), 0, nullptr,
            REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr, &key_, &disposition) == ERROR_SUCCESS,
            "Create isolated registry root");
        if (disposition != REG_CREATED_NEW_KEY) {
            RegCloseKey(key_);
            throw std::runtime_error("Refuse to reuse an existing registry fixture");
        }
        if (RegOverridePredefKey(HKEY_CURRENT_USER, key_) != ERROR_SUCCESS) {
            RegCloseKey(key_);
            RegDeleteTreeW(HKEY_CURRENT_USER, path_.c_str());
            throw std::runtime_error("Redirect fixture HKCU");
        }
    }
    ~RegistryScope()
    {
        RegOverridePredefKey(HKEY_CURRENT_USER, nullptr);
        RegCloseKey(key_);
        RegDeleteTreeW(HKEY_CURRENT_USER, path_.c_str());
    }
private:
    std::wstring path_;
    HKEY key_ = nullptr;
};

void TestQTabsSettings()
{
    RegistryScope scope;
    Check(!quickky::StartupEnabled() && !quickky::TabsEnabled(), "Default settings are disabled");
    Check(quickky::SetStartupEnabled(true) && quickky::StartupEnabled(), "Enable QTabs startup");
    wchar_t command[32768]{};
    DWORD bytes = sizeof(command);
    Check(RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        L"QTabs", RRF_RT_REG_SZ, nullptr, command, &bytes) == ERROR_SUCCESS,
        "Startup uses the QTabs value name");
    Check(command == L"\"" + quickky::ExecutablePath() + L"\"", "Startup quotes the executable path");
    Check(quickky::SetStartupEnabled(false) && !quickky::StartupEnabled(), "Disable QTabs startup");
    Check(quickky::SetStartupEnabled(false), "Disabling absent startup is idempotent");
    Check(quickky::SetTabsEnabled(true) && quickky::TabsEnabled(), "Save tabs preference");
    DWORD enabled = 0;
    bytes = sizeof(enabled);
    Check(RegGetValueW(HKEY_CURRENT_USER, L"Software\\QTabs", L"TabsEnabled",
        RRF_RT_REG_DWORD, nullptr, &enabled, &bytes) == ERROR_SUCCESS && enabled == 1,
        "Preferences use the QTabs key");
    Check(quickky::SetTabsEnabled(false) && !quickky::TabsEnabled(), "Clear tabs preference");
    Check(std::wstring(quickky::CurrentTrayStrings().about_title).find(L"QTabs") != std::wstring::npos,
        "Localized About title uses QTabs");
}

} // namespace

int wmain(int count, wchar_t** arguments)
{
    try {
        if (count == 6 && (std::wstring(arguments[1]) == L"--resident-fixture" ||
            std::wstring(arguments[1]) == L"--initializing-fixture")) return RunChild(arguments);
        Check(count == 1, "Unexpected fixture arguments");
        TestDuplicateDoesNotResetStop();
        TestObjectCollisions();
        TestStopRequest();
        TestStopDuringStartup();
        TestConcurrentLaunchAndRestart();
        TestCrashRecovery();
        TestQTabsSettings();
        std::cout << "QTabs isolated instance and settings tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
