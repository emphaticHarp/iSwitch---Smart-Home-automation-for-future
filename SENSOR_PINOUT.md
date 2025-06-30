# Smart Home Sensor Pin Mapping

## ESP8266 NodeMCU Pin Configuration

| ESP8266 Pin | Connected To Component | Pin Label | Sensor Model |
|-------------|----------------------|-----------|--------------|
| D1 | IR sensor | OUT | Generic IR Sensor |
| D2 | PIR sensor | SIG | Generic PIR Sensor |
| D3 | Ultrasonic US-015 | TRIG | US-015 Ultrasonic |
| D4 | Ultrasonic US-015 | ECHO | US-015 Ultrasonic |
| D5 | DHT11 Sensor Module | DATA | DHT11 |
| D6 | MQ-2 | Digital | MQ-2 Gas Sensor |
| D7 | KY-037 | D0 | KY-037 Sound Sensor |

## GPIO Pin Details

- **D1 (GPIO5)**: IR Sensor - Digital output for object detection
- **D2 (GPIO4)**: PIR Sensor - Digital output for motion detection  
- **D3 (GPIO0)**: US-015 TRIG - Trigger signal for distance measurement
- **D4 (GPIO2)**: US-015 ECHO - Echo signal for distance measurement
- **D5 (GPIO14)**: DHT11 - Digital data line for temperature/humidity
- **D6 (GPIO12)**: MQ-2 - Digital output for gas detection
- **D7 (GPIO13)**: KY-037 - Digital output for sound detection

## Power Requirements

- **VCC**: 3.3V for all sensors
- **GND**: Common ground connection
- **Current**: Ensure adequate power supply for all sensors

## Wiring Notes

1. Connect all sensor VCC pins to 3.3V
2. Connect all sensor GND pins to common ground
3. Connect signal pins according to the table above
4. Use appropriate pull-up/pull-down resistors if needed
5. Keep wires short to minimize interference

## Implementation Status

✅ **All sensors implemented in `src/main.cpp`:**
- ✅ IR Sensor (D1) - Object detection
- ✅ PIR Sensor (D2) - Motion detection
- ✅ US-015 Ultrasonic (D3/D4) - Distance measurement (0-400cm)
- ✅ DHT11 (D5) - Temperature/humidity
- ✅ MQ-2 Gas Sensor (D6) - Gas detection
- ✅ KY-037 Sound Sensor (D7) - Sound detection

## Data Format

### Firebase Paths
- `/sensors/temperature` - Temperature in °C
- `/sensors/humidity` - Humidity in %
- `/sensors/motion` - Motion detection (boolean)
- `/sensors/gas` - Gas detection (boolean)
- `/sensors/sound` - Sound detection (boolean)
- `/sensors/ir_object` - IR object detection (boolean)
- `/sensors/distance` - Distance in cm

### TCP Data Format
Data sent to ESP32: `timestamp,temp,hum,motion,gas,sound,irObject,distance`

Example: `1234567,23.45,67.89,1,0,1,0,25`

## Required Libraries

- `FirebaseESP8266` - Firebase integration
- `DHT sensor library` - Temperature/humidity sensor
- `Adafruit Unified Sensor` - Sensor abstraction layer
- `NewPing` - Ultrasonic sensor library
- `IRremote` - IR sensor library

## Sensor Behavior

- **IR Sensor**: Detects objects when output goes LOW
- **PIR Sensor**: Detects motion when output goes HIGH
- **MQ-2 Gas Sensor**: Detects gas when output goes LOW
- **KY-037 Sound Sensor**: Detects sound when output goes HIGH
- **US-015 Ultrasonic**: Returns distance in centimeters (0-400cm)
- **DHT11**: Returns temperature (°C) and humidity (%)

## Sensor Specifications

### US-015 Ultrasonic Sensor
- **Range**: 2cm - 400cm
- **Accuracy**: ±1cm
- **Operating Voltage**: 3.3V
- **Trigger Pulse**: 10μs minimum

### MQ-2 Gas Sensor
- **Detection**: LPG, Propane, Methane, Alcohol, Hydrogen, Smoke
- **Operating Voltage**: 3.3V
- **Digital Output**: TTL compatible
- **Response Time**: <10s

### KY-037 Sound Sensor
- **Operating Voltage**: 3.3V
- **Digital Output**: TTL compatible
- **Sensitivity**: Adjustable via potentiometer
- **Detection Range**: 50-100dB 