// elevator remote control
#include <SPI.h>
#include "RF24.h"

const int ACTIVITY_LED = D0;
//const int TXLED = D4; // actually also CE on radio
const int ELEV_IS_UP_LED = D1;
const int ELEV_IS_DOWN_LED = D2;

RF24 radio(D4, D8);
// these must be the same in the relay code
enum {TXADR=0, RXADR};
uint8_t addresses[][6] = { "Node0","Node1" };
//const uint64_t txAddress = 0xE8E8F0F0E1LL;    // send controls
//const uint64_t rxAddress = 0xE8E8F0F0E2LL;     // get elevator position
unsigned long lastTimeButtonSent;
// the bits in the control byte, also the index to the button arrays
#define UP 0
#define DOWN 1
// button arrays
int btnPins[] = { 9, 10 };  // SD3/GPIO10 and SD2/GPI09
// these are bit arrays
byte btnValues = 0;
byte btnPrevious = 0;
byte btnChanged = 0;
// where the elevator is, bit 0 is up, bit 1 is down
byte elevatorPosition = 0;

// update the buttons, debounce, set changed entry and new entry
// return true if anything changed
bool btnCheck() {
    bool retval = false;
    for (int i = 0; i < (sizeof(btnPins) / sizeof(*btnPins)); ++i) {
        bool newval = !digitalRead(btnPins[i]);
        // debounce if changed
        if (newval != bitRead(btnPrevious, i)) {
            delay(50);
            // read it again
            newval = !digitalRead(btnPins[i]);
            // if really changed, flag it
            if (newval != bitRead(btnPrevious, i)) {
                bitWrite(btnPrevious, i, newval);
                bitWrite(btnValues, i, newval);
                bitSet(btnChanged, i);
                retval = true;
            }
        }
    }
    return retval;
}

// get the first changed index button, -1 for none
int btnChangedIndex() {
    for (int i = 0; i < (sizeof(btnPins) / sizeof(*btnPins)); ++i) {
        if (bitRead(btnChanged, i)) {
            // reset the flag
            bitClear(btnChanged, i);
            return i;
        }
    }
    return -1;  // didn't find any
}

// send data
bool xmitData(byte* data, int bytes) {
    radio.stopListening();
    radio.openWritingPipe(addresses[TXADR]);
    int tries = 5;
    bool success = false;
    while (tries-- && !success) {
        // attempt to send it
        success = radio.write(data, bytes);
        if (!success)
            delay(25);
    }
    //  digitalWrite(TXLED, HIGH);
    radio.startListening();
    return success;
}

// blink the light
void blinkLED(int times, int timeBetween, int timeAfter) {
    while (times--) {
        digitalWrite(ACTIVITY_LED, LOW);
        delay(timeBetween);
        digitalWrite(ACTIVITY_LED, HIGH);
        if (times && timeAfter)
            delay(timeAfter);
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("init...");
    delay(10);
    pinMode(ACTIVITY_LED, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
    pinMode(ELEV_IS_DOWN_LED, OUTPUT);
    digitalWrite(ELEV_IS_DOWN_LED, LOW);
    pinMode(ELEV_IS_UP_LED, OUTPUT);
    digitalWrite(ELEV_IS_UP_LED, LOW);
    blinkLED(1, 500, 0);
    for (int i = 0; i < (sizeof(btnPins) / sizeof(*btnPins)); ++i) {
        if (btnPins[i]) {
            pinMode(btnPins[i], INPUT_PULLUP);
            Serial.print("button: ");
            Serial.println(btnPins[i]);
        }
    }
    Serial.println("running");
    radio.begin();
    //  radio.setPALevel(RF24_PA_MAX);
    radio.setChannel(100);
    radio.setDataRate(RF24_1MBPS);
    //  radio.setDataRate(RF24_250KBPS);
    radio.openReadingPipe(1, addresses[RXADR]);
    radio.startListening();
    lastTimeButtonSent = millis();
    //  digitalWrite(TXLED, HIGH);
    digitalWrite(ELEV_IS_DOWN_LED, HIGH);
    delay(500);
    digitalWrite(ELEV_IS_DOWN_LED, LOW);
    digitalWrite(ELEV_IS_UP_LED, HIGH);
    delay(500);
    digitalWrite(ELEV_IS_UP_LED, LOW);
    delay(250);
#if 0
    analogWriteRange(100);
    for (int i = 0; i < 100; ++i) {
        analogWrite(ELEV_IS_DOWN_LED, i);
        delay(50);
    }
#endif // 0
}

byte lastData = 0;

void loop() {
    if (btnCheck()) {
        // something changed, get the index
        int btn = btnChangedIndex();
        Serial.print(btn ? "DOWN " : "UP ");
        Serial.println(bitRead(btnValues, btn) ? "PRESSED" : "RELEASED");
        // force an xmit
        lastTimeButtonSent = 0;
    }
    // set current data
    byte data = btnValues;
    // can't set both bits!
    if (data >= 3)
        data = 0;
    // see if time to send and different from last time
    if (millis() > lastTimeButtonSent + 2000) {
        lastTimeButtonSent = millis();
        // keep sending data if not 0 and last one wasn't zero
        // I.E. no reason to keep sending 0's
    //    Serial.println("lastData: "+String(lastData));
        if (data || data != lastData) {
            Serial.println("sending: " + String(data));
            if (xmitData(&data, sizeof(data))) {
                blinkLED(1, 25, 0);
            }
            else {
                Serial.println("tx failed");
                blinkLED(5, 50, 25);
            }
        }
        lastData = data;
    }
    // get and set the elevator status
    // get from radio
    while (radio.available()) {
        Serial.println("got elevator position");
        radio.read(&elevatorPosition, sizeof elevatorPosition);
        digitalWrite(ELEV_IS_DOWN_LED, bitRead(elevatorPosition, DOWN) ? HIGH : LOW);
        digitalWrite(ELEV_IS_UP_LED, bitRead(elevatorPosition, UP) ? HIGH : LOW);
    }
    //  Serial.println("position:" + String(elevatorPosition));
    delay(10);
}
