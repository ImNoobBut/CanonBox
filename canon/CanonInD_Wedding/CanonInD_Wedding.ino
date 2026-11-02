/*
  CANON IN D - WEDDING PROCESSIONAL
  ESP8266 D1 MINI

  BUZZER          D6 (GPIO12)
  MAGNETIC SENSOR D7 (GPIO13)
  OLED SDA        D2 (GPIO4)
  OLED SCL        D1 (GPIO5)

  MUSIC ↔ ANIMATION (sixteenth units @ 66 BPM):
  Bass + gap      → Scene 0  Wedding Title
  Melody m13–14   → Scene 1  Interlocking Rings
  Melody m15–16   → Scene 2  Floating Hearts
  Melody m17–18   → Scene 3  Romantic Message
  Melody m19–21   → Scene 4  Forever & Always

  Pulses, ring motion, heart rise, and text reveals
  are driven by the live music beat clock.

  DOOR OPEN   = MUSIC ON
  DOOR CLOSED = MUSIC OFF

  SERIAL = 115200
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

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
  BEATS_MELODY_START,          // 0 Title  — bass + gap
  BEATS_MELODY_START + 32,     // 1 Rings  — m13–14
  BEATS_MELODY_START + 64,     // 2 Hearts — m15–16
  BEATS_MELODY_START + 96,     // 3 Message— m17–18
  0xFFFF                       // 4 Forever— m19–end
};

const int TOTAL_SCENES = 5;

const int MEASURE_NOTE_INDEX[] = {
  0, 13, 27, 41, 55, 68, 82, 96, 109
};
const int MEASURE_NUMBER[] = {
  13, 14, 15, 16, 17, 18, 19, 20, 21
};
const int MEASURE_COUNT = 9;

// =====================================
// MUSIC CLOCK  (drives animation sync)
// =====================================

bool musicPlaying = false;
unsigned int committedBeats = 0;
int currentNoteBeats = 0;
unsigned long noteStartMs = 0;
int currentNoteFreq = REST;

const unsigned long IDLE_SCENE_MS = 4500;
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

// Map pitch to a 0..1 “brightness” for sparkle density
float pitchGlow() {
  if (!musicPlaying || currentNoteFreq <= 0) {
    return 0.3f;
  }
  float t = (float)(currentNoteFreq - D4) / (float)(A5 - D4);
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  return t;
}

// Thin progress bar across the piece (music only)
void drawMusicProgress() {
  if (!musicPlaying) return;

  // Approx total: bass 64 + gap 4 + melody ~135 = 203
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
    centerText("OUR WEDDING", 16, 1);
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
    centerText("A LOVE STORY", 48, 1);
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

  centerText("FOREVER TOGETHER", 48, 1);
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
  centerText("TWO HEARTS", captionTop + 2, 1);
  centerText("ONE LOVE", captionTop + 11, 1);
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
    centerText("YOU ARE MY", 12, 1);
  }

  if (localBeat >= 8.0f || !musicPlaying) {
    centerText("FOREVER", 24, 2);
  }

  // Underline grows through the two measures (under FOREVER)
  float t = localBeat / 32.0f;
  if (t > 1.0f) t = 1.0f;
  if (t < 0.0f) t = 0.0f;
  int lineW = (int)(16 + t * 72);
  display.drawFastHLine(64 - lineW / 2, 42, lineW, SSD1306_WHITE);

  if (localBeat >= 20.0f || !musicPlaying) {
    centerText("& ALWAYS", 46, 1);
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

  centerText("FOREVER & ALWAYS", 44, 1);
  centerText("LOVE", 53, 1);
}

// =====================================
// MAIN ANIMATION
// =====================================

void drawWeddingAnimation() {
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

  switch (scene) {
    case 0: drawTitleScene(local); break;
    case 1: drawRingsScene(local); break;
    case 2: drawHeartsScene(local); break;
    case 3: drawMessageScene(local); break;
    case 4: drawFinalScene(local); break;
  }

  drawMusicProgress();
  display.display();
  animationFrame++;
}

void updateAnimation() {
  if (!oledReady) return;

  unsigned long now = millis();

  // Idle (door closed): gentle timed scene rotation
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
    drawWeddingAnimation();
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

    centerText("OUR WEDDING", 8, 1);
    centerText("CANON IN D", 24, 2);
    centerText("FOREVER & ALWAYS", 48, 1);

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
  Serial.println("   CANON IN D - WEDDING");
  Serial.println("   Beat-synced OLED scenes");
  Serial.println("================================");
  Serial.println("Bass         -> Title");
  Serial.println("Melody 13-14 -> Rings");
  Serial.println("Melody 15-16 -> Hearts");
  Serial.println("Melody 17-18 -> Message");
  Serial.println("Melody 19-21 -> Forever");
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

  // -------- BASS INTRODUCTION --------
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

  // -------- MELODY --------
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
