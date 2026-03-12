#include "../include/net.h"
#include "../include/serial.h"

static struct {
    ip_addr_t ip;
    mac_addr_t mac;
    int valid;
} arp_cache[16];

void arp_init(void) {
    for (int i = 0; i < 16; i++) arp_cache[i].valid = 0;
}

void arp_handle_packet(void *data, uint16_t len) {
    if (len < sizeof(arp_packet_t)) return;
    arp_packet_t *arp = (arp_packet_t *)data;
    
    if (ntohs(arp->hw_type) != 1 || ntohs(arp->proto_type) != 0x0800) return;
    
    // Cache sender's MAC
    int cache_idx = -1;
    for (int i = 0; i < 16; i++) {
        if (!arp_cache[i].valid || (arp_cache[i].ip[0] == arp->sender_ip[0] && 
            arp_cache[i].ip[1] == arp->sender_ip[1] &&
            arp_cache[i].ip[2] == arp->sender_ip[2] &&
            arp_cache[i].ip[3] == arp->sender_ip[3])) {
            cache_idx = i;
            break;
        }
    }
    
    if (cache_idx >= 0) {
        for (int i=0; i<4; i++) arp_cache[cache_idx].ip[i] = arp->sender_ip[i];
        for (int i=0; i<6; i++) arp_cache[cache_idx].mac[i] = arp->sender_mac[i];
        arp_cache[cache_idx].valid = 1;
    }
    
    // If request for us, send reply
    if (ntohs(arp->op) == 1) { // Request
        if (arp->target_ip[0] == net_my_ip[0] && arp->target_ip[1] == net_my_ip[1] &&
            arp->target_ip[2] == net_my_ip[2] && arp->target_ip[3] == net_my_ip[3]) {
            
            arp_packet_t reply;
            reply.hw_type = htons(1);
            reply.proto_type = htons(0x0800);
            reply.hw_len = 6;
            reply.proto_len = 4;
            reply.op = htons(2); // Reply
            
            for (int i=0; i<6; i++) {
                reply.sender_mac[i] = rtl8139_mac[i];
                reply.target_mac[i] = arp->sender_mac[i];
            }
            for (int i=0; i<4; i++) {
                reply.sender_ip[i] = net_my_ip[i];
                reply.target_ip[i] = arp->sender_ip[i];
            }
            
            ethernet_send(arp->sender_mac, 0x0806, &reply, sizeof(arp_packet_t));
        }
    }
}

void arp_request(ip_addr_t ip) {
    arp_packet_t req;
    req.hw_type = htons(1);
    req.proto_type = htons(0x0800);
    req.hw_len = 6;
    req.proto_len = 4;
    req.op = htons(1);
    
    for (int i=0; i<6; i++) {
        req.sender_mac[i] = rtl8139_mac[i];
        req.target_mac[i] = 0x00; // Unknown
    }
    for (int i=0; i<4; i++) {
        req.sender_ip[i] = net_my_ip[i];
        req.target_ip[i] = ip[i];
    }
    
    ethernet_send(net_bcast_mac, 0x0806, &req, sizeof(arp_packet_t));
}

int arp_resolve(ip_addr_t ip, mac_addr_t *out_mac) {
    for (int i = 0; i < 16; i++) {
        if (arp_cache[i].valid && 
            arp_cache[i].ip[0] == ip[0] && arp_cache[i].ip[1] == ip[1] &&
            arp_cache[i].ip[2] == ip[2] && arp_cache[i].ip[3] == ip[3]) {
            for (int j=0; j<6; j++) (*out_mac)[j] = arp_cache[i].mac[j];
            return 1;
        }
    }
    
    // Send request if not found
    arp_request(ip);
    return 0;
}
