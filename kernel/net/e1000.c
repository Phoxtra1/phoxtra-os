#include "e1000.h"
#include "../log/logger.h"
#include "../drivers/pci.h"
#include "../memory/vmm.h"

#define E1000_NUM_RX_DESC 32
#define E1000_NUM_TX_DESC 32
#define E1000_DMA_BUFFER_SIZE 2048u
#define E1000_MMIO_WINDOW 0x20000ULL
#define E1000_POLL_LIMIT 1000000u

#define REG_CTRL   0x0000
#define REG_STATUS 0x0008
#define REG_EERD   0x0014
#define REG_RCTL   0x0100
#define REG_TCTL   0x0400
#define REG_TIPG   0x0410
#define REG_RDBAL  0x2800
#define REG_RDBAH  0x2804
#define REG_RDLEN  0x2808
#define REG_RDH    0x2810
#define REG_RDT    0x2818
#define REG_TDBAL  0x3800
#define REG_TDBAH  0x3804
#define REG_TDLEN  0x3808
#define REG_TDH    0x3810
#define REG_TDT    0x3818
#define REG_RAL    0x5400
#define REG_RAH    0x5404

#define CTRL_FD    (1u << 0)
#define CTRL_SLU   (1u << 6)
#define CTRL_RST   (1u << 26)
#define STATUS_LU  (1u << 1)
#define RCTL_EN    (1u << 1)
#define RCTL_BAM   (1u << 15)
#define RCTL_SECRC (1u << 26)
#define TCTL_EN    (1u << 1)
#define TCTL_PSP   (1u << 3)
#define TX_CMD_EOP (1u << 0)
#define TX_CMD_IFCS (1u << 1)
#define TX_CMD_RS  (1u << 3)
#define DESC_DONE  (1u << 0)

static netdev_t e1000_dev;
static e1000_rx_desc_t rx_descs[E1000_NUM_RX_DESC] __attribute__((aligned(16)));
static e1000_tx_desc_t tx_descs[E1000_NUM_TX_DESC] __attribute__((aligned(16)));
static uint8_t rx_buffers[E1000_NUM_RX_DESC][E1000_DMA_BUFFER_SIZE] __attribute__((aligned(16)));
static uint8_t tx_buffers[E1000_NUM_TX_DESC][E1000_DMA_BUFFER_SIZE] __attribute__((aligned(16)));
static volatile uint32_t* mmio;
static uint32_t rx_cur;
static uint32_t tx_cur;
static uint8_t e1000_initialized;

static uint32_t reg_read(uint32_t offset) {
    return mmio[offset / sizeof(uint32_t)];
}
static void reg_write(uint32_t offset, uint32_t value) {
    mmio[offset / sizeof(uint32_t)] = value;
    asm volatile("mfence" ::: "memory");
}
static int identity_dma_range(const void* pointer, uint64_t size) {
    uint64_t start = (uint64_t)(uintptr_t)pointer;
    if (!size || start >= 0x100000000ULL || size > 0x100000000ULL - start) return 0;
    return vmm_ensure_identity_mapped(start, size) == 0;
}
static int find_controller(pci_device_t* out) {
    for (uint32_t i = 0; i < pci_get_device_count(); ++i) {
        pci_device_t* dev = pci_get_device(i);
        if (dev && dev->vendor_id == E1000_VENDOR_ID &&
            dev->device_id == E1000_DEVICE_ID) {
            *out = *dev;
            return 1;
        }
    }
    return 0;
}
static void fail_init(const char* reason) {
    mmio = 0;
    e1000_initialized = 0;
    klog(LOG_WARNING, reason);
}

void e1000_init(void) {
    e1000_initialized = 0;
    mmio = 0;
    rx_cur = 0;
    tx_cur = 0;
    klog(LOG_INFO, "Probing PCI for Intel 82540EM (8086:100E) Ethernet hardware");

    pci_device_t pci;
    if (!find_controller(&pci)) {
        klog(LOG_INFO, "No supported Intel 82540EM controller detected; Ethernet interface not registered");
        return;
    }

    uint64_t mmio_base = (uint64_t)(pci.bar0 & ~0x0Fu);
    uint32_t bar_type = (pci.bar0 >> 1) & 0x3u;
    if ((pci.bar0 & 1u) || bar_type == 1u || !mmio_base) {
        fail_init("e1000 initialization failed: unsupported I/O or reserved BAR type");
        return;
    }
    if (bar_type == 2u) mmio_base |= (uint64_t)pci.bar1 << 32;
    if (mmio_base >= 0x100000000ULL ||
        E1000_MMIO_WINDOW > 0x100000000ULL - mmio_base) {
        fail_init("e1000 initialization failed: MMIO BAR is outside the identity-mapped 32-bit window");
        return;
    }
    if (vmm_ensure_identity_mapped(mmio_base, E1000_MMIO_WINDOW) != 0 ||
        vmm_mark_uncached(mmio_base, E1000_MMIO_WINDOW) != 0) {
        fail_init("e1000 initialization failed: MMIO mapping is not safely available");
        return;
    }
    if (!identity_dma_range(rx_descs, sizeof(rx_descs)) ||
        !identity_dma_range(tx_descs, sizeof(tx_descs)) ||
        !identity_dma_range(rx_buffers, sizeof(rx_buffers)) ||
        !identity_dma_range(tx_buffers, sizeof(tx_buffers))) {
        fail_init("e1000 initialization failed: DMA buffers are not identity-mapped below 4 GiB");
        return;
    }

    /* Enable PCI memory-space access and bus mastering without rewriting W1C status bits. */
    uint32_t command_status = pci_config_read32(pci.bus, pci.slot, pci.func, 0x04);
    uint16_t command = (uint16_t)command_status;
    command |= 0x0006u; /* Memory Space + Bus Master */
    pci_config_write32(pci.bus, pci.slot, pci.func, 0x04, command);

    mmio = (volatile uint32_t*)(uintptr_t)mmio_base;
    reg_write(REG_CTRL, reg_read(REG_CTRL) | CTRL_RST);
    uint32_t spins = 0;
    while ((reg_read(REG_CTRL) & CTRL_RST) && spins++ < E1000_POLL_LIMIT) { }
    if (reg_read(REG_CTRL) & CTRL_RST) {
        fail_init("e1000 initialization failed: controller reset timed out");
        return;
    }

    uint32_t ral = reg_read(REG_RAL);
    uint32_t rah = reg_read(REG_RAH);
    uint8_t mac[ETH_ALEN] = {
        (uint8_t)ral, (uint8_t)(ral >> 8), (uint8_t)(ral >> 16), (uint8_t)(ral >> 24),
        (uint8_t)rah, (uint8_t)(rah >> 8)
    };
    int mac_zero = 1, mac_ones = 1;
    for (uint32_t i = 0; i < ETH_ALEN; ++i) {
        if (mac[i] != 0) mac_zero = 0;
        if (mac[i] != 0xFF) mac_ones = 0;
    }
    if (mac_zero || mac_ones || !(rah & (1u << 31))) {
        fail_init("e1000 initialization failed: controller did not expose a valid programmed MAC address");
        return;
    }

    for (uint32_t i = 0; i < E1000_NUM_RX_DESC; ++i) {
        rx_descs[i].address = (uint64_t)(uintptr_t)rx_buffers[i];
        rx_descs[i].length = 0;
        rx_descs[i].checksum = 0;
        rx_descs[i].status = 0;
        rx_descs[i].errors = 0;
        rx_descs[i].special = 0;
    }
    for (uint32_t i = 0; i < E1000_NUM_TX_DESC; ++i) {
        tx_descs[i].address = (uint64_t)(uintptr_t)tx_buffers[i];
        tx_descs[i].length = 0;
        tx_descs[i].cso = 0;
        tx_descs[i].cmd = 0;
        tx_descs[i].status = DESC_DONE;
        tx_descs[i].css = 0;
        tx_descs[i].special = 0;
    }

    reg_write(REG_RDBAL, (uint32_t)(uintptr_t)rx_descs);
    reg_write(REG_RDBAH, (uint32_t)((uint64_t)(uintptr_t)rx_descs >> 32));
    reg_write(REG_RDLEN, sizeof(rx_descs));
    reg_write(REG_RDH, 0);
    reg_write(REG_RDT, E1000_NUM_RX_DESC - 1u);

    reg_write(REG_TDBAL, (uint32_t)(uintptr_t)tx_descs);
    reg_write(REG_TDBAH, (uint32_t)((uint64_t)(uintptr_t)tx_descs >> 32));
    reg_write(REG_TDLEN, sizeof(tx_descs));
    reg_write(REG_TDH, 0);
    reg_write(REG_TDT, 0);
    reg_write(REG_TIPG, 10u | (8u << 10) | (6u << 20));
    reg_write(REG_RCTL, RCTL_EN | RCTL_BAM | RCTL_SECRC);
    reg_write(REG_TCTL, TCTL_EN | TCTL_PSP | (0x10u << 4) | (0x40u << 12));
    reg_write(REG_CTRL, reg_read(REG_CTRL) | CTRL_SLU | CTRL_FD);

    for (uint32_t i = 0; i < ETH_ALEN; ++i) e1000_dev.mac_addr[i] = mac[i];
    e1000_dev.name[0]='e'; e1000_dev.name[1]='t'; e1000_dev.name[2]='h'; e1000_dev.name[3]='0'; e1000_dev.name[4]='\0';
    e1000_dev.ip_addr = 0; /* Must be configured by a real DHCP/static configuration path. */
    e1000_dev.netmask = 0;
    e1000_dev.gateway = 0;
    e1000_dev.dns_server = 0;
    e1000_dev.flags = (reg_read(REG_STATUS) & STATUS_LU) ? 0x01u : 0u;
    e1000_dev.send_packet = e1000_send_packet;
    e1000_dev.receive_packet = e1000_receive_packet;
    e1000_dev.rx_packets = e1000_dev.tx_packets = 0;
    e1000_dev.rx_bytes = e1000_dev.tx_bytes = 0;

    if (netdev_register(&e1000_dev) != 0) {
        reg_write(REG_RCTL, 0);
        reg_write(REG_TCTL, 0);
        fail_init("e1000 initialization failed: network interface registry is full");
        return;
    }
    e1000_initialized = 1;
    klog(LOG_INFO, "e1000 hardware driver initialized: PCI discovery, MMIO, DMA rings, and MAC address configured");
    klog(LOG_INFO, (e1000_dev.flags & 0x01u)
        ? "e1000 physical link reports UP; IPv4 configuration is not yet assigned"
        : "e1000 controller initialized; physical link reports DOWN");
}

netdev_t* e1000_get_netdev(void) {
    return e1000_initialized ? &e1000_dev : (netdev_t*)0;
}

int e1000_send_packet(netdev_t* dev, const uint8_t* buffer, uint32_t len) {
    if (!e1000_initialized || !mmio || !dev || !buffer ||
        len < 14u || len > E1000_DMA_BUFFER_SIZE) return -1;

    uint32_t idx = tx_cur;
    e1000_tx_desc_t* desc = &tx_descs[idx];
    uint32_t spins = 0;
    while (!(desc->status & DESC_DONE) && spins++ < E1000_POLL_LIMIT) { }
    if (!(desc->status & DESC_DONE)) return -1;

    for (uint32_t i = 0; i < len; ++i) tx_buffers[idx][i] = buffer[i];
    desc->address = (uint64_t)(uintptr_t)tx_buffers[idx];
    desc->length = (uint16_t)len;
    desc->cso = 0;
    desc->cmd = TX_CMD_EOP | TX_CMD_IFCS | TX_CMD_RS;
    desc->status = 0;
    asm volatile("mfence" ::: "memory");
    tx_cur = (idx + 1u) % E1000_NUM_TX_DESC;
    reg_write(REG_TDT, tx_cur);

    spins = 0;
    while (!(desc->status & DESC_DONE) && spins++ < E1000_POLL_LIMIT) { }
    if (!(desc->status & DESC_DONE)) return -1;

    ++dev->tx_packets;
    dev->tx_bytes += len;
    return 0;
}

int e1000_receive_packet(netdev_t* dev, uint8_t* buffer, uint32_t max_len) {
    if (!e1000_initialized || !mmio || !dev || !buffer || !max_len) return -1;
    e1000_rx_desc_t* desc = &rx_descs[rx_cur];
    if (!(desc->status & DESC_DONE)) return 0;

    uint32_t length = desc->length;
    uint8_t errors = desc->errors;
    if (!length || length > E1000_DMA_BUFFER_SIZE || errors || length > max_len) {
        desc->status = 0;
        reg_write(REG_RDT, rx_cur);
        rx_cur = (rx_cur + 1u) % E1000_NUM_RX_DESC;
        return -1;
    }
    for (uint32_t i = 0; i < length; ++i) buffer[i] = rx_buffers[rx_cur][i];
    desc->status = 0;
    desc->length = 0;
    desc->errors = 0;
    reg_write(REG_RDT, rx_cur);
    rx_cur = (rx_cur + 1u) % E1000_NUM_RX_DESC;
    ++dev->rx_packets;
    dev->rx_bytes += length;
    return (int)length;
}
