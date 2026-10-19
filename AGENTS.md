# AGENTS.md

Personal ESPHome deployment: one YAML file per ESP8266/ESP32 device in a house,
built and flashed with `make` inside a Nix dev shell, integrated with
HomeAssistant over MQTT. There is no application to build and no test suite;
"correct" means `esphome compile` succeeds and the device behaves.

## Environment

Everything runs inside the Nix dev shell. `direnv` (`use flake`) loads it
automatically; otherwise prefix commands with `nix develop --command`.

```bash
nix develop                      # esphome, esptool, cc2538-bsl, gnumake
nix develop .#ci                 # CI variant, see "CI" below
```

ESPHome itself comes from nixpkgs (currently 2026.8.0, pinned via `flake.lock`),
not pip. Do not `pip install esphome`.

Two secrets files are required before anything compiles (both gitignored, both
have committed `.sample` counterparts):

| File                     | Keys                                                                                                                                  |
| ------------------------ | ------------------------------------------------------------------------------------------------------------------------------------- |
| `secrets.yaml`           | `somfy_remote_id_base`, `sntp_servers`                                                                                                |
| `templates/secrets.yaml` | `wifi_ssid`, `wifi_password`, `ota_password`, `mqtt_broker`, `mqtt_password`, `wifi_ap_lookup`, `api_encryption_key`, `syslog_server` |

```bash
cp -i secrets.yaml{.sample,}
cp -i templates/secrets.yaml{.sample,}
```

## Commands

```bash
make compile-all                       # compile every device
make compile-bathroomheater            # compile one device
make flash-all                         # OTA-flash everything with pending changes
make flash-bathroomheater              # OTA-flash one device
make -n flash-all | grep -E 'esphome (run|compile)'   # dry run: which devices are stale
make clean                             # drop stamps + .Makefile.deps (keeps .esphome/build)
```

Device name == YAML basename in the repo root (`bathroomheater.yaml` →
`make compile-bathroomheater`). Plain `esphome <cmd> <file>.yaml` works too once
inside the shell.

Gotchas:

- **Never pass `-j`/`--jobs`.** PlatformIO's library downloads are not
  parallel-safe. The Makefile sets `--keep-going` so one broken device does not
  stop the rest.
- Incremental builds are driven by touch-stamps in `.esphome/stamps/` plus a
  generated dependency file (see "Dependency tracking"). If a rebuild seems to
  be skipped, `touch` the stamp's prerequisites or `make clean`.
- Stamps also key on the resolved `esphome` binary path, so any nix upgrade
  rebuilds everything once.
- Flashing is always OTA (`esphome run --no-logs --device OTA`). Serial flashing
  of a fresh chip: `esphome run empty/empty32.yaml`, or upload
  `.esphome/build/<name>/.pioenvs/<name>/firmware.factory.bin` via
  https://web.esphome.io/.
- `empty/*.yaml` are minimal bootstrap configs (WiFi + OTA + `name_add_mac_suffix`)
  for brand-new chips. They live in a subdir on purpose so `make` does not treat
  them as devices; reference templates as `!include ../templates/wifi.yaml`.
- Serial console: `screen /dev/ttyUSB0 115200`. Note that `templates/syslog.yaml`
  sets `logger: baud_rate: 0`, so serial is silent by design on devices that
  include it; logs go to UDP syslog instead.
- Stale HomeAssistant MQTT entities: `esphome clean-mqtt <file>.yaml --topic 'homeassistant/+/<component>/#'`.

## Structure

```
*.yaml                     one file per device (the only things `make` discovers)
templates/                 reusable ESPHome packages, pulled in with !include
templates/mystrom.yaml     complete device template (myStrom ESP32 smart plug)
empty/                     bootstrap firmware for unflashed chips
components/                local external components (Python + C++)
components/ratgdo/         vendored garage-door controller (GPL, upstream Paul Wieland)
components/somfy/          Somfy RTS blinds via remote_transmitter
components/zehnder/        Zehnder heater IR climate
fonts/                     TTFs referenced by display: blocks
tubeszb/                   ZigBee coordinator firmware zips (flashed with cc2538-bsl)
Makefile                   device discovery, stamps, compile/flash rules
generate_deps.py           emits .Makefile.deps (included by the Makefile)
.github/workflows/compile-all.yaml
```

### Package composition

A device config is thin: an `esphome:` identity block, a `device_base:` package
that picks the board, then platform blocks.

```yaml
esphome:
  name: bathroomheater
  friendly_name: "Bathroom heater"
  area: "Bathroom"
  project:
    name: dasfoo.bathroomheater
    version: "1.0"

packages:
  device_base: !include templates/esp8266.yaml
  sensor: !include templates/esp8266-vcc.yaml
```

Board templates (`templates/esp8266.yaml`, `esp32.yaml`, `esp32-poe.yaml`,
`esp32-cam.yaml`, `esp01s.yaml`, `esp01m-ir.yaml`) each pull in the same set of
cross-cutting packages: `wifi`, `ota`, `mqtt`, `uptime`, `restart`, `syslog`.
`esp32-poe.yaml` swaps WiFi for Ethernet; `esp32-cam.yaml` and `btproxy.yaml`
use `esp-idf` framework and add `api`.

`templates/mystrom.yaml` is a whole device (relay, power sensor, buttons,
temperature shutdown). Devices based on it are only substitutions:

```yaml
substitutions:
  name: "freezer"
  friendly_name: "Freezer"
esphome:
  area: "Cellar"
packages:
  device_base: !include templates/mystrom.yaml
```

`templates/tubeszb-coordinator.yaml` is parameterised the same way via
`substitutions: channel: "24"`, which drives the node name
`zigbee-coordinator-ch${channel}`.

### Integration model

MQTT is the primary HomeAssistant integration (`mqtt.discovery`), with
`discovery_unique_id_generator: mac` so shared entities stay unique. The native
API (`templates/api.yaml`) is added only where needed — Bluetooth proxies, the
multi-device `blinds` config, the camera — and it explicitly disables MQTT
discovery to avoid the device appearing twice.

`templates/syslog.yaml` routes logs to a UDP syslog server using a fake
`platform: host` time source (workaround for esphome#12591) and disables both
serial and MQTT logging.

Schedules use `time:` `platform: sntp` with `servers: !secret sntp_servers` and
`timezone: Europe/Zurich`; `templates/time.yaml` adds a "clock invalid" sensor
that dereferences `id(sntp_time)`, so any device including it must define that
id. Per the README, the compiling machine's timezone also matters, since
ESPHome bakes the compiler's timezone in when syncing via NTP.

## Conventions

- Node names: kebab-case, equal to the file name. `project.name` is
  `dasfoo.<device>`; bump `project.version` on meaningful config changes.
- Entity `name:` values are human-readable with spaces. `name: None` is used
  deliberately for a device's primary entity (climate/relay) so HomeAssistant
  renders just the device name.
- Cross-device boilerplate entities are named `"ESP chip ..."` (uptime, restart
  button, MQTT connection uptime, WiFi signal, MAC/IP/BSSID).
- Housekeeping entities get `entity_category: diagnostic`, user tuning gets
  `entity_category: config`, and things not meant for HomeAssistant get
  `internal: true`.
- `id:` values are snake_case; ids referenced from templates must exist in the
  including file (e.g. `sntp_time`, `mystrom_relay`, `restart_button`).
- Lambdas are inline (`!lambda "return id(relay_1).state;"`) or block
  (`lambda: |-`) when multi-line.
- Comments explain hardware rationale (measured voltages, calibration factors,
  board quirks, upstream links) rather than what the YAML says.
- YAML is prettier-formatted (2-space indent); C++ is clang-format Google style
  (`.clang-format`); Python is black.

## Custom components

Local components are consumed with

```yaml
external_components:
  - source: components
```

or, to pull a single one, the nested form used by `garage-gate.yaml`:

```yaml
external_components:
  - source:
      type: local
      path: components
    components: [ratgdo]
```

Each component is a Python config layer (`__init__.py`, per-platform
subdirectories with `CONFIG_SCHEMA` + async `to_code`) over a C++
implementation (`.h`/`.cpp` in `namespace esphome::<name>`). `zehnder/climate.py`
and `somfy/cover.py` are the small, readable examples to copy.

Non-obvious bits:

- `somfy` stores its rolling code in MQTT under `/<prefix>/rolling_code/<remote_id>`
  and in flash. If neither has a code the component calls `mark_failed()` rather
  than transmit, to avoid desyncing the shared MQTT store. `remote_id_base` comes
  from `secrets.yaml`, each cover adds `remote_id_offset`; the base value is read
  off the MQTT broker (lowest integer key under that path).
- `ratgdo` counts observable subscribers in `CORE.data` and emits
  `RATGDO_MAX_*` defines at `CoroPriority.FINAL`; child platforms call
  `subscribe_*()` during codegen. Keep that ordering in mind when adding a
  sub-platform.
- `templates/tubeszb-coordinator.yaml` pulls a third-party component from
  GitHub (`source: github://oxan/esphome-stream-server`), cached under
  `.esphome/external_components/`.

## Dependency tracking

`generate_deps.py` regex-scans each device YAML for `!include` and `source:`
lines and writes `.Makefile.deps` (gitignored), which the Makefile `-include`s.
Consequences worth knowing:

- The include regex is `!include\s+([a-zA-Z0-9_/-]+\.yaml)` — no dots. Paths
  like `../templates/wifi.yaml` (used in `empty/`) are silently not tracked.
- The source regex is `source:\s+([a-zA-Z0-9_/-]+)`, which matches
  `- source: components` but **not** the nested `type:`/`path:` form. So edits to
  `components/ratgdo` do not invalidate `garage-gate`'s stamp; force a rebuild
  with `rm .esphome/stamps/.stamp-compile-garage-gate`.
- Generated `__pycache__` files inside `components/` end up as dependencies and
  make the deps file noisy.

## btproxy replicas

`btproxy.yaml` is excluded from device auto-discovery (`EXCLUDE_YAML` in the
Makefile) and built once per id in `BTPROXY_REPLICAS`:

```makefile
BTPROXY_REPLICAS := c25bac c2577c c25b74 c257a8
esphome run --no-logs --device btproxy-$* btproxy.yaml
```

Adding a physical proxy means adding its id there. `make flash-btproxies` and
`make compile-btproxies` act on all of them at once. Note the config hardcodes
`name: btproxy` with `name_add_mac_suffix: true`, so each replica is reachable as
`btproxy-<last 6 MAC hex>` — exactly the `BTPROXY_REPLICAS` ids. `-s name` does not
change the node name (substitutions only affect `${...}`), so all replicas share the
`.esphome/build/btproxy` directory. Flashing must name the device explicitly:
`--device OTA` mDNS-browses every `btproxy-<mac>.local` and would flash all proxies
at once, or none where multicast DNS does not reach the compiling machine; plain
`btproxy-<id>` resolves through LAN DNS. All proxies also disable MQTT on boot
because identical names would collide.

## CI

`.github/workflows/compile-all.yaml` copies the sample secrets and runs
`nix develop .#ci --command make compile-all`, caching `~/.platformio` and
`.esphome`. The `ci` dev shell overrides platformio with `platformio-core` to
avoid the bwrap wrapper that fails on runners without unprivileged user
namespaces. CI compiles only — nothing is flashed.

Linting/formatting runs as git hooks installed by the `nix develop` shellHook
(git-hooks.nix). `.pre-commit-config.yaml` is a generated symlink into
`/nix/store` and is gitignored — never edit it; change `flake.nix` instead.
`check-yaml` is disabled (chokes on `!include`/`!secret` tags) and
`flake-checker` is disabled (nixpkgs esphome breakage), both with comments in
`flake.nix` explaining why.

## Verifying changes

There is no test suite. The check is:

```bash
nix develop --command make compile-<device>
```

For a config-wide change (a file under `templates/`), compile every device that
includes it, or `make compile-all`. `esphome config <file>.yaml` renders the
merged config (packages, substitutions, secrets) and is the fastest way to see
what a template actually produced.
