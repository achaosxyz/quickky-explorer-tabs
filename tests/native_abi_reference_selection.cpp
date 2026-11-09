#include "abi_reference/profile.h"
#include "explorer/native_abi_internal.h"
#include <bcrypt.h>
#include <array>
#include <vector>

namespace quickky {
namespace {
using detail::NativeImageView;

bool HashBytes(const unsigned char* bytes, std::size_t size, std::array<unsigned char, 32>& digest)
{
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    bool match = false;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
        BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0 &&
        BCryptHashData(hash, const_cast<unsigned char*>(bytes), static_cast<ULONG>(size), 0) >= 0 &&
        BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0) {
        match = true;
    }
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    return match;
}

bool DigestMatches(const std::vector<unsigned char>& bytes, const unsigned char* expected)
{
    std::array<unsigned char, 32> digest{};
    return HashBytes(bytes.data(), bytes.size(), digest) &&
        std::equal(digest.begin(), digest.end(), expected);
}

bool EntryFingerprintsMatch(const NativeImageView& image, const NativeAbiProfile& profile)
{
    for (unsigned i = 0; i < 13; ++i) {
        const auto rva = profile.descriptor.methods[i];
        const auto& entry = profile.entry_fingerprints[i];
        if (!entry.length || !image.Section(rva, entry.length, true) ||
            (entry.unwind && !image.Function(rva, entry.length))) return false;
        std::vector<unsigned char> code(image.bytes + rva, image.bytes + rva + entry.length);
        if (i == 8) {
            // Fallback is stricter than accessor-family recognition: do not mask
            // relative calls or fields, only the reviewed diagnostic immediates.
            for (const auto offset : profile.data_diagnostics) {
                if (!offset || offset > code.size() || code.size() - offset < 5 ||
                    code[offset - 1] != 0xba || code[offset + 4] != 0xe8) return false;
                std::fill_n(code.begin() + offset, 4, static_cast<unsigned char>(0));
            }
        }
        if (!DigestMatches(code, entry.sha256)) return false;
    }
    return true;
}
} // namespace

bool SelectNativeAbi(const unsigned char* bytes, std::size_t size, std::uintptr_t base,
    const NativeAbiProfile* profiles, std::size_t count,
    NativeAbiDescriptor& descriptor, NativeAbiLayout& layout,
    std::uint64_t file_version, const NativeAbiProfile** matched)
{
    descriptor = {};
    layout = {};
    if (matched) *matched = nullptr;
    NativeImageIdentity identity;
    if (!profiles || !InspectNativeImage(bytes, size, identity)) return false;
    bool exact_identity = false;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& candidate = profiles[i].descriptor;
        if (!InlineIsEqualGUID(identity.pdb, candidate.image.pdb) ||
            identity.age != candidate.image.age || identity.size != candidate.image.size) continue;
        exact_identity = true;
        if (!ValidateNativeAbi(bytes, size, base, candidate, layout)) continue;
        descriptor = candidate;
        if (matched) *matched = profiles + i;
        return true;
    }
    // A known identity with a failed contract is not an unknown-version upgrade.
    if (exact_identity || !file_version) return false;
    NativeImageView image{bytes, size};
    if (!image.Open()) return false;
    std::vector<const NativeAbiProfile*> candidates;
    for (std::size_t i = 0; i < count; ++i) {
        if (profiles[i].file_version && profiles[i].file_version <= file_version)
            candidates.push_back(profiles + i);
    }
    std::stable_sort(candidates.begin(), candidates.end(), [](const auto a, const auto b) {
        return a->file_version > b->file_version;
    });
    for (const auto profile : candidates) {
        if (!EntryFingerprintsMatch(image, *profile)) continue;
        auto candidate = profile->descriptor;
        candidate.image = identity;
        if (!ValidateNativeAbi(bytes, size, base, candidate, layout)) continue;
        descriptor = candidate;
        if (matched) *matched = profile;
        return true;
    }
    return false;
}

} // namespace quickky
