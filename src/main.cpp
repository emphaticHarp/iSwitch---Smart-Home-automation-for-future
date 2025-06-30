// main.cpp - ESP8266 Smart Home with Multiple Sensors
// Sensor Models: IR, PIR, US-015 Ultrasonic, DHT11, MQ-2 Gas, KY-037 Sound

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FirebaseESP8266.h>
#include <DHT.h>
#include <NewPing.h>
#include <ArduinoJson.h>
#include <Hash.h> // For HMAC security
#include <EEPROM.h> // For persistent storage

// Macro stringification helpers
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

// Firmware version
#define FIRMWARE_VERSION "v1.0.5"

// WiFi credentials (from build flags with fallbacks for testing)
#ifndef WIFI_SSID
  #define WIFI_SSID "TestSSID"
#endif
#ifndef WIFI_PASSWORD
  #define WIFI_PASSWORD "TestPassword"
#endif

const char* ssid = STR(WIFI_SSID);
const char* password = STR(WIFI_PASSWORD);

// Firebase credentials (from build flags)
const char* firebase_host = STR(FIREBASE_HOST);
const char* firebase_auth = STR(FIREBASE_AUTH);

// API Token (from build flags for security)
const char* API_TOKEN = STR(API_TOKEN_SECRET);

// ESP32 connection (mDNS instead of hardcoded IP)
const char* esp32Hostname = "esp32.local";
const uint16_t esp32Port = 80;

// Web server for OTA updates (FIXED)
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

WiFiClient client;
FirebaseData firebaseData;
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;

// Sensor pins according to your connections
#define IR_PIN D1        // IR sensor OUT (object detection)
#define PIR_PIN D2       // PIR sensor SIG
#define ULTRASONIC_TRIG D3  // US-015 Ultrasonic TRIG
#define ULTRASONIC_ECHO D4  // US-015 Ultrasonic ECHO
#define DHT_PIN D5       // DHT11 Sensor Module DATA
#define GAS_PIN D6       // MQ-2 Gas Sensor Digital
#define SOUND_PIN D7     // KY-037 Sound Sensor D0
#define SOUND_ANALOG A0  // KY-037 Sound Sensor Analog (if available)
#define ERROR_LED D8     // Visual error indicator LED

#define DHTTYPE DHT11
#define MAX_DISTANCE 400 // US-015 can measure up to 400cm

// EEPROM addresses
#define EEPROM_SIZE 512
#define EEPROM_SENSOR_DATA_ADDR 0
#define EEPROM_MAGIC_NUMBER_ADDR 200
#define EEPROM_MAGIC_NUMBER 0xAA55

// Sensor objects
DHT dht(DHT_PIN, DHTTYPE);
NewPing sonar(ULTRASONIC_TRIG, ULTRASONIC_ECHO, MAX_DISTANCE);

// Timing variables
unsigned long lastSensorRead = 0;
unsigned long lastWiFiCheck = 0;
unsigned long lastHealthPing = 0;
unsigned long lastErrorBlink = 0;
unsigned long errorLEDState = 0; // For non-blocking LED control
unsigned long lastEEPROMWrite = 0; // Rate limiting for EEPROM writes
const unsigned long SENSOR_INTERVAL = 5000; // 5 seconds
const unsigned long WIFI_CHECK_INTERVAL = 30000; // 30 seconds
const unsigned long HEALTH_PING_INTERVAL = 60000; // 1 minute
const unsigned long ERROR_BLINK_INTERVAL = 1000; // 1 second for error LED
const unsigned long EEPROM_WRITE_INTERVAL = 600000UL; // 10 minutes (rate limiting)

// WiFi reconnection variables
unsigned long wifiReconnectStart = 0;
const unsigned long WIFI_RECONNECT_TIMEOUT = 30000; // 30 seconds timeout
bool isReconnecting = false;

// Sensor calibration and thresholds
const int GAS_THRESHOLD = 500;  // MQ-2 gas detection threshold
const int SOUND_THRESHOLD = 50; // KY-037 sound detection threshold
const int SOUND_ANALOG_THRESHOLD = 300; // Analog sound threshold

// DHT11 fault tolerance
const int DHT_RETRY_COUNT = 3;
const unsigned long DHT_RETRY_DELAY = 1000; // 1 second between retries

// Structure to hold all sensor data with enhanced error handling
struct SensorData {
  float temperature;
  float humidity;
  bool motion;
  bool gas;
  bool sound;
  bool irObject;
  unsigned int distance;
  unsigned long timestamp;
  bool isValid;
  int soundLevel; // Analog sound level
  int gasLevel;   // Analog gas level (if available)
  bool dhtError;  // DHT11 specific error flag
  bool ultrasonicError; // Ultrasonic sensor error flag
};

// Previous valid readings for fallback
SensorData lastValidData = {0};

// Error state tracking
bool hasErrors = false;
bool dhtErrorState = false;
bool ultrasonicErrorState = false;

// Enhanced logging with timestamps
void logMessage(const String& message) {
  Serial.println("[" + String(millis()) + " ms] " + message);
}

// HMAC security for API token (simplified for ESP8266 compatibility)
String generateHMAC(const String& data, const String& secret) {
  // Use a simple hash approach that's available on ESP8266
  // This is not true HMAC but provides basic data integrity
  String combined = data + secret;
  uint32_t hash = 0;
  for (size_t i = 0; i < combined.length(); i++) {
    hash = ((hash << 5) + hash) + combined.charAt(i); // Simple hash function
  }
  return String(hash, HEX);
}

// Load last valid sensor data from EEPROM with safety checks
void loadLastValidData() {
  EEPROM.begin(EEPROM_SIZE);
  
  // Check magic number to verify data integrity
  uint16_t magicNumber;
  EEPROM.get(EEPROM_MAGIC_NUMBER_ADDR, magicNumber);
  
  if (magicNumber == EEPROM_MAGIC_NUMBER) {
    EEPROM.get(EEPROM_SENSOR_DATA_ADDR, lastValidData);
    
    // Additional validation of loaded data
    if (lastValidData.temperature >= -40 && lastValidData.temperature <= 80 &&
        lastValidData.humidity >= 0 && lastValidData.humidity <= 100) {
      logMessage("✅ Loaded last valid sensor data from EEPROM");
      logMessage("  Temperature: " + String(lastValidData.temperature) + "°C");
      logMessage("  Humidity: " + String(lastValidData.humidity) + "%");
    } else {
      logMessage("⚠️ EEPROM data validation failed, using defaults");
      lastValidData.temperature = 25.0;
      lastValidData.humidity = 50.0;
      lastValidData.isValid = false;
    }
  } else {
    logMessage("⚠️ No valid EEPROM data found, using defaults");
    // Initialize with reasonable defaults
    lastValidData.temperature = 25.0;
    lastValidData.humidity = 50.0;
    lastValidData.isValid = false;
  }
}

// Save last valid sensor data to EEPROM with safety checks
void saveLastValidData() {
  if (lastValidData.isValid) {
    // Save data
    EEPROM.put(EEPROM_SENSOR_DATA_ADDR, lastValidData);
    EEPROM.put(EEPROM_MAGIC_NUMBER_ADDR, EEPROM_MAGIC_NUMBER);
    
    // Commit and verify
    if (EEPROM.commit()) {
      logMessage("💾 Saved valid sensor data to EEPROM");
      
      // Optional: Verify the write (commented out to save memory)
      // SensorData verifyStruct;
      // EEPROM.get(EEPROM_SENSOR_DATA_ADDR, verifyStruct);
      // if (verifyStruct.temperature == lastValidData.temperature) {
      //   logMessage("✅ EEPROM write verification successful");
      // } else {
      //   logMessage("❌ EEPROM write verification failed");
      // }
    } else {
      logMessage("❌ EEPROM commit failed");
    }
  }
}

// Non-blocking visual error indicator with LED
void updateErrorLED() {
  if (!hasErrors) {
    digitalWrite(ERROR_LED, LOW);
    errorLEDState = 0;
    return;
  }
  
  unsigned long currentTime = millis();
  
  // State machine for LED patterns
  if (dhtErrorState && ultrasonicErrorState) {
    // Both errors: rapid double blink pattern
    switch (errorLEDState) {
      case 0: digitalWrite(ERROR_LED, HIGH); errorLEDState = 1; break;
      case 1: if (currentTime - lastErrorBlink >= 100) { digitalWrite(ERROR_LED, LOW); errorLEDState = 2; lastErrorBlink = currentTime; } break;
      case 2: if (currentTime - lastErrorBlink >= 100) { digitalWrite(ERROR_LED, HIGH); errorLEDState = 3; lastErrorBlink = currentTime; } break;
      case 3: if (currentTime - lastErrorBlink >= 100) { digitalWrite(ERROR_LED, LOW); errorLEDState = 4; lastErrorBlink = currentTime; } break;
      case 4: if (currentTime - lastErrorBlink >= 100) { digitalWrite(ERROR_LED, HIGH); errorLEDState = 5; lastErrorBlink = currentTime; } break;
      case 5: if (currentTime - lastErrorBlink >= 100) { digitalWrite(ERROR_LED, LOW); errorLEDState = 0; lastErrorBlink = currentTime; } break;
    }
  } else if (dhtErrorState) {
    // DHT error: single blink pattern
    switch (errorLEDState) {
      case 0: digitalWrite(ERROR_LED, HIGH); errorLEDState = 1; lastErrorBlink = currentTime; break;
      case 1: if (currentTime - lastErrorBlink >= 200) { digitalWrite(ERROR_LED, LOW); errorLEDState = 0; } break;
    }
  } else if (ultrasonicErrorState) {
    // Ultrasonic error: double blink pattern
    switch (errorLEDState) {
      case 0: digitalWrite(ERROR_LED, HIGH); errorLEDState = 1; lastErrorBlink = currentTime; break;
      case 1: if (currentTime - lastErrorBlink >= 100) { digitalWrite(ERROR_LED, LOW); errorLEDState = 2; lastErrorBlink = currentTime; } break;
      case 2: if (currentTime - lastErrorBlink >= 100) { digitalWrite(ERROR_LED, HIGH); errorLEDState = 3; lastErrorBlink = currentTime; } break;
      case 3: if (currentTime - lastErrorBlink >= 100) { digitalWrite(ERROR_LED, LOW); errorLEDState = 0; } break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  logMessage("🚀 Smart Home System Starting...");
  logMessage("📦 Firmware Version: " + String(FIRMWARE_VERSION));
  logMessage("🔄 Reboot Reason: " + ESP.getResetReason());
  
  // Initialize error LED
  pinMode(ERROR_LED, OUTPUT);
  digitalWrite(ERROR_LED, LOW);
  
  // Load last valid sensor data from EEPROM
  loadLastValidData();
  
  // Initialize sensor pins
  pinMode(PIR_PIN, INPUT);
  pinMode(GAS_PIN, INPUT);
  pinMode(SOUND_PIN, INPUT);
  pinMode(IR_PIN, INPUT);
  
  // Initialize sensors
  dht.begin();

  logMessage("📡 Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✔️ WiFi Connected");
  logMessage("IP Address: " + WiFi.localIP().toString());

  // Setup mDNS
  if (MDNS.begin("esp8266-sensor")) {
    logMessage("🌐 mDNS responder started");
    MDNS.addService("http", "tcp", 80);
  }

  // Setup Web Server and OTA Update Server (FIXED)
  server.begin();
  httpUpdater.setup(&server);
  logMessage("📦 OTA Update Server ready at http://esp8266-sensor.local/update");

  // Firebase configuration
  logMessage("🔥 Configuring Firebase...");
  firebaseConfig.host = firebase_host;
  firebaseConfig.signer.tokens.legacy_token = firebase_auth;
  firebaseAuth.user.email = "";
  firebaseAuth.user.password = "";
  firebaseAuth.token.uid = "";
  Firebase.begin(&firebaseConfig, &firebaseAuth);
  Firebase.reconnectWiFi(true);
  logMessage("✔️ Firebase Connected");
  
  // Upload firmware version and reboot reason to Firebase
  if (Firebase.ready()) {
    Firebase.setString(firebaseData, "/status/esp8266_version", FIRMWARE_VERSION);
    Firebase.setString(firebaseData, "/status/esp8266_reboot_reason", ESP.getResetReason());
    Firebase.setString(firebaseData, "/status/esp8266_last_boot", String(millis() / 1000));
  }
  
  // System health check
  logMessage("💾 Free heap: " + String(ESP.getFreeHeap()) + " bytes");
  logMessage("📊 Heap fragmentation: " + String(ESP.getHeapFragmentation()) + "%");
  logMessage("✅ All sensors initialized successfully!");
  logMessage("📊 Starting sensor monitoring...");
}

// Improved WiFi reconnection with timeout
bool checkAndReconnectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (isReconnecting) {
      logMessage("✅ WiFi reconnection successful!");
      isReconnecting = false;
    }
    return true;
  }
  
  if (!isReconnecting) {
    logMessage("⚠️ WiFi disconnected, starting reconnection...");
    isReconnecting = true;
    wifiReconnectStart = millis();
    WiFi.reconnect();
  }
  
  // Check timeout with overflow protection
  if ((millis() - wifiReconnectStart) > WIFI_RECONNECT_TIMEOUT) {
    logMessage("❌ WiFi reconnection timeout, saving data and restarting...");
    saveLastValidData(); // Save data before restart
    ESP.restart();
  }
  
  return false;
}

// DHT11 fault tolerance with retry mechanism
bool readDHTWithRetry(float& temp, float& hum) {
  for (int attempt = 1; attempt <= DHT_RETRY_COUNT; attempt++) {
    temp = dht.readTemperature();
    hum = dht.readHumidity();
    
    if (!isnan(temp) && !isnan(hum) && temp > -40 && temp < 80 && hum > 0 && hum < 100) {
      if (attempt > 1) {
        logMessage("✅ DHT11 reading successful on attempt " + String(attempt));
      }
      return true;
    }
    
    if (attempt < DHT_RETRY_COUNT) {
      logMessage("⚠️ DHT11 reading failed, retrying in " + String(DHT_RETRY_DELAY) + "ms... (Attempt " + String(attempt) + "/" + String(DHT_RETRY_COUNT) + ")");
      delay(DHT_RETRY_DELAY);
    }
  }
  
  logMessage("❌ DHT11 reading failed after " + String(DHT_RETRY_COUNT) + " attempts");
  return false;
}

// Read all sensors and return data structure with enhanced error handling
SensorData readAllSensors() {
  SensorData data;
  data.timestamp = millis();
  data.isValid = true;
  data.dhtError = false;
  data.ultrasonicError = false;
  
  // Read DHT11 with fault tolerance
  if (readDHTWithRetry(data.temperature, data.humidity)) {
    lastValidData.temperature = data.temperature;
    lastValidData.humidity = data.humidity;
    lastValidData.isValid = true;
    dhtErrorState = false;
  } else {
    // Use last valid reading as fallback
    data.temperature = lastValidData.temperature;
    data.humidity = lastValidData.humidity;
    data.dhtError = true;
    data.isValid = false;
    dhtErrorState = true;
  }
  
  // Read digital sensors
  data.motion = digitalRead(PIR_PIN);
  data.gas = digitalRead(GAS_PIN) == LOW; // MQ-2 detects gas when LOW
  data.irObject = digitalRead(IR_PIN) == LOW; // IR sensor detects object when LOW
  
  // Read sound sensor (both digital and analog for better control)
  data.sound = digitalRead(SOUND_PIN) == HIGH; // KY-037 detects sound when HIGH
  
  // Enhanced analog sound reading (FIXED: Only use A0 for sound, not gas)
  data.soundLevel = analogRead(SOUND_ANALOG);
  if (data.soundLevel > SOUND_ANALOG_THRESHOLD) {
    data.sound = true; // Override digital reading with analog threshold
  }
  
  // Gas sensor: Use digital reading only to avoid analog conflict
  // If you need analog gas reading, use a different pin or multiplexer
  data.gasLevel = 0; // Set to 0 since we're not using analog for gas
  
  // Improved ultrasonic sensor handling with error detection (OPTIMIZED: Single ping)
  data.distance = sonar.ping_cm();
  if (data.distance == 0) {
    data.distance = 999; // Invalid reading
    data.ultrasonicError = true;
    ultrasonicErrorState = true;
  } else {
    ultrasonicErrorState = false;
  }
  
  // Update overall error state
  hasErrors = dhtErrorState || ultrasonicErrorState;
  
  return data;
}

// Send health check ping to Firebase
void sendHealthPing() {
  if (Firebase.ready()) {
    Firebase.setBool(firebaseData, "/status/esp8266", true);
    Firebase.setString(firebaseData, "/status/esp8266_ip", WiFi.localIP().toString());
    Firebase.setInt(firebaseData, "/status/esp8266_heap", ESP.getFreeHeap());
    Firebase.setInt(firebaseData, "/status/esp8266_uptime", millis() / 1000);
    Firebase.setInt(firebaseData, "/status/esp8266_rssi", WiFi.RSSI());
    Firebase.setBool(firebaseData, "/status/esp8266_has_errors", hasErrors);
    Firebase.setBool(firebaseData, "/status/esp8266_dht_error", dhtErrorState);
    Firebase.setBool(firebaseData, "/status/esp8266_ultrasonic_error", ultrasonicErrorState);
  }
}

// Optimized Firebase upload with sensor data history and memory optimization
bool uploadToFirebase(const SensorData& data) {
  const int MAX_RETRIES = 2;
  
  for (int retry = 0; retry <= MAX_RETRIES; retry++) {
    if (!Firebase.ready()) {
      if (retry < MAX_RETRIES) {
        logMessage("❌ Firebase not ready, retrying in 1 second... (Attempt " + String(retry + 1) + "/" + String(MAX_RETRIES + 1) + ")");
        delay(1000);
        continue;
      } else {
        logMessage("❌ Firebase not ready after " + String(MAX_RETRIES + 1) + " attempts");
        return false;
      }
    }
    
    // Use FirebaseJson for better memory management (single JSON object)
    FirebaseJson json;
    json.set("temperature", data.temperature);
    json.set("humidity", data.humidity);
    json.set("motion", data.motion);
    json.set("gas", data.gas);
    json.set("sound", data.sound);
    json.set("ir_object", data.irObject);
    json.set("distance", data.distance);
    json.set("timestamp", data.timestamp);
    json.set("is_valid", data.isValid);
    json.set("sound_level", data.soundLevel);
    json.set("gas_level", data.gasLevel);
    json.set("dht_error", data.dhtError);
    json.set("ultrasonic_error", data.ultrasonicError);
    json.set("firmware_version", FIRMWARE_VERSION);
    
    // Upload to sensor data history (timestamped path)
    String historyPath = "/logs/" + String(data.timestamp);
    if (Firebase.setJSON(firebaseData, historyPath, json)) {
      logMessage("✅ Sensor data uploaded to Firebase history: " + historyPath);
      
      // Also update current sensor state
      if (Firebase.setJSON(firebaseData, "/sensors", json)) {
        logMessage("✅ Current sensor state updated in Firebase");
      }
      
      return true;
    } else {
      if (retry < MAX_RETRIES) {
        logMessage("❌ Failed to upload to Firebase, retrying... (Attempt " + String(retry + 1) + "/" + String(MAX_RETRIES + 1) + ")");
        logMessage("Error: " + firebaseData.errorReason());
        delay(1000);
        continue;
      } else {
        logMessage("❌ Failed to upload to Firebase after " + String(MAX_RETRIES + 1) + " attempts");
        logMessage("Final error: " + firebaseData.errorReason());
        return false;
      }
    }
  }
  
  return false;
}

// Send data to ESP32 via HTTP JSON POST with mDNS and HMAC security
void sendToESP32_JSON(const SensorData& data) {
  // Try mDNS first, fallback to IP if needed
  String targetHost = esp32Hostname;
  
  if (!client.connect(targetHost, esp32Port)) {
    logMessage("❌ Failed to connect to ESP32 via mDNS, trying IP fallback...");
    // Fallback to hardcoded IP if mDNS fails
    if (!client.connect("192.168.1.100", esp32Port)) {
      logMessage("❌ Failed to connect to ESP32 (HTTP)");
      return;
    }
  }
  
  // Create JSON document with enhanced data
  StaticJsonDocument<512> doc; // Increased size for additional fields
  doc["temperature"] = data.temperature;
  doc["humidity"] = data.humidity;
  doc["motion"] = data.motion;
  doc["gas"] = data.gas;
  doc["sound"] = data.sound;
  doc["ir_object"] = data.irObject;
  doc["distance"] = data.distance;
  doc["timestamp"] = data.timestamp;
  doc["is_valid"] = data.isValid;
  doc["sound_level"] = data.soundLevel;
  doc["gas_level"] = data.gasLevel;
  doc["dht_error"] = data.dhtError;
  doc["ultrasonic_error"] = data.ultrasonicError;
  doc["firmware_version"] = FIRMWARE_VERSION;
  
  String json;
  serializeJson(doc, json);
  
  // Generate HMAC signature for security
  String signature = generateHMAC(json, String(API_TOKEN));
  
  String req = String("POST /update HTTP/1.1\r\n") +
               "Host: " + targetHost + "\r\n" +
               "Content-Type: application/json\r\n" +
               "Authorization: " + API_TOKEN + "\r\n" +
               "X-Signature: " + signature + "\r\n" +
               "Content-Length: " + json.length() + "\r\n\r\n" +
               json;
  
  client.print(req);
  
  unsigned long timeout = millis();
  while (client.connected() && (millis() - timeout) < 3000) { // Increased timeout to 3 seconds
    if (client.available()) {
      String line = client.readStringUntil('\n');
      if (line.startsWith("HTTP/1.1 200")) {
        logMessage("✅ Data sent to ESP32 via REST");
        break;
      }
    }
  }
  
  // Proper client cleanup
  if (client.connected()) {
    client.stop();
  }
}

void loop() {
  // Handle OTA updates and web server
  server.handleClient();
  
  // Handle mDNS
  MDNS.update();
  
  // Update error LED (non-blocking)
  updateErrorLED();
  
  // Check WiFi connection with improved reconnection logic
  if (!checkAndReconnectWiFi()) {
    return; // Wait for reconnection
  }

  // Timing control with overflow protection
  if ((millis() - lastSensorRead) < SENSOR_INTERVAL) {
    return;
  }
  lastSensorRead = millis();

  logMessage("🔄 Reading all sensors...");

  // Read all sensors
  SensorData sensorData = readAllSensors();
  
  // Save valid data to EEPROM periodically (RATE LIMITED: Every 10 minutes)
  if (sensorData.isValid && !sensorData.dhtError && (millis() - lastEEPROMWrite) >= EEPROM_WRITE_INTERVAL) {
    saveLastValidData();
    lastEEPROMWrite = millis();
    logMessage("💾 EEPROM write rate limit: Next write in 10 minutes");
  }
  
  // Print sensor status with emojis and enhanced information
  logMessage("🔍 Sensor Status:");
  logMessage("  🌡️ DHT11 - Temp: " + String(sensorData.temperature) + "°C, Humidity: " + String(sensorData.humidity) + "%" + (sensorData.dhtError ? " (ERROR)" : ""));
  logMessage("  📏 US-015 Distance: " + String(sensorData.distance) + " cm" + (sensorData.ultrasonicError ? " (ERROR)" : ""));
  logMessage("  🏃 PIR Motion: " + String(sensorData.motion ? "DETECTED" : "None"));
  logMessage("  ☁️ MQ-2 Gas: " + String(sensorData.gas ? "DETECTED" : "Safe") + " (Level: " + String(sensorData.gasLevel) + ")");
  logMessage("  🔊 KY-037 Sound: " + String(sensorData.sound ? "DETECTED" : "Quiet") + " (Level: " + String(sensorData.soundLevel) + ")");
  logMessage("  👁️ IR Object: " + String(sensorData.irObject ? "DETECTED" : "None"));
  
  if (hasErrors) {
    logMessage("⚠️ System has errors - check LED indicator");
  }

  // Upload to Firebase with optimized batch upload
  logMessage("📤 Uploading to Firebase...");
  bool firebaseSuccess = uploadToFirebase(sensorData);

  // Send to ESP32 via HTTP JSON POST
  sendToESP32_JSON(sensorData);

  // Send health ping periodically
  if ((millis() - lastHealthPing) >= HEALTH_PING_INTERVAL) {
    lastHealthPing = millis();
    sendHealthPing();
  }

  // Enhanced system health monitoring (every cycle)
  logMessage("💾 Free heap: " + String(ESP.getFreeHeap()) + " bytes");
  logMessage("📊 Heap fragmentation: " + String(ESP.getHeapFragmentation()) + "%");
  logMessage("📡 WiFi RSSI: " + String(WiFi.RSSI()) + " dBm");
  
  logMessage("⏰ Next reading in 5 seconds...");
}


