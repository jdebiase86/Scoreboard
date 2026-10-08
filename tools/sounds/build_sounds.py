"""Builds firmware/scoreboard/sb_sounds.h (names) and sb_sound_data.h (the
sounds) from Joe's picked sounds.

Runs the draft scripts (make*.py) in a scratch folder, takes the picked WAVs,
squeezes each to 8-bit mu-law (16 kHz mono) and writes them as C arrays.
Run from anywhere:  python3 tools/sounds/build_sounds.py
"""
import os, sys, subprocess, tempfile, wave, array, shutil

HERE = os.path.dirname(os.path.abspath(__file__))
SB = os.path.join(HERE, '..', '..', 'firmware', 'scoreboard')
OUT_IDS = os.path.join(SB, 'sb_sounds.h')        # the sound names (included anywhere)
OUT = os.path.join(SB, 'sb_sound_data.h')        # the sounds themselves (sb_audio.cpp only)
# id name -> (script, wav file)
PICKS = [
    ('SND_TOUCHDOWN', 'make2.py', 'v2_3_touchdown_fanfare.wav'),
    ('SND_FIELDGOAL', 'make.py', '4_field_goal_kick_and_ding.wav'),
    ('SND_WHISTLE', 'make2.py', 'v2_6_referee_whistle.wav'),
    ('SND_GOALHORN', 'make2.py', 'v2_5_hockey_goal_horn.wav'),
    ('SND_BUZZER', 'make4.py', 'v4_7b_buzzer_gym_electric.wav'),
    ('SND_SWISH', 'make3.py', 'v3_8b_swish_double_with_ding.wav'),
    ('SND_HOMERUN', 'make3.py', 'v3_1_home_run_charge_lower.wav'),
    ('SND_GRANDSLAM', 'make4.py', 'v4_2_grand_slam_organ_only.wav'),
    ('SND_THEYSCORED', 'make2.py', 'v2_9_sad_trombone_they_scored.wav'),
    ('SND_WIN', 'make3.py', 'v3_10b_win_song_organ_chords.wav'),
    ('SND_GAMESTART', 'make.py', '11_game_starting_organ.wav'),
    ('SND_HEARTBEAT', 'make.py', '12_close_game_heartbeat.wav'),
    ('SND_GATORS', 'make6.py', 'v6_3_orange_blue_horns_only.wav'),
]

def mulaw(samples):
    out = bytearray()
    for s in samples:
        sign = 0x80 if s < 0 else 0
        m = min(abs(s), 32635) + 132
        exp = 7
        while exp > 0 and not (m & (0x4000 >> (7 - exp))): exp -= 1
        mant = (m >> (exp + 3)) & 0x0F
        out.append(~(sign | (exp << 4) | mant) & 0xFF)
    return out

def main():
    tmp = tempfile.mkdtemp()
    try:
        for f in os.listdir(HERE):
            if f.endswith('.py') and f != os.path.basename(__file__): shutil.copy(os.path.join(HERE, f), tmp)
        for script in sorted({p[1] for p in PICKS}):
            subprocess.run([sys.executable, script], cwd=tmp, check=True, stdout=subprocess.DEVNULL)
        lines = ['// Made by tools/sounds/build_sounds.py - do not edit by hand.',
                 '// Joe\'s picked sounds, 16 kHz mono, 8-bit mu-law (G.711).',
                 '#pragma once', '#include <stdint.h>', '']
        names, total = [], 0
        for sid, script, wav in PICKS:
            w = wave.open(os.path.join(tmp, wav))
            assert w.getframerate() == 16000 and w.getnchannels() == 1 and w.getsampwidth() == 2
            a = array.array('h', w.readframes(w.getnframes()))
            data = mulaw(list(a))
            total += len(data)
            arr = sid.lower()
            lines.append('static const uint8_t %s[%d] = {' % (arr, len(data)))
            for i in range(0, len(data), 24):
                lines.append('  ' + ','.join('%d' % b for b in data[i:i + 24]) + ',')
            lines.append('};')
            names.append((sid, arr, len(a)))
        lines.append('')
        ids = ['// Made by tools/sounds/build_sounds.py - do not edit by hand.', '#pragma once', '#include <stdint.h>', '',
               'enum SoundId : uint8_t { SND_NONE = 0, ' + ', '.join(s for s, _, _ in names) + ', SND_COUNT };']
        open(OUT_IDS, 'w').write('\n'.join(ids) + '\n')
        lines.insert(3, '#include "sb_sounds.h"')
        lines.append('struct SoundClip { const uint8_t* data; uint32_t samples; };')
        lines.append('static const SoundClip SOUNDS[SND_COUNT] = {{nullptr, 0}, ' +
                     ', '.join('{%s, %d}' % (arr, n) for _, arr, n in names) + '};')
        open(OUT, 'w').write('\n'.join(lines) + '\n')
        print('wrote %s: %d sounds, %d KB' % (os.path.normpath(OUT), len(names), total // 1024))
    finally:
        shutil.rmtree(tmp)

main()
