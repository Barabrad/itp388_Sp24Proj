#define BLYNK_TEMPLATE_ID "[REDACTED]"
#define BLYNK_TEMPLATE_NAME "388 Project"
#define BLYNK_AUTH_TOKEN "[REDACTED]"
#define RFID_ADDR 0x7D // Default I2C address for RFID

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include "pitches.h"
#include <Wire.h> 
#include "SparkFun_Qwiic_Rfid.h"

typedef unsigned long u_long;

// Your WiFi credentials.
// Set password to "" for open networks.
// https://examples.blynk.cc/?board=ESP32&shield=ESP32%20WiFi&example=GettingStarted%2FBlynkBlink
char ssid[] = "YourNetworkName";
char pass[] = "YourPassword";

// Initialize RFID reader
Qwiic_Rfid myRfid(RFID_ADDR);

// ***************************** //
// ***** Constant Globals ****** //
// ***************************** //
// Pins
const int8_t BUZZER_PIN = 14;
// Array for notes (zeros are rests)
const uint8_t BPM = 135; // Beats per minute
const float BPS = BPM/60.0; // Beats per second
const uint8_t NATURAL_GAP = 50; // Natural delay of switching note, ms
// Melody (valid card)
const uint8_t MELODY_LEN = 29; // # of notes (including rests)
const uint16_t MELODY[MELODY_LEN] = {NOTE_GS3, NOTE_AS3, NOTE_CS4, NOTE_AS3, NOTE_F4, NOTE_F4, NOTE_DS4, 0, NOTE_GS3, NOTE_AS3, NOTE_CS4, NOTE_AS3, NOTE_DS4, NOTE_DS4, NOTE_CS4, NOTE_C4, NOTE_AS3, NOTE_GS3, NOTE_AS3, NOTE_CS4, NOTE_AS3, NOTE_CS4, NOTE_DS4, NOTE_C4, NOTE_AS3, NOTE_GS3, NOTE_GS3, NOTE_DS4, NOTE_CS4};
const float MELODY_BEATS[MELODY_LEN] = {0.25, 0.25, 0.25, 0.25, 0.75, 0.75, 1, 0.5, 0.25, 0.25, 0.25, 0.25, 0.75, 0.75, 0.75, 0.25, 0.5, 0.25, 0.25, 0.25, 0.25, 1, 0.5, 0.75, 0.25, 1, 0.5, 1, 1};
// Tritone (invalid card)
const uint8_t TRITONE_LEN = 2; // # of notes (including rests)
const uint16_t TRITONE[TRITONE_LEN] = {NOTE_F5, NOTE_B4};
const float TRITONE_BEATS[TRITONE_LEN] = {0.5, 0.5};
// Accepted RFID Cards
const String VALID_IDS[] = {"Enter IDs here after you test each card", "The ways in which you talk to me", "Suguru", "Have me wishing I were gone", "Satoru"};
// Timing
const u_long TIME_OPEN = 15000; // 15 seconds
const u_long SCAN_DELAY = 5000; // 5 seconds

// ***************************** //
// **** Non-Const. Globals ***** //
// ***************************** //
// Timing
u_long prevMillisDoor = 0;
u_long prevMillisScan = 0;
//u_long currMillis = 0;
// Controls
bool doorOpen = false;
//bool justCheckedRFID = false;

// ***************************** //
// ***** Helper Functions ****** //
// ***************************** //
void playSongWithDelay(const uint16_t* melody, const float* beats, const uint8_t len) {
  // Just for now, we're going to use delay()
  // To calculate the note time, we will use the beats/sec and the number of beats for each note
  for (int i = 0; i < len; i++) {
    int note_i = melody[i];
    float beats_i = beats[i];
    // Goal: Play the note and have a delay
    int noteTime = (beats_i/BPS)*1000; // ms
    if (note_i != 0) {
      tone(BUZZER_PIN, note_i, noteTime);
    }
    delay(noteTime + NATURAL_GAP);
    // Uncomment the line below to troubleshoot melody
    //Serial.println("  Note #" + String(i) + " (" + String(beats_i) + " beats, f = " + String(note_i) + " Hz, t = " + String(noteTime) + " ms)");
  }
}

void grantAccess() {
  // Play pleasant sound, disable actuator
  playSongWithDelay(MELODY, MELODY_BEATS, MELODY_LEN);
  Serial.println("Access granted!");
}

void denyAccess() {
  // Play unpleasant sound, don't disable actuator
  playSongWithDelay(TRITONE, TRITONE_BEATS, TRITONE_LEN);
  Serial.println("Access denied.");
}

void open() {
  // Open the door
  doorOpen = true;
  prevMillisDoor = millis();
  Serial.println("Opening door...");
}

void close() {
  // Close the door
  doorOpen = false;
  Serial.println("Closing door...");
}

bool isTilted() {
  // Figure out how to read from tilt sensor
  return false; // Placeholder so we can verify code works
}

bool verifyRFID(String tag) {
  // Verify RFID card
  bool isValid = false;
  for (String s : VALID_IDS) {
    if (tag == s) {isValid = true; break;} // If the tag matches a valid ID, break with true
  }
  return isValid;
}

// ***************************** //
// ****** Main Functions ******* //
// ***************************** //
void setup() {
  // Begin Serial
  Serial.begin(9600);
  // Begin I-squared-C
	Wire.begin(); 
  // Begin Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  // Make sure RFID is reachable
  if (!(myRfid.begin())) {Serial.println("Could not communicate with Qwiic RFID!");}
}

void loop() {
  Blynk.run();
  String tag = myRfid.getTag(); // Check for a scan
  if ((tag != "") && (millis() - prevMillisScan > SCAN_DELAY)) {
    prevMillisScan = millis();
    Serial.println("RFID detected. Verifying...");
    if (verifyRFID(tag)) {
      grantAccess();
      open();
      //continue; // I don't think "continue;" works in loop()
    } else {
      denyAccess();
      //continue;
    }
  }
  // We need to figure out how to prevent the denial of access temporarily after a successful swipe
  if (isTilted()) {
    denyAccess();
    close();
  }
  if (doorOpen && (millis() - prevMillisDoor > TIME_OPEN)) {
    close();
  }
}
