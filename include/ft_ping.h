#ifndef FT_PING_H
# define FT_PING_H

# include <netinet/in.h>
# include <signal.h>
# include <stddef.h>
# include <stdint.h>

# define PING_MAX_DATA 65507
# define PING_SLOTS 65536
# define PING_SECOND INT64_C(1000000000)

typedef struct s_options
{
	const char *host;
	uint64_t count;
	size_t data_size;
	int64_t interval;
	int64_t deadline;
	int64_t linger;
	int ttl;
	int verbose;
	int quiet;
} t_options;

typedef struct s_probe
{
	int64_t sent_at;
	int active;
	int replied;
	int completed;
} t_probe;

typedef struct s_ping
{
	t_options options;
	struct sockaddr_in destination;
	char address[INET_ADDRSTRLEN];
	int socket_fd;
	uint16_t identifier;
	t_probe *probes;
	unsigned char *outgoing;
	unsigned char *incoming;
	uint64_t transmitted;
	uint64_t received;
	uint64_t completed;
	uint64_t duplicates;
	uint64_t samples;
	uint64_t errors;
	double minimum;
	double maximum;
	double mean;
	double m2;
} t_ping;

extern volatile sig_atomic_t g_stop;

void ping_usage(void);
int ping_options(int argc, char **argv, t_options *options);
int ping_open(t_ping *ping);
void ping_close(t_ping *ping);
int ping_run(t_ping *ping);
void ping_statistics(const t_ping *ping);
void ping_record(t_ping *ping, double milliseconds);
int64_t ping_now(void);
uint16_t ping_read16(const unsigned char *bytes);
void ping_write16(unsigned char *bytes, uint16_t value);
uint16_t ping_checksum(const unsigned char *bytes, size_t length);
size_t ping_packet(unsigned char *bytes, size_t data_size, uint16_t id,
	uint16_t sequence, int64_t stamp);
int ping_payload(const unsigned char *bytes, size_t data_size, int64_t stamp);
int ping_receive(t_ping *ping);
int ping_process(t_ping *ping, const unsigned char *bytes, size_t length,
	int64_t received_at);

#endif
