#!/usr/bin/env python3
"""Guard against claiming functional networking while protocol paths are placeholders."""
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[1]
def read(path):
    return (root / path).read_text(encoding="utf-8")
errors = []
dhcp = read("kernel/net/dhcp.c")
dns = read("kernel/net/dns.c")
arp = read("kernel/net/arp.c")
ipv4 = read("kernel/net/ipv4.c")
udp = read("kernel/net/udp.c")
ethernet = read("kernel/net/ethernet.c")
socket = read("kernel/net/socket.c")
e1000 = read("kernel/net/e1000.c")
if "192, 168, 1, 150" in dhcp or "Received DHCPACK" in dhcp:
    errors.append("DHCP must not fabricate a lease")
if "Static known host" in dns or "142, 250, 190, 46" in dns:
    errors.append("DNS must not fabricate host answers")
if "Default Gateway ARP seed" in arp or "52, 0x54, 0x00, 0x12" in arp:
    errors.append("ARP must not seed a fabricated gateway MAC")
if "TCP 3-Way Handshake SYN -> SYN/ACK -> ACK Completed" in socket:
    errors.append("TCP connect must not claim a handshake without packet exchange")
if "return len;" in socket:
    errors.append("unsupported TCP send must not report bytes sent")
if "payload_len > 1500u" not in ethernet or "!dev->send_packet" not in ethernet:
    errors.append("Ethernet transmit path must validate frame bounds and callback")
if "payload_len > 1480u" not in ipv4:
    errors.append("IPv4 transmit path must validate payload bounds")
if "int ipv4_receive_packet(" not in ipv4 or "checksum_valid(packet, header_length)" not in ipv4:
    errors.append("IPv4 receive path must validate packet/header checksums before dispatch")
if "ipv4_receive_packet(dev, payload, payload_len)" not in ethernet:
    errors.append("Ethernet must dispatch inbound IPv4 frames to the IPv4 validator")
if "payload_len > 1472u" not in read("kernel/net/udp.c"):
    errors.append("UDP transmit path must validate payload bounds")
if "int udp_receive_packet(" not in udp or "udp_checksum_valid" not in udp or "udp_register_handler" not in udp:
    errors.append("UDP receive must validate lengths/checksums and dispatch through registered handlers")
if "udp_receive_packet(dev, ip->src_ip, ip->dest_ip" not in ipv4:
    errors.append("IPv4 UDP packets must reach the UDP receive validator")
if "DHCP lease request unavailable" in dhcp or "parse_options" not in dhcp or "DHCP_ACK" not in dhcp:
    errors.append("DHCP must parse and validate real OFFER/ACK messages before applying a lease")
if "pci_get_device_count()" not in e1000 or "REG_TDBAL" not in e1000 or "REG_RDBAL" not in e1000:
    errors.append("e1000 driver must discover PCI hardware and configure DMA rings")
if "192, 168, 1, 150" in e1000 or "MAC: 52:54:00:12:34:56" in e1000:
    errors.append("e1000 must not report fabricated MAC or static network configuration")
if "arp_receive_packet" not in arp or "ntohs(req->opcode)" not in arp:
    errors.append("ARP receive path must validate and process hardware packet headers")
if "void net_poll(void)" not in read("kernel/net/netdev.c") or "net_poll();" not in read("kernel/ui/wm.c"):
    errors.append("hardware packet receive must be polled from the desktop event loop")
if errors:
    print("Network integrity audit FAILED:")
    for error in errors: print(" - " + error)
    sys.exit(1)
print("Network integrity audit passed: e1000 uses PCI/MMIO/DMA setup; unsupported IP services do not fabricate success.")
