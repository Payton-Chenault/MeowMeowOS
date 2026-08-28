#include "../libs/meow_libc.h"

#define MODULE "PING"
DESCRIPTION("ping.elf: Send ICMP ECHO_REQUEST to network hosts");

extern void log_trace(const char *module, const char *fmt, ...);
extern void log_info(const char *module, const char *fmt, ...);
extern void log_error(const char *module, const char *fmt, ...);

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: ping <ip_address>\n");
        log_error(MODULE, "ping: missing IP argument");
        return 1;
    }
    
    uint32_t target_ip = inet_addr(argv[1]);
    if (target_ip == 0) {
        printf("ping: invalid IP address format\n");
        log_error(MODULE, "ping: invalid IP string '%s'", argv[1]);
        return 1;
    }
    
    log_info(MODULE, "Initiating ping to %s", argv[1]);
    printf("PING %s 56 data bytes\n", argv[1]);
    
    int transmitted = 0;
    int received = 0;
    
    for (int i = 0; i < 4; i++) {
        uint32_t latency;
        int ret = sys_ping(target_ip, &latency);
        
        if (ret == 0) {
            printf("64 bytes from %s: icmp_seq=%d time=%d ms\n", argv[1], i + 1, latency);
            log_trace(MODULE, "Received reply for sequence %d in %u ms", i + 1, latency);
            received++;
        } else if (ret == -1) {
            printf("From %s icmp_seq=%d Destination Host Unreachable\n", argv[1], i + 1);
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
    
    printf("\n--- %s ping statistics ---\n", argv[1]);
    printf("%d packets transmitted, %d received, %d%% packet loss\n",
        transmitted, received, ((transmitted - received) * 100) / transmitted);
    
    log_info(MODULE, "Ping session finished. Transmitted: %d, Received: %d", transmitted, received);
    return (received > 0) ? 0 : 1;
}