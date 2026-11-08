# Windows 11 Native ABI Adaptation

## Scope and status

This document owns native `ExplorerFrame.dll` compatibility detection.
[Explorer adapters](EXPLORER.md) owns tab operations and
[Hook integration](HOOK.md) owns attachment. Windows 10 does not use these rules.
The application uses compiled-in maps only: no symbol downloads, PDB loading,
symbol cache, or background resolver. Exact public PDBs are development evidence,
not an application dependency.

The current catalog contains one verified map: ExplorerFrame `10.0.26100.9949`,
CodeView `822A8C93-960A-C7B6-7F6A-87CF48E8CE6F`, age 1. Other versions can have
independent map files and share a reviewed accessor rule. Do not invent entries
for versions without matching symbols and validation evidence.

## Versioned maps and reusable rules

- `src/explorer/abi_profiles/profile.h` defines image identity, locator data,
  accessor-rule ID, and a human-readable component version.
- `src/explorer/abi_profiles/<VERSION>.h` contains one immutable, reviewed map:
  CodeView GUID/age, image size, window/tab-actions vtable RVAs, lookup RVA,
  thirteen typed method RVAs, and its accessor-rule ID.
- `src/explorer/native_abi_profiles.cpp` registers the versioned map files.
- `src/explorer/native_abi.cpp` implements rule validation and field derivation.
  Maps for different component versions can reuse the same rule when their
  accessor code/storage contract is unchanged.

File-version strings organize the maps; they do not authorize calls. Match the
actual loaded image by CodeView identity/size, then verify its map and accessor
rule. A resource-only version change with unchanged image identity does not
require a new version comparison. New image identities or moved entry points
require a reviewed map; an absent map does not trigger downloading or guessing.
This is offline, rule-based adaptation, not automatic support for unknown builds.

## Binding flow

1. On the owning Explorer UI thread, inspect the actual loaded module's bounded
   PE metadata and CodeView identity. Do not base selection on a newer on-disk DLL.
   Require exactly one valid RSDS record with nonzero GUID/age; malformed or
   duplicate records reject the image and leave the output identity empty.
2. Select a matching entry from the compiled catalog. A failed candidate must not
   prevent a later valid map from being considered. Never use a nearby version,
   nearest numerical RVA, or OS-build range as a substitute.
3. Reject unknown rule IDs. Validate section bounds, read-only vtables,
   executable method entries, the recognized accessor bodies, and the original
   lookup-call target. Derive object offsets only after these checks succeed.
4. Validate the root's window-data and tab-actions pointers before taking a COM
   reference or reading private storage. Existing identity, lease, folder-scope,
   GUID/PIDL confirmation, and uncertain-transfer quarantine checks still apply.
   Native object/record ranges must fit one committed readable memory region;
   execute-only, guarded, inaccessible, and uncommitted pages are not readable.
5. Unsupported images/rules remain ordinary Explorer windows. No clipping,
   additional Windows 11 strip, path-reopen fallback, or process termination occurs.

All map selection runs locally, with no controller worker or cross-process ABI
descriptor. The existing hook protocol remains unchanged. File-version resource
lookups and runtime PDB parsers are not part of admission.

## Accessor rule 1

The inspected `IFileExplorerTabActions2` has no initial enumeration or selected-
GUID getter. `RegisterTabEventsHandler` delegates to registration machinery; no
initial replay is established. Events cannot reconstruct a pre-existing source
group. No reviewed public COM/QueryInterface route replaces the initial snapshot.

Rule 1 recognizes complete primary unwind ranges of `GetTabDataById` and
`GetBrowserTabFromId` using reviewed SHA-256 fingerprints. Normalize only known
relative references, three object-access displacement operands, and four reviewed
`Return_Hr` diagnostic immediates at data offsets `0x1ED`, `0x20D`, `0x238`, and
`0x27C`. These diagnostic operands are source-location metadata, not layout or
ownership data. Their surrounding instructions remain fingerprinted. Preserve
the remaining instructions, branches, 40-byte browser records/GUID fields, and
32-byte tab-data output layout. Both output pointers use task-allocator ownership.
The masks are an explicit code-shape contract, not a general x64 byte scanner.

After recognition, derive the actions adjustment, vector begin/end fields, and
selected-browser field. Check alignment, bounded positive offsets, adjacent
vector pointers, non-overlapping active storage, and the actual helper-call
target. No fixed object offsets are copied from the versioned map. Relinking and
field displacement changes can share this rule; an unfamiliar compiler shape
cannot, even if its symbol names remain familiar.

The thirteen-slot interface, native drop entry point 4, private callees,
allocator and drop semantics remain private compatibility contracts. Fingerprints
do not formally prove every transitive behavior. The current image is the only
live-verified implementation; byte-buffer tests for relocated functions and new
identities do not establish support for another Windows release.

Static inspection of exact public symbols and images for `10.0.26100.9818` found
the same ordered thirteen-slot signatures and accessor shape, with only those
four diagnostic immediates differing after normalization. It is not registered
for production without scoped live acceptance. `10.0.26100.1` has the same slot
signatures but a different data-accessor extent and object fields; it requires
separate rule review, not a nearby-version or shifted-RVA fallback.

## Adding support for another component version

1. Obtain the exact image and matching public PDB in a development environment.
   Record file version, CodeView GUID/age, image size, and symbol provenance.
2. Inspect the window/tab-actions vtables and all thirteen method signatures,
   their order/extent, `this` adjustments, snapshot ownership, and native drop
   behavior. A function name alone is insufficient evidence.
3. If the accessor contract matches an existing rule, add only
   `abi_profiles/<VERSION>.h` and register it in `native_abi_profiles.cpp`.
   Record every RVA from that exact image; do not shift an older map by a delta.
4. If code shape changes, add a reviewed rule with its own full boundaries,
   normalization operands, derivation, and negative tests. If record layout,
   signature, extent, or ownership changes, update the adapter contract first.
   Do not broaden an existing rule with wildcard hashes or version-range bypasses.
5. Run C++17 builds and CTest, including catalog selection, unknown-map/rule
   rejection, damaged accessor, relocation, and field-derivation cases. Validate
   actual add/select/remove, current-folder `+`, pre-existing multi-tab sources,
   and maximized windows using the directory-scoped
   [Explorer testing procedure](../guides/EXPLORER_TESTING.md).
6. Claim compatibility only for the images and routes actually tested. Preserve
   destination GUID/PIDL confirmation and source revalidation in every rule.
