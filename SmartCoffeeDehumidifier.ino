// #include <Adafruit_SHT31.h>
#include <DHT.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <Wire.h>
#include <time.h>        // NTP — bawaan ESP32 SDK, tidak perlu install

// HTML halaman utama disimpan di PROGMEM (flash), bukan SPIFFS
#include "html_page.h"

// ================= WIFI =================

#define WIFI_CONFIG_FILE "/wifi.cfg"

// Default — akan di-override saat boot dari SPIFFS
String staSSID = "harun";
String staPass  = "harun3211";

const char *AP_SSID     = "CoffeeDehumidifier";
const char *AP_PASSWORD = "12345678";

// ================= PIN =================

#define RELAY_PIN 18
#define BUZZER_PIN 19

#define SDA_PIN 21
#define SCL_PIN 22

// ================= LOG =================

#define LOG_FILE "/log.csv"
#define LOG_MAX_KB 64 // rotate setelah ~64 KB
#define SETTINGS_FILE "/settings.json"

// ================= OBJECT =================

AsyncWebServer server(80);

// Adafruit_SHT31 sht31 = Adafruit_SHT31();

RTC_DS3231 rtc;

LiquidCrystal_I2C lcd(0x27, 16, 2);

#define DHT_PIN  23        // Ganti sesuai pin yang dipakai
#define DHT_TYPE DHT22

DHT dht(DHT_PIN, DHT_TYPE);
// ================= STATE =================

float humidity = 0;
float temperature = 0;

bool fanState = false;
bool autoMode = true;

float minHum = 65;
float maxHum = 80;

int schedStart = 7 * 60;
int schedEnd   = 18 * 60;

// LCD multi-page
const int LCD_PAGES      = 5;
int  lcdPage             = 0;
int  lcdInterval         = 3;  // detik per halaman (1-30)
unsigned long lastPageSwitch = 0;

// Log count untuk auto-update frontend
int logCount = 0;

unsigned long lastSensor = 0;
unsigned long lastLCD    = 0;

// Buzzer debounce — mencegah bunyi berulang terus-menerus
bool sensorErrorBeeped   = false;  // sudah beep untuk sensor error?
bool criticalHumBeeped   = false;  // sudah beep untuk humidity kritis?
#define CRITICAL_HUM_THRESHOLD 90.0f

// [Fix #12] default "INIT" agar tidak misleading sebelum WiFi selesai
String wifiMode = "INIT";
String ipAddress = "0.0.0.0";

// LCD shadow — untuk update tanpa flicker
String lcdRow0_prev = "";
String lcdRow1_prev = "";

// ================= NTP / TIMEZONE =================

// WIB = UTC+7 (tidak ada DST)
#define TZ_OFFSET_SEC  (7L * 3600L)
#define NTP_SERVER1    "pool.ntp.org"
#define NTP_SERVER2    "time.nist.gov"


// ================= RTC HELPER =================

// Parse __DATE__ ("Jun  3 2026") dan __TIME__ ("14:22:05")
// lalu tambahkan offset WIB (UTC+7) sebelum tulis ke DS3231.
// Dipanggil saat lostPower() = true (RTC belum pernah di-set / baterai habis).
void adjustRtcFromBuild() {
  // Parse bulan dari nama 3-huruf
  const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char monStr[4];
  int day, year, hour, minute, second;

  sscanf(__DATE__, "%3s %d %d", monStr, &day, &year);
  sscanf(__TIME__, "%d:%d:%d",  &hour, &minute, &second);

  int month = 1;
  for (int i = 0; i < 12; i++) {
    if (strncmp(monStr, months + i * 3, 3) == 0) {
      month = i + 1;
      break;
    }
  }

  // Tambahkan offset WIB = +7 jam
  // Gunakan DateTime arithmetic agar overflow hari/bulan ditangani otomatis
  DateTime buildTime(year, month, day, hour, minute, second);
  DateTime wibTime = buildTime + TimeSpan(0, 7, 0, 0); // +7 jam

  rtc.adjust(wibTime);

  Serial.printf("[RTC] Build time: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                year, month, day, hour, minute, second);
  Serial.printf("[RTC] Adjusted  : %04d-%02d-%02d %02d:%02d:%02d WIB\n",
                wibTime.year(), wibTime.month(),  wibTime.day(),
                wibTime.hour(), wibTime.minute(), wibTime.second());
}
// Sync NTP → tulis ke DS3231
// Dipanggil setelah WiFi STA berhasil konek.
void syncNTP() {
  Serial.println("[NTP] Syncing time (WIB UTC+7)...");
  configTime(TZ_OFFSET_SEC, 0, NTP_SERVER1, NTP_SERVER2);

  // Tunggu hingga NTP berhasil (maks 10 detik)
  struct tm timeinfo;
  unsigned long t0 = millis();
  while (!getLocalTime(&timeinfo) && millis() - t0 < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (!getLocalTime(&timeinfo)) {
    Serial.println("\n[NTP] Sync GAGAL — RTC tidak diubah");
    return;
  }

  // Tulis waktu NTP ke modul DS3231
  rtc.adjust(DateTime(
    timeinfo.tm_year + 1900,
    timeinfo.tm_mon  + 1,
    timeinfo.tm_mday,
    timeinfo.tm_hour,
    timeinfo.tm_min,
    timeinfo.tm_sec
  ));

  Serial.printf("\n[NTP] Sync OK: %04d-%02d-%02d %02d:%02d:%02d WIB\n",
    timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

// ================= HELPER =================

String getTimeStr() {
  DateTime now = rtc.now();
  char buf[10];
  sprintf(buf, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  return String(buf);
}

String getDateStr() {
  DateTime now = rtc.now();
  char buf[12];
  sprintf(buf, "%04d-%02d-%02d", now.year(), now.month(), now.day());
  return String(buf);
}

bool isScheduleActive() {
  DateTime now = rtc.now();
  int nowMin = now.hour() * 60 + now.minute();
  if (schedStart <= schedEnd) {
    return nowMin >= schedStart && nowMin < schedEnd;
  }
  // overnight schedule
  return nowMin >= schedStart || nowMin < schedEnd;
}

void setFan(bool on) {
  bool changed = (on != fanState);
  fanState = on;
  digitalWrite(RELAY_PIN, on ? LOW : HIGH);
  if (changed) {
    Serial.printf("[RELAY] Fan %s (hum=%.1f, auto=%s, sched=%s)\n",
                  on ? "ON" : "OFF", humidity,
                  autoMode ? "AUTO" : "MANUAL",
                  isScheduleActive() ? "ACTIVE" : "IDLE");
    // Buzzer: Fan ON = 1x pendek, Fan OFF = 2x pendek
    if (on) {
      beep(1, 150);
    } else {
      beep(2, 100);
    }
    writeLog(); // log hanya saat status berubah
  }
}

void beep(int t, int d) {
  for (int i = 0; i < t; i++) {
    digitalWrite(BUZZER_PIN, LOW);
    delay(d);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(d);
  }
}

// ================= WIFI CONFIG =================

void loadWifiConfig() {
  if (!SPIFFS.exists(WIFI_CONFIG_FILE)) return;
  File f = SPIFFS.open(WIFI_CONFIG_FILE, FILE_READ);
  if (!f) return;
  StaticJsonDocument<192> doc;
  if (!deserializeJson(doc, f)) {
    String s = doc["ssid"] | String("");
    String p = doc["pass"] | String("");
    if (s.length() > 0) staSSID = s;
    if (p.length() > 0) staPass  = p;
    Serial.println("WiFi config loaded: " + staSSID);
  }
  f.close();
}

void saveWifiConfig(const String &ssid, const String &pass) {
  File f = SPIFFS.open(WIFI_CONFIG_FILE, FILE_WRITE);
  if (!f) return;
  StaticJsonDocument<192> doc;
  doc["ssid"] = ssid;
  doc["pass"] = pass;
  serializeJson(doc, f);
  f.close();
}

// ================= SETTINGS PERSISTENCE =================

void loadSettings() {
  if (!SPIFFS.exists(SETTINGS_FILE)) return;
  File f = SPIFFS.open(SETTINGS_FILE, FILE_READ);
  if (!f) return;
  StaticJsonDocument<256> doc;
  if (!deserializeJson(doc, f)) {
    autoMode    = doc["auto"]         | true;
    minHum      = doc["min_hum"]      | 65.0f;
    maxHum      = doc["max_hum"]      | 80.0f;
    schedStart  = doc["sched_start"]  | (7 * 60);
    schedEnd    = doc["sched_end"]    | (18 * 60);
    lcdInterval = doc["lcd_interval"] | 3;
    if (lcdInterval < 1)  lcdInterval = 1;
    if (lcdInterval > 30) lcdInterval = 30;
  }
  f.close();
  Serial.println("Settings loaded.");
}

void saveSettings() {
  File f = SPIFFS.open(SETTINGS_FILE, FILE_WRITE);
  if (!f) return;
  StaticJsonDocument<256> doc;
  doc["auto"]         = autoMode;
  doc["min_hum"]      = minHum;
  doc["max_hum"]      = maxHum;
  doc["sched_start"]  = schedStart;
  doc["sched_end"]    = schedEnd;
  doc["lcd_interval"] = lcdInterval;
  serializeJson(doc, f);
  f.close();
}

// ================= LOG =================

void initLog() {
  if (!SPIFFS.exists(LOG_FILE)) {
    File f = SPIFFS.open(LOG_FILE, FILE_WRITE);
    if (f) { f.println("datetime,humidity,temperature,fan,mode"); f.close(); }
    logCount = 0;
  } else {
    // Hitung entri yang sudah ada
    File f = SPIFFS.open(LOG_FILE, FILE_READ);
    logCount = 0;
    if (f) {
      while (f.available()) { if (f.read() == '\n') logCount++; }
      f.close();
      if (logCount > 0) logCount--; // kurangi baris header
    }
  }
}

void writeLog() {
  // Rotate jika file terlalu besar
  File check = SPIFFS.open(LOG_FILE, FILE_READ);
  if (check) {
    size_t sz = check.size();
    check.close();
    if (sz > LOG_MAX_KB * 1024) {
      SPIFFS.remove(LOG_FILE);
      initLog();
    }
  }

  File f = SPIFFS.open(LOG_FILE, FILE_APPEND);
  if (!f)
    return;

  char row[64];
  snprintf(row, sizeof(row), "%s %s,%.1f,%.1f,%s,%s", getDateStr().c_str(),
           getTimeStr().c_str(), humidity, temperature, fanState ? "ON" : "OFF",
           autoMode ? "AUTO" : "MANUAL");

  f.println(row);
  f.close();
  logCount++;
}

// ================= EXCEL GENERATOR =================
// Membuat file .xlsx sederhana (minimal OOXML) dari CSV log di SPIFFS
// Hasilnya disimpan sementara di /log.xlsx lalu dikirim ke client

// Untuk menyederhanakan, kita generate file CSV yang bisa dibuka di Excel
// dengan ekstensi .xls (tab-delimited). Ini kompatibel dengan Excel tanpa
// perlu library tambahan di ESP32.

void generateExcelFromCSV(AsyncWebServerRequest *req) {
  if (!SPIFFS.exists(LOG_FILE)) {
    req->send(404, "text/plain", "Log belum tersedia");
    return;
  }

  // Kirim sebagai tab-separated values (.xls) — Excel buka native
  AsyncResponseStream *response = req->beginResponseStream(
      "application/vnd.ms-excel");
  response->addHeader("Content-Disposition",
                       "attachment; filename=\"log_dehumidifier.xls\"");

  File f = SPIFFS.open(LOG_FILE, FILE_READ);
  if (!f) {
    req->send(500, "text/plain", "Gagal baca log");
    return;
  }

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;
    // Ganti koma dengan tab untuk format TSV yang bisa dibaca Excel
    line.replace(",", "\t");
    response->println(line);
  }
  f.close();
  req->send(response);
}

// ================= SENSOR =================

// void readSensor() {
//   float h = sht31.readHumidity();
//   float t = sht31.readTemperature();
//   if (!isnan(h) && !isnan(t)) {
//     humidity = h;
//     temperature = t;
//     sensorErrorBeeped = false; // reset flag saat sensor kembali normal
//   } else {
//     // Buzzer: sensor error = 5x cepat (sekali saja sampai normal)
//     if (!sensorErrorBeeped) {
//       Serial.println("[SENSOR] Error: NaN detected!");
//       beep(5, 80);
//       sensorErrorBeeped = true;
//     }
//   }
// }
void readSensor() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();  // Celsius (default)
  if (!isnan(h) && !isnan(t)) {
    humidity = h;
    temperature = t;
    sensorErrorBeeped = false;
  } else {
    if (!sensorErrorBeeped) {
      Serial.println("[SENSOR] Error: NaN detected!");
      beep(5, 80);
      sensorErrorBeeped = true;
    }
  }
}
// ================= CONTROL =================

void controlLogic() {
  if (!autoMode)
    return;

  if (!isScheduleActive()) {
    // Di luar jadwal → matikan fan
    if (fanState) {
      Serial.println("[AUTO] Schedule IDLE -> Fan OFF");
    }
    setFan(false);
    return;
  }

  // Dalam jadwal — gunakan hysteresis
  if (!fanState && humidity >= maxHum) {
    Serial.printf("[AUTO] Hum %.1f >= %.1f (max) -> Fan ON\n", humidity, maxHum);
    setFan(true);
  }

  if (fanState && humidity <= minHum) {
    Serial.printf("[AUTO] Hum %.1f <= %.1f (min) -> Fan OFF\n", humidity, minHum);
    setFan(false);
  }

  // Buzzer: humidity kritis (> 90%) — 3x panjang, sekali saja
  if (humidity >= CRITICAL_HUM_THRESHOLD && !criticalHumBeeped) {
    Serial.printf("[ALERT] Humidity KRITIS: %.1f%% >= %.1f%%\n",
                  humidity, CRITICAL_HUM_THRESHOLD);
    beep(3, 300);
    criticalHumBeeped = true;
  }
  if (humidity < CRITICAL_HUM_THRESHOLD - 2.0f) {
    criticalHumBeeped = false; // reset saat turun di bawah 88%
  }
}

// ================= LCD (no-flicker) =================

void updateLCD() {
  char r0[17], r1[17];

  switch (lcdPage) {
    case 0: // Sensor utama
      snprintf(r0, sizeof(r0), "Hum : %.1f %%", humidity);
      snprintf(r1, sizeof(r1), "Fan : %s", fanState ? "ON " : "OFF");
      break;
    case 1: // Suhu + Mode
      snprintf(r0, sizeof(r0), "Temp: %.1f C", temperature);
      snprintf(r1, sizeof(r1), "Mode: %s", autoMode ? "AUTO" : "MANUAL");
      break;
    case 2: // WiFi + IP
      snprintf(r0, sizeof(r0), "WiFi:%-11s", wifiMode.c_str());
      snprintf(r1, sizeof(r1), "%-16s", ipAddress.c_str());
      break;
    case 3: // Jadwal + status aktif
      snprintf(r0, sizeof(r0), "%02d:%02d - %02d:%02d",
               schedStart/60, schedStart%60,
               schedEnd/60,   schedEnd%60);
      snprintf(r1, sizeof(r1), "Sched: %-9s",
               isScheduleActive() ? "ACTIVE" : "IDLE");
      break;
    case 4: // RTC tanggal + waktu
      snprintf(r0, sizeof(r0), "%-16s", getDateStr().c_str());
      snprintf(r1, sizeof(r1), "%-16s", getTimeStr().c_str());
      break;
    default:
      snprintf(r0, sizeof(r0), "Page %d", lcdPage);
      r1[0] = '\0';
  }

  String s0(r0), s1(r1);
  if (s0 != lcdRow0_prev) {
    lcd.setCursor(0, 0);
    lcd.print("                ");
    lcd.setCursor(0, 0);
    lcd.print(s0);
    lcdRow0_prev = s0;
  }
  if (s1 != lcdRow1_prev) {
    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0, 1);
    lcd.print(s1);
    lcdRow1_prev = s1;
  }
}

// ================= WEB =================

void setupWeb() {

  // GET / — halaman utama dari PROGMEM (bukan SPIFFS)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send_P(200, "text/html", HTML_PAGE);
  });

  // GET /api/status — JSON status lengkap
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req) {
    StaticJsonDocument<512> doc;
    doc["hum"]          = round(humidity * 10) / 10.0;
    doc["temp"]         = round(temperature * 10) / 10.0;
    doc["fan"]          = fanState ? "ON" : "OFF";
    doc["mode"]         = autoMode ? "AUTO" : "MANUAL";
    doc["min_hum"]      = minHum;
    doc["max_hum"]      = maxHum;
    doc["time"]         = getTimeStr();
    doc["date"]         = getDateStr();
    doc["wifi_mode"]    = wifiMode;
    doc["ip"]           = ipAddress;
    doc["lcd_interval"] = lcdInterval;
    doc["log_count"]    = logCount;
    doc["sched_active"] = isScheduleActive();

    char buf[6];
    sprintf(buf, "%02d:%02d", schedStart / 60, schedStart % 60);
    doc["time_start"] = String(buf);
    sprintf(buf, "%02d:%02d", schedEnd / 60, schedEnd % 60);
    doc["time_end"] = String(buf);

    String json;
    serializeJson(doc, json);
    req->send(200, "application/json", json);
  });

  // POST /api/control — mode & fan manual
  server.on("/api/control", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (req->hasParam("mode", true)) {
      String m = req->getParam("mode", true)->value();
      bool newAutoMode = (m == "AUTO");
      if (newAutoMode != autoMode) {
        autoMode = newAutoMode;
        Serial.printf("[WEB] Mode changed to %s\n", autoMode ? "AUTO" : "MANUAL");
        // Jika baru masuk AUTO, langsung evaluasi controlLogic
        if (autoMode) {
          controlLogic();
        }
        saveSettings();
      }
    }
    if (req->hasParam("fan", true)) {
      if (!autoMode) {
        String f = req->getParam("fan", true)->value();
        Serial.printf("[WEB] Manual fan control: %s\n", f.c_str());
        setFan(f == "ON");
      }
    }
    req->send(200, "application/json", "{\"ok\":true}");
  });

  // POST /api/settings/hysteresis — [Fix #4] dengan error checking
  server.on(
      "/api/settings/hysteresis", HTTP_POST, [](AsyncWebServerRequest *req) {},
      NULL,
      [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index,
         size_t total) {
        StaticJsonDocument<128> doc;
        DeserializationError err = deserializeJson(doc, data, len);

        if (err) {
          req->send(400, "application/json",
                    "{\"ok\":false,\"error\":\"Invalid JSON\"}");
          return;
        }

        float newMin = doc["min"] | -1.0f;
        float newMax = doc["max"] | -1.0f;

        // [Fix #10] validasi range 0–100
        if (newMin < 0 || newMax < 0 || newMin >= newMax || newMin > 100 ||
            newMax > 100) {
          req->send(400, "application/json",
                    "{\"ok\":false,\"error\":\"Range invalid\"}");
          return;
        }

        minHum = newMin;
        maxHum = newMax;
        Serial.printf("[WEB] Hysteresis updated: min=%.1f, max=%.1f\n", minHum, maxHum);
        saveSettings();
        beep(1, 100); // konfirmasi simpan
        req->send(200, "application/json", "{\"ok\":true}");
      });

  // POST /api/settings/schedule — [Fix #5] dengan error checking
  server.on(
      "/api/settings/schedule", HTTP_POST, [](AsyncWebServerRequest *req) {},
      NULL,
      [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index,
         size_t total) {
        StaticJsonDocument<128> doc;
        DeserializationError err = deserializeJson(doc, data, len);

        if (err) {
          req->send(400, "application/json",
                    "{\"ok\":false,\"error\":\"Invalid JSON\"}");
          return;
        }

        String start = doc["start"] | String("");
        String end = doc["end"] | String("");

        // [Fix #5] validasi format HH:MM
        if (start.length() != 5 || end.length() != 5 || start[2] != ':' ||
            end[2] != ':') {
          req->send(400, "application/json",
                    "{\"ok\":false,\"error\":\"Format waktu invalid\"}");
          return;
        }

        int sh = start.substring(0, 2).toInt();
        int sm = start.substring(3, 5).toInt();
        int eh = end.substring(0, 2).toInt();
        int em = end.substring(3, 5).toInt();

        if (sh > 23 || sm > 59 || eh > 23 || em > 59) {
          req->send(400, "application/json",
                    "{\"ok\":false,\"error\":\"Nilai waktu invalid\"}");
          return;
        }

        schedStart = sh * 60 + sm;
        schedEnd = eh * 60 + em;
        Serial.printf("[WEB] Schedule updated: %02d:%02d - %02d:%02d\n",
                      schedStart/60, schedStart%60, schedEnd/60, schedEnd%60);
        saveSettings();
        beep(1, 100); // konfirmasi simpan
        req->send(200, "application/json", "{\"ok\":true}");
      });

  // GET /download-log — download CSV log dari SPIFFS
  server.on("/download-log", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (SPIFFS.exists(LOG_FILE)) {
      req->send(SPIFFS, LOG_FILE, "text/csv",
                true); // true = as attachment
    } else {
      req->send(404, "text/plain", "Log belum tersedia");
    }
  });

  // GET /download-excel — download log sebagai Excel (.xls TSV)
  server.on("/download-excel", HTTP_GET, [](AsyncWebServerRequest *req) {
    generateExcelFromCSV(req);
  });

  // GET /api/wifi/config — kembalikan SSID tersimpan (password tidak dikirim)
  server.on("/api/wifi/config", HTTP_GET, [](AsyncWebServerRequest *req) {
    StaticJsonDocument<128> doc;
    doc["ssid"]  = staSSID;
    doc["saved"] = SPIFFS.exists(WIFI_CONFIG_FILE);
    String json;
    serializeJson(doc, json);
    req->send(200, "application/json", json);
  });

  // POST /api/settings/wifi — simpan kredensial & restart
  server.on(
      "/api/settings/wifi", HTTP_POST, [](AsyncWebServerRequest *req) {},
      NULL,
      [](AsyncWebServerRequest *req, uint8_t *data, size_t len,
         size_t index, size_t total) {
        StaticJsonDocument<192> doc;
        if (deserializeJson(doc, data, len)) {
          req->send(400, "application/json",
                    "{\"ok\":false,\"error\":\"Invalid JSON\"}");
          return;
        }
        String newSSID = doc["ssid"] | String("");
        String newPass = doc["pass"] | String("");
        if (newSSID.length() < 1 || newSSID.length() > 32) {
          req->send(400, "application/json",
                    "{\"ok\":false,\"error\":\"SSID tidak valid (1-32 karakter)\"}");
          return;
        }
        saveWifiConfig(newSSID, newPass);
        req->send(200, "application/json",
                  "{\"ok\":true,\"msg\":\"Tersimpan! Device restart dalam 2 detik...\"}");
        delay(2000);
        ESP.restart();
      });

  // GET /api/log — kirim raw CSV untuk ditampilkan di web
  server.on("/api/log", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (SPIFFS.exists(LOG_FILE)) {
      req->send(SPIFFS, LOG_FILE, "text/plain", false);
    } else {
      req->send(200, "text/plain",
                "datetime,humidity,temperature,fan,mode\n");
    }
  });

  // POST /api/log/reset — hapus & buat ulang file log
  server.on("/api/log/reset", HTTP_POST, [](AsyncWebServerRequest *req) {
    SPIFFS.remove(LOG_FILE);
    initLog();
    req->send(200, "application/json", "{\"ok\":true}");
  });

  // POST /api/settings/lcd-interval — atur kecepatan ganti halaman LCD
  server.on(
      "/api/settings/lcd-interval", HTTP_POST, [](AsyncWebServerRequest *req) {},
      NULL,
      [](AsyncWebServerRequest *req, uint8_t *data, size_t len,
         size_t index, size_t total) {
        StaticJsonDocument<64> doc;
        if (deserializeJson(doc, data, len)) {
          req->send(400, "application/json",
                    "{\"ok\":false,\"error\":\"Invalid JSON\"}");
          return;
        }
        int newVal = doc["interval"] | -1;
        if (newVal < 1 || newVal > 30) {
          req->send(400, "application/json",
                    "{\"ok\":false,\"error\":\"Interval harus 1-30 detik\"}");
          return;
        }
        lcdInterval = newVal;
        saveSettings();
        req->send(200, "application/json", "{\"ok\":true}");
      });

  // Catch-all 404 — PENTING! Tanpa ini, route yang tidak ada akan hang
  server.onNotFound([](AsyncWebServerRequest *req) {
    req->send(404, "text/plain", "Not Found: " + req->url());
  });

  server.begin();
  Serial.println("[WEB] Server started on port 80");
}

// ================= SETUP =================

// void setup() {
//   Serial.begin(115200);

//   // fanState = false;
//   setFan(false);
//   // digitalWrite(RELAY_PIN, HIGH);
//   // digitalWrite(BUZZER_PIN, HIGH);
//   pinMode(RELAY_PIN, OUTPUT);
//   pinMode(BUZZER_PIN, OUTPUT);
// digitalWrite(RELAY_PIN, HIGH);
//   digitalWrite(BUZZER_PIN, HIGH);
//   delay(3000);


//   Wire.begin(SDA_PIN, SCL_PIN);

//   lcd.init();
//   lcd.backlight();
//   lcd.setCursor(0, 0);
//   lcd.print("Booting...");

//   // SPIFFS untuk logging & wifi config (HTML TIDAK lagi dari sini)
//   if (!SPIFFS.begin(true)) {
//     Serial.println("SPIFFS Mount Failed");
//   } else {
//     Serial.printf("[SPIFFS] Total: %u bytes, Used: %u bytes\n",
//                   SPIFFS.totalBytes(), SPIFFS.usedBytes());
//     initLog();
//     loadWifiConfig();
//     loadSettings();
//   }

//   // sht31.begin(0x44);
// dht.begin();
//   // SEBELUM:
// rtc.begin();
// if (rtc.lostPower()) {
//   rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));  // ← UTC / timezone build komputer!
// }

// // SESUDAH:
// if (!rtc.begin()) {
//   Serial.println("[RTC] ERROR: Modul DS3231 tidak terdeteksi!");
// } else {
//   if (rtc.lostPower()) {
//     // Baterai habis atau pertama kali — set dari waktu kompilasi + offset WIB
//     adjustRtcFromBuild();
//     Serial.println("[RTC] lostPower — set dari build time + WIB offset");
//   } else {
//     Serial.printf("[RTC] Waktu tersimpan: %s %s WIB\n",
//                   getDateStr().c_str(), getTimeStr().c_str());
//   }
// }
void setup() {
  Serial.begin(115200);

  // ── Pin harus dikonfigurasi DULU sebelum digitalWrite/setFan ──
  pinMode(RELAY_PIN,  OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(RELAY_PIN,  HIGH);  // relay OFF (active-low)
  digitalWrite(BUZZER_PIN, HIGH);  // buzzer OFF (active-low)
  fanState = false;                // sinkronkan state tanpa trigger beep/log

  delay(3000);

  Wire.begin(SDA_PIN, SCL_PIN);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Booting...");

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
  } else {
    Serial.printf("[SPIFFS] Total: %u bytes, Used: %u bytes\n",
                  SPIFFS.totalBytes(), SPIFFS.usedBytes());
    initLog();
    loadWifiConfig();
    loadSettings();
  }

  dht.begin();

  // ── RTC — hanya satu blok, tanpa duplikat ──
  if (!rtc.begin()) {
    Serial.println("[RTC] ERROR: Modul DS3231 tidak terdeteksi!");
  } else {
    if (rtc.lostPower()) {
      adjustRtcFromBuild();
      Serial.println("[RTC] lostPower — set dari build time + WIB offset");
    } else {
      Serial.printf("[RTC] Waktu tersimpan: %s %s WIB\n",
                    getDateStr().c_str(), getTimeStr().c_str());
    }
  }

  // ── WiFi ──
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP().toString());

  WiFi.begin(staSSID.c_str(), staPass.c_str());
  Serial.println("Connecting WiFi STA...");

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiMode  = "AP+STA";
    ipAddress = WiFi.localIP().toString();
    Serial.println("\nSTA Connected: " + ipAddress);
    syncNTP();
  } else {
    wifiMode  = "AP";
    ipAddress = WiFi.softAPIP().toString();
    Serial.println("\nAP Only: " + ipAddress);
    Serial.println("[NTP] Tidak bisa sync — tidak ada koneksi internet");
  }

  setupWeb();
  beep(2, 100);

  Serial.printf("[INIT] Auto=%s, MinHum=%.1f, MaxHum=%.1f\n",
                autoMode ? "true" : "false", minHum, maxHum);
  Serial.printf("[INIT] Schedule=%02d:%02d - %02d:%02d, Active=%s\n",
                schedStart/60, schedStart%60,
                schedEnd/60,   schedEnd%60,
                isScheduleActive() ? "YES" : "NO");

  lcd.clear();
  lcd.print("System Ready");
  lcdRow0_prev = "";
  lcdRow1_prev = "";
}
//   // ── WiFi ──
//   // AP selalu dikonfigurasi terlebih dahulu,
//   // lalu coba STA. Hasilnya device selalu bisa diakses
//   // via AP 192.168.4.1 maupun IP lokal jika STA konek.
//   WiFi.mode(WIFI_AP_STA);
//   WiFi.softAP(AP_SSID, AP_PASSWORD);

//   Serial.print("AP IP: ");
//   Serial.println(WiFi.softAPIP().toString());

//   WiFi.begin(staSSID.c_str(), staPass.c_str());
//   Serial.println("Connecting WiFi STA...");

//   unsigned long startAttempt = millis();
//   while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
//     delay(500);
//     Serial.print(".");
//   }

//   if (WiFi.status() == WL_CONNECTED) {
//     wifiMode = "AP+STA";
//     ipAddress = WiFi.localIP().toString();
//     Serial.println("\nSTA Connected: " + ipAddress);
//     // Sinkronisasi waktu via NTP → tulis ke DS3231
//     syncNTP();
//   } else {
//     wifiMode = "AP";
//     ipAddress = WiFi.softAPIP().toString();
//     Serial.println("\nAP Only: " + ipAddress);
//     Serial.println("[NTP] Tidak bisa sync — tidak ada koneksi internet");
//   }

//   setupWeb();

//   beep(2, 100);

//   // Print status awal untuk debugging
//   Serial.printf("[INIT] Auto=%s, MinHum=%.1f, MaxHum=%.1f\n",
//                 autoMode ? "true" : "false", minHum, maxHum);
//   Serial.printf("[INIT] Schedule=%02d:%02d - %02d:%02d, Active=%s\n",
//                 schedStart/60, schedStart%60,
//                 schedEnd/60, schedEnd%60,
//                 isScheduleActive() ? "YES" : "NO");

//   lcd.clear();
//   lcd.print("System Ready");
//   lcdRow0_prev = ""; // reset shadow setelah clear
//   lcdRow1_prev = "";
// }

// ================= LOOP =================

void loop() {
  unsigned long now = millis();

  if (now - lastSensor > 2000) {
    lastSensor = now;
    readSensor();
    controlLogic();
  }

  // Ganti halaman LCD sesuai interval yang dikonfigurasi
  if (now - lastPageSwitch > (unsigned long)lcdInterval * 1000) {
    lastPageSwitch = now;
    lcdPage = (lcdPage + 1) % LCD_PAGES;
    lcdRow0_prev = ""; // paksa redraw
    lcdRow1_prev = "";
  }

  if (now - lastLCD > 500) { // update LCD lebih cepat untuk page baru
    lastLCD = now;
    updateLCD();
  }

  delay(1);
}