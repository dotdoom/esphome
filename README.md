# esphome

## Installation

1. https://nixos.org/download/
2. direnv

Your PC timezone is important for esphome schedules, as they will use compiler's
timezone when syncing the clock (NTP).

## Compiling

```
$ cp -i secrets.yaml{.sample,}
$ cp -i templates/secrets.yaml{.sample,}

# Edit both secrets.yaml files.

$ ./reflash.sh compile ''
```

## Running

```
$ ./reflash.sh
```

or

```
$ esphome run file.yaml
```

Look up list of runnable configurations inside `reflash.sh` script.

## Running remotely

If you don't have esphome installed on a computer that you connect ESP chip to,
use https://web.esphome.io/ to upload the compiled binary.

The binary resides in
`.esphome/build/<component>/.pioenvs/<component>/firmware.factory.bin`.

## Serial console

Use `screen /dev/tty.usbserial-0001 115200` (or `/dev/ttyUSB*`) to read the
console.

## Cleaning up removed MQTT entities

This can be done via `clean-mqtt` command line:

```shell
$ esphome clean-mqtt spare-power-meter.yaml \
    --topic homeassistant/binary_sensor/<component>/<entity>/config
```

or

```shell
$ esphome clean-mqtt spare-power-meter.yaml \
    --topic homeassistant/+/<component>/#
```

When `--topic` is specified, the YAML file will only be used to extract MQTT
credentials. All other topics will remain untouched.

## Hardware: heating

### Photoresistor

Schematics:

1. A0 -> 10K Ohm -> GND
2. A0 -> Photoresistor -> 3V3

Assembly:

1. Put isolation band on the legs
2. Solder legs to dupont wire (the other end has to be male)
3. Bath it in the insulation varnish
4. Think the isolation band with a fan

Color coding for RJ45 cable:

- orange: furnace
- blue: floor

5528 photoresistor: 8-20K resistance by brightness, up to 1 MOhm.
Brightness range is 1.47 - 2.2V.
Darkness is 3.267V.
