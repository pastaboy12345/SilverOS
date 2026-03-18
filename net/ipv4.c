#include "../include/net.h"
#include "../include/serial.h"

void ipv4_handle_packet(void *data, uint16_t len) {
    uint16_t total_length;
    uint16_t payload_len;

    if (len < sizeof(ipv4_header_t)) return;
    ipv4_header_t *ip = (ipv4_header_t *)data;
    
    if (ip->version != 4) return;
    
    uint8_t ihl_bytes = ip->ihl * 4;
    if (len < ihl_bytes) return;
    total_length = ntohs(ip->total_length);
    if (total_length < ihl_bytes) return;
    if (total_length > len) total_length = len;
    payload_len = total_length - ihl_bytes;
    
    if (ip->dest_ip[0] == net_my_ip[0] && ip->dest_ip[1] == net_my_ip[1] &&
        ip->dest_ip[2] == net_my_ip[2] && ip->dest_ip[3] == net_my_ip[3]) {
        
        void *payload = (uint8_t *)data + ihl_bytes;
        
        if (ip->proto == 1) { // ICMP
            icmp_handle_packet(ip, payload, payload_len);
        } else if (ip->proto == 17) { // UDP
            udp_handle_packet(ip, payload, payload_len);
        }
    }
}

static uint16_t ip_id = 0;

void ipv4_send(ip_addr_t dest, uint8_t proto, void *payload, uint16_t payload_len) {
    uint8_t packet[1536];
    ipv4_header_t *ip = (ipv4_header_t *)packet;
    
    ip->ihl = 5;
    ip->version = 4;
    ip->tos = 0;
    ip->total_length = htons(sizeof(ipv4_header_t) + payload_len);
    ip->identification = htons(ip_id++);
    ip->flags_fragment = 0;
    ip->ttl = 64;
    ip->proto = proto;
    ip->checksum = 0;
    for (int i=0; i<4; i++) {
        ip->src_ip[i] = net_my_ip[i];
        ip->dest_ip[i] = dest[i];
    }
    
    ip->checksum = net_checksum(ip, sizeof(ipv4_header_t));
    
    for (int i=0; i<payload_len; i++) {
        packet[sizeof(ipv4_header_t) + i] = ((uint8_t *)payload)[i];
    }
    
    mac_addr_t dest_mac;
    if (arp_resolve(dest, &dest_mac)) {
        ethernet_send(dest_mac, 0x0800, packet, sizeof(ipv4_header_t) + payload_len);
    } else {
        serial_printf("[IPv4] Dropping packet to %d.%d.%d.%d, waiting for ARP resolution\n",
                      dest[0], dest[1], dest[2], dest[3]);
    }
}
