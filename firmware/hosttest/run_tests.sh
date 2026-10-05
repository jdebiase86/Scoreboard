#!/bin/bash
# Builds and runs every host test. Run from anywhere.
set -e
cd "$(dirname "$0")"
P=~/Arduino/libraries/PNGdec/src; J=~/Arduino/libraries/ArduinoJson/src; S=../scoreboard
F="-std=c++17 -O1 -Wno-format-truncation -D__LINUX__ -DPNG_MAX_BUFFERED_PIXELS=8194 -I $J -I $P"
python3 -c "s=open('../../preview/scoreboard_live.py').read();open('/tmp/page.js','w').write(s[s.index('<script>')+8:s.index('</script>')])"
node dump_js.js /tmp/js_frames.json
python3 make_feeds.py
g++ $F -o /tmp/test_render test_render.cpp $S/sb_render.cpp $S/sb_gfx.cpp $S/sb_logo.cpp && /tmp/test_render | tail -1
g++ $F -o /tmp/test_parse test_parse.cpp $S/sb_parse.cpp $S/sb_render.cpp $S/sb_gfx.cpp $S/sb_logo.cpp && /tmp/test_parse | grep -E "bad|DIFF"
g++ $F -o /tmp/test_enrich test_enrich.cpp $S/sb_parse.cpp $S/sb_gfx.cpp && /tmp/test_enrich
for c in adler32 crc32 infback inffast inflate inftrees zutil; do gcc -D__LINUX__ -DPNG_MAX_BUFFERED_PIXELS=8194 -O2 -I $P -c $P/$c.c -o /tmp/z_$c.o; done
g++ $F -o /tmp/test_foot test_foot.cpp $S/sb_parse.cpp $S/sb_render.cpp $S/sb_gfx.cpp $S/sb_logo.cpp && /tmp/test_foot | tail -2
g++ -std=c++17 -w -o /tmp/portal_host portal_host.cpp && /tmp/portal_host | head -3 | cut -c1-80
g++ $F -o /tmp/test_logo test_logo.cpp $S/sb_png.cpp $S/sb_logo.cpp $S/sb_render.cpp $S/sb_gfx.cpp $P/PNGdec.cpp /tmp/z_*.o && /tmp/test_logo | tail -2
node dump_fx.js /tmp/fx_frames.json
g++ $F -o /tmp/test_fx test_fx.cpp $S/sb_fx.cpp $S/sb_gfx.cpp $S/sb_logo.cpp && /tmp/test_fx | tail -2
g++ $F -o /tmp/test_events test_events.cpp $S/sb_events.cpp $S/sb_fx.cpp $S/sb_gfx.cpp $S/sb_logo.cpp && /tmp/test_events | tail -1
