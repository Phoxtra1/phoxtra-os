# Phoxdrive In-Vehicle Hardware & OS Subsystem Specification

## 1. Product Mission & Overview

**Phoxdrive** is an intelligent automotive hardware unit powered natively by **Phoxtra OS**. Installed directly inside vehicles, Phoxdrive acts as a proactive digital co-pilot and real-time road hazard sentry designed to dramatically reduce road accidents, alert drivers of immediate hazards, and protect vehicle suspensions against deplorable road infrastructure.

Rather than running on external consumer mobile apps, **Phoxdrive runs Phoxtra OS on bare-metal hardware** installed in the vehicle, interfacing with vehicle sensors, radar/cameras, GPS, and audio output.

---

## 2. Core Functional Pillars

### 2.1 Stalled & Broken-down Vehicle Radar Detection
* **Objective**: Prevent high-speed collisions with stationary or stalled vehicles on poorly lit highways and unlit rural corridors.
* **Mechanism**:
  - Ingests forward-facing radar/LiDAR distance telemetry and camera proximity triggers.
  - Computes relative closing velocity and calculates time-to-impact (TTI).
  - Automatically triggers distance alerts at 150m, 100m, and 50m intervals.
  - Broadcasts audible speech warnings: *"Warning: Stationary broken vehicle detected 80 meters ahead. Reduce speed and prepare to merge."*

### 2.2 Accident-Prone Zone Geofencing
* **Objective**: Warn drivers before they enter statistically dangerous blackspots, sharp blind curves, or high-crash intersections.
* **Mechanism**:
  - Compares real-time GPS coordinates against an onboard vector database of accident hotspots.
  - Warns drivers 300 meters prior to entering the hazard sector.
  - Spoken notification: *"Caution: Approaching known accident-prone sector. Drive with extreme vigilance."*

### 2.3 Deplorable Road & Pothole Surface Scanner
* **Objective**: Protect vehicles from wheel rim destruction, tie-rod breaks, and rollover accidents caused by severe potholes and unpaved, washed-out road surfaces.
* **Mechanism**:
  - Continuously monitors high-frequency IMU 3-axis accelerometer data (especially vertical Z-axis shocks).
  - Normal road travel registers vertical gravity around $\approx 1000\text{ mg}$. Severe shocks exceeding $1800\text{ mg}$ or drops below $200\text{ mg}$ are classified as critical road defects.
  - Rates road segments on an index from 0 (deplorable/impassable) to 100 (pristine paved highway).
  - When scores drop below 40, Phoxdrive proactively warns: *"Attention: Severe deplorable road condition and potholes detected ahead. Slow down."*
  - Automatically logs the GPS coordinates and vibration magnitude for crowd-sourced road quality mapping via Phoxtra Zero-Trust Cloud Sync.

### 2.4 Speed Monitoring & Proactive Voice Speak-out Alerts
* **Objective**: Prevent catastrophic speeding accidents by audibly engaging the driver.
* **Mechanism**:
  - Continuously cross-checks vehicle speed (via CAN bus / OBD-II or GPS ground speed) against zone limits.
  - Instantly sounds synthetic speech alerts when safe or legal limits are breached: *"Overspeeding alert! Current vehicle speed exceeds the legal road limit."*
  - Incorporates voice audio muting and unmuting controls via shell and UI.

---

## 3. Subsystem Implementation within Phoxtra OS

* **Master Controller**: [`kernel/hardware/phoxdrive/phoxdrive.c`](file:///c:/Users/HP/Documents/Projects/apps/phoxtra-os/kernel/hardware/phoxdrive/phoxdrive.c)  
  Manages initialization, device registration (`HARDWARE_CLASS_AUTOMOTIVE`), and high-level vehicle state.
* **Hazard & Radar Scanner**: [`kernel/hardware/phoxdrive/phoxdrive_hazard.c`](file:///c:/Users/HP/Documents/Projects/apps/phoxtra-os/kernel/hardware/phoxdrive/phoxdrive_hazard.c)  
  Handles active hazard tracking, severity indexing, and automatic alert dispatching.
* **Synthetic Voice Engine**: [`kernel/hardware/phoxdrive/phoxdrive_audio.c`](file:///c:/Users/HP/Documents/Projects/apps/phoxtra-os/kernel/hardware/phoxdrive/phoxdrive_audio.c)  
  Manages spoken voice alert synthesis and audio history.
* **Vehicle Telemetry**: [`kernel/hardware/phoxdrive/phoxdrive_telemetry.c`](file:///c:/Users/HP/Documents/Projects/apps/phoxtra-os/kernel/hardware/phoxdrive/phoxdrive_telemetry.c)  
  Ingests GPS, speed, 3-axis IMU accelerations, and pothole impact shocks.
* **Interactive CLI Tooling**: [`kernel/src/shell.c`](file:///c:/Users/HP/Documents/Projects/apps/phoxtra-os/kernel/src/shell.c)  
  Supports `phoxdrive status`, `phoxdrive hazard`, `phoxdrive speed <kmh>`, `phoxdrive broken <dist>`, `phoxdrive road <0-100>`, `phoxdrive alert <msg>`, and `phoxdrive clear`.
* **Automotive Heads-Up Display**: [`kernel/ui/wm.c`](file:///c:/Users/HP/Documents/Projects/apps/phoxtra-os/kernel/ui/wm.c)  
  Integrated in the Phoxtra-WM desktop environment and in-vehicle head units.
