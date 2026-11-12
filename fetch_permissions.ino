/*
  Smart FabLab Access Terminal — Step 5
  Local member cache: fetches the full member list from Apps Script
  periodically (and once at boot) instead of per-tag-scan.

  Tag scans now do an instant LOCAL lookup -> no HTTP wait, and scans
  still work even if WiFi/Apps Script is briefly slow or unreachable
  (as long as a cache refresh has succeeded at least once since boot).

  Wiring: same as Step 4 (RC522 + LCD + 2 buttons, see 04_lcd_buttons_permissions.ino)
  Libraries required: MFRC522, ArduinoJson, LiquidCrystal (built-in)
*/

#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LiquidCrystal.h>

// ---------- WiFi credentials ----------
const char* WIFI_SSID     = "FabLab_Public";
const char* WIFI_PASSWORD = "FabulousLab42!";

// ---------- Apps Script Web App deployment URL ----------
const char* APPS_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbxUJGQ5MDAPxy8sVKXnYn48vs6k6Isa3TsmQqjEBrAV0Tm9yYAqf0J_kdTX4nozNi9-kw/exec";

// Must match REGISTER_TOKEN in the Apps Script
const char* REGISTER_TOKEN = "change-me-fablab";
const char* UNKNOWN_NAME   = "NEW USER";

// ---------- RC522 pins ----------
#define RST_PIN 22
#define SS_PIN   5
MFRC522 rfid(SS_PIN, RST_PIN);

// ---------- LCD pins (parallel, 4-bit mode) ----------
const int LCD_RS = 13, LCD_E = 14, LCD_D4 = 27, LCD_D5 = 26, LCD_D6 = 25, LCD_D7 = 33;
LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// ---------- Button pins ----------
const int BTN_NEXT = 16;
const int BTN_PREV = 17;
const unsigned long DEBOUNCE_MS = 50;

// ---------- Timing ----------
const unsigned long IDLE_TIMEOUT_MS   = 15000;
const unsigned long REFRESH_INTERVAL_MS = 5UL * 60UL * 1000UL; // 5 minutes, once cache is healthy
const unsigned long RETRY_INTERVAL_MS   = 15UL * 1000UL;       // 15s, while cache has never succeeded
unsigned long lastActivity = 0;
unsigned long lastRefreshAttempt = 0;
unsigned long lastSuccessfulRefresh = 0; // 0 = never succeeded

// ---------- Local member cache ----------
#define MAX_MEMBERS 60 // bump if your club roster is bigger

struct MemberRecord {
  String uid;
  String name;
  bool onboarding;
  bool laser;
  bool cnc;
  bool bambu;
};

MemberRecord cache[MAX_MEMBERS];
int cacheCount = 0;

// ---------- State ----------
enum TerminalState { STATE_IDLE, STATE_ACTIVE };
TerminalState state = STATE_IDLE;
int currentPage = 0;
bool needsRedraw = true;

struct ActiveMember {
  bool found = false;
  String uid = "";
  String name = "";
  bool onboarding = false;
  bool laser = false;
  bool cnc = false;
  bool bambu = false;
};
ActiveMember member;

// Debounce tracking
bool lastNextReading = HIGH, lastPrevReading = HIGH;
unsigned long lastNextChange = 0, lastPrevChange = 0;
bool nextStable = HIGH, prevStable = HIGH;

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(BTN_NEXT, INPUT_PULLUP);
  pinMode(BTN_PREV, INPUT_PULLUP);

  SPI.begin();
  rfid.PCD_Init();

  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.print("Starting up...");

  connectToWiFi();
  refreshCache(); // initial load so the terminal is usable immediately

  showIdleScreen();
  Serial.println("Ready. Tap an NFC tag...");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  // Periodic background refresh (non-blocking w.r.t. tag scans --
  // only runs between loop() iterations, scans are still read every loop).
  // Retry quickly if we've never had a successful refresh yet; once healthy,
  // settle into the normal (slower) refresh cadence.
  unsigned long currentInterval = (lastSuccessfulRefresh == 0) ? RETRY_INTERVAL_MS : REFRESH_INTERVAL_MS;
  if (millis() - lastRefreshAttempt > currentInterval) {
    refreshCache();
  }

  checkForTag();
  checkButtons();

  if (state == STATE_ACTIVE && millis() - lastActivity > IDLE_TIMEOUT_MS) {
    state = STATE_IDLE;
    needsRedraw = true;
  }

  if (needsRedraw) {
    redraw();
    needsRedraw = false;
  }
}

// ---------- Cache refresh (bulk fetch) ----------

void refreshCache() {
  lastRefreshAttempt = millis();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cache refresh skipped: WiFi not connected.");
    return;
  }

  HTTPClient http;
  String url = String(APPS_SCRIPT_URL) + "?mode=all";
  http.begin(url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000); // Apps Script + redirect hop can be slow, especially on a cold start

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.print("Cache refresh failed, HTTP code: ");
    Serial.print(httpCode);
    Serial.print(" (");
    Serial.print(httpCode < 0 ? HTTPClient::errorToString(httpCode) : "HTTP status error");
    Serial.println(")");
    http.end();
    return; // will retry automatically -- see RETRY_INTERVAL_MS in loop()
  }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.print("Cache refresh JSON parse failed: ");
    Serial.println(err.c_str());
    return;
  }

  JsonArray members = doc["members"].as<JsonArray>();
  int count = 0;
  for (JsonObject m : members) {
    if (count >= MAX_MEMBERS) {
      Serial.println("Warning: member list truncated, increase MAX_MEMBERS.");
      break;
    }
    cache[count].uid        = String((const char*)(m["uid"] | ""));
    cache[count].name       = String((const char*)(m["name"] | ""));
    cache[count].onboarding = m["onboarding"] | false;
    cache[count].laser      = m["laser"] | false;
    cache[count].cnc        = m["cnc"] | false;
    cache[count].bambu      = m["bambu"] | false;
    count++;
  }

  cacheCount = count;
  lastSuccessfulRefresh = millis();
  Serial.print("Cache refreshed. Members loaded: ");
  Serial.println(cacheCount);
}

// Local lookup -- instant, no network involved
ActiveMember lookupMember(const String &uid) {
  ActiveMember result;
  result.uid = uid;

  for (int i = 0; i < cacheCount; i++) {
    if (cache[i].uid == uid) {
      result.found      = true;
      result.name       = cache[i].name;
      result.onboarding = cache[i].onboarding;
      result.laser      = cache[i].laser;
      result.cnc        = cache[i].cnc;
      result.bambu      = cache[i].bambu;
      return result;
    }
  }
  return result; // found stays false
}

// Asks Apps Script to append a placeholder row (name "NEW USER", all FALSE)
// for an unknown UID. On success, also adds it to the local cache so
// tapping the same tag again doesn't trigger another registration.
bool registerUnknownTag(const String &uid) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Register skipped: WiFi not connected.");
    return false;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Registering...");

  HTTPClient http;
  String url = String(APPS_SCRIPT_URL) + "?mode=register&uid=" + urlEncode(uid) +
               "&token=" + urlEncode(String(REGISTER_TOKEN));
  http.begin(url);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.print("Register failed, HTTP code: ");
    Serial.println(httpCode);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    Serial.println("Register failed: bad JSON response.");
    return false;
  }
  if (!(doc["ok"] | false)) {
    Serial.println("Register rejected by Apps Script (check token).");
    return false;
  }

  // Mirror the new row into the local cache
  if (cacheCount < MAX_MEMBERS) {
    cache[cacheCount].uid        = uid;
    cache[cacheCount].name       = UNKNOWN_NAME;
    cache[cacheCount].onboarding = false;
    cache[cacheCount].laser      = false;
    cache[cacheCount].cnc        = false;
    cache[cacheCount].bambu      = false;
    cacheCount++;
  }

  Serial.print("Registered new tag in sheet: ");
  Serial.println(uid);
  return true;
}

// ---------- NFC ----------

void checkForTag() {
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  String uidString = getUidString(rfid.uid.uidByte, rfid.uid.size);
  Serial.print("Tag detected. UID: ");
  Serial.println(uidString);

  member = lookupMember(uidString); // local lookup, no HTTP wait

  // Unknown tag -> add a placeholder row to the sheet for an admin to complete
  if (!member.found && lastSuccessfulRefresh != 0) {
    if (registerUnknownTag(uidString)) {
      member = lookupMember(uidString); // now found, as "NEW USER" with all permissions NO
    }
  }

  state = STATE_ACTIVE;
  currentPage = 1;
  lastActivity = millis();
  needsRedraw = true;

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// ---------- Buttons ----------

void checkButtons() {
  bool nextReading = digitalRead(BTN_NEXT);
  bool prevReading = digitalRead(BTN_PREV);

  if (nextReading != lastNextReading) lastNextChange = millis();
  if (prevReading != lastPrevReading) lastPrevChange = millis();

  if (millis() - lastNextChange > DEBOUNCE_MS && nextReading != nextStable) {
    nextStable = nextReading;
    if (nextStable == LOW) onNextPressed();
  }
  if (millis() - lastPrevChange > DEBOUNCE_MS && prevReading != prevStable) {
    prevStable = prevReading;
    if (prevStable == LOW) onPrevPressed();
  }

  lastNextReading = nextReading;
  lastPrevReading = prevReading;
}

void onNextPressed() {
  if (state != STATE_ACTIVE) return;
  currentPage = (currentPage + 1) % 4;
  lastActivity = millis();
  needsRedraw = true;
}

void onPrevPressed() {
  if (state != STATE_ACTIVE) return;
  currentPage = (currentPage + 3) % 4;
  lastActivity = millis();
  needsRedraw = true;
}

// ---------- Display ----------

void redraw() {
  if (state == STATE_IDLE) {
    showIdleScreen();
    return;
  }
  showPage(currentPage);
}

void showIdleScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Tap Tag to Scan");
  // Small staleness indicator on line 2 if cache has never loaded
  if (lastSuccessfulRefresh == 0) {
    lcd.setCursor(0, 1);
    lcd.print("Cache: none yet");
  }
}

void showPage(int page) {
  lcd.clear();

  if (!member.found && page != 0) {
    lcd.setCursor(0, 0);
    lcd.print("Not registered");
    lcd.setCursor(0, 1);
    lcd.print("See an admin");
    return;
  }

  switch (page) {
    case 0: {
      String uid = member.uid;
      lcd.setCursor(0, 0);
      lcd.print(uid.substring(0, min((int)uid.length(), 16)));
      if (uid.length() > 16) {
        lcd.setCursor(0, 1);
        lcd.print(uid.substring(16));
      }
      break;
    }
    case 1: {
      lcd.setCursor(0, 0);
      lcd.print(member.name.substring(0, min((int)member.name.length(), 16)));
      lcd.setCursor(0, 1);
      lcd.print("Onboard: ");
      lcd.print(member.onboarding ? "YES" : "NO");
      break;
    }
    case 2: {
      lcd.setCursor(0, 0);
      lcd.print("Laser: ");
      lcd.print(member.laser ? "OK" : "NO");
      lcd.setCursor(0, 1);
      lcd.print("CNC: ");
      lcd.print(member.cnc ? "OK" : "NO");
      break;
    }
    case 3: {
      lcd.setCursor(0, 0);
      lcd.print("Bambu H2C:");
      lcd.setCursor(0, 1);
      lcd.print(member.bambu ? "OK" : "NO");
      break;
    }
  }
}

// ---------- Helpers ----------

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    delay(300);
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

// Minimal URL-encoder for query params (turns ':' into %3A, etc.)
String urlEncode(const String &str) {
  String encoded = "";
  char buf[4];
  for (unsigned int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (isalnum(c)) {
      encoded += c;
    } else {
      sprintf(buf, "%%%02X", c);
      encoded += buf;
    }
  }
  return encoded;
}
