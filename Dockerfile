ARG DEBIAN_VERSION=13
FROM debian:${DEBIAN_VERSION} AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc make libc6-dev && rm -rf /var/lib/apt/lists/*
WORKDIR /build
COPY Makefile ./
COPY include/ include/
COPY src/ src/
RUN make

FROM build AS test
RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 iptables && rm -rf /var/lib/apt/lists/*
COPY tests/ tests/
CMD ["make", "test"]

FROM debian:${DEBIAN_VERSION} AS runtime
COPY --from=build /build/ft_ping /usr/local/bin/ft_ping
ENTRYPOINT ["/usr/local/bin/ft_ping"]
