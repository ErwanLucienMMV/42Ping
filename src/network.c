#include "ft_ping.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int resolve(t_ping *ping)
{
	struct addrinfo hints;
	struct addrinfo *addresses;
	int error;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_RAW;
	hints.ai_protocol = IPPROTO_ICMP;
	error = getaddrinfo(ping->options.host, NULL, &hints, &addresses);
	if (error != 0)
	{
		fprintf(stderr, "ft_ping: %s: %s\n", ping->options.host,
			gai_strerror(error));
		return (-1);
	}
	memcpy(&ping->destination, addresses->ai_addr,
		sizeof(ping->destination));
	freeaddrinfo(addresses);
	if (!inet_ntop(AF_INET, &ping->destination.sin_addr, ping->address,
			sizeof(ping->address)))
		return (-1);
	return (0);
}

int ping_open(t_ping *ping)
{
	if (resolve(ping) < 0)
		return (-1);
	ping->socket_fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (ping->socket_fd < 0)
	{
		fprintf(stderr, "ft_ping: socket: %s", strerror(errno));
		if (errno == EPERM || errno == EACCES)
			fputs(" (root or CAP_NET_RAW required)", stderr);
		fputc('\n', stderr);
		return (-1);
	}
	if (ping->options.ttl && setsockopt(ping->socket_fd, IPPROTO_IP,
			IP_TTL, &ping->options.ttl, sizeof(ping->options.ttl)) < 0)
	{
		fprintf(stderr, "ft_ping: setsockopt: %s\n", strerror(errno));
		return (-1);
	}
	ping->identifier = (uint16_t)getpid();
	ping->probes = calloc(PING_SLOTS, sizeof(*ping->probes));
	ping->outgoing = malloc(8 + ping->options.data_size);
	ping->incoming = malloc(65535);
	if (!ping->probes || !ping->outgoing || !ping->incoming)
	{
		fputs("ft_ping: cannot allocate packet buffers\n", stderr);
		return (-1);
	}
	return (0);
}

void ping_close(t_ping *ping)
{
	if (ping->socket_fd >= 0)
		close(ping->socket_fd);
	free(ping->probes);
	free(ping->outgoing);
	free(ping->incoming);
}
