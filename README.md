# wifi_rfid

So far we:

1. Set up a Google Sheet + Apps Script that returns our member data as JSON,
   both for single lookups and as a full bulk export (used to build a local
   cache on the ESP32 — see step 4).
   https://docs.google.com/spreadsheets/d/1XQJovlWFOaad1_bkNsqT0VvluUiznTj9TqH80cHhJdY/edit?gid=0#gid=0

2. Wired the ESP32 to the RFID reader.

**Wiring (ESP32 <-> RC522, SPI)**

| RC522 Pin | ESP32 Pin |
|---|---|
| SDA (SS) | GPIO 5 |
| SCK | GPIO 18 |
| MOSI | GPIO 23 |
| MISO | GPIO 19 |
| RST | GPIO 22 |
| 3.3V | 3.3V |
| GND | GND |

3. Wired the 16x2 LCD (1602A, bare 16-pin parallel version — no I2C
   backpack). Running in 4-bit mode.

**Wiring (ESP32 <-> LCD)**

| LCD Pin | Function | ESP32 / Connect to |
|---|---|---|
| 1 | VSS | GND |
| 2 | VDD | 5V |
| 3 | V0 (contrast) | GND (straight to ground, no pot — works fine at 5V) |
| 4 | RS | GPIO 13 |
| 5 | RW | GND (write-only) |
| 6 | E | GPIO 14 |
| 7–10 | D0–D3 | not connected (4-bit mode) |
| 11 | D4 | GPIO 27 |
| 12 | D5 | GPIO 26 |
| 13 | D6 | GPIO 25 |
| 14 | D7 | GPIO 33 |
| 15 | A (backlight +) | 5V, through a resistor (check the board — many already have one built in) |
| 16 | K (backlight -) | GND |

4. Wired 2 push buttons for page navigation (NEXT / PREV), each between its
   GPIO and GND, using the ESP32's internal pull-up — no external resistor
   needed.

**Wiring (buttons)**

| Button | ESP32 Pin |
|---|---|
| NEXT | GPIO 16 |
| PREV | GPIO 17 |

5. Flashed firmware that:
   - Connects to WiFi (auto-reconnects if dropped)
   - Reads NFC tag UIDs
   - Fetches the **full member list** from Apps Script on boot and every
     5 minutes in the background, caching it locally on the ESP32
   - Looks up scanned tags **against the local cache** (instant, no HTTP
     wait per scan — we moved off per-tag fetching because it was slow
     and unreliable)
   - Shows the result on the LCD across 4 pages (UID / Member profile /
     Heavy machinery / 3D printing), navigable with the NEXT/PREV buttons
   - Resets to an idle "Tap Tag to Scan" screen after 15s of inactivity

**Current status: working end-to-end on the bench.** Tap a registered tag
→ permissions show on the LCD, navigable with the buttons, auto-resets
when idle. Unregistered tags show "Not registered."

---

## Next steps

### Apps Script / Sheet (Graham)
1. Replace the google script app from one setup for the Fablab´s google document matching the columns:
UID | Name | Onboarding | Laser | Cnc | Bambu`
And both the spreadsheet and the relevant TAB must be named Members with the current implementation.

2. Decide and build the **first-scan name association** flow for
   unregistered tags (see challenge options: auto-queue to an
   "Unassigned Tags" tab + Form, local config webserver, or a
   placeholder row an admin fills in later).

### Code
1. ~~Update the `.ino` to the new Apps Script (bulk fetch + local
   cache).~~ ✅ done — 
2. ~~Add a UI that talks to the buttons + screen.~~ ✅ done
3. Implement whichever first-scan association.

### Hardware
1. ~~Install screen.~~ ✅ done — 1602A, parallel/4-bit wiring.
2. ~~Install buttons.~~ ✅ done — NEXT/PREV on GPIO16/17.

### 3D
1. Case — wall-mountable, keyhole slots or mounting tabs, cutouts for
   LCD + buttons + NFC tap target.
2. Design for NFC tag placement/mounting.
