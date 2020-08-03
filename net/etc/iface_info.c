#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>

int main(int argc, char **argv)
{
    struct ifreq ifr;
    struct sockaddr_in *ipaddr;
    char address[INET_ADDRSTRLEN];

    if (argc != 2) {
        printf("usage: program interface");
        return -1;
    }

    strncpy(ifr.ifr_name, argv[1], strlen(argv[1]));

    int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

    if (sock < 0) {
        printf("socket() error: %s", strerror(errno));
        return -1;
    }

    if (ioctl(sock, SIOCGIFHWADDR, &ifr) != -1)
        printf("Mac address: %02x:%02x:%02x:%02x:%02x:%02x\n",
                (unsigned char)ifr.ifr_hwaddr.sa_data[0],
                (unsigned char)ifr.ifr_hwaddr.sa_data[1],
                (unsigned char)ifr.ifr_hwaddr.sa_data[2],
                (unsigned char)ifr.ifr_hwaddr.sa_data[3],
                (unsigned char)ifr.ifr_hwaddr.sa_data[4],
                (unsigned char)ifr.ifr_hwaddr.sa_data[5]);

    if (ioctl(sock, SIOCGIFMTU, &ifr) != -1)
        printf("MTU: %d\n", ifr.ifr_mtu);

    if (ioctl(sock, SIOCGIFADDR, &ifr) != -1) {
        ipaddr = (struct sockaddr_in *)&ifr.ifr_addr;
        if (inet_ntop(AF_INET, &ipaddr->sin_addr, address, sizeof(address)) != NULL) {
            printf("Ip address: %s\n", address);
        }
    }

    if (ioctl(sock, SIOCGIFBRDADDR, &ifr) != -1) {
        ipaddr = (struct sockaddr_in *)&ifr.ifr_broadaddr;
        if (inet_ntop(AF_INET, &ipaddr->sin_addr, address, sizeof(address)) != NULL) {
            printf("Broadcast: %s\n", address);
        }
    }

    if (ioctl(sock, SIOCGIFNETMASK, &ifr) != -1) {
        ipaddr = (struct sockaddr_in *)&ifr.ifr_netmask;
        if (inet_ntop(AF_INET, &ipaddr->sin_addr, address, sizeof(address)) != NULL) {
            printf("Netmask: %s\n", address);
        }
    }

    close(sock);
    return 0;
}
