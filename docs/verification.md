# Verification record

Tests performed on 10 November 2026 using Docker Desktop 4.41.0,
Docker Engine 28.1.1 and the shared Linux kernel
`5.15.153.1-microsoft-standard-WSL2`.

## Observed results

| Environment | Compiler / C library | Result |
| --- | --- | --- |
| Debian 12 (bookworm, existing `node:22` image) | GCC 12.2 / glibc 2.36 | Strict build; protocol tests; 64 integration/reference checks; 10 controlled network checks and 26 ICMP error cases passed. |
| Debian 13 (trixie) | GCC 14.2 / glibc 2.41 | Strict build; protocol tests; 58 integration checks, 9 network checks and 26 error cases passed in the delivered test image. Runtime image passed with only NET_RAW. |
| Debian 7 (wheezy) userland | GCC 4.7.2 / glibc 2.13 | Native compiler build passed; protocol tests, 64 integration/reference checks, 10 network checks and 26 error cases passed under QEMU user-mode emulation. |

Debian 7 caveat: its original apt HTTP method and the native old-libc binary
crashed in this WSL2 environment at the fixed legacy address
`0xffffffffff600400`. `strace` and GDB confirmed that failure. The archived
compiler packages were downloaded through Debian 12 and extracted into an
isolated Debian 7 root filesystem; nothing was installed into the Windows host.
The code then built with GCC 4.7.2 and its original libc headers/libraries.
Execution used QEMU 7.2 user-mode emulation with Debian 7's own dynamic loader
and library paths. This verifies the old toolchain/userland; it is **not** a
claim to have booted a Debian 7 VM or tested its original kernel. All networking
tests ultimately used the shared WSL2 Linux kernel listed above.

The reference executable was Debian's prebuilt `inetutils-ping` package
`2:2.0-1+deb11u2` (amd64). `ping -V` confirmed GNU inetutils 2.0.
No standard ping source code was used or inspected. Reference executions were
only test comparisons, as required by the assessment sheet. Timing values and
the per-process identifier were normalized for exact line comparisons; the
final RTT summary was excluded, as the subject permits.

## Coverage

- Valid IPv4 addresses (including Linux's special local `0.0.0.0` destination),
  hostnames and FQDNs, including a trailing-dot FQDN.
- Required `-v` / `-?`, older-sheet `-h`, option clusters and bad arguments.
- Payload sizes 0, 1, 7, 8, 15, 16, 17, 57, 1501 and 65507 bytes.
- Correct sequence numbering, one-second default scheduling, configurable
  intervals, count, deadline and final-reply waits.
- Ctrl+C and SIGTERM during a 3600-second interval; orderly statistics and exit.
- Root/raw-socket permissions and failure as UID/GID 65534 without capabilities.
- Zero loss, 100% loss and controlled partial loss; GNU-compatible truncation
  of integer loss percentages.
- Matched ICMP Destination Host Unreachable messages that do not stop probing;
  verbose header dumps and default suppression of error-packet output.
- Twenty-six injected ICMP error type/code combinations with the minimum
  quoted IPv4 header and eight ICMP bytes; exact output matched GNU 2.0.
- Finite-count completion on either replies or ICMP errors, counting each
  request once even when repeated errors or a later reply arrive.
- Real TTL-expired responses from the Docker gateway using `--ttl=1`.
- Known checksum vectors, odd-length packets, every truncation of valid packets,
  invalid checksums, unrelated identifiers/sources/sequences, stale payloads,
  duplicate replies, IPv4 options and truncated quoted ICMP messages.
- Fixed-seed parsing of 20000 arbitrary malformed packets.

Valgrind 3.19 reported **zero errors and no remaining heap allocations** for
real loopback/hostname packet exchanges and the protocol/malformed-packet suite.

The executable has exactly one program-defined global variable, `g_stop`.
It calls none of `fcntl`, `poll`, `ppoll`, `system`, `popen` or the `exec` family.
DNS and standard library internals may use their own system calls; the check
concerns functions directly used by the submitted implementation.

## Reproduction

```sh
make test
python3 tests/integration.py ./ft_ping --reference /path/to/GNU-2.0/ping
# In a disposable container with NET_ADMIN:
python3 tests/network.py ./ft_ping --reference /path/to/GNU-2.0/ping
python3 tests/errors.py ./ft_ping --reference /path/to/GNU-2.0/ping
valgrind --leak-check=full --error-exitcode=99 ./ft_ping -c3 -i.01 localhost
valgrind --leak-check=full --error-exitcode=99 ./obj/packet_tests
```

The program continues running by default until a stop signal; tests use finite
counts and deadlines. Tests do not require public hosts to answer ICMP.
