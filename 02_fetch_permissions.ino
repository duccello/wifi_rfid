/*
  Smart FabLab Access Terminal — Step 2
  WiFi + RC522 UID read + HTTP fetch of permissions from Google Apps Script

  New library required: "ArduinoJson" by Benoit Blanchon (Library Manager)

  Wiring: same as Step 1 (see 01_wifi_rfid_starter.ino)
*/

#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ---------- WiFi credentials ----------
const char* WIFI_SSID     = "FabLab_Public";
const char* WIFI_PASSWORD = "FabulousLab42!";

// ---------- Apps Script Web App deployment URL ----------
// Get this from Deploy > New deployment > Web app in Apps Script.
// It looks like: https://script.google.com/macros/s/XXXXXXXXXXXX/exec
const char* APPS_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbxff-9PTM9Zetw9YaKRcnv_E562R8PDri0ROorxsSGVwn9apE02DuG24WeMys0MqXWXbw/exec";

// ---------- RC522 pins ----------
#define RST_PIN 22
#define SS_PIN   5

MFRC522 rfid(SS_PIN, RST_PIN);

// ---------- Member data holder ----------
struct MemberInfo {
  bool found = false;
  String uid = "";
  String name = "";
  bool onboarding = false;
  bool laser = false;
  bool cnc = false;
  bool bambu = false;
};

void setup() {
  Serial.begin(115200);
  delay(500);

  SPI.begin();
  rfid.PCD_Init();
  delay(50);

  Serial.print("RC522 firmware version: 0x");
  byte version = rfid.PCD_ReadRegister(rfid.VersionReg);
  Serial.println(version, HEX);

  connectToWiFi();

  Serial.println("Ready. Tap an NFC tag...");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi dropped, reconnecting...");
    connectToWiFi();
  }

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  String uidString = getUidString(rfid.uid.uidByte, rfid.uid.size);
  Serial.print("Tag detected. UID: ");
  Serial.println(uidString);

  MemberInfo member = fetchMemberInfo(uidString);
  printMemberInfo(member);

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  delay(1000); // debounce
}

// ---------- HTTP fetch + parse ----------

MemberInfo fetchMemberInfo(const String &uid) {
  MemberInfo result;
  result.uid = uid;

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot fetch: WiFi not connected.");
    return result;
  }

  HTTPClient http;
  String url = String(APPS_SCRIPT_URL) + "?uid=" + urlEncode(uid);

  http.begin(url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); // Apps Script responses 302 before landing on final URL
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.print("HTTP request failed, code: ");
    Serial.println(httpCode);
    http.end();
    return result;
  }

  String payload = http.getString();
  http.end();

  Serial.println(payload);
  JsonDocument doc; // ArduinoJson v7 style
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.print("JSON parse failed: ");
    Serial.println(err.c_str());
    return result;
  }

  result.found = doc["found"] | false;
  if (result.found) {
    result.name       = String((const char*)(doc["name"] | ""));
    result.onboarding = doc["onboarding"] | false;
    result.laser      = doc["laser"] | false;
    result.cnc        = doc["cnc"] | false;
    result.bambu      = doc["bambu"] | false;
  }

  return result;
}

void printMemberInfo(const MemberInfo &m) {
  if (!m.found) {
    Serial.println("=> UID not registered. New/unassigned tag.");
    return;
  }
  Serial.println("=> Member found:");
  Serial.println("   Name: " + m.name);
  Serial.println("   Onboarded: " + String(m.onboarding ? "YES" : "NO"));
  Serial.println("   Laser: "    + String(m.laser ? "OK" : "NO"));
  Serial.println("   CNC: "      + String(m.cnc ? "OK" : "NO"));
  Serial.println("   Bambu H2C: "+ String(m.bambu ? "OK" : "NO"));
}

// ---------- Helpers ----------

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected. IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connection failed (will retry in loop).");
  }
}

String getUidString(byte *buffer, byte bufferSize) {
  String result = "";
  for (byte i = 0; i < bufferSize; i++) {
    if (buffer[i] < 0x10) result += "0";
    result += String(buffer[i], HEX);
    if (i < bufferSize - 1) result += ":";
  }
  result.toUpperCase();
  return result;
}

// Minimal URL-encoder for the UID query param (handles ':' etc.)
String urlEncode(const String &str) {
  String encoded = "";
  char c;
  char buf[4];
  for (unsigned int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c)) {
      encoded += c;
    } else {
      sprintf(buf, "%%%02X", c);
      encoded += buf;
    }
  }
  return encoded;
}
