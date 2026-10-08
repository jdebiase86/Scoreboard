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
- SD: CS39 MOSI40 CLK41 MISO42. Audio: MCLK38 SCLK48 LRCK21 DSDIN14 SDOUT47, NS4150 amp enable IO3. sb_audio.cpp sets up the ES8311 (I2C 0x18, slave, 16 kHz, MCLK 256 fs); Joe says the little speaker sounds great.

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

## Wheel and modes (1.11)
- Wheel stops: AUTO, a team mode per favourite (saved as settings.pin, survives restarts), ALL NFL, ALL COLLEGE (ranked + all SEC), then other live games (temporary).
- AUTO shows the full screens (no ticker). A team mode shows the normal screen with that team's league in the ticker (straight from the feed its game came from; college merges the ranked FBS feed). A short push swaps to the other view (bigMode, reset on every stop change). Hold 3 s = sound off / on (settings.sound, also the Sound button at the top of scoreboard.local).
- AUTO: none of your teams live = take turns between all of them (their final if they played today, else their next game) every rotation interval; exactly one live = stay on it; two or more live (any sport) = take turns between the live ones, football first in line. When your one live game ends, its final shows for a turn and the rotation resumes. A game still waiting to start counts as "near" (checked every minute) for up to 3 hours past its listed time.
- Picking a stop: the card stays up with LOADING until that team's game is published (no flash of the previous game).

## Full screens (1.11, Mini-Scoreboard style)
renderFull in sb_render.cpp (mock-ups: firmware/hosttest/mock_full5.cpp, checked with mock_real.cpp): plain period + clock on top (no LIVE tag), both 26x24 logos (away x=0, home x=38) with the scores under them. Football: possession ball between the logos, timeout dashes, thin 5-row field (end zones in team colours, red zone tinted and outlined), win bar, ball spot ("AT PHI 34"). Baseball: team-colour edge stripes, logos at x=2/36, a field strip with the big diamond, B/S dots left, outs right. Hockey: records, a period tracker and "2ND PERIOD"; our power play = a banner in our colour with the half-size logo, time left and a draining gold bar; theirs = PENALTY KILL on caution tape. Basketball: BONUS (or record) under each score, quarter tracker, "3RD QUARTER". Final: grey FINAL tag, loser's score dimmed, green WIN under your team, "NEXT TUE 7:05P" box (sb_net fillNext, looked up 8 days ahead, kept 3 h). Upcoming (1.12, Joe's pick A from mock_pre2.cpp, after trying B): the day spelled out in F5 gold with a dark orange shadow and a thin grey line under it (Joe's style 5 of 6 tried; TODAY, TOMORROW, SATURDAY, WEDNESDAY; the real weekday from the game's date in the board's time zone, a date past a week), logos with AT and records, then the big start time with the small grey date beside it. With no playoff series line everything sits 2-7 rows lower to fill the space; in the playoffs the series shows in gold at the bottom. Joe likes the final screen as it is. Close game (closeGame in sb_events: last 2 min, within 8 / 3 / 1) turns the clock gold and shows CLOSE GAME. Playoffs: thin gold frame. Your team's new score flashes in its colour, then stays gold for a minute. The normal screen's small diamond is a green outline (Joe's pick B; preview matches).

## Sounds (1.11, 1.12)
Joe's picks are made by tools/sounds (make*.py drafts, build_sounds.py writes firmware/scoreboard/sb_sounds.h + sb_sound_data.h as 8-bit mu-law, 16 kHz). touchdown fanfare, field goal, whistle (kickoff, flag), Gators fight song at a Florida kickoff instead of the whistle (1.12: SND_GATORS, Orange and Blue transcribed from a band chart Joe sent, make6.py v6_3 horns only; fxSound checks the pinned abbr FLA; Joe wants it only for the Gators), goal horn, buzzer (end of quarter / period / intermission), swish + ding (3-pointer), organ Charge (home run), Charge twice + chord (grand slam), sad trombone (they scored; never basketball), organ chord win song, game-start organ (non-football start), heartbeat (close game, then every minute). Animations play theirs as they start (fxSound); the sound-only ones come from sb_net checkEvents (soundEvent, closeGame). Your teams only. No crowd sounds: synthesised crowds sounded like a noise machine. scoreboard.local has a button per sound.

## Logos (1.9)
- Joe picked, per team, how each logo looks at small sizes (<40 px: kickoff 26x24, ticker 24, wheel cards, full-game screen): firmware/scoreboard/sb_logofix.h. FIX_LIGHT = ESPN's regular team-colour logo instead of the dark-background one; FIX_KEYLINE = a thin light outline drawn as one clean dot-wide edge (shrinkLogo keyline); FIX_CROP_TOP = drop the top 40% (76ers' stars); FIX_LETTERS = team letters (only Mississippi State now). Celebration logos (54 px) ignore the picks. The same table drives the mock-ups (hosttest/logo_dir.h loadTeamLogo).
- To revisit a logo: draw contact sheets (1 today, 2 keyline, 3 light) and let Joe pick, then edit sb_logofix.h.

## Full ticker (1.9)
ALL NFL / ALL COLLEGE: two games a page, 24-dot logos along the top, each score small under its logo, clock / FINAL / start time bottom middle with the quarter or date just above in dim grey, the ball between the logos on the side with possession; red zone turns the quarter and clock red. Scores everywhere use the plain font 1 (the 1.7 chunky 1 is gone, preview matches).

## Logos on the board
The net task publishes the two 26x24 logos for every game (the full screens use them).

## Mini scoreboard
Separate repo jdebiase86/Mini-Scoreboard (design settled, no firmware yet). Never publish its releases here: boards install this repo's latest release.

## Case
enclosure/case.scad (OpenSCAD, part = case / test_screen / test_board): 2 inch deep box, the screen is the front. Screen screws M3 x 30 into its corner inserts at 10 mm from the sides and 20.2 mm from top/bottom (Joe measured). Board bottom-left (seen from the front) on M2 x 4 heat-set insert posts (2.9 mm holes), its edge in a 1 mm wall pocket; wheel/USB openings fitted on test prints (4.4 mm tall, 48 mm long, 8 mm bar between wheel and USB-C). Test pieces were printed and fit.

## Later
- Joe (after 1.11): the basketball close-game heartbeat is wrong - within 3 points with 2 minutes left is nothing in basketball. Joe's idea for the fix: in basketball the heartbeat beats more often when the score swings back and forth quickly in the last minute or so (a tight, lead-changing finish), instead of just any game within 3. Talk it through with Joe before coding; leave 1.11 as is.
- Joe likes a heartbeat that speeds up as the tension rises, for other sports too (ideas to talk through: football speeding up as the clock runs down with the game within one score, hockey in the last minute of a one-goal game, baseball from the 9th with the tying or winning run on base or at the plate). Mock up / talk it through first.
- Check the new full screens against live games (no live captures for baseball, hockey and basketball yet), night mode, all-teams countdown screen, QR code on the setup screen, second (gift) board setup, then the setup PDF. Racing (NASCAR/F1) much later.
