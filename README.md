# epex-proxy

HTTP daemon that serves Dutch EPEX day-ahead electricity prices from ENTSO-E,
with all-in consumer prices pre-calculated per provider (Tibber, Energiek,
Frank Energie, ANWB, Vandebron).

Prices are fetched once per day after 13:00 CET/CEST when ENTSO-E publishes
them, cached to disk, and served from memory. The `/health` endpoint reports
fetch state for each configured area.

---

## Table of contents

1. [Requirements](#requirements)
2. [Docker (recommended)](#docker-recommended)
3. [Building from source](#building-from-source)
4. [Debian package](#debian-package)
5. [Configuration reference](#configuration-reference)
6. [API](#api)

---

## Requirements

| Deployment | What you need |
|---|---|
| Docker | Docker Engine 24+ and Compose V2 (`docker compose`) |
| From source | CMake 3.25+, Ninja, GCC 14+ or Clang 17+, Boost 1.81+, OpenSSL |
| Debian package | The above, plus `dpkg-dev` for packaging |

An [ENTSO-E Transparency Platform](https://transparency.entsoe.eu/) API token
is required for all deployment methods. The process is not well-advertised:

1. Register an account at the link above.
2. **Email <transparency@entsoe.eu>** and request API access. Include your
   registered email address. Access is not granted automatically — without this
   step the token option never appears in your account.
3. Once they reply and grant access, go to
   *My Account Settings → Web API Security Token* to generate your token.

---

## Docker (recommended)

### 1. Clone and configure

```sh
git clone https://github.com/your-org/open-epex.eu.git
cd open-epex.eu
```

Create a `.env` file in the project root (it is git-ignored):

```sh
cat > .env <<'EOF'
ENTSOE_TOKEN=your_token_here
EPEX_DOMAIN=epex.example.com
EOF
```

`EPEX_DOMAIN` is the public hostname Caddy will obtain a TLS certificate for.
It must be a real DNS name pointing at the server when you first start the
stack, because Caddy uses Let's Encrypt HTTP-01 challenge.

### 2. Review the config

`docker/config.json` is mounted read-only into the container as
`/etc/epex-proxy/config.json`. The defaults work out of the box for the
Netherlands (`nl` area). Edit provider fees if your contract differs.

Key fields that differ from a local dev setup:

| Field | Docker value | Reason |
|---|---|---|
| `listen[].address` | `0.0.0.0` | Reachable from the Caddy container |
| `data_dir` | `/var/lib/epex-proxy` | Backed by a named volume |

### 3. Build and start

```sh
docker compose up -d --build
```

The builder stage compiles the binary inside an Alpine 3.21 container.
Subsequent starts reuse the image unless you pass `--build` again.

To follow logs:

```sh
docker compose logs -f
```

### 4. Update

```sh
git pull
docker compose up -d --build
```

The price cache in the `epex-data` volume is preserved across rebuilds.

### 5. Price cache volume

The `epex-data` volume holds one JSON file per day per area
(e.g. `nl/2026-05-21.json`). On startup the daemon loads all of them into
memory; new files are written after each successful ENTSO-E fetch.

**Losing the volume is harmless** — it is purely a cache of data that can be
re-fetched from ENTSO-E. The daemon starts empty and repopulates automatically:
today's prices appear after the next scheduled fetch (within minutes of
startup if it is already past 13:00 CET), tomorrow's after ENTSO-E publishes
them (~13:00 CET the same day). There is no manual recovery step required.

The only practical consequence of losing the volume is a brief gap in the
`/health` response until the first fetch completes.

### 6. Stop and clean up

```sh
docker compose down          # stop, keep volumes
docker compose down -v       # stop and delete the price cache too
```

### Architecture

```
Internet
   │  HTTPS :443
   ▼
┌──────┐   HTTP :8080   ┌────────────┐   HTTPS   ┌──────────────────┐
│ Caddy│ ─────────────► │ epex-proxy │ ─────────► │ ENTSO-E API      │
└──────┘                └────────────┘            └──────────────────┘
                               │
                     named volume (epex-data)
                      /var/lib/epex-proxy
                   one JSON file per day per area
                    (re-fetchable from ENTSO-E)
```

Caddy handles TLS termination and automatic certificate renewal. The proxy
itself speaks plain HTTP and is not exposed to the host network.

---

## Building from source

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The binary is at `build/epex-proxy`. Run it with:

```sh
export ENTSOE_TOKEN=your_token_here
./build/epex-proxy config.json
```

The default `config.json` listens on `127.0.0.1:8080` and stores cache files
under `./data/`.

### System install

```sh
cmake -S . -B build -G Ninja -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
sudo cmake --install build
```

Installs:

| Path | What |
|---|---|
| `$prefix/bin/epex-proxy` | Binary |
| `$prefix/lib/systemd/system/epex-proxy.service` | Systemd unit |
| `$prefix/share/epex-proxy/config.json.example` | Example config |

---

## Debian package

Requires `dpkg-dev` on the build host (`apt install dpkg-dev`) for
`dpkg-shlibdeps`, which auto-detects runtime library dependencies.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
cmake --build build --target package
```

`-DCMAKE_INSTALL_PREFIX=/usr` is required because the systemd unit is
generated from a template via `configure_file`, which bakes the binary path
into the unit at configure time. Without it the `ExecStart=` line would point
at the wrong location.

This produces `build/epex-proxy-1.0.1-Linux.deb`. Install it:

```sh
sudo dpkg -i build/epex-proxy-1.0.1-Linux.deb
```

The `postinst` script:

- Creates the `epex-proxy` system user and group.
- Creates `/var/lib/epex-proxy/` (data directory).
- Copies `/usr/share/epex-proxy/config.json.example` to
  `/etc/epex-proxy/config.json` **if no config exists yet** (safe on upgrades).
- Creates `/etc/epex-proxy/environment` with an empty `ENTSOE_TOKEN=` line.
- Enables and starts the systemd service.

After install, set your token and restart:

```sh
sudo editor /etc/epex-proxy/environment   # set ENTSOE_TOKEN=your_token
sudo editor /etc/epex-proxy/config.json   # adjust listen address / providers
sudo systemctl restart epex-proxy
```

---

## Configuration reference

```jsonc
{
  // Addresses and ports to listen on (plain HTTP).
  // Use 127.0.0.1 behind a local reverse proxy, 0.0.0.0 in Docker.
  "listen": [
    {"address": "127.0.0.1", "port": 8080}
  ],

  // Directory for per-day JSON cache files.
  // Created automatically if it doesn't exist.
  "data_dir": "/var/lib/epex-proxy",

  "areas": {
    // Area key — used in API paths, e.g. /nl/prices/today
    "nl": {
      "name": "Netherlands",
      "bidding_zone": "10YNL----------L",  // ENTSO-E EIC code
      "timezone": "Europe/Amsterdam",      // for local-date calculations

      // Energy tax (excl. VAT), EUR/kWh — supports date-range history
      "energy_tax": [
        {"valid_from": "2025-01-01", "value": 0.1228},
        {"valid_from": "2026-01-01", "value": 0.1108}
      ],

      "vat_rate": 0.21,  // applied on top of all consumer prices

      "providers": {
        // Provider key — used in API paths, e.g. /nl/prices/today?provider=tibber
        "tibber": {
          "name": "Tibber",
          // Delivery fee (EUR/kWh, excl. VAT) — what you pay per kWh consumed
          "delivery_fee":   [{"valid_from": "2025-01-01", "value": 0.0248}],
          // Redelivery fee (EUR/kWh, excl. VAT) — what you receive per kWh fed back
          "redelivery_fee": [{"valid_from": "2025-01-01", "value": 0.0248}]
        }
        // ... repeat for other providers
      }
    }
  }
}
```

Both `energy_tax`, `delivery_fee`, and `redelivery_fee` are **rate schedules**:
the entry with the highest `valid_from` that is ≤ the query date is used.
Add a new entry when a rate changes; old entries are kept for historical queries.

The ENTSO-E token is never in the config file. Pass it as:

```sh
export ENTSOE_TOKEN=your_token_here   # from source / systemd EnvironmentFile
```

---

## API

Full spec: [`openapi.yaml`](openapi.yaml)

| Endpoint | Description |
|---|---|
| `GET /health` | Fetch state and slot counts per area |
| `GET /{area}/prices/today` | Today's 15-min slots, EPEX and all-in per provider |
| `GET /{area}/prices/tomorrow` | Tomorrow's slots (available after ~13:00 CET) |
| `GET /{area}/prices/current` | Single slot for the current 15-min window |

Optional query parameter: `?provider=<key>` to get only one provider's prices.

---

## License

[MIT](LICENSE) — © 2026 Martijn Otto
