#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

#define BUTTON_A_PIN 7   // Stopwatch button
#define BUTTON_B_PIN 6   // Countdown button
#define LED_PIN 8

// Timing config
const unsigned long DEBOUNCE_MS = 50;
const unsigned long LONGPRESS_MS = 800;
const unsigned long HOLD_REPEAT_MS = 350;
const unsigned long TICK_MS = 1000;
const unsigned long LED_BLINK_MS = 500;

// Countdown limits
const int COUNT_STEP = 10;
const int COUNT_MIN = 10;
const int COUNT_MAX = 300;

enum TimerMode { IDLE, STOPWATCH, COUNTDOWN };
TimerMode currentMode = IDLE;

// Stopwatch
int stopwatchSeconds = 0;
bool isStopwatchRunning = false;

// Countdown
int initialCountdownSetting = 10;
int countdownSeconds = 0;
bool isCountdownRunning = false;
bool countdownFinished = false;

// Button states
bool lastRawA = HIGH, lastRawB = HIGH;
unsigned long lastDebounceTimeA = 0, lastDebounceTimeB = 0;
bool stableA = HIGH, stableB = HIGH;

// Long presses
unsigned long pressStartA = 0, pressStartB = 0;
bool longAengaged = false, longBengaged = false;
unsigned long lastHoldRepeatB = 0;

// Timing
unsigned long lastTick = 0;
unsigned long lastLedToggle = 0;
bool ledState = LOW;

// LCD cache
TimerMode lastShownMode = IDLE;
char lastLine0[17] = "";
char lastLine1[17] = "";

// -------- Helpers ----------
void formatTime(int totalSeconds, char* buffer) {
  int m = totalSeconds / 60;
  int s = totalSeconds % 60;
  sprintf(buffer, "%02d:%02d", m, s);
}

void safePrintLine(int row, const char* txt) {
  char* cache = (row == 0 ? lastLine0 : lastLine1);
  if (strcmp(cache, txt) != 0) {
    lcd.setCursor(0, row);
    lcd.print("                ");
    lcd.setCursor(0, row);
    lcd.print(txt);
    strncpy(cache, txt, 16);
    cache[16] = '\0';
  }
}

void updateLCD() {
  char buf[17];

  if (currentMode != lastShownMode) {
    lastLine0[0] = 0;
    lastLine1[0] = 0;
    lastShownMode = currentMode;
  }

  if (currentMode == STOPWATCH) {
    safePrintLine(0, "   STOPWATCH");
    formatTime(stopwatchSeconds, buf);
    char line[17];
    snprintf(line, 17, "     %s", buf);
    safePrintLine(1, line);

  } else if (currentMode == COUNTDOWN) {
    safePrintLine(0, "   COUNTDOWN");
    if (countdownSeconds > 0) {
      formatTime(countdownSeconds, buf);
      char line[17];
      snprintf(line, 17, "     %s", buf);
      safePrintLine(1, line);
    } else {
      safePrintLine(1, "   TIME UP!     ");
      digitalWrite(LED_PIN, HIGH);
    }

  } else { 
    safePrintLine(0, "A : Stopwatch");
    char line[17];
    snprintf(line, 17, "B : CD For : %ds", initialCountdownSetting);
    safePrintLine(1, line);
  }
}

// ------ Setup -------
void setup() {
  lcd.init();
  lcd.backlight();

  pinMode(BUTTON_A_PIN, INPUT_PULLUP);
  pinMode(BUTTON_B_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  lastShownMode = (TimerMode)255;
  updateLCD();
}

// ------ Loop -------
void loop() {
  unsigned long now = millis();
  bool rawA = digitalRead(BUTTON_A_PIN);
  bool rawB = digitalRead(BUTTON_B_PIN);

  // Debounce A
  if (rawA != lastRawA) lastDebounceTimeA = now;
  if ((now - lastDebounceTimeA) > DEBOUNCE_MS && stableA != rawA) {
    stableA = rawA;
    if (stableA == LOW) {
      pressStartA = now;
      longAengaged = false;
    } else {
      unsigned long held = now - pressStartA;
      if (!longAengaged && held < LONGPRESS_MS) {
        // short click A
        if (currentMode != STOPWATCH) {
          currentMode = STOPWATCH;
          isStopwatchRunning = true;
          isCountdownRunning = false;
          countdownFinished = false;
        } else {
          isStopwatchRunning = !isStopwatchRunning;
        }
        updateLCD();
      }
    }
  }
  lastRawA = rawA;

  // Debounce B
  if (rawB != lastRawB) lastDebounceTimeB = now;
  if ((now - lastDebounceTimeB) > DEBOUNCE_MS && stableB != rawB) {
    stableB = rawB;
    if (stableB == LOW) {
      pressStartB = now;
      longBengaged = false;
      lastHoldRepeatB = now;
    } else {
      unsigned long held = now - pressStartB;
      if (!longBengaged && held < LONGPRESS_MS) {
        // short click B
        currentMode = COUNTDOWN;
        isStopwatchRunning = false;
        countdownFinished = false;

        if (isCountdownRunning) {
          isCountdownRunning = false;
        } else if (countdownSeconds <= 0) {
          countdownSeconds = initialCountdownSetting;
          isCountdownRunning = true;
        } else {
          isCountdownRunning = true;
        }
        updateLCD();
      }
    }
  }
  lastRawB = rawB;

  // Hold A → Reset everything
  if (stableA == LOW && now - pressStartA >= LONGPRESS_MS && !longAengaged) {
    longAengaged = true;
    stopwatchSeconds = 0;
    countdownSeconds = 0;
    isStopwatchRunning = false;
    isCountdownRunning = false;
    countdownFinished = false;
    currentMode = IDLE;
    digitalWrite(LED_PIN, LOW);
    updateLCD();
  }

  // Hold B → Increase countdown setting (+30s loop)
  if (stableB == LOW && now - pressStartB >= LONGPRESS_MS) {
    if ((now - lastHoldRepeatB) >= HOLD_REPEAT_MS) {
      initialCountdownSetting += COUNT_STEP;
      if (initialCountdownSetting > COUNT_MAX) initialCountdownSetting = COUNT_MIN;
      currentMode = IDLE;
      updateLCD();
      lastHoldRepeatB = now;
    }
    longBengaged = true;
  }

  // Tick 1-second
  if (now - lastTick >= TICK_MS) {
    lastTick = now;

    if (isStopwatchRunning) {
      stopwatchSeconds++;
      if (currentMode == STOPWATCH) updateLCD();
    }

    if (isCountdownRunning) {
      if (countdownSeconds > 0) {
        countdownSeconds--;
        if (currentMode == COUNTDOWN) updateLCD();
        if (countdownSeconds == 0) {
          isCountdownRunning = false;
          countdownFinished = true;
          updateLCD();
        }
      }
    }
  }

  // LED behavior
  if (isStopwatchRunning || isCountdownRunning) {
    if (now - lastLedToggle >= LED_BLINK_MS) {
      lastLedToggle = now;
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
    }
  } else if (countdownFinished) {
    // TIME UP blink
    if (now - lastLedToggle >= 700) {
      lastLedToggle = now;
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
    }
  } else {
    digitalWrite(LED_PIN, LOW);
    ledState = LOW;
  }

  updateLCD();
}
