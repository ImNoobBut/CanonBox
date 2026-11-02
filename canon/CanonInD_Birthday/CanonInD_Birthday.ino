/*
  CANON IN D - BIRTHDAY SURPRISE
  ESP8266 D1 MINI

  BUZZER          D6 (GPIO12)
  MAGNETIC SENSOR D7 (GPIO13)
  OLED SDA        D2 (GPIO4)
  OLED SCL        D1 (GPIO5)

  MUSIC ↔ ANIMATION (sixteenth units @ 66 BPM):
  Bass [0, 24)        → Scene 0  Boot / splash
  Bass [24, 68)       → Scene 1  PIN unlock (auto)
  Melody m13–14       → Scene 2  Intro
  Melody m15–16       → Scene 3  Balloons
  Melody early 17–18  → Scene 4  Gallery
  Melody late 17–18   → Scene 5  Quiz
  Melody early 19–21  → Scene 6  Cake
  Melody late cadence → Scene 7  Finale letter

  Pulses, pops, typewriter, and page flips
  are driven by the live music beat clock.

  DOOR OPEN   = MUSIC ON
  DOOR CLOSED = MUSIC OFF

  SERIAL = 115200
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
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

// Absolute beat where each scene ends (music-playing mode)
const unsigned int SCENE_BEAT_END[] = {
  24,                              // 0 boot
  BEATS_MELODY_START,              // 1 pin     — rest of bass + gap
  BEATS_MELODY_START + 32,         // 2 intro   — m13–14
  BEATS_MELODY_START + 64,         // 3 balloons— m15–16
  BEATS_MELODY_START + 80,         // 4 gallery — early m17–18
  BEATS_MELODY_START + 96,         // 5 quiz    — late m17–18
  BEATS_MELODY_START + 112,        // 6 cake    — early m19–21
  0xFFFF                           // 7 finale  — cadence
};

const int TOTAL_SCENES = 8;

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

void loadFlash(const char* ptr) {
  strncpy_P(flashBuf, ptr, sizeof(flashBuf) - 1);
  flashBuf[sizeof(flashBuf) - 1] = '\0';
}

void loadFlashTable(const char* const* table, int index) {
  const char* ptr = (const char*)pgm_read_ptr(&table[index]);
  loadFlash(ptr);
}

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
int animationScene = 0;
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
  for (int i = 0; i < TOTAL_SCENES - 1; i++) {
    if (beat < (float)SCENE_BEAT_END[i]) {
      return i;
    }
  }
  return TOTAL_SCENES - 1;
}

float sceneLocalBeat(float beat, int scene) {
  float start = 0.0f;
  if (scene > 0) {
    start = (float)SCENE_BEAT_END[scene - 1];
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
    centerText("Baby", 34, 2);
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

    const char* name = "Baby";
    int chars = (int)(lb * 2.0f) + 1;
    if (chars > 4) chars = 4;
    if (!musicPlaying) chars = 4;

    char buf[5];
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
    centerText("- Lolo", 16, 1);
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

  loadFlashTable(BALLOON_L1, idx);
  centerText(flashBuf, 40, 1);
  loadFlashTable(BALLOON_L2, idx);
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

  loadFlashTable(GALLERY_L1, idx);
  centerTextAt(flashBuf, 22, 1, slide);
  loadFlashTable(GALLERY_L2, idx);
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

  loadFlashTable(QUIZ_Q, qi);
  centerText(flashBuf, 10, 1);

  uint8_t correct = pgm_read_byte(&QUIZ_CORRECT[qi]);

  // Single column, tight rows — stay above progress bar (y<=58)
  for (int o = 0; o < 4; o++) {
    loadFlashTable(QUIZ_OPT, qi * 4 + o);
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

  loadFlashTable(FINALE_L0, page);
  if (flashBuf[0] != '\0') {
    centerText(flashBuf, big ? 6 : 10, 1);
  }

  loadFlashTable(FINALE_L1, page);
  if (flashBuf[0] != '\0') {
    centerText(flashBuf, big ? 22 : 26, big ? 2 : 1);
  }

  loadFlashTable(FINALE_L2, page);
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

// =====================================
// MAIN ANIMATION
// =====================================

void drawBirthdayAnimation() {
  if (!oledReady) return;

  float beat = musicPlaying ? musicBeat() : (float)animationFrame;
  int scene = animationScene;
  float local = 0.0f;

  if (musicPlaying) {
    int synced = sceneFromMusicBeat(beat);
    if (synced != animationScene) {
      animationScene = synced;
      sceneStartTime = millis();
      animationFrame = 0;
      Serial.print("Scene -> ");
      Serial.println(animationScene);
    }
    scene = animationScene;
    local = sceneLocalBeat(beat, scene);
  } else {
    local = (float)animationFrame;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  switch (scene) {
    case 0: drawBootScene(local); break;
    case 1: drawPinScene(local); break;
    case 2: drawIntroScene(local); break;
    case 3: drawBalloonsScene(local); break;
    case 4: drawGalleryScene(local); break;
    case 5: drawQuizScene(local); break;
    case 6: drawCakeScene(local); break;
    case 7: drawFinaleScene(local); break;
  }

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
      if (animationScene >= TOTAL_SCENES) {
        animationScene = 0;
      }
      animationFrame = 0;
    }
  }

  if (now - lastAnimation >= FRAME_INTERVAL) {
    lastAnimation = now;
    drawBirthdayAnimation();
  }
}

// =====================================
// SENSOR CONTROL
// =====================================

bool musicEnabled() {
  return digitalRead(SENSOR_PIN) == HIGH;
}

// =====================================
// WAIT FOR DOOR OPEN
// =====================================

void waitForDoorOpen() {
  noTone(BUZZER_PIN);
  endMusicClock();

  Serial.println("Waiting for door to open...");

  while (!musicEnabled()) {
    updateAnimation();
    delay(10);
  }

  Serial.println("DOOR OPEN - MUSIC STARTING");

  unsigned long startTime = millis();
  while (millis() - startTime < 500) {
    if (!musicEnabled()) {
      return;
    }
    updateAnimation();
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
      Serial.println("DOOR CLOSED - MUSIC STOPPED");
      return false;
    }

    updateAnimation();
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
// SETUP
// =====================================

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(SENSOR_PIN, INPUT_PULLUP);

  noTone(BUZZER_PIN);

  Serial.begin(115200);

  Wire.begin(D2, D1);

  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    oledReady = true;

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    centerText("FOR YOU", 8, 1);
    centerText("Baby", 24, 2);
    centerText("Happy Birthday", 48, 1);

    display.display();
    delay(1500);

    animationScene = 0;
    animationFrame = 0;
    sceneStartTime = millis();
    lastAnimation = 0;
  } else {
    Serial.println("OLED initialization failed!");
  }

  Serial.println();
  Serial.println("================================");
  Serial.println("   CANON IN D - BIRTHDAY");
  Serial.println("   Beat-synced OLED scenes");
  Serial.println("================================");
  Serial.println("Bass early   -> Boot");
  Serial.println("Bass late    -> PIN unlock");
  Serial.println("Melody 13-14 -> Intro");
  Serial.println("Melody 15-16 -> Balloons");
  Serial.println("Melody 17-18 -> Gallery/Quiz");
  Serial.println("Melody 19-21 -> Cake/Finale");
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
  Serial.println("========== BASS ==========");

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
  Serial.println("========== MELODY ==========");

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

  if (!controlledDelay(REPEAT_GAP)) {
    return;
  }
}
