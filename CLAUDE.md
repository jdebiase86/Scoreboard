# Scoreboard - notes for Claude

## Working with Joe
- Reply in plain text only: no markdown, bullets, bold or code blocks. Joe often reads on his phone and copies text into iMessage. Numbered lines like "1." are fine.
- Talk a design through before coding. For anything visual, render a mock-up picture first (firmware/hosttest mock_*.cpp programs write .ppm frames; scale up 6x with PIL and lay out side by side) and wait for Joe to pick. Don't code until he says go.
- Joe isn't a programmer. Explain in plain words, keep steps short, and say exactly what to tap.
- Never ask for or accept Joe's GitHub (or any) password.
- A second board is a gift for a family member who isn't technical. Setup must stay dead simple; a plain-language setup PDF comes last, after the code is settled.

## Privacy (this repo is public)
- No personal details in anything committed or published: code, comments, test data, mock-ups, README, CLAUDE.md, commit messages, release notes or build files. That means no names of Joe's family ("Joe" alone is fine), no Wi-Fi names or passwords, no home address or home network addresses, no emails, keys or tokens, no spreadsheet or document links, no ages or health details. Use made-up placeholders ("Home Wi-Fi", "Swimmer 1").
- Personal settings (Wi-Fi, teams, time zone) are entered on the board's setup page and stay on the board.
- Before each release: search the changed files and commit messages for the above. If anything personal is found, including in old commits, show Joe what and where before changing anything; never quietly fix the history.
- Other projects (swim PR board, Mini-Scoreboard) get their own repos; don't add their notes or personal data here.

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

If the cloud session can't download the esp32 core, rely on the GitHub Action build and check its log. It builds (without publishing) on every pull request that touches firmware/scoreboard, and builds and publishes on every push to main that does.

## Releasing
- Version lives in firmware/scoreboard/sb_version.h (FW_VERSION "X.Y"). Bump it for every update Joe should get; never reuse a version.
- When a change with a new version lands on main, .github/workflows/release.yml builds it and publishes release vX.Y with scoreboard-X.Y.bin attached. Boards install the latest release overnight, or when Joe taps "Check for updates now" at scoreboard.local.
- Work on a branch, then tell Joe in plain words what changed and that approving (merging) it will publish the update.

## Tests (firmware/hosttest)
- run_tests.sh builds and runs the host tests: test_render (pixel parity with preview/scoreboard_live.py), test_parse, test_enrich, test_foot, test_logo, test_fx (slow, ~10 min), test_events. Paths to PNGdec/ArduinoJson point at ~/Arduino/libraries.
- Whenever render rules change, update preview/scoreboard_live.py to match so test_render stays at 0 differ.
- Logo tests and mock-ups read real ESPN logos from a local logo folder (downloaded on Joe's Mac by get_logos.py); they skip or fall back to letters when the files aren't there.

## Wheel and modes (1.7)
- Wheel stops: AUTO, a team mode per favourite (saved as settings.pin, survives restarts), ALL NFL, ALL COLLEGE (ranked + all SEC), then other live games (temporary). Short push = big / full-game screen (no ticker). Hold 3 s = AUTO with ticker.
- AUTO: live favourites take turns (football first); none live = cycle all favourites every rotation interval.
- AUTO when none of your teams is live takes turns between all of them (their final if they played today, else their next game), every rotation interval; the full-recheck path rotates the same way. When your one live game ends, its final shows for a turn and the rotation resumes (1.9). A game still waiting to start counts as "near" (checked every minute) for up to 3 hours past its listed time.

## Logos (1.9)
- Joe picked, per team, how each logo looks at small sizes (<40 px: kickoff 26x24, ticker 24, wheel cards, full-game screen): firmware/scoreboard/sb_logofix.h. FIX_LIGHT = ESPN's regular team-colour logo instead of the dark-background one; FIX_KEYLINE = a thin light outline drawn as one clean dot-wide edge (shrinkLogo keyline); FIX_CROP_TOP = drop the top 40% (76ers' stars); FIX_LETTERS = team letters (only Mississippi State now). Celebration logos (54 px) ignore the picks. The same table drives the mock-ups (hosttest/logo_dir.h loadTeamLogo).
- To revisit a logo: draw contact sheets (1 today, 2 keyline, 3 light) and let Joe pick, then edit sb_logofix.h.

## Full ticker (1.9)
ALL NFL / ALL COLLEGE: two games a page, 24-dot logos along the top, each score small under its logo, clock / FINAL / start time bottom middle with the quarter or date just above in dim grey, the ball between the logos on the side with possession; red zone turns the quarter and clock red. Scores everywhere use the plain font 1 (the 1.7 chunky 1 is gone, preview matches).

## Football full-game screen (1.8, Option B)
renderFootballFull in sb_render.cpp: both 26x24 logos side by side (away x=0, home x=38, y=8), scores under them, the ball between them for possession, timeout dots under the scores. Field: solid grass, faint midfield line, end zones in team colours (away left, home right), small football on its spot, yellow line to gain, red tint only on the 20 yards in front of the goal being attacked. Win bar: away share from the left; away goes white if the colours look alike. The net task now publishes the 26x24 logos for live football games too (the old 16-px live logos are gone). Mock-up: firmware/hosttest/mock_full2.cpp.

## Mini scoreboard
Separate repo jdebiase86/Mini-Scoreboard (design settled, no firmware yet). Never publish its releases here: boards install this repo's latest release.

## Later
Full-game screens for baseball, hockey and basketball (need live data captures), night mode, all-teams countdown screen, QR code on the setup screen, second (gift) board setup, then the setup PDF. Racing (NASCAR/F1) much later.
