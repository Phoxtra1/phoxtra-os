# Phoxtra OS Automated OTA (Over-The-Air) Update Architecture

## 1. Overview
Phoxtra OS features a zero-interruption, automated system updater (`phoxtra-updater`) connected directly to **Phoxtra Cloud (`cloud.phoxtra.com`)**. Everyday laptop users do **not** need to use terminal commands or manual prompts to update their operating system or applications.

---

## 2. Automated Background Update Workflow

1. **Background Update Check**:
   - The OS update daemon polls `cloud.phoxtra.com/v1/updates` securely over TLS.
2. **Cryptographic Validation**:
   - Downloads signed binary diffs and checks them against the **PhoxtraID Root CA** and motherboard **TPM 2.0 SPI chip**.
3. **Silent Staging**:
   - Update files are staged in a secondary partition or system staging directory (`/sys/update_stage/`) while the user continues working uninterrupted.
4. **Desktop Notification**:
   - Displays a modern glassmorphic notification on `phoxtra-wm`:
     > *"Phoxtra OS v1.0.1 Update Installed. Apply on next restart."*

---

## 3. Dual-Mode Flexibility

* **Graphical Auto-Pilot (Default)**: Fully automated for everyday consumers and enterprise laptops.
* **Developer CLI (Optional)**: Power users can run `update` or `phoxtra-update` in the terminal to inspect active updates manually.
