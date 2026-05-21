# ── Stage 1: Build ─────────────────────────────────────────────────────────────
# Alpine 3.21 ships GCC 14 (full C++23 std::format) and Boost 1.85.
FROM alpine:3.21 AS builder

RUN apk add --no-cache \
        cmake ninja g++ git \
        boost-dev openssl-dev

# Copy only what CMake needs (keeps the layer cache useful)
WORKDIR /src
COPY CMakeLists.txt .
COPY src/           src/
COPY systemd/       systemd/
COPY config.json    .

# pugixml — fetched from GitHub by FetchContent (git required above)
# nlohmann/json — fetched by URL; no extra package needed
RUN cmake -S /src -B /build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
    && cmake --build /build \
    && cmake --install /build --prefix /install

# ── Stage 2: Runtime ───────────────────────────────────────────────────────────
FROM alpine:3.21 AS runtime

# libssl3 / libcrypto3 — TLS for outbound ENTSO-E connections
# ca-certificates — root CAs for that TLS verification
# libstdc++ / libgcc — C++ runtime (not present by default in alpine:3.21)
RUN apk add --no-cache \
        libssl3 libcrypto3 \
        ca-certificates \
        libstdc++ libgcc

RUN addgroup -S epex-proxy \
    && adduser -S -G epex-proxy -H -h /var/lib/epex-proxy \
               -s /sbin/nologin epex-proxy \
    && mkdir -p /var/lib/epex-proxy /etc/epex-proxy \
    && chown epex-proxy:epex-proxy /var/lib/epex-proxy

COPY --from=builder /install/bin/epex-proxy    /usr/bin/epex-proxy
COPY --from=builder /install/share/epex-proxy/ /usr/share/epex-proxy/

USER epex-proxy

# Price data cache
VOLUME /var/lib/epex-proxy

# HTTP — Caddy (or any reverse proxy) connects here; not exposed to the host
EXPOSE 8080

ENTRYPOINT ["/usr/bin/epex-proxy", "/etc/epex-proxy/config.json"]
