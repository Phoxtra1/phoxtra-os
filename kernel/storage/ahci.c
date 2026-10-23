#include "ahci.h"
#include "../memory/heap.h"
#include "../memory/vmm.h"
#include "../drivers/pci.h"
#include "../log/logger.h"
#include "partition.h"

/*
 * Read-only AHCI/SATA support for 512-byte logical-sector ATA disks.
 * DMA addresses are restricted to the bootstrap identity-mapped first 4 GiB.
 * Writes are deliberately unsupported until a separately reviewed storage policy,
 * flush semantics, and recovery implementation exist.
 */

#define AHCI_BAR_WINDOW      0x1100u
#define AHCI_PORTS           32u
#define AHCI_POLL_LIMIT      1000000u
#define AHCI_SIGNATURE_ATA   0x00000101u
#define AHCI_FIS_TYPE_H2D    0x27u
#define ATA_CMD_IDENTIFY     0xECu
#define ATA_CMD_READ_DMA_EXT  0x25u
#define ATA_STATUS_ERR       0x01u
#define ATA_STATUS_DRQ       0x08u
#define ATA_STATUS_BSY       0x80u
#define AHCI_PXIS_TFES        (1u << 30)
#define AHCI_PXCMD_ST         (1u << 0)
#define AHCI_PXCMD_FRE        (1u << 4)
#define AHCI_PXCMD_FR         (1u << 14)
#define AHCI_PXCMD_CR         (1u << 15)
#define AHCI_GHC_AE           (1u << 31)
#define AHCI_CAP_SLOTS(cap)   ((((cap) >> 8) & 0x1Fu) + 1u)
#define RAM_TEST_SECTOR_COUNT 65536u
#define RAM_TEST_SECTOR_SIZE  512u
#define RAM_TEST_BYTES        ((uint64_t)RAM_TEST_SECTOR_COUNT * RAM_TEST_SECTOR_SIZE)

typedef struct __attribute__((packed)) {
    uint16_t flags;
    uint16_t prdt_length;
    uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t reserved[4];
} ahci_command_header_t;

typedef struct __attribute__((packed)) {
    uint32_t data_base;
    uint32_t data_base_upper;
    uint32_t reserved;
    uint32_t byte_count_and_interrupt;
} ahci_prdt_entry_t;

typedef struct __attribute__((packed, aligned(128))) {
    uint8_t command_fis[64];
    uint8_t atapi_command[16];
    uint8_t reserved[48];
    ahci_prdt_entry_t prdt[1];
} ahci_command_table_t;

typedef struct __attribute__((aligned(1024))) {
    uint8_t command_list[1024] __attribute__((aligned(1024)));
    uint8_t received_fis[256] __attribute__((aligned(256)));
    ahci_command_table_t command_table __attribute__((aligned(256)));
    uint8_t data[512] __attribute__((aligned(512)));
} ahci_port_dma_t;

typedef struct ahci_physical_drive {
    block_device_t device;
    volatile hba_port_t* port;
    ahci_port_dma_t* dma;
    uint8_t active;
    uint8_t busy;
    uint8_t port_index;
    uint8_t read_confirmed;
} ahci_physical_drive_t;

static ahci_physical_drive_t physical_drives[AHCI_PORTS];
static ahci_port_dma_t physical_dma[AHCI_PORTS] __attribute__((aligned(1024)));
static block_device_t ram_test_dev;
static uint8_t* ram_test_storage = (void*)0;

static void kstrncpy(char* dst, const char* src, int maxlen) {
    if (!dst || !src || maxlen <= 0) return;
    int i = 0;
    for (; i < maxlen - 1 && src[i]; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

static void zero_bytes(void* address, uint64_t size) {
    uint8_t* bytes = (uint8_t*)address;
    for (uint64_t i = 0; i < size; ++i) bytes[i] = 0;
}

static int wait_register(volatile uint32_t* reg, uint32_t mask, int want_set) {
    for (uint32_t spins = 0; spins < AHCI_POLL_LIMIT; ++spins) {
        uint32_t value = *reg;
        if (want_set ? ((value & mask) == mask) : ((value & mask) == 0)) return 0;
    }
    return -1;
}

static int dma_physical_address(const void* pointer, uint64_t size, uint32_t* physical_out) {
    if (!pointer || !size || !physical_out) return -1;
    uint64_t virtual_start = (uint64_t)(uintptr_t)pointer;
    if (vmm_ensure_identity_mapped(virtual_start, size) != 0) return -1;
    uint64_t physical_start = vmm_virt_to_phys(virtual_start);
    if (!physical_start || physical_start != virtual_start ||
        physical_start >= 0x100000000ULL || size > 0x100000000ULL - physical_start) return -1;
    *physical_out = (uint32_t)physical_start;
    return 0;
}

static int stop_port_engine(volatile hba_port_t* port) {
    port->cmd &= ~AHCI_PXCMD_ST;
    if (wait_register(&port->cmd, AHCI_PXCMD_CR, 0) != 0) return -1;
    port->cmd &= ~AHCI_PXCMD_FRE;
    if (wait_register(&port->cmd, AHCI_PXCMD_FR, 0) != 0) return -1;
    return 0;
}

static int start_port_engine(volatile hba_port_t* port) {
    port->cmd |= AHCI_PXCMD_FRE;
    port->cmd |= AHCI_PXCMD_ST;
    if (wait_register(&port->cmd, AHCI_PXCMD_ST | AHCI_PXCMD_FRE, 1) != 0) return -1;
    return 0;
}

static int ahci_issue_data_command(ahci_physical_drive_t* drive, uint8_t command,
                                   uint64_t lba, uint8_t* data_buffer) {
    if (!drive || !drive->port || !drive->dma || !data_buffer ||
        data_buffer != drive->dma->data) return -1;

    uint32_t clb_phys = 0, table_phys = 0, data_phys = 0;
    if (dma_physical_address(drive->dma->command_list, sizeof(drive->dma->command_list), &clb_phys) != 0 ||
        dma_physical_address(&drive->dma->command_table, sizeof(drive->dma->command_table), &table_phys) != 0 ||
        dma_physical_address(data_buffer, RAM_TEST_SECTOR_SIZE, &data_phys) != 0) return -1;

    volatile hba_port_t* port = drive->port;
    for (uint32_t spins = 0; spins < AHCI_POLL_LIMIT; ++spins) {
        if ((port->tfd & (ATA_STATUS_BSY | ATA_STATUS_DRQ)) == 0) break;
        if (spins + 1u == AHCI_POLL_LIMIT) return -1;
    }
    if (port->tfd & ATA_STATUS_ERR) return -1;

    ahci_command_header_t* headers = (ahci_command_header_t*)drive->dma->command_list;
    zero_bytes(&headers[0], sizeof(headers[0]));
    headers[0].flags = 5u; /* Register host-to-device FIS is 5 DWORDs; read-only command. */
    headers[0].prdt_length = 1u;
    headers[0].ctba = table_phys;
    headers[0].ctbau = 0;

    ahci_command_table_t* table = &drive->dma->command_table;
    zero_bytes(table, sizeof(*table));
    table->prdt[0].data_base = data_phys;
    table->prdt[0].data_base_upper = 0;
    table->prdt[0].reserved = 0;
    table->prdt[0].byte_count_and_interrupt = RAM_TEST_SECTOR_SIZE - 1u;

    uint8_t* fis = table->command_fis;
    fis[0] = AHCI_FIS_TYPE_H2D;
    fis[1] = 0x80u; /* Command FIS */
    fis[2] = command;
    if (command == ATA_CMD_READ_DMA_EXT) {
        fis[4] = (uint8_t)lba;
        fis[5] = (uint8_t)(lba >> 8);
        fis[6] = (uint8_t)(lba >> 16);
        fis[7] = 1u << 6; /* LBA mode */
        fis[8] = (uint8_t)(lba >> 24);
        fis[9] = (uint8_t)(lba >> 32);
        fis[10] = (uint8_t)(lba >> 40);
        fis[12] = 1u; /* Exactly one sector per DMA operation */
    }

    port->is = 0xFFFFFFFFu;   /* Clear write-one-to-clear interrupt status. */
    port->serr = 0xFFFFFFFFu;
    asm volatile("mfence" ::: "memory");
    port->ci = 1u;             /* Slot zero; this driver serializes each port. */

    uint32_t spins = 0;
    int task_error = 0;
    while ((port->ci & 1u) && spins++ < AHCI_POLL_LIMIT) {
        if (port->is & AHCI_PXIS_TFES) task_error = 1;
    }
    if (port->ci & 1u) {
        /* Never reuse a DMA buffer while the controller may still be accessing it. */
        (void)stop_port_engine(port);
        drive->active = 0;
        klog(LOG_ERROR, "AHCI: command timed out; port disabled to protect DMA buffers");
        return -1;
    }
    if (task_error || (port->is & AHCI_PXIS_TFES) || (port->tfd & ATA_STATUS_ERR)) return -1;
    return 0;
}

static uint64_t identify_sector_count(const uint16_t* identify) {
    if (!identify || (identify[83] & 0xC000u) != 0x4000u ||
        !(identify[83] & (1u << 10))) return 0; /* Require valid LBA48 support. */

    if ((identify[106] & 0xC000u) == 0x4000u && (identify[106] & (1u << 12))) {
        uint32_t logical_words = (uint32_t)identify[117] |
                                 ((uint32_t)identify[118] << 16);
        if (logical_words != 256u) return 0; /* Non-512-byte logical sectors need separate support. */
    }

    return ((uint64_t)identify[103] << 48) |
           ((uint64_t)identify[102] << 32) |
           ((uint64_t)identify[101] << 16) |
           (uint64_t)identify[100];
}

static int prepare_physical_port(volatile hba_port_t* port, uint32_t index) {
    if (!port || index >= AHCI_PORTS) return -1;
    ahci_port_dma_t* dma = &physical_dma[index];
    ahci_physical_drive_t* drive = &physical_drives[index];
    if (stop_port_engine(port) != 0) {
        klog(LOG_WARNING, "AHCI: command engine did not stop cleanly; port skipped");
        return -1;
    }

    uint32_t clb_phys = 0, fb_phys = 0;
    if (dma_physical_address(dma->command_list, sizeof(dma->command_list), &clb_phys) != 0 ||
        dma_physical_address(dma->received_fis, sizeof(dma->received_fis), &fb_phys) != 0 ||
        dma_physical_address(&dma->command_table, sizeof(dma->command_table), &clb_phys) != 0 ||
        dma_physical_address(dma->data, sizeof(dma->data), &clb_phys) != 0) {
        klog(LOG_WARNING, "AHCI: DMA structures are not safely identity mapped below 4 GiB; port skipped");
        return -1;
    }
    /* Recompute after using clb_phys as the scratch output above. */
    if (dma_physical_address(dma->command_list, sizeof(dma->command_list), &clb_phys) != 0 ||
        dma_physical_address(dma->received_fis, sizeof(dma->received_fis), &fb_phys) != 0) return -1;

    zero_bytes(dma, sizeof(*dma));
    zero_bytes(drive, sizeof(*drive));
    port->clb = clb_phys;
    port->clbu = 0;
    port->fb = fb_phys;
    port->fbu = 0;
    port->ie = 0;
    port->is = 0xFFFFFFFFu;
    port->serr = 0xFFFFFFFFu;

    if (start_port_engine(port) != 0) {
        klog(LOG_WARNING, "AHCI: command engine did not start; port skipped");
        return -1;
    }

    drive->port = port;
    drive->dma = dma;
    drive->port_index = (uint8_t)index;
    if (ahci_issue_data_command(drive, ATA_CMD_IDENTIFY, 0, dma->data) != 0) {
        (void)stop_port_engine(port);
        klog(LOG_INFO, "AHCI: IDENTIFY DEVICE failed; port is not registered");
        return -1;
    }

    const uint16_t* identify = (const uint16_t*)dma->data;
    uint64_t sectors = identify_sector_count(identify);
    if (!sectors) {
        (void)stop_port_engine(port);
        klog(LOG_WARNING, "AHCI: disk lacks supported valid LBA48/512-byte geometry; port skipped");
        return -1;
    }

    drive->device.total_sectors = sectors;
    drive->device.sector_size = RAM_TEST_SECTOR_SIZE;
    drive->device.flags = BLOCKDEV_FLAG_READONLY;
    drive->device.read_sectors = ahci_read_sectors;
    drive->device.write_sectors = ahci_write_sectors;
    drive->device.priv_data = drive;
    drive->active = 1;

    drive->device.name[0] = 's'; drive->device.name[1] = 'a';
    drive->device.name[2] = 't'; drive->device.name[3] = 'a';
    if (index < 10u) {
        drive->device.name[4] = (char)('0' + index);
        drive->device.name[5] = '\0';
    } else {
        drive->device.name[4] = (char)('0' + (index / 10u));
        drive->device.name[5] = (char)('0' + (index % 10u));
        drive->device.name[6] = '\0';
    }

    if (blockdev_register(&drive->device) != 0) {
        drive->active = 0;
        (void)stop_port_engine(port);
        klog(LOG_WARNING, "AHCI: disk could not be registered; device is unavailable");
        return -1;
    }

    klog(LOG_INFO, "AHCI: read-only ATA disk registered after IDENTIFY; physical writes disabled");
    return 0;
}

static int probe_ahci_controller(void) {
    pci_device_t* controller = (void*)0;
    for (uint32_t i = 0; i < pci_get_device_count(); ++i) {
        pci_device_t* candidate = pci_get_device(i);
        if (candidate && candidate->class_id == AHCI_PCI_CLASS &&
            candidate->subclass == AHCI_PCI_SUBCLASS &&
            candidate->prog_if == AHCI_PCI_PROGIF) {
            controller = candidate;
            break;
        }
    }

    if (!controller) {
        klog(LOG_INFO, "AHCI: no supported PCI AHCI controller found; no physical SATA disk is registered");
        return 0;
    }

    uint32_t bar = controller->bar5;
    uint32_t bar_type = (bar >> 1) & 0x3u;
    uint64_t mmio_base = (uint64_t)(bar & ~0x0Fu);
    if ((bar & 1u) || bar_type != 0 || !mmio_base ||
        mmio_base >= 0x100000000ULL || sizeof(hba_mem_t) > 0x100000000ULL - mmio_base) {
        klog(LOG_WARNING, "AHCI: controller BAR5 is not a supported 32-bit MMIO region");
        return -1;
    }

    if (vmm_ensure_identity_mapped(mmio_base, sizeof(hba_mem_t)) != 0 ||
        vmm_mark_uncached(mmio_base, sizeof(hba_mem_t)) != 0) {
        klog(LOG_WARNING, "AHCI: controller MMIO cannot be mapped safely; physical storage disabled");
        return -1;
    }

    uint32_t command_status = pci_config_read32(controller->bus, controller->slot,
                                               controller->func, 0x04);
    uint16_t command = (uint16_t)command_status;
    command |= 0x0006u; /* Enable PCI memory-space access and bus mastering. */
    pci_config_write32(controller->bus, controller->slot, controller->func, 0x04, command);

    volatile hba_mem_t* hba = (volatile hba_mem_t*)(uintptr_t)mmio_base;
    hba->ghc = (hba->ghc | AHCI_GHC_AE) & ~(1u << 1); /* Poll completions; keep HBA interrupts disabled. */

    /* Honor the optional BIOS/OS ownership handshake when the HBA advertises it. */
    if (hba->cap2 & 1u) {
        hba->bohc |= (1u << 1);
        uint32_t spins = 0;
        while ((hba->bohc & ((1u << 0) | (1u << 4))) && spins++ < AHCI_POLL_LIMIT) { }
        if (hba->bohc & ((1u << 0) | (1u << 4))) {
            klog(LOG_WARNING, "AHCI: BIOS/OS ownership handoff timed out; physical storage disabled");
            return -1;
        }
    }

    uint32_t implemented_ports = hba->pi;
    uint32_t discovered = 0;
    uint32_t slots = AHCI_CAP_SLOTS(hba->cap);
    if (slots == 0) {
        klog(LOG_WARNING, "AHCI: controller reports no command slots");
        return -1;
    }

    for (uint32_t i = 0; i < AHCI_PORTS; ++i) {
        if (!(implemented_ports & (1u << i))) continue;
        volatile hba_port_t* port = &hba->ports[i];
        uint32_t status = port->ssts;
        if ((status & 0x0Fu) != 3u || ((status >> 8) & 0x0Fu) != 1u ||
            port->sig != AHCI_SIGNATURE_ATA) continue; /* Only connected SATA ATA disks. */
        if (prepare_physical_port(port, i) == 0) ++discovered;
    }

    klog(LOG_INFO, discovered
        ? "AHCI: physical read-only SATA probing completed"
        : "AHCI: controller detected but no supported readable ATA disks were registered");
    return (int)discovered;
}

/* Simulated volatile RAM storage retained only for the built-in desktop demo. */
static int ram_test_read_sectors(block_device_t* dev, uint64_t lba,
                                 uint32_t count, uint8_t* buffer) {
    if (dev != &ram_test_dev || !ram_test_storage || !buffer || count == 0 ||
        dev->sector_size != RAM_TEST_SECTOR_SIZE ||
        dev->total_sectors != RAM_TEST_SECTOR_COUNT) return -1;
    if (lba >= dev->total_sectors || (uint64_t)count > dev->total_sectors - lba) return -1;

    const uint64_t offset = lba * RAM_TEST_SECTOR_SIZE;
    const uint64_t bytes = (uint64_t)count * RAM_TEST_SECTOR_SIZE;
    for (uint64_t i = 0; i < bytes; ++i) buffer[i] = ram_test_storage[offset + i];
    return 0;
}

static int ram_test_write_sectors(block_device_t* dev, uint64_t lba,
                                  uint32_t count, const uint8_t* buffer) {
    (void)dev; (void)lba; (void)count; (void)buffer;
    return -1;
}

int ahci_read_sectors(block_device_t* dev, uint64_t lba, uint32_t count, uint8_t* buffer) {
    if (!dev || !buffer || count == 0 || (dev->flags & BLOCKDEV_FLAG_SIMULATED)) return -1;
    ahci_physical_drive_t* drive = (ahci_physical_drive_t*)dev->priv_data;
    if (!drive || &drive->device != dev || !drive->active || !drive->port || !drive->dma) return -1;
    if (dev->sector_size != RAM_TEST_SECTOR_SIZE ||
        lba >= dev->total_sectors || (uint64_t)count > dev->total_sectors - lba) return -1;
    if (__sync_lock_test_and_set(&drive->busy, 1u)) return -1;

    uint8_t* out = buffer;
    int result = 0;
    for (uint32_t i = 0; i < count; ++i) {
        if (ahci_issue_data_command(drive, ATA_CMD_READ_DMA_EXT, lba + i, drive->dma->data) != 0) {
            result = -1;
            break;
        }
        const uint8_t* src = drive->dma->data;
        for (uint32_t byte = 0; byte < RAM_TEST_SECTOR_SIZE; ++byte) out[(uint64_t)i * RAM_TEST_SECTOR_SIZE + byte] = src[byte];
    }
    if (result == 0 && !drive->read_confirmed) {
        drive->read_confirmed = 1;
        klog(LOG_INFO, "AHCI: confirmed read-only sector I/O on SATA device");
    }
    __sync_lock_release(&drive->busy);
    return result;
}

int ahci_write_sectors(block_device_t* dev, uint64_t lba, uint32_t count, const uint8_t* buffer) {
    (void)dev; (void)lba; (void)count; (void)buffer;
    return -1;
}

void ahci_init(void) {
    /* Probe physical hardware independently of the RAM-backed demo disk. */
    (void)probe_ahci_controller();

    if (ram_test_storage) {
        klog(LOG_INFO, "Storage: RAM test disk is already initialized");
        return;
    }
    if (blockdev_get("ramdisk0")) {
        klog(LOG_WARNING, "Storage: ramdisk0 name is already registered; refusing to replace another device");
        return;
    }

    ram_test_storage = (uint8_t*)kmalloc((size_t)RAM_TEST_BYTES);
    if (!ram_test_storage) {
        klog(LOG_ERROR, "Storage: failed to allocate RAM test disk");
        return;
    }
    zero_bytes(ram_test_storage, RAM_TEST_BYTES);

    /* Seed a demo MBR only in RAM. This is not a physical disk or filesystem. */
    mbr_sector_t* demo_mbr = (mbr_sector_t*)ram_test_storage;
    demo_mbr->signature = 0xAA55u;
    demo_mbr->entries[0].sys_id = 0x83u;
    demo_mbr->entries[0].start_lba = 2048u;
    demo_mbr->entries[0].total_sectors = 16384u;
    demo_mbr->entries[1].sys_id = 0x83u;
    demo_mbr->entries[1].start_lba = 18432u;
    demo_mbr->entries[1].total_sectors = 32768u;
    demo_mbr->entries[2].sys_id = 0x0Cu;
    demo_mbr->entries[2].start_lba = 51200u;
    demo_mbr->entries[2].total_sectors = 14336u;

    kstrncpy(ram_test_dev.name, "ramdisk0", (int)sizeof(ram_test_dev.name));
    ram_test_dev.total_sectors = RAM_TEST_SECTOR_COUNT;
    ram_test_dev.sector_size = RAM_TEST_SECTOR_SIZE;
    ram_test_dev.flags = BLOCKDEV_FLAG_SIMULATED | BLOCKDEV_FLAG_READONLY;
    ram_test_dev.read_sectors = ram_test_read_sectors;
    ram_test_dev.write_sectors = ram_test_write_sectors;
    ram_test_dev.priv_data = (void*)0;

    if (blockdev_register(&ram_test_dev) != 0) {
        klog(LOG_ERROR, "Storage: failed to register RAM test disk");
        kfree(ram_test_storage);
        ram_test_storage = (void*)0;
        return;
    }
    klog(LOG_INFO, "Storage: volatile RAM test disk /dev/ramdisk0 registered read-only; physical SATA/NVMe writes remain disabled");
}
