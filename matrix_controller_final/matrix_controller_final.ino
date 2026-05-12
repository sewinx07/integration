/*
 * ============================================================
 *  MATRIX GAME CONTROLLER — Arduino Uno  (Final Version)
 * ============================================================
 *
 *  LEFT SIDE  — Gameplay movement
 *  ────────────────────────────────
 *  A0  Left joystick X   →  'a' (left) / 'd' (right)
 *  A1  Left joystick Y   →  \x20 (jump) / \x01 (sprint)
 *  D2  Walk LEFT button  →  'a'
 *  D3  Jump button       →  \x20  SPACE
 *  D4  Walk RIGHT button →  'd'
 *  D5  Sprint button     →  \x01  LSHIFT
 *
 *  RIGHT SIDE — Shooting + actions
 *  ────────────────────────────────
 *  A2  Right joystick X  →  \x02 LCTRL (shoot — any deflection)
 *  A3  Right joystick Y  →  \x02 LCTRL (shoot — any deflection)
 *  D6  SHOOT button      →  \x02 LCTRL
 *  D7  PAUSE button      →  'p'
 *  D8  GUIDE button      →  'h'
 *  D9  DESTROY button    →  'f'
 *
 *  SYSTEM
 *  ────────────────────────────────
 *  D10 Level 1 button    →  \x18 F11
 *  D11 Level 2 button    →  \x19 F12
 *  D12 Quit button       →  \x03 ESC
 *
 *  LED — D13
 *  ────────────────────────────────
 *  OFF       = idle (no damage)
 *  Blinks 3× = player hit      (6 half-cycles,  ~0.5 s)
 *  Blinks 7× = player death    (14 half-cycles, ~1.1 s)
 *
 * ============================================================
 *  PROTOCOL
 *  Arduino → PC : 'P' + raw_byte + '\n'  (press)
 *                 'R' + raw_byte + '\n'  (release)
 *
 *  Uses Serial.write() for raw bytes so NO character is ever
 *  interpreted or dropped by the serial layer (fixes SPACE).
 *
 *  PC → Arduino : 'B\n'  blink-hit (6 half-cycles)
 *                 'D\n'  blink-death (14 half-cycles)
 *                 'S\n'  stop / LED off
 *
 *  Key byte codes:
 *   \x20 = SPACE  (jump)
 *   \x01 = LSHIFT (sprint)
 *   \x02 = LCTRL  (shoot)
 *   \x03 = ESC
 *   \x0D = RETURN (menu confirm)
 *   \x18 = F11    (level 1)
 *   \x19 = F12    (level 2)
 *   \x00 = NONE   (stick at rest — never sent)
 *
 * FIX [LED-1]: readSerialCommands() inner newline-flush loop removed.
 *   The old code peeked for '\n' in a nested while(), which on a slow
 *   serial buffer split would leave '\n' in the buffer to be read as
 *   an unknown command on the next loop() iteration — causing the
 *   command to occasionally be missed. Unknown bytes (incl. '\n') now
 *   simply fall through the if/else chain harmlessly.
 * ============================================================
 */

// ── Special key byte codes ────────────────────────────────────
#define K_SPACE   '\x20'   // explicit hex — Serial.write safe
#define K_LSHIFT  '\x01'
#define K_LCTRL   '\x02'   // shoot
#define K_ESC     '\x03'
#define K_RETURN  '\x0D'   // Enter / menu confirm
#define K_F11     '\x18'   // Level 1
#define K_F12     '\x19'   // Level 2
#define K_NONE    '\x00'   // stick at rest — never sent

// ── Joystick pins ─────────────────────────────────────────────
#define L_JOY_X   A0
#define L_JOY_Y   A1
#define R_JOY_X   A2
#define R_JOY_Y   A3

// ── Deadzone ──────────────────────────────────────────────────
#define DEAD_LOW    400
#define DEAD_HIGH   624

// ── LED ───────────────────────────────────────────────────────
#define LED_PIN         13
#define BLINK_INTERVAL  80UL    // ms per half-cycle

int           blinkCount = 0;
unsigned long blinkNext  = 0;

// ── Debounce ──────────────────────────────────────────────────
#define DEBOUNCE_MS 50UL

// ── Button descriptor ─────────────────────────────────────────
struct Button {
  int  pin;
  char key;
  bool lastState, currentState;
  unsigned long lastDebounce;
};

Button buttons[] = {
  // LEFT — movement
  {  2, 'a',       HIGH, HIGH, 0 },  // Walk LEFT
  {  3, K_SPACE,   HIGH, HIGH, 0 },  // Jump
  {  4, 'd',       HIGH, HIGH, 0 },  // Walk RIGHT
  {  5, K_LSHIFT,  HIGH, HIGH, 0 },  // Sprint

  // RIGHT — actions
  {  6, K_LCTRL,   HIGH, HIGH, 0 },  // Shoot
  {  7, 'p',       HIGH, HIGH, 0 },  // Pause
  {  8, 'h',       HIGH, HIGH, 0 },  // Guide overlay
  {  9, 'f',       HIGH, HIGH, 0 },  // Destroy platform

  // SYSTEM
  { 10, K_F11,     HIGH, HIGH, 0 },  // Level 1
  { 11, K_F12,     HIGH, HIGH, 0 },  // Level 2
  { 12, K_ESC,     HIGH, HIGH, 0 },  // Quit / back
};
const int N = sizeof(buttons) / sizeof(buttons[0]);

// ── Joystick state ────────────────────────────────────────────
char leftX_active      = K_NONE;
char leftY_active      = K_NONE;
char rightShoot_active = K_NONE;

// ═════════════════════════════════════════════════════════════
//  SERIAL HELPERS
//  Serial.write() sends raw bytes — nothing is ever stripped.
//  This is the critical fix for K_SPACE (\x20) and any other
//  byte that Serial.print() might silently interpret.
// ═════════════════════════════════════════════════════════════
void sendPress(char k) {
  if (k == K_NONE) return;
  Serial.write('P');
  Serial.write((uint8_t)k);
  Serial.write('\n');
}

void sendRelease(char k) {
  if (k == K_NONE) return;
  Serial.write('R');
  Serial.write((uint8_t)k);
  Serial.write('\n');
}

// ═════════════════════════════════════════════════════════════
//  AXIS READ  →  -1 / 0 / +1
// ═════════════════════════════════════════════════════════════
int readAxis(int pin) {
  int v = analogRead(pin);
  if (v < DEAD_LOW)  return -1;
  if (v > DEAD_HIGH) return  1;
  return 0;
}

// ═════════════════════════════════════════════════════════════
//  LEFT JOYSTICK — gameplay movement
//  X: left='a'   right='d'
//  Y: up=SPACE   down=LSHIFT
// ═════════════════════════════════════════════════════════════
void handleLeftJoystick() {
  int x = readAxis(L_JOY_X);
  int y = readAxis(L_JOY_Y);

  char wantX = (x == -1) ? 'a' : (x == 1) ? 'd' : K_NONE;
  if (wantX != leftX_active) {
    sendRelease(leftX_active);
    sendPress(wantX);
    leftX_active = wantX;
  }

  char wantY = (y == -1) ? K_SPACE : (y == 1) ? K_LSHIFT : K_NONE;
  if (wantY != leftY_active) {
    sendRelease(leftY_active);
    sendPress(wantY);
    leftY_active = wantY;
  }
}

// ═════════════════════════════════════════════════════════════
//  RIGHT JOYSTICK — shooting
//  Any deflection on X or Y → hold LCTRL (fire bullet)
//  Returns to center        → release LCTRL
// ═════════════════════════════════════════════════════════════
void handleRightJoystick() {
  int x = readAxis(R_JOY_X);
  int y = readAxis(R_JOY_Y);

  bool wantShoot = (x != 0 || y != 0);
  char wantKey   = wantShoot ? K_LCTRL : K_NONE;

  if (wantKey != rightShoot_active) {
    sendRelease(rightShoot_active);
    sendPress(wantKey);
    rightShoot_active = wantKey;
  }
}

// ═════════════════════════════════════════════════════════════
//  BUTTONS — debounced
// ═════════════════════════════════════════════════════════════
void handleButtons() {
  unsigned long now = millis();
  for (int i = 0; i < N; i++) {
    Button &btn = buttons[i];
    bool reading = digitalRead(btn.pin);
    if (reading != btn.currentState) {
      btn.lastDebounce = now;
      btn.currentState = reading;
    }
    if ((now - btn.lastDebounce) >= DEBOUNCE_MS) {
      if (btn.currentState != btn.lastState) {
        btn.lastState = btn.currentState;
        if (btn.currentState == LOW) sendPress(btn.key);
        else                         sendRelease(btn.key);
      }
    }
  }
}

// ═════════════════════════════════════════════════════════════
//  LED — OFF at idle, blinks only on damage event from PC
// ═════════════════════════════════════════════════════════════
void startBlink(int halfCycles) {
  blinkCount = halfCycles;
  blinkNext  = millis();
}

void updateLED() {
  if (blinkCount <= 0) {
    digitalWrite(LED_PIN, LOW);   // OFF — no damage
    return;
  }
  unsigned long now = millis();
  if (now >= blinkNext) {
    bool ledOn = (blinkCount % 2 == 0);
    digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
    blinkNext = now + BLINK_INTERVAL;
    blinkCount--;
  }
}

// ═════════════════════════════════════════════════════════════
//  READ COMMANDS FROM PC
//   'B' → blink 6  half-cycles (~0.5 s) — player hit
//   'D' → blink 14 half-cycles (~1.1 s) — player death
//   'S' → stop immediately, LED off
//
//  FIX [LED-1]: removed the inner while(peek=='\n') flush loop.
//  '\n' and any other unrecognised byte fall through harmlessly.
//  The old nested flush could leave '\n' in the buffer when bytes
//  arrived split across two loop() calls, causing the command to
//  be silently dropped on the next iteration.
// ═════════════════════════════════════════════════════════════
void readSerialCommands() {
  while (Serial.available()) {
    char cmd = Serial.read();
    if      (cmd == 'B') startBlink(6);
    else if (cmd == 'D') startBlink(14);
    else if (cmd == 'S') { blinkCount = 0; digitalWrite(LED_PIN, LOW); }
    // '\n' and anything else: ignored silently
  }
}

// ═════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(9600);
  for (int i = 0; i < N; i++)
    pinMode(buttons[i].pin, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);   // OFF at startup
}

void loop() {
  readSerialCommands();
  updateLED();
  handleButtons();
  handleLeftJoystick();
  handleRightJoystick();
}
