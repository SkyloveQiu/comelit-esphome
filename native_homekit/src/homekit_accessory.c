#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <stdio.h>

#include "config.h"

void comelit_accessory_identify(homekit_value_t value) {
    (void)value;
    printf("HomeKit identify requested\n");
}

// A Doorbell service is used for the incoming call event. The characteristic
// is event-only; main.cpp installs a getter that returns null as required by
// HAP for Programmable Switch Event.
homekit_characteristic_t cha_doorbell_event =
    HOMEKIT_CHARACTERISTIC_(PROGRAMMABLE_SWITCH_EVENT, 0);

// These are momentary switches. main.cpp sends the Comelit command and returns
// the characteristic to OFF immediately after a HomeKit tap.
homekit_characteristic_t cha_main_door = HOMEKIT_CHARACTERISTIC_(ON, false);

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(
        .id = 1,
        .category = homekit_accessory_category_video_door_bell,
        .services = (homekit_service_t *[]) {
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION,
                .characteristics = (homekit_characteristic_t *[]) {
                    HOMEKIT_CHARACTERISTIC(NAME, COMELIT_DEVICE_NAME),
                    HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Comelit / native HomeKit"),
                    HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, COMELIT_SERIAL_NUMBER),
                    HOMEKIT_CHARACTERISTIC(MODEL, "Comelit ESP8266"),
                    HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "native-1.0.0"),
                    HOMEKIT_CHARACTERISTIC(IDENTIFY, comelit_accessory_identify),
                    NULL
                }),
            HOMEKIT_SERVICE(DOORBELL,
                .primary = true,
                .characteristics = (homekit_characteristic_t *[]) {
                    HOMEKIT_CHARACTERISTIC(NAME, "Incoming Call"),
                    &cha_doorbell_event,
                    NULL
                }),
            HOMEKIT_SERVICE(SWITCH,
                .characteristics = (homekit_characteristic_t *[]) {
                    HOMEKIT_CHARACTERISTIC(NAME, "Open Main Door"),
                    &cha_main_door,
                    NULL
                }),
            NULL
        }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    // Incremented after removing the secondary-door service so HomeKit
    // refreshes the accessory database for already-paired controllers.
    .config_number = 2,
    .password = COMELIT_HOMEKIT_CODE
};
