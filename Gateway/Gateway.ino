#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <WiFi.h>
#include <FirebaseESP32.h>

// ----------------- OLED -----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define I2C_ADDRESS 0x3C
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ----------------- BUTTONS -----------------
#define BTN_PAGE_PIN 27
#define BTN_PUMP_PIN 33

// ----------------- LoRa -----------------
#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_SS     5
#define LORA_RST   14
#define LORA_DIO0  26

#define LORA_BAND 433E6

// ----------------- WiFi -----------------
#define WIFI_SSID     "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// ----------------- Firebase -----------------

#define FIREBASE_HOST "YOUR_FIREBASE_HOST"
#define FIREBASE_AUTH "YOUR_FIREBASE_SECRET"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ----------------- Mesh IDs -----------------
const uint8_t ID_CENTRAL = 1;
const uint8_t ID_SOIL    = 2;
const uint8_t ID_CLIMATE = 3;

// ----------------- State -----------------
uint16_t seqCounter = 1;

// last received raw strings
String soilStr = "SOIL=?";
String flowStr = "FLOW=?";
String pumpStr = "PUMP=?";
String tempStr = "T=?";
String humStr  = "H=?";
String ldrStr  = "LDR=?";

// parsed values
int soilVal = 0;
int flowVal = 0;
int pumpVal = 0;
float tempVal = 0.0;
float humVal = 0.0;
int ldrVal = 0;

// last heard timestamps
unsigned long lastHeard2 = 0;
unsigned long lastHeard3 = 0;

// firebase timing
unsigned long lastFirebaseMeta = 0;
unsigned long lastPumpCheck = 0;
bool firebaseReady = false;

// ack waiting
struct Pending {
  bool active = false;
  uint16_t seq = 0;
  uint8_t dst = 0;
  uint8_t via = 0;
  unsigned long t0 = 0;
  uint8_t retries = 0;
  String type;
  String payload;
} pending;

// duplicate filtering
uint16_t lastSeqFrom2 = 0;
uint16_t lastSeqFrom3 = 0;

// display page
uint8_t displayPage = 0; // 0=soil, 1=climate

// debounce
int lastPageRead = HIGH;
int stablePage = HIGH;
unsigned long lastPageChangeMs = 0;
const unsigned long debounceMs = 40;

int lastPumpRead = HIGH;
int stablePump = HIGH;
unsigned long lastPumpChangeMs = 0;

// manual pump state
bool manualPumpState = false;

// polling
unsigned long lastPoll = 0;
uint8_t pollStep = 0;

// =====================================================
// Utility
// =====================================================

String makePacket(uint8_t src, uint8_t dst, uint8_t via, uint8_t ttl, uint16_t seq, const String& type, const String& payload) {
  return "S," + String(src) + "," + String(dst) + "," + String(via) + "," + String(ttl) + "," + String(seq) + "," + type + "," + payload;
}

bool parsePacket(const String& s,
                 uint8_t &src, uint8_t &dst, uint8_t &via, uint8_t &ttl, uint16_t &seq,
                 String &type, String &payload) {
  if (!s.startsWith("S,")) return false;

  int idx[8];
  int c = 0;
  idx[c++] = 0;
  for (int i = 0; i < (int)s.length() && c < 8; i++) {
    if (s[i] == ',') idx[c++] = i + 1;
  }
  if (c < 7) return false;

  auto getField = [&](int field)->String {
    int start = idx[field];
    int end = (field == 7) ? s.length() : s.indexOf(',', start);
    if (end < 0) end = s.length();
    return s.substring(start, end);
  };

  src = (uint8_t)getField(1).toInt();
  dst = (uint8_t)getField(2).toInt();
  via = (uint8_t)getField(3).toInt();
  ttl = (uint8_t)getField(4).toInt();
  seq = (uint16_t)getField(5).toInt();
  type = getField(6);
  payload = (idx[7] <= (int)s.length()) ? s.substring(idx[7]) : "";
  return true;
}

void loraSend(const String& p) {
  LoRa.beginPacket();
  LoRa.print(p);
  LoRa.endPacket();
  Serial.print("TX: ");
  Serial.println(p);
}

void startPending(uint8_t dst, uint8_t via, const String& type, const String& payload) {
  pending.active = true;
  pending.seq = seqCounter++;
  pending.dst = dst;
  pending.via = via;
  pending.t0 = millis();
  pending.retries = 0;
  pending.type = type;
  pending.payload = payload;

  String pkt = makePacket(ID_CENTRAL, dst, via, 3, pending.seq, type, payload);
  loraSend(pkt);
}

void resendPending() {
  pending.retries++;
  pending.t0 = millis();
  String pkt = makePacket(ID_CENTRAL, pending.dst, pending.via, 3, pending.seq, pending.type, pending.payload);
  loraSend(pkt);
}

void clearPending() {
  pending.active = false;
}

void sendAck(uint8_t dst, uint16_t seq, uint8_t via) {
  String pkt = makePacket(ID_CENTRAL, dst, via, 3, seq, "ACK", "");
  loraSend(pkt);
}

// =====================================================
// WiFi / Firebase
// =====================================================

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting WiFi");
  unsigned long t0 = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi failed, continuing without cloud");
  }
}

void initFirebase() {
  if (WiFi.status() != WL_CONNECTED) {
    firebaseReady = false;
    return;
  }

  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  firebaseReady = true;
  Serial.println("Firebase initialized");
}

bool cloudOK() {
  return firebaseReady && WiFi.status() == WL_CONNECTED;
}

void uploadSoilNode() {
  if (!cloudOK()) return;

  Firebase.setInt(fbdo, "/sensors/soilMoisture", soilVal);
  Firebase.setInt(fbdo, "/sensors/flowRate", flowVal);
  Firebase.setInt(fbdo, "/sensors/pump", pumpVal);

  String ts = String(millis());
  Firebase.setInt(fbdo, "/history/soilMoisture/" + ts, soilVal);
  Firebase.setInt(fbdo, "/history/flowRate/" + ts, flowVal);

  Firebase.setInt(fbdo, "/meta/node2/lastSeenMs", lastHeard2);
}

void uploadClimateNode() {
  if (!cloudOK()) return;

  Firebase.setFloat(fbdo, "/sensors/temperature", tempVal);
  Firebase.setFloat(fbdo, "/sensors/humidity", humVal);
  Firebase.setInt(fbdo, "/sensors/light", ldrVal);

  String ts = String(millis());
  Firebase.setFloat(fbdo, "/history/temperature/" + ts, tempVal);
  Firebase.setFloat(fbdo, "/history/humidity/" + ts, humVal);
  Firebase.setInt(fbdo, "/history/light/" + ts, ldrVal);

  Firebase.setInt(fbdo, "/meta/node3/lastSeenMs", lastHeard3);
}

void updateGatewayStatus() {
  if (!cloudOK()) return;

  Firebase.setInt(fbdo, "/meta/gateway/lastSeenMs", millis());
  Firebase.setInt(fbdo, "/meta/gateway/rssi", WiFi.RSSI());
  Firebase.setInt(fbdo, "/meta/node2/lastSeenMs", lastHeard2);
  Firebase.setInt(fbdo, "/meta/node3/lastSeenMs", lastHeard3);
}

void syncPumpStateToFirebase() {
  if (!cloudOK()) return;
  Firebase.setBool(fbdo, "/pump/state", manualPumpState);
}

void checkPumpCommand() {
  if (!cloudOK()) return;

  if (Firebase.getBool(fbdo, "/pump/state")) {
    bool firebasePump = fbdo.boolData();

    if (firebasePump != manualPumpState && !pending.active) {
      manualPumpState = firebasePump;

      String payload = String("PUMP=") + (manualPumpState ? "1" : "0");
      startPending(ID_SOIL, 0, "CMD", payload);

      Serial.print("Pump command from Firebase: ");
      Serial.println(manualPumpState ? "ON" : "OFF");

      updateDisplay();
    }
  } else {
    Serial.print("Firebase get /pump/state failed: ");
    Serial.println(fbdo.errorReason());
  }
}

// =====================================================
// Display
// =====================================================

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);

  if (displayPage == 0) {
    display.println("VIEW: NODE 2 (SOIL)");
    display.println("--------------------");
    display.println(soilStr);
    display.println(flowStr);
    display.println(pumpStr);
    display.println();
    display.print("Link2: ");
    display.println((millis() - lastHeard2 < 15000) ? "OK" : "LOST");
    display.print("ManualPump: ");
    display.println(manualPumpState ? "ON" : "OFF");
  } else {
    display.println("VIEW: NODE 3 (CLIM)");
    display.println("--------------------");
    display.println(tempStr);
    display.println(humStr);
    display.println(ldrStr);
    display.println();
    display.print("Link3: ");
    display.println((millis() - lastHeard3 < 15000) ? "OK" : "LOST");
  }

  display.display();
}

// =====================================================
// Data handling
// =====================================================

void handleDataFrom(uint8_t src, const String& payload) {
  Serial.print("DATA from ");
  Serial.print(src);
  Serial.print(" -> ");
  Serial.println(payload);

  if (src == ID_SOIL) {
    lastHeard2 = millis();

    int a = payload.indexOf("SOIL=");
    int b = payload.indexOf("FLOW=");
    int c = payload.indexOf("PUMP=");

    if (a >= 0) {
      int end = payload.indexOf(';', a);
      if (end < 0) end = payload.length();
      soilStr = payload.substring(a, end);
      soilVal = soilStr.substring(5).toInt();
    }

    if (b >= 0) {
      int end = payload.indexOf(';', b);
      if (end < 0) end = payload.length();
      flowStr = payload.substring(b, end);
      flowVal = flowStr.substring(5).toInt();
    }

    if (c >= 0) {
      int end = payload.indexOf(';', c);
      if (end < 0) end = payload.length();
      pumpStr = payload.substring(c, end);
      pumpVal = pumpStr.substring(5).toInt();
    }

    uploadSoilNode();
  }

  else if (src == ID_CLIMATE) {
    lastHeard3 = millis();

    int t = payload.indexOf("T=");
    int h = payload.indexOf("H=");
    int l = payload.indexOf("LDR=");

    if (t >= 0) {
      int end = payload.indexOf(';', t);
      if (end < 0) end = payload.length();
      tempStr = payload.substring(t, end);
      tempVal = tempStr.substring(2).toFloat();
    }

    if (h >= 0) {
      int end = payload.indexOf(';', h);
      if (end < 0) end = payload.length();
      humStr = payload.substring(h, end);
      humVal = humStr.substring(2).toFloat();
    }

    if (l >= 0) {
      int end = payload.indexOf(';', l);
      if (end < 0) end = payload.length();
      ldrStr = payload.substring(l, end);
      ldrVal = ldrStr.substring(4).toInt();
    }

    uploadClimateNode();
  }
}

// =====================================================
// Buttons
// =====================================================

void handlePageButton() {
  int r = digitalRead(BTN_PAGE_PIN);

  if (r != lastPageRead) {
    lastPageRead = r;
    lastPageChangeMs = millis();
  }

  if (millis() - lastPageChangeMs > debounceMs) {
    if (stablePage != r) {
      stablePage = r;

      if (stablePage == LOW) {
        displayPage ^= 1;
        updateDisplay();
      }
    }
  }
}

void handlePumpButton() {
  int r = digitalRead(BTN_PUMP_PIN);

  if (r != lastPumpRead) {
    lastPumpRead = r;
    lastPumpChangeMs = millis();
  }

  if (millis() - lastPumpChangeMs > debounceMs) {
    if (stablePump != r) {
      stablePump = r;

      if (stablePump == LOW) {
        if (!pending.active) {
          manualPumpState = !manualPumpState;
          syncPumpStateToFirebase();

          String payload = String("PUMP=") + (manualPumpState ? "1" : "0");
          startPending(ID_SOIL, 0, "CMD", payload);

          updateDisplay();
        } else {
          Serial.println("Busy (waiting ACK)");
        }
      }
    }
  }
}

// =====================================================
// Setup
// =====================================================

void setup() {
  Serial.begin(115200);

  pinMode(BTN_PAGE_PIN, INPUT_PULLUP);
  pinMode(BTN_PUMP_PIN, INPUT_PULLUP);

  // OLED
  Wire.begin();
  if (!display.begin(I2C_ADDRESS)) {
    Serial.println("OLED init failed");
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println("Central starting...");
  display.println("GPIO27: Page");
  display.println("GPIO33: Pump");
  display.display();

  // LoRa
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_BAND)) {
    Serial.println("LoRa begin failed");
    while (true) delay(1000);
  }

  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.enableCrc();

  Serial.println("LoRa ready");

  // WiFi + Firebase
  connectWiFi();
  initFirebase();

  if (cloudOK()) {
    Firebase.setBool(fbdo, "/pump/state", manualPumpState);
  }

  updateDisplay();
}

// =====================================================
// Loop
// =====================================================

void loop() {
  handlePageButton();
  handlePumpButton();

  // -------- Receive --------
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String msg;
    while (LoRa.available()) msg += (char)LoRa.read();

    Serial.print("RX: ");
    Serial.println(msg);

    uint8_t src, dst, via, ttl;
    uint16_t seq;
    String type, payload;

    if (parsePacket(msg, src, dst, via, ttl, seq, type, payload)) {
      if (dst == ID_CENTRAL) {

        if (src == ID_SOIL) {
          if (seq == lastSeqFrom2) goto done_rx;
          lastSeqFrom2 = seq;
        } else if (src == ID_CLIMATE) {
          if (seq == lastSeqFrom3) goto done_rx;
          lastSeqFrom3 = seq;
        }

        if (type == "ACK") {
          if (pending.active && seq == pending.seq && src == pending.dst) {
            clearPending();
          }
        }

        if (type == "DATA") {
          handleDataFrom(src, payload);
          updateDisplay();
          sendAck(src, seq, 0);
        }
      }
    } else {
      Serial.println("Packet parse failed");
    }

done_rx:
    ;
  }

  // -------- Poll Nodes --------
  if (!pending.active && millis() - lastPoll > 3000) {
    lastPoll = millis();
    pollStep ^= 1;

    if (pollStep == 0) {
      startPending(ID_SOIL, 0, "PING", "REQ=DATA");
    } else {
      startPending(ID_CLIMATE, 0, "PING", "REQ=DATA");
    }
  }

  // -------- Retry / failover --------
  if (pending.active) {
    if (millis() - pending.t0 > 800) {
      if (pending.retries < 2) {
        resendPending();
      } else {
        uint8_t relay = (pending.dst == ID_SOIL) ? ID_CLIMATE : ID_SOIL;
        bool relayAlive = (relay == ID_SOIL)
          ? (millis() - lastHeard2 < 15000)
          : (millis() - lastHeard3 < 15000);

        if (relayAlive && pending.via == 0) {
          pending.via = relay;
          pending.retries = 0;
          resendPending();
        } else {
          clearPending();
        }
      }
    }
  }

  // -------- Serial control --------
  if (Serial.available() && !pending.active) {
    char c = Serial.read();
    if (c == '1' || c == '0') {
      manualPumpState = (c == '1');
      syncPumpStateToFirebase();

      String payload = String("PUMP=") + (manualPumpState ? "1" : "0");
      startPending(ID_SOIL, 0, "CMD", payload);
      updateDisplay();
    }
  }

  // -------- Meta update --------
  if (millis() - lastFirebaseMeta > 5000) {
    lastFirebaseMeta = millis();
    updateGatewayStatus();
  }

  // -------- Firebase pump command --------
  if (millis() - lastPumpCheck > 2000) {
    lastPumpCheck = millis();
    checkPumpCommand();
  }
}