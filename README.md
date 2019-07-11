# ESP8622 based Scheduling IR Thermostat
Drag your heat pump kicking and screaming into the 21st century for under $10.

<img align="center" src="https://github.com/davidhampgonsalves/IR-Schedule-Thermostat/raw/master/esp8622.jpg"/>

## Details
Many heat-pumps only support basic, once a day, on / off scheduling and do not support external thermostat input.

This project uses an ESP8622, IR LED and a battery to download your thermostat schedule and send updates to the heat pump via IR (just like the remote) as each scheduled temperature change or power event occurs.

The ESP deep sleeps between updates so it can run on a small battery for months.

More details can be found on my [blog](https://www.davidhampgonsalves.com/esp-based-scheduling-thermostat/).

## Setup
These steps are intended for a NodeMCU v3(Lolin) but should work with slight modifications on any ESP8622.

- Compile / Flash using [platform.io](https://platformio.org/) via: `pio run`
- Attach a IR LED to pin 4(D2 on my board) and ground. A resistor isn't required since the ESP can't even source enough current to full illuminate the LED.
- Connect pins D0 to the RST pin to enable deep sleep (on my board this breaks code uploading so I made this connection plugable).
- Connect battery to VIN (regulated power will no longer work if you disconnected the voltage regulator).
- Mount board close to receiver IR on heatpump (small black square on front of unit).

If you want your ESP to last months you will need(at minimum) to remove the voltage regulator and maybe other components that will be specific to your development board. In my case I used a NodeMCU v3 (Lolin) board so I followed [these steps](https://itooktheredpill.irgendwo.org/2017/reducing-nodemcu-power-consumption/).
