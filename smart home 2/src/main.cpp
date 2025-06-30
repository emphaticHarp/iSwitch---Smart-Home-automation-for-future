// ESP32 Smart Home Controller (Manual + Auto via Arduino IoT Cloud)
// Controls Fan, Exhaust, Room Light, Main Light based on cloud input + sensor data
// Web Interface for real-time control and monitoring

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>

// Function prototypes
void controlActuators(float temp, int gas, int sound, int motion, int ir);
void setRGB(int r, int g, int b);
String getStatusJSON();
void handleRoot();
void handleStatus();
void handleToggle();
void handleUpdate();
void saveRelayStates();

// Device Identity (for reference/documentation)
const char* DEVICE_ID = "4c20eeb6-c003-4749-9fd6-e3d8c22c92ad";
const char* DEVICE_SECRET_KEY = "oKDgnNIBqOaWgrT?Xy?AFpn8e";

// WiFi credentials
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
const char* ssid = STR(WIFI_SSID);
const char* password = STR(WIFI_PASSWORD);

// Server
WiFiServer server(5000);
WiFiClient client;
WebServer webServer(80);

// Relay Pins
#define RELAY_FAN 19
#define RELAY_EXHAUST 21
#define RELAY_ROOM_LIGHT 22
#define RELAY_MAIN_LIGHT 25
#define BUZZER 2

// RGB LED Pins
#define LED_R 15
#define LED_G 4
#define LED_B 16

// PWM Channels
#define PWM_CHANNEL_R 0
#define PWM_CHANNEL_G 1
#define PWM_CHANNEL_B 2
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8

// Thresholds
const float TEMP_THRESHOLD = 30.0;

// State Variables
bool fanState = false;
bool exhaustState = false;
bool buzzerState = false;
bool roomLightState = false;
bool mainLightState = false;

// Manual Controls from Arduino IoT Cloud (simulate cloud updates)
bool iotFan = false;
bool iotExhaust = false;
bool iotRoomLight = false;
bool iotMainLight = false;

// Sensor Data
float currentTemp = 0.0;
float currentHumidity = 0.0;
bool motionDetected = false;
bool gasDetected = false;
bool soundDetected = false;
bool irObjectDetected = false;
unsigned int distance = 0;

// Status Variables
bool wifiConnected = false;
bool firebaseConnected = false;
bool iotCloudConnected = false;

// Debounce Timing
unsigned long lastActuatorUpdate = 0;
const unsigned long ACTUATOR_DEBOUNCE = 1000;

// Persistent storage
Preferences preferences;

// Auto light timeout
unsigned long lastMotionTime = 0;
const unsigned long MAIN_LIGHT_TIMEOUT = 5 * 60 * 1000; // 5 minutes

// Security token for REST API
const char* API_TOKEN = "changeme123";

// Buzzer pulse
unsigned long buzzerPulseStart = 0;
const unsigned long BUZZER_PULSE_DURATION = 500; // ms

void setup() {
  Serial.begin(115200);
  Serial.println("🔧 Starting Smart Home Controller...");

  // PWM Setup
  ledcSetup(PWM_CHANNEL_R, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_G, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(PWM_CHANNEL_B, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(LED_R, PWM_CHANNEL_R);
  ledcAttachPin(LED_G, PWM_CHANNEL_G);
  ledcAttachPin(LED_B, PWM_CHANNEL_B);

  // Relay & Buzzer
  pinMode(RELAY_FAN, OUTPUT);
  pinMode(RELAY_EXHAUST, OUTPUT);
  pinMode(RELAY_ROOM_LIGHT, OUTPUT);
  pinMode(RELAY_MAIN_LIGHT, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(RELAY_FAN, LOW);
  digitalWrite(RELAY_EXHAUST, LOW);
  digitalWrite(RELAY_ROOM_LIGHT, LOW);
  digitalWrite(RELAY_MAIN_LIGHT, LOW);
  digitalWrite(BUZZER, LOW);
  setRGB(0, 0, 0);

  // WiFi Setup
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500); Serial.print("."); attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected: " + WiFi.localIP().toString());
    wifiConnected = true;
  } else {
    Serial.println("\nWiFi failed. Restarting...");
    ESP.restart();
  }

  // Web Server Routes
  webServer.on("/", handleRoot);
  webServer.on("/status", handleStatus);
  webServer.on("/toggle/fan", handleToggle);
  webServer.on("/toggle/exhaust", handleToggle);
  webServer.on("/toggle/room-light", handleToggle);
  webServer.on("/toggle/main-light", handleToggle);
  webServer.on("/update", HTTP_POST, handleUpdate);

  webServer.begin();
  Serial.println("🌐 Web Server started on port 80");

  server.begin();
  Serial.println("📡 TCP Server Ready on port 5000");
  
  // Simulate IoT Cloud connection
  iotCloudConnected = true;
  firebaseConnected = true;

  preferences.begin("relays", false);
  // Restore relay/manual states
  iotFan = preferences.getBool("iotFan", false);
  iotExhaust = preferences.getBool("iotExhaust", false);
  iotRoomLight = preferences.getBool("iotRoomLight", false);
  iotMainLight = preferences.getBool("iotMainLight", false);
  fanState = preferences.getBool("fanState", false);
  exhaustState = preferences.getBool("exhaustState", false);
  roomLightState = preferences.getBool("roomLightState", false);
  mainLightState = preferences.getBool("mainLightState", false);

  ArduinoOTA.begin();
}

void loop() {
  webServer.handleClient();
  ArduinoOTA.handle();
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 10000) {
    lastWiFiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Reconnecting WiFi...");
      WiFi.disconnect();
      WiFi.reconnect();
      wifiConnected = false;
    } else {
      wifiConnected = true;
    }
  }
  // Auto main light timeout
  if (mainLightState && !iotMainLight && (millis() - lastMotionTime > MAIN_LIGHT_TIMEOUT)) {
    mainLightState = false;
    digitalWrite(RELAY_MAIN_LIGHT, LOW);
    saveRelayStates();
  }
  // Buzzer pulse
  if (buzzerState && buzzerPulseStart == 0) {
    buzzerPulseStart = millis();
    digitalWrite(BUZZER, HIGH);
  }
  if (buzzerPulseStart && millis() - buzzerPulseStart > BUZZER_PULSE_DURATION) {
    digitalWrite(BUZZER, LOW);
    buzzerPulseStart = 0;
    buzzerState = false;
  }
  // Check TCP Client
  client = server.available();
  if (client && client.connected()) {
    String line = client.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      unsigned long ts;
      float temp, hum;
      int motion, gas, sound, ir;
      unsigned int dist;

      int parsed = sscanf(line.c_str(), "%lu,%f,%f,%d,%d,%d,%d,%u",
                          &ts, &temp, &hum, &motion, &gas, &sound, &ir, &dist);
      if (parsed == 8) {
        // Update sensor data
        currentTemp = temp;
        currentHumidity = hum;
        motionDetected = motion;
        gasDetected = gas;
        soundDetected = sound;
        irObjectDetected = ir;
        distance = dist;
        
        controlActuators(temp, gas, sound, motion, ir);
      }
    }
    client.stop();
  }
  delay(10);
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>Smart Home Controller</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    :root {
      --bg-gradient-light: linear-gradient(120deg, #a8edea 0%, #7ed6df 50%, #70e1f5 100%, #43e97b 100%);
      --bg-gradient-dark: linear-gradient(120deg, #232526 0%, #414345 100%);
      --card-bg-light: rgba(255,255,255,0.75);
      --card-bg-dark: rgba(34,34,34,0.85);
      --text-light: #222;
      --text-dark: #f3f3f3;
      --accent-light: #43e97b;
      --accent-dark: #70e1f5;
    }
    body {
      font-family: 'Inter', 'Segoe UI', Arial, sans-serif;
      background: var(--bg-gradient-light);
      min-height: 100vh;
      color: var(--text-light);
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      transition: background 0.5s, color 0.5s;
    }
    body.dark {
      background: var(--bg-gradient-dark);
      color: var(--text-dark);
    }
    .main-card {
      background: var(--card-bg-light);
      color: var(--text-light);
      border-radius: 32px;
      box-shadow: 0 8px 40px 0 rgba(67,233,123,0.10), 0 1.5px 8px 0 rgba(102,126,234,0.10);
      padding: 36px 32px 24px 32px;
      margin: 0 auto;
      display: flex;
      flex-direction: column;
      align-items: center;
      min-width: 340px;
      max-width: 520px;
      z-index: 2;
      backdrop-filter: blur(12px);
      border: 1.5px solid rgba(67,233,123,0.13);
    }
    body.dark .main-card {
      background: var(--card-bg-dark);
      color: var(--text-dark);
    }
    .header {
      grid-column: 1 / -1;
      text-align: center;
      margin-bottom: 18px;
    }
    .header h1 {
      font-size: 2.5em;
      font-weight: 700;
      letter-spacing: 0.01em;
      color: var(--text-light);
      margin-bottom: 8px;
      text-shadow: 0 2px 12px rgba(67,233,123,0.10);
    }
    body.dark .header h1 {
      color: var(--text-dark);
    }
    .header p {
      color: var(--text-light);
      font-size: 1.15em;
      font-weight: 400;
      letter-spacing: 0.01em;
      margin-bottom: 0;
    }
    body.dark .header p {
      color: var(--text-dark);
    }
    /* Glassy Sidebars */
    .sidebar {
      background: rgba(255,255,255,0.55);
      border-radius: 18px;
      box-shadow: 0 4px 24px rgba(102,126,234,0.08);
      padding: 18px 0 18px 0;
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 18px;
      backdrop-filter: blur(8px);
      min-width: 60px;
      z-index: 3;
    }
    .sidebar .icon {
      position: relative;
      margin-bottom: 8px;
      cursor: pointer;
      transition: transform 0.2s;
    }
    .sidebar .icon:hover { transform: scale(1.12); }
    .sidebar .icon svg {
      width: 32px; height: 32px; fill: #bbb; transition: fill 0.3s;
      filter: drop-shadow(0 2px 6px rgba(102,126,234,0.08));
    }
    .sidebar .icon.connected svg { fill: #22c55e; }
    .sidebar .icon.disconnected svg { fill: #ef4444; }
    .sidebar .icon:hover::after {
      content: attr(data-tooltip);
      position: absolute;
      left: 110%; top: 50%; transform: translateY(-50%);
      background: #fff; color: #222; font-size: 0.9em; padding: 3px 10px;
      border-radius: 6px; box-shadow: 0 2px 8px rgba(102,126,234,0.08);
      white-space: nowrap;
      z-index: 10;
    }
    .sidebar h3 {
      color: #222;
      margin-bottom: 10px;
      font-size: 1.1em;
      font-weight: 700;
      text-align: center;
      letter-spacing: 0.01em;
    }
    .sidebar span { font-size: 0.95em; margin-top: 2px; color: #444; }
    /* Main Controls Card */
    .main-content {
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 24px;
      width: 100%;
    }
    .controls-card {
      background: none;
      border-radius: 22px;
      box-shadow: none;
      padding: 0;
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 28px 36px;
      margin-bottom: 0;
      min-width: 320px;
      max-width: 480px;
    }
    .control-card {
      background: rgba(255,255,255,0.85);
      box-shadow: 0 2px 12px rgba(67,233,123,0.08);
      border-radius: 16px;
      padding: 18px 8px 12px 8px;
      text-align: center;
      transition: box-shadow 0.2s, background 0.2s;
    }
    .control-card:hover {
      box-shadow: 0 4px 24px rgba(67,233,123,0.13);
      background: rgba(255,255,255,0.95);
    }
    .control-card h3 {
      color: #222;
      margin-bottom: 10px;
      font-size: 1.1em;
      font-weight: 600;
      letter-spacing: 0.01em;
    }
    .switch {
      position: relative;
      display: inline-block;
      width: 54px;
      height: 28px;
      margin: 6px 0;
    }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider {
      position: absolute;
      cursor: pointer;
      top: 0; left: 0; right: 0; bottom: 0;
      background-color: #e5e7eb;
      transition: .4s;
      border-radius: 28px;
      box-shadow: 0 2px 8px rgba(102,126,234,0.08);
    }
    .slider:before {
      position: absolute;
      content: "";
      height: 22px; width: 22px;
      left: 3px; bottom: 3px;
      background-color: #fff;
      transition: .4s;
      border-radius: 50%;
      box-shadow: 0 2px 8px rgba(102,126,234,0.10);
    }
    input:checked + .slider { background-color: #22c55e; }
    input:checked + .slider:before { transform: translateX(26px); }
    .status-text {
      margin-top: 4px;
      font-weight: 600;
      color: #43e97b;
      font-size: 0.95em;
      letter-spacing: 0.01em;
    }
    /* Sensor Bar */
    .sensor-data {
      background: rgba(255,255,255,0.8);
      border-radius: 18px;
      box-shadow: 0 4px 24px rgba(67,233,123,0.10);
      padding: 12px 10px 8px 10px;
      margin-top: 18px;
      backdrop-filter: blur(8px);
      min-width: 320px;
      max-width: 600px;
      margin-left: auto;
      margin-right: auto;
      border: 1.5px solid rgba(67,233,123,0.13);
    }
    .sensor-data h3 {
      color: #222;
      margin-bottom: 8px;
      font-size: 1em;
      font-weight: 700;
      text-align: center;
    }
    .sensor-grid {
      display: grid;
      grid-template-columns: repeat(6, 1fr);
      gap: 10px;
    }
    .sensor-item {
      text-align: center;
      padding: 4px 0 2px 0;
      background: none;
      border-radius: 8px;
    }
    .sensor-item .icon svg {
      width: 22px; height: 22px; margin-bottom: 2px;
      fill: #bbb;
    }
    .icon.normal svg { fill: #22c55e; }
    .icon.alert svg { fill: #ef4444; }
    .icon.inactive svg { fill: #bbb; }
    .sensor-value {
      font-size: 1em;
      font-weight: 600;
      color: #43e97b;
      margin-bottom: 0;
    }
    .sensor-label {
      color: #888;
      font-size: 0.8em;
      margin-top: 1px;
    }
    @media (max-width: 900px) {
      .container { grid-template-columns: 1fr; }
      .main-card { min-width: 0; max-width: 100%; }
      .sensor-data { min-width: 0; max-width: 100%; }
      .sensor-grid { grid-template-columns: repeat(3, 1fr); }
    }
    @media (max-width: 600px) {
      .header h1 { font-size: 1.1em; }
      .controls-card { grid-template-columns: 1fr; gap: 12px; padding: 18px 6px 12px 6px; }
      .sensor-grid { grid-template-columns: repeat(2, 1fr); }
      .sidebar { min-width: 40px; padding: 8px 0; }
      .main-card { padding: 16px 4px 10px 4px; }
    }
    .theme-toggle {
      position: absolute;
      top: 18px;
      right: 24px;
      background: rgba(255,255,255,0.7);
      border: none;
      border-radius: 18px;
      padding: 6px 18px;
      font-size: 1em;
      font-weight: 600;
      color: #222;
      cursor: pointer;
      box-shadow: 0 2px 8px rgba(67,233,123,0.10);
      transition: background 0.3s, color 0.3s;
      z-index: 10;
    }
    body.dark .theme-toggle {
      background: rgba(34,34,34,0.7);
      color: #f3f3f3;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <button class="theme-toggle" id="theme-toggle">🌙 Dark Mode</button>
      <h1>Smart Home Controller</h1>
      <p>Real-time monitoring and control</p>
    </div>
    <!-- Left Sidebar - Connection Status -->
    <div class="sidebar">
      <h3>Connection Status</h3>
      <span class="icon" id="wifi-icon" data-tooltip="WiFi Connection">
        <svg width="24" height="24" viewBox="0 0 24 24"><path d="M12 20c.552 0 1-.447 1-1s-.448-1-1-1-1 .447-1 1 .448 1 1 1zm2.07-2.93c-.39-.39-1.02-.39-1.41 0-.39.39-.39 1.02 0 1.41.39.39 1.02.39 1.41 0 .39-.39.39-1.02 0-1.41zm2.83-2.83c-.78-.78-2.05-.78-2.83 0-.78.78-.78 2.05 0 2.83.78.78 2.05.78 2.83 0 .78-.78.78-2.05 0-2.83zm2.83-2.83c-1.17-1.17-3.07-1.17-4.24 0-1.17 1.17-1.17 3.07 0 4.24 1.17 1.17 3.07 1.17 4.24 0 1.17-1.17 1.17-3.07 0-4.24z"/></svg>
      </span>
      <span class="icon" id="firebase-icon" data-tooltip="Firebase">
        <svg width="24" height="24" viewBox="0 0 24 24"><path d="M3 17.25l7.39-12.67c.18-.31.63-.31.81 0l7.39 12.67c.18.31-.04.7-.41.7H3.41c-.37 0-.59-.39-.41-.7z"/></svg>
      </span>
      <span class="icon" id="iot-icon" data-tooltip="Arduino IoT Cloud">
        <svg width="24" height="24" viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm0 18c-4.41 0-8-3.59-8-8s3.59-8 8-8 8 3.59 8 8-3.59 8-8 8z"/></svg>
      </span>
    </div>
    <!-- Main Card -->
    <div class="main-card">
      <div class="main-content">
        <div class="controls-card">
          <div class="control-card">
            <h3>Fan Control</h3>
            <label class="switch">
              <input type="checkbox" id="fan-switch" onclick="toggleRelay('fan')">
              <span class="slider"></span>
            </label>
            <div class="status-text" id="fan-text">OFF</div>
          </div>
          <div class="control-card">
            <h3>Exhaust Control</h3>
            <label class="switch">
              <input type="checkbox" id="exhaust-switch" onclick="toggleRelay('exhaust')">
              <span class="slider"></span>
            </label>
            <div class="status-text" id="exhaust-text">OFF</div>
          </div>
          <div class="control-card">
            <h3>Room Light</h3>
            <label class="switch">
              <input type="checkbox" id="room-light-switch" onclick="toggleRelay('room-light')">
              <span class="slider"></span>
            </label>
            <div class="status-text" id="room-light-text">OFF</div>
          </div>
          <div class="control-card">
            <h3>Main Light</h3>
            <label class="switch">
              <input type="checkbox" id="main-light-switch" onclick="toggleRelay('main-light')">
              <span class="slider"></span>
            </label>
            <div class="status-text" id="main-light-text">OFF</div>
          </div>
        </div>
      </div>
      <!-- Sensor Data - Horizontal Layout -->
      <div class="sensor-data">
        <h3>Sensor Data</h3>
        <div class="sensor-grid">
          <div class="sensor-item">
            <span class="icon" id="motion-sensor-icon">
              <svg width="24" height="24" viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"/></svg>
            </span>
            <div class="sensor-value" id="temp-value">--&deg;C</div>
            <div class="sensor-label">Temperature</div>
          </div>
          <div class="sensor-item">
            <span class="icon" id="gas-sensor-icon">
              <svg width="24" height="24" viewBox="0 0 24 24"><rect x="4" y="4" width="16" height="16" rx="4"/></svg>
            </span>
            <div class="sensor-value" id="humidity-value">--%</div>
            <div class="sensor-label">Humidity</div>
          </div>
          <div class="sensor-item">
            <span class="icon" id="motion-sensor-icon">
              <svg width="24" height="24" viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"/></svg>
            </span>
            <div class="sensor-value" id="motion-value">No</div>
            <div class="sensor-label">Motion</div>
          </div>
          <div class="sensor-item">
            <span class="icon" id="gas-sensor-icon">
              <svg width="24" height="24" viewBox="0 0 24 24"><rect x="4" y="4" width="16" height="16" rx="4"/></svg>
            </span>
            <div class="sensor-value" id="gas-value">Safe</div>
            <div class="sensor-label">Gas</div>
          </div>
          <div class="sensor-item">
            <span class="icon" id="sound-sensor-icon">
              <svg width="24" height="24" viewBox="0 0 24 24"><path d="M3 12h2l4 8V4l4 8h2"/></svg>
            </span>
            <div class="sensor-value" id="sound-value">Quiet</div>
            <div class="sensor-label">Sound</div>
          </div>
          <div class="sensor-item">
            <span class="icon" id="distance-sensor-icon">
              <svg width="24" height="24" viewBox="0 0 24 24"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm0 18c-4.41 0-8-3.59-8-8s3.59-8 8-8 8 3.59 8 8-3.59 8-8 8z"/></svg>
            </span>
            <div class="sensor-value" id="distance-value">--cm</div>
            <div class="sensor-label">Distance</div>
          </div>
        </div>
      </div>
    </div>
    <!-- Right Sidebar - System Status -->
    <div class="sidebar">
      <h3>System Status</h3>
      <span class="icon" id="fan-status-icon" data-tooltip="Fan">
        <svg width="24" height="24" viewBox="0 0 24 24"><path d="M12 4V2m0 20v-2m8-8h2M2 12H4m15.07-7.07l1.41-1.41M4.93 19.07l-1.41 1.41m0-16.97l1.41 1.41M19.07 19.07l1.41-1.41"/></svg>
      </span>
      <span class="icon" id="exhaust-status-icon" data-tooltip="Exhaust">
        <svg width="24" height="24" viewBox="0 0 24 24"><path d="M3 12h18M3 16h18M3 8h18"/></svg>
      </span>
      <span class="icon" id="room-light-status-icon" data-tooltip="Room Light">
        <svg width="24" height="24" viewBox="0 0 24 24"><path d="M12 2a7 7 0 0 1 7 7c0 3.87-3.13 7-7 7s-7-3.13-7-7a7 7 0 0 1 7-7zm0 18v2m-4-2h8"/></svg>
      </span>
      <span class="icon" id="main-light-status-icon" data-tooltip="Main Light">
        <svg width="24" height="24" viewBox="0 0 24 24"><circle cx="12" cy="12" r="6"/></svg>
      </span>
    </div>
  </div>

  <script>
    function toggleRelay(device) {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "/toggle/" + device, true);
      xhr.onload = function() {
        if (xhr.status == 200) {
          updateStatus();  // Immediately refresh status
        }
      };
      xhr.send();
    }
    
    function updateStatus() {
      var xhr = new XMLHttpRequest();
      xhr.onreadystatechange = function() {
        if (xhr.readyState == 4 && xhr.status == 200) {
          var data = JSON.parse(xhr.responseText);
          
          // Connection status icons
          document.getElementById('wifi-icon').className = 'icon ' + (data.wifi ? 'connected' : 'disconnected');
          document.getElementById('firebase-icon').className = 'icon ' + (data.firebase ? 'connected' : 'disconnected');
          document.getElementById('iot-icon').className = 'icon ' + (data.iot ? 'connected' : 'disconnected');
          
          // System status icons
          document.getElementById('fan-status-icon').className = 'icon ' + (data.fan ? 'connected' : 'disconnected');
          document.getElementById('exhaust-status-icon').className = 'icon ' + (data.exhaust ? 'connected' : 'disconnected');
          document.getElementById('room-light-status-icon').className = 'icon ' + (data.roomLight ? 'connected' : 'disconnected');
          document.getElementById('main-light-status-icon').className = 'icon ' + (data.mainLight ? 'connected' : 'disconnected');
          
          // Sensor icons
          document.getElementById('motion-sensor-icon').className = 'icon ' + (data.motion ? 'normal' : 'inactive');
          document.getElementById('gas-sensor-icon').className = 'icon ' + (data.gas ? 'alert' : 'normal');
          document.getElementById('sound-sensor-icon').className = 'icon ' + (data.sound ? 'alert' : 'normal');
          
          // Update switches
          document.getElementById('fan-switch').checked = data.fan;
          document.getElementById('exhaust-switch').checked = data.exhaust;
          document.getElementById('room-light-switch').checked = data.roomLight;
          document.getElementById('main-light-switch').checked = data.mainLight;
          
          // Update status text
          document.getElementById('fan-text').textContent = data.fan ? 'ON' : 'OFF';
          document.getElementById('exhaust-text').textContent = data.exhaust ? 'ON' : 'OFF';
          document.getElementById('room-light-text').textContent = data.roomLight ? 'ON' : 'OFF';
          document.getElementById('main-light-text').textContent = data.mainLight ? 'ON' : 'OFF';
          
          // Update sensor data
          document.getElementById('temp-value').textContent = data.temperature + '°C';
          document.getElementById('humidity-value').textContent = data.humidity + '%';
          document.getElementById('motion-value').textContent = data.motion ? 'Yes' : 'No';
          document.getElementById('gas-value').textContent = data.gas ? 'Alert' : 'Safe';
          document.getElementById('sound-value').textContent = data.sound ? 'Detected' : 'Quiet';
          document.getElementById('distance-value').textContent = data.distance + 'cm';
        }
      };
      xhr.open("GET", "/status", true);
      xhr.send();
    }
    
    // Update status every 2 seconds
    setInterval(updateStatus, 2000);
    updateStatus(); // Initial update

    // Theme toggle logic
    function setTheme(dark) {
      if (dark) {
        document.body.classList.add('dark');
        localStorage.setItem('theme', 'dark');
        document.getElementById('theme-toggle').textContent = '☀️ Light Mode';
      } else {
        document.body.classList.remove('dark');
        localStorage.setItem('theme', 'light');
        document.getElementById('theme-toggle').textContent = '🌙 Dark Mode';
      }
    }
    document.getElementById('theme-toggle').onclick = function() {
      setTheme(!document.body.classList.contains('dark'));
    };
    // On load, set theme from localStorage
    (function() {
      var theme = localStorage.getItem('theme');
      setTheme(theme === 'dark');
    })();
  </script>
</body>
</html>
)rawliteral";
  webServer.send(200, "text/html", html);
}

void handleStatus() {
  webServer.send(200, "application/json", getStatusJSON());
}

void handleToggle() {
  String uri = webServer.uri();
  if (uri == "/toggle/fan") {
    iotFan = !iotFan;
  } else if (uri == "/toggle/exhaust") {
    iotExhaust = !iotExhaust;
  } else if (uri == "/toggle/room-light") {
    iotRoomLight = !iotRoomLight;
  } else if (uri == "/toggle/main-light") {
    iotMainLight = !iotMainLight;
  }
  webServer.send(200, "text/plain", "OK");
}

void handleUpdate() {
  if (!webServer.hasHeader("Authorization") || webServer.header("Authorization") != API_TOKEN) {
    webServer.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
    return;
  }
  if (webServer.args() == 0) {
    webServer.send(400, "application/json", "{\"error\":\"No data\"}");
    return;
  }
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, webServer.arg(0));
  if (error) {
    webServer.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  currentTemp = doc["temperature"] | currentTemp;
  currentHumidity = doc["humidity"] | currentHumidity;
  motionDetected = doc["motion"] | motionDetected;
  gasDetected = doc["gas"] | gasDetected;
  soundDetected = doc["sound"] | soundDetected;
  irObjectDetected = doc["ir_object"] | irObjectDetected;
  distance = doc["distance"] | distance;
  unsigned long ts = doc["timestamp"] | millis();
  // Update actuators
  controlActuators(currentTemp, gasDetected, soundDetected, motionDetected, irObjectDetected);
  webServer.send(200, "application/json", "{\"status\":\"ok\"}");
}

void controlActuators(float temp, int gas, int sound, int motion, int ir) {
  if (millis() - lastActuatorUpdate < ACTUATOR_DEBOUNCE) return;
  lastActuatorUpdate = millis();
  // Fan Control
  bool autoFan = temp > TEMP_THRESHOLD;
  fanState = autoFan || iotFan;
  digitalWrite(RELAY_FAN, fanState);
  // Exhaust Control
  bool autoExhaust = gas == 1;
  exhaustState = autoExhaust || iotExhaust;
  digitalWrite(RELAY_EXHAUST, exhaustState);
  // Room Light Manual
  roomLightState = iotRoomLight;
  digitalWrite(RELAY_ROOM_LIGHT, roomLightState);
  // Main Light Auto + Manual
  bool motionNow = (motion || ir);
  if (motionNow) lastMotionTime = millis();
  mainLightState = motionNow || iotMainLight;
  digitalWrite(RELAY_MAIN_LIGHT, mainLightState);
  // Buzzer
  if (sound) buzzerState = true;
  // RGB Indicator
  if (motionNow) setRGB(0, 255, 0);
  else if (gas) setRGB(255, 0, 0);
  else setRGB(0, 0, 255);
  saveRelayStates();
}

String getStatusJSON() {
  String json = "{";
  json += "\"wifi\":" + String(wifiConnected ? "true" : "false") + ",";
  json += "\"firebase\":" + String(firebaseConnected ? "true" : "false") + ",";
  json += "\"iot\":" + String(iotCloudConnected ? "true" : "false") + ",";
  json += "\"fan\":" + String(fanState ? "true" : "false") + ",";
  json += "\"exhaust\":" + String(exhaustState ? "true" : "false") + ",";
  json += "\"roomLight\":" + String(roomLightState ? "true" : "false") + ",";
  json += "\"mainLight\":" + String(mainLightState ? "true" : "false") + ",";
  json += "\"temperature\":" + String(currentTemp, 1) + ",";
  json += "\"humidity\":" + String(currentHumidity, 1) + ",";
  json += "\"motion\":" + String(motionDetected ? "true" : "false") + ",";
  json += "\"gas\":" + String(gasDetected ? "true" : "false") + ",";
  json += "\"sound\":" + String(soundDetected ? "true" : "false") + ",";
  json += "\"distance\":" + String(distance);
  json += "}";
  return json;
}

void setRGB(int r, int g, int b) {
  ledcWrite(PWM_CHANNEL_R, r);
  ledcWrite(PWM_CHANNEL_G, g);
  ledcWrite(PWM_CHANNEL_B, b);
}

void saveRelayStates() {
  preferences.putBool("iotFan", iotFan);
  preferences.putBool("iotExhaust", iotExhaust);
  preferences.putBool("iotRoomLight", iotRoomLight);
  preferences.putBool("iotMainLight", iotMainLight);
  preferences.putBool("fanState", fanState);
  preferences.putBool("exhaustState", exhaustState);
  preferences.putBool("roomLightState", roomLightState);
  preferences.putBool("mainLightState", mainLightState);
}
