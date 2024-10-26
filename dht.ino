#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <FirebaseESP32.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <EEPROM.h>
#include <vector>

#define DHTPIN 15
#define DHTTYPE DHT11
#define EEPROM_SIZE 512
#define MAX_NETWORKS 5
#define SSID_MAX_LENGTH 32
#define PASSWORD_MAX_LENGTH 64
#define CONFIG_PIN 34

DHT dht(DHTPIN, DHTTYPE);

// Token generation process info
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Firebase credentials
#define API_KEY "AIzaSyBlWsZE8PsCpOWn1vpA4EE9Ye__dP12NQo"
#define DATABASE_URL "https://weather-81834-default-rtdb.asia-southeast1.firebasedatabase.app/"

// Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// NTP Client setup
WiFiUDP udp;
NTPClient timeClient(udp, "pool.ntp.org", 19800, 60000);

WebServer server(80);
String password = "";
bool signupOK = false;

struct WiFiCredential {
  char ssid[SSID_MAX_LENGTH];
  char password[PASSWORD_MAX_LENGTH];
};

std::vector<WiFiCredential> savedNetworks;

// Function prototypes
void handleRoot();
void handleSubmit();
void updateFirebase();
String getAvailableNetworks();
void loadSavedNetworks();
void saveNetwork(const char* ssid, const char* password);
bool connectToSavedNetworks();

void initializeFirebaseAndNTP() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase signup OK");
    signupOK = true;
  } else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Initialize NTP Client
  timeClient.begin();
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  
  pinMode(CONFIG_PIN, INPUT_PULLUP);
  
  EEPROM.begin(EEPROM_SIZE);
  loadSavedNetworks();
  
  // Start Access Point
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("ESP32_Config");
  server.on("/", handleRoot);
  server.on("/submit", handleSubmit);
  server.begin();
  Serial.println("Access Point Started");

  // Try to connect to saved networks
  connectToSavedNetworks();
}

void loop() {
  server.handleClient();
  
  if (WiFi.status() == WL_CONNECTED) {
    updateFirebase();
  } else {
    // If WiFi is disconnected, try to reconnect
    connectToSavedNetworks();
  }
  
  delay(100);
}

void loadSavedNetworks() {
  int address = 0;
  for (int i = 0; i < MAX_NETWORKS; i++) {
    WiFiCredential credential;
    EEPROM.get(address, credential);
    if (strlen(credential.ssid) > 0) {
      savedNetworks.push_back(credential);
    }
    address += sizeof(WiFiCredential);
  }
}

void saveNetwork(const char* ssid, const char* password) {
  WiFiCredential newCredential;
  strncpy(newCredential.ssid, ssid, SSID_MAX_LENGTH);
  strncpy(newCredential.password, password, PASSWORD_MAX_LENGTH);

  if (savedNetworks.size() >= MAX_NETWORKS) {
    savedNetworks.erase(savedNetworks.begin());
  }
  savedNetworks.push_back(newCredential);

  int address = 0;
  for (const auto& cred : savedNetworks) {
    EEPROM.put(address, cred);
    address += sizeof(WiFiCredential);
  }
  EEPROM.commit();
}

bool connectToSavedNetworks() {
  for (const auto& cred : savedNetworks) {
    Serial.printf("Trying to connect to %s\n", cred.ssid);
    WiFi.begin(cred.ssid, cred.password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      attempts++;
      server.handleClient(); // Handle AP clients while attempting to connect
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("Connected to %s\n", cred.ssid);
      initializeFirebaseAndNTP();
      return true;
    }
  }
  return false;
}

void handleRoot() {
  String networks = getAvailableNetworks();
  
  String html = "<form action=\"/submit\" method=\"POST\">";
  html += "Select SSID: <select name=\"ssid\">" + networks + "</select><br>";
  html += "Password: <input type=\"password\" name=\"password\"><br>";
  html += "<input type=\"submit\" value=\"Submit\">";
  html += "</form>";
  server.send(200, "text/html", html);
}

void handleSubmit() {
  String ssid = server.arg("ssid");
  password = server.arg("password");

  // Save the network
  saveNetwork(ssid.c_str(), password.c_str());

  // Connect to the Wi-Fi network
  WiFi.begin(ssid.c_str(), password.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to Wi-Fi: " + ssid);
    server.send(200, "text/html", "Connected! You can close this page.");
    
    // Initialize Firebase
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    if (Firebase.signUp(&config, &auth, "", "")) {
      Serial.println("Firebase signup OK");
      signupOK = true;
    } else {
      Serial.printf("%s\n", config.signer.signupError.message.c_str());
    }
    config.token_status_callback = tokenStatusCallback;
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    // Initialize NTP Client
    timeClient.begin();
  } else {
    server.send(200, "text/html", "Failed to connect. Please try again.");
  }
}

String getAvailableNetworks() {
  String networks = "";
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    networks += "<option value=\"" + WiFi.SSID(i) + "\">" + WiFi.SSID(i) + "</option>";
  }
  WiFi.scanDelete();
  return networks;
}

void updateFirebase() {
  if (!signupOK) return;

  timeClient.update();
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  if (Firebase.ready()) {
    // Update humidity
    if (Firebase.setFloat(fbdo, "/humidity/value", h) &&
        Firebase.setString(fbdo, "/humidity/timestamp", timeClient.getFormattedTime())) {
      Serial.println("Humidity data sent to Firebase successfully");
    } else {
      Serial.println("Failed to send humidity data to Firebase");
      Serial.println("Reason: " + fbdo.errorReason());
    }

    // Update temperature
    if (Firebase.setFloat(fbdo, "/temperature/value", t) &&
        Firebase.setString(fbdo, "/temperature/timestamp", timeClient.getFormattedTime())) {
      Serial.println("Temperature data sent to Firebase successfully");
    } else {
      Serial.println("Failed to send temperature data to Firebase");
      Serial.println("Reason: " + fbdo.errorReason());
    }

    Serial.print("Humidity: ");
    Serial.println(h);
    Serial.print("Temperature: ");
    Serial.println(t);

    delay(100); 
  }
}

void startConfigMode() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
  }
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_Config");
  server.on("/", handleRoot);
  server.on("/submit", handleSubmit);
  server.begin();
  Serial.println("Access Point Started");
  
  while (digitalRead(CONFIG_PIN) == LOW) {
    server.handleClient();
    delay(10);
  }
}
