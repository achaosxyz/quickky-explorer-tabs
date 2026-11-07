# Release Preview and Publication

## Contract

Development happens on `develop`. `VERSION` contains the version being prepared.
The application is `QTabs.exe`; its adjacent integration library is
`QuickkyExplorerHook.dll`. The downloadable archive is `QTabs_vX.X.X.zip`.
The ZIP includes the matching MSVC redistributable DLLs selected by CMake's
`InstallRequiredSystemLibraries`. Extract the complete archive; verify smoke
and scoped Explorer loading on an OS without a globally installed VC++ runtime.

There are two procedures below. **Preview** presents release notes in the
conversation for maintainer confirmation; no standalone file is required.
It does not build, change refs, or create a GitHub prerelease.
**Release** promotes `develop` to `main`, runs Windows smoke checks,
publishes the tested ZIP through GitHub Actions, and then increments `develop`.
Publishing requires a separate, explicit `mode=release` dispatch.

Before hosted build/smoke, `main` advances to the selected source commit. Only
after successful publication does `develop` receive a version-only commit.
"Next minor" means SemVer MINOR + 1,
PATCH = 0: releasing `0.0.1` starts `0.1.0` development. `main`, the tag, ZIP,
and release resources retain `0.0.1`.

## Prerequisites

- Commit and push the intended changes to `develop`; local uncommitted work is
  never part of an Actions release or its commit-based notes.
- Keep `main` an ancestor of the selected `develop` commit. If necessary, merge
  current `main` into `develop`, resolve conflicts, and validate before dispatch.
- The release workflow file must exist on the default branch for GitHub manual
  dispatch. For initial setup, land the workflow infrastructure on `main` through
  the maintainer-approved bootstrap process before the first release dispatch.
  Bootstrap must be a coherent buildable commit, including the scripts and their
  CMake/source inputs, rather than workflow YAML alone.
- Enable GitHub Actions. The publishing job requests `contents: write`; preview
  and smoke jobs request only `contents: read`. No PAT or stored release secret
  is required. Repository/ruleset protections must allow the intended bot updates.
  The scripts do not bypass protections or change repository security settings.
- GitHub-hosted `windows-2022` uses its installed Visual Studio 2022 compiler in
  C++17 mode. This does not replace the verified local VS 2017/v141 build path.
- Local scripts require Windows PowerShell 5.1 or PowerShell 7, Git, CMake 3.21+,
  MSVC C++17, and an installed Windows SDK. GitHub CLI is required only to dispatch,
  inspect, or publish remotely.

Do not describe the resulting release as generally compatible Windows 10/11
integration. The current application remains experimental. Smoke checks do not
exercise real Explorer attachment, native-tab migration, or recovery.

## SOP 1: Preview

1. Switch to `develop`, inspect the worktree and version, and review the intended
   source changes. Do not discard uncommitted work to obtain a clean tree.

   ```powershell
   git switch develop
   git status --short
   Get-Content VERSION
   ```

2. Present the proposed English release notes in the conversation and obtain
   maintainer confirmation. Include the version, features, download filename,
   and known limitations. Do not publish or create a preview file for this step.
   Pass the approved body as the `release_notes` dispatch input when publishing.

   For optional inspection of commit-based notes and the pinned source plan,
   the internal automation helper writes ignored intermediate artifacts:

   ```powershell
   ./scripts/preview-release.ps1
   Get-Content build/release/RELEASE_NOTES.md
   Get-Content build/release/release-plan.json
   ```

   Notes list non-merge commits since the latest reachable lower `vX.X.X` tag.
   For the first release they list the source history. The plan records the exact
   source SHA, release version/tag, ZIP name, and next development version. Local
   preview warns about uncommitted changes; it does not include them in the notes.

3. Optionally, after the intended changes are committed and explicitly pushed,
   obtain the hosted notes-only preview:

   ```powershell
   gh workflow run release.yml --ref develop -f mode=preview
   gh run list --workflow release.yml --limit 5
   gh run view RUN_ID
   gh run download RUN_ID --name release-preview --dir build/hosted-preview
   ```

   Replace `RUN_ID` with the selected run. Review its summary, notes, source SHA,
   and version. The workflow uses the selected commit, not a later moving
   `develop` tip. Preview does not compile, reserve a tag, merge branches, bump
   `VERSION`, or create a public/draft release. Local packaging is a separate
   development operation, not part of preview.

## SOP 2: Release

1. Review the preview and validation evidence. Follow
   [Validation](VALIDATION.md) for affected code; the hosted minimum is not a
   substitute for a full review. Freeze `develop` while publication is running.
2. Explicitly dispatch publication from `develop`:

   ```powershell
   gh workflow run release.yml --ref develop -f mode=release
   gh run list --workflow release.yml --limit 5
   gh run watch RUN_ID --exit-status
   ```

   Include `-f release_notes="$approvedNotes"` on the dispatch when notes were
   confirmed in the conversation. Supply their text directly, not an additional
   file. Without this input, automation generates commit-based notes. The tested
   artifact carries the selected body through publication and retry checks.

3. The promotion job first verifies the exact `develop` SHA and fast-forwards
   `main` to it without a force push. Only after that succeeds does the read-only
   preparation job check scripts, generate notes, compile the application, run
   CTest `app.smoke` and the extracted-package smoke, and upload the candidate
   artifact. The write-enabled job consumes that same
   artifact and checks the exact source SHA/version and current branch state.
4. Publication creates `vX.X.X` at the tested commit, uploads the ZIP to a draft,
   and publishes only after upload succeeds. It then confirms publication and
   atomically keeps `main` pinned to that source while updating `develop` with
   the minor-version commit. Neither branch is force-pushed. Changes to either
   branch can stop finalization without partially updating the other branch.
5. Verify all three results, not merely the existence of the GitHub Release:

   ```powershell
   gh release view vX.X.X
   git fetch origin --tags
   git log -1 origin/main
   git show origin/develop:VERSION
   ```

   The tag and `origin/main` must identify the tested source, the release must
   contain `QTabs_vX.X.X.zip`, and `origin/develop` must contain the next minor
   version. Do not increment the local `VERSION` a second time.

GitHub-token branch pushes do not automatically trigger another push workflow.
The release preparation job therefore performs its own mandatory smoke checks;
there is no reliance on a subsequent `main` push run or recursive release trigger.

## Failure inspection and recovery

- **Promotion failure:** no build, tag, release, or version bump is performed.
- **Build/smoke failure:** no tag, release, or version bump is made. `main` stays
  at the selected candidate; automation never rewinds it. Fix the source on
  `develop`, review it, generate a new preview, and promote the corrected candidate.
- **Draft/upload failure:** inspect the existing draft and its assets. Automation
  will not overwrite or automatically publish an existing draft. A maintainer
  must reconcile it against the tested artifact before taking further action.
- **Published release, branch update failed:** the public release is not rolled
  back or deleted. Inspect protections and both remote tips. If `develop` still
  equals the tested source, rerun the original failed publication job/run. It
  verifies the existing tag, notes, asset name and ZIP SHA-256 digest, then retries only the missing
  branch finalization. Use the original source run, not a new dispatch at another
  commit. Rerun only failed jobs so the tested artifact is reused; rebuilding can
  change its bytes/build timestamp. A finalized run is a verified no-op while
  both branch tips still match its release and version bump. Older runs stop
  rather than attempt to move branches that have since advanced.
- **Concurrent development or divergent main:** stop and reconcile deliberately.
  Never reset/force-push a branch or repoint an existing release tag. A rerun
  refuses unrelated newer development commits instead of losing them.

Synthetic script checks use fixture repositories beneath `build/release-tests/`
and a mocked GitHub CLI. They cover version validation, publish gating, successful
and repeated finalization, failed draft publication, and concurrent-branch
protection. They do not publish anything to GitHub. A real hosted release and
repository-specific ruleset behavior must be reported as unverified until run.
