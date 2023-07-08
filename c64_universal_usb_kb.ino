/* Commodore 64 Universal USB Keyboard for Pro Micro by Rodney Hester

   Allows use of the Commodore 64 keyboard over USB to multiple target
   platforms without modification

   Inspired by code by DJ Sures (Synthiam.com) and Chris Garrett @ makerhacks

   MANY thanks to David Hover/homerghost from the MiSTer FPGA Forum for his
   helpful testing, insightful suggestions and wireless Teslian wizardry! 

   Framework adapted from https://github.com/venice1200/c64-keyboard-USB and
   https://github.com/omiq/c64-keyboard-USB
   
   Tested with Anykey 64 by T'Pau at https://github.com/T-Pau/Anykey and
   Keyboard-Test from NTS at https://csdb.dk/release/?id=98411

   This approach could be adapted with required changes for operation on
   Teensy, Leonardo, and similar Arduino devices but is unsupported by the
   author.
*/

/* Set to true to blink LED when keyboard target changes or false to disable
   If the keyboard LED is connected to pin 0, it will be used, otherwise the
   onboard TX/yellow LED will be used
*/
bool allowBlink = true;

// Uncomment to debug using Arduino IDE Serial Monitor at 115200 baud
// WARNING: DO NOT ENABLE UNLESS CONNECTED TO MACHINE WITH SERIAL MONITOR ACTIVE
//#define DEBUG

/* THEORY OF OPERATION
   ===================
   The 8x8 Commodore 64 keyboard matrix is read serially, with RESTORE read on
   keyboard pin 3. An 8x10 matrix is used in practice to simplify row and
   column math.

   Individual keys are mapped via lookup table to their emulated counterparts
   (and overridden as necessary for specific keys and target platforms).

   All keyboard mappings assume US keyboards (both physical C64 keyboard and
   endpoint OS keyboard mappings).

   Most keypresses are handled literally, with a keypress transmitted on
   detection (after debounce) and an unpress transmitted on release. This
   precisely matches the behavior of the original keyboard input, including
   "ghosting" issues common to keyboards without diodes.

   VICE mapping is positional, which is NOT the software default.

   Special handlers are provided for convenience menus and keymap cycling:

   C=+F7        Opens convenience menu (on MiSTer and by default on BMC64),
                acts as (Left) Windows key in ASCII mode

   C=+F1        Cycle keymaps forward, wrapping around at the end. Keymap
                sequence is defined below.
   C=+SHIFT+F1  Cycle keymaps backward, wrapping around at the beginning.
   C=+CTRL+F1   Reset keymap to 1 regardless of prior setting
*/

// Keymap definitions
#define ASCII        1 // (Native OS) US keyboard definition only!
#define VICE2_BMC64  2 // VICE 2.x/BMC64 C64 and VIC-20 positional keyboard mapping
#define VICE3        3 // VICE 3.x C64 and VIC-20 positional keyboard mapping
#define MISTER       4 // MiSTer C64 and VIC-20 cores

/* VICE and MiSTer C64 mode on C128 is not well supported due to differences
   in mapping across the emulators.

   If blink support has been uncommented as described earlier, either the
   internal RX/yellow LED or the external/keyboard LED (the latter being
   favored if available) will be used to communicate the current keymap
   setting via blinking one second after power-up and again upon changes.
   See CONNECTIONS below for details.

   The currently selected map will be saved to EEPROM and restored
   each time the keyboard is plugged into USB until changed again. EEPROM
   will accept approximately 100,000 write cycles (or keyboard mapping
   changes) before failure.

   Shifted selection is not possible in ASCII mode due to undefined intent
   when pressing SHIFT+CRSRDN or SHIFT+CRSRRT.

   CAVEAT EMPTOR: The Arduino HID library takes (reasonable for its intent)
   liberties with SHIFT key handling. Keystrokes that would not ordinarily be
   shifted on a traditional PC keyboard are deliberately unshifted, and those
   requiring SHIFT are implicitly shifted by the library itself. This causes
   side-effects that must be handled via temporary manual handling of SHIFT
   as necessary [see unshift() and the end of release()].

   Please note that certain declared values for key equivalents are unused in
   code due to the Arduino HID function requirement that any non-printable key
   be referenced explicitly by macro and cannot be passed by variable.
*/

/* C64->ASCII mappings
     ARROW LEFT           `
     (SHIFT) ARROW LEFT   ~
     (SHIFT) -            _
     CLR                  END
     INST                 DEL
     POUND		  \
     (SHIFT) POUND        |
     ARROW UP             ^
     (SHIFT) @            {
     (SHIFT) *            }
     RESTORE              TAB
     STOP                 ESC
     C=                   LEFT ALT
     (CTRL) CRSRUP        PAGE UP
     (CTRL) CRSRDN        PAGE DOWN
*/

#define MAXTARGETS      4     // Highest keyboard target number
#define BLINK_RATE      75    // Milliseconds, smaller = faster
#define LED_START_DELAY 1000  // Millseconds delay until blinking target at startup

// HID library definitions, from https://github.com/NicoHood/HID
#include "HID-Project.h"
// EEPROM support
#include <EEPROM.h>

/* =======================================================================================
   -------------------------------------------------
   CONNECTIONS (to Sparkfun Pro Micro or compatible)
   -------------------------------------------------

    C64  |  Arduino  | Port
   ==========================
     20      2 - SDA    A7
     19      3 - SCL    A1
     18      4 - A6     A2
     17      5          A3
     16      6 - A7     A4
     15      7          A5
     14      8 - A8     A6
     13      9 - A9     A0
     12     10 - A10    B0
     11     16 - MOSI   B1
     10     14 - MISO   B2
      9     15 - SCLK   B7
      8     18 - A0     B4
      7     19 - A1     B5
      6     20 - A2     B6
      5     21 - A3     B3
      4     N/C         VCC
      3      1 - TX   RESTORE
      2     N/C         N/C
      1     GND         GND
   ==========================

   Optionally, the Commodore 64 power LED may be connected to the Pro Micro
   for illumination when connected to USB. Connect the red wire from the LED
   in series with a 390 ohm (as standard, 180-470 ohms depending on desired
   brightness with lower values being brighter) resistor to Pro Micro pin 0
   and the black wire to any available GND.

   -----------------------------------
   Commodore 64 keyboard matrix layout
   -----------------------------------

   A       10       16       14       21       18       19       20       15        1        0

      RC   C0       C1       C2       C3       C4       C5       C6       C7       C8       C9

   9  R0   DEL      RETURN   CRSRRT   F7       F1       F3       F5       CRSRDN   [1]
   3  R1   3        W        A        4        Z        S        E        LSHIFT   [1]
   4  R2   5        R        D        6        C        F        T        X        [1]
   5  R3   7        Y        G        8        B        H        U        V        [1]
   6  R4   9        I        J        0        M        K        O        N        [1]
   7  R5   +        P        L        –        .        :        @        ,        [1]
   8  R6   £        *        ;        HOME     RSHIFT   =        ↑        /        RESTORE
   2  R7   1        ←        CTRL     2        SPCBAR   C=       Q        STOP     [1]

   [1] As RESTORE is grounded and acts in original hardware as a switch,
       depressing it actually activates all rows for column 8 simultaneously.
       A software filter is used to reduce it to a single row.
*/

int  thisKey;
bool isKeyDown;
bool lShifted, rShifted, ctrlPressed, cmdrPressed = false;
bool enterMenu, pageScr = false;
bool clrPressed, instPressed, runPressed = false;
int lastKeyState[80];
long lastDebounceTime[80];
bool shifted[80];
int debounceDelay = 5;
int rowPinMap[8] = {9, 3, 4, 5, 6, 7, 8, 2};
int colPinMap[9] = {10, 16, 14, 21, 18, 19, 20, 15, 1};
char keymap[80];
int activeLED, target;

void setKeymap() {
  // Key definitions with target-specific mappings
  // NOTE: Non-literal names are Arduino macros and ignored here!

  #ifdef DEBUG
    Serial.print(String(target));
    Serial.println(" MAP");
  #endif

  // Default key mapping to VICE3 style
  // NOTE: Older releases of VICE had different positional keyboard locations!
  // Current mappings are for VICE 3.7 (Positional keyboard mapping).

  // FIRST ROW
  keymap[71] = '`';
  keymap[70] = '1';
  keymap[73] = '2';
  keymap[10] = '3';
  keymap[13] = '4';
  keymap[20] = '5';
  keymap[23] = '6';
  keymap[30] = '7';
  keymap[33] = '8';
  keymap[40] = '9';
  keymap[43] = '0';
  keymap[50] = '-';
  keymap[53] = '=';
  keymap[60] = KEY_END;
  keymap[63] = KEY_HOME;
  keymap[0]  = KEY_BACKSPACE;

  // SECOND ROW
  keymap[72] = KEY_TAB;
  keymap[76] = 'q';
  keymap[11] = 'w';
  keymap[16] = 'e';
  keymap[21] = 'r';
  keymap[26] = 't';
  keymap[31] = 'y';
  keymap[36] = 'u';
  keymap[41] = 'i';
  keymap[46] = 'o';
  keymap[51] = 'p';
  keymap[56] = '[';
  keymap[61] = ']';
  keymap[66] = '\\';
  keymap[68] = KEY_PAGE_UP;

  // THIRD ROW
  keymap[77] = KEY_ESC;
  keymap[17] = KEY_LEFT_SHIFT;
  keymap[12] = 'a';
  keymap[15] = 's';
  keymap[22] = 'd';
  keymap[25] = 'f';
  keymap[32] = 'g';
  keymap[35] = 'h';
  keymap[42] = 'j';
  keymap[45] = 'k';
  keymap[52] = 'l';
  keymap[55] = ';';
  keymap[62] = '\'';
  keymap[65] = KEY_PAGE_DOWN;
  keymap[1]  = KEY_ENTER;

  // FOURTH ROW
  keymap[75] = KEY_LEFT_CTRL;
  keymap[17] = KEY_LEFT_SHIFT;
  keymap[14] = 'z';
  keymap[27] = 'x';
  keymap[24] = 'c';
  keymap[37] = 'v';
  keymap[34] = 'b';
  keymap[47] = 'n';
  keymap[44] = 'm';
  keymap[57] = ',';
  keymap[54] = '.';
  keymap[67] = '/';
  keymap[64] = KEY_RIGHT_SHIFT;
  keymap[7]  = KEY_DOWN_ARROW;
  keymap[2]  = KEY_RIGHT_ARROW;

  // SPACE AND FUNCTION KEYS
  keymap[74] = ' ';
  keymap[4]  = KEY_F1;
  keymap[5]  = KEY_F3;
  keymap[6]  = KEY_F5;
  keymap[3]  = KEY_F7;

  switch (target) {
    case ASCII:
      keymap[50] = '+';              // +
      keymap[53] = '-';              // -
      keymap[60] = '\\';             // £
      keymap[72] = KEY_LEFT_CTRL;    // CTRL
      keymap[56] = '@';              // @
      keymap[61] = '*';              // *
      keymap[66] = '^';              // ↑
      keymap[68] = KEY_TAB;          // RESTRE
      keymap[55] = ':';              // :
      keymap[62] = ';';              // ;
      keymap[65] = '=';              // =
      keymap[75] = KEY_LEFT_ALT;     // C=
      break;

    // VICE 2.x or BMC64
    case VICE2_BMC64:
      keymap[60] = KEY_INSERT;       // £
      keymap[66] = KEY_DELETE;       // ↑
      keymap[65] = '\\';             // =
      break;

    case MISTER:
      keymap[50] = '=';              // +
      keymap[53] = '-';              // -
      keymap[60] = '\\';             // £
      keymap[72] = KEY_LEFT_CTRL;    // CTRL
      keymap[66] = KEY_F9;           // ↑
      keymap[68] = KEY_F11;          // RESTRE
      keymap[65] = KEY_F10;          // =
      keymap[75] = KEY_LEFT_ALT;     // C=
      break;
  }
}

void setup() {
  // Disable onboard LEDs
  pinMode(LED_BUILTIN_RX, INPUT);
  pinMode(LED_BUILTIN_TX, INPUT);

  #ifdef DEBUG
    // Start up serial support and wait until complete
    Serial.begin(115200);
    while (!Serial) {}
  #endif

  // Turn the external LED on if it exists and set active LED
  activeLED = 0;
  pinMode(activeLED, INPUT_PULLUP);
  digitalWrite(activeLED, LOW);
  if (!digitalRead(activeLED)) {
    #ifdef DEBUG
      Serial.print(String(activeLED));
      Serial.println(" DETECT");
    #endif
    pinMode(activeLED, OUTPUT);
    digitalWrite(activeLED, HIGH);
  } else {
    #ifdef DEBUG
      Serial.print(String(activeLED));
      Serial.println(" ABSENT");
    #endif
    activeLED = 17;
  }

  // Set up HID keyboard
  BootKeyboard.begin();
  BootKeyboard.releaseAll();

  // Initialize key states and pins
  for (int i = 0; i < 80; i++) lastKeyState[i] = false;
  for (int row = 0; row < 8; row++) pinMode(rowPinMap[row], INPUT_PULLUP);
  for (int col = 0; col < 9; col++) pinMode(colPinMap[col], INPUT_PULLUP);

  // Read EEPROM value for keyboard target
  target = EEPROM.read(0);
  // Target not valid, reset to 1
  if (target < 1 || target > MAXTARGETS) {
    target = 1;
    EEPROM.write(0, target);
  }
  #ifdef DEBUG
    Serial.print(String(target));
    Serial.println(" EEPROM");
  #endif
  setKeymap();

  // Delay start to buy time for the human to read the LEDs
  delay(LED_START_DELAY);
  blinkLED(target);
}

// Is keyboard in actively-SHIFTed state?
bool shiftedKey() {
  return (lShifted || rShifted);
}

// Is target MiSTer or ASCII?
bool misterOrASCII() {
  return (target == MISTER || target == ASCII);
}

// Temporarily remove SHIFTed state for keys that map to opposite state on a
// standard US keyboard
void unshift() {
  if (lShifted)
    BootKeyboard.release(KEY_LEFT_SHIFT);
  if (rShifted)
    BootKeyboard.release(KEY_RIGHT_SHIFT);
}

void blinkLED(int times) {
  // External LED control is inverted from onboard LED
  // Internal LED is managed by INPUT/OUTPUT control, external managed by
  // pin HIGH/LOW state
  if (allowBlink) {
    for (int blink = 0; blink < times; blink++) {
      if (activeLED != 0)
        pinMode(activeLED, OUTPUT);
      digitalWrite(activeLED, LOW);
      delay(BLINK_RATE);
      if (activeLED != 0)
        pinMode(activeLED, INPUT);
      else
        digitalWrite(activeLED, HIGH);
      delay(BLINK_RATE);
    }
  }
}

void press(uint8_t key) {
  #ifdef DEBUG
    Serial.print(String(thisKey));
    Serial.println(" PRESS");
  #endif

  switch (key) {
    // MiSTer and ASCII SHIFTed keys 6-9 have different symbols and 0 requires unSHIFTing
    case 23: // 6
      if (shiftedKey() && misterOrASCII()) {
        shifted[key] = true;
        BootKeyboard.press('&');
      } else
        BootKeyboard.press(keymap[key]);
      break;

    case 30: // 7
      if (shiftedKey()) {
        shifted[key] = true;
        switch (target) {
          case MISTER:
            BootKeyboard.press('^');
            break;
          case ASCII:
            unshift();
            BootKeyboard.press('\'');
            break;
          default:
            BootKeyboard.press(keymap[key]);
        }
      } else
        BootKeyboard.press(keymap[key]);
      break;

    case 33: // 8
      if (shiftedKey() && misterOrASCII()) {
        shifted[key] = true;
        BootKeyboard.press('(');
      } else
        BootKeyboard.press(keymap[key]);
      break;

    case 40: // 9
      if (shiftedKey() && misterOrASCII()) {
        shifted[key] = true;
        BootKeyboard.press(')');
      } else
        BootKeyboard.press(keymap[key]);
      break;

    case 43: // 0
      unshift();
      BootKeyboard.press(keymap[key]);
      break;

    case 0:  // DEL
      if (target == ASCII && shiftedKey()) {
        shifted[key] = true;
        unshift();
        BootKeyboard.press(KEY_DELETE);
      } else
        BootKeyboard.press(KEY_BACKSPACE);
      break;

    case 1:  // RETURN
      BootKeyboard.press(KEY_RETURN);
      break;

    case 2:  // CRSRRT
      // Allow true cursor left for quality-of-life
      if (shiftedKey()) {
        shifted[key] = true;
        if (target == ASCII)
          unshift();
        BootKeyboard.press(KEY_LEFT_ARROW);
      } else
        BootKeyboard.press(KEY_RIGHT_ARROW);
      break;

    case 3:  // F7
      switch (target) {
        case MISTER:
          // Special handler - C=+F7/F12 opens MiSTer menu
          if (cmdrPressed) {
            enterMenu = true;
            BootKeyboard.release(KEY_LEFT_ALT);
            delay(25);
            BootKeyboard.press(KEY_F12);
            delay(25);
            BootKeyboard.press(KEY_LEFT_ALT);
          } else
            BootKeyboard.press(KEY_F7);
          break;
        case ASCII:
          // Special handler - C=+F7 is (Left) Windows key
          if (cmdrPressed) {
            enterMenu = true;
            BootKeyboard.release(KEY_LEFT_ALT);
            BootKeyboard.press(KEY_LEFT_WINDOWS);
          } else
          if (shiftedKey()) {
            shifted[key] = true;
            unshift();
            BootKeyboard.press(KEY_F8);
          } else
            BootKeyboard.press(KEY_F7);
          break;
        default:
          BootKeyboard.press(KEY_F7);
      }
      break;

    case 4:  // F1
      // Special handler - C=+F1 rotates keyboard targets
      if (cmdrPressed) {
        // Reset target if C= and CTRL are active
        if (ctrlPressed)
          target = 1;
        else {
          if (shiftedKey()) {
            // If SHIFT is active, rotate backward
            target--;
            if (target < 1)
              target = MAXTARGETS;
          } else {
            // Otherwise rotate forward
            target++;
            if (target > MAXTARGETS)
              target = 1;
          }
        }
        EEPROM.write(0, target);

        // Reset keyboard
        BootKeyboard.releaseAll();

        setKeymap();
        blinkLED(target);
      } else
      if (target == ASCII && shiftedKey()) {
        shifted[key] = true;
        unshift();
        BootKeyboard.press(KEY_F2);
      } else
        BootKeyboard.press(KEY_F1);
      break;

    case 5:  // F3
      if (target == ASCII && shiftedKey()) {
        shifted[key] = true;
        unshift();
        BootKeyboard.press(KEY_F4);
      } else
        BootKeyboard.press(KEY_F3);
      break;

    case 6:  // F5
      if (target == ASCII && shiftedKey()) {
        shifted[key] = true;
        unshift();
        BootKeyboard.press(KEY_F6);
      } else
        BootKeyboard.press(KEY_F5);
      break;

    case 7:  // CRSRDN
      // Allow true cursor up for quality-of-life
      if (shiftedKey()) {
        shifted[key] = true;
        if (target == ASCII) {
          unshift();
          if (ctrlPressed) {
            // CTRL+CRSRUP does PAGE UP
            pageScr = true;
            BootKeyboard.release(KEY_LEFT_CTRL);
            BootKeyboard.press(KEY_PAGE_UP);
          }  else
            BootKeyboard.press(KEY_UP_ARROW);
        } else
          BootKeyboard.press(KEY_UP_ARROW);
      } else {
        if (target == ASCII) {
          if (ctrlPressed) {
            // CTRL+CRSRDN does PAGE DOWN
            pageScr = true;
            BootKeyboard.release(KEY_LEFT_CTRL);
            BootKeyboard.press(KEY_PAGE_DOWN);
          } else
            BootKeyboard.press(KEY_DOWN_ARROW);
        } else
        BootKeyboard.press(KEY_DOWN_ARROW);
      }
      break;

    case 17: // LSHIFT
      lShifted = true;
      BootKeyboard.press(KEY_LEFT_SHIFT);
      break;

    case 55: // :
      if (target == ASCII && shiftedKey()) {
        shifted[key] = true;
        unshift();
        BootKeyboard.press('[');
      } else
        BootKeyboard.press(keymap[key]);
      break;

    case 56: // @
      if (target == ASCII && shiftedKey()) {
        shifted[key] = true;
        BootKeyboard.press('{');
      } else
        BootKeyboard.press(keymap[key]);
      break;

    case 60: // £
      switch (target) {
        case VICE3:
          BootKeyboard.press(KEY_END);
          break;
        case VICE2_BMC64:
          BootKeyboard.press(KEY_INSERT);
          break;
        default:
          BootKeyboard.press(keymap[key]);
      }
      break;

    case 61: // *
      if (target == ASCII && shiftedKey()) {
        shifted[key] = true;
        BootKeyboard.press('}');
      } else
        BootKeyboard.press(keymap[key]);
      break;

    case 62: // ;
      if (target == ASCII && shiftedKey()) {
        shifted[key] = true;
        unshift();
        BootKeyboard.press(']');
      } else
        BootKeyboard.press(keymap[key]);
      break;

    case 63: // HOME
      if (target == ASCII && shiftedKey()) {
        clrPressed = true;
        unshift();
        BootKeyboard.press(KEY_END);
      } else
        BootKeyboard.press(KEY_HOME);
      break;

    case 64: // RSHIFT
      rShifted = true;
      BootKeyboard.press(KEY_RIGHT_SHIFT);
      break;

    case 65: // =
      if (target == ASCII)
        unshift();
      switch (target) {
        case VICE3:
          BootKeyboard.press(KEY_PAGE_DOWN);
          break;
        case MISTER:
          BootKeyboard.press(KEY_F10);
          break;
        default:
          BootKeyboard.press(keymap[key]);
      }
      break;

    case 66: // ↑
      switch (target) {
        case MISTER:
          BootKeyboard.press(KEY_F9);
          break;
        case VICE2_BMC64:
          BootKeyboard.press(KEY_DELETE);
          break;
        default:
          BootKeyboard.press(keymap[key]);
      }
      break;

    case 68: // RESTORE
      switch (target) {
        case MISTER:
          BootKeyboard.press(KEY_F11);
          break;
        case ASCII:
          BootKeyboard.press(KEY_TAB);
          break;
        default:
          BootKeyboard.press(KEY_PAGE_UP);
      }
      break;

    case 72: // CTRL
      ctrlPressed = true;
      switch (target) {
        case VICE3:
        case VICE2_BMC64:
          BootKeyboard.press(KEY_TAB);
          break;
        default:
          BootKeyboard.press(KEY_LEFT_CTRL);
      }
      break;

    case 73: // 2
      if (target == ASCII && shiftedKey()) {
        shifted[key] = true;
        BootKeyboard.press('"');
      } else
        BootKeyboard.press(keymap[key]);
      break;

    case 75: // C=
      cmdrPressed = true;
      if (misterOrASCII())
        BootKeyboard.press(KEY_LEFT_ALT);
      else
        BootKeyboard.press(KEY_LEFT_CTRL);
      break;

    case 77: // STOP
      BootKeyboard.press(KEY_ESC);
      break;

    default:
      BootKeyboard.press(keymap[key]);
  }
}

void release(uint8_t key) {
  #ifdef DEBUG
    Serial.print(String(thisKey));
    Serial.println(" RELEASE");
  #endif

  switch (key) {
    // MiSTer and ASCII SHIFTed keys 6-9 have different symbols
    case 23: // 6
      if (shifted[key]) {
        shifted[key] = false;
        BootKeyboard.release('&');
      } else
        BootKeyboard.release(keymap[key]);
      break;

    case 30: // 7
      if (shifted[key]) {
        shifted[key] = false;
        switch (target) {
          case MISTER:
            BootKeyboard.release('^');
            break;
          case ASCII:
            BootKeyboard.release('\'');
            break;
          default:
            BootKeyboard.release(keymap[key]);
        }
      } else
        BootKeyboard.release(keymap[key]);
      break;

    case 33: // 8
      if (shifted[key]) {
        shifted[key] = false;
        BootKeyboard.release('(');
      } else
        BootKeyboard.release(keymap[key]);
      break;

    case 40: // 9
      if (shifted[key]) {
        shifted[key] = false;
        BootKeyboard.release(')');
      } else
        BootKeyboard.release(keymap[key]);
      break;

    case 0:  // DEL
      if (target == ASCII && shifted[key]) {
        shifted[key] = false;
        BootKeyboard.release(KEY_DELETE);
      } else
        BootKeyboard.release(KEY_BACKSPACE);
      break;

    case 1:  // RETURN
      BootKeyboard.release(KEY_RETURN);
      break;

    case 2:  // CRSRRT
      // Allow true cursor left for quality-of-life
      if (shifted[key]) {
        shifted[key] = false;
        BootKeyboard.release(KEY_LEFT_ARROW);
      } else
        BootKeyboard.release(KEY_RIGHT_ARROW);
      break;

    case 3:  // F7
      switch (target) {
        case MISTER:
          // Special handler - Ctrl+F7/F12 opens MiSTer menu
          if (enterMenu) {
            enterMenu = false;
            BootKeyboard.release(KEY_F12);
          } else
            BootKeyboard.release(KEY_F7);
          break;
        case ASCII:
          if (enterMenu) {
            enterMenu = false;
            BootKeyboard.release(KEY_LEFT_WINDOWS);
          } else
          if (shifted[key]) {
            shifted[key] = false;
            BootKeyboard.release(KEY_F8);
          } else
            BootKeyboard.release(KEY_F7);
        default:
          BootKeyboard.release(KEY_F7);
      }
      break;

    case 4:  // F1
      if (target == ASCII && shifted[key]) {
        shifted[key] = false;
        BootKeyboard.release(KEY_F2);
      } else
        BootKeyboard.release(KEY_F1);
      break;

    case 5:  // F3
      if (target == ASCII && shifted[key]) {
        shifted[key] = false;
        BootKeyboard.release(KEY_F4);
      } else
        BootKeyboard.release(KEY_F3);
      break;

    case 6:  // F5
      if (target == ASCII && shifted[key]) {
        shifted[key] = false;
        BootKeyboard.release(KEY_F6);
      } else
        BootKeyboard.release(KEY_F5);
      break;

    case 7:  // CRSRDN
      // Allow true cursor up for quality-of-life
      if (shifted[key]) {
        shifted[key] = false;
        if (pageScr && target == ASCII) {
          pageScr = false;
          BootKeyboard.release(KEY_PAGE_UP);
          // Restore CTRL if held down
          if (ctrlPressed)
            BootKeyboard.press(KEY_LEFT_CTRL);
        } else
          BootKeyboard.release(KEY_UP_ARROW);
      } else {
        if (pageScr && target == ASCII) {
          pageScr = false;
          BootKeyboard.release(KEY_PAGE_DOWN);
          // Restore CTRL if held down
          if (ctrlPressed)
            BootKeyboard.press(KEY_LEFT_CTRL);
        } else
          BootKeyboard.release(KEY_DOWN_ARROW);
      }
      break;

    case 17: // LSHIFT
      lShifted = false;
      BootKeyboard.release(KEY_LEFT_SHIFT);
      break;

    case 55: // :
      if (target == ASCII && shifted[key]) {
        shifted[key] = false;
        BootKeyboard.release('[');
      } else
        BootKeyboard.release(keymap[key]);
      break;

    case 56: // @
      if (target == ASCII && shifted[key]) {
        shifted[key] = false;
        BootKeyboard.release('{');
      } else
        BootKeyboard.release(keymap[key]);
      break;

    case 60: // £
      switch (target) {
        case VICE3:
          BootKeyboard.release(KEY_END);
          break;
        case VICE2_BMC64:
          BootKeyboard.release(KEY_INSERT);
          break;
        default:
          BootKeyboard.release(keymap[key]);
      }
      break;

    case 61: // *
      if (target == ASCII && shifted[key]) {
        shifted[key] = false;
        BootKeyboard.release('}');
      } else
        BootKeyboard.release(keymap[key]);
      break;

    case 62: // ;
      if (target == ASCII && shifted[key]) {
        shifted[key] = false;
        BootKeyboard.release(']');
      } else
        BootKeyboard.release(keymap[key]);
      break;

    case 63: // HOME
      if (clrPressed && target == ASCII) {
        clrPressed = false;
        BootKeyboard.release(KEY_END);
      } else
        BootKeyboard.release(KEY_HOME);
      break;

    case 64: // RSHIFT
      rShifted = false;
      BootKeyboard.release(KEY_RIGHT_SHIFT);
      break;

    case 65: // =
      switch (target) {
        case VICE3:
          BootKeyboard.release(KEY_PAGE_DOWN);
          break;
        case MISTER:
          BootKeyboard.release(KEY_F10);
          break;
        default:
          BootKeyboard.release(keymap[key]);
      }
      break;

    case 66: // ↑
      switch (target) {
        case MISTER:
          BootKeyboard.release(KEY_F9);
          break;
        case VICE2_BMC64:
          BootKeyboard.release(KEY_DELETE);
          break;
        default:
          BootKeyboard.release(keymap[key]);
      }
      break;

    case 68: // RESTORE
      switch (target) {
        case MISTER:
          BootKeyboard.release(KEY_F11);
          break;
        case ASCII:
          BootKeyboard.release(KEY_TAB);
          break;
        default:
          BootKeyboard.release(KEY_PAGE_UP);
      }
      break;

    case 72: // CTRL
      ctrlPressed = false;
        switch (target) {
          case VICE3:
          case VICE2_BMC64:
            BootKeyboard.release(KEY_TAB);
            break;
          default:
            BootKeyboard.release(KEY_LEFT_CTRL);
        }
      break;

    case 73: // 2
    if(target == ASCII && shifted[key]) {
        shifted[key] = false;
        BootKeyboard.release('"');
    } else
      BootKeyboard.release(keymap[key]);
    break;

    case 75: // C=
      cmdrPressed = false;
      if (misterOrASCII())
        BootKeyboard.release(KEY_LEFT_ALT);
      else
        BootKeyboard.release(KEY_LEFT_CTRL);
      break;

    case 77: // STOP
      BootKeyboard.release(KEY_ESC);
      break;

    default:
      BootKeyboard.release(keymap[key]);
  }

  // Restore SHIFTed keyboard state
  if (lShifted)
    BootKeyboard.press(KEY_LEFT_SHIFT);
  if (rShifted)
    BootKeyboard.press(KEY_RIGHT_SHIFT);
}

void loop() {
  thisKey   = NULL;
  isKeyDown = NULL;

  // Read each row...
  for (int row = 0; row < 8; row++) {
    int rowPin = rowPinMap[row];
    pinMode(rowPin, OUTPUT);
    digitalWrite(rowPin, LOW);
    // ...and column
    for (int col = 0; col < 9; col++) {
      thisKey   = col + (row * 10);
      isKeyDown = !digitalRead(colPinMap[col]);

      // Filter column 8, ignoring all except RESTORE because GND registers every row
      if (thisKey % 10 == 8 && thisKey != 68)
        break;

      // Non-blocking delay
      if (millis() < lastDebounceTime[thisKey] + debounceDelay)
        continue;

      // Is the key currently down and wasn't before?
      if (isKeyDown && !lastKeyState[thisKey]) {
        // Toggle the key state on
        lastKeyState[thisKey] = true;
        press(thisKey);
      }

      // The key is NOT down but WAS before
      if (!isKeyDown && lastKeyState[thisKey]) { 
        // Toggle the key state off
        lastKeyState[thisKey] = false;
        release(thisKey);
      }
    }

    lastDebounceTime[thisKey] = millis();
    digitalWrite(rowPin, HIGH);
    delay(1);
    pinMode(rowPin, INPUT_PULLUP);
  }
}
