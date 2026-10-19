#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <WiFi.h>
#include <FirebaseClient.h>
#include "ExampleFunctions.h"

// ===============================
// YOUR DETAILS
// ===============================

#define WIFI_SSID "Ndabezitha"
#define WIFI_PASSWORD "24683579"

#define API_KEY "AIzaSyDq9GDhiHNZwsgFbm9k5hNJffWouZ-wwvk"

#define USER_EMAIL "esp32device@heartrate.local"
#define USER_PASSWORD "1Q2W3e4r"

#define DATABASE_URL "https://heart-rate-monitoring-sy-df300-default-rtdb.firebaseio.com"

// ===============================
// FIREBASE
// ===============================

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

bool testSent = false;


// ===============================
// SETUP
// ===============================

void setup() {

  Serial.begin(115200);
  delay(1000);

  // -------------------------------
  // Wi-Fi
  // -------------------------------

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {

    Serial.print(".");
    delay(300);
  }

  Serial.println();
  Serial.println("Wi-Fi connected!");

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  Serial.println();


  // -------------------------------
  // Firebase
  // -------------------------------

  Firebase.printf(
    "Firebase Client v%s\n",
    FIREBASE_CLIENT_VERSION
  );

  set_ssl_client_insecure_and_buffer(ssl_client);

  Serial.println("Connecting to Firebase...");

  initializeApp(
    aClient,
    app,
    getAuth(user_auth),
    auth_debug_print,
    "authTask"
  );

  app.getApp<RealtimeDatabase>(Database);

  Database.url(DATABASE_URL);
}


// ===============================
// LOOP
// ===============================

void loop() {

  // Required for Firebase authentication
  app.loop();


  // Wait until Firebase login is ready
  if (app.ready() && !testSent) {

    Serial.println();
    Serial.println("Firebase authenticated!");

    Serial.println("Sending test BPM...");


    // Send a test heart-rate value
    bool status = Database.set<int>(
      aClient,
      "/deviceTest/bpm",
      75
    );


    if (status) {

      Serial.println("SUCCESS!");
      Serial.println("Test BPM sent to Firebase.");

    } else {

      Serial.println("FAILED!");

      Firebase.printf(
        "Error: %s | Code: %d\n",
        aClient.lastError().message().c_str(),
        aClient.lastError().code()
      );
    }


    testSent = true;
  }
}