#include <WiFi.h>
#include <WebServer.h>

// WiFi details
const char* ssid = "SmartHouseGRP9";
const char* password = "24683579";

// Relay pins
#define KITCHEN_RELAY_PIN 23
#define LIVING_RELAY_PIN 22
#define PLUG_RELAY_PIN 21

// Active HIGH relay
#define RELAY_ON LOW
#define RELAY_OFF HIGH

bool kitchenState = false;
bool livingState = false;
bool plugState = false;

int activeUsers = 0;
const int maxUsers = 5;

WebServer server(80);

void sendCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void handleRoot() {
  sendCORS();
  server.send(200, "text/plain", "ESP32 Smart House Relay Server is Running");
}

void sendStatus() {
  sendCORS();

  String json = "{";
  json += "\"light1\":" + String(kitchenState ? "true" : "false") + ",";
  json += "\"light2\":" + String(livingState ? "true" : "false") + ",";
  json += "\"plug1\":" + String(plugState ? "true" : "false") + ",";
  json += "\"activeUsers\":" + String(activeUsers) + ",";
  json += "\"maxUsers\":" + String(maxUsers);
  json += "}";

  server.send(200, "application/json", json);
}

void userLogin() {
  if (activeUsers < maxUsers) {
    activeUsers++;
  }

  sendStatus();
}

void userLogout() {
  if (activeUsers > 0) {
    activeUsers--;
  }

  sendStatus();
}

void kitchenOn() {
  kitchenState = true;
  digitalWrite(KITCHEN_RELAY_PIN, RELAY_ON);
  Serial.println("Kitchen Light ON");
  sendCORS();
  server.send(200, "text/plain", "Kitchen Light ON");
}

void kitchenOff() {
  kitchenState = false;
  digitalWrite(KITCHEN_RELAY_PIN, RELAY_OFF);
  Serial.println("Kitchen Light OFF");
  sendCORS();
  server.send(200, "text/plain", "Kitchen Light OFF");
}

void livingRoomOn() {
  livingState = true;
  digitalWrite(LIVING_RELAY_PIN, RELAY_ON);
  Serial.println("Living Room Light ON");
  sendCORS();
  server.send(200, "text/plain", "Living Room Light ON");
}

void livingRoomOff() {
  livingState = false;
  digitalWrite(LIVING_RELAY_PIN, RELAY_OFF);
  Serial.println("Living Room Light OFF");
  sendCORS();
  server.send(200, "text/plain", "Living Room Light OFF");
}

void plugOn() {
  plugState = true;
  digitalWrite(PLUG_RELAY_PIN, RELAY_ON);
  Serial.println("Plug ON");
  sendCORS();
  server.send(200, "text/plain", "Plug ON");
}

void plugOff() {
  plugState = false;
  digitalWrite(PLUG_RELAY_PIN, RELAY_OFF);
  Serial.println("Plug OFF");
  sendCORS();
  server.send(200, "text/plain", "Plug OFF");
}

void allOn() {
  kitchenState = true;
  livingState = true;
  plugState = true;

  digitalWrite(KITCHEN_RELAY_PIN, RELAY_ON);
  digitalWrite(LIVING_RELAY_PIN, RELAY_ON);
  digitalWrite(PLUG_RELAY_PIN, RELAY_ON);

  Serial.println("All Devices ON");
  sendCORS();
  server.send(200, "text/plain", "All Devices ON");
}

void allOff() {
  kitchenState = false;
  livingState = false;
  plugState = false;

  digitalWrite(KITCHEN_RELAY_PIN, RELAY_OFF);
  digitalWrite(LIVING_RELAY_PIN, RELAY_OFF);
  digitalWrite(PLUG_RELAY_PIN, RELAY_OFF);

  Serial.println("All Devices OFF");
  sendCORS();
  server.send(200, "text/plain", "All Devices OFF");
}

void setup() {
  Serial.begin(115200);

  pinMode(KITCHEN_RELAY_PIN, OUTPUT);
  pinMode(LIVING_RELAY_PIN, OUTPUT);
  pinMode(PLUG_RELAY_PIN, OUTPUT);

  digitalWrite(KITCHEN_RELAY_PIN, RELAY_OFF);
  digitalWrite(LIVING_RELAY_PIN, RELAY_OFF);
  digitalWrite(PLUG_RELAY_PIN, RELAY_OFF);

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi Connected!");
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);

  server.on("/status", sendStatus);
  server.on("/user/login", userLogin);
  server.on("/user/logout", userLogout);

  server.on("/light1/on", kitchenOn);
  server.on("/light1/off", kitchenOff);

  server.on("/light2/on", livingRoomOn);
  server.on("/light2/off", livingRoomOff);

  server.on("/plug1/on", plugOn);
  server.on("/plug1/off", plugOff);

  server.on("/all/on", allOn);
  server.on("/all/off", allOff);

  server.begin();

  Serial.println("Smart House Relay Web Server Started");
}

void loop() {
  server.handleClient();
}