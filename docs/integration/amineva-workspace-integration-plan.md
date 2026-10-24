# Amineva Workspace Integration Plan for Phoxtra OS

**Current status: Catalogue entry only — not an integrated or launchable Workspace suite.**

**Networking status (partial prerequisite only):** the prototype now includes bounded ARP/IPv4 validation, UDP receive handlers, a DHCP lease-acquisition exchange, DNS A-record resolution, and an experimental minimal TCP stream/socket path with checksum validation and bounded send/receive behavior. These paths have automated regression/build/QEMU coverage but have not been validated on a physical laptop NIC. TCP out-of-order reassembly/congestion control and full standards interoperability remain incomplete; DHCP lease acquisition, renewal, rebinding, and expiry clearing now have automated tests but remain physically unvalidated. TLS/certificate validation and a browser/web runtime remain unavailable. DNS-over-TCP fallback for truncated UDP responses is implemented in a bounded form and covered by automated tests. This partial network milestone does not make Workspace launchable.

The Application Center contains an Amineva Workspace catalogue record describing Docs, Sheets, Slides, Forms, and Drive. The current record has no launch destination and is marked planned/pending. The presence of this entry is not evidence that those products open from Phoxtra OS.

## Goal

Make Amineva Workspace a reliable, clearly identified productivity entry point in Phoxtra OS without misrepresenting external web services as native applications or claiming capabilities that have not been implemented.

## Integration paths to evaluate

### Option 1 — Browser-based Workspace service (recommended first evaluation)

This is likely the smallest initial integration if the existing Workspace products are usable web applications.

Prerequisites:
- A working network driver and tested network configuration in Phoxtra OS.
- A maintained browser or web runtime with HTTPS/TLS, certificate validation, URL navigation, downloads, uploads, and reliable memory behavior.
- Configured and verified service URLs for the actual deployed Workspace applications.
- An account/session design that respects the existing Workspace authentication model.
- A safe hand-off between Docs, Sheets, Slides, Forms, and Drive, including file formats and access controls.

The current prototype does not deliver a complete browser/TLS user feature. Therefore, simply adding a URL to `launch_destination` is not sufficient. Do not mark this option complete until the runtime is built/ported or a suitable supported runtime is integrated, compiled, licensed appropriately, and tested.

### Option 2 — Dedicated native Phoxtra client

A native client can provide tighter OS integration but requires a defined application/runtime API, networking, authentication, secure session storage, document editing/viewing components, and service interfaces. It is a larger project and should follow a written interface contract rather than a placeholder launcher.

### Option 3 — External-device hand-off

Until Phoxtra has a supported browser/runtime, documentation may explain that Workspace is accessed on a separate supported device/browser. This is a user workflow, not an OS-level integration, and must be labelled accordingly.

## Recommended staged implementation

1. **Inventory the live Workspace deployment.** List each real application URL and whether it works without redirects or undocumented session assumptions. Do not invent URLs.
2. **Write the integration contract.** Define authentication, session expiry/logout, app-to-app navigation, file types, Drive access, error handling, and what works online/offline.
3. **Verify OS networking prerequisites.** The repository now contains experimental TCP stream sockets, DHCP lease acquisition, DNS A-query parsing, and UDP/IPv4 packet validation, but those tests do not prove physical NIC interoperability or complete protocol standards compliance. Close the remaining gaps: out-of-order TCP reassembly/congestion control and broader interoperability, physical NIC validation of DHCP lease lifecycle, DNS response sizes beyond the current bounded parser, HTTPS/TLS, certificate-chain/hostname validation, valid system time, and connection failure handling.
4. **Choose and integrate a browser/runtime.** Evaluate memory footprint, licensing, architecture, graphics/input integration, sandboxing, security updates, and build reproducibility. Do not use an unsupported web engine simply because it can be linked.
5. **Add launch metadata only after the runtime exists.** Configure a real verified URL or native client entry. Keep the state pending while unavailable.
6. **Test authentication and data flow.** Verify login, logout, expired sessions, direct links, file open/save/download/upload, cross-app file access, permission boundaries, and service outage behavior.
7. **Evaluate offline requirements separately.** If offline use is important, specify what can be cached, how sync conflicts are resolved, and how data is encrypted/protected. Do not infer offline support from a PWA label or cache alone.
8. **Update Application Center and release documentation.** Each application should say whether it is a native panel, a working web service, a planned integration, or unavailable on this build.

## Amineva product catalogue

The following Amineva Global products are present as planned entries: AgroClimate, Adesua, DataLens, Inventra, Sikasem, and Amineva Workspace. Each product needs a verified service endpoint and a tested launch/authentication flow before it can be labelled available. Do not expose fabricated links or mark catalogue-only entries as launchable.

Amnewsworld News and Amnewsworld Radio and Media are separate publisher entries. Amnewsworld is an independent media organization and should not be labelled an Amineva Global product.

## Acceptance criteria

Workspace integration is ready to mark as implemented only when:
- The configured entry launches the actual app in the selected runtime.
- Authentication works and can be safely ended.
- Docs, Sheets, Slides, Forms, and Drive entry points are real and tested.
- File open/save and Drive permissions behave as documented.
- Network loss, server errors, expired sessions, and unsupported links fail safely and clearly.
- The build includes the runtime and CI covers its integration surface.
- README, Application Center descriptions, and release notes match the tested capabilities.

Until then, keep the catalogue entry pending and describe Workspace integration as planned.
