//BrakeGuard Firmware — MYOSA 6.0 (MYOSA-native version)

#include <myosa.h>
#include <WiFi.h>
#include <PubSubClient.h>

MYOSA myosa;

// =============================================================================
// CONFIG
// =============================================================================

// ---- Feature toggles -------------------------------------------------------
#define SIMULATE_PROBE          0   // 1 = fake capacitance data (no probe wired yet)_for testing
#define ENABLE_WIFI_MQTT        0   // set 0 to skip WiFi/MQTT for bench testing
#define ENABLE_BLE_APP          0   // myosa.begin() starts BLE server regardless;

// ---- Buzzer: direct GPIO (see BUZZER NOTE above) ----------------------------
#define BUZZER_PIN    4     

// ---- Capacitive probe RC circuit pins ---------------------------------------
#define PROBE_CHARGE_PIN   25
#define PROBE_READ_PIN     26

// ---- WiFi / MQTT config -----------------------------------------------------
const char* WIFI_SSID     = "YOUR_WIFI_SSID";       
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";   
const char* MQTT_BROKER   = "broker.hivemq.com";    
const int   MQTT_PORT     = 1883;
const char* MQTT_CLIENT_ID = "BrakeGuard_ESP32";
const char* MQTT_TOPIC_STATE = "brakeguard/state";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ---- Calibration constants --------------------------------
// Linear model: moisture% = CAL_A * rawReading(us) + CAL_B
float CAL_A = 0.0f;
float CAL_B = 0.0f;

const float moisturePoints[]     = {0.0, 1.0, 2.0, 3.0, 3.5};
const float boilingPointPoints[] = {205.0, 190.0, 175.0, 155.0, 140.0};
const int NUM_CAL_POINTS = 5;

// ---- Risk index weights & thresholds -----------------------------------------
float W_MOISTURE = 0.45f;
float W_DECEL     = 0.35f;
float W_FREQUENCY = 0.20f;

float RISK_WARNING_ON   = 30.0f;
float RISK_HARSH_ON     = 55.0f;
float RISK_HIGH_ON      = 75.0f;
float HYSTERESIS        = 5.0f;

#define BRAKING_WINDOW_MS       10000
#define HARD_BRAKE_THRESHOLD_G  0.5f
#define SENSOR_READ_INTERVAL_MS   200
#define OLED_UPDATE_INTERVAL_MS   500
#define MQTT_PUBLISH_INTERVAL_MS 2000
#define BLE_SEND_INTERVAL_MS     1000

// =============================================================================
// GLOBAL STATE
// =============================================================================

float filteredDecel_g = 0.0f;
unsigned long brakeEventTimestamps[20];
int brakeEventCount = 0;

float moisturePercent = 0.0f;
float ambientTempC = 0.0f;
float ambientPressurePa = 0.0f;
float estimatedBoilingPointC = 0.0f;
float riskIndex = 0.0f;

enum RiskState { SAFE, WARNING, HARSH_BRAKING, HIGH_RISK };
RiskState currentState = SAFE;

unsigned long lastSensorRead = 0;
unsigned long lastOledUpdate = 0;
unsigned long lastMqttPublish = 0;
unsigned long lastBleSend = 0;

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n--- BrakeGuard Firmware Boot (MYOSA-native) ---");

  Wire.begin();
  Wire.setClock(100000);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  #if !SIMULATE_PROBE
    pinMode(PROBE_CHARGE_PIN, OUTPUT);
    digitalWrite(PROBE_CHARGE_PIN, LOW);
  #endif

  // myosa.begin() initializes OLED, AccelAndGyro, AirQuality, BarometricPressure,
  // LightProximityAndGesture, the Actuator/gpioExpander (soft-fails harmlessly
  // if not physically present), TempAndHumidity, AND starts a BLE GATT server
  // for the MYOSA mobile app — all in one call, with Serial status lines for
  // each. Watch Serial output on first upload to confirm what's detected.
  myosa.begin();

  Serial.println("myosa.begin() complete — check lines above for per-sensor status");

  #if ENABLE_WIFI_MQTT
    connectWiFi();
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  #endif

  Serial.println("--- Setup complete ---\n");
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
  unsigned long now = millis();

  #if ENABLE_WIFI_MQTT
    if (!mqttClient.connected()) reconnectMQTT();
    mqttClient.loop();
  #endif

  if (now - lastSensorRead >= SENSOR_READ_INTERVAL_MS) {
    lastSensorRead = now;
    readMotion();
    readAmbient();
    readProbeAndComputeMoisture();
    computeRiskIndex();
    updateBuzzer();
  }

  if (now - lastOledUpdate >= OLED_UPDATE_INTERVAL_MS) {
    lastOledUpdate = now;
    updateOLED();
  }

  #if ENABLE_WIFI_MQTT
    if (now - lastMqttPublish >= MQTT_PUBLISH_INTERVAL_MS) {
      lastMqttPublish = now;
      publishMQTT();
    }
  #endif

  #if ENABLE_BLE_APP
    if (now - lastBleSend >= BLE_SEND_INTERVAL_MS) {
      lastBleSend = now;
      myosa.sendBleData();
    }
  #endif
}

// =============================================================================
// SENSOR READING (via myosa.h getters)
// =============================================================================

void readMotion() {
  float accelX_cm_s2 = myosa.Ag.getAccelX(false);
  float rawAccel_g = accelX_cm_s2 / 981.0f;

  const float ALPHA = 0.3f;
  filteredDecel_g = ALPHA * rawAccel_g + (1 - ALPHA) * filteredDecel_g;

  if (fabs(filteredDecel_g) > HARD_BRAKE_THRESHOLD_G) {
    logBrakeEvent();
  }
}

void logBrakeEvent() {
  unsigned long now = millis();
  if (brakeEventCount > 0 && (now - brakeEventTimestamps[brakeEventCount - 1]) < 500) {
    return;
  }
  if (brakeEventCount < 20) {
    brakeEventTimestamps[brakeEventCount++] = now;
  } else {
    for (int i = 1; i < 20; i++) brakeEventTimestamps[i - 1] = brakeEventTimestamps[i];
    brakeEventTimestamps[19] = now;
  }
}

int getBrakingFrequency() {
  unsigned long now = millis();
  int count = 0;
  for (int i = 0; i < brakeEventCount; i++) {
    if (now - brakeEventTimestamps[i] <= BRAKING_WINDOW_MS) count++;
  }
  return count;
}

void readAmbient() {
  ambientTempC = myosa.Pr.getTempC(false);
  ambientPressurePa = myosa.Pr.getPressurePascal(false);
}

// =============================================================================
// PROBE READING (simulated or real)
// =============================================================================

float readProbeRawMicroseconds() {
  #if SIMULATE_PROBE
    static float simBase = 300.0f;
    simBase += random(-3, 4);
    if (simBase < 200) simBase = 200;
    if (simBase > 600) simBase = 600;
    return simBase + random(-5, 5);
  #else
    pinMode(PROBE_READ_PIN, OUTPUT);
    digitalWrite(PROBE_READ_PIN, LOW);
    digitalWrite(PROBE_CHARGE_PIN, LOW);
    delay(2);
    pinMode(PROBE_READ_PIN, INPUT);

    unsigned long start = micros();
    digitalWrite(PROBE_CHARGE_PIN, HIGH);
    while (digitalRead(PROBE_READ_PIN) == LOW) {
      if (micros() - start > 50000UL) {
        Serial.println("WARNING: probe charge timeout");
        return -1;
      }
    }
    return (float)(micros() - start);
  #endif
}

void readProbeAndComputeMoisture() {
  float raw = readProbeRawMicroseconds();

  if (raw < 0) {
    Serial.println("Probe reading FAILED");
    return;
  }

  Serial.print("RAW probe reading (us): ");
  Serial.println(raw);

  moisturePercent = CAL_A * raw + CAL_B;

  if (moisturePercent < 0)
    moisturePercent = 0;

  estimatedBoilingPointC = interpolateBoilingPoint(moisturePercent);
}

float interpolateBoilingPoint(float moisture) {
  if (moisture <= moisturePoints[0]) return boilingPointPoints[0];
  if (moisture >= moisturePoints[NUM_CAL_POINTS - 1]) return boilingPointPoints[NUM_CAL_POINTS - 1];
  for (int i = 0; i < NUM_CAL_POINTS - 1; i++) {
    if (moisture >= moisturePoints[i] && moisture <= moisturePoints[i + 1]) {
      float t = (moisture - moisturePoints[i]) / (moisturePoints[i + 1] - moisturePoints[i]);
      return boilingPointPoints[i] + t * (boilingPointPoints[i + 1] - boilingPointPoints[i]);
    }
  }
  return boilingPointPoints[NUM_CAL_POINTS - 1];
}

// =============================================================================
// RISK INDEX
// =============================================================================

void computeRiskIndex() {
  float moistureScore = constrain((moisturePercent / 3.5f) * 100.0f, 0, 100);
  float decelScore = constrain((fabs(filteredDecel_g) / 1.0f) * 100.0f, 0, 100);
  int freq = getBrakingFrequency();
  float freqScore = constrain((freq / 5.0f) * 100.0f, 0, 100);

  riskIndex = W_MOISTURE * moistureScore + W_DECEL * decelScore + W_FREQUENCY * freqScore;

  switch (currentState) {
    case SAFE:
      if (riskIndex > RISK_WARNING_ON) currentState = WARNING;
      break;
    case WARNING:
      if (riskIndex > RISK_HARSH_ON) currentState = HARSH_BRAKING;
      else if (riskIndex < RISK_WARNING_ON - HYSTERESIS) currentState = SAFE;
      break;
    case HARSH_BRAKING:
      if (riskIndex > RISK_HIGH_ON) currentState = HIGH_RISK;
      else if (riskIndex < RISK_HARSH_ON - HYSTERESIS) currentState = WARNING;
      break;
    case HIGH_RISK:
      if (riskIndex < RISK_HIGH_ON - HYSTERESIS) currentState = HARSH_BRAKING;
      break;
  }
}

const char* stateToString(RiskState s) {
  switch (s) {
    case SAFE: return "SAFE";
    case WARNING: return "WARNING";
    case HARSH_BRAKING: return "HARSH BRAKING";
    case HIGH_RISK: return "HIGH RISK";
  }
  return "UNKNOWN";
}

// =============================================================================
// OUTPUTS: OLED, BUZZER, MQTT
// =============================================================================

void updateOLED() {
  myosa.display.clearDisplay();
  myosa.display.setTextSize(1);
  myosa.display.setTextColor(WHITE);
  myosa.display.setCursor(0, 0);

  myosa.display.println("BRAKEGUARD");
  myosa.display.print("Moisture: ");
  myosa.display.print(moisturePercent, 1);
  myosa.display.println(" %");
  myosa.display.print("Temp: ");
  myosa.display.print(ambientTempC, 1);
  myosa.display.println(" C");
  myosa.display.print("Decel: ");
  myosa.display.print(filteredDecel_g, 2);
  myosa.display.println(" g");
  myosa.display.print("BoilPt: ");
  myosa.display.print(estimatedBoilingPointC, 0);
  myosa.display.println(" C");
  myosa.display.println();
  myosa.display.print("Status: ");
  myosa.display.println(stateToString(currentState));

  myosa.display.display();
}

void updateBuzzer() {
  if (currentState == HIGH_RISK) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
}

// =============================================================================
// WIFI / MQTT
// =============================================================================

#if ENABLE_WIFI_MQTT
void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi connection FAILED — continuing without network.");
  }
}

void reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  static unsigned long lastAttempt = 0;
  unsigned long now = millis();
  if (now - lastAttempt < 5000) return;
  lastAttempt = now;

  Serial.print("Attempting MQTT connection...");
  if (mqttClient.connect(MQTT_CLIENT_ID)) {
    Serial.println("connected");
  } else {
    Serial.print("failed, rc=");
    Serial.println(mqttClient.state());
  }
}

void publishMQTT() {
  if (!mqttClient.connected()) return;

  String payload = "{";
  payload += "\"moisture\":" + String(moisturePercent, 1) + ",";
  payload += "\"temp\":" + String(ambientTempC, 1) + ",";
  payload += "\"pressure_pa\":" + String(ambientPressurePa, 0) + ",";
  payload += "\"decel_g\":" + String(filteredDecel_g, 2) + ",";
  payload += "\"boiling_point\":" + String(estimatedBoilingPointC, 0) + ",";
  payload += "\"risk_index\":" + String(riskIndex, 1) + ",";
  payload += "\"status\":\"" + String(stateToString(currentState)) + "\"";
  payload += "}";

  mqttClient.publish(MQTT_TOPIC_STATE, payload.c_str());
  Serial.println("MQTT published: " + payload);
}
#endif
