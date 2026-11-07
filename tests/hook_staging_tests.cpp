#include "hook/hook_client.h"
#include "runtime_files.h"

#include <iterator>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

std::wstring g_source;
std::wstring g_destination;
bool g_partial_file_written = false;

void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

std::wstring Leaf(const std::wstring& path)
{
    const auto separator = path.find_last_of(L"\\/");
    return separator == std::wstring::npos ? path : path.substr(separator + 1);
}

BOOL WINAPI FailAfterWritingPartialFile(LPCWSTR source, LPCWSTR destination, BOOL)
{
    g_source = source;
    g_destination = destination;
    HANDLE file = CreateFileW(destination, GENERIC_WRITE, 0, nullptr, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return FALSE;

    constexpr char partial[] = "partial runtime copy";
    DWORD written = 0;
    const BOOL wrote = WriteFile(file, partial, sizeof(partial), &written, nullptr);
    CloseHandle(file);
    g_partial_file_written = wrote && written == sizeof(partial);
    SetLastError(ERROR_DISK_FULL);
    return FALSE;
}

class RuntimeSourceFixture {
public:
    explicit RuntimeSourceFixture(std::wstring path) : path_(std::move(path)) {}
    RuntimeSourceFixture(const RuntimeSourceFixture&) = delete;
    RuntimeSourceFixture& operator=(const RuntimeSourceFixture&) = delete;
    ~RuntimeSourceFixture()
    {
        if (created_) DeleteFileW(path_.c_str());
    }

    bool EnsureExists()
    {
        if (GetFileAttributesW(path_.c_str()) != INVALID_FILE_ATTRIBUTES) return true;
        const auto error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) return false;

        HANDLE file = CreateFileW(path_.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        created_ = true;
        constexpr char source[] = "runtime source fixture";
        DWORD written = 0;
        const BOOL wrote = WriteFile(file, source, sizeof(source), &written, nullptr);
        CloseHandle(file);
        return wrote && written == sizeof(source);
    }

private:
    std::wstring path_;
    bool created_ = false;
};

void TestFailedRuntimeCopyCleanup()
{
    Check(std::size(quickky::kRuntimeFiles) != 0,
        "The staging test requires at least one configured runtime filename");

    wchar_t executable[32768]{};
    const auto length = GetModuleFileNameW(nullptr, executable, static_cast<DWORD>(std::size(executable)));
    Check(length != 0 && length < std::size(executable), "Locate staging-test executable");
    const std::wstring executable_path(executable, length);
    const auto separator = executable_path.find_last_of(L"\\/");
    Check(separator != std::wstring::npos, "Locate staging-test directory");

    const std::wstring runtime_name = quickky::kRuntimeFiles[0];
    RuntimeSourceFixture source(executable_path.substr(0, separator + 1) + runtime_name);
    Check(source.EnsureExists(), "Prepare first runtime source for deterministic copy failure");

    quickky::HookLibrary library(&FailAfterWritingPartialFile);
    Check(!library.Load(), "A failed runtime copy must fail hook-library loading");
    Check(g_partial_file_written, "The injected copy failure must leave a partial destination");
    Check(Leaf(g_source) == runtime_name && Leaf(g_destination) == runtime_name,
        "Exercise the runtime-copy path rather than the hook-DLL copy path");
    Check(GetFileAttributesW(g_destination.c_str()) == INVALID_FILE_ATTRIBUTES,
        "Remove the partial runtime destination after copy failure");

    const auto staging_separator = g_destination.find_last_of(L"\\/");
    Check(staging_separator != std::wstring::npos, "Locate per-run staging directory");
    const auto staging_directory = g_destination.substr(0, staging_separator);
    Check(GetFileAttributesW(staging_directory.c_str()) == INVALID_FILE_ATTRIBUTES,
        "Remove the empty per-run staging directory after copy failure");
}

} // namespace

int main()
{
    try {
        TestFailedRuntimeCopyCleanup();
        std::cout << "PASS: failed runtime staging copy removes its partial destination.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
