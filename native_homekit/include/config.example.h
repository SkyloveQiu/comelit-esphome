#pragma once

// Copy this file to config.h and edit the local copy. config.h is ignored so
// Wi-Fi and HomeKit credentials are not published to a fork.
#define COMELIT_WIFI_SSID "YOUR_WIFI_SSID"
#define COMELIT_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define COMELIT_HOMEKIT_CODE "111-11-111"

// The webhook uses Authorization: Bearer <token>. Use a random value with at
// least 32 characters. The placeholder deliberately disables the endpoint.
#define COMELIT_WEBHOOK_ENABLED 1
#define COMELIT_WEBHOOK_PORT 80
#define COMELIT_WEBHOOK_TOKEN "CHANGE_ME_TO_A_RANDOM_SECRET"
#define COMELIT_WEBHOOK_MIN_INTERVAL_MS 3000

#define COMELIT_DEVICE_NAME "Comelit Intercom"
#define COMELIT_SERIAL_NUMBER "COMELIT-001"

// Set this to the address configured on the indoor intercom.
#define COMELIT_ADDRESS 1

// Supported values: 25, 26, 27, or 0 for the older board revision.
#define COMELIT_HW_VERSION 26

// For hardware 2.5: 0 = default/high sensitivity, 1 = low sensitivity.
// For hardware 2.6/2.7: 0 = default (position 8), otherwise 1..9.
#define COMELIT_SENSITIVITY 0

// 0 = Simplebus 2, 1 = Simplebus 1.
#define COMELIT_SIMPLEBUS1 0

// ESP8266 GPIO numbers, matching the supplied Comelit interface wiring.
#define COMELIT_RX_PIN 12 // D6
#define COMELIT_TX_PIN 5  // D1

#define COMELIT_TX2_ENABLED 0
#define COMELIT_TX2_PIN 4 // D2
#define COMELIT_CAPACITOR_PIN 4 // D2

#define COMELIT_CALL_COMMAND 50
#define COMELIT_MAIN_DOOR_COMMAND 16

// Additional main-door frames sent when a bus pulse is missed.
#define COMELIT_MAIN_DOOR_RETRIES 5
#define COMELIT_MAIN_DOOR_RETRY_DELAY_MS 250

// Set to 1 only to remove HomeKit pairing data on the next boot; then set it
// back to 0 and flash again.
#define COMELIT_RESET_HOMEKIT 0
