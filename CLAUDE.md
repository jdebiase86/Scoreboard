# Scoreboard - notes for Claude

## Working with Joe
- Reply in plain text only: no markdown, bullets, bold or code blocks. Joe often reads on his phone and copies text into iMessage. Numbered lines like "1." are fine.
- Talk a design through before coding. For anything visual, render a mock-up picture first (firmware/hosttest mock_*.cpp programs write .ppm frames; scale up 6x with PIL and lay out side by side) and wait for Joe to pick. Don't code until he says go.
- Joe isn't a programmer. Explain in plain words, keep steps short, and say exactly what to tap.
- Never ask for or accept Joe's GitHub (or any) password.
- A second board is for Joe's father-in-law (older, Windows PC, Central time). Setup must stay dead simple; a plain-language setup PDF comes last, after the code is settled.

## Hardware
- Seengreat RGB Matrix HUB75 S3 (ESP32-S3, 16 MB flash, OPI PSRAM) driving a P3 64x64 HUB75 panel. Powered from the USB-C port that is NOT labeled power.
- HUB75: R1=5 G1=4 B1=6 R2=15 G2=7 B2=17 A=8 B=18 C=10 D=9 E=16 CLK=12 LAT=11 OE=13.
- Thumbwheel on a PCA9557 I2C expander, SDA=IO1, SCL=IO2. UP=K1 (bit 1), DOWN=K3 (bit 2), PUSH=K2 (bit 3). At boot: unjam the bus (9 clocks + STOP), then take the first address in 0x19-0x1F, 0x18, 0x20-0x27, 0x38-0x3F whose config register 0x03 reads 0xFF (the ES8311 codec sits at 0x18).
- SD: CS39 MOSI40 CLK41 MISO42. Audio: MCLK38 SCLK48 LRCK21 DSDIN14 SDOUT47, NS4150 amp enable IO3 (too quiet to be useful, unused).

## Firmware layout (firmware/scoreboard)
- scoreboard.ino: loop on core 1 - modes, wheel, popups, animations, drawing.
- sb_net.cpp: network task on core 0 - ESPN fetches, Auto/team-mode/wheel logic, logo cache (80 slots), full ticker, OTA check. Shared data goes through the `lock` mutex and snapshot copies (netSnapshot, netCard, netFullTicker, fxTake); never hand the loop a pointer into net-task memory.
- sb_parse.cpp: ESPN JSON (ArduinoJson with filters). sb_render.cpp: all screens. sb_fx.cpp: animations. sb_events.cpp: what just happened (touchdown, goal...). sb_logo.cpp: logo shrinking (v7 for <40 px; snap for 54 px celebration logos). sb_portal.cpp: setup page / scoreboard.local. sb_settings.cpp: saved settings (Preferences).
- Big structs live in PSRAM via sbAlloc; keep big temporaries off the stack.
- ESPN football situation: yardLine 0 = home goal line, 100 = away goal line.

## Build
arduino-cli, esp32 core 2.0.9, libraries ArduinoJson 7.4.2, PNGdec 1.1.7, ESP32 HUB75 LED MATRIX PANEL DMA Display 3.0.15, Adafruit GFX 1.12.6, Adafruit BusIO 1.17.4.

    arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB" --build-property "compiler.cpp.extra_flags=-std=gnu++17 -DPNG_MAX_BUFFERED_PIXELS=8194" --output-dir build firmware/scoreboard

The update file is build/scoreboard.ino.bin renamed scoreboard-X.Y.bin (first byte 0xE9).

If the cloud session can't download the esp32 core, rely on the GitHub Action build (it runs on every push to main that touches firmware/scoreboard) and check its log.

## Releasing
- Version lives in firmware/scoreboard/sb_version.h (FW_VERSION "X.Y"). Bump it for every update Joe should get; never reuse a version.
- When a change with a new version lands on main, .github/workflows/release.yml builds it and publishes release vX.Y with scoreboard-X.Y.bin attached. Boards install the latest release overnight, or when Joe taps "Check for updates now" at scoreboard.local.
- Work on a branch, then tell Joe in plain words what changed and that approving (merging) it will publish the update.

## Tests (firmware/hosttest)
- run_tests.sh builds and runs the host tests: test_render (pixel parity with preview/scoreboard_live.py), test_parse, test_enrich, test_foot, test_logo, test_fx (slow, ~10 min), test_events. Paths to PNGdec/ArduinoJson point at ~/Arduino/libraries.
- Whenever render rules change, update preview/scoreboard_live.py to match so test_render stays at 0 differ.
- Logo tests and mock-ups read real ESPN logos from a local logo folder (downloaded on Joe's Mac by get_logos.py); they skip or fall back to letters when the files aren't there.

## Wheel and modes (1.7)
- Wheel stops: AUTO, a team mode per favourite (saved as settings.pin, survives restarts), ALL NFL, ALL COLLEGE (ranked + all SEC), then other live games (temporary). Short push = big / full-game screen. Hold 3 s = AUTO with ticker.
- AUTO: live favourites take turns (football first); none live = cycle all favourites every rotation interval.
- Unreadable logos show the team letters in team colour (lettersOnly list in sb_net.cpp).

## Next up (approved by Joe 2026-10-04)
Football full-game screen, Option B: both logos side by side at full size (26x24 boxes at x=0 and x=38, y=8), each score under its logo, the ball between the logos for possession, timeout dots under each score. Cleaned-up field: solid grass, faint midfield line, end zones in full team colours with AWAY on the left and HOME on the right (matching the logos), a small football on its spot (no white arrow), yellow line to gain, red tint only on the 20 yards in front of the goal being attacked. Win bar: away share from the left; if the two team colours look alike, the away side is white. Prototype code: work/option_b_sb_render.cpp (style 1). Needs 26x24 logos published for live games (today only 16-px logos are published once a game starts).

## Later
Full-game screens for baseball, hockey and basketball (need live data captures), night mode, all-teams countdown screen, QR code on the setup screen, father-in-law board setup (his teams Cowboys and LSU, Central time), then the setup PDF. Racing (NASCAR/F1) much later.
