# Round 5: fight-song drafts for the start of your team's game. Original
# marches written here (not any team's real song), marching-band style:
# brass melody with a trombone part under it, tuba oom-pah, bass drum and
# timpani. No noise-based drums (Joe heard those as hiss).
from lib import *
def timp(f, dur=0.6):
    return [math.sin(TAU*f*i/R*(1+0.1*math.exp(-i/R*20)))*math.exp(-i/R*6) for i in range(int(dur*R))]
def kick(dur=0.25):   # bass drum: a low thump, pure tone
    return [math.sin(TAU*(55+60*math.exp(-i/R*30))*i/R)*math.exp(-i/R*14) for i in range(int(dur*R))]
def bell(f, dur=0.5):  # glockenspiel sparkle
    return [(math.sin(TAU*f*i/R)+0.3*math.sin(TAU*f*2.76*i/R))*math.exp(-i/R*7) for i in range(int(dur*R))]

def march(mel, chords, e, key=0, harm=-4, glock=False, end=None):
    """mel: (midi, eighths); chords: one (root midi) per 4 eighths; e: seconds per eighth"""
    out=[]; t=0
    for m,d in mel:
        if m:
            mix(out, brass(note(m+key), d*e*0.92), t)
            mix(out, brass(note(m+key+harm), d*e*0.92), t, 0.45)   # trombones a third or so under
            if glock: mix(out, bell(note(m+key+12), 0.4), t, 0.12)
        t+=d*e
    for i,root in enumerate(chords):   # tuba oom-pah + drum on the beats
        b=i*4*e
        mix(out, brass(note(root+key-24), e*0.8, 0.6), b, 0.55)
        mix(out, brass(note(root+key-17), e*0.8, 0.6), b+2*e, 0.45)
        mix(out, kick(), b, 0.5); mix(out, kick(), b+2*e, 0.35)
    if end:   # big held final chord with timpani
        for m in end: mix(out, brass(note(m+key), 1.4), t, 0.4)
        mix(out, brass(note(end[0]+key-24), 1.4, 0.6), t, 0.6)
        mix(out, timp(note(end[0]+key-24), 1.2), t, 0.8)
        mix(out, kick(0.4), t, 0.6)
    return out

# A: bright college march (C major, quick)
melA=[(67,1),(67,1),(72,2),(72,1),(71,1),(69,1),(67,1),(64,1),(67,1),(72,1),(76,1),(74,4),
      (74,1),(74,1),(77,2),(76,1),(74,1),(72,1),(71,1),(69,1),(71,1),(74,1),(71,1),(72,4)]
save('v5_1_fight_march', march(melA,[60,60,60,55,55,55],0.17,key=0,harm=-4,end=(72,76,79)))

# B: big rally - three rising calls answered by the band, then the hit
melB=[(67,1),(67,1),(67,1),(72,3),(0,2),(69,1),(69,1),(69,1),(74,3),(0,2),
      (71,1),(71,1),(71,1),(76,2),(74,1),(72,1),(71,1),(74,2),(79,4)]
save('v5_2_fight_rally', march(melB,[60,60,62,62,64,64,67],0.16,key=-2,harm=-5,end=(77,81,84)))

# C: proud fight song with glockenspiel on top (F major, a touch slower)
melC=[(60,2),(65,1),(65,1),(69,2),(65,2),(72,3),(70,1),(69,2),(67,2),
      (65,1),(67,1),(69,1),(70,1),(72,2),(74,2),(72,2),(67,2),(65,4)]
save('v5_3_fight_song_glock', march(melC,[53,53,53,48,53,48,53],0.19,key=0,harm=-3,glock=True,end=(65,69,72,77)))
