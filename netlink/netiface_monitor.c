#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <memory.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/rtnetlink.h>

// помещает блоки данных в массив атрибутов rtattr
void parse_rtattr(struct rtattr *tb[], int max, struct rtattr *rta, int len)
{
	memset(tb, 0, sizeof(struct rtattr *) * (max + 1));

	while (RTA_OK(rta, len)) {
		if (rta->rta_type <= max) {
			tb[rta->rta_type] = rta;
		}
		rta = RTA_NEXT(rta,len);
	}
}

int main(void)
{
	int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);

	if (fd < 0) {
		printf("socket error: %s\n", strerror(errno));
		return -1;
	}

	char buf[8192];
	// структура сообщения
    struct iovec iov = {
	    .iov_base = buf,
	    .iov_len = sizeof(buf)
    };

    // локальный адрес
    struct sockaddr_nl local = {
	    .nl_family = AF_NETLINK,
	    .nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV4_ROUTE,
	    .nl_pid = getpid()
    };

	// структура сообщения netlink
	struct msghdr msg = {
		.msg_name = &local,
		.msg_namelen = sizeof(local),
		.msg_iov = &iov,
		.msg_iovlen = 1
	};

	if (bind(fd, (struct sockaddr*)&local, sizeof(local)) < 0) {
		printf("bind error: %s\n", strerror(errno));
		close(fd);
		return -1;
	}

	while (1) {
		ssize_t status = recvmsg(fd, &msg, MSG_DONTWAIT);

		if (status < 0) {
			if (errno == EINTR || errno == EAGAIN) {
				usleep(250000);
				continue;
			}

			printf("recvmsg error: %s\n", strerror(errno));
			continue;
		}

		if (msg.msg_namelen != sizeof(local)) {	//провeряем длину адреса
			printf("incorrect message len\n");
			continue;
		}

		// указатель на заголовок сообщения
		struct nlmsghdr *h;

		for (h = (struct nlmsghdr*)buf; status >= (ssize_t)sizeof(*h); ) {
			int len = h->nlmsg_len;     // длина текущего сообщения
			int l = len - sizeof(*h);	// длина всего блока
			char *ifname;				// имя соединения

			if ((l < 0) || (len > status)) {
				printf("incorrect message len: %i", len);
				continue;
			}

			// смотрим тип сообщения
			if ((h->nlmsg_type == RTM_NEWROUTE) || (h->nlmsg_type == RTM_DELROUTE)) {
				printf("routes have changed\n");
			} else {
				char *iface;
				char *conn;
				struct ifinfomsg *ifi;           // структура представления сетевого устройства
				struct rtattr *tb[IFLA_MAX + 1]; // массив атрибутов соединения

				ifi	= (struct ifinfomsg*)NLMSG_DATA(h); // получаем информацию о сетевом соединении

				parse_rtattr(tb, IFLA_MAX, IFLA_RTA(ifi), h->nlmsg_len); // получаем атрибуты сетевого соединения

				if (tb[IFLA_IFNAME]) // проверяем валидность атрибута, хранящего имя соединения
					ifname = (char*)RTA_DATA(tb[IFLA_IFNAME]);

				if (ifi->ifi_flags & IFF_UP)
					iface = "UP";
				else
					iface = "DOWN";

				if (ifi->ifi_flags & IFF_RUNNING)
					conn = "RUNNING";
				else
					conn = "NOT RUNNING";

				char ifaddr[256];
				struct ifaddrmsg *ifa;         // структура представления сетевого адреса
				struct rtattr *tba[IFA_MAX+1]; // массив атрибутов адреса

				ifa = (struct ifaddrmsg*)NLMSG_DATA(h);	// получаем данные из соединения

				parse_rtattr(tba, IFA_MAX, IFA_RTA(ifa), h->nlmsg_len);	// получаем атрибуты сетевого соединения

				if (tba[IFA_LOCAL])  // проверяем валидность указателя локального адреса
					inet_ntop(AF_INET, RTA_DATA(tba[IFA_LOCAL]), ifaddr, sizeof(ifaddr));	// получаем IP адрес

				switch (h->nlmsg_type) {
					case RTM_DELADDR:
						printf("Interface %s address was removed\n", ifname);
						break;

					case RTM_DELLINK:
						printf("Interface %s was removed\n", ifname);
						break;

					case RTM_NEWLINK:
						printf("Interface %s, interface state %s %s\n", ifname, iface, conn);
						break;

					case RTM_NEWADDR:
						printf("Address %s was added to interface %s\n", ifaddr, ifname);
						break;
				}
			}

			status -= NLMSG_ALIGN(len);	// выравниваемся по длине сообщения

			h = (struct nlmsghdr*)((char*)h + NLMSG_ALIGN(len)); // получаем следующее сообщение
		}

		usleep(250000);
	}

	close(fd);

	return 0;
}
