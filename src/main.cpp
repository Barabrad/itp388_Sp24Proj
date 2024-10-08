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
const uint8_t BUZZER_PIN = 13; // Adjust values based on pin mapping
const uint8_t MAGSWITCH_PIN = 32;
const uint8_t TILT_IN_PIN = 34; // Tilt sensor on the inside
const uint8_t TILT_OUT_PIN = 39; // Tilt sensor on the outside (can override locking)
const uint8_t SOLENOID_PIN = 15;
const uint8_t SS_PIN = 17; // SDA for RFID Reader
const uint8_t RST_PIN = 16; // Reset for RFID Reader
const uint8_t BAT_PIN = A13; // https://learn.adafruit.com/adafruit-huzzah32-esp32-feather/power-management#measuring-battery-3122383
const uint8_t BUT_VAL_PIN = 21; // To simulate valid RFID swipe
const uint8_t BUT_INVAL_PIN = 23; // To simulate invalid RFID swipe
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
// Timing
const u_long TIME_OPEN = 15000; // 15 seconds
const u_long SCAN_DELAY = 5000; // 5 seconds
const u_long WARNING_DELAY = 10000; // 10 seconds
const u_long TIME_ACTUATE = 2000; // 2 seconds (to push the lock to close)
const u_long BAT_UPDATE = 60000; // 60 seconds
const u_long TIME_TBST = 5000; // 5 seconds for each troubleshoot packet

// ***************************** //
// **** Non-Const. Globals ***** //
// ***************************** //
// Timing
u_long prevMillisDoor = 0;
u_long prevMillisScan = 0;
u_long prevMillisWarning = 0;
u_long prevMillisSound = 0;
u_long prevMillisAct = 0;
u_long prevMillisBat = 0;
u_long prevMillisTbst = 0; // For troubleshooting tilt
// Controls
const bool DO_TBST = false; // Enable/disable troubleshooting packets
const bool USE_BUT_FOR_CARD = false; // Enable buttons for testing if RDID is unavailable
bool doorOpen = false;
bool prevActuated = false;
bool actuated = false;
//bool justCheckedRFID = false;
// Other
uint8_t tilt_position = 0;
float batVoltage = -1;
uint8_t overrideVal = 0;

// Initialize RFID reader
MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance.
String rfid = "";
const String VALID_CARDS[] = {"61 C5 7A 10"}; // The card that came with the reader
// RFID tag (for invalid demo): 03 E2 0C 50

// Unorganized variables for non-blocking speaker code
uint8_t noteIndex = 0;
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

// Plays a sound given inputs of the notes array, duration (beats) array, and number of notes
void playSound(const uint16_t* melody, const float* beats, const int num_notes) {
  // For every note, play the note for its duration (pausing would make the code much more complex)
  if (millis() - prevMillisSound >= pauseBetweenNotes) {
    // Resets noteIndex and boolean variables if play_alarm is true or the last note has been played, without changing play_alarm itself so that the alarm can be played continuously until it is deactivated
    if (noteIndex >= num_notes) {
      noteIndex = 0;
      grant_access = false;
      deny_access = false;
      warning = false;
      pauseBetweenNotes = 0; // Resets pauseBetweenNotes for the next sound that is played
      noTone(BUZZER_PIN); // Stops buzzer
    }
    else {
      prevMillisSound = millis();
      int note_i = melody[noteIndex];
      float beats_i = beats[noteIndex];
      int noteTime = (beats_i/BPS)*1000; // ms
      // Plays the note, but with no duration (since duration is blocking)
      if (note_i == 0) {noTone(BUZZER_PIN);}
      else {tone(BUZZER_PIN, note_i);}
      pauseBetweenNotes = noteTime; // Gap in notes would be hard to do without blocking
      // Increments noteIndex after the note is played
      //Serial.println(String(noteIndex) + ": " + String(note_i) + " Hz");
      noteIndex = noteIndex + 1;
    }
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

void disableAct() {
  // Disable the actuator
  digitalWrite(SOLENOID_PIN, LOW);
  actuated = false;
}

void open() {
  // Open the door
  disableAct();
  doorOpen = true;
  prevMillisDoor = millis();
  Serial.println("Opening door...");
}

void close() {
  // Close the door
  digitalWrite(SOLENOID_PIN, HIGH);
  doorOpen = false;
  actuated = true;
  prevMillisAct = millis();
  Serial.println("Closing door...");
}

bool isTilted(const uint8_t TILT_PIN) {
  // Figure out how to read from tilt sensor
  tilt_position = map(analogRead(TILT_PIN), 0, 4095, 0, 342);
  // Serial.println("Tilt angle: " + String(tilt_position));
  // delay(500);
  //Serial.println(tilt_position); // For testing
  if (tilt_position >= 30 && tilt_position <= 120) { // Adjust values during testing
    Serial.println("Doorknob tilted.");
    return true;
  }
  return false; // Placeholder so we can verify code works
}

bool verifyRFID(String tag) {
  // Verify RFID card
  for (int i = 0; i < sizeof(VALID_CARDS)/sizeof(VALID_CARDS[0]); i++) {
    if (tag == VALID_CARDS[i]) {
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

BLYNK_WRITE(V2) {
  //prevLockVal = overrideVal;
  overrideVal = param.asInt(); // assigning incoming value from pin V2 to a variable
}

// ***************************** //
// ****** Main Functions ******* //
// ***************************** //
void setup() {
  Serial.begin(9600);
	Wire.begin(); 
  // Begin Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  delay(2000);
  Blynk.virtualWrite(V1, 0); // Set initial actuation value to false
  Serial.println("Blynk initialized.");

  // Initialize RFID reader
  SPI.begin(); // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522
  delay(1000); // Optional delay. Some board do need more time after init to be ready, see Readme
  mfrc522.PCD_DumpVersionToSerial(); // Show details of PCD - MFRC522 Card Reader details

  Serial.println("RFID initialized.");

  // Set up pins
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(MAGSWITCH_PIN, INPUT);
  pinMode(TILT_IN_PIN, INPUT);
  pinMode(TILT_OUT_PIN, INPUT);
  pinMode(SOLENOID_PIN, OUTPUT);
  pinMode(SS_PIN, OUTPUT);
  pinMode(RST_PIN, OUTPUT);
  pinMode(BAT_PIN, INPUT);
  pinMode(BUT_VAL_PIN, INPUT);
  pinMode(BUT_INVAL_PIN, INPUT);

  Serial.println("Setup complete.");
}

void loop() {
  Blynk.run();
  selectSound();
  prevActuated = actuated;
  // Check tilt values
  if ((millis() - prevMillisTbst > TIME_TBST) && DO_TBST) {
    prevMillisTbst = millis();
    int outer = map(analogRead(TILT_OUT_PIN), 0, 4095, 0, 342);
    int inner = map(analogRead(TILT_IN_PIN), 0, 4095, 0, 342);
    int valid = digitalRead(BUT_VAL_PIN);
    int invalid = digitalRead(BUT_INVAL_PIN);
    Serial.println("(O,I) = (" + String(outer) + "," + String(inner) + ")");
    Serial.println("(V,IV) = (" + String(valid) + "," + String(invalid) + ")");
  }
  // Measure battery voltage
  if ((millis() - prevMillisBat > BAT_UPDATE) || (batVoltage == -1)) {
    prevMillisBat = millis();
    // https://forums.adafruit.com/viewtopic.php?t=196384
    float halfVolt = analogRead(BAT_PIN);
    batVoltage = (halfVolt*2)*(3.3/4096); // 3.3 reference voltage; 12-bit ADC -> 4096 values
    Serial.println("Battery: " + String(batVoltage) + " V");
    Blynk.virtualWrite(V0, batVoltage);
  }
  // Checks actuator (can't keep on for long or else it burns out)
  if (actuated && (millis() - prevMillisAct > TIME_ACTUATE)) {
    prevMillisAct = millis();
    disableAct();
    Serial.println("Actuation time ended.");
  }
  // Checks RFID
  if (!USE_BUT_FOR_CARD) {rfid = readRFID();}
  else {
    if (digitalRead(BUT_VAL_PIN) == HIGH) {rfid = "8B A0 F0 13";}
    else if (digitalRead(BUT_INVAL_PIN) == HIGH) {rfid = "Nuh uh";}
    else {rfid = "";}
  }
  if (overrideVal == 1 && !doorOpen) {
    Serial.println("Access granted!");
    grant_access = true;
    //grantAccess();
    open();
  } else if ((rfid != "") && (millis() - prevMillisScan > SCAN_DELAY) && !doorOpen) {
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
  } else { // Everything after only runs if RFID check does not go through
    if (doorOpen && (millis() - prevMillisDoor > TIME_OPEN)) { // Time to close door
      if (digitalRead(MAGSWITCH_PIN) == LOW) { // Mag switch closed
        close();
        //doorOpen = false; // Redundant, since close() changes that variable
      } else if (digitalRead(MAGSWITCH_PIN) == HIGH && millis() - prevMillisWarning > WARNING_DELAY) { // Mag switch open
        Serial.println("Warning.");
        warning = true;
        //warning();
        prevMillisWarning = millis();
      }
    } else if (!doorOpen && isTilted(TILT_IN_PIN)) { // Door closed and tilted from inside, but without RFID read
      close();
      Serial.println("Warning: Attempted opening from inside");
      warning = true;
      //warning();
    } else if (!doorOpen && isTilted(TILT_OUT_PIN)) { // Door closed and tilted from outside, but without RFID read
      open();
      Serial.println("Opened from outside.");
      grant_access = true;
      //grantAccess();
    }
  }
  if (actuated != prevActuated) {Blynk.virtualWrite(V1, actuated);}
}