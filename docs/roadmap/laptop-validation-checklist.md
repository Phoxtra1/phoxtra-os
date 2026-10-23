# Phoxtra OS Standard-Laptop Validation Checklist

**Purpose:** Validate the integrated Phoxtra OS on a standard laptop before any first dedicated Phoxtra hardware build.

This checklist is a test record. Do not mark a test passed unless it was performed on the physical laptop and its result was observed.

## Automated preflight evidence (CI only)

- Workflow run: https://github.com/Phoxtra1/phoxtra-os/actions/runs/39895758147
- Commit: `7422912b2c0c4514c42d79f1087919a553cdd2bd`
- CI conclusion: **Passed** on 2026-10-23.
- Passed in CI: kernel build; desktop and embedded product-profile builds; ISO generation and non-empty artifact verification; QEMU boot and framebuffer checks; theme switching; Application Center and Workspace catalogue access; functional mouse click and file-manager navigation; RAM-backed file/folder creation; screenshot rendering checks; ISO checksum generation and artifact publication.
- Published ISO artifact: `phoxtra-os-iso-7422912b2c0c4514c42d79f1087919a553cdd2bd` (available from the workflow run's Artifacts section).
- Physical laptop validation: **Not yet performed**. CI results do not pass Phase 7's hardware acceptance gate.
- Pointer artwork redesign: **Deferred as a separate task**. Do not change the pointer artwork during Phase 7. Still record basic pointer/touchpad functionality during laptop validation; report visual artifacts as observed rather than silently treating them as passed.

## 1. Test machine and media

- Date/time of test:
- Tester:
- Laptop manufacturer/model:
- CPU:
- RAM:
- Graphics/display hardware:
- Firmware mode (UEFI/legacy, if known):
- USB media capacity:
- Phoxtra OS commit / artifact name:
- ISO SHA-256:
- Test result: Not started / In progress / Blocked / Passed

## 2. Prepare a bootable test USB

1. Open the successful Phoxtra OS build run in GitHub Actions and download the artifact named `phoxtra-os-iso-<commit>`.
2. Extract `phoxtra.iso` and `phoxtra.iso.sha256` from the artifact.
3. On Windows, verify the checksum in PowerShell with:
   ```powershell
   Get-FileHash .\phoxtra.iso -Algorithm SHA256
   ```
   Compare the displayed hash with the value in `phoxtra.iso.sha256`. Stop if they differ.
4. Use Rufus or a comparable USB imaging tool to write the ISO to a spare USB drive. Writing the image erases that USB drive; confirm the selected device carefully.
5. Back up important laptop data and save work. Use the laptop's one-time boot menu to boot from the USB. Do not choose any option that formats or installs to the laptop's internal drive.
6. At the Phoxtra shell prompt, type `gui` and press Enter to open the desktop.

## 3. Boot and hardware checks

For each item, record **Pass**, **Fail**, **Blocked**, or **Not supported**, plus notes.

- [ ] USB boot reaches Phoxtra OS without hanging or rebooting repeatedly.
- [ ] Display initializes and the desktop/framebuffer is visible.
- [ ] Screen resolution and visible desktop bounds are sensible.
- [ ] Keyboard input works.
- [ ] Touchpad or mouse works, where supported.
- [ ] Pointer moves smoothly and leaves no trails, ghost pixels, or stale cursor images.
- [ ] Repeated input does not freeze the system or corrupt the display.
- [ ] Shutdown/reboot behaviour is observed and recorded.

## 4. Desktop and interaction checks

- [ ] Taskbar logo and PHOXTRA typography are clear and correctly placed.
- [ ] Press `1`, `2`, and `3` to test Midnight Gold, Cyber Sapphire, and Emerald Dark themes.
- [ ] Theme changes update the desktop, windows, taskbar, and relevant accents without visual corruption.
- [ ] File manager opens and renders its directory listing.
- [ ] Folder navigation and returning to a parent directory work.
- [ ] File selection and file opening/preview behaviour work where implemented.
- [ ] New Folder and New File keyboard actions work where available.
- [ ] Window drag, close, minimize, and maximize/restore controls work.
- [ ] Taskbar/dock actions restore the expected window.
- [ ] No overlapping, clipped, or stale UI remains after interactions.

## 5. Application environment and integrated workspace

- [ ] Press `A` to open the Application Center and verify it renders without covering or corrupting the desktop permanently.
- [ ] Confirm native entries for Shell Console, Files, and System Status open the expected existing panels.
- [ ] Inspect Phoxtra application entries and verify each row shows an accurate availability state.
- [ ] Open the Amineva Workspace entry with `W`; verify the Docs, Sheets, Slides, Forms, Drive, and collaboration scope is described correctly.
- [ ] Use `B` to return to the catalogue and `Q` to close it; verify the previous desktop window and input still work.
- [ ] Confirm Amnewsworld is identified as an independent media organization, with its news and radio/media-service entry points separate from Amineva Global.
- [ ] Confirm Amineva Global product entries are AgroClimate, Adesua, DataLens, Inventra, Sikasem, and Amineva Workspace; verify all external launch states remain pending until actual launch hand-off is implemented.
- [ ] Do not mark external Amineva apps as launched merely because their catalogue entries are visible.
- [ ] Do not claim account sign-in, shared files, online service hand-off, or offline support unless each capability is connected and separately tested.

**Integration limitation:** The Application Center currently provides native panel entry points and a catalogue of planned integrations. It does not yet launch the external Amineva applications or provide the full Amineva Workspace runtime. Record those as **Not implemented / Blocked**, not as a laptop hardware defect.

**Storage safety warning:** This build registers a 32 MiB volatile RAM test disk. It is not a physical SATA/AHCI/NVMe driver. The Ext2/FAT32 modules are demo adapters, not real on-disk filesystem implementations. Partition discovery is read-only; the installer and partition-formatting path are disabled. Files are temporary and lost on reboot. Do not run an installer, format a partition, or write to the laptop's internal drive. Record firmware/device information only; do not treat the simulated disk as evidence of physical storage support.

## 6. Reliability and evidence

- [ ] Leave the desktop running and interacting for at least 15 minutes.
- [ ] Repeat a boot and the core input/desktop checks.
- [ ] Record any panic, freeze, unexpected reset, graphical artifact, or unresponsive device.
- [ ] Record unsupported hardware separately from defects.
- [ ] Re-test every failed item after a fix.

### Test notes

| Test / symptom | Result | Evidence or notes | Retest result |
|---|---|---|---|
| CI build, ISO, and QEMU preflight | Passed in CI; not physical-laptop evidence | Run 39895758147, commit 7422912b2c0c4514c42d79f1087919a553cdd2bd | N/A |
| Physical laptop boot and display | Not started | | |
| Keyboard | Not started | | |
| Pointer / touchpad | Not started | | |
| Themes and typography | Not started | | |
| File manager and windows | Not started | | |
| Stability / reboot | Not started | | |

## 7. Acceptance gate

The laptop milestone is **not passed** until the boot, display, keyboard, pointer, desktop-interaction, and stability checks have been completed on the physical laptop; release-blocking defects have been fixed and retested; and the result is recorded above.

Only after this gate passes may work proceed to the first dedicated Phoxtra hardware integration. The Phase 8 preparation plan is documented in [phase8-hardware-integration-plan.md](phase8-hardware-integration-plan.md); it is gated and does not count as hardware validation. A successful CMake build, ISO generation, or QEMU test is necessary evidence, but it does not replace physical-laptop validation.


## Storage safety automation

CI runs `scripts/check_storage_safety.py` to guard against reintroducing automatic partition writes or a pretend installer. A passing static audit is a regression guard, not proof of physical storage driver support. See the [Phase 8 hardware integration plan](phase8-hardware-integration-plan.md).
