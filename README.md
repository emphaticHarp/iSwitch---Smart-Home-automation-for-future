# 🏠 iSwitch: AI-Powered IoT Smart Home Automation System

> **Built with ESP8266 + ESP32 + Arduino IoT Cloud + Sensors + OpenAI API (optional)**  
> Wireless sensor communication | Real-time control | Voice assistant-ready

---

## 📋 Project Overview

**iSwitch** is an advanced modular IoT Smart Home Automation system that wirelessly connects sensors (via ESP8266) with a main control hub (ESP32) using TCP communication. It provides real-time monitoring, auto/manual appliance control, and integration with Arduino IoT Cloud, voice assistants, and AI APIs.

---

## 📶 System Architecture

```
[Sensors - ESP8266] --(WiFi TCP)--> [ESP32 - Control & Dashboard]
                                     |
     ┌─────────────────────┬────────┴─────────┬────────────────────┐
     ↓                     ↓                  ↓                    ↓
[Relay for Fan]     [Relay for Light]   [Buzzer & Speaker]   [RGB LED Status]
```

---

## 🔧 Components Used

### 🧠 Microcontrollers
- **ESP8266 (LoLin NodeMCU)** – Sensor Node
- **ESP32 Dev Board** – Main Controller

### 🎯 Sensors
- **PIR Sensor** (Motion Detection)
- **IR Sensor** (Presence Detection)
- **DHT11** (Temperature + Humidity)
- **Ultrasonic** (Distance Measurement)
- **MQ-2** (Gas Leakage Detection)
- **Sound Sensor** (KY-037)

### ⚡ Actuators & Output
- **4-Channel Relay Module** (Fan, Lights, Exhaust)
- **Buzzer** (Alert System)
- **TPA3110 30W+30W Amplifier + Speaker**
- **RGB LED** (Status Indicator)

---

## 🔌 Functional Description

| Feature             | Details                                      |
| ------------------- | -------------------------------------------- |
| 🚪 Motion Detection | PIR + IR for room entry detection            |
| 🌡️ Temp Monitoring | DHT11 triggers fan automatically             |
| 💨 Gas Detection    | MQ-2 auto-triggers exhaust fan & buzzer      |
| 🧠 AI Integration   | Voice command via OpenAI (optional)          |
| 📢 Alerts           | Welcome audio, gas/heat alerts via speaker   |
| 🔦 Lighting         | Auto/manual control with sensor + IoT toggle |
| 📲 Control          | Arduino IoT Cloud dashboard + mobile/web     |

---

## 📁 Project Structure

```
smart home/
│
├── src/main.cpp                    ← ESP8266 Sensor Node (TCP Client)
├── platformio.ini                  ← ESP8266 Build Configuration
│
├── smart home 2/
│   ├── src/main.cpp               ← ESP32 Main Controller (TCP Server)
│   └── platformio.ini             ← ESP32 Build Configuration
│
├── SENSOR_PINOUT.md               ← Detailed Wiring Diagrams
├── IMPROVEMENTS_GUIDE.md          ← Advanced Features & Optimizations
└── README.md                      ← This file
```

---

## 🔄 Data Format (TCP Protocol)

**ESP8266 → ESP32 (every 5 seconds):**

```json
{
  "temperature": 28.5,
  "humidity": 65,
  "motion": true,
  "gas": false,
  "sound": false,
  "ir_object": true,
  "distance": 12,
  "timestamp": 1719735945,
  "is_valid": true,
  "firmware_version": "v1.0.5"
}
```

ESP32 parses and uses this to:
- Display live dashboard
- Trigger appliances automatically
- Send data to Arduino IoT Cloud
- Control RGB LED status

---

## 🌐 Arduino IoT Cloud Integration

**Used to toggle:**
- Fan
- Exhaust
- Room Light
- Main Light

**Control via:**
- Web dashboard
- Arduino IoT Remote app
- Optional voice via Alexa/Google Home

> **Device ID:** `4c20eeb6-c003-4749-9fd6-e3d8c22c92ad`  
> **Secret Key:** `oKDgnNIBqOaWgrT?Xy?AFpn8e`

---

## 🔥 Firebase Integration

**ESP8266 uploads to Firebase:**
- Real-time sensor data
- Historical logs with timestamps
- System health monitoring
- Error states and diagnostics

**Firebase Structure:**
```
/sensors/          ← Current sensor state
/logs/{timestamp}  ← Historical data
/status/esp8266    ← System health
```

---

## 🛡️ Security Features

- **API Token Authentication** (HMAC signatures)
- **Build Flag Credentials** (no hardcoded secrets)
- **Data Validation** (sensor range checking)
- **EEPROM Data Integrity** (magic number verification)
- **WiFi Security** (WPA2/WPA3 support)

---

## 📡 How to Run

### 1. **Flash ESP8266 Code:**
```bash
# In main project directory
pio run -e nodemcuv2 --target upload
```
- Set WiFi and Firebase credentials in `platformio.ini`
- Monitor serial output for debug info

### 2. **Flash ESP32 Code:**
```bash
# Navigate to ESP32 project
cd "smart home 2"
pio run -e esp32dev --target upload
```
- Set WiFi credentials in `smart home 2/platformio.ini`
- Monitor serial output for debug info

### 3. **Open Web Interface:**
- Access ESP32's IP address in browser
- Control relays in real-time
- View live sensor data and system status

### 4. **Test Automation:**
- Trigger sensors and observe relay responses
- Check Firebase for data uploads
- Verify OTA updates at `http://esp8266-sensor.local/update`

---

## 🚀 Advanced Features

### **Production-Grade Optimizations:**
- **EEPROM Rate Limiting** (protects flash memory)
- **Non-blocking LED Control** (smooth OTA updates)
- **WiFi Auto-reconnection** with timeout
- **Memory Optimization** (single JSON objects)
- **Error LED Patterns** (visual diagnostics)

### **Real-time Monitoring:**
- **Web Dashboard** with live updates
- **Serial Debug Output** with timestamps
- **Firebase Health Pings** every minute
- **System Status JSON** endpoints

### **Reliability Features:**
- **Sensor Fallback** (last valid readings)
- **DHT11 Retry Mechanism** (3 attempts)
- **Ultrasonic Error Detection**
- **Client Timeout Protection** (3-second timeout)

---

## 🧠 Optional Enhancements

- **OpenAI GPT-4 API** for intelligent voice interactions
- **LCD/Touchscreen** for local control
- **Blynk Integration** for mobile apps
- **8-16 Channel Relay** system expansion
- **MQTT Support** for additional IoT integration
- **SSL/TLS Encryption** for secure communications
- **Sensor Calibration** (automatic threshold adjustment)
- **Battery Monitoring** for portable deployments

---

## 📊 Performance Metrics

| Metric | ESP8266 | ESP32 |
|--------|---------|-------|
| **RAM Usage** | 45.2% (37KB/81KB) | 15.1% (49KB/327KB) |
| **Flash Usage** | 59.0% (616KB/1MB) | 67.5% (885KB/1.3MB) |
| **Sensor Read Interval** | 5 seconds | Real-time |
| **Web Update Interval** | N/A | 2 seconds |

---

## 🛠️ Troubleshooting

### **Common Issues:**
1. **WiFi Connection**: Check credentials and signal strength
2. **Sensor Errors**: Verify wiring and power supply
3. **Firebase Upload**: Monitor serial for error messages
4. **TCP Communication**: Ensure both devices on same network

### **Debug Tools:**
- **Serial Monitor**: Real-time logs with timestamps
- **Error LED**: Visual patterns for sensor issues
- **Firebase Console**: Data flow verification
- **Web Interface**: System status monitoring

---

## 📷 Gallery

<details>
<summary>Click to expand</summary>

📸 **Circuit Diagrams, Dashboard Screenshots, and Hardware Layout** can be added here.

### **System Block Diagram:**
```
[ESP8266 Sensors] → [WiFi] → [ESP32 Controller] → [Relays + Actuators]
       ↓                ↓              ↓                ↓
   [DHT11, PIR,     [TCP/IP]      [Web Server]    [Fan, Lights,
    Gas, IR, etc.]              [IoT Cloud]      Buzzer, LED]
```

### **Web Dashboard Features:**
- Real-time sensor data display
- Relay control buttons
- System status indicators
- WiFi/Firebase/Cloud status
- Error state visualization

</details>

---

## 👨‍💻 Author

**Soumyajyoti Banik**  
B.Tech CSE | 3rd Year  
IoT Developer & AI Enthusiast  
- **GitHub**: [@emphaticHarp](https://github.com/emphaticHarp)
- **Email**: soumyajyotibanik07@gmail.com

---

## 📜 License

MIT License. Use freely for learning, modify for your own project, and give credit if you reuse.

---

## 🤝 Contributing

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

---

## 📞 Support & Questions

- **GitHub Issues**: Open an issue for bugs or feature requests
- **Email**: soumyajyotibanik07@gmail.com
- **Documentation**: See `IMPROVEMENTS_GUIDE.md` for advanced usage
- **Wiring Help**: Check `SENSOR_PINOUT.md` for detailed connections

---

**⭐ Star this repository if it helped you with your IoT project!** 