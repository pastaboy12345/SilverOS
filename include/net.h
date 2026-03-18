#ifndef SILVEROS_NET_H
#define SILVEROS_NET_H

#include "types.h"
#include "rtl8139.h"

// MAC and IP format
typedef uint8_t mac_addr_t[6];
typedef uint8_t ip_addr_t[4];

// Ethernet II Header (14 bytes)
typedef struct __attribute__((packed)) {
    mac_addr_t dest;
    mac_addr_t src;
    uint16_t type; 
} ethernet_header_t;

// ARP Packet (28 bytes)
typedef struct __attribute__((packed)) {
    uint16_t hw_type;   
    uint16_t proto_type; 
    uint8_t hw_len;     
    uint8_t proto_len;  
    uint16_t op;        
    mac_addr_t sender_mac;
    ip_addr_t sender_ip;
    mac_addr_t target_mac;
    ip_addr_t target_ip;
} arp_packet_t;

// IPv4 Header (20 bytes)
typedef struct __attribute__((packed)) {
    uint8_t ihl : 4;
    uint8_t version : 4;
    uint8_t tos;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_fragment;
    uint8_t ttl;
    uint8_t proto; 
    uint16_t checksum;
    ip_addr_t src_ip;
    ip_addr_t dest_ip;
} ipv4_header_t;

// ICMP Header (8 bytes)
typedef struct __attribute__((packed)) {
    uint8_t type; 
    uint8_t code; 
    uint16_t checksum;
    uint16_t identifier;
    uint16_t sequence;
} icmp_header_t;

// UDP Header (8 bytes)
typedef struct __attribute__((packed)) {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} udp_header_t;

extern ip_addr_t net_my_ip;
extern mac_addr_t net_bcast_mac;

// Endianness Helpers
static inline uint16_t htons(uint16_t v) { return (v >> 8) | (v << 8); }
static inline uint16_t ntohs(uint16_t v) { return (v >> 8) | (v << 8); }
static inline uint32_t htonl(uint32_t v) {
    return ((v & 0xFF) << 24) | ((v & 0xFF00) << 8) | ((v >> 8) & 0xFF00) | ((v >> 24) & 0xFF);
}
static inline uint32_t ntohl(uint32_t v) { return htonl(v); }

uint16_t net_checksum(void *data, int len);

void net_init(void); 
void net_receive_packet(void *data, uint16_t len);

void ethernet_send(mac_addr_t dest, uint16_t type, void *payload, uint16_t payload_len);

void arp_init(void);
void arp_handle_packet(void *data, uint16_t len);
void arp_request(ip_addr_t ip);
int arp_resolve(ip_addr_t ip, mac_addr_t *out_mac);

void ipv4_handle_packet(void *data, uint16_t len);
void ipv4_send(ip_addr_t dest, uint8_t proto, void *payload, uint16_t payload_len);
void udp_handle_packet(ipv4_header_t *ip_hdr, void *data, uint16_t len);
void udp_send(ip_addr_t dest, uint16_t src_port, uint16_t dest_port, const void *payload, uint16_t payload_len);

void icmp_handle_packet(ipv4_header_t *ip_hdr, void *data, uint16_t len);
void icmp_send_echo_request(ip_addr_t ip);

#endif
