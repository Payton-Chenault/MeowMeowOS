#ifndef NET_H
#define NET_H

#include <stdint.h>
#include <stdbool.h>

#define IP(a, b, c, d) ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))

typedef struct {
    uint8_t dest[6];
    uint8_t src[6];
    uint16_t ethertype;
} __attribute__((packed)) eth_header_t;

typedef struct {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t opcode;
    uint8_t sender_mac[6];
    uint32_t sender_ip;
    uint8_t target_mac[6];
    uint32_t target_ip;
} __attribute__((packed)) arp_header_t;

typedef struct {
    uint8_t ihl : 4;
    uint8_t version : 4;
    uint8_t tos;
    uint16_t total_length;
    uint16_t id;
    uint16_t frag_offset;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dest_ip;
} __attribute__((packed)) ipv4_header_t;

typedef struct {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
} __attribute__((packed)) icmp_header_t;

uint16_t htons(uint16_t v);
uint32_t htonl(uint32_t v);
uint16_t ntohs(uint16_t v);
uint32_t ntohl(uint32_t v);
uint16_t net_checksum(const uint8_t *data, uint32_t length);

void net_initialize(void);
void net_set_mac(uint8_t *mac);
void net_handle_packet(uint8_t *data, uint32_t len);
int net_ping(uint32_t target_ip, uint32_t *latency_out);

#endif