# Phoxtra OS Prototype Completion and Release Gates

**Status:** Active completion plan. This document distinguishes work that can be advanced in CI from tests that require physical hardware.

## What “complete” means for this milestone

The first milestone is a **completed, documented, testable x86_64 prototype**, not a claim that every planned OS capability or mobile version is production-ready.

A milestone is complete only when the evidence below exists. Each line must be marked **Passed**, **Failed**, **Blocked**, or **Not implemented**; do not silently treat an unimplemented feature as passed.

## Gate A — Reproducible build and automated regression checks

- [ ] Default x86_64 kernel and ISO build successfully.
- [ ] Desktop and embedded product profiles build successfully.
- [ ] Storage-safety audit passes.
- [ ] QEMU reaches the shell and desktop event loop.
- [ ] Theme switching, Application Center, Workspace catalogue access, mouse/file-manager interaction, RAM-backed file/folder creation, and screenshot sanity checks pass.
- [ ] Hardware readiness report is exercised; its output clearly distinguishes PCI inventory from real driver support.
- [ ] ISO SHA-256 file is generated and can be verified against the downloaded ISO.
- [ ] CI results and artifact commit are recorded in the completion log.
- [ ] No release claim implies physical-device validation from QEMU-only evidence.

## Gate B — Prototype behavior and scope accuracy

- [ ] Native panels marked available open the interface actually implemented.
- [ ] Planned external applications are labelled as planned/pending.
- [ ] Amineva Workspace integration status is explicit: the current catalogue entry is not a working suite.
- [ ] Volatile RAM storage, the simulated read-only disk, and loss of files on reboot are clearly disclosed.
- [ ] Installer and formatting remain disabled until a separately reviewed persistent-storage implementation exists.
- [ ] No physical internal disk is written during discovery or prototype testing.
- [ ] Known limitations are present in README, release notes, and funding/demo descriptions.
- [ ] Cursor artwork remains a separate task; do not fold it into unrelated release work.

## Gate C — Physical standard-laptop acceptance

**Current status: Not yet performed. This gate cannot be completed by repository edits or QEMU CI.**

When the owner is ready, use [the standard-laptop validation checklist](../roadmap/laptop-validation-checklist.md) and record the laptop model/specifications, exact ISO commit and SHA-256, boot method, display, keyboard, pointer/touchpad, desktop interactions, repeat boots, stability, and observed limitations.

- [ ] Boot reaches the prototype on a physical laptop from USB.
- [ ] Display and supported input devices work reliably enough for the declared prototype scope.
- [ ] Desktop interactions and repeated boot behavior are recorded.
- [ ] Unsupported devices are distinguished from regressions.
- [ ] Release-blocking defects are fixed and retested.
- [ ] Test evidence is saved and the checklist is explicitly marked passed.

Do not install to or format the laptop's internal disk. The current prototype has no working physical persistent-storage driver or installer.

## Gate D — Amineva Workspace and web application access

**Current status: Catalogue-only entry; actual integration not implemented.**

- [ ] Choose the initial access model for Workspace: browser-based service, a maintained embedded web runtime, or a future native client.
- [ ] Confirm that a usable browser/network/TLS/certificate path exists before relying on web URLs. A URL in a catalogue is not an application runtime.
- [ ] Define the actual Workspace service URL(s), authentication redirect/callback flow, session handling, logout, and failure/offline states.
- [ ] Decide how Docs, Sheets, Slides, Forms, and Drive hand off files; test permissions and cross-app behavior.
- [ ] Test upload/download, unsupported file types, expired sessions, unavailable service, and reconnect behavior.
- [ ] Do not advertise offline operation unless it is implemented and tested separately.
- [ ] Test each linked service end-to-end against a real deployment before marking it available.

The operating system cannot create a working Workspace integration solely from the catalogue metadata. A browser/web runtime, networking, certificate validation, and application-specific account/file integration are prerequisites.

## Gate E — Distribution and draft release

- [ ] Select a version identifier using semantic-style tags, for example `v0.2.0-alpha.1`; do not imply stable production status.
- [ ] Run the build from a reviewed commit and confirm all required CI jobs passed.
- [ ] Download the ISO and checksum from the same artifact/release, verify SHA-256, and record the commit SHA.
- [ ] Prepare release notes, limitations, hardware expectations, test instructions, and known issues.
- [ ] Create a **draft**, unpublished GitHub Release first. Do not publish automatically from a normal push or pull request.
- [ ] Review the draft on the actual distribution page before choosing whether to publish.
- [ ] Keep a backup of the exact ISO, checksum, release notes, and source commit.
- [ ] Configure a stable download URL or owned storage only when ready to support it; CI artifacts expire.

The manual workflow has an explicit opt-in to create an unpublished draft. It does not make a public release automatically. The draft tag must be unique and should be created only after the version is reviewed.

## Gate F — Production OS (separate, larger milestone)

A completed prototype is not a production OS. Before general-purpose daily-use claims, plan and implement at least:

- A robust memory/interrupt/process model and clear kernel/user-space boundaries, verified through targeted tests.
- Persistent disk drivers and a real on-disk filesystem with safe mount/write semantics, corruption handling, and recovery.
- A reviewed installation/update/recovery model and rollback strategy.
- Supported device-driver matrix and tested graphics/input/network/storage behavior.
- Browser or application runtime, networking, TLS/certificate validation, and usable external application integrations.
- Application packaging, dependency management, integrity verification, permission enforcement, and update strategy.
- Security review, threat model, vulnerability reporting, reproducible build/release process, and backup/recovery procedures.
- Physical hardware compatibility testing and extended stability testing.

The precise implementation sequence must be based on source-code audits and test evidence; do not mark the existing kernel as meeting these items merely because modules or file names exist.

## Gate G — Dedicated Phoxtra hardware and mobile

Phase 8 target-board work remains gated on the physical laptop milestone and actual target selection. A board specification must cover firmware, CPU, RAM, display, input, storage, power, thermal behavior, peripherals, and independent recovery.

Mobile phones are a separate architecture and product effort. A mobile build requires selecting an ARM64 target, boot/firmware path, display/touch/input drivers, power management, networking, storage, security model, and an application strategy. The current x86_64 ISO must not be represented as a phone OS.

## Completion log template

| Date | Commit / version | Gate | Status | Evidence / issue | Owner / next action |
|---|---|---|---|---|---|
| | | A — CI | | | |
| | | B — scope | | | |
| | | C — laptop | Not yet performed | | |
| | | D — Workspace | Not implemented | | |
| | | E — draft release | | | |
| | | F — production | Not complete | | |
| | | G — dedicated hardware/mobile | Not complete | | |
