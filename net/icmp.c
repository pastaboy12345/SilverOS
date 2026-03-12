#include "../include/net.h"
#include "../include/serial.h"

void icmp_handle_packet(ipv4_header_t *ip_hdr, void *data, uint16_t len) {
    if (len < sizeof(icmp_header_t)) return;
    icmp_header_t *icmp = (icmp_header_t *)data;
    
    if (icmp->type == 8) { // Echo Request
        serial_printf("[ICMP] Ping received from %d.%d.%d.%d, sending reply.\n",
                      ip_hdr->src_ip[0], ip_hdr->src_ip[1], ip_hdr->src_ip[2], ip_hdr->src_ip[3]);
                      
        uint8_t reply[1536];
        icmp_header_t *reply_hdr = (icmp_header_t *)reply;
        reply_hdr->type = 0; // Reply
        reply_hdr->code = 0;
        reply_hdr->identifier = icmp->identifier;
        reply_hdr->sequence = icmp->sequence;
        reply_hdr->checksum = 0;
        
        // Copy payload
        uint16_t payload_len = len - sizeof(icmp_header_t);
        for (int i=0; i<payload_len; i++) {
            reply[sizeof(icmp_header_t) + i] = ((uint8_t *)data)[sizeof(icmp_header_t) + i];
        }
        
        reply_hdr->checksum = net_checksum(reply, len);
        
        ipv4_send(ip_hdr->src_ip, 1, reply, len);
    } else if (icmp->type == 0) { // Echo Reply
        serial_printf("[ICMP] Ping reply received from %d.%d.%d.%d (seq=%d)\n",
                      ip_hdr->src_ip[0], ip_hdr->src_ip[1], ip_hdr->src_ip[2], ip_hdr->src_ip[3],
                      ntohs(icmp->sequence));
        /* Print to active terminal context if needed, handled globally typically via events but serial is ok for now */              
    }
}

static uint16_t ping_seq = 1;

void icmp_send_echo_request(ip_addr_t ip) {
    uint8_t packet[64];
    icmp_header_t *icmp = (icmp_header_t *)packet;
    
    icmp->type = 8;
    icmp->code = 0;
    icmp->identifier = htons(0x1234);
    icmp->sequence = htons(ping_seq++);
    icmp->checksum = 0;
    
    for (int i=0; i<32; i++) { // Dummy payload 32 bytes
        packet[sizeof(icmp_header_t) + i] = 'A' + (i % 26);
    }
    
    uint16_t len = sizeof(icmp_header_t) + 32;
    icmp->checksum = net_checksum(packet, len);
    
    serial_printf("[ICMP] Pinging %d.%d.%d.%d...\n", ip[0], ip[1], ip[2], ip[3]);
    ipv4_send(ip, 1, packet, len);
}
