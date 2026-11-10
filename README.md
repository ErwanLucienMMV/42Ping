# ft_ping

C implementation of IPv4 ICMP Echo for the supplied 42 `ft_ping` subject
(version 5.1), with GNU inetutils 2.0 output as its reference.

```sh
make
sudo ./ft_ping 127.0.0.1
sudo ./ft_ping -v --ttl=1 -c 3 -W 1 1.1.1.1
./ft_ping '-?'
```

The first command runs until Ctrl+C. Help is available without root. The raw
socket requires root or `CAP_NET_RAW`; missing permissions produce a clear error.
The program creates and parses its own ICMP packets. It never executes another
ping program and uses no source code from a standard ping implementation.

## How ping works, step by step

1. **Read the arguments.** Choose one IPv4 destination and options. By default,
   send a 56-byte payload once per second, indefinitely.
2. **Resolve the destination once.** `getaddrinfo()` with `AF_INET` converts an
   address, hostname or FQDN to an IPv4 address. Keep that address for the run.
   Replies display numeric addresses without reverse DNS queries.
3. **Open a raw ICMP socket.** `socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)` asks
   Linux to deliver ICMP messages to the process. Linux checks the process's
   permissions here. An optional `IP_TTL` socket setting controls outgoing TTL.
4. **Build an Echo Request.** The ICMP header is eight bytes: type 8, code 0,
   checksum, process identifier and sequence number. Append the payload, which
   contains a monotonic timestamp and a reproducible byte pattern. The sequence
   starts at zero and wraps after 65535. The Internet checksum covers the entire
   ICMP message, including an odd final byte padded with zero for calculation.
5. **Send it through the kernel.** `sendto()` passes the ICMP message to Linux.
   The kernel supplies the IPv4 header, selects a route and next hop, resolves
   the link-layer neighbor when needed, and transmits the packet. The application
   does not construct Ethernet frames or set `IP_HDRINCL`.
6. **Wait for a reply or the next send time.** `pselect()` sleeps until the
   socket is readable, a timer expires, or a stop signal arrives. Signals are
   unblocked atomically while waiting, so Ctrl+C also works with long intervals.
   There is no busy loop, `fcntl`, `poll`, or `ppoll`.
7. **Validate received packets.** Raw IPv4 reception includes the IP header.
   Check its variable header length, packet lengths, fragmentation fields and
   protocol, then verify the ICMP checksum. An Echo Reply has type 0 and code 0.
   Match its source address, identifier, outstanding sequence and payload before
   counting it. The timestamp rejects stale replies after sequence-number reuse
   when the payload is large enough. Duplicates are marked and counted separately.
   Linux's special local destination `0.0.0.0` can reply from `127.0.0.1`, so
   it is matched by identifier, sequence and payload instead of a fixed source.
8. **Calculate and print the result.** Subtract the recorded monotonic send time
   from the receive time to obtain RTT. Print ICMP length, sender, sequence,
   received TTL and RTT. The received TTL belongs to the reply packet, so it is
   not the TTL the request had when it arrived at its destination.
9. **Handle ICMP errors without stopping.** A router can return Time Exceeded or
   Destination Unreachable instead of an Echo Reply. Inspect the original IP and
   ICMP headers quoted inside the error to match it to this run. With `-v`, print
   the error and quoted IP-header dump, then keep sending. Errors are not replies.
10. **Stop and report statistics.** Ctrl+C/SIGTERM, a count limit or a deadline
    ends the run. Report transmitted requests, unique replies, duplicates and
    integer packet-loss percentage, followed by min/average/max/stddev RTT.
    Welford's update keeps the variance stable. The sole global variable is the
    `volatile sig_atomic_t` stop flag; signal handlers only set that flag.

Default ICMP length is 8 + 56 = 64 bytes; with a normal 20-byte IPv4 header,
the IP packet is 84 bytes. Like inetutils 2.0, payloads shorter than the native
`struct timeval` (16 bytes on the tested 64-bit builds) omit timing output.
No reply can mean packet loss, filtering or a routing problem; it does not prove
the destination is powered off.

Protocol references: [RFC 792](https://www.rfc-editor.org/rfc/rfc792.html),
[RFC 1071](https://www.rfc-editor.org/rfc/rfc1071.html),
[Linux raw sockets](https://man7.org/linux/man-pages/man7/raw.7.html), and
[GNU inetutils manual](https://www.gnu.org/software/inetutils/manual/inetutils.html#ping-invocation).
Implementation was written from the protocol and documented socket interfaces;
the prebuilt GNU binary is used only by optional comparison tests.

## Options

| Option | Behavior |
| --- | --- |
| `-?`, `-h`, `--help` | Print usage and exit successfully. |
| `-v`, `--verbose` | Include the identifier in the header and show ICMP error diagnostics. |
| `-c N`, `--count=N` | Send N requests, then await outstanding replies/errors; 1..4294967295. |
| `-i SEC`, `--interval=SEC` | Interval between requests; default 1 second. |
| `-s N`, `--size=N` | Payload size; 0..65507, default 56. |
| `--ttl=N` | Outgoing IPv4 TTL; 1..255, otherwise the kernel default. |
| `-w SEC`, `--timeout=SEC` | Total run deadline after DNS/socket setup. |
| `-W SEC`, `--linger=SEC` | Maximum final wait after the last counted send; default 10 seconds. |
| `-n`, `--numeric` | Explicit numeric reply output, which is always enabled. |
| `-q`, `--quiet` | Header and statistics only. |

Time arguments accept decimal seconds from 0.001 to 86400. There is deliberately
no flood mode. The mandatory options are `-v` and `-?`; `-h` also covers the
older evaluation sheet. Extra options are a documented subset, not a claim to
implement every GNU ping option. Exit codes: 0 if at least one Echo Reply was
received, 1 if none was received, 2 for argument/setup/local fatal errors.

## What is needed in a VM

For the supplied assessment, use a **Debian >= 7 VM with Linux kernel > 3.14**,
root access, and access to that VM from a cluster computer. A convenient console
VM allocation is one vCPU, 1 GiB RAM and 8 GiB disk; these resource figures are
practical suggestions, not subject requirements. A recent Debian installation
avoids the extra work of configuring an archived Debian 7 installation.

Install a C toolchain, make and Git. As root on a recent Debian installation:

```sh
apt-get update
apt-get install -y build-essential git
uname -r
make
./ft_ping -c 3 127.0.0.1
```

Runtime dependencies are the standard C library, math library and, for old glibc,
the real-time library (`-lm -lrt`). No third-party C library is needed. NAT or
bridged networking works if routing and ICMP are allowed; DNS is required for
names not present in `/etc/hosts`. No inbound port-forwarding is needed for Echo
Replies. SSH is optional if that is how the cluster computer reaches the VM.

For non-root execution during development, optionally install `libcap2-bin` and
run `sudo setcap cap_net_raw+ep ./ft_ping` after building. Rebuilding the executable
can remove that file capability. The assessment sheet still asks for root access.
Python 3.5+ is needed only for the Python test scripts; Valgrind is optional.

The assessment reference is specifically GNU inetutils **2.0**. A VM's default
`ping` may be iputils or another inetutils version. Check `ping -V` before making
output comparisons. Debian's prebuilt
[`inetutils-ping_2.0-1+deb11u2_amd64.deb`](https://ftp.debian.org/debian/pool/main/i/inetutils/inetutils-ping_2.0-1+deb11u2_amd64.deb)
can be extracted with `dpkg-deb -x package.deb /tmp/inetutils20`; its reference
binary is then `/tmp/inetutils20/bin/ping`. That package is only a test oracle,
not a dependency of the implementation.

## Docker development and tests

```sh
docker build -t ft-ping .
docker run --rm -it --cap-drop=ALL --cap-add=NET_RAW ft-ping 127.0.0.1
docker run --rm --cap-drop=ALL --cap-add=NET_RAW ft-ping -c 3 localhost

docker build --target test -t ft-ping-tests .
docker run --rm --cap-add=NET_RAW ft-ping-tests
docker run --rm --cap-add=NET_RAW --cap-add=NET_ADMIN ft-ping-tests \
    python3 tests/network.py ./ft_ping
docker run --rm --cap-add=NET_RAW --cap-add=NET_ADMIN ft-ping-tests \
    python3 tests/errors.py ./ft_ping
```

Default image version is Debian 13. Select another maintained Debian release
with `--build-arg DEBIAN_VERSION=12`. The image has separate build, test and
runtime stages. `NET_ADMIN` is needed only for `tests/network.py` and
`tests/errors.py`, which add
temporary loopback firewall rules and hosts aliases in its disposable container,
then removes them. Normal execution needs only `NET_RAW`; no full privileged
container or host networking is necessary.

On a Linux checkout, `make test` runs the protocol tests and executable tests.
Optional output comparison with a separately installed GNU inetutils 2.0 binary:

```sh
python3 tests/integration.py ./ft_ping --reference /path/to/inetutils-2.0/ping
python3 tests/network.py ./ft_ping --reference /path/to/inetutils-2.0/ping
python3 tests/errors.py ./ft_ping --reference /path/to/inetutils-2.0/ping
```

The network and injected-error suites must be run in a disposable Linux container
with `NET_ADMIN`.
The test oracle never becomes a dependency of `ft_ping`. See
[the verification record](docs/verification.md) for observed results and the
Debian 7 compatibility test.

## Does the subject allow a container instead of the VM?

The subject explicitly says, in General Instructions (printed page 4),
"Your project must be realized in a virtual machine running on Debian (>= 7.0)."
It also requires the VM to be usable from a cluster computer. Submission and
peer-evaluation (printed page 7) repeats that evaluation must take place in a VM
with Linux kernel > 3.14.

The supplied older evaluation sheet (PDF page 3) is equally explicit:
"L'évaluation doit se passer dans une machine virtuelle sous linux ayant un
noyau > 3.14 en disposant des droits root." Its wording does not itself specify
Debian; the subject supplies that requirement.

Neither document names Docker or expressly prohibits it by name. Nevertheless,
**a standalone container does not satisfy the stated full-VM environment on its
own**. [Containers share their host's kernel](https://docs.docker.com/get-started/docker-concepts/the-basics/what-is-a-container/);
Docker Desktop's underlying WSL2
Linux VM does not turn a Debian container into a Debian virtual machine with its
own kernel. Docker is suitable for the development tests authorized in this
task. Use the required VM for evaluation unless the evaluator explicitly accepts
a different arrangement. Docker inside the required Debian VM can be an
additional tool, but does not replace the VM requirement.

The older sheet and the current subject differ on help (`-h` versus `-?`) and the
reference version. This implementation follows the subject's inetutils 2.0
reference, accepts both help spellings, defines one global, and avoids the three
functions (`fcntl`, `poll`, `ppoll`) expressly excluded by the evaluation sheet.

## Source map

- `src/main.c`: lifecycle, signal handlers and the one global stop flag.
- `src/options.c`: argument validation and help.
- `src/network.c`: one-time IPv4 resolution, socket permissions and cleanup.
- `src/packet.c`: byte-order-safe ICMP construction, payloads and checksum.
- `src/receive.c`: bounded packet parsing, reply matching and ICMP errors.
- `src/run.c`: send scheduling, count/deadline handling and `pselect()`.
- `src/statistics.c`: duplicate-aware counters and stable RTT statistics.
- `src/util.c`: monotonic clock and 16-bit network-order helpers.
- `tests/`: protocol boundaries/fuzz inputs, executable and controlled network tests.
