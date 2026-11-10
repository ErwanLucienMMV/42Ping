/* Protocol boundary and hostile-input tests; no network access required. */
#include "ft_ping.h"
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ip_header(unsigned char *packet, size_t length, size_t header)
{
	memset(packet, 0, header);
	packet[0] = (unsigned char)(0x40 | header / 4);
	packet[8] = 64;
	packet[9] = 1;
	ping_write16(packet + 2, (uint16_t)length);
	packet[12] = 127;
	packet[15] = 1;
	packet[16] = 127;
	packet[19] = 1;
}

static size_t echo(unsigned char *packet, size_t header, uint16_t id,
	uint16_t sequence, int64_t stamp)
{
	ip_header(packet, header + 64, header);
	ping_packet(packet + header, 56, id, sequence, stamp);
	packet[header] = 0;
	packet[header + 2] = 0;
	packet[header + 3] = 0;
	ping_write16(packet + header + 2, ping_checksum(packet + header, 64));
	return (header + 64);
}

static void parser_checks(t_ping *ping)
{
	unsigned char packet[160];
	size_t length;
	size_t index;
	uint64_t received;
	uint64_t errors;

	ping->identifier = 0x1234;
	ping->options.data_size = 56;
	ping->options.quiet = 1;
	assert(inet_pton(AF_INET, "127.0.0.1", &ping->destination.sin_addr) == 1);
	ping->probes[9].active = 1;
	ping->probes[9].sent_at = 5000000;
	length = echo(packet, 20, ping->identifier, 9, 5000000);
	/* Every truncation of a valid packet must be ignored. */
	index = 0;
	while (index < length)
		ping_process(ping, packet, index++, 7000000);
	assert(ping->received == 0);
	ping_process(ping, packet, length, 7000000);
	assert(ping->received == 1 && ping->samples == 1 && ping->mean == 2.0);
	assert(ping->completed == 1);
	ping_process(ping, packet, length, 8000000);
	assert(ping->received == 1 && ping->duplicates == 1);
	assert(ping->completed == 1);
	assert(ping->samples == 2 && ping->mean == 2.5);
	assert(ping->minimum == 2.0 && ping->maximum == 3.0);
	received = ping->received;
	packet[30] ^= 1;
	ping_process(ping, packet, length, 9000000);
	assert(ping->received == received && ping->duplicates == 1);
	length = echo(packet, 20, 0xabcd, 9, 5000000);
	ping_process(ping, packet, length, 9000000);
	assert(ping->received == received && ping->duplicates == 1);
	length = echo(packet, 20, ping->identifier, 10, 5000000);
	ping_process(ping, packet, length, 9000000);
	assert(ping->received == received);
	length = echo(packet, 20, ping->identifier, 9, 4999999);
	ping_process(ping, packet, length, 9000000);
	assert(ping->duplicates == 1);
	length = echo(packet, 20, ping->identifier, 9, 5000000);
	packet[12] = 126;
	ping_process(ping, packet, length, 9000000);
	assert(ping->duplicates == 1);
	length = echo(packet, 24, ping->identifier, 9, 5000000);
	ping_process(ping, packet, length, 9000000);
	assert(ping->duplicates == 2);
	/* A router error includes the original IPv4 header and ICMP header. */
	ip_header(packet, 56, 20);
	memset(packet + 20, 0, 36);
	packet[20] = 11;
	ip_header(packet + 28, 84, 20);
	ping_packet(packet + 48, 0, ping->identifier, 9, 0);
	ping_write16(packet + 22, ping_checksum(packet + 20, 36));
	ping_process(ping, packet, 56, 9000000);
	assert(ping->errors == 1);
	assert(ping->completed == 1);
	errors = ping->errors;
	for (index = 0; index < 56; ++index)
		ping_process(ping, packet, index, 9000000);
	assert(ping->errors == errors);
	/* An error completes a request once; a later Echo Reply is still its
	 * first reply and must not complete the same request a second time. */
	ping->probes[10].active = 1;
	ping->probes[10].sent_at = 5000000;
	ping_write16(packet + 54, 10);
	ping_write16(packet + 22, 0);
	ping_write16(packet + 22, ping_checksum(packet + 20, 36));
	ping_process(ping, packet, 56, 9000000);
	ping_process(ping, packet, 56, 9000000);
	assert(ping->completed == 2 && ping->errors == 3);
	length = echo(packet, 20, ping->identifier, 10, 5000000);
	ping_process(ping, packet, length, 9000000);
	assert(ping->completed == 2 && ping->received == 2);
	assert(ping->duplicates == 2);
	/* Fixed-seed fuzzing exercises all sizes and arbitrary IPv4/ICMP fields. */
	srand(42);
	for (index = 0; index < 20000; ++index)
	{
		size_t size;
		size_t byte;

		size = (size_t)rand() % sizeof(packet);
		for (byte = 0; byte < size; ++byte)
			packet[byte] = (unsigned char)rand();
		ping_process(ping, packet, size, 10000000);
	}
}

int main(void)
{
	/* RFC 1071 section 3 example, including its odd-length-prefix case. */
	const unsigned char example[] = {0x00, 0x01, 0xf2, 0x03, 0xf4, 0xf5, 0xf6, 0xf7};
	unsigned char packet[65];
	t_ping ping;
	size_t length;

	assert(ping_checksum(example, sizeof(example)) == 0x220d);
	assert(ping_checksum(example, 3) == 0x0dfe);
	assert(ping_checksum(example, 0) == 0xffff);
	length = ping_packet(packet, 57, 0x1234, 65535, 123456789);
	assert(length == 65 && packet[0] == 8 && packet[1] == 0);
	assert(ping_read16(packet + 4) == 0x1234);
	assert(ping_read16(packet + 6) == 65535);
	assert(ping_checksum(packet, length) == 0);
	packet[64] ^= 1;
	assert(ping_checksum(packet, length) != 0);
	memset(&ping, 0, sizeof(ping));
	ping.probes = calloc(PING_SLOTS, sizeof(*ping.probes));
	assert(ping.probes);
	parser_checks(&ping);
	free(ping.probes);
	puts("PASS: checksum vectors, packet validation, duplicate accounting,"
		" ICMP errors and 20000 malformed packets");
	return (0);
}
