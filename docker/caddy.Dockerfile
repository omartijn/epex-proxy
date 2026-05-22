# ── Stage 1: Build the SPA ─────────────────────────────────────────────────
FROM node:22-alpine AS ui-builder

WORKDIR /ui
COPY ui/package*.json ./
RUN npm ci
COPY ui/ ./
RUN npm run build

# ── Stage 2: Caddy + built assets ──────────────────────────────────────────
FROM caddy:2-alpine

COPY --from=ui-builder /ui/dist /srv/www
# Caddyfile is mounted at runtime (contains {env.EPEX_DOMAIN})
