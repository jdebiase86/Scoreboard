// Types used by scoreboard.ino. They live in a header because the Arduino
// build writes prototypes for the .ino's functions near the top of the file,
// before anything the .ino itself defines.
#pragma once
#include <stdint.h>

enum Mode { M_SETUP, M_CONNECTING, M_FALLBACK, M_CONNECTED_MSG, M_RUNNING };

// Score glow memory for one game (see scoreboard.ino)
struct ScoreMemo { char eventId[16] = ""; bool pinnedHome = true; int mine = -1, other = -1;
                   uint32_t glowAt = 0, flashAt = 0, seenAt = 0; };
