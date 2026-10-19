#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ====================================================
// PINS
// ====================================================

#define SDA_PIN 8
#define SCL_PIN 9

// ====================================================
// OLED
// ====================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ====================================================
// MAX30102
// ====================================================

MAX30105 sensor;

// ====================================================
// HEART RATE SETTINGS
// ====================================================

// Store the latest 15 valid beats
const byte RATE_SIZE = 15;

byte rates[RATE_SIZE];

byte rateSpot = 0;
byte validReadings = 0;

long lastBeat = 0;

float instantBPM = 0;
float smoothedBPM = 0;

int averageBPM = 0;
int displayBPM = 0;

// ====================================================
// OLED UPDATE SETTINGS
// ====================================================

// Update displayed BPM every 8 seconds
const unsigned long DISPLAY_INTERVAL = 8000;

unsigned long lastDisplayUpdate = 0;


// ====================================================
// SETUP
// ====================================================

void setup() {

  Serial.begin(115200);
  delay(1000);

  // Start I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  // --------------------------------------------------
  // OLED
  // --------------------------------------------------

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {

    Serial.println("OLED initialization failed!");

    while (1);
  }

  display.setTextColor(SSD1306_WHITE);

  // --------------------------------------------------
  // MAX30102
  // --------------------------------------------------

  if (!sensor.begin(Wire, I2C_SPEED_FAST)) {

    Serial.println("MAX30102 not found!");

    while (1);
  }

  sensor.setup();

  sensor.setPulseAmplitudeRed(0x0A);
  sensor.setPulseAmplitudeGreen(0);

  Serial.println("Heart Rate Monitor Started");

  // --------------------------------------------------
  // START SCREEN
  // --------------------------------------------------

  display.clearDisplay();

  display.setTextSize(2);
  display.setCursor(5, 5);
  display.println("HEART RATE");

  display.setTextSize(2);
  display.setCursor(17, 35);
  display.println("READY");

  display.display();

  delay(2000);
}


// ====================================================
// LOOP
// ====================================================

void loop() {

  long irValue = sensor.getIR();


  // ==================================================
  // NO FINGER
  // ==================================================

  if (irValue < 50000) {

    // Reset measurement data
    lastBeat = 0;

    instantBPM = 0;
    smoothedBPM = 0;

    averageBPM = 0;
    displayBPM = 0;

    validReadings = 0;
    rateSpot = 0;

    lastDisplayUpdate = 0;


    // Show no-finger message
    display.clearDisplay();

    display.setTextSize(2);
    display.setCursor(5, 5);
    display.println("HEART RATE");

    display.setTextSize(2);
    display.setCursor(5, 37);
    display.println("FINGER...");

    display.display();

    delay(100);

    return;
  }


  // ==================================================
  // HEARTBEAT DETECTION
  // ==================================================

  if (checkForBeat(irValue)) {

    long now = millis();


    // Ignore first beat because there is no
    // previous beat interval yet
    if (lastBeat != 0) {

      long delta = now - lastBeat;

      instantBPM = 60.0 / (delta / 1000.0);


      // ==================================================
      // FILTER OBVIOUS BAD READINGS
      // ==================================================

      if (instantBPM >= 40 && instantBPM <= 200) {


        // ==================================================
        // STORE BPM
        // ==================================================

        rates[rateSpot] = (byte)instantBPM;

        rateSpot++;

        rateSpot %= RATE_SIZE;


        if (validReadings < RATE_SIZE) {

          validReadings++;
        }


        // ==================================================
        // CALCULATE 15-BEAT MOVING AVERAGE
        // ==================================================

        int total = 0;

        for (byte i = 0; i < validReadings; i++) {

          total += rates[i];
        }

        averageBPM = total / validReadings;


        // ==================================================
        // SMOOTH THE AVERAGE
        // ==================================================

        if (smoothedBPM == 0) {

          smoothedBPM = averageBPM;
        }

        else {

          // Give more weight to previous stable reading
          smoothedBPM =
            (0.80 * smoothedBPM) +
            (0.20 * averageBPM);
        }


        // ==================================================
        // SERIAL MONITOR
        // ==================================================

        // Serial Monitor still shows every valid heartbeat.
        // This does NOT control OLED update speed.

        Serial.print("Instant: ");
        Serial.print(instantBPM, 1);

        Serial.print(" | 15-Beat Average: ");
        Serial.print(averageBPM);

        Serial.print(" | Smoothed: ");
        Serial.println(smoothedBPM, 1);
      }
    }


    lastBeat = now;
  }


  // ==================================================
  // OLED UPDATE - ONLY EVERY 8 SECONDS
  // ==================================================

  unsigned long currentTime = millis();


  // Only display BPM after we have some valid beats
  if (validReadings >= 3) {

    if (currentTime - lastDisplayUpdate >= DISPLAY_INTERVAL) {

      displayBPM = round(smoothedBPM);


      display.clearDisplay();


      // TITLE
      display.setTextSize(2);
      display.setCursor(5, 0);
      display.println("HEART RATE");


      // BPM NUMBER
      display.setTextSize(3);
      display.setCursor(10, 30);
      display.print(displayBPM);


      // BPM LABEL
      display.setTextSize(2);
      display.setCursor(72, 36);
      display.print("BPM");


      display.display();


      // Remember when OLED was updated
      lastDisplayUpdate = currentTime;


      Serial.print(">>> OLED UPDATED: ");
      Serial.print(displayBPM);
      Serial.println(" BPM");
    }
  }
}