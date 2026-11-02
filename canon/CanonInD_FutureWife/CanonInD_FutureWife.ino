/*
  CANON IN D - FOR MY FUTURE WIFE
  ESP8266 D1 MINI

  BUZZER          D6 (GPIO12)
  MAGNETIC SENSOR D7 (GPIO13)
  OLED SDA        D2 (GPIO4)
  OLED SCL        D1 (GPIO5)

  MUSIC ↔ ANIMATION (sixteenth units @ 66 BPM):
  Bass [0, 32)        → Scene 0  Dedication
  Bass [32, 68)       → Scene 1  Constellation
  Melody m13–14       → Scene 2  Two paths
  Melody m15–16       → Scene 3  Promises
  Melody m17–18       → Scene 4  Why you
  Melody m19–cadence  → Scene 5  Seal

  Pulses, star links, path motion, and text reveals
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
  32,                              // 0 Dedication   — early bass
  BEATS_MELODY_START,              // 1 Constellation— late bass + gap
  BEATS_MELODY_START + 32,         // 2 Two paths    — m13–14
  BEATS_MELODY_START + 64,         // 3 Promises     — m15–16
  BEATS_MELODY_START + 96,         // 4 Why you      — m17–18
  0xFFFF                           // 5 Seal         — m19–cadence
};

const int TOTAL_SCENES = 6;

const int MEASURE_NOTE_INDEX[] = {
  0, 13, 27, 41, 55, 68, 82, 96, 109
};
const int MEASURE_NUMBER[] = {
  13, 14, 15, 16, 17, 18, 19, 20, 21
};
const int MEASURE_COUNT = 9;

// =====================================
// PROGMEM COPY
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

const unsigned long IDLE_SCENE_MS = 4000;
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

void centerText(const char* message, int y, int textSize) {
  display.setTextSize(textSize);

  int16_t x1, y1;
  uint16_t w, h;

  display.getTextBounds(message, 0, 0, &x1, &y1, &w, &h);

  int x = (SCREEN_WIDTH - (int)w) / 2;
  display.setCursor(x, y);
  display.println(message);
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

float idleLocal(float sceneLen) {
  float t = (float)(animationFrame % 48) / 48.0f;
  return t * sceneLen;
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

  loadFlashTable(PROMISES, idx);
  centerText(flashBuf, 22, 1);

  int underlineY = 34;
  if (idx == 2) {
    loadFlash(PROMISE_SUB_2);
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

  loadFlashTable(REASONS, idx);
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
    centerText("My future wife", 16, 1);
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

    centerText("I choose you.", 40, 1);

    if (lb >= 24.0f || !musicPlaying) {
      centerText("- forever yours", 52, 1);
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
// MAIN ANIMATION
// =====================================

void drawFutureWifeAnimation() {
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
    case 0: drawDedicationScene(local); break;
    case 1: drawConstellationScene(local); break;
    case 2: drawTwoPathsScene(local); break;
    case 3: drawPromisesScene(local); break;
    case 4: drawWhyYouScene(local); break;
    case 5: drawSealScene(local); break;
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
    drawFutureWifeAnimation();
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

    centerText("FOR MY", 6, 1);
    centerText("future", 18, 2);
    centerText("wife", 36, 2);
    centerText("Canon in D", 54, 1);

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
  Serial.println("   CANON IN D - FUTURE WIFE");
  Serial.println("   Beat-synced OLED scenes");
  Serial.println("================================");
  Serial.println("Bass early   -> Dedication");
  Serial.println("Bass late    -> Constellation");
  Serial.println("Melody 13-14 -> Two paths");
  Serial.println("Melody 15-16 -> Promises");
  Serial.println("Melody 17-18 -> Why you");
  Serial.println("Melody 19-21 -> Seal");
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
