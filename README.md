# Scoreboard

A 64x64 LED sports scoreboard: a Seengreat "RGB Matrix HUB75 S3" (ESP32-S3) board driving a P3 64x64 panel. It follows your teams on ESPN (NFL, college football, MLB, NHL, NBA), with live scores, a ticker, celebration animations and a thumbwheel for picking what's on screen.

Updates install over Wi-Fi: each board checks this repository's latest release (nightly, or "Check for updates now" at scoreboard.local) and installs the attached scoreboard-X.Y.bin.

- firmware/scoreboard: the board's firmware (Arduino, esp32 core 2.0.9)
- firmware/hosttest: tests and mock-up renderers that run on a computer
- preview: the original browser preview the firmware's look is matched against
- .github/workflows: builds the firmware and publishes a release whenever the version number changes on main

Companion project: [Mini-Scoreboard](https://github.com/jdebiase86/Mini-Scoreboard), a desk-sized mini scoreboard on a 4.0" ESP32 touch screen that can also act as a remote for this board. It has its own repo so its updates never mix with this board's releases.
