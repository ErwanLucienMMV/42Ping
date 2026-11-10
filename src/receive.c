#include "ft_ping.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>

static size_t ipv4_header(const unsigned char *bytes, size_t length)
{
	size_t header;

	if (length < 20 || bytes[0] >> 4 != 4)
		return (0);
	header = (bytes[0] & 15) * 4;
	if (header < 20 || header > length || bytes[9] != IPPROTO_ICMP
		|| (ping_read16(bytes + 6) & 0x3fff) != 0)
		return (0);
	return (header);
}

static void reply(t_ping *ping, const unsigned char *ip,
	const unsigned char *icmp, size_t length, int64_t received_at)
{
	uint16_t sequence;
	t_probe *probe;
	double milliseconds;
	int duplicate;
	char source[INET_ADDRSTRLEN];

	if (icmp[1] != 0 || ping_read16(icmp + 4) != ping->identifier
		|| (ping->destination.sin_addr.s_addr != htonl(INADDR_ANY)
			&& memcmp(ip + 12, &ping->destination.sin_addr, 4) != 0))
		return ;
	sequence = ping_read16(icmp + 6);
	probe = &ping->probes[sequence];
	if (!probe->active || length != ping->options.data_size + 8)
		return ;
	if (!ping_payload(icmp + 8, length - 8, probe->sent_at))
	{
		if (ping->options.verbose)
			fprintf(stderr, "ft_ping: damaged or stale reply: icmp_seq=%u\n",
				sequence);
		return ;
	}
	duplicate = probe->replied;
	probe->replied = 1;
	if (!probe->completed)
		++ping->completed;
	probe->completed = 1;
	if (duplicate)
		++ping->duplicates;
	else
		++ping->received;
	milliseconds = (double)(received_at - probe->sent_at) / 1000000.0;
	if (ping->options.data_size >= sizeof(struct timeval))
		ping_record(ping, milliseconds);
	if (!ping->options.quiet)
	{
		if (!inet_ntop(AF_INET, ip + 12, source, sizeof(source)))
			return ;
		printf("%zu bytes from %s: icmp_seq=%u ttl=%u",
			length, source, sequence, ip[8]);
		if (ping->options.data_size >= sizeof(struct timeval))
			printf(" time=%.3f ms", milliseconds);
		printf("%s\n", duplicate ? " (DUP!)" : "");
	}
}

static void dump_request(const unsigned char *ip, size_t header,
	const unsigned char *icmp, size_t length)
{
	size_t index;
	char source[INET_ADDRSTRLEN];
	char destination[INET_ADDRSTRLEN];
	uint16_t fragment;

	if (!inet_ntop(AF_INET, ip + 12, source, sizeof(source))
		|| !inet_ntop(AF_INET, ip + 16, destination, sizeof(destination)))
		return ;
	puts("IP Hdr Dump:");
	index = 0;
	while (index < header)
	{
		printf(" %02x%02x", ip[index], ip[index + 1]);
		index += 2;
	}
	puts(" ");
	puts("Vr HL TOS  Len   ID Flg  off TTL Pro  cks      Src\tDst\tData");
	fragment = ping_read16(ip + 6);
	printf(" %u %2u  %02x %04x %04x %3x %04x %3.2x %3.2x %04x %s  %s \n",
		ip[0] >> 4, ip[0] & 15, ip[1], ping_read16(ip + 2),
		ping_read16(ip + 4), fragment >> 13, fragment & 0x1fff,
		ip[8], ip[9], ping_read16(ip + 10), source, destination);
	printf("ICMP: type %u, code %u, size %zu, id 0x%04x, seq 0x%04x\n",
		icmp[0], icmp[1], length, ping_read16(icmp + 4),
		ping_read16(icmp + 6));
}

static const char *unreachable_message(unsigned int code)
{
	const char *messages[] = {
		"Destination Net Unreachable", "Destination Host Unreachable",
		"Destination Protocol Unreachable", "Destination Port Unreachable",
		"Fragmentation needed and DF set", "Source Route Failed",
		"Network Unknown", "Host Unknown", "Host Isolated", NULL, NULL,
		"Destination Network Unreachable At This TOS",
		"Destination Host Unreachable At This TOS", "Packet Filtered",
		"Precedence Violation", "Precedence Cutoff"
	};

	if (code < sizeof(messages) / sizeof(*messages))
		return (messages[code]);
	return (NULL);
}

static void describe_error(const unsigned char *icmp)
{
	const char *redirects[] = {"Redirect Network", "Redirect Host",
		"Redirect Type of Service and Network", "Redirect Type of Service and Host"};
	const char *message;
	unsigned int code;
	char address[INET_ADDRSTRLEN];

	code = icmp[1];
	if (icmp[0] == 3)
	{
		message = unreachable_message(code);
		if (message)
			puts(message);
		else
			printf("Dest Unreachable, Unknown Code: %u\n", code);
	}
	else if (icmp[0] == 11 && code <= 1)
		puts(code == 0 ? "Time to live exceeded" : "Frag reassembly time exceeded");
	else if (icmp[0] == 11)
		printf("Time exceeded, Unknown Code: %u\n", code);
	else if (icmp[0] == 12)
	{
		if (inet_ntop(AF_INET, icmp + 4, address, sizeof(address)))
			printf("Parameter problem: IP address = %s\n", address);
	}
	else if (icmp[0] == 5 && code < 4)
		puts(redirects[code]);
	else if (icmp[0] == 5)
		printf("Redirect, Unknown Code: %u\n", code);
	else
		puts("Source Quench");
}

static void icmp_error(t_ping *ping, const unsigned char *ip,
	const unsigned char *icmp, size_t length)
{
	size_t header;
	const unsigned char *quoted;
	uint16_t sequence;
	char router[INET_ADDRSTRLEN];

	if (length < 8 + 20 + 8)
		return ;
	quoted = icmp + 8;
	header = ipv4_header(quoted, length - 8);
	if (!header || length < 8 + header + 8
		|| memcmp(quoted + 16, &ping->destination.sin_addr, 4) != 0
		|| quoted[header] != 8 || quoted[header + 1] != 0
		|| ping_read16(quoted + header + 4) != ping->identifier)
		return ;
	sequence = ping_read16(quoted + header + 6);
	if (!ping->probes[sequence].active)
		return ;
	++ping->errors;
	if (!ping->probes[sequence].completed)
		++ping->completed;
	ping->probes[sequence].completed = 1;
	if (!ping->options.verbose || ping->options.quiet)
		return ;
	if (!inet_ntop(AF_INET, ip + 12, router, sizeof(router)))
		return ;
	printf("%zu bytes from %s: ", length, router);
	describe_error(icmp);
	dump_request(quoted, header, quoted + header,
		ping_read16(quoted + 2) >= header
		? ping_read16(quoted + 2) - header : 8);
}

int ping_process(t_ping *ping, const unsigned char *bytes, size_t length,
	int64_t received_at)
{
	size_t header;
	size_t total;
	const unsigned char *icmp;

	header = ipv4_header(bytes, length);
	if (!header)
		return (0);
	total = ping_read16(bytes + 2);
	if (total < header + 8 || total > length)
		return (0);
	icmp = bytes + header;
	if (ping_checksum(icmp, total - header) != 0)
	{
		if (ping->options.verbose)
			fputs("ft_ping: discarded ICMP packet with invalid checksum\n", stderr);
		return (0);
	}
	if (icmp[0] == 0)
		reply(ping, bytes, icmp, total - header, received_at);
	else if (icmp[0] == 3 || icmp[0] == 4 || icmp[0] == 5
		|| icmp[0] == 11 || icmp[0] == 12)
		icmp_error(ping, bytes, icmp, total - header);
	fflush(stdout);
	return (0);
}

int ping_receive(t_ping *ping)
{
	ssize_t length;
	int64_t received_at;

	length = recvfrom(ping->socket_fd, ping->incoming, 65535, MSG_DONTWAIT,
		NULL, NULL);
	if (length < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
			return (0);
		fprintf(stderr, "ft_ping: recvfrom: %s\n", strerror(errno));
		return (-1);
	}
	received_at = ping_now();
	if (received_at < 0)
		return (-1);
	return (ping_process(ping, ping->incoming, (size_t)length, received_at));
}
