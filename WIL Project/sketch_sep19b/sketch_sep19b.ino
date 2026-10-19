#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <WiFi.h>
#include <FirebaseClient.h>
#include "ExampleFunctions.h"
#include <time.h>

#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ====================================================
// WIFI + FIREBASE
// ====================================================

#define WIFI_SSID "Ndabezitha"
#define WIFI_PASSWORD "24683579"

#define API_KEY "AIzaSyDq9GDhiHNZwsgFbm9k5hNJffWouZ-wwvk"

#define USER_EMAIL "esp32device@heartrate.local"
#define USER_PASSWORD "1Q2W3e4r"

#define DATABASE_URL \
"https://heart-rate-monitoring-sy-df300-default-rtdb.firebaseio.com"

// Patient assigned to this ESP32
#define PATIENT_UID "ETERjMZShPM6FBXrv9GSQqiDJn72"

// ====================================================
// HARDWARE
// ====================================================

#define SDA_PIN 8
#define SCL_PIN 9

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);

MAX30105 sensor;

// ====================================================
// FIREBASE OBJECTS
// ====================================================

SSL_CLIENT ssl_client;

using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);

UserAuth user_auth(
  API_KEY,
  USER_EMAIL,
  USER_PASSWORD,
  3000
);

FirebaseApp app;
RealtimeDatabase Database;

// ====================================================
// HEART RATE
// ====================================================

const byte RATE_SIZE = 25;

byte rates[RATE_SIZE];

byte rateSpot = 0;
byte validReadings = 0;

long lastBeat = 0;

float instantBPM = 0;
float smoothedBPM = 0;

int averageBPM = 0;
int displayBPM = 0;

unsigned long readingTimestamp = 0;

// ====================================================
// UPDATE INTERVAL
// ====================================================

const unsigned long UPDATE_INTERVAL = 15000;

unsigned long lastUpdate = 0;

// ====================================================
// TIME / NTP
// ====================================================

const char* ntpServer = "pool.ntp.org";

// South Africa Standard Time: UTC +2
const long gmtOffset_sec = 2 * 60 * 60;

const int daylightOffset_sec = 0;

// Prevent repeated "FINGER..." OLED redraws
bool fingerPreviouslyDetected = false;

// ====================================================
// MONITORING SESSION
// ====================================================

bool sessionActive = false;

unsigned long sessionStartTimestamp = 0;

String sessionId = "";

int sessionBpmTotal = 0;
int sessionBpmCount = 0;

int sessionMinBPM = 0;
int sessionMaxBPM = 0;
// ====================================================
// SETUP
// ====================================================

void setup() {

  Serial.begin(115200);
  delay(1000);

  // --------------------------------------------------
  // I2C
  // --------------------------------------------------

  Wire.begin(SDA_PIN, SCL_PIN);

  // --------------------------------------------------
  // OLED
  // --------------------------------------------------

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDRESS)) {

    Serial.println("OLED initialization failed!");

    while (1);
  }

  display.setTextColor(SSD1306_WHITE);

  display.clearDisplay();

  display.setTextSize(2);
  display.setCursor(5, 5);
  display.println("HEART RATE");

  display.setTextSize(2);
  display.setCursor(17, 35);
  display.println("STARTING");

  display.display();

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

  Serial.println("MAX30102 ready.");

  // --------------------------------------------------
  // WIFI
  // --------------------------------------------------

  Serial.print("Connecting to Wi-Fi");

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  while (WiFi.status() != WL_CONNECTED) {

    Serial.print(".");
    delay(300);
  }

  Serial.println();
  Serial.println("Wi-Fi connected!");

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // --------------------------------------------------
  // NTP TIME
  // --------------------------------------------------

  configTime(
    gmtOffset_sec,
    daylightOffset_sec,
    ntpServer
  );

  Serial.println("NTP time configured.");

  struct tm timeinfo;

  if (getLocalTime(&timeinfo)) {

    Serial.print("Current time: ");
    Serial.println(
      &timeinfo,
      "%Y-%m-%d %H:%M:%S"
    );

  } else {

    Serial.println(
      "Failed to obtain NTP time."
    );
  }

  // --------------------------------------------------
  // FIREBASE
  // --------------------------------------------------

  Firebase.printf(
    "Firebase Client v%s\n",
    FIREBASE_CLIENT_VERSION
  );

  set_ssl_client_insecure_and_buffer(
    ssl_client
  );

  Serial.println(
    "Connecting to Firebase..."
  );

  initializeApp(
    aClient,
    app,
    getAuth(user_auth),
    auth_debug_print,
    "authTask"
  );

  app.getApp<RealtimeDatabase>(
    Database
  );

  Database.url(DATABASE_URL);

  Serial.println("System initialized.");
}

// ====================================================
// LOOP
// ====================================================

void loop() {

  // Firebase needs this continuously
  app.loop();

  // Read optical sensor continuously
  long irValue = sensor.getIR();

  // ==================================================
  // NO FINGER
  // ==================================================

  if (irValue < 50000) {

    if (fingerPreviouslyDetected) {

    Serial.println("Finger removed.");

    if (sessionActive) {

        time_t now;
        time(&now);

        unsigned long sessionEndTimestamp =
            (unsigned long)now;

        Serial.print("Session ended: ");
        Serial.println(sessionEndTimestamp);

      if (app.ready()) {

    String sessionEndPath =
        "/patients/" +
        String(PATIENT_UID) +
        "/sessions/" +
        sessionId +
        "/endTimestamp";

    bool endStatus =
        Database.set<unsigned long>(
            aClient,
            sessionEndPath,
            sessionEndTimestamp
        );

    if (endStatus) {

        Serial.println(
            "Session end saved to Firebase."
        );
    if (sessionBpmCount > 0) {

    int sessionAverageBPM =
        sessionBpmTotal / sessionBpmCount;

    String sessionAveragePath =
        "/patients/" +
        String(PATIENT_UID) +
        "/sessions/" +
        sessionId +
        "/averageBPM";

    bool averageStatus =
        Database.set<int>(
            aClient,
            sessionAveragePath,
            sessionAverageBPM
        );

    if (averageStatus) {

        Serial.println(
            "Session average saved to Firebase."
        );

    } else {

        Serial.println(
            "Session average save FAILED."
        );
    }
}
    } else {

        Serial.println(
            "Session end Firebase save FAILED."
        );
    }
}

        sessionActive = false;
    }
}

    fingerPreviouslyDetected = false;

    // Reset heart-rate calculation
    lastBeat = 0;

    instantBPM = 0;
    smoothedBPM = 0;

    averageBPM = 0;
    displayBPM = 0;

    validReadings = 0;
    rateSpot = 0;

    lastUpdate = millis();

    display.clearDisplay();

    display.setTextSize(2);
    display.setCursor(5, 5);
    display.println("HEART RATE");

    display.setTextSize(2);
    display.setCursor(5, 37);
    display.println("FINGER...");

    display.display();

    delay(50);

    return;
  }

  // ==================================================
  // FINGER DETECTED
  // ==================================================

  if (!fingerPreviouslyDetected) {

    fingerPreviouslyDetected = true;

    // Start a new monitoring session
    if (!sessionActive) {

        time_t now;
        time(&now);

        sessionStartTimestamp = (unsigned long)now;

        sessionId = String(sessionStartTimestamp);

        // Reset statistics for the new session
        sessionBpmTotal = 0;
        sessionBpmCount = 0;

        sessionMinBPM = 0;
        sessionMaxBPM = 0;

        sessionActive = true;

        Serial.print("Session started: ");
        Serial.println(sessionId);

        if (app.ready()) {

    String sessionPath =
        "/patients/" +
        String(PATIENT_UID) +
        "/sessions/" +
        sessionId +
        "/startTimestamp";

    bool sessionStatus =
        Database.set<unsigned long>(
            aClient,
            sessionPath,
            sessionStartTimestamp
        );

    if (sessionStatus) {

        Serial.println(
            "Session start saved to Firebase."
        );

    } else {

        Serial.println(
            "Session start Firebase save FAILED."
        );
    }
}
    }

    lastUpdate = millis();

    Serial.println();
    Serial.println("Finger detected.");
    Serial.println("Measuring...");
  }

  // ==================================================
  // HEARTBEAT DETECTION
  // ==================================================

  if (checkForBeat(irValue)) {

    long now = millis();

    if (lastBeat != 0) {

      long delta = now - lastBeat;

      instantBPM =
        60.0 / (delta / 1000.0);

      // -----------------------------------------------
      // FILTER OBVIOUS BAD DETECTIONS
      // -----------------------------------------------

      if (instantBPM >= 40 &&
          instantBPM <= 200) {

        rates[rateSpot] =
          (byte)instantBPM;

        rateSpot++;

        rateSpot %= RATE_SIZE;

        if (validReadings < RATE_SIZE) {

          validReadings++;
        }

        // ---------------------------------------------
        // MOVING AVERAGE
        // ---------------------------------------------

        int total = 0;

        for (byte i = 0;
             i < validReadings;
             i++) {

          total += rates[i];
        }

        averageBPM =
          total / validReadings;

        // ---------------------------------------------
        // SMOOTHING
        // ---------------------------------------------

        if (smoothedBPM == 0) {

          smoothedBPM =
            averageBPM;

        } else {

          smoothedBPM =
            (0.80 * smoothedBPM) +
            (0.20 * averageBPM);
        }

        Serial.print("Instant: ");
        Serial.print(instantBPM, 1);

        Serial.print(" | Average: ");
        Serial.print(averageBPM);

        Serial.print(" | Smoothed: ");
        Serial.println(smoothedBPM, 1);
      }
    }

    lastBeat = now;
  }

  // ==================================================
  // UPDATE OLED + FIREBASE
  // ==================================================

  unsigned long currentTime =
    millis();

  if (validReadings >= 3 &&
      currentTime - lastUpdate >= UPDATE_INTERVAL) {

    displayBPM =
      round(smoothedBPM);

    // ------------------------------------------------
    // GET REAL TIMESTAMP
    // ------------------------------------------------

    time_t now;
    time(&now);

    readingTimestamp =
      (unsigned long)now;

    Serial.print("Timestamp: ");
    Serial.println(readingTimestamp);

    // ------------------------------------------------
    // CREATE FIREBASE PATHS
    // ------------------------------------------------

    String patientBasePath =
      "/patients/" +
      String(PATIENT_UID);

    String readingPath =
      patientBasePath +
      "/readings/" +
      String(readingTimestamp);

    String currentBpmPath =
      patientBasePath +
      "/current/bpm";

    String currentTimestampPath =
      patientBasePath +
      "/current/timestamp";

    // ------------------------------------------------
    // OLED
    // ------------------------------------------------

    display.clearDisplay();

    display.setTextSize(2);
    display.setCursor(5, 0);
    display.println("HEART RATE");

    display.setTextSize(3);
    display.setCursor(10, 30);
    display.print(displayBPM);

    display.setTextSize(2);
    display.setCursor(72, 36);
    display.print("BPM");

    display.display();

    Serial.println();

    Serial.print(">>> DISPLAY BPM: ");
    Serial.println(displayBPM);

    // ------------------------------------------------
    // FIREBASE
    // ------------------------------------------------

    if (app.ready()) {

      // ----------------------------------------------
      // CURRENT BPM
      // ----------------------------------------------

      Serial.print(
        "Sending BPM to Firebase... "
      );

      bool status =
        Database.set<int>(
          aClient,
          currentBpmPath,
          displayBPM
        );

      if (status) {

        Serial.println("SUCCESS");
        Database.set<unsigned long>(
          aClient,
          currentTimestampPath,
          readingTimestamp
        );

        // --------------------------------------------
        // HISTORICAL BPM
        // --------------------------------------------

        String historyBpmPath =
          readingPath + "/bpm";

        bool historyStatus =
          Database.set<int>(
            aClient,
            historyBpmPath,
            displayBPM
          );

        if (historyStatus) {

          Serial.println(
            "Historical BPM saved."
          );

          // Update statistics for this monitoring session
          if (sessionActive) {

              sessionBpmTotal += displayBPM;
              sessionBpmCount++;

              // First BPM reading in this session
              if (sessionBpmCount == 1) {

                  sessionMinBPM = displayBPM;
                  sessionMaxBPM = displayBPM;

              } else {

                  if (displayBPM < sessionMinBPM) {
                      sessionMinBPM = displayBPM;
                  }

                  if (displayBPM > sessionMaxBPM) {
                      sessionMaxBPM = displayBPM;
                  }
              }
          }
          Serial.print("Session readings: ");
          Serial.print(sessionBpmCount);

          Serial.print(" | Min: ");
          Serial.print(sessionMinBPM);

          Serial.print(" | Max: ");
          Serial.print(sessionMaxBPM);

          Serial.print(" | Average: ");

          if (sessionBpmCount > 0) {
              Serial.println(
                  sessionBpmTotal / sessionBpmCount
              );
          // ------------------------------------------
          // SAVE SESSION MINIMUM BPM
          // ------------------------------------------

          String sessionMinPath =
              "/patients/" +
              String(PATIENT_UID) +
              "/sessions/" +
              sessionId +
              "/minimumBPM";

          bool minStatus =
              Database.set<int>(
                  aClient,
                  sessionMinPath,
                  sessionMinBPM
              );

          if (minStatus) {

              Serial.println(
                  "Session minimum saved to Firebase."
              );

          } else {

              Serial.println(
                  "Session minimum save FAILED."
              );
          }


          // ------------------------------------------
          // SAVE SESSION MAXIMUM BPM
          // ------------------------------------------

          String sessionMaxPath =
              "/patients/" +
              String(PATIENT_UID) +
              "/sessions/" +
              sessionId +
              "/maximumBPM";

          bool maxStatus =
              Database.set<int>(
                  aClient,
                  sessionMaxPath,
                  sessionMaxBPM
              );

          if (maxStatus) {

              Serial.println(
                  "Session maximum saved to Firebase."
              );

          } else {

              Serial.println(
                  "Session maximum save FAILED."
              );
          }

          }

          // ------------------------------------------
          // HISTORICAL TIMESTAMP
          // ------------------------------------------

          String timestampPath =
            readingPath +
            "/timestamp";

          bool timestampStatus =
            Database.set<unsigned long>(
              aClient,
              timestampPath,
              readingTimestamp
            );

          if (timestampStatus) {

            Serial.println(
              "Timestamp saved."
            );

            String sessionIdPath =
            readingPath +
            "/sessionId";

        bool sessionIdStatus =
            Database.set<String>(
                aClient,
                sessionIdPath,
                sessionId
            );

        if (sessionIdStatus) {

            Serial.println(
                "Session ID saved with reading."
            );

        } else {

            Serial.println(
                "Session ID save FAILED."
            );
        }

          } else {

            Serial.println(
              "Timestamp FAILED."
            );
          }

        } else {

          Serial.println(
            "Historical BPM FAILED."
          );
        }

      } else {

        Serial.println("FAILED");

        Firebase.printf(
          "Firebase error: %s | Code: %d\n",
          aClient.lastError()
            .message()
            .c_str(),
          aClient.lastError()
            .code()
        );
      }

    } else {

      Serial.println(
        "Firebase not ready - BPM not sent."
      );
    }

    lastUpdate =
      currentTime;
  }
}