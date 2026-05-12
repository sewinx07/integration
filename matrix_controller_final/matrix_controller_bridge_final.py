"""
matrix_controller_bridge_final.py
===================================
Matches matrix_controller_final.ino EXACTLY.

PROTOCOL the Arduino sends:
──────────────────────────────────────────────────────────────
  "Pk\n"   key press   (k = the char defined in the .ino)
  "Rk\n"   key release

  Special single-byte key codes:
    \x00  = K_NONE    → ignored
    \x01  = LSHIFT    → Key.shift_l   (sprint)
    \x02  = LCTRL     → Key.ctrl_l    (shoot — right analog OR D6)
    \x03  = ESC       → Key.esc       (quit / back)
    \x0D  = RETURN    → Key.enter + mouse click (menu confirm)
    \x18  = F11       → Key.f11       (switch to level 1)
    \x19  = F12       → Key.f12       (switch to level 2)

  Plain ASCII chars sent as-is:
    'a'  walk left     'd'  walk right
    ' '  jump          'p'  pause
    'h'  guide         'f'  destroy platform

PROTOCOL the bridge sends back to Arduino:
──────────────────────────────────────────
  'B\n'  → blink LED  6 half-cycles  (~0.5 s) — player hit
  'D\n'  → blink LED 14 half-cycles  (~1.1 s) — player death
  'S\n'  → stop blink immediately, LED off

LED BEHAVIOUR
─────────────
LED is OFF at all times except when the player takes damage.
  • Hit   → 3 blinks then off
  • Death → 7 blinks then off

The bridge watches the game process stdout for "HIT" and "DEAD"
lines printed by main.c and sends the matching command to Arduino.

─────────────────────────────────────────────────────────────────
REQUIRED CHANGE IN main.c   [LED-2]
─────────────────────────────────────────────────────────────────
Add this block in the APP_GAME update section of main.c, AFTER
the checkBulletsVsEnemies() calls and BEFORE the death-detection
block (before "int p1Alive = p1.isAlive;").

It must come before renderMinimap() because minimap.c resets
damageEvent to 0 when it draws the red flash overlay.

    /* ── LED feedback for Arduino controller ── */
    if (p1.damageEvent) {
        printf(p1.isAlive ? "HIT\\n" : "DEAD\\n");
        fflush(stdout);
    }
    if (dispMode == MODE_MULTI && p2.damageEvent) {
        printf(p2.isAlive ? "HIT\\n" : "DEAD\\n");
        fflush(stdout);
    }

Do NOT reset damageEvent here — renderMinimap() handles that.
─────────────────────────────────────────────────────────────────

Install once:
    pip install pyserial pynput

Run:
    python matrix_controller_bridge_final.py
"""

import serial
import sys
import time
import subprocess
import threading
import os
from pynput.keyboard import Controller as KbdController, Key
from pynput.mouse    import Controller as MouseController, Button as MButton

# ══════════════════════════════════════════════════════════════
#  CONFIGURATION  — edit these lines
# ══════════════════════════════════════════════════════════════
PORT     = '/dev/ttyACM0'   # Windows: 'COM3' etc.  Linux: '/dev/ttyACM0'
BAUDRATE = 9600
GAME_EXE = './game'         # path to your compiled game binary
# ══════════════════════════════════════════════════════════════

# ── Controllers ───────────────────────────────────────────────
kbd   = KbdController()
mouse = MouseController()

# ── Held-key tracker (for clean release on exit) ─────────────
held_keys = set()

# ══════════════════════════════════════════════════════════════
#  KEY MAP  — maps .ino byte codes → pynput keys
# ══════════════════════════════════════════════════════════════
SPECIAL_KEYS = {
    '\x01': Key.shift_l,   # K_LSHIFT → sprint
    '\x02': Key.ctrl_l,    # K_LCTRL  → shoot (right analog + D6)
    '\x03': Key.esc,       # K_ESC    → quit / back
    '\x18': Key.f11,       # K_F11    → level 1
    '\x19': Key.f12,       # K_F12    → level 2
    '\x20': Key.space,     # K_SPACE  → jump (force pynput Key.space)
    # \x0D (K_RETURN) handled separately below
}

# ══════════════════════════════════════════════════════════════
#  KEY HELPERS
# ══════════════════════════════════════════════════════════════
def resolve(code: str):
    """Return the pynput key object for a given byte code."""
    return SPECIAL_KEYS.get(code, code)

def do_press(code: str):
    key = resolve(code)
    try:
        kbd.press(key)
        held_keys.add(key)
    except Exception as e:
        print(f"[WARN] press({repr(key)}): {e}")

def do_release(code: str):
    key = resolve(code)
    try:
        kbd.release(key)
        held_keys.discard(key)
    except Exception as e:
        print(f"[WARN] release({repr(key)}): {e}")

def release_all():
    """Release every held key — called on bridge shutdown."""
    for k in list(held_keys):
        try: kbd.release(k)
        except Exception: pass
    held_keys.clear()

# ══════════════════════════════════════════════════════════════
#  MOUSE HELPER  (used only for \x0D menu confirm click)
# ══════════════════════════════════════════════════════════════
def do_click():
    """Left-click at the current OS cursor position."""
    try:
        mouse.press(MButton.left)
        time.sleep(0.04)   # 40 ms — SDL2 needs time to register
        mouse.release(MButton.left)
    except Exception as e:
        print(f"[WARN] mouse click: {e}")

# ══════════════════════════════════════════════════════════════
#  MAIN SERIAL PARSER
# ══════════════════════════════════════════════════════════════
def handle_line(line: str):
    """
    Parse one line from the Arduino and act on it.
    Format: "Pk" = press key k,  "Rk" = release key k.
    """
    if len(line) < 2:
        return

    action = line[0]   # 'P' = press  /  'R' = release
    code   = line[1]   # key character

    # ── K_NONE (\x00) — ignore ────────────────────────────────
    if code == '\x00':
        return

    # ── K_RETURN (\x0D) — menu confirm ────────────────────────
    # Sends Key.enter (harmless in gameplay) plus a mouse click
    # so SDL2 menus register SDL_MOUSEBUTTONDOWN on hovered button.
    if code == '\x0D':
        if action == 'P':
            try:
                kbd.press(Key.enter)
                held_keys.add(Key.enter)
            except Exception:
                pass
            threading.Thread(target=do_click, daemon=True).start()
        elif action == 'R':
            try:
                kbd.release(Key.enter)
                held_keys.discard(Key.enter)
            except Exception:
                pass
        return

    # ── All other keys (ASCII + special incl. LCTRL shoot) ────
    if   action == 'P': do_press(code)
    elif action == 'R': do_release(code)

# ══════════════════════════════════════════════════════════════
#  LED COMMANDS — sent to Arduino
# ══════════════════════════════════════════════════════════════
def send_cmd(cmd: str):
    """
    Send a single-char command to the Arduino followed by newline.
    The Arduino's readSerialCommands() reads byte-by-byte and
    ignores '\n', so this is safe even if bytes arrive split
    across two loop() calls.
    """
    try:
        ser.write((cmd + '\n').encode())
    except Exception as e:
        print(f"[WARN] serial write: {e}")

def blink_hit():
    """Player took damage — 3 blinks (6 half-cycles)."""
    print("  [LED] HIT  — 3 blinks")
    send_cmd('B')

def blink_death():
    """Player died — 7 blinks (14 half-cycles)."""
    print("  [LED] DEAD — 7 blinks")
    send_cmd('D')

def led_off():
    """Stop any active blink and turn LED off."""
    send_cmd('S')

# ══════════════════════════════════════════════════════════════
#  GAME PROCESS — launch & watch stdout for HIT / DEAD
#
#  FIX [LED-2]: The game must print "HIT\n" or "DEAD\n" to stdout
#  when p1.damageEvent is set. See the docstring at the top of this
#  file for the exact lines to add to main.c.
#
#  The print must happen BEFORE renderMinimap() is called because
#  minimap.c resets damageEvent = 0 inside renderMinimap().
#  If the printf comes after renderMinimap, damageEvent is always
#  0 and the LED never receives a trigger.
# ══════════════════════════════════════════════════════════════
game_process = None

def watch_stdout():
    """
    Background thread: reads game stdout line by line.
    Triggers LED blink commands when "HIT" or "DEAD" is seen.
    Ignores all other stdout lines (debug prints, score info, etc).
    """
    if not game_process:
        return
    try:
        for raw in game_process.stdout:
            msg = raw.decode('utf-8', errors='ignore').strip()
            if   msg == 'HIT':  blink_hit()
            elif msg == 'DEAD': blink_death()
    except Exception:
        pass

def launch_game():
    global game_process
    if not os.path.exists(GAME_EXE):
        print(f"[WARN] '{GAME_EXE}' not found.")
        print("       Launch the game manually — bridge still works.")
        print("       LED feedback will NOT work unless the bridge")
        print("       launched the game (stdout pipe required).")
        return
    print(f"[OK] Launching: {GAME_EXE}")
    game_process = subprocess.Popen(
        [GAME_EXE],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
    )
    threading.Thread(target=watch_stdout, daemon=True).start()
    print("[OK] Game running, LED watcher active.")

# ══════════════════════════════════════════════════════════════
#  SERIAL CONNECTION
# ══════════════════════════════════════════════════════════════
try:
    ser = serial.Serial(PORT, BAUDRATE, timeout=1)
    time.sleep(2)   # wait for Arduino reset after serial connect
    print(f"[OK] Arduino connected on {PORT}")
except serial.SerialException as e:
    print(f"[ERROR] Cannot open {PORT}: {e}")
    print("  1) Is the Arduino plugged in?")
    print("  2) Is the Arduino IDE Serial Monitor closed?")
    print("  3) Is PORT set correctly at the top of this file?")
    sys.exit(1)

# ══════════════════════════════════════════════════════════════
#  ENTRY POINT
# ══════════════════════════════════════════════════════════════
def main():
    print("=" * 56)
    print("  MATRIX GAME Controller Bridge — Final Version")
    print("=" * 56)
    print(f"  Port    : {PORT}")
    print(f"  Game    : {GAME_EXE}")
    print()
    print("  LEFT ANALOG / D2–D5:")
    print("    ←/→  move (a / d)")
    print("    ↑    jump (SPACE)")
    print("    ↓    sprint (LSHIFT)")
    print()
    print("  RIGHT ANALOG / D6:")
    print("    any deflection → SHOOT (LCTRL)")
    print()
    print("  D7  pause      D8  guide     D9  destroy platform")
    print("  D10 level 1    D11 level 2   D12 quit (ESC)")
    print()
    print("  LED: OFF=idle   3 blinks=hit   7 blinks=death")
    print()

    launch_game()
    print("[OK] Bridge running. Press Ctrl+C to stop.\n")

    try:
        while True:
            raw = ser.readline()
            if not raw:
                continue
            # Strip only newline characters — preserve spaces and
            # control bytes that are part of the key protocol.
            line = raw.decode('utf-8', errors='ignore').rstrip('\r\n')
            if len(line) >= 2:
                handle_line(line)

    except KeyboardInterrupt:
        print("\n[INFO] Stopping bridge...")

    finally:
        release_all()
        led_off()          # LED off on exit
        ser.close()
        if game_process and game_process.poll() is None:
            game_process.terminate()
        print("[INFO] Done.")

if __name__ == '__main__':
    main()