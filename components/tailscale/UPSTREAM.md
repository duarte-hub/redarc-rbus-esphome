# Tailscale component (vendored)

Copied from https://github.com/Csontikka/esphome-tailscale at release v0.5.28,
commit `0a34e85f1a0483e8bf4cb9951dbf1ac531f9fcf0` (2026-10-20), MIT licensed
(`licenses/esphome-tailscale-LICENSE`).

The bundled libraries live at the repository root in `microlink/`, where this component's
`__init__.py` expects them (`<repo>/microlink/components/microlink`):

| Library | Author | License |
| --- | --- | --- |
| microlink (Tailscale protocol client) | Cameron Malone | MIT, `microlink/LICENSE` |
| wireguard_lwip | Daniel Hope (floorsense.nz) | BSD-style, `microlink/components/microlink/components/wireguard_lwip/LICENSE` |
| X25519 | Daniel J. Bernstein | public domain, `microlink/x25519-license.txt` |

"Tailscale" is a trademark of Tailscale Inc., and "WireGuard" of Jason A. Donenfeld. Neither
project is affiliated with or endorses this code.

## Changes made here

- `__init__.py`: `disable_telemetry` defaults to `true` (upstream sends anonymous usage events
  to a Cloudflare Worker unless you turn it off).
- `__init__.py`, `tailscale.h`, `tailscale.cpp`: `auth_key` is optional. With no key in the YAML
  or saved on the device the client does not start and the setup hint says to enter one; saving a
  key (the `VPN Auth Key Override` text entity) starts it as before.
- `__init__.py`: `CONFIG_ML_ENABLE_CONFIG_HTTPD` is pinned off. That optional microlink HTTP
  server has unauthenticated POST endpoints (`/api/settings`, `/api/wifi`, `/api/restart`); it is
  already off by default and never started by this wrapper, so it is not in the firmware.
- `microlink/components/microlink/src/ml_coord.c`: the Noise message-2 body buffer is allocated
  at least 3 bytes. A 1-2 byte body after the HTTP headers was followed by a read of
  `3 - body_buf_len` bytes into the exact-size allocation (1-2 bytes written past the end).
  Reachable only from the control-server connection, which runs inside TLS. Worth reporting
  upstream.

Everything else is unmodified so a newer upstream release can be diffed and merged. Review the
upstream changelog before updating: this code runs on the device next to your Tailscale key.
- `microlink/components/microlink/src/ml_coord.c`, `fetch_server_pubkey()`: with no `login_server`
  (Tailscale's own control plane) the control server's Noise public key was fetched over plain HTTP
  on port 80. That key is what authenticates the server, so a man in the middle could substitute
  it, read the auth key sent in the registration request and impersonate the control plane. It is
  now fetched over HTTPS on port 443 with the built-in CA bundle. A custom `http://` login_server
  still works but logs a warning. Worth reporting upstream.
