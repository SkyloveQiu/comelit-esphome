#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <arduino_homekit_server.h>

#include "config.h"

extern "C" {
extern homekit_server_config_t config;
extern homekit_characteristic_t cha_doorbell_event;
extern homekit_characteristic_t cha_main_door;
}

namespace {

constexpr uint16_t kEdgeBufferSize = 400;
constexpr uint32_t kFilterUs = 1000;
constexpr uint32_t kIdleUs = 10000;

bool timeReached(uint32_t now, uint32_t target) {
    return static_cast<int32_t>(now - target) >= 0;
}

enum JumperState : uint8_t {
    JUMPER_OPEN = 0,
    JUMPER_HIGH = 1,
    JUMPER_LOW = 2,
};

void setJumper(uint8_t pin, JumperState state) {
    if (state == JUMPER_OPEN) {
        pinMode(pin, INPUT);
    } else {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, state == JUMPER_HIGH ? HIGH : LOW);
    }
}

void configureBoardRevision() {
#if COMELIT_HW_VERSION == 25
    // On 2.5, D5 open selects low sensitivity and D5 grounded selects high.
    setJumper(14, COMELIT_SENSITIVITY == 1 ? JUMPER_OPEN : JUMPER_LOW);
#elif COMELIT_HW_VERSION == 26 || COMELIT_HW_VERSION == 27
    // Positions 1..9 are the same D5/D0 combinations as the ESPHome
    // component. Position 8 is the default used by the supplied firmware.
    const uint8_t position = COMELIT_SENSITIVITY == 0 ? 8 : COMELIT_SENSITIVITY;
    const JumperState d5[] = {
        JUMPER_OPEN, // unused index 0
        JUMPER_HIGH, JUMPER_HIGH, JUMPER_HIGH,
        JUMPER_OPEN, JUMPER_LOW, JUMPER_OPEN,
        JUMPER_OPEN, JUMPER_LOW, JUMPER_LOW,
    };
    const JumperState d0[] = {
        JUMPER_OPEN, // unused index 0
        JUMPER_HIGH, JUMPER_OPEN, JUMPER_LOW,
        JUMPER_HIGH, JUMPER_HIGH, JUMPER_OPEN,
        JUMPER_LOW, JUMPER_OPEN, JUMPER_LOW,
    };
    const uint8_t safePosition = position >= 1 && position <= 9 ? position : 8;
    setJumper(14, d5[safePosition]); // D5
    setJumper(16, d0[safePosition]); // D0
#endif
}

class ComelitBus {
public:
    using FrameHandler = void (*)(uint8_t command, uint8_t address);

    void begin(FrameHandler handler) {
        handler_ = handler;
        configureBoardRevision();

#if COMELIT_HW_VERSION == 27
        pinMode(COMELIT_CAPACITOR_PIN, OUTPUT);
        digitalWrite(COMELIT_CAPACITOR_PIN, LOW);
        capacitorReleaseAt_ = millis() + 8000;
        capacitorReleased_ = false;
#endif

        pinMode(COMELIT_TX_PIN, OUTPUT);
        digitalWrite(COMELIT_TX_PIN, LOW);
#if COMELIT_TX2_ENABLED
        pinMode(COMELIT_TX2_PIN, OUTPUT);
        digitalWrite(COMELIT_TX2_PIN, LOW);
#endif

        pinMode(COMELIT_RX_PIN, INPUT);
        noInterrupts();
        writeIndex_ = 0;
        readIndex_ = 0;
        overflow_ = false;
        edgeTimes_[0] = micros();
        interrupts();

        instance_ = this;
        attachInterrupt(digitalPinToInterrupt(COMELIT_RX_PIN), rxInterrupt, CHANGE);
        Serial.printf("Comelit native bus ready: HW %d, RX GPIO%d, TX GPIO%d\n",
                      COMELIT_HW_VERSION, COMELIT_RX_PIN, COMELIT_TX_PIN);
    }

    void loop() {
#if COMELIT_HW_VERSION == 27
        if (!capacitorReleased_ && timeReached(millis(), capacitorReleaseAt_)) {
            digitalWrite(COMELIT_CAPACITOR_PIN, HIGH);
            capacitorReleased_ = true;
        }
#endif

        if (mainRetryPending_ && !sending_ && timeReached(millis(), mainRetryDueAt_)) {
            if (mainRetriesRemaining_ == 0) {
                mainRetryPending_ = false;
            } else {
                --mainRetriesRemaining_;
                sendCommand(COMELIT_MAIN_DOOR_COMMAND, COMELIT_ADDRESS);
            }
        }

        if (sending_) {
#if COMELIT_SIMPLEBUS1
            sendingLoopSimplebus1();
#else
            sendingLoopSimplebus2();
#endif
            return;
        }

        receiveLoop();
    }

    bool sendMainDoor() {
        if (sending_ || mainRetryPending_) {
            Serial.println("Comelit: main door request ignored while busy");
            return false;
        }
        mainRetriesRemaining_ = COMELIT_MAIN_DOOR_RETRIES;
        mainRetryPending_ = true;
        return sendCommand(COMELIT_MAIN_DOOR_COMMAND, COMELIT_ADDRESS);
    }

private:
    static ComelitBus *instance_;
    static volatile uint32_t edgeTimes_[kEdgeBufferSize];
    static volatile uint16_t writeIndex_;
    static volatile uint16_t readIndex_;
    static volatile bool overflow_;

    FrameHandler handler_{nullptr};
    bool sending_{false};
    bool preamble_{false};
    bool sendBuffer_[19]{};
    uint8_t sendIndex_{0};
    uint32_t sendNextBit_{0};
    uint32_t sendNextChange_{0};
    bool mainRetryPending_{false};
    uint8_t mainRetriesRemaining_{0};
    uint32_t mainRetryDueAt_{0};
#if COMELIT_HW_VERSION == 27
    bool capacitorReleased_{false};
    uint32_t capacitorReleaseAt_{0};
#endif

    static void ICACHE_RAM_ATTR rxInterrupt() {
        if (instance_ == nullptr) {
            return;
        }

        const uint16_t next = (writeIndex_ + 1) % kEdgeBufferSize;
        // The comparator output is inverted. Keep the same edge parity as the
        // original ESPHome implementation so frame lengths remain 38/76.
        const bool level = !digitalRead(COMELIT_RX_PIN);
        if (level != (next & 1)) {
            return;
        }

        const uint32_t now = micros();
        if (now - edgeTimes_[writeIndex_] <= kFilterUs) {
            return;
        }
        if (next == readIndex_) {
            overflow_ = true;
            return;
        }

        edgeTimes_[next] = now;
        writeIndex_ = next;
    }

    void writeTx(bool active) {
        digitalWrite(COMELIT_TX_PIN, active ? HIGH : LOW);
#if COMELIT_TX2_ENABLED
        digitalWrite(COMELIT_TX2_PIN, active ? HIGH : LOW);
#endif
    }

    void toggleTx() {
        digitalWrite(COMELIT_TX_PIN, !digitalRead(COMELIT_TX_PIN));
#if COMELIT_TX2_ENABLED
        digitalWrite(COMELIT_TX2_PIN, !digitalRead(COMELIT_TX2_PIN));
#endif
    }

    bool sendCommand(uint8_t command, uint8_t address) {
        if (sending_) {
            Serial.printf("Comelit: command %u ignored while busy\n", command);
            return false;
        }

        detachInterrupt(digitalPinToInterrupt(COMELIT_RX_PIN));

        uint8_t checksum = 0;
        uint8_t index = 0;
        for (uint8_t bit = 0; bit < 6; ++bit) {
            sendBuffer_[index++] = (command >> bit) & 1;
            checksum += sendBuffer_[index - 1] ? 1 : 0;
        }
        for (uint8_t bit = 0; bit < 8; ++bit) {
            sendBuffer_[index++] = (address >> bit) & 1;
            checksum += sendBuffer_[index - 1] ? 1 : 0;
        }
        for (uint8_t bit = 0; bit < 4; ++bit) {
            sendBuffer_[index++] = (checksum >> bit) & 1;
        }
        // The original implementation sends one trailing zero pulse after the
        // 18 protocol bits. This is retained for compatibility with the PCB.
        sendBuffer_[18] = false;

#if COMELIT_HW_VERSION == 27
        digitalWrite(COMELIT_CAPACITOR_PIN, LOW);
        capacitorReleased_ = false;
        capacitorReleaseAt_ = millis() + 3000;
#endif

        sendIndex_ = 0;
        sendNextBit_ = 0;
        sendNextChange_ = 0;
        preamble_ = true;
        sending_ = true;
        Serial.printf("Comelit: sending command %u, address %u\n", command, address);
        return true;
    }

    void finishSending() {
        sending_ = false;
        preamble_ = false;
        sendNextBit_ = 0;
        sendNextChange_ = 0;
        sendIndex_ = 0;
        writeTx(false);
        attachInterrupt(digitalPinToInterrupt(COMELIT_RX_PIN), rxInterrupt, CHANGE);
        if (mainRetryPending_ && mainRetriesRemaining_ > 0) {
            mainRetryDueAt_ = millis() + COMELIT_MAIN_DOOR_RETRY_DELAY_MS;
        }
    }

    void sendingLoopSimplebus2() {
        uint32_t now = micros();
        if (preamble_) {
            if (sendNextBit_ == 0 && sendNextChange_ == 0) {
                writeTx(true);
                sendNextBit_ = now + 3000;
                sendNextChange_ = now + 20;
                while (!timeReached(micros(), sendNextBit_)) {
                    if (timeReached(micros(), sendNextChange_)) {
                        toggleTx();
                        sendNextChange_ += 20;
                    }
                }
                sendNextBit_ = 0;
                sendNextChange_ += 16000;
                writeTx(false);
                return;
            }
            if (!timeReached(now, sendNextChange_)) {
                return;
            }
            sendNextBit_ = now + 3000;
            sendNextChange_ = now + 20;
            preamble_ = false;
        }

        if (sendIndex_ < 19) {
            if (sendNextChange_ > 0) {
                while (!timeReached(micros(), sendNextBit_)) {
                    if (timeReached(micros(), sendNextChange_)) {
                        toggleTx();
                        sendNextChange_ += 20;
                    }
                }
                sendNextChange_ = 0;
                writeTx(false);
                sendNextBit_ += sendBuffer_[sendIndex_] ? 6000 : 3000;
            } else {
                if (!timeReached(now, sendNextBit_)) {
                    return;
                }
                sendNextBit_ = now + 3000;
                sendNextChange_ = now + 20;
                ++sendIndex_;
            }
        } else {
            finishSending();
        }
    }

    void sendingLoopSimplebus1() {
        uint32_t now = micros();
        if (preamble_) {
            if (sendNextBit_ == 0 && sendNextChange_ == 0) {
                writeTx(true);
                sendNextBit_ = now + 3000;
                while (!timeReached(micros(), sendNextBit_)) {
                    yield();
                }
                sendNextBit_ = 0;
                sendNextChange_ = now + 16000;
                writeTx(false);
                return;
            }
            if (!timeReached(now, sendNextChange_)) {
                return;
            }
            sendNextBit_ = now + 3000;
            sendNextChange_ = now + 3020;
            preamble_ = false;
        }

        if (sendIndex_ < 19) {
            if (sendNextChange_ > 0) {
                writeTx(true);
                while (!timeReached(micros(), sendNextBit_)) {
                    yield();
                }
                sendNextChange_ = 0;
                writeTx(false);
                sendNextBit_ += sendBuffer_[sendIndex_] ? 6000 : 3000;
            } else {
                if (!timeReached(now, sendNextBit_)) {
                    return;
                }
                sendNextBit_ = now + 3000;
                sendNextChange_ = now + 3020;
                ++sendIndex_;
            }
        } else {
            finishSending();
        }
    }

    void receiveLoop() {
        uint16_t writeAt;
        uint16_t readAt;
        noInterrupts();
        writeAt = writeIndex_;
        readAt = readIndex_;
        const bool overflow = overflow_;
        const uint32_t lastChange = edgeTimes_[writeAt];
        interrupts();

        const uint16_t distance = (kEdgeBufferSize + writeAt - readAt) % kEdgeBufferSize;
        if (distance <= 1) {
            return;
        }
        if (micros() - lastChange < kIdleUs) {
            return;
        }

        if (overflow) {
            Serial.println("Comelit: receive buffer overflow; frame discarded");
            noInterrupts();
            readIndex_ = writeIndex_;
            overflow_ = false;
            interrupts();
            return;
        }

        static uint32_t durations[kEdgeBufferSize];
        uint16_t count = 0;

        noInterrupts();
        writeAt = writeIndex_;
        if (micros() - edgeTimes_[writeAt] < kIdleUs) {
            interrupts();
            return;
        }

        // Skip the first timestamp from the preceding idle level, matching the
        // framing used by the original component.
        readAt = (readAt + 1) % kEdgeBufferSize;
        uint16_t previous = readAt;
        readAt = (readAt + 1) % kEdgeBufferSize;
        while (previous != writeAt && count < kEdgeBufferSize - 1) {
            const uint32_t delta = edgeTimes_[readAt] - edgeTimes_[previous];
            if (delta >= kIdleUs) {
                break;
            }
            durations[count++] = delta;
            previous = readAt;
            readAt = (readAt + 1) % kEdgeBufferSize;
        }
        readAt = (kEdgeBufferSize + readAt - 1) % kEdgeBufferSize;
        readIndex_ = readAt;
        interrupts();

        if (count == 0) {
            return;
        }
        durations[count++] = kIdleUs;

        if (count == 76 && !COMELIT_SIMPLEBUS1) {
            Serial.println("Comelit: received Simplebus 1 frame while Simplebus 2 TX is selected");
        }
        if (count == 38 || count == 76) {
            decode(durations, count);
        }
    }

    void decode(const uint32_t *durations, uint16_t count) {
        uint8_t bits[18]{};
        uint8_t bitCount = 0;

        if (count == 38) {
            for (uint16_t i = 1; i < count - 1; i += 2) {
                const uint32_t value = durations[i];
                if (value < 3200 && value > 1000) {
                    bits[bitCount++] = 0;
                } else if (value < 6200 && value > 3500) {
                    bits[bitCount++] = 1;
                }
            }
        } else {
            for (uint16_t i = 3; i < count - 1; i += 4) {
                const uint32_t value = durations[i];
                if (value < 2500 && value > 1000) {
                    bits[bitCount++] = 0;
                } else if (value < 6200 && value > 3500) {
                    bits[bitCount++] = 1;
                }
            }
        }

        if (bitCount != 18) {
            return;
        }

        uint8_t ones = 0;
        for (uint8_t i = 0; i < 14; ++i) {
            ones += bits[i] ? 1 : 0;
        }
        const uint8_t receivedChecksum = bits[14] | (bits[15] << 1) |
                                          (bits[16] << 2) | (bits[17] << 3);
        if (receivedChecksum != ones) {
            return;
        }

        uint8_t command = 0;
        uint8_t address = 0;
        for (uint8_t i = 0; i < 6; ++i) {
            command |= bits[i] << i;
        }
        for (uint8_t i = 0; i < 8; ++i) {
            address |= bits[i + 6] << i;
        }

        if (command == 63) {
            return;
        }
        Serial.printf("C%u_A%u\n", command, address);
        if (handler_ != nullptr) {
            handler_(command, address);
        }
    }
};

ComelitBus *ComelitBus::instance_ = nullptr;
volatile uint32_t ComelitBus::edgeTimes_[kEdgeBufferSize]{};
volatile uint16_t ComelitBus::writeIndex_ = 0;
volatile uint16_t ComelitBus::readIndex_ = 0;
volatile bool ComelitBus::overflow_ = false;

ComelitBus bus;

homekit_value_t doorbellEventGetter() {
    return HOMEKIT_NULL_CPP();
}

bool mainDoorOffPending = false;
uint32_t mainDoorOffAt = 0;

void mainDoorSetter(const homekit_value_t value) {
    if (!value.bool_value) {
        cha_main_door.value.bool_value = false;
        mainDoorOffPending = false;
        return;
    }
    bus.sendMainDoor();
    cha_main_door.value.bool_value = true;
    // Do not notify OFF from inside the HomeKit write callback. The library is
    // still completing the write response at that point, which can leave iOS
    // showing "Updating". Let the main loop send the OFF notification later.
    mainDoorOffPending = true;
    mainDoorOffAt = millis() + 500;
}

void serviceMainDoorSwitch() {
    if (!mainDoorOffPending || !timeReached(millis(), mainDoorOffAt)) {
        return;
    }
    mainDoorOffPending = false;
    cha_main_door.value.bool_value = false;
    homekit_characteristic_notify(&cha_main_door, cha_main_door.value);
}

void onComelitFrame(uint8_t command, uint8_t address) {
    if (command == COMELIT_CALL_COMMAND && address == COMELIT_ADDRESS) {
        cha_doorbell_event.value.uint8_value = 0; // single press
        homekit_characteristic_notify(&cha_doorbell_event, cha_doorbell_event.value);
        Serial.println("HomeKit: incoming call notification sent");
    }
}

void connectWifi() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);
    WiFi.begin(COMELIT_WIFI_SSID, COMELIT_WIFI_PASSWORD);

    Serial.printf("Connecting to Wi-Fi: %s", COMELIT_WIFI_SSID);
    const uint32_t startedAt = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print('.');
        if (millis() - startedAt > 30000) {
            Serial.println(" timeout; rebooting");
            ESP.restart();
        }
    }
    Serial.printf("\nWi-Fi connected: %s\n", WiFi.localIP().toString().c_str());
}

void setupHomeKit() {
    cha_doorbell_event.getter = doorbellEventGetter;
    cha_main_door.setter = mainDoorSetter;

#if COMELIT_RESET_HOMEKIT
    Serial.println("Resetting HomeKit pairing data");
    homekit_storage_reset();
#endif

    arduino_homekit_setup(&config);
    Serial.printf("HomeKit is ready; pair with code %s\n", COMELIT_HOMEKIT_CODE);
}

} // namespace

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println();
    Serial.println("Comelit native HomeKit firmware");
    Serial.printf("Configured intercom address: %u\n", COMELIT_ADDRESS);

    connectWifi();
    bus.begin(onComelitFrame);
    setupHomeKit();
}

void loop() {
    bus.loop();
    serviceMainDoorSwitch();
    arduino_homekit_loop();
    delay(1);
}
