#include "platform.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void platform_get_network(char *out, size_t size) {
    if (!out || size == 0) return;
    struct ifaddrs *ifaddr, *ifa;
    char ip_str[INET_ADDRSTRLEN] = "";
    char ifname[64] = "";

    if (getifaddrs(&ifaddr) == 0) {
        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == NULL) continue;
            if (ifa->ifa_addr->sa_family == AF_INET) { // IPv4
                if (strcmp(ifa->ifa_name, "lo") != 0) { // Exclude loopback
                    struct sockaddr_in *pAddr = (struct sockaddr_in *)ifa->ifa_addr;
                    inet_ntop(AF_INET, &pAddr->sin_addr, ip_str, sizeof(ip_str));
                    snprintf(ifname, sizeof(ifname), "%s", ifa->ifa_name);
                    break;
                }
            }
        }
        freeifaddrs(ifaddr);
    }

    if (strlen(ip_str) > 0) {
        snprintf(out, size, "%s (%s)", ip_str, ifname);
    } else {
        snprintf(out, size, "Disconnected");
    }
}
