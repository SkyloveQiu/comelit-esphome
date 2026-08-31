# Comelit native HomeKit firmware

This branch is a personal native-HomeKit build for an ESP-12F Comelit
interface. It is intended for a Comelit 6701W installation and one main door.

This is a standalone Arduino/PlatformIO firmware for the Comelit interface
board. It does not use Home Assistant, ESPHome API, Homebridge, or another
bridge. It uses the native HAP implementation in
[Arduino-HomeKit-ESP8266](https://github.com/Mixiaoxiao/Arduino-HomeKit-ESP8266).

The accessory exposes:

- **Incoming Call** as a HomeKit doorbell event;
- **Open Main Door** as a momentary HomeKit switch that sends command 16;
- a token-protected HTTP webhook for **Open Main Door**;

Audio and video are not available: the Comelit board only decodes and sends
Simplebus commands.

## Configure and flash

1. Install [VS Code](https://code.visualstudio.com/) and the PlatformIO IDE
   extension, or install PlatformIO Core.
2. Open the `native_homekit` folder as the PlatformIO project.
3. Copy `include/config.example.h` to `include/config.h`, then edit the local
   `config.h`:
   - set `COMELIT_WIFI_SSID` and `COMELIT_WIFI_PASSWORD`;
   - set `COMELIT_HW_VERSION` to the revision printed on the PCB or supplied
     with the board;
   - set `COMELIT_ADDRESS` to the S1 address of the indoor intercom;
   - keep `COMELIT_SIMPLEBUS1` at `0` for Simplebus 2, unless your installation
     is known to use Simplebus 1.
   Do not commit `config.h`: it is intentionally ignored because it contains
   local Wi-Fi and HomeKit credentials.
4. If the ESP-12F is a bare module on the Comelit PCB, use a 3.3 V USB-UART
   adapter: connect GND, cross TX/RX, hold GPIO0 low while resetting for the
   first upload, then let GPIO0 return high for normal boot. If it is mounted
   on a Wemos D1 mini carrier, use the `d1_mini` environment and its USB port.
   Do **not** connect USB while the board is powered from the Comelit bus
   unless the hardware documentation explicitly confirms that both supplies
   are safe together.
5. Build and upload the `esp12f` environment (or `d1_mini` for that carrier).

The first native HomeKit flash should be done with an erase-flash operation if
the board previously ran ESPHome. This removes old firmware data and any stale
HomeKit pairing data. After uploading, open the serial monitor at 115200 baud.

## Find the Comelit address

The firmware prints every valid bus frame as `C<command>_A<address>`. Press a
button on the indoor intercom and note the address in the resulting line. Put
that value into `COMELIT_ADDRESS`, upload again, and reconnect the board to the
bus. A command 50 frame with that address will now trigger **Incoming Call**.

## Pair with Apple Home

After Wi-Fi connects, the accessory is advertised on the local network. Add it
from the Apple Home app as an accessory and enter the setup code from
`COMELIT_HOMEKIT_CODE` (the default is `111-11-111`). The ESP8266 HomeKit
implementation may take several seconds to become visible and pairing is much
more reliable at 160 MHz, which is already selected in `platformio.ini`.

If the accessory was paired before and no longer pairs after changing the
accessory definition, set `COMELIT_RESET_HOMEKIT` to `1`, flash once, then set
it back to `0` and flash again.

## Webhook for Apple Shortcuts

HomeKit remains enabled. The firmware also starts an HTTP server on the
configured port (default `80`) with these endpoints:

```text
POST /api/door/open
GET  /api/health
```

Both endpoints require either of these headers:

```text
Authorization: Bearer YOUR_WEBHOOK_TOKEN
```

or:

```text
X-Webhook-Token: YOUR_WEBHOOK_TOKEN
```

The open endpoint returns `202` when the Comelit command has been accepted,
`401` for a wrong token, `409` while the bus is busy, and `429` when another
accepted request arrives within the three-second safety window. In Apple
Shortcuts, use **Get Contents of URL**, set the method to `POST`, leave the
request body empty, and add the `Authorization` header.

The token is compiled into the ignored local `include/config.h`; it is never
written to Git. Change it and flash again to rotate it. A placeholder token
disables the endpoint rather than leaving an unauthenticated webhook.

Important security note: the ESP8266 server is plain HTTP. Do not forward port
80 directly to the Internet unless you accept that the token can be captured
by anyone able to observe the connection. For public access, put an HTTPS
reverse proxy in front of the ESP (for example Azure Functions, an Azure
Application Gateway, or a VPN) and forward only the protected internal
request. At minimum, use a non-default external port, disable UPnP, and add a
firewall allow-list where possible.

## Tested personal configuration

The tested device uses an ESP-12F, a Comelit 6701W with S1 switches 1 and 5
set to ON (address 17), Simplebus 2, and one main-door command. The firmware
sends each complete frame without yielding to HomeKit or the WebServer, then
sends five additional attempts with a 250ms gap. The HomeKit switch reset is
also delayed to improve reliability on a busy bus.

## Hardware notes

The pin defaults and the sensitivity jumpers mirror the current ESPHome
component: RX GPIO12, TX GPIO5, and the 2.6/2.7 sensitivity selection on
D5/GPIO14 and D0/GPIO16. Hardware 2.7 also controls the power capacitor on
D2/GPIO4 during startup and transmission.

This is an uncertified DIY HomeKit accessory. It is intended for local use and
does not provide Apple-certified video doorbell features.
