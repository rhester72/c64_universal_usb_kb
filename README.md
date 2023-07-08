Commodore 64 Universal USB Keyboard for Pro Micro by Rodney Hester

Allows use of the Commodore 64 keyboard over USB to multiple target platforms without modification

Inspired by code by DJ Sures (Synthiam.com) and Chris Garrett @ makerhacks
   
MANY thanks to David Hover/homerghost from the MiSTer FPGA Forum for his helpful testing, insightful suggestions and wireless Teslian wizardry!

Framework adapted from https://github.com/venice1200/c64-keyboard-USB and https://github.com/omiq/c64-keyboard-USB
   
Tested with [Anykey 64 by T'Pau](https://github.com/T-Pau/Anykey) and [Keyboard-Test by NTS](https://csdb.dk/release/?id=98411).

This approach could be adapted with required changes for operation on Teensy, Leonardo, and similar Arduino devices but is unsupported by the author.

Theory of operation
-------------------

The 8x8 Commodore 64 keyboard matrix is read serially, with RESTORE read on keyboard pin 3. An 8x10 matrix is used in practice to simplify row and column math.

Individual keys are mapped via lookup table to their emulated counterparts (and overridden as necessary for specific keys and target platforms).

All keyboard mappings assume US keyboards (both physical C64 keyboard and endpoint OS keyboard mappings).

Most keypresses are handled literally, with a keypress transmitted on detection (after debounce) and an unpress transmitted on release. This precisely matches the behavior of the original keyboard input, including "ghosting" issues common to keyboards without diodes.

VICE mapping is positional, which is NOT the software default.

Special handlers are provided for convenience menus and keymap cycling:

C=+F7        Opens convenience menu (on MiSTer and by default on BMC64), acts as (Left) Windows key in ASCII mode
C=+F1        Cycle keymaps forward, wrapping around at the end. Keymap sequence is defined below.
C=+SHIFT+F1  Cycle keymaps backward, wrapping around at the beginning.
C=+CTRL+F1   Reset keymap to 1 regardless of prior setting

Please note that ASCII mode requires the host operating system to support US keyboard mappings (only)!

VICE 2.x uses the same mappings as BMC64, which are different from those of VICE 3.x. The MiSTer core is natively supported.

VICE and MiSTer C64 mode on C128 is not well supported due to differences in mapping across the emulators.

If blink support has been enabled, either the internal RX/yellow LED or the external/keyboard LED (the latter being favored if available) will be used to communicate the current keymap setting via blinking one second after power-up and again upon changes. See CONNECTIONS below for details.

The currently selected map will be saved to EEPROM and restored each time the keyboard is plugged into USB until changed again. EEPROM will accept approximately 100,000 write cycles (or keyboard mapping changes) before failure.

Shifted selection is not possible in ASCII mode due to undefined intent when pressing SHIFT+CRSRDN or SHIFT+CRSRRT.

CAVEAT EMPTOR: The Arduino HID library takes (reasonable for its intent) liberties with SHIFT key handling. Keystrokes that would not ordinarily be shifted on a traditional PC keyboard are deliberately unshifted, and those requiring SHIFT are implicitly shifted by the library itself. This causes side-effects that must be handled via temporary manual handling of SHIFT as necessary [see unshift() and the end of release()].

Please note that certain declared values for key equivalents are unused in code due to the Arduino HID function requirement that any non-printable key be referenced explicitly by macro and cannot be passed by variable.

Code was deliberately created as large switch statements with predictive left member comparative operators to minimize cycle latency, achieving 400 matrix scans per second at 16Mhz (actual C64 hardware achieved 60 scans/second).

C64->ASCII mappings
-------------------

```
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
```

Connections (to Sparkfun Pro Micro or compatible)
-------------------------------------------------

```
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
```

Optionally, the Commodore 64 power LED may be connected to the Pro Micro for illumination when connected to USB. Connect the red wire from the LED in series with a 390 ohm (as standard, 180-470 ohms depending on desired brightness with lower values being brighter) resistor to Pro Micro pin 0 and the black wire to any available GND.

Commodore 64 keyboard matrix layout
-----------------------------------

```
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
```

   [1] As RESTORE is grounded and acts in original hardware as a switch, depressing it actually activates all rows for column 8 simultaneously. A software filter is used to reduce it to a single row.
