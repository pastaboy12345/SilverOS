#include "../include/net.h"
#include "../include/serial.h"

uint16_t net_checksum(void *data, int len) {
    uint16_t *p = (uint16_t *)data;
    uint32_t sum = 0;
    while (len > 1) {
        sum += *p++;
        len -= 2;
    }
    if (len > 0) {
        sum += *(uint8_t *)p;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

ip_addr_t net_my_ip = {10, 0, 2, 15}; // QEMU user networking default
mac_addr_t net_bcast_mac = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void net_init(void) {
    arp_init();
    serial_printf("[NET] Network Stack Initialized. IP: %d.%d.%d.%d\n",
                  net_my_ip[0], net_my_ip[1], net_my_ip[2], net_my_ip[3]);
}

void net_receive_packet(void *data, uint16_t len) {
    if (len < sizeof(ethernet_header_t)) return;
    
    ethernet_header_t *eth = (ethernet_header_t *)data;
    uint16_t type = ntohs(eth->type);
    
    void *payload = (uint8_t *)data + sizeof(ethernet_header_t);
    uint16_t payload_len = len - sizeof(ethernet_header_t);
    
    if (type == 0x0806) {
        // ARP
        arp_handle_packet(payload, payload_len);
    } else if (type == 0x0800) {
        // IPv4
        ipv4_handle_packet(payload, payload_len);
    } else {
        // Ignored
    }
}
