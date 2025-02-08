# esphome

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
