/*
  CANON IN D - WIFI DISPLAY CONFIG
  ESP8266 Lolin/Wemos D1 Mini

  Copy of Birthday + Future Wife + Wedding with SoftAP web config.
  Original sketch left untouched; empty/Unset fields use baked-in defaults.

  BUZZER          D6 (GPIO12)
  MAGNETIC SENSOR D7 (GPIO13)
  OLED SDA        D2 (GPIO4)
  OLED SCL        D1 (GPIO5)

  WiFi SoftAP + captive portal: join the box Wi-Fi and the phone
  settings page opens automatically (DNS captive portal).
  Default SSID/pass CanonBox / canon1234 (changeable in the app).
  Configure theme, messages, and Wi-Fi without changing the music.

  Themes rotate after each full playthrough (unless locked via WiFi):
    0 Birthday     — 8 scenes
    1 Future Wife  — 6 scenes
    2 Wedding      — 5 scenes

  DOOR OPEN   = MUSIC ON  + OLED ON
  DOOR CLOSED = MUSIC OFF + OLED OFF

  SERIAL = 115200
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <string.h>
#include <math.h>
#include <pgmspace.h>

// =====================================
// HARDWARE
// =====================================

#define BUZZER_PIN D6
#define SENSOR_PIN D7

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

bool oledReady = false;
bool displayOn = false;

// =====================================
// WIFI SOFTAP
// =====================================

const char* AP_SSID_DEFAULT = "CanonBox";
const char* AP_PASS_DEFAULT = "canon1234";
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
IPAddress apGateway(192, 168, 4, 1);
IPAddress apSubnet(255, 255, 255, 0);

ESP8266WebServer server(80);
DNSServer dnsServer;

#define TXT_LEN 24
#define AP_SSID_LEN 33
#define AP_PASS_LEN 65
#define CFG_MAGIC 0x31444343UL  /* CDC1 */
#define CFG_VERSION 2

// =====================================
// MUSICAL NOTES
// =====================================

#define REST 0
#define D4 294
#define E4 330
#define FS4 370
#define G4 392
#define A4 440
#define B4 494

#define CS5 554
#define D5 587
#define E5 659
#define FS5 740
#define G5 784
#define A5 880

// =====================================
// TEMPO
// =====================================

const int QUARTER_BPM = 66;
// One unit = one sixteenth (quarter note = 4 units)
const int BEAT = (60000UL / QUARTER_BPM) / 4;

const int LEGATO_GAP = 30;
const int REPEAT_GAP = 3500;

// =====================================
// MUSICAL STRUCTURE
// =====================================

struct Note {
  int frequency;
  int beats;
};

const Note bass[] = {
  {D4, 8}, {A4, 8}, {B4, 8}, {FS4, 8},
  {G4, 8}, {D4, 8}, {G4, 8}, {A4, 8}
};

const Note melody[] = {

  // MEASURE 13 - D
  {A5, 2}, {FS5, 1}, {G5, 1}, {A5, 2},
  {FS5, 1}, {G5, 1}, {A5, 2},

  // MEASURE 13 - A
  {A4, 1}, {B4, 1}, {CS5, 1}, {D5, 1},
  {E5, 1}, {G5, 1},

  // MEASURE 14 - Bm
  {FS5, 2}, {D5, 1}, {E5, 1}, {FS5, 2},

  // MEASURE 14 - F#m
  {FS4, 1}, {G4, 1}, {A4, 1}, {B4, 1},
  {A4, 1}, {G4, 1}, {A4, 1},
  {FS4, 1}, {G4, 1}, {A4, 1},

  // MEASURE 15 - G
  {G4, 2}, {B4, 1}, {A4, 1}, {G4, 2},

  // MEASURE 15 - D
  {FS4, 1}, {E4, 1}, {FS4, 1}, {E4, 1},
  {D4, 1}, {E4, 1}, {FS4, 1},
  {G4, 1}, {A4, 1}, {B4, 1},

  // MEASURE 16 - G
  {G4, 2}, {B4, 1}, {A4, 1}, {B4, 2},
  {CS5, 1}, {D5, 1},

  // MEASURE 16 - A
  {A4, 1}, {B4, 1}, {CS5, 1}, {D5, 1},
  {E5, 1}, {FS5, 1}, {G5, 1}, {A5, 1},

  // MEASURE 17 - D
  {A5, 2}, {FS5, 1}, {G5, 1}, {A5, 2},
  {FS5, 1}, {G5, 1}, {A5, 2},

  // MEASURE 17 - A
  {A4, 1}, {B4, 1}, {CS5, 1}, {D5, 1},
  {E5, 1}, {G5, 1},

  // MEASURE 18 - Bm
  {FS5, 2}, {D5, 1}, {E5, 1}, {FS5, 2},

  // MEASURE 18 - F#m
  {FS4, 1}, {G4, 1}, {A4, 1}, {B4, 1},
  {A4, 1}, {G4, 1}, {A4, 1},
  {FS4, 1}, {G4, 1}, {A4, 1},

  // MEASURE 19 - G
  {G4, 2}, {B4, 1}, {A4, 1}, {G4, 2},

  // MEASURE 19 - D
  {FS4, 1}, {E4, 1}, {FS4, 1}, {E4, 1},
  {D4, 1}, {E4, 1}, {FS4, 1},
  {G4, 1}, {A4, 1}, {B4, 1},

  // MEASURE 20 - G
  {G4, 2}, {B4, 1}, {A4, 1}, {B4, 2},
  {CS5, 1}, {D5, 1},

  // MEASURE 20 - A  (15 sixteenths total with G half)
  {A4, 1}, {B4, 1}, {CS5, 1}, {D5, 1},
  {E5, 1}, {FS5, 1}, {G5, 1},

  // MEASURE 21 - cadence hold (fills remaining + soft fermata)
  {A5, 8}
};

const int bassLength = sizeof(bass) / sizeof(bass[0]);
const int totalNotes = sizeof(melody) / sizeof(melody[0]);

const unsigned int BEATS_BASS = 64;
const unsigned int BEATS_REST_GAP = 4;
const unsigned int BEATS_MELODY_START = BEATS_BASS + BEATS_REST_GAP; // 68


// =====================================
// THEMES
// =====================================

enum ThemeId {
  THEME_BIRTHDAY = 0,
  THEME_FUTURE_WIFE = 1,
  THEME_WEDDING = 2,
  THEME_COUNT = 3
};

int currentTheme = THEME_BIRTHDAY;
bool themeLocked = false;

const int THEME_SCENE_COUNTS[THEME_COUNT] = { 8, 6, 5 };
const int IDLE_TOTAL_SCENES = 8 + 6 + 5;  // 19

// Absolute beat where each scene ends (music-playing mode), per theme
const unsigned int BIRTHDAY_SCENE_BEAT_END[] = {
  24,                              // 0 boot
  BEATS_MELODY_START,              // 1 pin
  BEATS_MELODY_START + 32,         // 2 intro
  BEATS_MELODY_START + 64,         // 3 balloons
  BEATS_MELODY_START + 80,         // 4 gallery
  BEATS_MELODY_START + 96,         // 5 quiz
  BEATS_MELODY_START + 112,        // 6 cake
  0xFFFF                           // 7 finale
};

const unsigned int FUTUREWIFE_SCENE_BEAT_END[] = {
  32,                              // 0 Dedication
  BEATS_MELODY_START,              // 1 Constellation
  BEATS_MELODY_START + 32,         // 2 Two paths
  BEATS_MELODY_START + 64,         // 3 Promises
  BEATS_MELODY_START + 96,         // 4 Why you
  0xFFFF                           // 5 Seal
};

const unsigned int WEDDING_SCENE_BEAT_END[] = {
  BEATS_MELODY_START,              // 0 Title
  BEATS_MELODY_START + 32,         // 1 Rings
  BEATS_MELODY_START + 64,         // 2 Hearts
  BEATS_MELODY_START + 96,         // 3 Message
  0xFFFF                           // 4 Forever
};

const unsigned int* sceneBeatEnds() {
  if (currentTheme == THEME_FUTURE_WIFE) return FUTUREWIFE_SCENE_BEAT_END;
  if (currentTheme == THEME_WEDDING) return WEDDING_SCENE_BEAT_END;
  return BIRTHDAY_SCENE_BEAT_END;
}

int themeSceneCount() {
  return THEME_SCENE_COUNTS[currentTheme];
}

const char* themeName(int theme) {
  if (theme == THEME_FUTURE_WIFE) return "FutureWife";
  if (theme == THEME_WEDDING) return "Wedding";
  return "Birthday";
}

void advanceTheme() {
  if (themeLocked) {
    Serial.print("Theme locked: ");
    Serial.println(themeName(currentTheme));
    return;
  }
  currentTheme = (currentTheme + 1) % THEME_COUNT;
  Serial.print("Theme -> ");
  Serial.println(themeName(currentTheme));
}

// Idle global index 0..18 -> (theme, local scene)
void resolveIdleScene(int idleIndex, int* outTheme, int* outScene) {
  if (idleIndex < 0) idleIndex = 0;
  if (idleIndex >= IDLE_TOTAL_SCENES) idleIndex = idleIndex % IDLE_TOTAL_SCENES;

  if (idleIndex < 8) {
    *outTheme = THEME_BIRTHDAY;
    *outScene = idleIndex;
  } else if (idleIndex < 14) {
    *outTheme = THEME_FUTURE_WIFE;
    *outScene = idleIndex - 8;
  } else {
    *outTheme = THEME_WEDDING;
    *outScene = idleIndex - 14;
  }
}

const int MEASURE_NOTE_INDEX[] = {
  0, 13, 27, 41, 55, 68, 82, 96, 109
};
const int MEASURE_NUMBER[] = {
  13, 14, 15, 16, 17, 18, 19, 20, 21
};
const int MEASURE_COUNT = 9;

// =====================================
// PROGMEM COPY  (birthday content bible)
// =====================================

const char BALLOON_L1_0[] PROGMEM = "Soft late-night";
const char BALLOON_L2_0[] PROGMEM = "smile";
const char BALLOON_L1_1[] PROGMEM = "I live for";
const char BALLOON_L2_1[] PROGMEM = "your laugh";
const char BALLOON_L1_2[] PROGMEM = "Favorite chaos";
const char BALLOON_L2_2[] PROGMEM = "us laughing";
const char BALLOON_L1_3[] PROGMEM = "Full laugh";
const char BALLOON_L2_3[] PROGMEM = "favorite sound";
const char BALLOON_L1_4[] PROGMEM = "Chin in hand";
const char BALLOON_L2_4[] PROGMEM = "mine";
const char BALLOON_L1_5[] PROGMEM = "Thumbs up";
const char BALLOON_L2_5[] PROGMEM = "we find way";
const char BALLOON_L1_6[] PROGMEM = "Quiet watch";
const char BALLOON_L2_6[] PROGMEM = "just us";
const char BALLOON_L1_7[] PROGMEM = "Caught you";
const char BALLOON_L2_7[] PROGMEM = "thinking";
const char BALLOON_L1_8[] PROGMEM = "Stay on line";
const char BALLOON_L2_8[] PROGMEM = "while you rest";
const char BALLOON_L1_9[] PROGMEM = "Rest well";
const char BALLOON_L2_9[] PROGMEM = "so loved";
const char BALLOON_L1_10[] PROGMEM = "Asleep on call";
const char BALLOON_L2_10[] PROGMEM = "home";
const char BALLOON_L1_11[] PROGMEM = "Still here";
const char BALLOON_L2_11[] PROGMEM = "watching";
const char BALLOON_L1_12[] PROGMEM = "Soft look";
const char BALLOON_L2_12[] PROGMEM = "undoes me";

const char* const BALLOON_L1[] PROGMEM = {
  BALLOON_L1_0, BALLOON_L1_1, BALLOON_L1_2, BALLOON_L1_3,
  BALLOON_L1_4, BALLOON_L1_5, BALLOON_L1_6, BALLOON_L1_7,
  BALLOON_L1_8, BALLOON_L1_9, BALLOON_L1_10, BALLOON_L1_11,
  BALLOON_L1_12
};

const char* const BALLOON_L2[] PROGMEM = {
  BALLOON_L2_0, BALLOON_L2_1, BALLOON_L2_2, BALLOON_L2_3,
  BALLOON_L2_4, BALLOON_L2_5, BALLOON_L2_6, BALLOON_L2_7,
  BALLOON_L2_8, BALLOON_L2_9, BALLOON_L2_10, BALLOON_L2_11,
  BALLOON_L2_12
};

const int BALLOON_COUNT = 13;

const char GALLERY_L1_0[] PROGMEM = "Little house";
const char GALLERY_L2_0[] PROGMEM = "under stars";
const char GALLERY_L1_1[] PROGMEM = "Match to Marry";
const char GALLERY_L2_1[] PROGMEM = "nights";
const char GALLERY_L1_2[] PROGMEM = "Purple + balloons";
const char GALLERY_L2_2[] PROGMEM = "heart stayed";
const char GALLERY_L1_3[] PROGMEM = "You rewired";
const char GALLERY_L2_3[] PROGMEM = "my heart";
const char GALLERY_L1_4[] PROGMEM = "Your name";
const char GALLERY_L2_4[] PROGMEM = "on our raft";
const char GALLERY_L1_5[] PROGMEM = "Ten hours";
const char GALLERY_L2_5[] PROGMEM = "we made top";

const char* const GALLERY_L1[] PROGMEM = {
  GALLERY_L1_0, GALLERY_L1_1, GALLERY_L1_2,
  GALLERY_L1_3, GALLERY_L1_4, GALLERY_L1_5
};

const char* const GALLERY_L2[] PROGMEM = {
  GALLERY_L2_0, GALLERY_L2_1, GALLERY_L2_2,
  GALLERY_L2_3, GALLERY_L2_4, GALLERY_L2_5
};

const int GALLERY_COUNT = 6;

const char QUIZ_Q_0[] PROGMEM = "Superpower at work?";
const char QUIZ_Q_1[] PROGMEM = "How do I feel?";
const char QUIZ_Q_2[] PROGMEM = "I remind you to?";
const char QUIZ_Q_3[] PROGMEM = "I hope for you?";

const char* const QUIZ_Q[] PROGMEM = {
  QUIZ_Q_0, QUIZ_Q_1, QUIZ_Q_2, QUIZ_Q_3
};

// Options packed as "A text" rows; correct index 0..3
const char QUIZ_A0[] PROGMEM = "A Kindness";
const char QUIZ_B0[] PROGMEM = "B Never sleep";
const char QUIZ_C0[] PROGMEM = "C Say no";
const char QUIZ_D0[] PROGMEM = "D Skip meals";

const char QUIZ_A1[] PROGMEM = "A I love you";
const char QUIZ_B1[] PROGMEM = "B Just okay";
const char QUIZ_C1[] PROGMEM = "C Neutral";
const char QUIZ_D1[] PROGMEM = "D Unsure";

const char QUIZ_A2[] PROGMEM = "A Skip lunch";
const char QUIZ_B2[] PROGMEM = "B Drink water";
const char QUIZ_C2[] PROGMEM = "C Overtime";
const char QUIZ_D2[] PROGMEM = "D Ignore me";

const char QUIZ_A3[] PROGMEM = "A More nights";
const char QUIZ_B3[] PROGMEM = "B Less rest";
const char QUIZ_C3[] PROGMEM = "C Someone cares";
const char QUIZ_D3[] PROGMEM = "D Fewer msgs";

// Flat 4 questions × 4 options
const char* const QUIZ_OPT[] PROGMEM = {
  QUIZ_A0, QUIZ_B0, QUIZ_C0, QUIZ_D0,
  QUIZ_A1, QUIZ_B1, QUIZ_C1, QUIZ_D1,
  QUIZ_A2, QUIZ_B2, QUIZ_C2, QUIZ_D2,
  QUIZ_A3, QUIZ_B3, QUIZ_C3, QUIZ_D3
};

const uint8_t QUIZ_CORRECT[4] PROGMEM = { 0, 0, 1, 2 };
const int QUIZ_COUNT = 4;

// Finale pages: line0 / line1 / line2 (empty = skip)
const char FIN_0_0[] PROGMEM = "Happy Birthday";
const char FIN_0_1[] PROGMEM = "Baby";
const char FIN_0_2[] PROGMEM = "<3";

const char FIN_1_0[] PROGMEM = "Today is all";
const char FIN_1_1[] PROGMEM = "about you.";
const char FIN_1_2[] PROGMEM = "";

const char FIN_2_0[] PROGMEM = "Thank you for";
const char FIN_2_1[] PROGMEM = "existing.";
const char FIN_2_2[] PROGMEM = "";

const char FIN_3_0[] PROGMEM = "Glad life";
const char FIN_3_1[] PROGMEM = "led me to you.";
const char FIN_3_2[] PROGMEM = "";

const char FIN_4_0[] PROGMEM = "Every version";
const char FIN_4_1[] PROGMEM = "I appreciate.";
const char FIN_4_2[] PROGMEM = "";

const char FIN_5_0[] PROGMEM = "Today, rest.";
const char FIN_5_1[] PROGMEM = "Eat. Smile.";
const char FIN_5_2[] PROGMEM = "";

const char FIN_6_0[] PROGMEM = "You are loved.";
const char FIN_6_1[] PROGMEM = "Family + me.";
const char FIN_6_2[] PROGMEM = "";

const char FIN_7_0[] PROGMEM = "I'd still";
const char FIN_7_1[] PROGMEM = "choose you.";
const char FIN_7_2[] PROGMEM = "";

const char FIN_8_0[] PROGMEM = "Today.";
const char FIN_8_1[] PROGMEM = "Tomorrow.";
const char FIN_8_2[] PROGMEM = "Always.";

const char FIN_9_0[] PROGMEM = "I love you";
const char FIN_9_1[] PROGMEM = "so much.";
const char FIN_9_2[] PROGMEM = "";

const char FIN_10_0[] PROGMEM = "Happy Birthday";
const char FIN_10_1[] PROGMEM = "again, my love.";
const char FIN_10_2[] PROGMEM = "- Lolo";

const char* const FINALE_L0[] PROGMEM = {
  FIN_0_0, FIN_1_0, FIN_2_0, FIN_3_0, FIN_4_0,
  FIN_5_0, FIN_6_0, FIN_7_0, FIN_8_0, FIN_9_0, FIN_10_0
};

const char* const FINALE_L1[] PROGMEM = {
  FIN_0_1, FIN_1_1, FIN_2_1, FIN_3_1, FIN_4_1,
  FIN_5_1, FIN_6_1, FIN_7_1, FIN_8_1, FIN_9_1, FIN_10_1
};

const char* const FINALE_L2[] PROGMEM = {
  FIN_0_2, FIN_1_2, FIN_2_2, FIN_3_2, FIN_4_2,
  FIN_5_2, FIN_6_2, FIN_7_2, FIN_8_2, FIN_9_2, FIN_10_2
};

const int FINALE_PAGES = 11;

char flashBuf[24];

struct DisplayConfig {
  uint32_t magic;
  uint16_t version;
  uint8_t theme;
  uint8_t locked;
  char nickname[TXT_LEN];
  char signer[TXT_LEN];
  char balloonL1[13][TXT_LEN];
  char balloonL2[13][TXT_LEN];
  char galleryL1[6][TXT_LEN];
  char galleryL2[6][TXT_LEN];
  char quizQ[4][TXT_LEN];
  char quizOpt[16][TXT_LEN];
  int8_t quizCorrect[4];
  char finaleL0[11][TXT_LEN];
  char finaleL1[11][TXT_LEN];
  char finaleL2[11][TXT_LEN];
  char promises[4][TXT_LEN];
  char promiseSub2[TXT_LEN];
  char reasons[6][TXT_LEN];
  char wedTitle[TXT_LEN];
  char wedSubtitle[TXT_LEN];
  char wedRings[TXT_LEN];
  char wedHearts1[TXT_LEN];
  char wedHearts2[TXT_LEN];
  char wedMsg1[TXT_LEN];
  char wedMsg2[TXT_LEN];
  char wedMsg3[TXT_LEN];
  char wedFinal1[TXT_LEN];
  char wedFinal2[TXT_LEN];
  char fwSealTitle[TXT_LEN];
  char fwSealChoose[TXT_LEN];
  char fwSealSign[TXT_LEN];
  char apSsid[AP_SSID_LEN];
  char apPass[AP_PASS_LEN];
  uint8_t apOpen;  // 1 = open network (ignore password)
};

DisplayConfig cfg;

void clearConfig() {
  memset(&cfg, 0, sizeof(cfg));
  cfg.magic = CFG_MAGIC;
  cfg.version = CFG_VERSION;
  cfg.theme = THEME_BIRTHDAY;
  cfg.locked = 0;
  for (int i = 0; i < 4; i++) cfg.quizCorrect[i] = -1;
  cfg.apOpen = 0;
  // empty apSsid/apPass => use factory defaults at runtime
}

void applyThemeFromConfig() {
  if (cfg.theme >= THEME_COUNT) cfg.theme = THEME_BIRTHDAY;
  currentTheme = cfg.theme;
  themeLocked = cfg.locked != 0;
}

void syncConfigThemeFields() {
  cfg.theme = (uint8_t)currentTheme;
  cfg.locked = themeLocked ? 1 : 0;
}

bool saveConfig() {
  syncConfigThemeFields();
  cfg.magic = CFG_MAGIC;
  cfg.version = CFG_VERSION;
  File f = LittleFS.open("/display.cfg", "w");
  if (!f) {
    Serial.println("Config save failed");
    return false;
  }
  size_t n = f.write((const uint8_t*)&cfg, sizeof(cfg));
  f.close();
  return n == sizeof(cfg);
}

bool loadConfig() {
  clearConfig();
  if (!LittleFS.exists("/display.cfg")) {
    applyThemeFromConfig();
    return false;
  }
  File f = LittleFS.open("/display.cfg", "r");
  if (!f) {
    applyThemeFromConfig();
    return false;
  }
  size_t n = f.read((uint8_t*)&cfg, sizeof(cfg));
  f.close();
  if (cfg.magic != CFG_MAGIC) {
    Serial.println("Config invalid; using defaults");
    clearConfig();
    applyThemeFromConfig();
    return false;
  }
  // v1 files are shorter; unread Wi-Fi fields stay zero from clearConfig()
  if (cfg.version == 1 && n > 0) {
    cfg.apSsid[0] = '\0';
    cfg.apPass[0] = '\0';
    cfg.apOpen = 0;
    cfg.version = CFG_VERSION;
    applyThemeFromConfig();
    saveConfig();
    Serial.println("Config upgraded to v2");
    return true;
  }
  if (cfg.version != CFG_VERSION || n != sizeof(cfg)) {
    Serial.println("Config invalid; using defaults");
    clearConfig();
    applyThemeFromConfig();
    return false;
  }
  applyThemeFromConfig();
  Serial.println("Config loaded from LittleFS");
  return true;
}

void loadFlash(const char* ptr) {
  strncpy_P(flashBuf, ptr, sizeof(flashBuf) - 1);
  flashBuf[sizeof(flashBuf) - 1] = '\0';
}

void loadFlashTable(const char* const* table, int index) {
  const char* ptr = (const char*)pgm_read_ptr(&table[index]);
  loadFlash(ptr);
}

void loadOverrideOrTable(const char overrideRow[TXT_LEN], const char* const* table, int index) {
  if (overrideRow[0] != '\0') {
    strncpy(flashBuf, overrideRow, sizeof(flashBuf) - 1);
    flashBuf[sizeof(flashBuf) - 1] = '\0';
  } else {
    loadFlashTable(table, index);
  }
}

const char* textOr(const char* overrideTxt, const char* fallback) {
  return (overrideTxt && overrideTxt[0] != '\0') ? overrideTxt : fallback;
}

const char* nicknameText() {
  return textOr(cfg.nickname, "Baby");
}

const char* signerText() {
  return textOr(cfg.signer, "Lolo");
}

const char* activeApSsid() {
  return textOr(cfg.apSsid, AP_SSID_DEFAULT);
}

const char* activeApPass() {
  if (cfg.apPass[0] != '\0') return cfg.apPass;
  return AP_PASS_DEFAULT;
}

void applySoftAP() {
  const char* ssid = activeApSsid();
  WiFi.softAPConfig(apIP, apGateway, apSubnet);
  if (cfg.apOpen) {
    WiFi.softAP(ssid);
  } else {
    WiFi.softAP(ssid, activeApPass());
  }
}

void copyArgToFieldN(char* dest, const String& value, size_t maxLen) {
  String v = value;
  v.trim();
  if (v.equalsIgnoreCase("unset")) v = "";
  if (maxLen < 2) return;
  strncpy(dest, v.c_str(), maxLen - 1);
  dest[maxLen - 1] = '\0';
}

void copyArgToField(char* dest, const String& value) {
  copyArgToFieldN(dest, value, TXT_LEN);
}

void serviceWifi() {
  dnsServer.processNextRequest();
  server.handleClient();
  yield();
}

// =====================================
// PROGMEM COPY  (future wife)
// =====================================

const char PROMISE_0[] PROGMEM = "I will stay.";
const char PROMISE_1[] PROGMEM = "I will listen.";
const char PROMISE_2[] PROGMEM = "I will grow";
const char PROMISE_3[] PROGMEM = "I will choose you.";

const char* const PROMISES[] PROGMEM = {
  PROMISE_0, PROMISE_1, PROMISE_2, PROMISE_3
};

const char PROMISE_SUB_2[] PROGMEM = "with you.";

const int PROMISE_COUNT = 4;

const char REASON_0[] PROGMEM = "Your laugh";
const char REASON_1[] PROGMEM = "Your calm";
const char REASON_2[] PROGMEM = "Your courage";
const char REASON_3[] PROGMEM = "Feels like home";
const char REASON_4[] PROGMEM = "Our future";
const char REASON_5[] PROGMEM = "This love";

const char* const REASONS[] PROGMEM = {
  REASON_0, REASON_1, REASON_2,
  REASON_3, REASON_4, REASON_5
};

const int REASON_COUNT = 6;


// =====================================
// MUSIC CLOCK  (drives animation sync)
// =====================================

bool musicPlaying = false;
unsigned int committedBeats = 0;
int currentNoteBeats = 0;
unsigned long noteStartMs = 0;
int currentNoteFreq = REST;

const unsigned long IDLE_SCENE_MS = 3500;
const unsigned long FRAME_INTERVAL = 45;

unsigned long lastAnimation = 0;
unsigned long sceneStartTime = 0;
int animationScene = 0;   // music: local scene; idle: global 0..18
int animationFrame = 0;

// =====================================
// MUSIC CLOCK HELPERS
// =====================================

float musicBeat() {
  if (!musicPlaying || currentNoteBeats <= 0) {
    return (float)committedBeats;
  }

  unsigned long elapsed = millis() - noteStartMs;
  float frac = (float)elapsed / (float)BEAT;

  if (frac < 0.0f) frac = 0.0f;
  if (frac > (float)currentNoteBeats) {
    frac = (float)currentNoteBeats;
  }

  return (float)committedBeats + frac;
}

int sceneFromMusicBeat(float beat) {
  const unsigned int* ends = sceneBeatEnds();
  int total = themeSceneCount();
  for (int i = 0; i < total - 1; i++) {
    if (beat < (float)ends[i]) {
      return i;
    }
  }
  return total - 1;
}

float sceneLocalBeat(float beat, int scene) {
  const unsigned int* ends = sceneBeatEnds();
  float start = 0.0f;
  if (scene > 0) {
    start = (float)ends[scene - 1];
  }
  return beat - start;
}

void beginMusicClock() {
  musicPlaying = true;
  committedBeats = 0;
  currentNoteBeats = 0;
  noteStartMs = millis();
  currentNoteFreq = REST;
  animationScene = 0;
  animationFrame = 0;
  sceneStartTime = millis();
}

void endMusicClock() {
  musicPlaying = false;
  committedBeats = 0;
  currentNoteBeats = 0;
  currentNoteFreq = REST;
}

void beginNoteClock(int frequency, int beats) {
  currentNoteFreq = frequency;
  currentNoteBeats = beats;
  noteStartMs = millis();
}

void commitNoteClock(int beats) {
  committedBeats += beats;
  currentNoteBeats = 0;
}

// =====================================
// CENTER TEXT
// =====================================

// Safe content band ends above the music progress bar (y=61..63)
const int CONTENT_BOTTOM = 58;

void centerText(const char* message, int y, int textSize) {
  if (message == NULL || message[0] == '\0') return;

  display.setTextSize(textSize);

  int16_t x1, y1;
  uint16_t w, h;

  display.getTextBounds(message, 0, 0, &x1, &y1, &w, &h);

  // Offset by x1 so glyphs with non-zero origin stay truly centered
  int x = (SCREEN_WIDTH - (int)w) / 2 - x1;
  display.setCursor(x, y);
  display.print(message);
}

void centerTextAt(const char* message, int y, int textSize, int shakeX) {
  if (message == NULL || message[0] == '\0') return;

  display.setTextSize(textSize);

  int16_t x1, y1;
  uint16_t w, h;

  display.getTextBounds(message, 0, 0, &x1, &y1, &w, &h);

  int x = (SCREEN_WIDTH - (int)w) / 2 - x1 + shakeX;
  display.setCursor(x, y);
  display.print(message);
}

// =====================================
// DRAW PRIMITIVES
// =====================================

void drawHeart(int x, int y, int size) {
  display.fillCircle(x - size / 2, y, size / 2 + 1, SSD1306_WHITE);
  display.fillCircle(x + size / 2, y, size / 2 + 1, SSD1306_WHITE);
  display.fillTriangle(
    x - size, y,
    x + size, y,
    x, y + size * 2,
    SSD1306_WHITE
  );
}

void drawOutlineHeart(int x, int y, int size) {
  display.drawCircle(x - size / 2, y, size / 2 + 1, SSD1306_WHITE);
  display.drawCircle(x + size / 2, y, size / 2 + 1, SSD1306_WHITE);
  display.drawLine(x - size, y + 1, x, y + size * 2, SSD1306_WHITE);
  display.drawLine(x + size, y + 1, x, y + size * 2, SSD1306_WHITE);
}

void drawSparkle(int x, int y, int size) {
  display.drawLine(x - size, y, x + size, y, SSD1306_WHITE);
  display.drawLine(x, y - size, x, y + size, SSD1306_WHITE);

  if (size >= 3) {
    display.drawLine(x - 1, y - 1, x + 1, y + 1, SSD1306_WHITE);
    display.drawLine(x - 1, y + 1, x + 1, y - 1, SSD1306_WHITE);
  }

  display.drawPixel(x, y, SSD1306_WHITE);
}

void drawDiamond(int x, int y, int size) {
  display.drawLine(x, y - size, x + size, y, SSD1306_WHITE);
  display.drawLine(x + size, y, x, y + size, SSD1306_WHITE);
  display.drawLine(x, y + size, x - size, y, SSD1306_WHITE);
  display.drawLine(x - size, y, x, y - size, SSD1306_WHITE);
  display.drawLine(x - size, y, x + size, y, SSD1306_WHITE);
  display.drawLine(x, y - size, x, y + size, SSD1306_WHITE);
}

void drawStars(int phase) {
  const int starX[] = { 8, 23, 43, 64, 85, 105, 120 };
  const int starY[] = { 16, 30, 13, 23, 12, 31, 17 };

  for (int i = 0; i < 7; i++) {
    int pulse = (phase + i * 3) % 12;

    if (pulse < 4) {
      display.drawPixel(starX[i], starY[i], SSD1306_WHITE);
    } else if (pulse < 7) {
      drawSparkle(starX[i], starY[i], 2);
    }
  }
}

void drawCheck(int x, int y) {
  display.drawLine(x, y + 3, x + 2, y + 5, SSD1306_WHITE);
  display.drawLine(x + 2, y + 5, x + 7, y, SSD1306_WHITE);
}

void drawBalloon(int x, int y, int r) {
  display.drawCircle(x, y, r, SSD1306_WHITE);
  display.drawLine(x, y + r, x, y + r + 6, SSD1306_WHITE);
  display.drawPixel(x - 2, y - 2, SSD1306_WHITE);
}

void drawPopBurst(int x, int y, int frame) {
  int s = 3 + frame * 2;
  drawSparkle(x, y, s);
  display.drawPixel(x - s, y - s / 2, SSD1306_WHITE);
  display.drawPixel(x + s, y - s / 2, SSD1306_WHITE);
  display.drawPixel(x - s / 2, y + s, SSD1306_WHITE);
  display.drawPixel(x + s / 2, y + s, SSD1306_WHITE);
}

// Soft quarter-note envelope 0..1 (peaks on the beat)
float beatPulse() {
  if (musicPlaying) {
    float b = musicBeat();
    float q = fmodf(b, 4.0f) / 4.0f;
    if (q < 0.45f) {
      return 1.0f - (q / 0.45f);
    }
    return 0.0f;
  }

  int phase = animationFrame % 16;
  return (phase < 8) ? (1.0f - phase / 8.0f) : 0.0f;
}

// Stronger hit on every half note (bass feel)
float halfNotePulse() {
  if (musicPlaying) {
    float b = musicBeat();
    float h = fmodf(b, 8.0f) / 8.0f;
    if (h < 0.35f) {
      return 1.0f - (h / 0.35f);
    }
    return 0.0f;
  }
  return beatPulse();
}

int beatPhaseInt() {
  if (musicPlaying) {
    return (int)musicBeat();
  }
  return animationFrame;
}

float pitchGlow() {
  if (!musicPlaying || currentNoteFreq <= 0) {
    return 0.3f;
  }
  float t = (float)(currentNoteFreq - D4) / (float)(A5 - D4);
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  return t;
}

void drawMusicProgress() {
  if (!musicPlaying) return;

  const float TOTAL = 203.0f;
  float b = musicBeat();
  float t = b / TOTAL;
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;

  int w = (int)(t * (SCREEN_WIDTH - 4));
  display.drawFastHLine(2, 62, SCREEN_WIDTH - 4, SSD1306_WHITE);
  if (w > 0) {
    display.fillRect(2, 61, w, 3, SSD1306_WHITE);
  }
}

// Effective localBeat for idle: stretch animationFrame into scene feel
float idleLocal(float sceneLen) {
  float t = (float)(animationFrame % 48) / 48.0f;
  return t * sceneLen;
}
// =====================================
// SCENE 0 - BOOT / SPLASH
// =====================================

void drawBootScene(float localBeat) {
  float lb = musicPlaying ? localBeat : idleLocal(24.0f);
  float pulse = halfNotePulse();
  int phase = beatPhaseInt();

  drawStars(phase / 2);

  int heartSize = (pulse > 0.35f) ? 7 : 5;

  if (lb < 12.0f) {
    drawHeart(64, 18, heartSize);
    centerText("FOR YOU", 40, 1);
  } else {
    drawOutlineHeart(64, 14, 4 + (pulse > 0.3f ? 1 : 0));
    centerText(nicknameText(), 34, 2);
  }
}

// =====================================
// SCENE 1 - PIN GATE (auto theater)
// =====================================

void drawPinScene(float localBeat) {
  float lb = musicPlaying ? localBeat : idleLocal(44.0f);
  float pulse = beatPulse();

  int shakeX = 0;
  bool unlocked = lb >= 32.0f;

  if (lb >= 18.0f && lb < 22.0f) {
    int shakePhase = (int)((lb - 18.0f) * 6.0f);
    shakeX = ((shakePhase % 2) == 0) ? 2 : -2;
  }

  if (unlocked) {
    if ((int)(lb * 4.0f) % 2 == 0 && lb < 38.0f) {
      display.fillRect(0, 0, SCREEN_WIDTH, CONTENT_BOTTOM, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }

    centerTextAt("Unlocked!", 18, 2, 0);
    display.setTextColor(SSD1306_WHITE);
    if (pulse > 0.2f) {
      drawHeart(64, 44, 5);
    } else {
      drawOutlineHeart(64, 44, 4);
    }
    return;
  }

  centerTextAt("Private invite", 2, 1, shakeX);
  centerTextAt("Enter code", 14, 1, shakeX);

  int filled = (int)(lb / 2.5f);
  if (filled < 0) filled = 0;
  if (filled > 6) filled = 6;

  char dots[8];
  for (int i = 0; i < 6; i++) {
    dots[i] = (i < filled) ? '*' : '-';
  }
  dots[6] = '\0';

  centerTextAt(dots, 30, 2, shakeX);

  if (lb >= 18.0f && lb < 22.0f) {
    centerTextAt("Try again", 50, 1, shakeX);
  } else {
    centerTextAt("6 digits", 50, 1, shakeX);
  }
}

// =====================================
// SCENE 2 - INTRO
// =====================================

void drawIntroScene(float localBeat) {
  float lb = musicPlaying ? localBeat : idleLocal(32.0f);
  float pulse = beatPulse();
  int page = (int)(lb / (32.0f / 3.0f));
  if (page > 2) page = 2;
  if (!musicPlaying) {
    page = (animationFrame / 16) % 3;
  }

  if (page == 0) {
    centerText("For you", 8, 1);

    const char* name = nicknameText();
    int nameLen = (int)strlen(name);
    if (nameLen > 12) nameLen = 12;
    int chars = (int)(lb * 2.0f) + 1;
    if (chars > nameLen) chars = nameLen;
    if (!musicPlaying) chars = nameLen;
    if (chars < 0) chars = 0;

    char buf[13];
    for (int i = 0; i < chars; i++) buf[i] = name[i];
    buf[chars] = '\0';
    centerText(buf, 24, 2);

    int bob = (int)(pulse * 2.0f);
    drawOutlineHeart(64, 48 - bob, 4);
  } else if (page == 1) {
    centerText("Made with love", 18, 1);
    centerText("I have for you", 34, 1);
    if (pulse > 0.3f) {
      drawSparkle(64, 50, 3);
    }
  } else {
    char signLine[20];
    snprintf(signLine, sizeof(signLine), "- %s", signerText());
    centerText(signLine, 16, 1);
    centerText("Open gift >", 32, 1);
    drawHeart(64, 48, pulse > 0.35f ? 5 : 4);
  }
}

// =====================================
// SCENE 3 - BALLOONS (13 pops)
// =====================================

void drawBalloonsScene(float localBeat) {
  float lb = musicPlaying ? localBeat : idleLocal(32.0f);
  const float PER = 2.3f;
  int idx = (int)(lb / PER);
  if (idx < 0) idx = 0;

  centerText("Pop balloons", 0, 1);

  if (idx >= BALLOON_COUNT) {
    centerText("All popped!", 24, 2);
    drawHeart(64, 48, 5);
    return;
  }

  char counter[12];
  snprintf(counter, sizeof(counter), "%d/13", idx + 1);
  centerText(counter, 10, 1);

  // Balloon row centered, clear of caption band
  int start = (idx / 5) * 5;
  int countInRow = BALLOON_COUNT - start;
  if (countInRow > 5) countInRow = 5;
  int rowWidth = (countInRow - 1) * 22;
  int rowLeft = (SCREEN_WIDTH - rowWidth) / 2;

  for (int i = 0; i < countInRow; i++) {
    int bi = start + i;
    int x = rowLeft + i * 22;
    int y = 26;
    if (bi < idx) {
      display.drawLine(x - 4, y - 4, x + 4, y + 4, SSD1306_WHITE);
      display.drawLine(x + 4, y - 4, x - 4, y + 4, SSD1306_WHITE);
    } else if (bi == idx) {
      float frac = fmodf(lb, PER);
      int bob = (int)(sinf(lb * 2.0f) * 2.0f);
      if (frac > PER - 0.6f) {
        drawPopBurst(x, y + bob, (int)((frac - (PER - 0.6f)) * 5.0f));
      } else {
        drawBalloon(x, y + bob, 5);
      }
    } else {
      drawBalloon(x, y, 5);
    }
  }

  loadOverrideOrTable(cfg.balloonL1[idx], BALLOON_L1, idx);
  centerText(flashBuf, 40, 1);
  loadOverrideOrTable(cfg.balloonL2[idx], BALLOON_L2, idx);
  centerText(flashBuf, 50, 1);
}

// =====================================
// SCENE 4 - GALLERY / MEMORIES
// =====================================

void drawGalleryScene(float localBeat) {
  float lb = musicPlaying ? localBeat : idleLocal(16.0f);
  const float PER = 16.0f / (float)GALLERY_COUNT;
  int idx = (int)(lb / PER);
  if (idx < 0) idx = 0;
  if (idx >= GALLERY_COUNT) idx = GALLERY_COUNT - 1;
  if (!musicPlaying) {
    idx = (animationFrame / 8) % GALLERY_COUNT;
  }

  float frac = fmodf(lb, PER) / PER;
  int slide = 0;
  if (frac < 0.15f && musicPlaying) {
    slide = (int)((0.15f - frac) / 0.15f * 10.0f);
  }

  char header[16];
  snprintf(header, sizeof(header), "Memories %d/6", idx + 1);
  centerText(header, 0, 1);

  // Polaroid frame — fixed size, captions centered inside
  const int frameX = 14 + slide;
  const int frameY = 12;
  const int frameW = 100;
  const int frameH = 34;
  display.drawRect(frameX, frameY, frameW, frameH, SSD1306_WHITE);
  display.drawRect(frameX + 2, frameY + 2, frameW - 4, frameH - 4, SSD1306_WHITE);

  loadOverrideOrTable(cfg.galleryL1[idx], GALLERY_L1, idx);
  centerTextAt(flashBuf, 22, 1, slide);
  loadOverrideOrTable(cfg.galleryL2[idx], GALLERY_L2, idx);
  centerTextAt(flashBuf, 34, 1, slide);

  centerText("Next >", 50, 1);
}

// =====================================
// SCENE 5 - QUIZ (auto correct)
// =====================================

void drawQuizScene(float localBeat) {
  float lb = musicPlaying ? localBeat : idleLocal(16.0f);
  const float PER = 16.0f / (float)QUIZ_COUNT;
  int qi = (int)(lb / PER);
  if (qi < 0) qi = 0;
  if (qi >= QUIZ_COUNT) qi = QUIZ_COUNT - 1;
  if (!musicPlaying) {
    qi = (animationFrame / 12) % QUIZ_COUNT;
  }

  float frac = fmodf(lb, PER);
  bool showCheck = musicPlaying && (frac > PER - 1.0f);
  bool highlight = beatPulse() > 0.4f;

  char header[12];
  snprintf(header, sizeof(header), "Quiz %d/4", qi + 1);
  centerText(header, 0, 1);

  loadOverrideOrTable(cfg.quizQ[qi], QUIZ_Q, qi);
  centerText(flashBuf, 10, 1);

  uint8_t correct = (cfg.quizCorrect[qi] >= 0)
    ? (uint8_t)cfg.quizCorrect[qi]
    : pgm_read_byte(&QUIZ_CORRECT[qi]);

  // Single column, tight rows — stay above progress bar (y<=58)
  for (int o = 0; o < 4; o++) {
    loadOverrideOrTable(cfg.quizOpt[qi * 4 + o], QUIZ_OPT, qi * 4 + o);
    int y = 20 + o * 9;

    if (o == correct && (showCheck || highlight)) {
      display.fillRect(0, y - 1, SCREEN_WIDTH, 9, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setTextSize(1);
      display.setCursor(4, y);
      display.print(flashBuf);
      display.setTextColor(SSD1306_WHITE);
      if (showCheck) {
        drawCheck(116, y);
      }
    } else {
      display.setTextSize(1);
      display.setCursor(4, y);
      display.print(flashBuf);
    }
  }
}

// =====================================
// SCENE 6 - CAKE / MAKE A WISH
// =====================================

void drawCakeScene(float localBeat) {
  float lb = musicPlaying ? localBeat : idleLocal(16.0f);
  float pulse = beatPulse();

  bool blown = lb >= 10.0f;
  bool done = lb >= 13.0f;

  if (done) {
    centerText("Wish made!", 16, 2);
    centerText("Open letter", 40, 1);
    if (pulse > 0.3f) {
      drawSparkle(18, 28, 2);
      drawSparkle(110, 28, 2);
      drawSparkle(36, 52, 2);
      drawSparkle(92, 52, 2);
    }
    return;
  }

  centerText("Make a wish", 0, 1);

  // Cake body sits mid-screen; hint stays below it (no white-on-white)
  display.fillRect(32, 32, 64, 12, SSD1306_WHITE);
  display.drawRect(28, 30, 72, 16, SSD1306_WHITE);

  const int cx[] = { 48, 64, 80 };
  for (int i = 0; i < 3; i++) {
    int x = cx[i];
    display.drawFastVLine(x, 16, 14, SSD1306_WHITE);
    display.drawFastVLine(x + 1, 16, 14, SSD1306_WHITE);

    if (!blown) {
      int jitter = ((beatPhaseInt() + i * 3) % 2);
      int fy = 13 - jitter;
      display.fillCircle(x, fy, 2, SSD1306_WHITE);
      if (pulse > 0.4f) {
        display.drawPixel(x, fy - 3, SSD1306_WHITE);
      }
    } else {
      int smokeY = 12 - (int)((lb - 10.0f) * 2.0f) - i;
      if (smokeY > 0) {
        display.drawPixel(x, smokeY, SSD1306_WHITE);
        display.drawPixel(x + 1, smokeY - 2, SSD1306_WHITE);
      }
    }
  }

  if (blown && !done) {
    for (int w = 0; w < 3; w++) {
      int wy = 14 + w * 4;
      display.drawFastHLine(6, wy, 14, SSD1306_WHITE);
    }
    drawSparkle(16, 24, 2);
    drawSparkle(112, 20, 2);
  }

  centerText(blown ? "..." : "Blow / press", 50, 1);
}

// =====================================
// SCENE 7 - FINALE LETTER
// =====================================

void drawFinaleScene(float localBeat) {
  float lb = musicPlaying ? localBeat : idleLocal(23.0f);
  const float PER = 2.0f;
  int page = (int)(lb / PER);
  if (page < 0) page = 0;
  if (page >= FINALE_PAGES) page = FINALE_PAGES - 1;
  if (!musicPlaying) {
    page = (animationFrame / 10) % FINALE_PAGES;
  }

  float pulse = beatPulse();
  bool big = (page == 0);

  loadOverrideOrTable(cfg.finaleL0[page], FINALE_L0, page);
  if (flashBuf[0] != '\0') {
    centerText(flashBuf, big ? 6 : 10, 1);
  }

  loadOverrideOrTable(cfg.finaleL1[page], FINALE_L1, page);
  if (flashBuf[0] != '\0') {
    centerText(flashBuf, big ? 22 : 26, big ? 2 : 1);
  }

  loadOverrideOrTable(cfg.finaleL2[page], FINALE_L2, page);
  if (flashBuf[0] != '\0') {
    if (flashBuf[0] == '<' && flashBuf[1] == '3') {
      drawHeart(64, 44, pulse > 0.3f ? 6 : 4);
    } else {
      centerText(flashBuf, 40, 1);
    }
  }

  if (page < FINALE_PAGES - 1) {
    centerText("v", 52, 1);
  } else {
    int hs = pulse > 0.25f ? 5 : 3;
    drawHeart(64, 50, hs);
  }
}

// Heart outline vertices for constellation linking
// Kept inside y 8..42 so caption + progress bar stay clear
const int HEART_PTS = 8;
const int8_t HEART_PX[HEART_PTS] = { 64, 52, 44, 48, 64, 80, 84, 76 };
const int8_t HEART_PY[HEART_PTS] = { 10, 14, 22, 32, 42, 32, 22, 14 };

// =====================================
// SCENE 0 - DEDICATION  (bass early)
// =====================================

void drawDedicationScene(float localBeat) {
  float lb = musicPlaying ? localBeat : idleLocal(32.0f);
  float pulse = halfNotePulse();
  int phase = beatPhaseInt();

  drawStars(phase / 2);

  // Typewriter "For my" over first ~10 beats
  const char* forMy = "For my";
  int chars = (int)(lb * 0.7f) + 1;
  if (chars > 6) chars = 6;
  if (!musicPlaying) chars = 6;

  char buf[8];
  for (int i = 0; i < chars; i++) buf[i] = forMy[i];
  buf[chars] = '\0';
  centerText(buf, 4, 1);

  // Size-2 "future wife" is 132px wide — split to fit 128px
  if (lb >= 10.0f || !musicPlaying) {
    int bob = (int)(pulse * 2.0f);
    centerText("future", 18 - bob, 2);
  }
  if (lb >= 14.0f || !musicPlaying) {
    int bob = (int)(pulse * 2.0f);
    centerText("wife", 36 - bob, 2);
  }

  if (lb >= 22.0f || !musicPlaying) {
    drawOutlineHeart(64, 54, pulse > 0.3f ? 4 : 3);
  } else if (lb >= 18.0f) {
    drawSparkle(64, 54, 2);
  }
}

// =====================================
// SCENE 1 - CONSTELLATION  (bass late)
// =====================================

void drawConstellationScene(float localBeat) {
  float lb = musicPlaying ? localBeat : idleLocal(36.0f);
  float half = halfNotePulse();
  int phase = beatPhaseInt();

  drawStars(phase / 2);

  // Reveal heart points one by one across the scene
  int revealed = (int)(lb / 4.0f) + 1;
  if (revealed > HEART_PTS) revealed = HEART_PTS;
  if (!musicPlaying) revealed = HEART_PTS;

  for (int i = 0; i < revealed; i++) {
    int x = HEART_PX[i];
    int y = HEART_PY[i];
    if (half > 0.35f) {
      drawSparkle(x, y, 2);
    } else {
      display.fillCircle(x, y, 1, SSD1306_WHITE);
    }

    if (i > 0) {
      display.drawLine(
        HEART_PX[i - 1], HEART_PY[i - 1],
        x, y,
        SSD1306_WHITE
      );
    }
  }

  // Close the loop once all points are lit
  if (revealed >= HEART_PTS) {
    display.drawLine(
      HEART_PX[HEART_PTS - 1], HEART_PY[HEART_PTS - 1],
      HEART_PX[0], HEART_PY[0],
      SSD1306_WHITE
    );
    if (half > 0.4f) {
      drawOutlineHeart(64, 24, 3);
    }
  }

  if (lb >= 20.0f || !musicPlaying) {
    centerText("Always", 50, 1);
  }
}

// =====================================
// SCENE 2 - TWO PATHS  (m13–14)
// =====================================

void drawTwoPathsScene(float localBeat) {
  float lb = musicPlaying ? localBeat : idleLocal(32.0f);
  float pulse = beatPulse();

  // Approach across first measure, meet/hold through second
  float t;
  if (lb < 16.0f) {
    t = lb / 16.0f;
  } else {
    t = 1.0f;
  }
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;

  float ease = t * t * (3.0f - 2.0f * t);

  int leftX = 8 + (int)(ease * 52.0f);
  int rightX = 120 - (int)(ease * 52.0f);
  int pathY = 22 - (int)(pulse * 2.0f);

  // Trail lines from edges
  int leftLen = leftX - 4;
  if (leftLen > 0) {
    display.drawFastHLine(4, pathY, leftLen, SSD1306_WHITE);
  }
  int rightLen = 124 - rightX;
  if (rightLen > 0) {
    display.drawFastHLine(rightX, pathY, rightLen, SSD1306_WHITE);
  }

  display.fillCircle(leftX, pathY, 3, SSD1306_WHITE);
  display.fillCircle(rightX, pathY, 3, SSD1306_WHITE);

  if (t >= 0.98f) {
    int hs = pulse > 0.3f ? 5 : 4;
    drawHeart(64, pathY - 1, hs);
    if (pulse > 0.25f) {
      drawSparkle(48, pathY - 10, 2);
      drawSparkle(80, pathY - 10, 2);
    }
    centerText("one road", 42, 1);
  } else {
    centerText("two paths", 42, 1);
  }

  centerText("meet", 52, 1);
}

// =====================================
// SCENE 3 - PROMISES  (m15–16)
// =====================================

void drawPromisesScene(float localBeat) {
  float lb = musicPlaying ? localBeat : idleLocal(32.0f);
  float pulse = beatPulse();

  const float PER = 8.0f;
  int idx = (int)(lb / PER);
  if (idx < 0) idx = 0;
  if (idx >= PROMISE_COUNT) idx = PROMISE_COUNT - 1;
  if (!musicPlaying) {
    idx = (animationFrame / 12) % PROMISE_COUNT;
  }

  centerText("Promises", 4, 1);

  loadOverrideOrTable(cfg.promises[idx], PROMISES, idx);
  centerText(flashBuf, 22, 1);

  int underlineY = 34;
  if (idx == 2) {
    if (cfg.promiseSub2[0] != '\0') {
      strncpy(flashBuf, cfg.promiseSub2, sizeof(flashBuf) - 1);
      flashBuf[sizeof(flashBuf) - 1] = '\0';
    } else {
      loadFlash(PROMISE_SUB_2);
    }
    centerText(flashBuf, 34, 1);
    underlineY = 46;
  }

  // Pulse underline grows within each vow window
  float frac = fmodf(lb, PER) / PER;
  if (!musicPlaying) frac = 1.0f;
  int lineW = (int)(20 + frac * 70);
  display.drawFastHLine(64 - lineW / 2, underlineY, lineW, SSD1306_WHITE);

  char counter[8];
  snprintf(counter, sizeof(counter), "%d/4", idx + 1);
  centerText(counter, 52, 1);

  if (pulse > 0.4f) {
    drawSparkle(10, 24, 2);
    drawSparkle(118, 24, 2);
  }
}

// =====================================
// SCENE 4 - WHY YOU  (m17–18)
// =====================================

void drawWhyYouScene(float localBeat) {
  float lb = musicPlaying ? localBeat : idleLocal(32.0f);
  float pulse = beatPulse();
  float glow = pitchGlow();

  const float PER = 32.0f / (float)REASON_COUNT;
  int idx = (int)(lb / PER);
  if (idx < 0) idx = 0;
  if (idx >= REASON_COUNT) idx = REASON_COUNT - 1;
  if (!musicPlaying) {
    idx = (animationFrame / 10) % REASON_COUNT;
  }

  centerText("Why you", 2, 1);

  display.drawRoundRect(6, 14, 116, 28, 4, SSD1306_WHITE);

  loadOverrideOrTable(cfg.reasons[idx], REASONS, idx);
  centerText(flashBuf, 24, 1);

  char counter[8];
  snprintf(counter, sizeof(counter), "%d/6", idx + 1);
  centerText(counter, 48, 1);

  if (pulse > 0.3f || glow > 0.6f) {
    drawSparkle(18, 20, 2);
    drawSparkle(110, 20, 2);
  } else {
    drawOutlineHeart(18, 50, 3);
    drawOutlineHeart(110, 50, 3);
  }
}

// =====================================
// SCENE 5 - SEAL  (m19–cadence)
// =====================================

void drawSealScene(float localBeat) {
  float lb = musicPlaying ? localBeat : idleLocal(40.0f);
  float pulse = beatPulse();
  float half = halfNotePulse();

  float t = lb / 40.0f;
  if (t > 1.0f) t = 1.0f;
  if (t < 0.0f) t = 0.0f;

  if (lb < 12.0f && musicPlaying) {
    centerText(textOr(cfg.fwSealTitle, "My future wife"), 16, 1);
    drawOutlineHeart(64, 36, 4 + (pulse > 0.3f ? 1 : 0));
  } else {
    // Cap heart so tip stays above caption band
    int heartSize = 4 + (int)(t * 3.0f);
    if (pulse > 0.25f) heartSize += 1;
    if (heartSize > 8) heartSize = 8;
    drawHeart(64, 14, heartSize);

    if (half > 0.35f) {
      int ringR = 10 + (int)(t * 6.0f);
      if (ringR > 16) ringR = 16;
      display.drawCircle(64, 18, ringR, SSD1306_WHITE);
    }

    centerText(textOr(cfg.fwSealChoose, "I choose you."), 40, 1);

    if (lb >= 24.0f || !musicPlaying) {
      centerText(textOr(cfg.fwSealSign, "- forever yours"), 52, 1);
    }
  }

  // Cadence flash on final A5 hold
  if (musicPlaying && currentNoteFreq == A5 && currentNoteBeats >= 4) {
    drawSparkle(36, 16, 3);
    drawSparkle(92, 16, 3);
    drawSparkle(64, 4, 2);
  } else if (pulse > 0.4f) {
    drawSparkle(24, 14, 2);
    drawSparkle(104, 14, 2);
  }
}

// =====================================
// SCENE 0 - WEDDING TITLE  (bass)
// =====================================

void drawTitleScene(float localBeat) {
  float pulse = halfNotePulse();
  int phase = beatPhaseInt();

  // Border breathes on half notes; leave room for progress bar
  int inset = (pulse > 0.5f) ? 2 : 4;
  display.drawRoundRect(
    inset, inset,
    SCREEN_WIDTH - inset * 2,
    CONTENT_BOTTOM - inset,
    5,
    SSD1306_WHITE
  );

  drawStars(phase / 2);

  // Bass step meter — one segment per half-note of the intro
  if (musicPlaying) {
    int step = (int)(localBeat / 8.0f);
    if (step > 7) step = 7;
    if (step < 0) step = 0;

    for (int i = 0; i <= step; i++) {
      int x = 28 + i * 10;
      display.fillRect(x, 8, 7, 2, SSD1306_WHITE);
    }
  }

  // Title fades in over first two bass notes
  if (localBeat >= 4.0f || !musicPlaying) {
    centerText(textOr(cfg.wedTitle, "OUR WEDDING"), 16, 1);
  }

  // Mid ornaments sit between title and subtitle
  drawOutlineHeart(24, 34, 4);
  drawOutlineHeart(104, 34, 4);

  if (pulse > 0.35f) {
    drawDiamond(64, 34, 5);
  } else {
    drawSparkle(64, 34, 4);
  }

  if (localBeat >= 16.0f || !musicPlaying) {
    centerText(textOr(cfg.wedSubtitle, "A LOVE STORY"), 48, 1);
  }
}

// =====================================
// SCENE 1 - WEDDING RINGS  (m13–14)
// =====================================

void drawRingsScene(float localBeat) {
  // Approach across first measure, hold interlocking through second
  float t;
  if (localBeat < 16.0f) {
    t = localBeat / 16.0f;
  } else {
    t = 1.0f;
  }
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;

  float ease = t * t * (3.0f - 2.0f * t);

  int movement = (int)(ease * 11.0f);
  int leftRing = 48 + movement;
  int rightRing = 80 - movement;
  int ringY = 26;

  // Soft vertical bob on the quarter
  int bob = (int)(beatPulse() * 2.0f);
  ringY -= bob;

  display.drawCircle(leftRing, ringY, 12, SSD1306_WHITE);
  display.drawCircle(leftRing, ringY, 10, SSD1306_WHITE);
  display.drawCircle(rightRing, ringY, 12, SSD1306_WHITE);
  display.drawCircle(rightRing, ringY, 10, SSD1306_WHITE);

  // Band highlights
  display.drawPixel(leftRing - 5, ringY - 9, SSD1306_WHITE);
  display.drawPixel(rightRing + 5, ringY - 9, SSD1306_WHITE);

  float pulse = beatPulse();
  float glow = pitchGlow();

  int sparkleSize = (pulse > 0.4f || glow > 0.7f) ? 5 : 3;
  drawDiamond(64, 8, sparkleSize);

  // Side sparkles intensify with higher melody notes
  if (glow > 0.45f || pulse > 0.3f) {
    drawSparkle(18, 22, 3);
    drawSparkle(110, 22, 3);
  }

  // “Locked” flash once rings fully meet
  if (t >= 0.98f && pulse > 0.2f) {
    drawSparkle(64, ringY, 4);
  }

  centerText(textOr(cfg.wedRings, "FOREVER TOGETHER"), 48, 1);
}

// =====================================
// SCENE 2 - FLOATING HEARTS  (m15–16)
// =====================================

void drawHeartsScene(float localBeat) {
  int phase = beatPhaseInt();
  float pulse = beatPulse();
  const int captionTop = 42;

  for (int i = 0; i < 5; i++) {
    float speed = 0.9f + (i % 3) * 0.45f;
    float offset = i * 14.0f;

    // Rise locked to tempo so float rate matches the piece
    float yf = 70.0f - fmodf(localBeat * speed + offset, 86.0f);
    int y = (int)yf;
    int x = 12 + i * 26;
    x += (int)(sinf((localBeat + offset) * 0.40f) * 5.0f);

    int size = (i % 2 == 0) ? 4 : 3;
    if (pulse > 0.55f && (i % 2 == 0)) {
      size += 1;
    }

    if (y > -10 && y < captionTop - 2) {
      drawHeart(x, y, size);

      // Soft trail dots behind rising hearts
      int trailY = y + size * 3 + 4;
      if (trailY < captionTop - 2) {
        display.drawPixel(x, trailY, SSD1306_WHITE);
      }
    }
  }

  drawStars(phase / 2);

  display.drawFastHLine(0, captionTop, SCREEN_WIDTH, SSD1306_WHITE);
  centerText(textOr(cfg.wedHearts1, "TWO HEARTS"), captionTop + 2, 1);
  centerText(textOr(cfg.wedHearts2, "ONE LOVE"), captionTop + 11, 1);
}

// =====================================
// SCENE 3 - ROMANTIC MESSAGE  (m17–18)
// =====================================

void drawMessageScene(float localBeat) {
  float pulse = beatPulse();
  int phase = beatPhaseInt();

  // Frame clears the progress bar
  display.drawRoundRect(4, 2, 120, CONTENT_BOTTOM - 4, 5, SSD1306_WHITE);

  drawOutlineHeart(18, 12, 3);
  drawOutlineHeart(110, 12, 3);
  drawStars(phase / 2);

  // Phrase reveals on the beat of each half of the scene
  if (localBeat >= 0.0f || !musicPlaying) {
    centerText(textOr(cfg.wedMsg1, "YOU ARE MY"), 12, 1);
  }

  if (localBeat >= 8.0f || !musicPlaying) {
    centerText(textOr(cfg.wedMsg2, "FOREVER"), 24, 2);
  }

  // Underline grows through the two measures (under FOREVER)
  float t = localBeat / 32.0f;
  if (t > 1.0f) t = 1.0f;
  if (t < 0.0f) t = 0.0f;
  int lineW = (int)(16 + t * 72);
  display.drawFastHLine(64 - lineW / 2, 42, lineW, SSD1306_WHITE);

  if (localBeat >= 20.0f || !musicPlaying) {
    centerText(textOr(cfg.wedMsg3, "& ALWAYS"), 46, 1);
  }

  if (pulse > 0.35f || pitchGlow() > 0.65f) {
    drawSparkle(64, 6, 3);
  } else {
    drawDiamond(64, 6, 3);
  }
}

// =====================================
// SCENE 4 - FOREVER & ALWAYS  (m19–end)
// =====================================

void drawFinalScene(float localBeat) {
  float pulse = beatPulse();
  float half = halfNotePulse();

  // Concentric outline ring expands with phrase progress
  // Cap radius so the ring stays above the caption band
  float t = localBeat / 40.0f;
  if (t > 1.0f) t = 1.0f;
  if (t < 0.0f) t = 0.0f;

  const int ringCX = 64;
  const int ringCY = 22;
  int ringR = 8 + (int)(t * 12.0f);
  display.drawCircle(ringCX, ringCY, ringR, SSD1306_WHITE);
  if (half > 0.4f) {
    display.drawCircle(ringCX, ringCY, ringR + 2, SSD1306_WHITE);
  }

  int heartSize = (pulse > 0.25f) ? 7 : 5;
  drawHeart(ringCX, ringCY - 2, heartSize);

  // Alternating sparkle pairs on eighths
  int eighth = (int)(localBeat * 2.0f);
  if ((eighth % 2) == 0) {
    drawSparkle(28, 12, 3);
    drawSparkle(100, 12, 3);
  } else {
    drawSparkle(20, 32, 2);
    drawSparkle(108, 32, 2);
  }

  drawDiamond(64, 4, (pulse > 0.4f) ? 4 : 3);

  // Cadence: when the final A5 hold is near, flash extra sparkles
  if (musicPlaying && currentNoteFreq == A5 && currentNoteBeats >= 4) {
    drawSparkle(46, 22, 3);
    drawSparkle(82, 22, 3);
  }

  centerText(textOr(cfg.wedFinal1, "FOREVER & ALWAYS"), 44, 1);
  centerText(textOr(cfg.wedFinal2, "LOVE"), 53, 1);
}

// =====================================
// MAIN ANIMATION
// =====================================

void drawThemeScene(int theme, int scene, float local) {
  if (theme == THEME_BIRTHDAY) {
    switch (scene) {
      case 0: drawBootScene(local); break;
      case 1: drawPinScene(local); break;
      case 2: drawIntroScene(local); break;
      case 3: drawBalloonsScene(local); break;
      case 4: drawGalleryScene(local); break;
      case 5: drawQuizScene(local); break;
      case 6: drawCakeScene(local); break;
      case 7: drawFinaleScene(local); break;
      default: drawBootScene(local); break;
    }
    return;
  }

  if (theme == THEME_FUTURE_WIFE) {
    switch (scene) {
      case 0: drawDedicationScene(local); break;
      case 1: drawConstellationScene(local); break;
      case 2: drawTwoPathsScene(local); break;
      case 3: drawPromisesScene(local); break;
      case 4: drawWhyYouScene(local); break;
      case 5: drawSealScene(local); break;
      default: drawDedicationScene(local); break;
    }
    return;
  }

  // THEME_WEDDING
  switch (scene) {
    case 0: drawTitleScene(local); break;
    case 1: drawRingsScene(local); break;
    case 2: drawHeartsScene(local); break;
    case 3: drawMessageScene(local); break;
    case 4: drawFinalScene(local); break;
    default: drawTitleScene(local); break;
  }
}

void drawCombinedAnimation() {
  if (!oledReady) return;

  float beat = musicPlaying ? musicBeat() : (float)animationFrame;
  int theme = currentTheme;
  int scene = animationScene;
  float local = 0.0f;

  if (musicPlaying) {
    int synced = sceneFromMusicBeat(beat);
    if (synced != animationScene) {
      animationScene = synced;
      sceneStartTime = millis();
      animationFrame = 0;
      Serial.print(themeName(currentTheme));
      Serial.print(" scene -> ");
      Serial.println(animationScene);
    }
    scene = animationScene;
    theme = currentTheme;
    local = sceneLocalBeat(beat, scene);
  } else {
    resolveIdleScene(animationScene, &theme, &scene);
    local = (float)animationFrame;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  drawThemeScene(theme, scene, local);

  drawMusicProgress();
  display.display();
  animationFrame++;
}

void updateAnimation() {
  if (!oledReady) return;

  unsigned long now = millis();

  if (!musicPlaying) {
    if (now - sceneStartTime >= IDLE_SCENE_MS) {
      sceneStartTime = now;
      animationScene++;
      if (animationScene >= IDLE_TOTAL_SCENES) {
        animationScene = 0;
      }
      animationFrame = 0;
    }
  }

  if (now - lastAnimation >= FRAME_INTERVAL) {
    lastAnimation = now;
    drawCombinedAnimation();
  }
}

// =====================================
// SENSOR CONTROL
// =====================================

bool musicEnabled() {
  return digitalRead(SENSOR_PIN) == HIGH;
}

// =====================================
// OLED POWER
// =====================================

void setDisplayPower(bool on) {
  if (!oledReady) return;
  if (on == displayOn) return;

  if (on) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    displayOn = true;
  } else {
    display.clearDisplay();
    display.display();
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    displayOn = false;
  }
}

// =====================================
// WAIT FOR DOOR OPEN
// =====================================

void waitForDoorOpen() {
  noTone(BUZZER_PIN);
  endMusicClock();
  setDisplayPower(false);

  Serial.println("Waiting for door to open...");

  while (!musicEnabled()) {
    serviceWifi();
    delay(20);
  }

  Serial.println("DOOR OPEN - MUSIC STARTING");

  setDisplayPower(true);
  animationScene = 0;
  animationFrame = 0;
  sceneStartTime = millis();
  lastAnimation = 0;

  unsigned long startTime = millis();
  while (millis() - startTime < 500) {
    if (!musicEnabled()) {
      setDisplayPower(false);
      return;
    }
    updateAnimation();
    serviceWifi();
    delay(10);
  }
}

// =====================================
// CONTROLLED DELAY
// =====================================

bool controlledDelay(unsigned long duration) {
  unsigned long startTime = millis();

  while (millis() - startTime < duration) {
    if (!musicEnabled()) {
      noTone(BUZZER_PIN);
      endMusicClock();
      setDisplayPower(false);
      Serial.println("DOOR CLOSED - MUSIC STOPPED");
      return false;
    }

    updateAnimation();
    serviceWifi();
    delay(4);
  }

  return true;
}

// =====================================
// PLAY NOTE
// =====================================

bool playNote(int frequency, int beats) {
  unsigned long duration = (unsigned long)BEAT * beats;

  beginNoteClock(frequency, beats);

  if (frequency == REST) {
    noTone(BUZZER_PIN);

    if (!controlledDelay(duration)) {
      return false;
    }

    commitNoteClock(beats);
    return true;
  }

  unsigned long soundTime =
    duration > (unsigned long)LEGATO_GAP
      ? duration - LEGATO_GAP
      : duration;

  tone(BUZZER_PIN, frequency);

  if (!controlledDelay(soundTime)) {
    noTone(BUZZER_PIN);
    return false;
  }

  noTone(BUZZER_PIN);

  if (!controlledDelay(duration - soundTime)) {
    return false;
  }

  commitNoteClock(beats);
  return true;
}

void logMeasureIfNeeded(int noteIndex) {
  for (int m = 0; m < MEASURE_COUNT; m++) {
    if (noteIndex == MEASURE_NOTE_INDEX[m]) {
      Serial.print("--- MEASURE ");
      Serial.print(MEASURE_NUMBER[m]);
      Serial.println(" ---");
      return;
    }
  }
}

// =====================================
// WIFI WEB UI (friendly + captive portal)
// =====================================

void sendRaw(const char* s) {
  if (s) server.sendContent(s);
}

void sendEscaped(const char* s) {
  if (!s) return;
  for (const char* p = s; *p; p++) {
    char c = *p;
    if (c == '&') server.sendContent(F("&amp;"));
    else if (c == '<') server.sendContent(F("&lt;"));
    else if (c == '>') server.sendContent(F("&gt;"));
    else if (c == '"') server.sendContent(F("&quot;"));
    else {
      char one[2] = { c, 0 };
      server.sendContent(one);
    }
  }
}

void sendPageStart(const char* title) {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  server.sendContent(F(
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>"
  ));
  sendRaw(title);
  server.sendContent(F(
    "</title><style>"
    "body{font-family:system-ui,-apple-system,sans-serif;margin:0;background:#f6f3ee;color:#1c1917}"
    "main{max-width:480px;margin:0 auto;padding:16px 16px 40px}"
    "h1{font-size:1.35rem;margin:8px 0 4px}"
    "h2{font-size:1.05rem;margin:18px 0 8px}"
    "p,li{font-size:.95rem;line-height:1.4}"
    ".hint{color:#57534e;margin:0 0 14px}"
    ".card{display:block;background:#fff;border:1px solid #e7e5e4;border-radius:14px;"
    "padding:16px;margin:10px 0;text-decoration:none;color:inherit;box-shadow:0 1px 2px rgba(0,0,0,.04)}"
    ".card strong{display:block;font-size:1.05rem;margin-bottom:4px}"
    ".card span{color:#57534e;font-size:.9rem}"
    "label{display:block;margin:12px 0 4px;font-weight:600;font-size:.92rem}"
    "input,select,textarea{width:100%;box-sizing:border-box;padding:12px;border-radius:10px;"
    "border:1px solid #d6d3d1;background:#fff;font-size:1rem}"
    "button,.btn{display:inline-block;width:100%;box-sizing:border-box;margin:10px 0 0;"
    "padding:14px 16px;border:0;border-radius:12px;background:#0f766e;color:#fff;"
    "font-size:1rem;font-weight:700;text-align:center;text-decoration:none}"
    ".btn-secondary{background:#44403c}"
    ".btn-danger{background:#b91c1c}"
    ".back{display:inline-block;margin:4px 0 12px;color:#0f766e;font-weight:600;text-decoration:none}"
    ".ok{background:#ecfdf5;border:1px solid #a7f3d0;border-radius:12px;padding:14px;margin:12px 0}"
    ".warn{background:#fff7ed;border:1px solid #fed7aa;border-radius:12px;padding:12px;margin:12px 0;font-size:.92rem}"
    ".row{display:flex;gap:8px}.row>*{flex:1}"
    "</style></head><body><main>"
  ));
}

void sendPageEnd() {
  server.sendContent(F("</main></body></html>"));
}

void sendBackHome() {
  server.sendContent(F("<a class='back' href='/'>← Back to home</a>"));
}

void sendField(const char* label, const char* name, const char* value, const char* placeholder) {
  server.sendContent(F("<label>"));
  sendRaw(label);
  server.sendContent(F("</label><input name='"));
  sendRaw(name);
  server.sendContent(F("' maxlength='23' placeholder=\""));
  sendEscaped(placeholder ? placeholder : "");
  server.sendContent(F("\" value=\""));
  sendEscaped(value);
  server.sendContent(F("\">"));
}

void sendSavedPage(const char* extra) {
  sendPageStart("Saved");
  server.sendContent(F("<div class='ok'><h1>Saved!</h1>"
    "<p>Your box will use this the next time the screen updates.</p>"));
  if (extra && extra[0]) {
    server.sendContent(F("<p>"));
    sendRaw(extra);
    server.sendContent(F("</p>"));
  }
  server.sendContent(F("</div><a class='btn' href='/'>Back to home</a>"));
  sendPageEnd();
}

void handleRoot() {
  sendPageStart("Canon Box");
  server.sendContent(F(
    "<h1>Canon Box settings</h1>"
    "<p class='hint'>Change what shows on the little screen. The music stays the same.</p>"
    "<a class='card' href='/theme'><strong>Change the story on screen</strong>"
    "<span>Birthday, Future Wife, or Wedding</span></a>"
    "<a class='card' href='/names'><strong>Names (who it’s for)</strong>"
    "<span>Nickname and your sign-off</span></a>"
    "<a class='card' href='/birthday'><strong>Birthday messages</strong>"
    "<span>Balloons, memories, quiz, finale letter</span></a>"
    "<a class='card' href='/future'><strong>Future wife messages</strong>"
    "<span>Promises, reasons, seal lines</span></a>"
    "<a class='card' href='/wedding'><strong>Wedding messages</strong>"
    "<span>Titles and captions</span></a>"
    "<a class='card' href='/wifi'><strong>Wi‑Fi name &amp; password</strong>"
    "<span>What your phone connects to</span></a>"
    "<form method='POST' action='/reset' onsubmit=\"return confirm('Restore all original messages? Wi‑Fi settings stay the same.');\">"
    "<button class='btn-danger' type='submit'>Restore original messages</button></form>"
  ));
  sendPageEnd();
}

void handleTheme() {
  sendPageStart("Story");
  sendBackHome();
  server.sendContent(F(
    "<h1>Which story?</h1>"
    "<p class='hint'>Pick what plays on the screen with the music.</p>"
    "<form method='POST' action='/save/theme'>"
    "<label>Story</label><select name='theme'>"
  ));
  const char* labels[3] = { "Birthday", "Future Wife", "Wedding" };
  for (int t = 0; t < THEME_COUNT; t++) {
    char opt[80];
    snprintf(opt, sizeof(opt), "<option value='%d'%s>%s</option>",
             t, currentTheme == t ? " selected" : "", labels[t]);
    server.sendContent(opt);
  }
  server.sendContent(F(
    "</select>"
    "<label>After each song</label><select name='locked'>"
  ));
  if (themeLocked) {
    server.sendContent(F(
      "<option value='0'>Switch to the next story</option>"
      "<option value='1' selected>Keep this story only</option>"
    ));
  } else {
    server.sendContent(F(
      "<option value='0' selected>Switch to the next story</option>"
      "<option value='1'>Keep this story only</option>"
    ));
  }
  server.sendContent(F("</select><button type='submit'>Save story</button></form>"));
  sendPageEnd();
}

void handleNames() {
  sendPageStart("Names");
  sendBackHome();
  server.sendContent(F(
    "<h1>Names</h1>"
    "<p class='hint'>Leave a box blank to keep the original wording.</p>"
    "<form method='POST' action='/save/names'>"
  ));
  sendField("Nickname (on screen)", "nickname", cfg.nickname, "Baby");
  sendField("Your sign-off name", "signer", cfg.signer, "Lolo");
  server.sendContent(F("<button type='submit'>Save names</button></form>"));
  sendPageEnd();
}

void handleBirthday() {
  sendPageStart("Birthday");
  sendBackHome();
  server.sendContent(F(
    "<h1>Birthday messages</h1>"
    "<p class='hint'>Leave blank to keep the original. Short lines work best on the tiny screen.</p>"
    "<form method='POST' action='/save/birthday'>"
    "<h2>Balloon pops</h2>"
  ));
  for (int i = 0; i < BALLOON_COUNT; i++) {
    char n1[12], n2[12], lab1[28], lab2[28];
    char ph1[TXT_LEN], ph2[TXT_LEN];
    snprintf(n1, sizeof(n1), "b1_%d", i);
    snprintf(n2, sizeof(n2), "b2_%d", i);
    snprintf(lab1, sizeof(lab1), "Balloon %d — line 1", i + 1);
    snprintf(lab2, sizeof(lab2), "Balloon %d — line 2", i + 1);
    loadFlashTable(BALLOON_L1, i); strncpy(ph1, flashBuf, TXT_LEN); ph1[TXT_LEN - 1] = 0;
    loadFlashTable(BALLOON_L2, i); strncpy(ph2, flashBuf, TXT_LEN); ph2[TXT_LEN - 1] = 0;
    sendField(lab1, n1, cfg.balloonL1[i], ph1);
    sendField(lab2, n2, cfg.balloonL2[i], ph2);
    yield();
  }

  server.sendContent(F("<h2>Memories</h2>"));
  for (int i = 0; i < GALLERY_COUNT; i++) {
    char n1[12], n2[12], lab1[28], lab2[28];
    char ph1[TXT_LEN], ph2[TXT_LEN];
    snprintf(n1, sizeof(n1), "g1_%d", i);
    snprintf(n2, sizeof(n2), "g2_%d", i);
    snprintf(lab1, sizeof(lab1), "Memory %d — line 1", i + 1);
    snprintf(lab2, sizeof(lab2), "Memory %d — line 2", i + 1);
    loadFlashTable(GALLERY_L1, i); strncpy(ph1, flashBuf, TXT_LEN); ph1[TXT_LEN - 1] = 0;
    loadFlashTable(GALLERY_L2, i); strncpy(ph2, flashBuf, TXT_LEN); ph2[TXT_LEN - 1] = 0;
    sendField(lab1, n1, cfg.galleryL1[i], ph1);
    sendField(lab2, n2, cfg.galleryL2[i], ph2);
  }

  server.sendContent(F("<h2>Quiz</h2>"));
  for (int q = 0; q < QUIZ_COUNT; q++) {
    char qn[12], lab[24], ph[TXT_LEN];
    snprintf(qn, sizeof(qn), "qq_%d", q);
    snprintf(lab, sizeof(lab), "Question %d", q + 1);
    loadFlashTable(QUIZ_Q, q); strncpy(ph, flashBuf, TXT_LEN); ph[TXT_LEN - 1] = 0;
    sendField(lab, qn, cfg.quizQ[q], ph);
    for (int o = 0; o < 4; o++) {
      char on[16], ol[28], po[TXT_LEN];
      snprintf(on, sizeof(on), "qo_%d_%d", q, o);
      snprintf(ol, sizeof(ol), "Answer %c", 'A' + o);
      loadFlashTable(QUIZ_OPT, q * 4 + o); strncpy(po, flashBuf, TXT_LEN); po[TXT_LEN - 1] = 0;
      sendField(ol, on, cfg.quizOpt[q * 4 + o], po);
    }
    uint8_t defC = pgm_read_byte(&QUIZ_CORRECT[q]);
    int cur = (cfg.quizCorrect[q] >= 0) ? cfg.quizCorrect[q] : (int)defC;
    char sel[220];
    snprintf(sel, sizeof(sel),
      "<label>Correct answer</label><select name='qc_%d'>"
      "<option value='0'%s>A</option><option value='1'%s>B</option>"
      "<option value='2'%s>C</option><option value='3'%s>D</option></select>",
      q,
      cur == 0 ? " selected" : "",
      cur == 1 ? " selected" : "",
      cur == 2 ? " selected" : "",
      cur == 3 ? " selected" : "");
    server.sendContent(sel);
    yield();
  }

  server.sendContent(F("<h2>Finale letter pages</h2>"));
  for (int p = 0; p < FINALE_PAGES; p++) {
    char n0[12], n1[12], n2[12], l0[28], l1[28], l2[28];
    char p0[TXT_LEN], p1[TXT_LEN], p2[TXT_LEN];
    snprintf(n0, sizeof(n0), "f0_%d", p);
    snprintf(n1, sizeof(n1), "f1_%d", p);
    snprintf(n2, sizeof(n2), "f2_%d", p);
    snprintf(l0, sizeof(l0), "Page %d — line 1", p + 1);
    snprintf(l1, sizeof(l1), "Page %d — line 2", p + 1);
    snprintf(l2, sizeof(l2), "Page %d — line 3", p + 1);
    loadFlashTable(FINALE_L0, p); strncpy(p0, flashBuf, TXT_LEN); p0[TXT_LEN - 1] = 0;
    loadFlashTable(FINALE_L1, p); strncpy(p1, flashBuf, TXT_LEN); p1[TXT_LEN - 1] = 0;
    loadFlashTable(FINALE_L2, p); strncpy(p2, flashBuf, TXT_LEN); p2[TXT_LEN - 1] = 0;
    sendField(l0, n0, cfg.finaleL0[p], p0);
    sendField(l1, n1, cfg.finaleL1[p], p1);
    sendField(l2, n2, cfg.finaleL2[p], p2[0] ? p2 : "(optional)");
    yield();
  }

  server.sendContent(F("<button type='submit'>Save birthday messages</button></form>"));
  sendPageEnd();
}

void handleFuture() {
  sendPageStart("Future Wife");
  sendBackHome();
  server.sendContent(F(
    "<h1>Future wife messages</h1>"
    "<p class='hint'>Leave blank to keep the original.</p>"
    "<form method='POST' action='/save/future'><h2>Promises</h2>"
  ));
  for (int i = 0; i < PROMISE_COUNT; i++) {
    char n[12], lab[24], ph[TXT_LEN];
    snprintf(n, sizeof(n), "p_%d", i);
    snprintf(lab, sizeof(lab), "Promise %d", i + 1);
    loadFlashTable(PROMISES, i); strncpy(ph, flashBuf, TXT_LEN); ph[TXT_LEN - 1] = 0;
    sendField(lab, n, cfg.promises[i], ph);
  }
  {
    char ph[TXT_LEN];
    loadFlash(PROMISE_SUB_2); strncpy(ph, flashBuf, TXT_LEN); ph[TXT_LEN - 1] = 0;
    sendField("Extra line under promise 3", "psub", cfg.promiseSub2, ph);
  }
  server.sendContent(F("<h2>Why you</h2>"));
  for (int i = 0; i < REASON_COUNT; i++) {
    char n[12], lab[24], ph[TXT_LEN];
    snprintf(n, sizeof(n), "r_%d", i);
    snprintf(lab, sizeof(lab), "Reason %d", i + 1);
    loadFlashTable(REASONS, i); strncpy(ph, flashBuf, TXT_LEN); ph[TXT_LEN - 1] = 0;
    sendField(lab, n, cfg.reasons[i], ph);
  }
  server.sendContent(F("<h2>Closing lines</h2>"));
  sendField("First line", "fw_title", cfg.fwSealTitle, "My future wife");
  sendField("Middle line", "fw_choose", cfg.fwSealChoose, "I choose you.");
  sendField("Last line", "fw_sign", cfg.fwSealSign, "- forever yours");
  server.sendContent(F("<button type='submit'>Save messages</button></form>"));
  sendPageEnd();
}

void handleWedding() {
  sendPageStart("Wedding");
  sendBackHome();
  server.sendContent(F(
    "<h1>Wedding messages</h1>"
    "<p class='hint'>Leave blank to keep the original.</p>"
    "<form method='POST' action='/save/wedding'>"
  ));
  sendField("Main title", "wed_title", cfg.wedTitle, "OUR WEDDING");
  sendField("Subtitle", "wed_sub", cfg.wedSubtitle, "A LOVE STORY");
  sendField("Rings caption", "wed_rings", cfg.wedRings, "FOREVER TOGETHER");
  sendField("Hearts line 1", "wed_h1", cfg.wedHearts1, "TWO HEARTS");
  sendField("Hearts line 2", "wed_h2", cfg.wedHearts2, "ONE LOVE");
  sendField("Message line 1", "wed_m1", cfg.wedMsg1, "YOU ARE MY");
  sendField("Message line 2", "wed_m2", cfg.wedMsg2, "FOREVER");
  sendField("Message line 3", "wed_m3", cfg.wedMsg3, "& ALWAYS");
  sendField("Final line 1", "wed_f1", cfg.wedFinal1, "FOREVER & ALWAYS");
  sendField("Final line 2", "wed_f2", cfg.wedFinal2, "LOVE");
  server.sendContent(F("<button type='submit'>Save wedding messages</button></form>"));
  sendPageEnd();
}

void handleWifiPage() {
  sendPageStart("Wi-Fi");
  sendBackHome();
  server.sendContent(F(
    "<h1>Wi‑Fi name &amp; password</h1>"
    "<p class='hint'>This is the network your phone joins to open these settings.</p>"
    "<div class='warn'>After you save, reconnect your phone to the new Wi‑Fi name. The settings page should open again by itself.</div>"
    "<form method='POST' action='/save/wifi'>"
    "<label>Wi‑Fi name</label><input name='ap_ssid' maxlength='32' required value=\""
  ));
  sendEscaped(activeApSsid());
  server.sendContent(F("\"><label>Password</label><input id='ap_pass' name='ap_pass' maxlength='63' value=\""));
  if (!cfg.apOpen) sendEscaped(activeApPass());
  server.sendContent(F("\">"
    "<label style='font-weight:500'><input type='checkbox' id='showpass' checked "
    "onchange=\"document.getElementById('ap_pass').type=this.checked?'text':'password'\"> Show password</label>"
    "<label style='font-weight:500'><input type='checkbox' name='ap_open' value='1'"));
  if (cfg.apOpen) server.sendContent(F(" checked"));
  server.sendContent(F(
    "> No password (open network)</label>"
    "<button type='submit'>Save Wi‑Fi</button></form>"
    "<form method='POST' action='/wifi/reset' style='margin-top:16px' "
    "onsubmit=\"return confirm('Restore default Wi‑Fi name and password?');\">"
    "<button class='btn-secondary' type='submit'>Restore default Wi‑Fi</button></form>"
  ));
  char cur[120];
  snprintf(cur, sizeof(cur), "<p class='hint'>Current name: <strong>%s</strong></p>", activeApSsid());
  server.sendContent(cur);
  sendPageEnd();
}

void handleSaveTheme() {
  if (server.hasArg("theme")) {
    int t = server.arg("theme").toInt();
    if (t < 0) t = 0;
    if (t >= THEME_COUNT) t = THEME_COUNT - 1;
    currentTheme = t;
  }
  if (server.hasArg("locked")) {
    themeLocked = server.arg("locked").toInt() != 0;
  }
  saveConfig();
  sendSavedPage("");
}

void handleSaveNames() {
  if (server.hasArg("nickname")) copyArgToField(cfg.nickname, server.arg("nickname"));
  if (server.hasArg("signer")) copyArgToField(cfg.signer, server.arg("signer"));
  saveConfig();
  sendSavedPage("");
}

void handleSaveBirthday() {
  for (int i = 0; i < BALLOON_COUNT; i++) {
    char n1[12], n2[12];
    snprintf(n1, sizeof(n1), "b1_%d", i);
    snprintf(n2, sizeof(n2), "b2_%d", i);
    if (server.hasArg(n1)) copyArgToField(cfg.balloonL1[i], server.arg(n1));
    if (server.hasArg(n2)) copyArgToField(cfg.balloonL2[i], server.arg(n2));
  }
  for (int i = 0; i < GALLERY_COUNT; i++) {
    char n1[12], n2[12];
    snprintf(n1, sizeof(n1), "g1_%d", i);
    snprintf(n2, sizeof(n2), "g2_%d", i);
    if (server.hasArg(n1)) copyArgToField(cfg.galleryL1[i], server.arg(n1));
    if (server.hasArg(n2)) copyArgToField(cfg.galleryL2[i], server.arg(n2));
  }
  for (int q = 0; q < QUIZ_COUNT; q++) {
    char qn[12], qcn[12];
    snprintf(qn, sizeof(qn), "qq_%d", q);
    snprintf(qcn, sizeof(qcn), "qc_%d", q);
    if (server.hasArg(qn)) copyArgToField(cfg.quizQ[q], server.arg(qn));
    for (int o = 0; o < 4; o++) {
      char on[16];
      snprintf(on, sizeof(on), "qo_%d_%d", q, o);
      if (server.hasArg(on)) copyArgToField(cfg.quizOpt[q * 4 + o], server.arg(on));
    }
    if (server.hasArg(qcn)) {
      int c = server.arg(qcn).toInt();
      if (c < 0) c = 0;
      if (c > 3) c = 3;
      cfg.quizCorrect[q] = (int8_t)c;
    }
  }
  for (int p = 0; p < FINALE_PAGES; p++) {
    char n0[12], n1[12], n2[12];
    snprintf(n0, sizeof(n0), "f0_%d", p);
    snprintf(n1, sizeof(n1), "f1_%d", p);
    snprintf(n2, sizeof(n2), "f2_%d", p);
    if (server.hasArg(n0)) copyArgToField(cfg.finaleL0[p], server.arg(n0));
    if (server.hasArg(n1)) copyArgToField(cfg.finaleL1[p], server.arg(n1));
    if (server.hasArg(n2)) copyArgToField(cfg.finaleL2[p], server.arg(n2));
  }
  saveConfig();
  sendSavedPage("");
}

void handleSaveFuture() {
  for (int i = 0; i < PROMISE_COUNT; i++) {
    char n[12];
    snprintf(n, sizeof(n), "p_%d", i);
    if (server.hasArg(n)) copyArgToField(cfg.promises[i], server.arg(n));
  }
  if (server.hasArg("psub")) copyArgToField(cfg.promiseSub2, server.arg("psub"));
  for (int i = 0; i < REASON_COUNT; i++) {
    char n[12];
    snprintf(n, sizeof(n), "r_%d", i);
    if (server.hasArg(n)) copyArgToField(cfg.reasons[i], server.arg(n));
  }
  if (server.hasArg("fw_title")) copyArgToField(cfg.fwSealTitle, server.arg("fw_title"));
  if (server.hasArg("fw_choose")) copyArgToField(cfg.fwSealChoose, server.arg("fw_choose"));
  if (server.hasArg("fw_sign")) copyArgToField(cfg.fwSealSign, server.arg("fw_sign"));
  saveConfig();
  sendSavedPage("");
}

void handleSaveWedding() {
  if (server.hasArg("wed_title")) copyArgToField(cfg.wedTitle, server.arg("wed_title"));
  if (server.hasArg("wed_sub")) copyArgToField(cfg.wedSubtitle, server.arg("wed_sub"));
  if (server.hasArg("wed_rings")) copyArgToField(cfg.wedRings, server.arg("wed_rings"));
  if (server.hasArg("wed_h1")) copyArgToField(cfg.wedHearts1, server.arg("wed_h1"));
  if (server.hasArg("wed_h2")) copyArgToField(cfg.wedHearts2, server.arg("wed_h2"));
  if (server.hasArg("wed_m1")) copyArgToField(cfg.wedMsg1, server.arg("wed_m1"));
  if (server.hasArg("wed_m2")) copyArgToField(cfg.wedMsg2, server.arg("wed_m2"));
  if (server.hasArg("wed_m3")) copyArgToField(cfg.wedMsg3, server.arg("wed_m3"));
  if (server.hasArg("wed_f1")) copyArgToField(cfg.wedFinal1, server.arg("wed_f1"));
  if (server.hasArg("wed_f2")) copyArgToField(cfg.wedFinal2, server.arg("wed_f2"));
  saveConfig();
  sendSavedPage("");
}

void handleSaveWifi() {
  if (server.hasArg("ap_ssid")) {
    copyArgToFieldN(cfg.apSsid, server.arg("ap_ssid"), AP_SSID_LEN);
  }
  if (cfg.apSsid[0] == '\0') {
    strncpy(cfg.apSsid, AP_SSID_DEFAULT, AP_SSID_LEN - 1);
    cfg.apSsid[AP_SSID_LEN - 1] = '\0';
  }

  cfg.apOpen = server.hasArg("ap_open") ? 1 : 0;
  if (cfg.apOpen) {
    cfg.apPass[0] = '\0';
  } else if (server.hasArg("ap_pass")) {
    copyArgToFieldN(cfg.apPass, server.arg("ap_pass"), AP_PASS_LEN);
    // WPA2 needs 8+ chars; fall back to default if too short
    if (strlen(cfg.apPass) > 0 && strlen(cfg.apPass) < 8) {
      strncpy(cfg.apPass, AP_PASS_DEFAULT, AP_PASS_LEN - 1);
      cfg.apPass[AP_PASS_LEN - 1] = '\0';
    }
    if (cfg.apPass[0] == '\0') {
      strncpy(cfg.apPass, AP_PASS_DEFAULT, AP_PASS_LEN - 1);
      cfg.apPass[AP_PASS_LEN - 1] = '\0';
    }
  }

  saveConfig();
  applySoftAP();

  sendPageStart("Reconnect");
  server.sendContent(F(
    "<div class='ok'><h1>Wi‑Fi updated</h1>"
    "<p>Please reconnect your phone to this network:</p><p><strong>"
  ));
  sendEscaped(activeApSsid());
  server.sendContent(F("</strong></p>"));
  if (cfg.apOpen) {
    server.sendContent(F("<p>No password (open network).</p>"));
  } else {
    server.sendContent(F("<p>Password: <strong>"));
    sendEscaped(activeApPass());
    server.sendContent(F("</strong></p>"));
  }
  server.sendContent(F(
    "<p>The settings page should open automatically after you join.</p></div>"
    "<a class='btn' href='/'>Open settings</a>"
  ));
  sendPageEnd();
}

void handleWifiReset() {
  cfg.apSsid[0] = '\0';
  cfg.apPass[0] = '\0';
  cfg.apOpen = 0;
  saveConfig();
  applySoftAP();
  sendPageStart("Wi-Fi restored");
  server.sendContent(F(
    "<div class='ok'><h1>Default Wi‑Fi restored</h1>"
    "<p>Reconnect to <strong>CanonBox</strong> / <strong>canon1234</strong>.</p></div>"
    "<a class='btn' href='/'>Open settings</a>"
  ));
  sendPageEnd();
}

void handleReset() {
  char keepSsid[AP_SSID_LEN];
  char keepPass[AP_PASS_LEN];
  uint8_t keepOpen = cfg.apOpen;
  strncpy(keepSsid, cfg.apSsid, AP_SSID_LEN);
  strncpy(keepPass, cfg.apPass, AP_PASS_LEN);
  keepSsid[AP_SSID_LEN - 1] = '\0';
  keepPass[AP_PASS_LEN - 1] = '\0';

  clearConfig();
  strncpy(cfg.apSsid, keepSsid, AP_SSID_LEN);
  strncpy(cfg.apPass, keepPass, AP_PASS_LEN);
  cfg.apSsid[AP_SSID_LEN - 1] = '\0';
  cfg.apPass[AP_PASS_LEN - 1] = '\0';
  cfg.apOpen = keepOpen;
  applyThemeFromConfig();
  saveConfig();
  sendPageStart("Restored");
  server.sendContent(F(
    "<div class='ok'><h1>Original messages restored</h1>"
    "<p>Wi‑Fi settings were left as they are.</p></div>"
    "<a class='btn' href='/'>Back to home</a>"
  ));
  sendPageEnd();
}

void handleCaptiveProbe() {
  // Many phones show a portal window for redirects from connectivity checks
  server.sendHeader("Location", String("http://") + apIP.toString() + "/", true);
  server.send(302, "text/plain", "");
}

void startWifiConfig() {
  WiFi.mode(WIFI_AP);
  applySoftAP();

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/theme", HTTP_GET, handleTheme);
  server.on("/names", HTTP_GET, handleNames);
  server.on("/birthday", HTTP_GET, handleBirthday);
  server.on("/future", HTTP_GET, handleFuture);
  server.on("/wedding", HTTP_GET, handleWedding);
  server.on("/wifi", HTTP_GET, handleWifiPage);

  server.on("/save/theme", HTTP_POST, handleSaveTheme);
  server.on("/save/names", HTTP_POST, handleSaveNames);
  server.on("/save/birthday", HTTP_POST, handleSaveBirthday);
  server.on("/save/future", HTTP_POST, handleSaveFuture);
  server.on("/save/wedding", HTTP_POST, handleSaveWedding);
  server.on("/save/wifi", HTTP_POST, handleSaveWifi);
  server.on("/wifi/reset", HTTP_POST, handleWifiReset);
  server.on("/reset", HTTP_POST, handleReset);

  server.on("/generate_204", HTTP_GET, handleCaptiveProbe);
  server.on("/gen_204", HTTP_GET, handleCaptiveProbe);
  server.on("/hotspot-detect.html", HTTP_GET, handleCaptiveProbe);
  server.on("/library/test/success.html", HTTP_GET, handleCaptiveProbe);
  server.on("/connecttest.txt", HTTP_GET, handleCaptiveProbe);
  server.on("/ncsi.txt", HTTP_GET, handleCaptiveProbe);
  server.on("/fwlink", HTTP_GET, handleCaptiveProbe);
  server.on("/canonical.html", HTTP_GET, handleCaptiveProbe);
  server.on("/success.txt", HTTP_GET, handleCaptiveProbe);

  server.onNotFound([]() {
    if (server.method() == HTTP_GET) {
      handleCaptiveProbe();
    } else {
      server.send(404, "text/plain", "Not found");
    }
  });
  server.begin();

  Serial.println();
  Serial.println("WiFi SoftAP + captive portal ready");
  Serial.print("SSID: ");
  Serial.println(activeApSsid());
  if (cfg.apOpen) {
    Serial.println("PASS: (open network)");
  } else {
    Serial.print("PASS: ");
    Serial.println(activeApPass());
  }
  Serial.print("Portal: http://");
  Serial.println(apIP);
}

// =====================================
// SETUP
// =====================================

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(SENSOR_PIN, INPUT_PULLUP);

  noTone(BUZZER_PIN);

  Serial.begin(115200);
  delay(50);

  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed; formatting...");
    LittleFS.format();
    LittleFS.begin();
  }
  loadConfig();

  Wire.begin(D2, D1);

  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    oledReady = true;
    displayOn = true;

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    centerText("Canon WiFi", 4, 1);
    centerText(activeApSsid(), 18, 1);
    if (cfg.apOpen) {
      centerText("(open WiFi)", 30, 1);
    } else {
      centerText(activeApPass(), 30, 1);
    }
    centerText("auto portal", 44, 1);

    display.display();
    delay(2000);

    // Start with screen off until the door opens
    setDisplayPower(false);

    animationScene = 0;
    animationFrame = 0;
    sceneStartTime = millis();
    lastAnimation = 0;
  } else {
    Serial.println("OLED initialization failed!");
  }

  startWifiConfig();

  Serial.println();
  Serial.println("================================");
  Serial.println("   CANON IN D - WIFI DISPLAY");
  Serial.println("   Birthday + FutureWife + Wedding");
  Serial.println("   SoftAP + captive portal");
  Serial.println("================================");
  Serial.print("Starting theme: ");
  Serial.println(themeName(currentTheme));
  Serial.print("Theme locked: ");
  Serial.println(themeLocked ? "yes" : "no");
  Serial.print("WiFi SSID: ");
  Serial.println(activeApSsid());
  Serial.println("Join WiFi — phone settings page should open by itself.");
  Serial.println("Blank web fields keep original message defaults.");
  Serial.println();
}

// =====================================
// MAIN LOOP
// =====================================

void loop() {
  waitForDoorOpen();

  if (!musicEnabled()) {
    return;
  }

  beginMusicClock();

  Serial.println();
  Serial.print("========== ");
  Serial.print(themeName(currentTheme));
  Serial.println(" / BASS ==========");

  for (int i = 0; i < bassLength; i++) {
    if (!playNote(bass[i].frequency, bass[i].beats)) {
      return;
    }
  }

  Serial.println("Bass finished.");

  if (!playNote(REST, BEATS_REST_GAP)) {
    return;
  }

  Serial.println();
  Serial.print("========== ");
  Serial.print(themeName(currentTheme));
  Serial.println(" / MELODY ==========");

  for (int i = 0; i < totalNotes; i++) {
    logMeasureIfNeeded(i);

    if (!playNote(melody[i].frequency, melody[i].beats)) {
      return;
    }
  }

  noTone(BUZZER_PIN);
  endMusicClock();

  Serial.println();
  Serial.println("================================");
  Serial.println("Canon in D sequence completed!");
  Serial.println("Waiting before repeat...");
  Serial.println("================================");

  advanceTheme();

  if (!controlledDelay(REPEAT_GAP)) {
    return;
  }
}
