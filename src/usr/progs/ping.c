#include "../libs/meow_libc.h"

#define MODULE "PING"
DESCRIPTION("ping.elf: Send ICMP ECHO_REQUEST to network hosts or domain names");

extern void log_trace(const char *module, const char *fmt, ...);
extern void log_info(const char *module, const char *fmt, ...);
extern void log_warning(const char *module, const char *fmt, ...);
extern void log_error(const char *module, const char *fmt, ...);

static bool is_ipv4_address(const char *str) {
    int dots = 0;
    int digits = 0;
    while (*str) {
        if (*str == '.') {
            if (digits == 0) return false;
            dots++;
            digits = 0;
        } else if (*str >= '0' && *str <= '9') {
            digits++;
        } else {
            return false;
        }
        str++;
    }
    return (dots == 3 && digits > 0);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: ping <ip_or_domain>\n");
        log_error(MODULE, "ping: missing IP or hostname argument");
        return 1;
    }
    
    uint32_t target_ip = 0;
    const char *target_name = argv[1];

    if (is_ipv4_address(target_name)) {
        target_ip = inet_addr(target_name);
        if (target_ip == 0) {
            printf("ping: invalid IP address format\n");
            log_error(MODULE, "ping: invalid IP string '%s'", target_name);
            return 1;
        }
        printf("PING %s 56 data bytes\n", target_name);
    } else {
        printf("Resolving %s...\n", target_name);
        log_info(MODULE, "Attempting DNS resolution for domain '%s'", target_name);
        
        if (sys_dns_resolve(target_name, &target_ip) != 0 || target_ip == 0) {
            printf("ping: cannot resolve %s: Unknown host\n", target_name);
            log_error(MODULE, "ping: DNS lookup failed for '%s'", target_name);
            return 1;
        }

        char ip_buf[32];
        snprintf(ip_buf, sizeof(ip_buf), "%u.%u.%u.%u",
                 target_ip & 0xFF, (target_ip >> 8) & 0xFF,
                 (target_ip >> 16) & 0xFF, (target_ip >> 24) & 0xFF);
        
        printf("PING %s (%s) 56 data bytes\n", target_name, ip_buf);
        log_info(MODULE, "Domain '%s' resolved to %s", target_name, ip_buf);
    }
    
    int transmitted = 0;
    int received = 0;
    
    for (int i = 0; i < 4; i++) {
        uint32_t latency;
        int ret = sys_ping(target_ip, &latency);
        
        if (ret == 0) {
            printf("64 bytes from %s: icmp_seq=%d time=%d ms\n", target_name, i + 1, latency);
            log_trace(MODULE, "Received reply for sequence %d in %u ms", i + 1, latency);
            received++;
        } else if (ret == -1) {
            printf("From %s icmp_seq=%d Destination Host Unreachable\n", target_name, i + 1);
            log_warning(MODULE, "Host unreachable for sequence %d", i + 1);
        } else {
            printf("Request timeout for icmp_seq %d\n", i + 1);
            log_warning(MODULE, "Request timeout for sequence %d", i + 1);
        }
        transmitted++;
        
        if (i < 3) {
            uint32_t start_ticks = sys_uptime();
            while (sys_uptime() < start_ticks + 1000) { sys_yield(); }
        }
    }
    
    printf("\n--- %s ping statistics ---\n", target_name);
    printf("%d packets transmitted, %d received, %d%% packet loss\n",
        transmitted, received, ((transmitted - received) * 100) / transmitted);
    
    log_info(MODULE, "Ping session finished for %s. Transmitted: %d, Received: %d", target_name, transmitted, received);
    return (received > 0) ? 0 : 1;
}