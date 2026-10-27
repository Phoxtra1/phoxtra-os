#include "e1000.h"
#include "ethernet.h"
#include "../drivers/pci.h"
#include "../memory/vmm.h"
#include "../log/logger.h"
#include <stddef.h>

#define E1000_NUM_RX_DESC 32u
#define E1000_NUM_TX_DESC 32u
#define E1000_RX_BUFFER_SIZE 2048u
#define E1000_TX_BUFFER_SIZE 2048u
#define E1000_MMIO_SIZE 0x6000u
#define E1000_POLL_LIMIT 1000000u

#define E1000_REG_CTRL   0x0000u
#define E1000_REG_STATUS 0x0008u
#define E1000_REG_EERD   0x0014u
#define E1000_REG_IMC    0x00D8u
#define E1000_REG_RCTL   0x0100u
#define E1000_REG_TCTL   0x0400u
#define E1000_REG_TIPG   0x0410u
#define E1000_REG_RDBAL  0x2800u
#define E1000_REG_RDBAH  0x2804u
#define E1000_REG_RDLEN  0x2808u
#define E1000_REG_RDH    0x2810u
#define E1000_REG_RDT    0x2818u
#define E1000_REG_TDBAL  0x3800u
#define E1000_REG_TDBAH  0x3804u
#define E1000_REG_TDLEN  0x3808u
#define E1000_REG_TDH    0x3810u
#define E1000_REG_TDT    0x3818u
#define E1000_REG_RAL0   0x5400u
#define E1000_REG_RAH0   0x5404u

#define E1000_CTRL_FD    0x00000001u
#define E1000_CTRL_ASDE  0x00000020u
#define E1000_CTRL_SLU   0x00000040u
#define E1000_CTRL_RST   0x04000000u
#define E1000_STATUS_LU  0x00000002u
#define E1000_RCTL_EN    0x00000002u
#define E1000_RCTL_BAM   0x00008000u
#define E1000_RCTL_SECRC 0x04000000u
#define E1000_TCTL_EN    0x00000002u
#define E1000_TCTL_PSP   0x00000008u
#define E1000_TCTL_CT    (0x10u << 4)
#define E1000_TCTL_COLD  (0x40u << 12)
#define E1000_TXD_CMD_EOP 0x01u
#define E1000_TXD_CMD_IFCS 0x02u
#define E1000_TXD_CMD_RS 0x08u
#define E1000_TXD_STAT_DD 0x01u
#define E1000_RXD_STAT_DD 0x01u
#define E1000_RXD_STAT_EOP 0x02u

static netdev_t e1000_dev;
static e1000_rx_desc_t rx_descs[E1000_NUM_RX_DESC] __attribute__((aligned(16)));
static e1000_tx_desc_t tx_descs[E1000_NUM_TX_DESC] __attribute__((aligned(16)));
static uint8_t rx_buffers[E1000_NUM_RX_DESC][E1000_RX_BUFFER_SIZE] __attribute__((aligned(16)));
static uint8_t tx_buffers[E1000_NUM_TX_DESC][E1000_TX_BUFFER_SIZE] __attribute__((aligned(16)));
static volatile uint32_t* e1000_registers;
static uint32_t rx_cur;
static uint32_t tx_cur;
static uint8_t e1000_initialized;

static uint32_t e1000_read_reg(uint32_t offset) {
    if (!e1000_registers || (offset & 3u) != 0u || offset >= E1000_MMIO_SIZE)
        return 0xFFFFFFFFu;
    uint32_t value = e1000_registers[offset / 4u];
    __asm__ volatile("" ::: "memory");
    return value;
}

static void e1000_write_reg(uint32_t offset, uint32_t value) {
    if (!e1000_registers || (offset & 3u) != 0u || offset >= E1000_MMIO_SIZE) return;
    __asm__ volatile("" ::: "memory");
    e1000_registers[offset / 4u] = value;
    __asm__ volatile("" ::: "memory");
    (void)e1000_registers[offset / 4u];
}

static int dma_address(const void* pointer, uint64_t* address_out) {
    if (!pointer || !address_out) return -1;
    uint64_t physical = vmm_virt_to_phys((uint64_t)(uintptr_t)pointer);
    if (!physical || physical >= 0x100000000ULL) return -1;
    *address_out = physical;
    return 0;
}

static int e1000_locate_pci_device(pci_device_t* result) {
    if (!result) return -1;
    for (uint32_t i = 0; i < pci_get_device_count(); ++i) {
        pci_device_t* candidate = pci_get_device(i);
        if (!candidate || candidate->vendor_id != E1000_VENDOR_ID ||
            candidate->device_id != E1000_DEVICE_ID) continue;
        *result = *candidate;
        return 0;
    }
    return -1;
}

static int e1000_enable_pci_bus_master(const pci_device_t* pci) {
    if (!pci) return -1;
    uint32_t command_status = pci_config_read32(pci->bus, pci->slot, pci->func, 0x04u);
    uint32_t command = (command_status & 0xFFFFu) | 0x0006u;
    pci_config_write32(pci->bus, pci->slot, pci->func, 0x04u,
                       (command_status & 0xFFFF0000u) | command);
    uint32_t verify = pci_config_read32(pci->bus, pci->slot, pci->func, 0x04u);
    return ((verify & 0x0006u) == 0x0006u) ? 0 : -1;
}

static int e1000_map_mmio(const pci_device_t* pci) {
    if (!pci || (pci->bar0 & 1u)) return -1;
    uint64_t base = (uint64_t)(pci->bar0 & 0xFFFFFFF0u);
    if (((pci->bar0 >> 1) & 3u) == 2u)
        base |= ((uint64_t)pci->bar1 << 32);
    if (!base || base >= 0x100000000ULL ||
        E1000_MMIO_SIZE > 0x100000000ULL - base) return -1;
    if (vmm_ensure_identity_mapped(base, E1000_MMIO_SIZE) != 0 ||
        vmm_mark_uncached(base, E1000_MMIO_SIZE) != 0) return -1;
    e1000_registers = (volatile uint32_t*)(uintptr_t)base;
    return 0;
}

static int e1000_reset_controller(void) {
    uint32_t ctrl = e1000_read_reg(E1000_REG_CTRL);
    if (ctrl == 0xFFFFFFFFu) return -1;
    e1000_write_reg(E1000_REG_CTRL, ctrl | E1000_CTRL_RST);
    for (uint32_t i = 0; i < E1000_POLL_LIMIT; ++i) {
        if (!(e1000_read_reg(E1000_REG_CTRL) & E1000_CTRL_RST)) {
            (void)e1000_read_reg(E1000_REG_STATUS);
            return 0;
        }
    }
    return -1;
}

static int e1000_read_eeprom_word(uint8_t address, uint16_t* output) {
    if (!output || address > 63u) return -1;
    e1000_write_reg(E1000_REG_EERD, ((uint32_t)address << 8) | 1u);
    for (uint32_t poll = 0; poll < E1000_POLL_LIMIT; ++poll) {
        uint32_t result = e1000_read_reg(E1000_REG_EERD);
        if (result == 0xFFFFFFFFu) return -1;
        if (result & 0x10u) {
            *output = (uint16_t)(result >> 16);
            return 0;
        }
    }
    return -1;
}

static int e1000_read_mac(uint8_t mac[ETH_ALEN]) {
    if (!mac) return -1;

    /* Read the controller's assigned MAC from EEPROM rather than assuming QEMU's
       default address; program RAL/RAH before enabling receive. */
    uint16_t words[3];
    int eeprom_ok = e1000_read_eeprom_word(0u, &words[0]) == 0 &&
                    e1000_read_eeprom_word(1u, &words[1]) == 0 &&
                    e1000_read_eeprom_word(2u, &words[2]) == 0;
    if (eeprom_ok) {
        mac[0] = (uint8_t)words[0];
        mac[1] = (uint8_t)(words[0] >> 8);
        mac[2] = (uint8_t)words[1];
        mac[3] = (uint8_t)(words[1] >> 8);
        mac[4] = (uint8_t)words[2];
        mac[5] = (uint8_t)(words[2] >> 8);
    } else {
        uint32_t low = e1000_read_reg(E1000_REG_RAL0);
        uint32_t high = e1000_read_reg(E1000_REG_RAH0);
        if (low == 0xFFFFFFFFu || high == 0xFFFFFFFFu || !(high & 0x80000000u))
            return -1;
        mac[0] = (uint8_t)low;
        mac[1] = (uint8_t)(low >> 8);
        mac[2] = (uint8_t)(low >> 16);
        mac[3] = (uint8_t)(low >> 24);
        mac[4] = (uint8_t)high;
        mac[5] = (uint8_t)(high >> 8);
    }

    uint8_t any = 0, all_ff = 1;
    for (uint32_t i = 0; i < ETH_ALEN; ++i) {
        any |= mac[i];
        if (mac[i] != 0xFFu) all_ff = 0;
    }
    if (!any || all_ff || (mac[0] & 1u)) return -1;

    uint32_t low = (uint32_t)mac[0] | ((uint32_t)mac[1] << 8) |
                   ((uint32_t)mac[2] << 16) | ((uint32_t)mac[3] << 24);
    uint32_t high = (uint32_t)mac[4] | ((uint32_t)mac[5] << 8) | 0x80000000u;
    e1000_write_reg(E1000_REG_RAL0, low);
    e1000_write_reg(E1000_REG_RAH0, high);
    return 0;
}

static int e1000_initialize_rings(void) {
    uint64_t rx_ring_phys, tx_ring_phys;
    if (dma_address(rx_descs, &rx_ring_phys) != 0 ||
        dma_address(tx_descs, &tx_ring_phys) != 0) return -1;

    for (uint32_t i = 0; i < E1000_NUM_RX_DESC; ++i) {
        uint64_t buffer_phys;
        if (dma_address(rx_buffers[i], &buffer_phys) != 0) return -1;
        rx_descs[i].address = buffer_phys;
        rx_descs[i].length = 0;
        rx_descs[i].checksum = 0;
        rx_descs[i].status = 0;
        rx_descs[i].errors = 0;
        rx_descs[i].special = 0;
    }
    for (uint32_t i = 0; i < E1000_NUM_TX_DESC; ++i) {
        uint64_t buffer_phys;
        if (dma_address(tx_buffers[i], &buffer_phys) != 0) return -1;
        tx_descs[i].address = buffer_phys;
        tx_descs[i].length = 0;
        tx_descs[i].cso = 0;
        tx_descs[i].cmd = 0;
        tx_descs[i].status = E1000_TXD_STAT_DD;
        tx_descs[i].css = 0;
        tx_descs[i].special = 0;
    }

    e1000_write_reg(E1000_REG_IMC, 0xFFFFFFFFu);
    e1000_write_reg(E1000_REG_TDBAL, (uint32_t)tx_ring_phys);
    e1000_write_reg(E1000_REG_TDBAH, (uint32_t)(tx_ring_phys >> 32));
    e1000_write_reg(E1000_REG_TDLEN, (uint32_t)sizeof(tx_descs));
    e1000_write_reg(E1000_REG_TDH, 0);
    e1000_write_reg(E1000_REG_TDT, 0);

    e1000_write_reg(E1000_REG_RDBAL, (uint32_t)rx_ring_phys);
    e1000_write_reg(E1000_REG_RDBAH, (uint32_t)(rx_ring_phys >> 32));
    e1000_write_reg(E1000_REG_RDLEN, (uint32_t)sizeof(rx_descs));
    e1000_write_reg(E1000_REG_RDH, 0);
    e1000_write_reg(E1000_REG_RDT, E1000_NUM_RX_DESC - 1u);
    e1000_write_reg(E1000_REG_TIPG, 0x0060200Au);
    tx_cur = 0;
    rx_cur = 0;
    return 0;
}

void e1000_init(void) {
    if (e1000_initialized) return;
    e1000_registers = (volatile uint32_t*)0;
    for (size_t i = 0; i < sizeof(e1000_dev); ++i)
        ((uint8_t*)&e1000_dev)[i] = 0;

    klog(LOG_INFO, "E1000: searching PCI inventory for Intel 82540EM-compatible controller");
    pci_device_t pci;
    if (e1000_locate_pci_device(&pci) != 0) {
        klog(LOG_WARNING, "E1000: supported PCI controller not found; interface not registered");
        return;
    }
    if (e1000_enable_pci_bus_master(&pci) != 0 || e1000_map_mmio(&pci) != 0) {
        klog(LOG_ERROR, "E1000: PCI bus-master or MMIO initialization failed");
        e1000_registers = (volatile uint32_t*)0;
        return;
    }
    if (e1000_reset_controller() != 0) {
        klog(LOG_ERROR, "E1000: controller reset timed out");
        e1000_registers = (volatile uint32_t*)0;
        return;
    }
    if (e1000_read_mac(e1000_dev.mac_addr) != 0) {
        klog(LOG_ERROR, "E1000: hardware MAC registers are invalid; interface not registered");
        e1000_registers = (volatile uint32_t*)0;
        return;
    }
    if (e1000_initialize_rings() != 0) {
        klog(LOG_ERROR, "E1000: DMA ring addresses could not be resolved below 4 GiB");
        e1000_registers = (volatile uint32_t*)0;
        return;
    }

    e1000_write_reg(E1000_REG_CTRL, e1000_read_reg(E1000_REG_CTRL) |
                                     E1000_CTRL_SLU | E1000_CTRL_ASDE | E1000_CTRL_FD);
    e1000_write_reg(E1000_REG_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP |
                                      E1000_TCTL_CT | E1000_TCTL_COLD);
    e1000_write_reg(E1000_REG_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM |
                                      E1000_RCTL_SECRC);
    uint32_t status = 0;
    for (uint32_t i = 0; i < E1000_POLL_LIMIT / 10u; ++i) {
        status = e1000_read_reg(E1000_REG_STATUS);
        if (status == 0xFFFFFFFFu || (status & E1000_STATUS_LU)) break;
    }

    e1000_dev.ip_addr = 0;
    e1000_dev.netmask = 0;
    e1000_dev.gateway = 0;
    e1000_dev.dns_server = 0;
    e1000_dev.flags = (status != 0xFFFFFFFFu && (status & E1000_STATUS_LU)) ? 0x01u : 0u;
    e1000_dev.send_packet = e1000_send_packet;
    e1000_dev.receive_packet = e1000_receive_packet;

    e1000_initialized = 1;
    if (netdev_register(&e1000_dev) != 0) {
        e1000_initialized = 0;
        e1000_write_reg(E1000_REG_RCTL, 0);
        e1000_write_reg(E1000_REG_TCTL, 0);
        klog(LOG_ERROR, "E1000: network device registry full; controller left disabled");
        return;
    }
    klog(LOG_INFO, "E1000: PCI device initialized with hardware MAC and RX/TX DMA descriptor rings");
}

netdev_t* e1000_get_netdev(void) {
    return e1000_initialized ? &e1000_dev : (void*)0;
}

int e1000_send_packet(netdev_t* dev, const uint8_t* buffer, uint32_t len) {
    if (!e1000_initialized || !e1000_registers || !dev || !buffer ||
        len < sizeof(eth_header_t) || len > 1518u) return -1;

    const uint32_t index = tx_cur;
    volatile e1000_tx_desc_t* descriptor = &tx_descs[index];
    uint32_t polls = 0;
    while (!(descriptor->status & E1000_TXD_STAT_DD) && polls++ < E1000_POLL_LIMIT) {
        if (e1000_read_reg(E1000_REG_STATUS) == 0xFFFFFFFFu) return -1;
    }
    if (!(descriptor->status & E1000_TXD_STAT_DD)) return -2;

    for (uint32_t i = 0; i < len; ++i) tx_buffers[index][i] = buffer[i];
    descriptor->length = (uint16_t)len;
    descriptor->cso = 0;
    descriptor->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
    descriptor->status = 0;
    descriptor->css = 0;
    descriptor->special = 0;
    __asm__ volatile("mfence" ::: "memory");
    e1000_write_reg(E1000_REG_TDT, (index + 1u) % E1000_NUM_TX_DESC);

    polls = 0;
    while (!(descriptor->status & E1000_TXD_STAT_DD) && polls++ < E1000_POLL_LIMIT)
        (void)e1000_read_reg(E1000_REG_STATUS);
    if (!(descriptor->status & E1000_TXD_STAT_DD)) return -2;
    if (descriptor->status & 0x0Eu) return -1;

    tx_cur = (index + 1u) % E1000_NUM_TX_DESC;
    ++dev->tx_packets;
    dev->tx_bytes += len;
    return 0;
}

int e1000_receive_packet(netdev_t* dev, uint8_t* buffer, uint32_t max_len) {
    if (!e1000_initialized || !e1000_registers || !dev || !buffer || !max_len)
        return -1;
    const uint32_t index = rx_cur;
    volatile e1000_rx_desc_t* descriptor = &rx_descs[index];
    __asm__ volatile("mfence" ::: "memory");
    if (!(descriptor->status & E1000_RXD_STAT_DD)) return 0;

    uint32_t length = descriptor->length;
    uint8_t status = descriptor->status;
    uint8_t errors = descriptor->errors;
    int result = -1;
    if ((status & E1000_RXD_STAT_EOP) && errors == 0u &&
        length >= sizeof(eth_header_t) && length <= max_len &&
        length <= E1000_RX_BUFFER_SIZE) {
        for (uint32_t i = 0; i < length; ++i) buffer[i] = rx_buffers[index][i];
        ++dev->rx_packets;
        dev->rx_bytes += length;
        result = (int)length;
    }

    descriptor->length = 0;
    descriptor->checksum = 0;
    descriptor->status = 0;
    descriptor->errors = 0;
    descriptor->special = 0;
    __asm__ volatile("mfence" ::: "memory");
    e1000_write_reg(E1000_REG_RDT, index);
    rx_cur = (index + 1u) % E1000_NUM_RX_DESC;
    return result;
}
