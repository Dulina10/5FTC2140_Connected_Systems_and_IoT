#include <SPI.h>
#include <LoRa.h>
#include <DHT.h>

// -------- DHT11 --------
#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// -------- LDR --------
#define LDR_PIN A0

// -------- LoRa  --------

#define LORA_SS   10
#define LORA_RST  9
#define LORA_DIO0 3  

#define LORA_BAND 433E6

// -------- Node IDs --------
const uint8_t ID_CENTRAL = 1;
const uint8_t ID_SOIL    = 2;
const uint8_t ID_CLIMATE = 3;

uint16_t lastSeqFrom1 = 0;

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
}

void sendAck(uint8_t dst, uint16_t seq) {
  String pkt = makePacket(ID_CLIMATE, dst, 0, 3, seq, "ACK", "");
  loraSend(pkt);
}

void sendData(uint8_t dst, uint16_t seq) {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  int ldr = analogRead(LDR_PIN);

  // DHT can fail; protect it
  String payload;
  if (isnan(t) || isnan(h)) {
    payload = "T=NA;H=NA;LDR=" + String(ldr);
  } else {
    payload = "T=" + String(t, 1) + ";H=" + String(h, 0) + ";LDR=" + String(ldr);
  }

  String pkt = makePacket(ID_CLIMATE, dst, 0, 3, seq, "DATA", payload);
  loraSend(pkt);
}

void relayForward(uint8_t src, uint8_t dst, uint8_t via, uint8_t ttl, uint16_t seq, const String& type, const String& payload) {
  if (ttl == 0) return;
  uint8_t newTtl = ttl - 1;
  String pkt = makePacket(src, dst, 0, newTtl, seq, type, payload);
  loraSend(pkt);
}

void setup() {
  Serial.begin(9600);
  dht.begin();

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_BAND)) {
    Serial.println("LoRa begin failed");
    while (1) {}
  }
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.enableCrc();
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (!packetSize) return;

  String msg;
  while (LoRa.available()) msg += (char)LoRa.read();

  uint8_t src, dst, via, ttl;
  uint16_t seq;
  String type, payload;

  if (!parsePacket(msg, src, dst, via, ttl, seq, type, payload)) return;

  // -------- Relay behavior --------
  if (via == ID_CLIMATE && dst != ID_CLIMATE) {
    relayForward(src, dst, via, ttl, seq, type, payload);
    return;
  }

  if (dst != ID_CLIMATE) return;

  // duplicate filter from central
  if (src == ID_CENTRAL) {
    if (seq == lastSeqFrom1) return;
    lastSeqFrom1 = seq;
  }

  if (type == "PING") {
    if (payload.indexOf("REQ=DATA") >= 0) {
      sendData(src, seq);
    }
    sendAck(src, seq);
  }
}