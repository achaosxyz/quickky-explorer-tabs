# Windows 11 Native ABI Adaptation

## Scope and status

This document owns native `ExplorerFrame.dll` compatibility detection.
[Explorer adapters](EXPLORER.md) owns tab operations and
[Hook integration](HOOK.md) owns attachment. Windows 10 does not use these rules.
The application uses compiled-in maps only: no symbol downloads, PDB loading,
symbol cache, or background resolver. Exact public PDBs are development evidence,
not an application dependency.

The catalog covers the exact x64 images below, inspected with matching public
PDBs. Each map supplies all thirteen typed entries and full-entry fingerprints:

| Component version | CodeView GUID (age 1) | Accessor rule | Validation boundary |
| --- | --- | --- | --- |
| `10.0.26100.9549` | `822A8C93-960A-C7B6-7F6A-87CF48E8CE6F` | 1 | Scoped live evidence and offline checks |
| `10.0.26100.9818` | `468742F3-D41C-3441-A502-2FBFFEF9A656` | 1 | Exact-image/PDB and offline checks; no live acceptance yet |
| `10.0.26100.1` | `3F594208-71FE-A9BE-3371-575498A24FC7` | 2 | Exact-image/PDB and offline checks; no live acceptance yet |
| `10.0.28000.3086` | `7B7F6EF1-F61E-1768-4DBD-B199F7981957` | 3 | Exact-image/PDB; no live acceptance yet |
| `10.0.28000.2605` | `46334190-881B-8CCE-8CBB-A6D18E3809CD` | 4 | Exact-image/PDB; no live acceptance yet |
| `10.0.28000.2525` | `FF6575FC-8454-293D-682E-0CF7B3DFBB6A` | 4 | Exact-image/PDB; no live acceptance yet |
| `10.0.28000.1761` | `433C17BE-CFB4-C1B3-19C2-626768F06589` | 4 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.9278` | `E4B44D51-365E-4FA3-AA9B-BAD5CA3BB93A` | 1 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.8972` | `F8DF25B5-96A8-B9B9-8314-A107AAADB0B3` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.8737` | `25F6494D-2336-5391-5E3B-C33ED41C9C3B` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.8655` | `02B1900B-20AF-6948-BE5E-5C0EC76FCCAC` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.22621.5413` | `A4E52999-241E-E1DB-266F-228962F81BC2` | 5 | Exact-image/PDB; no live acceptance yet |
| `10.0.22621.5331` | `DB6F325F-FA06-4F1F-A770-CD1A20D1C6EF` | 5 | Exact-image/PDB; no live acceptance yet |
| `10.0.22621.5262` | `36FCC227-5BB4-12D5-9C6D-4455460E9EEB` | 5 | Exact-image/PDB; no live acceptance yet |
| `10.0.22621.3958` | `B03A2527-BB6D-FC57-1C50-CC6538607749` | 5 | Exact-image/PDB; no live acceptance yet |
| `10.0.22621.4541` | `C07E8BBF-5F04-A49B-C02A-A07CA755B9A7` | 5 | Exact-image/PDB; no live acceptance yet |
| `10.0.22621.4601` | `074624E6-B9F6-E0F1-4F92-AF0FB3896551` | 5 | Exact-image/PDB; no live acceptance yet |
| `10.0.22621.4746` | `2819EF18-7F81-DA5A-610E-79AF352B87EC` | 5 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.7309` | `A7E04BA1-6DB3-EA3E-7043-D341B60F4F82` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.7462` | `A13F5397-7AE4-71D4-781B-9F6BCA709529` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.7705` | `5E537B80-9194-8BD8-B43A-056AB12076DD` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.7824` | `11C1E947-47AD-6FFD-F761-97572130AF1C` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.7920` | `F66372C6-49A7-230C-8B9A-EF22A0EDEC7E` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.8115` | `FAEC0B07-5559-383C-8212-CAC3401CAD92` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.8328` | `216177BC-DEB0-3937-01AF-EDAE6EBFD9FD` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.8457` | `E183F37E-71AB-7E36-957E-E0D4E284FC46` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.8521` | `A610550A-3A1D-79D6-B896-A3F72E55B3B2` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.22621.3672` | `185040F1-2C18-D327-8B3C-FDE3174F020F` | 5 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.5074` | `37BCC217-328C-E5F2-CDF1-3FD9CDEBF286` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.6725` | `8CCA1CF2-38BF-0C29-ADD1-8BAE72943DC6` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.7019` | `D79124F2-7E3B-B690-9994-67B1EBA077C8` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.7171` | `5BE6EC1F-C1CF-AC7E-0FFD-515440221BB0` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.22621.5909` | `4203F893-8472-64F2-8852-84745243E287` | 5 | Exact returned image/PDB; no live acceptance yet |
| `10.0.22621.6197` | `608B122F-1A03-6CED-EEE8-74D51C41129C` | 5 | Exact returned image/PDB; no live acceptance yet |
| `10.0.26100.4202` | `20906462-08A7-1F46-2362-BC04AD41C0A8` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.4768` | `52C60A00-9553-E194-8EED-0785A24A111C` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.4484` | `0A714E7F-550C-BABA-0855-5DC4EC85B646` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.2454` | `6CB1D2F3-D0A3-1FC1-E1DA-DB5CE35E153B` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.2605` | `8FACD8F0-1357-7EFB-B125-47A2F87A551E` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.2894` | `81D83040-D08C-49A1-D465-25EFB7709FCA` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.3037` | `F1622050-0830-7630-7B8B-8D380C549587` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.3323` | `D59DFDC7-94C4-CEB5-3E87-367B410874C2` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.3624` | `6A3DAAC9-EE88-C266-867C-DEBB1164F51D` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.3912` | `07EF89E6-E05F-4210-B22A-237B4471F1E0` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.4061` | `8CA4DFEC-47E5-CDA1-4CB4-E4E8511CFDB5` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.1150` | `3612DF9E-C90C-DB9A-7E94-A4DD3FFF1381` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.1455` | `75742287-3EBA-42FE-431F-41EF8EFCF141` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.1591` | `BCB202AE-253F-E88E-3AC7-B4DABD6A2840` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.1882` | `56F6CEA4-412A-A28D-AF3D-1A60F97A601B` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.2161` | `D03E0FF8-D98E-A150-5EE7-860D9F6A997F` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.2314` | `4EA07794-D106-15D7-F665-B0FD19480FDD` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.1000` | `83D07B8B-0415-7FDE-BA8A-4AE8667E9581` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.26100.712` | `14B05528-EBAF-508C-ADCD-3B2D93361E5E` | 2 | Exact-image/PDB; no live acceptance yet |
| `10.0.22621.5983` | `15DA11F4-AC58-2136-0379-7359A5CA49DC` | 5 | Exact-image/PDB; no live acceptance yet |

This is not a claim that every Windows 11 component version is supported.

The current catalog contains 54 exact images: 52 indexed candidates and two
independently identified server-returned images. The development inventory covers
92 indexed candidates; the other 40 comprise nine images lacking the required
interfaces, four deliberately excluded accessor contracts, and 27 without
verified exact-image evidence. An index label or failed acquisition is not a map.

The inspected `10.0.22621.317`, `.608`, `.675`, `.755`, `.900`, `.1105` and
`.1344` images have the older `IFileExplorerTabActions` interface, not the thirteen-slot
`IFileExplorerTabActions2` contract. Their exact
symbols expose `AddTab`, `RemoveTab` and `SelectTab`, but not the native tab-drop,
`AddTabAtIndex` or `MoveTabToNewWindow` primitives used by this adapter. Do not
register fabricated thirteen-slot maps for them or silently substitute path
reopening. They remain unsupported under this transfer contract.

The exact `10.0.22000.348` and `10.0.22000.708` images expose neither actions
interface nor the required transfer methods. They also remain unsupported;
an absent lookup result alone is not evidence of missing transfer capability.

Map versions refer to the executable image's fixed version resource. A localized
shell property can instead report a newer MUI/display version: on the tested
host it reports `26100.9949`, but the main DLL's fixed version and complete SHA-256
match the indexed `26100.9549` image. Never use that merged display value to order
private-code compatibility candidates.

A symbol-server response that differs from the requested index hash is not
evidence for the requested version. An independently inspected returned image
may have its own map under its actual fixed version and CodeView identity;
retain the original request's mismatch instead of relabeling it as successful.

## Versioned maps and reusable rules

- `src/explorer/abi_profiles/profile.h` defines image identity, locator data,
  accessor-rule ID, and a human-readable component version.
- `src/explorer/abi_profiles/<VERSION>.cpp` is the implementation unit for that
  exact component version. It owns its immutable reviewed map:
  CodeView GUID/age, image size, window/tab-actions vtable RVAs, lookup RVA,
  thirteen typed method RVAs, and its accessor-rule ID. Full-entry fingerprints
  and the numeric component version additionally constrain fallback selection.
  Each unit exposes one profile getter. The `9549` unit owns accessor rule 1,
  reused by `9818` and `9278`; the `1` unit owns accessor rule 2, reused by
  `8655`, `8737` and `8972`, and the `28000.3086` unit owns rule 3. The `28000.2605`
  unit owns rule 4, reused by `28000.2525` and `28000.1761`.
  The `22621.5413` unit owns rule 5, reused by the other mapped `22621` images.
  Do not copy the controller,
  protocol, storage checks or tab-operation code into each version unit.
- `src/explorer/native_abi_profiles.cpp` registers the versioned map files.
- `src/explorer/native_abi.cpp` implements common metadata, candidate ordering,
  hashing and storage checks. `native_abi_internal.h` is the private boundary
  between those checks and the version units' accessor recognizers. Maps share
  a recognizer when their accessor code/storage contract is unchanged.

Version labels organize maps, not permission to call an interface. Prefer an
exact CodeView identity/size match and verify its accessor rule. If identity is
unknown, use the actual loaded module's fixed version resource to consider only
same-or-lower numeric versions, nearest first, under the stronger fallback checks
below. There is no download, shifted-RVA search, or unconditional version-range
admission.

## Binding flow

The tray's explicit compatibility check uses the same catalog/contract selector
with `LOAD_LIBRARY_AS_IMAGE_RESOURCE` alone to require an image layout. It checks
the returned handle tag before any RVA reads, rejects raw-data mappings, and uses
the preferred base for an image resource or the actual base when Windows retains
an already-loaded executable module. The reported fixed version always belongs
to the checked mapping, which may precede a pending on-disk update. It releases
its mapping/reference immediately. It does
not execute DLL code, parse symbols, inject, or infer live transfer success.
This diagnostic is separate from target binding, which always checks the actual
module loaded by the selected Explorer thread.

1. On the owning Explorer UI thread, inspect the actual loaded module's bounded
   PE metadata and CodeView identity. Do not base selection on a newer on-disk DLL.
   Require exactly one valid RSDS record with nonzero GUID/age; malformed or
   duplicate records reject the image and leave the output identity empty.
2. Prefer a matching identity in the compiled catalog. Otherwise consider
   same-or-lower versions, nearest first, using the read-only fallback gate. A
   failed candidate must not prevent a later valid candidate from being checked.
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
descriptor. The existing hook protocol remains unchanged. The version hint comes
only from the loaded module, never a newer on-disk DLL. Runtime PDB parsers are
not part of admission.

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
do not formally prove every transitive behavior. Only `10.0.26100.9549` has scoped
live evidence; offline checks do not establish end-to-end operation on the other
Windows installations. `10.0.26100.9818` and `10.0.26100.9278` share rule 1,
including its reviewed diagnostic operands. The `9278` image derives tabs
`0x1308` and active `0x1320`, unlike the `9549/9818` layout.

## Accessor rule 2

The exact `10.0.26100.1` symbols retain all thirteen ordered signatures. Its
`GetTabDataById` primary range is `0x2F5` bytes instead of `0x2CD`; it obtains the
title through an interface dispatch rather than a direct browser-host call.
`TabDataFree` still frees the outputs at `+0x10/+0x18` using `CoTaskMemFree`, and
the output is still 32 bytes. The lookup body retains the 40-byte record/GUID
contract and shares rule 1's reviewed lookup fingerprint.

Rule 2 has its own complete data-accessor fingerprint and explicit relative,
displacement and diagnostic operands. It derives the actions adjustment from
`+0x4A`, checks the helper call at `+0x60`, and reads the active-field displacement
at `+0xAC`. Its observed layout is actions `0xE8`, tabs `0x198`, active `0x1B0`;
these values are derived from code, not blindly installed from the version label.
The exact `26100.8737` and `26100.8972` images also match rule 2 and retain the
thirteen typed signatures, but derive tabs `0x1308` and active `0x1320` instead.
The exact `26100.8655` image shares this rule with tabs `0x208` and active `0x220`.

## Accessor rule 3

The exact `10.0.28000.3086` image retains all thirteen ordered signatures, the
40-byte record lookup contract and 32-byte output. Its primary data-accessor
extent is `0x2BF`; it uses direct title retrieval and task-allocator ownership.
The root adjustment operand is at `+0x3D`, the lookup-call operand at `+0x4A`
(relative to `+0x4E`), and the active-field operand at `+0x8E`. This different
instruction shape must not be admitted by rule 1 or 2. A complete independent
fingerprint recognizes it before shared storage checks derive actions `0xE8`,
tabs `0x1308` and active `0x1320`. The lookup still recognizes the shared `0xE3`
body and derives root vector fields `0x13F0/0x13F8`.

## Accessor rule 4

The exact `28000.2525` and `28000.2605` images share a `0x2EA` primary data
body and the thirteen-slot signatures. Their title retrieval uses the interface
dispatch at slot `+0x28`, with the same 32-byte output and task-allocator cleanup.
Rule 4 independently recognizes that body, then reads root adjustment `+0x3D`,
lookup rel32 `+0x4A` and active displacement `+0xA2`. The reviewed diagnostic
operands are `0x20A/0x22A/0x255/0x299`.

Despite the shared body, layouts differ: `2525` derives actions/tabs/active
`0xE8/0x258/0x270` from root vector `0x340/0x348`; `2605` derives
`0xE8/0x1308/0x1320` from `0x13F0/0x13F8`. This is why profiles cannot install
another version's object offsets. The recognized code, not revision proximity,
must derive and validate the fields.
The exact `28000.1761` image also matches rule 4; its independently derived
actions/tabs/active layout is `0xE8/0x1A8/0x1C0`, not either newer layout.

## Accessor rule 5: 22621 split lookup

The exact `22621.5413`, `22621.5331` and `22621.5262` images expose the same
thirteen typed Actions2 entries. Their public lookup symbol is spelled without
parameter decoration; development probes must accept that exact bare name as
well as the decorated form. The original probe's missing-locator result was
not missing transfer capability.

Rule 5 recognizes a complete `0x347` data accessor and a complete `0xCF` lookup.
The data root adjustment is at `+0x6E`, lookup-call operand at `+0x86`, and
selected-browser operand at `+0x117`. Diagnostic immediates are
`0x45/0xA3/0x166/0x265`. The output remains 32 bytes with task-allocated title
and PIDL fields at `+0x10/+0x18`.

The lookup derives its vector pointers from `+0x17/+0x21`. It retains the
40-byte record with browser/site pointers, GUID at `+0x10`, and flags at `+0x20`.
Its not-equal branch at `+0x41` reaches a separate nine-byte cold fragment:
`add rbx,0x28; jmp rel32`. Validate its executable/unwind bounds, complete
normalized body and exact backedge to lookup `+0x28`; validating only the hot
range would leave the record stride unproven. These images derive actions
`0xE8`, tabs `0x1A0`, and active `0x1B8`.

The inspected compact accessor layouts in `10.0.22621.2215`, `10.0.22621.2361`,
`10.0.22621.2792` and `10.0.22621.3085` are intentionally outside the supported
catalog. Their transfer interfaces exist, but their data/lookup contracts do not
match rule 5. Do not admit them by version proximity or reuse offsets from a
different shape.

## Conservative nearest-version fallback

An unknown CodeView identity may use a candidate only after read-only validation:

1. Read the numeric four-word version from the loaded module's fixed version
   resource. Missing/malformed version metadata disables fallback, not exact
   identity matching. Never read a replacement DLL from disk for this hint.
2. Consider only same-or-lower catalog versions, nearest first. Do not try newer
   versions or mutate Explorer while testing a candidate.
3. Require the candidate's original vtable/method RVAs and all thirteen complete
   entry bodies to match their stored fingerprints. Require recorded unwind
   boundaries for non-leaf entries; reviewed leaf thunks include their complete
   jump/return. Only the reviewed data-accessor diagnostic immediates may differ
   in this extra check: do not wildcard relative references or object offsets.
4. Rebind the descriptor to the inspected identity only after the entry check;
   then validate its accessor rule, lookup target, sections and derived fields.
   A failed candidate permits checking the next lower candidate.
5. Return no adapter if no candidate passes. File-version proximity alone never
   authorizes a private call. Moved functions or unfamiliar bodies need a new
   development-time map; this is not universal forward compatibility.

Fallback recognizes an already reviewed code contract, not an arbitrary private
ABI from names alone. Transitive private behavior remains a compatibility risk;
keep destination confirmation, source revalidation and timeout quarantine.

## Adding support for another component version

1. Obtain the exact image and matching public PDB in a development environment.
   Record file version, CodeView GUID/age, image size, and symbol provenance.
   Use the recorded verified image path: a rejected or truncated payload may be
   retained separately. Recheck provenance before generating a map.
2. Inspect the window/tab-actions vtables and all thirteen method signatures,
   their order/extent, `this` adjustments, snapshot ownership, and native drop
   behavior. A function name alone is insufficient evidence.
3. If the accessor contract matches an existing rule, add only
   `abi_profiles/<VERSION>.cpp` and register it in `native_abi_profiles.cpp`, including
   exact full-entry fingerprint lengths/boundaries and the numeric version.
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
   `QuickkyNativeAbiTests.exe --image PATH\ExplorerFrame.dll` additionally checks
   an explicitly supplied reviewed image: its exact map, derived layout,
   fallback rejection/selection, each typed-entry fingerprint, fixed version
   resource and ASLR relocation. The test process does not initialize that DLL,
   invoke its private interfaces or attach to Explorer. Downloaded images/PDBs
   stay in ignored research output, not the product package or source tree.
6. Claim compatibility only for the images and routes actually tested. Preserve
   destination GUID/PIDL confirmation and source revalidation in every rule.
