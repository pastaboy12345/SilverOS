#include "../include/net.h"

void ethernet_send(mac_addr_t dest, uint16_t type, void *payload, uint16_t payload_len) {
    uint8_t frame[1536];
    ethernet_header_t *hdr = (ethernet_header_t *)frame;
    
    for (int i = 0; i < 6; i++) {
        hdr->dest[i] = dest[i];
        hdr->src[i] = rtl8139_mac[i];
    }
    hdr->type = htons(type);
    
    for (int i = 0; i < payload_len; i++) {
        frame[14 + i] = ((uint8_t *)payload)[i];
    }
    
    rtl8139_send_packet(frame, 14 + payload_len);
}
