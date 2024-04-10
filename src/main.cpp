#define BLYNK_TEMPLATE_ID "[REDACTED]"
#define BLYNK_TEMPLATE_NAME "388 Project"
#define BLYNK_AUTH_TOKEN "[REDACTED]"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include "pitches.h"

// Your WiFi credentials.
// Set password to "" for open networks.
// https://examples.blynk.cc/?board=ESP32&shield=ESP32%20WiFi&example=GettingStarted%2FBlynkBlink
char ssid[] = "YourNetworkName";
char pass[] = "YourPassword";

// Pins
const int8_t BUZZER_PIN = 14;

// Constants
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


// put function declarations here:
void playSongWithDelay(const uint16_t*, const float*, const uint8_t);
void grantAccess();
void denyAccess();

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
}

void loop() {
  // put your main code here, to run repeatedly:
  Blynk.run();
}

// Helper functions
void playSongWithDelay(const int* melody, const float* beats, const int len) {
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
}

void denyAccess() {
  // Play unpleasant sound, don't disable actuator
  playSongWithDelay(TRITONE, TRITONE_BEATS, TRITONE_LEN);
}
