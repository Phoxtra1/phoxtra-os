#ifndef PHOXTRA_AHCI_H
#define PHOXTRA_AHCI_H

#include "blockdev.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AHCI_PCI_CLASS     0x01
#define AHCI_PCI_SUBCLASS  0x06
#define AHCI_PCI_PROGIF    0x01

#define AHCI_DEV_SATA      0x00000101
#define AHCI_DEV_SATAPI    0xEB140101

#define ATA_CMD_READ_DMA_EX  0x25
#define ATA_CMD_WRITE_DMA_EX 0x35
#define ATA_CMD_IDENTIFY     0xEC

typedef struct [[gnu::packed]] {
    uint32_t clb;
    uint32_t clbu;
    uint32_t fb;
    uint32_t fbu;
    uint32_t is;
    uint32_t ie;
    uint32_t cmd;
    uint32_t rsv0;
    uint32_t tfd;
    uint32_t sig;
    uint32_t ssts;
    uint32_t sctl;
    uint32_t serr;
    uint32_t sact;
    uint32_t ci;
    uint32_t sntf;
    uint32_t fbs;
    uint32_t rsv1[11];
    uint32_t vendor[4];
} hba_port_t;

typedef struct [[gnu::packed]] {
    uint32_t cap;
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;
    uint32_t vs;
    uint32_t ccc_ctl;
    uint32_t ccc_ports;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap2;
    uint32_t bohc;
    uint8_t  rsv[0xA0 - 0x2C];
    uint8_t  vendor[0x100 - 0xA0];
    hba_port_t ports[32];
} hba_mem_t;

void ahci_init(void);
int ahci_read_sectors(block_device_t* dev, uint64_t lba, uint32_t count, uint8_t* buffer);
int ahci_write_sectors(block_device_t* dev, uint64_t lba, uint32_t count, const uint8_t* buffer);

#ifdef __cplusplus
}
#endif

#endif
