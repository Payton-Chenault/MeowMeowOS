#include "net.h"
#include "../lib/string/string.h"
#include "../utils/logging/logger.h"
#include "../kernel_services/kernel_services.h"
#include "../arch/x86/pit/pit.h"
#include "../arch/x86/task/task.h"

#define MODULE "NET"

extern void rtl8139_send(uint8_t *data, uint32_t len);

static uint8_t my_mac[6];

// Static networking configuration for QEMU SLIRP
static uint32_t my_ip = IP(10, 0, 2, 15);
static uint32_t my_netmask = IP(255, 255, 255, 0);
static uint32_t my_gateway = IP(10, 0, 2, 2);
static uint32_t my_dns_server = IP(10, 0, 2, 3);

static volatile bool arp_received_flag = false;
static volatile bool icmp_received_flag = false;

static volatile bool dns_resolved_flag = false;
static volatile uint32_t dns_resolved_ip = 0;
static volatile uint16_t dns_query_id = 0x4D4F; // 'MO' (MeowOS)
static volatile uint16_t dns_client_port = 53123;

typedef struct {
    uint32_t ip;
    uint8_t mac[6];
    bool valid;
} arp_entry_t;

static arp_entry_t arp_cache[16];

uint16_t htons(uint16_t v) {
    return ((v & 0xFF) << 8) | ((v & 0xFF00) >> 8);
}

uint32_t htonl(uint32_t v) {
    return ((v & 0xFF) << 24) | ((v & 0xFF00) << 8) | ((v >> 8) & 0xFF00) | ((v >> 24) & 0xFF);
}

uint16_t ntohs(uint16_t v) { return htons(v); }
uint32_t ntohl(uint32_t v) { return htonl(v); }

uint16_t net_checksum(const uint8_t *data, uint32_t length) {
    uint32_t sum = 0;
    const uint16_t *p = (const uint16_t *)data;
    while (length > 1) {
        sum += *p++;
        length -= 2;
    }
    if (length > 0) {
        sum += *(const uint8_t *)p;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return ~sum;
}

void net_set_mac(uint8_t *mac) {
    memcpy(my_mac, mac, 6);
    log_debug(MODULE, "Network stack MAC address configured successfully");
}

static void arp_cache_update(uint32_t ip, uint8_t *mac) {
    for (int i = 0; i < 16; i++) {
        if (!arp_cache[i].valid || arp_cache[i].ip == ip) {
            arp_cache[i].ip = ip;
            memcpy(arp_cache[i].mac, mac, 6);
            arp_cache[i].valid = true;
            log_trace(MODULE, "Updated ARP cache entry for IP %u.%u.%u.%u",
                      ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24) & 0xFF);
            return;
        }
    }
    log_warning(MODULE, "ARP cache full, failed to insert IP entry");
}

static uint8_t *arp_lookup(uint32_t ip) {
    for (int i = 0; i < 16; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            return arp_cache[i].mac;
        }
    }
    return NULL;
}

static void net_send_packet(uint8_t *dest_mac, uint16_t ethertype, uint8_t *payload, uint32_t payload_len) {
    uint32_t frame_len = sizeof(eth_header_t) + payload_len;
    uint8_t *frame = kmem_zalloc(frame_len);
    if (!frame) {
        log_error(MODULE, "net_send_packet: failed to allocate memory for frame transmission");
        return;
    }

    eth_header_t *eth = (eth_header_t *)frame;
    memcpy(eth->dest, dest_mac, 6);
    memcpy(eth->src, my_mac, 6);
    eth->ethertype = htons(ethertype);
    memcpy(frame + sizeof(eth_header_t), payload, payload_len);

    rtl8139_send(frame, frame_len);
    kmem_free(frame);
}

static void arp_send_request(uint32_t target_ip) {
    arp_header_t arp;
    arp.htype = htons(1);
    arp.ptype = htons(0x0800);
    arp.hlen = 6;
    arp.plen = 4;
    arp.opcode = htons(1); // Request
    memcpy(arp.sender_mac, my_mac, 6);
    arp.sender_ip = my_ip;
    memset(arp.target_mac, 0, 6);
    arp.target_ip = target_ip;

    uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    net_send_packet(bcast, 0x0806, (uint8_t *)&arp, sizeof(arp_header_t));
    log_trace(MODULE, "Broadcasted ARP Request for IP %d.%d.%d.%d",
              target_ip & 0xFF, (target_ip >> 8) & 0xFF, (target_ip >> 16) & 0xFF, (target_ip >> 24) & 0xFF);
}

void net_send_udp_packet(uint32_t target_ip, uint16_t src_port, uint16_t dest_port, const uint8_t *payload, uint16_t payload_len) {
    uint32_t udp_len = sizeof(udp_header_t) + payload_len;
    uint32_t ip_len = sizeof(ipv4_header_t) + udp_len;
    uint8_t *packet = kmem_zalloc(ip_len);
    if (!packet) {
        log_error(MODULE, "net_send_udp_packet: failed to allocate memory for UDP packet");
        return;
    }

    ipv4_header_t *ip = (ipv4_header_t *)packet;
    ip->ihl = 5;
    ip->version = 4;
    ip->tos = 0;
    ip->total_length = htons(ip_len);
    ip->id = htons(0x4321);
    ip->frag_offset = 0;
    ip->ttl = 64;
    ip->protocol = 17; // UDP
    ip->src_ip = my_ip;
    ip->dest_ip = target_ip;
    ip->checksum = 0;
    ip->checksum = net_checksum((uint8_t *)ip, sizeof(ipv4_header_t));

    udp_header_t *udp = (udp_header_t *)(packet + sizeof(ipv4_header_t));
    udp->src_port = htons(src_port);
    udp->dest_port = htons(dest_port);
    udp->length = htons(udp_len);
    udp->checksum = 0; // Checksum optional in IPv4

    memcpy(packet + sizeof(ipv4_header_t) + sizeof(udp_header_t), payload, payload_len);

    uint32_t next_hop_ip = target_ip;
    if ((target_ip & my_netmask) != (my_ip & my_netmask)) {
        next_hop_ip = my_gateway;
    }

    uint8_t *mac = arp_lookup(next_hop_ip);
    if (!mac) {
        arp_received_flag = false;
        arp_send_request(next_hop_ip);
        uint32_t timeout = get_ticks() + 1000;
        while (!arp_received_flag && get_ticks() < timeout) {
            task_yield();
        }
        mac = arp_lookup(next_hop_ip);
        if (!mac) {
            log_warning(MODULE, "net_send_udp_packet: ARP resolution failed for destination");
            kmem_free(packet);
            return;
        }
    }

    net_send_packet(mac, 0x0800, packet, ip_len);
    kmem_free(packet);
    log_trace(MODULE, "Transmitted UDP packet to %u.%u.%u.%u:%u (%u bytes)",
              target_ip & 0xFF, (target_ip >> 8) & 0xFF, (target_ip >> 16) & 0xFF, (target_ip >> 24) & 0xFF,
              dest_port, payload_len);
}

static void icmp_send_echo_request(uint32_t target_ip, uint8_t *target_mac, uint16_t id, uint16_t seq) {
    uint32_t icmp_len = sizeof(icmp_header_t);
    uint32_t ip_len = sizeof(ipv4_header_t) + icmp_len;
    uint8_t *packet = kmem_zalloc(ip_len);
    if (!packet) {
        log_error(MODULE, "icmp_send_echo_request: out of memory for packet allocation");
        return;
    }

    ipv4_header_t *ip = (ipv4_header_t *)packet;
    ip->ihl = 5;
    ip->version = 4;
    ip->tos = 0;
    ip->total_length = htons(ip_len);
    ip->id = htons(0x1234);
    ip->frag_offset = 0;
    ip->ttl = 64;
    ip->protocol = 1; // ICMP
    ip->src_ip = my_ip;
    ip->dest_ip = target_ip;
    ip->checksum = 0;
    ip->checksum = net_checksum((uint8_t *)ip, sizeof(ipv4_header_t));

    icmp_header_t *icmp = (icmp_header_t *)(packet + sizeof(ipv4_header_t));
    icmp->type = 8; // Echo Request
    icmp->code = 0;
    icmp->id = htons(id);
    icmp->sequence = htons(seq);
    icmp->checksum = 0;
    icmp->checksum = net_checksum((uint8_t *)icmp, sizeof(icmp_header_t));

    net_send_packet(target_mac, 0x0800, packet, ip_len);
    kmem_free(packet);
    log_trace(MODULE, "Sent ICMP Echo Request to IP %d.%d.%d.%d",
              target_ip & 0xFF, (target_ip >> 8) & 0xFF, (target_ip >> 16) & 0xFF, (target_ip >> 24) & 0xFF);
}

static void dns_handle_packet(const uint8_t *payload, uint32_t len) {
    if (len < sizeof(dns_header_t)) return;

    dns_header_t *dns = (dns_header_t *)payload;
    if (ntohs(dns->id) != dns_query_id) {
        log_trace(MODULE, "Ignored DNS response with mismatched transaction ID 0x%X", ntohs(dns->id));
        return;
    }

    uint16_t ancount = ntohs(dns->ancount);
    if (ancount == 0) {
        log_warning(MODULE, "DNS server returned 0 answers");
        return;
    }

    uint32_t pos = sizeof(dns_header_t);

    // Skip Query / Question section
    uint16_t qdcount = ntohs(dns->qdcount);
    for (uint16_t q = 0; q < qdcount; q++) {
        while (pos < len && payload[pos] != 0) {
            pos += payload[pos] + 1;
        }
        pos += 1; // Skip terminal 0x00
        pos += 4; // Skip QTYPE and QCLASS
    }

    // Parse Answer section looking for Type 1 (A record)
    for (uint16_t a = 0; a < ancount && pos < len; a++) {
        if ((payload[pos] & 0xC0) == 0xC0) {
            pos += 2; // Compressed pointer
        } else {
            while (pos < len && payload[pos] != 0) {
                pos += payload[pos] + 1;
            }
            pos++;
        }

        if (pos + 10 > len) break;

        uint16_t type = (payload[pos] << 8) | payload[pos + 1];
        uint16_t class = (payload[pos + 2] << 8) | payload[pos + 3];
        uint16_t rdlength = (payload[pos + 8] << 8) | payload[pos + 9];
        pos += 10;

        if (type == 1 && class == 1 && rdlength == 4 && pos + 4 <= len) {
            dns_resolved_ip = payload[pos] | (payload[pos + 1] << 8) |
                              (payload[pos + 2] << 16) | (payload[pos + 3] << 24);
            dns_resolved_flag = true;
            log_info(MODULE, "DNS resolved address to %u.%u.%u.%u",
                     dns_resolved_ip & 0xFF, (dns_resolved_ip >> 8) & 0xFF,
                     (dns_resolved_ip >> 16) & 0xFF, (dns_resolved_ip >> 24) & 0xFF);
            return;
        }
        pos += rdlength;
    }
}

void net_handle_packet(uint8_t *data, uint32_t len) {
    if (len < sizeof(eth_header_t)) return;
    eth_header_t *eth = (eth_header_t *)data;
    uint16_t ethtype = ntohs(eth->ethertype);

    if (ethtype == 0x0806) { // ARP
        arp_header_t *arp = (arp_header_t *)(data + sizeof(eth_header_t));
        if (ntohs(arp->opcode) == 1 && arp->target_ip == my_ip) {
            log_trace(MODULE, "Responding to incoming ARP Request");
            arp->opcode = htons(2);
            memcpy(arp->target_mac, arp->sender_mac, 6);
            arp->target_ip = arp->sender_ip;
            memcpy(arp->sender_mac, my_mac, 6);
            arp->sender_ip = my_ip;
            net_send_packet(arp->target_mac, 0x0806, (uint8_t *)arp, sizeof(arp_header_t));
        } else if (ntohs(arp->opcode) == 2) {
            log_trace(MODULE, "Received ARP Reply from IP %d.%d.%d.%d",
                      arp->sender_ip & 0xFF, (arp->sender_ip >> 8) & 0xFF, (arp->sender_ip >> 16) & 0xFF, (arp->sender_ip >> 24) & 0xFF);
            arp_cache_update(arp->sender_ip, arp->sender_mac);
            arp_received_flag = true;
        }
    } else if (ethtype == 0x0800) { // IPv4
        ipv4_header_t *ip = (ipv4_header_t *)(data + sizeof(eth_header_t));
        if (ip->dest_ip == my_ip) {
            if (ip->protocol == 1) { // ICMP
                icmp_header_t *icmp = (icmp_header_t *)(data + sizeof(eth_header_t) + (ip->ihl * 4));
                if (icmp->type == 8) {
                    log_trace(MODULE, "Received ICMP Echo Request. Sending Reply.");
                    icmp->type = 0;
                    icmp->checksum = 0;
                    icmp->checksum = net_checksum((uint8_t *)icmp, ntohs(ip->total_length) - (ip->ihl * 4));

                    uint32_t tmp_ip = ip->src_ip;
                    ip->src_ip = my_ip;
                    ip->dest_ip = tmp_ip;
                    ip->checksum = 0;
                    ip->checksum = net_checksum((uint8_t *)ip, ip->ihl * 4);

                    net_send_packet(eth->src, 0x0800, (uint8_t *)ip, ntohs(ip->total_length));
                } else if (icmp->type == 0) {
                    log_trace(MODULE, "Received ICMP Echo Reply.");
                    icmp_received_flag = true;
                }
            } else if (ip->protocol == 17) { // UDP
                udp_header_t *udp = (udp_header_t *)(data + sizeof(eth_header_t) + (ip->ihl * 4));
                uint16_t dest_port = ntohs(udp->dest_port);
                uint16_t udp_len = ntohs(udp->length);
                if (udp_len >= sizeof(udp_header_t)) {
                    uint32_t payload_len = udp_len - sizeof(udp_header_t);
                    const uint8_t *payload = (const uint8_t *)udp + sizeof(udp_header_t);
                    if (dest_port == dns_client_port) {
                        dns_handle_packet(payload, payload_len);
                    }
                }
            }
        }
    }
}

static int dns_format_qname(const char *hostname, uint8_t *qname_out) {
    int out_idx = 0;
    const char *curr = hostname;
    while (*curr) {
        const char *next_dot = strchr(curr, '.');
        int len = next_dot ? (next_dot - curr) : (int)strlen(curr);
        if (len > 63 || len == 0) return -1;
        qname_out[out_idx++] = (uint8_t)len;
        for (int i = 0; i < len; i++) {
            qname_out[out_idx++] = curr[i];
        }
        if (!next_dot) break;
        curr = next_dot + 1;
    }
    qname_out[out_idx++] = 0;
    return out_idx;
}

int net_dns_resolve(const char *hostname, uint32_t *ip_out) {
    if (!hostname || !ip_out) return -1;

    log_info(MODULE, "Querying DNS server (10.0.2.3) for '%s'...", hostname);

    uint8_t query_buf[512];
    dns_header_t *dns = (dns_header_t *)query_buf;
    dns->id = htons(dns_query_id);
    dns->flags = htons(0x0100); // Standard query with Recursion Desired
    dns->qdcount = htons(1);
    dns->ancount = 0;
    dns->nscount = 0;
    dns->arcount = 0;

    int qname_len = dns_format_qname(hostname, query_buf + sizeof(dns_header_t));
    if (qname_len < 0) {
        log_error(MODULE, "Invalid hostname formatting for DNS query: '%s'", hostname);
        return -1;
    }

    uint32_t query_pos = sizeof(dns_header_t) + qname_len;
    // QTYPE = 0x0001 (A record)
    query_buf[query_pos++] = 0x00;
    query_buf[query_pos++] = 0x01;
    // QCLASS = 0x0001 (IN)
    query_buf[query_pos++] = 0x00;
    query_buf[query_pos++] = 0x01;

    dns_resolved_flag = false;
    dns_resolved_ip = 0;

    net_send_udp_packet(my_dns_server, dns_client_port, 53, query_buf, query_pos);

    uint32_t timeout = get_ticks() + 2500;
    while (!dns_resolved_flag && get_ticks() < timeout) {
        task_yield();
    }

    if (dns_resolved_flag) {
        *ip_out = dns_resolved_ip;
        dns_query_id++;
        return 0;
    }

    log_warning(MODULE, "DNS resolution timed out for '%s'", hostname);
    dns_query_id++;
    return -1;
}

int net_ping(uint32_t target_ip, uint32_t *latency_out) {
    uint32_t next_hop_ip = target_ip;

    if ((target_ip & my_netmask) != (my_ip & my_netmask)) {
        next_hop_ip = my_gateway;
        log_trace(MODULE, "Target IP %u.%u.%u.%u is off-subnet. Routing via gateway %u.%u.%u.%u",
                  target_ip & 0xFF, (target_ip >> 8) & 0xFF, (target_ip >> 16) & 0xFF, (target_ip >> 24) & 0xFF,
                  my_gateway & 0xFF, (my_gateway >> 8) & 0xFF, (my_gateway >> 16) & 0xFF, (my_gateway >> 24) & 0xFF);
    }

    uint8_t *mac = arp_lookup(next_hop_ip);
    if (!mac) {
        arp_received_flag = false;
        arp_send_request(next_hop_ip);
        uint32_t timeout = get_ticks() + 1000;
        while (!arp_received_flag && get_ticks() < timeout) {
            task_yield();
        }
        mac = arp_lookup(next_hop_ip);
        if (!mac) {
            log_warning(MODULE, "ARP Resolution Timeout for next hop IP");
            return -1;
        }
    }

    icmp_received_flag = false;
    uint32_t start = get_ticks();
    
    icmp_send_echo_request(target_ip, mac, 0x1234, 1);
    
    uint32_t timeout = get_ticks() + 2000;
    while (!icmp_received_flag && get_ticks() < timeout) {
        task_yield();
    }

    if (icmp_received_flag) {
        *latency_out = get_ticks() - start;
        return 0;
    }
    
    log_warning(MODULE, "ICMP Echo Reply Timeout");
    return -2;
}

void net_initialize(void) {
    memset(arp_cache, 0, sizeof(arp_cache));
    log_info(MODULE, "Initialized Layer 3 & Transport Stack (IPv4: 10.0.2.15, DNS: 10.0.2.3)");
}