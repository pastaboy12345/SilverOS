#include "../include/net.h"
#include "../include/string.h"

void udp_handle_packet(ipv4_header_t *ip_hdr, void *data, uint16_t len) {
    udp_header_t *udp;
    uint8_t *payload;
    uint16_t payload_len;

    if (!ip_hdr || !data || len < sizeof(udp_header_t)) {
        return;
    }

    udp = (udp_header_t *)data;
    payload = (uint8_t *)data + sizeof(udp_header_t);
    payload_len = len - sizeof(udp_header_t);
    (void)udp;
    (void)payload;
    (void)payload_len;
    (void)ip_hdr;
}

void udp_send(ip_addr_t dest, uint16_t src_port, uint16_t dest_port, const void *payload, uint16_t payload_len) {
    uint8_t packet[1536];
    udp_header_t *udp;

    if (payload_len + sizeof(udp_header_t) > sizeof(packet)) {
        return;
    }

    udp = (udp_header_t *)packet;
    udp->src_port = htons(src_port);
    udp->dest_port = htons(dest_port);
    udp->length = htons((uint16_t)(sizeof(udp_header_t) + payload_len));
    udp->checksum = 0;

    if (payload_len > 0) {
        memcpy(packet + sizeof(udp_header_t), payload, payload_len);
    }

    ipv4_send(dest, 17, packet, (uint16_t)(sizeof(udp_header_t) + payload_len));
}
