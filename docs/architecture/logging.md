# Kernel Logging Architecture - Phoxtra OS v0.4.0-alpha

Phoxtra OS includes a centralized logging system to record kernel subsystem events, warnings, errors, and security events.

## Log Levels

| Log Level | Constant | Tag Prefix | Purpose |
| :--- | :--- | :--- | :--- |
| **Info** | `LOG_INFO` | `[INFO]` | General system operational messages |
| **Warning** | `LOG_WARNING` | `[WARNING]` | Non-fatal potential anomalies |
| **Error** | `LOG_ERROR` | `[ERROR]` | Fatal subsystem errors |
| **Security** | `LOG_SECURITY` | `[SECURITY]` | Security events & access protection |
| **Kernel** | `LOG_KERNEL` | `[KERNEL]` | Low-level core subsystem startup logs |

## API Usage

```c
#include "logger.h"

klog(LOG_INFO, "Subsystem ready");
klog(LOG_KERNEL, "Memory manager started");
klog(LOG_SECURITY, "Protection layer online");
```
