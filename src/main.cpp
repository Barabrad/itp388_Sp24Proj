/*
Todo:
-uncomment/implement Blynk code
*/

// WiFi and Blynk Credentials
#include "secrets_def.h" // Default
//#include "secrets.h" // Specific (user must make this file)

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include "pitches.h"
#include <Wire.h> 
#include <MFRC522.h>
#include <SPI.h>

typedef unsigned long u_long;

// Your WiFi credentials.
// Set password to "" for open networks.
// https://examples.blynk.cc/?board=ESP32&shield=ESP32%20WiFi&example=GettingStarted%2FBlynkBlink
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;


// ***************************** //
// ***** Constant Globals ****** //
// ***************************** //
// Pins
const int8_t BUZZER_PIN = 14; //adjust values based on pin mapping
const int8_t MAGSWITCH_PIN = 15;
const int8_t TILT_PIN = 34;
const int8_t SOLENOID_PIN = 23;
const int8_t SS_PIN = 33;
const int8_t RST_PIN = 32;
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
// Warning
const uint8_t WARNING_LEN = 5; // # of notes (including rests)
const uint16_t WARNING[WARNING_LEN] = {NOTE_G5, NOTE_F5, NOTE_D5, NOTE_C5, NOTE_A4};
const float WARNING_BEATS[WARNING_LEN] = {0.5, 0.5, 0.5, 0.5, 1};
// Accepted RFID Cards
const String VALID_IDS[] = {"Enter IDs here after you test each card", "The ways in which you talk to me", "Suguru", "Have me wishing I were gone", "Satoru"};
// Timing
const u_long TIME_OPEN = 15000; // 15 seconds
const u_long SCAN_DELAY = 5000; // 5 seconds
const u_long WARNING_DELAY = 10000; // 10 seconds

// ***************************** //
// **** Non-Const. Globals ***** //
// ***************************** //
// Timing
u_long prevMillisDoor = 0;
u_long prevMillisScan = 0;
u_long prevMillisWarning = 0;
u_long prevMillisSound = 0;
// Controls
bool doorOpen = false;
//bool justCheckedRFID = false;
// Other
uint8_t tilt_position = 0;

// Initialize RFID reader
MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance.
String rfid = "";
const String cards[] = {"8B A0 F0 13", "EA D4 00 80"};

//Unorganized variables for non-blocking speaker code
int8_t noteIndex = 0;
bool grant_access = false;
bool deny_access = false;
bool warning = false;
u_long pauseBetweenNotes = 0;

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

void selectSound() {
  if (grant_access) {
    playSound(MELODY, MELODY_BEATS, MELODY_LEN);
  } else if (deny_access) {
    playSound(TRITONE, TRITONE_BEATS, TRITONE_LEN);
  } else if (warning) {
    playSound(WARNING, WARNING_BEATS, WARNING_LEN);
  } //else no sound is played
}

//plays a sound given inputs of the notes array, duration array, and number of notes
void playSound(const uint16_t* notes, const float* duration, const int num_notes) {
  //for every note, play the note for 1 * the note duration and pause for 0.3 * the note duration
  if (millis() - prevMillisSound >= pauseBetweenNotes) {
    prevMillisSound = millis();
    int noteTime = 1000 / duration[noteIndex];
    //plays the note
    tone(BUZZER_PIN, notes[noteIndex], noteTime);
    pauseBetweenNotes = noteTime * 1.3;
    //increments noteIndex after the note is played
    noteIndex = noteIndex + 1;
    //resets noteIndex and boolean variables if play_alarm is true or the last note has been played, without changing play_alarm itself so that the alarm can be played continuously until it is deactivated
    if (noteIndex >= num_notes) {
      noteIndex = 0;
      grant_access = false;
      deny_access = false;
      warning = false;
      //resets pauseBetweenNotes for the next sound that is played
      pauseBetweenNotes = 0;
      noTone(BUZZER_PIN);
    }
  }
}

// void grantAccess() {
//   // Play pleasant sound, disable actuator
//   playSongWithDelay(MELODY, MELODY_BEATS, MELODY_LEN);
//   Serial.println("Access granted!");
// }

// void denyAccess() {
//   // Play unpleasant sound, don't disable actuator
//   playSongWithDelay(TRITONE, TRITONE_BEATS, TRITONE_LEN);
//   Serial.println("Access denied.");
// }

// void warning() {
//   // Play warning sound, don't disable actuator
//   playSongWithDelay(WARNING, WARNING_BEATS, WARNING_LEN);
//   Serial.println("Warning.");
// }

void open() {
  // Open the door
  digitalWrite(SOLENOID_PIN, HIGH);
  doorOpen = true;
  prevMillisDoor = millis();
  Serial.println("Opening door...");
}

void close() {
  // Close the door
  digitalWrite(SOLENOID_PIN, LOW);
  doorOpen = false;
  Serial.println("Closing door...");
}

bool isTilted() {
  // Figure out how to read from tilt sensor
  tilt_position = map(analogRead(TILT_PIN), 0, 4095, 0, 342);
  //Serial.println(tilt_position); //for testing
  if (tilt_position >= 30 && tilt_position <= 120) { //adjust values during testing
    Serial.println("Doorknob tilted.");
    return true;
  }
  return false; // Placeholder so we can verify code works
}

bool verifyRFID(String tag) {
  // Verify RFID card
  for (int i = 0; i < sizeof(cards)/sizeof(cards[0]); i++) {
    if (tag == cards[i]) {
      return true;
    }
  }
  return false;
}

String readRFID() {
  String scannedCard = "";
  if (mfrc522.PICC_IsNewCardPresent() == true) {
    if (mfrc522.PICC_ReadCardSerial() == true) {
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        if (mfrc522.uid.uidByte[i] < 0x10) {
          scannedCard = scannedCard + " 0";
        } else {
          scannedCard = scannedCard + " ";
        }
        scannedCard = scannedCard + String(mfrc522.uid.uidByte[i], HEX);
      }
      scannedCard.toUpperCase();
      scannedCard.trim();
      Serial.println("Scanned card: " + scannedCard);
    }
  }
  return scannedCard;
}

// ***************************** //
// ****** Main Functions ******* //
// ***************************** //
void setup() {
  Serial.begin(9600);
	Wire.begin(); 
  // Begin Blynk
  //Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  delay(2000);
  Serial.println("Checkpoint 2 complete.");

  // Initialize RFID reader
  SPI.begin(); // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522
  delay(4); // Optional delay. Some board do need more time after init to be ready, see Readme
  mfrc522.PCD_DumpVersionToSerial(); // Show details of PCD - MFRC522 Card Reader details

  Serial.println("RFID initialized.");

  // Set up pins
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(MAGSWITCH_PIN, INPUT);
  pinMode(TILT_PIN, INPUT);
  pinMode(SOLENOID_PIN, OUTPUT);
  pinMode(SS_PIN, OUTPUT);
  pinMode(RST_PIN, OUTPUT);

  Serial.println("Setup complete.");
}

void loop() {
  //Blynk.run();
  selectSound();
  //checks RFID
  rfid = readRFID();
  if (rfid != "" && (millis() - prevMillisScan > SCAN_DELAY) && !doorOpen) {
    prevMillisScan = millis();
    if (verifyRFID(rfid)) {
      Serial.println("Access granted!");
      grant_access = true;
      //grantAccess();
      open();
    } else {
      Serial.println("Access denied.");
      deny_access = true;
      //denyAccess();
    }
  } else { //everything after only runs if RFID check does not go through
    if (doorOpen && (millis() - prevMillisDoor > TIME_OPEN)) { //time to close door
      if (digitalRead(MAGSWITCH_PIN) == LOW) { //mag switch closed
        close();
        doorOpen = false;
      } else if (digitalRead(MAGSWITCH_PIN) == HIGH && millis() - prevMillisWarning > WARNING_DELAY) { //mag switch open
        Serial.println("Warning.");
        warning = true;
        //warning();
        prevMillisWarning = millis();
      }
    } else if (!doorOpen && isTilted()) { //door open and tilted
      close();
      Serial.println("Warning.");
      warning = true;
      //warning();
    }
  }
}