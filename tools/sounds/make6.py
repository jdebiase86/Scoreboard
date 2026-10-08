# Round 6: fight song for football kickoff, from the band chart Joe sent
# (the Gators' "Orange and Blue", quarter = 158, 2/4, concert Bb). Trumpet 1
# carries the tune, trumpets 2 under it, tuba oom-pah, horn after-beats, and the
# drumline: bass drum on the beats, snare eighths with accents, light cymbal.
# Written trumpet pitches are a whole step above concert.
from lib import *
S16 = 60/158/4                      # one sixteenth note, seconds
def snare(dur=0.12):
    x = biquad(noise(dur), 'bp', 1900, 0.9)
    return [(x[i]*1.3 + 0.5*math.sin(TAU*190*i/R))*math.exp(-i/R*38) for i in range(len(x))]
def kick(dur=0.3):
    return [math.sin(TAU*(50+70*math.exp(-i/R*35))*i/R)*math.exp(-i/R*11) for i in range(int(dur*R))]
def cym(dur=0.25, decay=16):
    x = biquad(noise(dur), 'hp', 5000, 0.7); return [v*math.exp(-i/R*decay) for i,v in enumerate(x)]
def timp(f, dur=1.2):
    return [math.sin(TAU*f*i/R*(1+0.08*math.exp(-i/R*20)))*math.exp(-i/R*4) for i in range(int(dur*R))]
CH = {'Bb':(58,62,65),'Eb':(55,58,63),'F':(57,60,63,65),'G':(55,59,62,65),'C':(55,58,60,64)}
ROOT = {'Bb':34,'Eb':39,'F':41,'G':43,'C':36}
def line(out, notes, t0, gain, conc=-2, bright=1.0):
    t = t0
    for m,d in notes:
        if m: mix(out, brass(note(m+conc), d*S16*0.93, bright), t, gain)
        t += d*S16
    return t
def band(out, chords, t0, drums=True):
    for i,c in enumerate(chords):
        b = t0 + i*8*S16
        if c:
            mix(out, brass(note(ROOT[c]), 3*S16, 0.7), b, 0.6)          # tuba oom
            mix(out, brass(note(ROOT[c]+7), 3*S16, 0.7), b+4*S16, 0.5)  # ... pah
            for m in CH[c]:                                              # horns on the after-beats
                for k in (2, 6): mix(out, brass(note(m), 1.6*S16, 0.8), b+k*S16, 0.12)
        if drums:
            for k in (0, 4): mix(out, kick(), b+k*S16, 0.55); mix(out, cym(), b+k*S16, 0.05)
            for k in range(0, 8, 2): mix(out, snare(), b+k*S16, 0.22 if k % 4 else 0.32)
def intro(out):
    # the chart's drum lead-in (bars 5-6): snare triplets over two big bass hits, then two accents
    b = 0
    for k in range(6): mix(out, snare(), b + k*(8*S16/6), 0.18 + 0.03*k)
    for k in (0, 4): mix(out, kick(), b+k*S16, 0.6); mix(out, cym(0.4, 9), b+k*S16, 0.08)
    b = 8*S16
    mix(out, snare(), b, 0.4); mix(out, snare(), b+2*S16, 0.4)
    mix(out, kick(), b, 0.6); mix(out, kick(), b+2*S16, 0.6); mix(out, cym(0.4, 9), b+4*S16, 0.1)
    return 16*S16
def finish(out, t):
    for m in (58,62,65,70,74): mix(out, brass(note(m), 1.5), t, 0.3)
    mix(out, brass(note(34), 1.5, 0.7), t, 0.6)
    mix(out, timp(note(34)), t, 0.8); mix(out, kick(0.5), t, 0.7); mix(out, cym(1.6, 2.5), t, 0.18)

# bars 7-13 (written trumpet pitches, sixteenths)
T1a = [(67,8),(66,4),(67,4),(76,6),(75,2),(76,8),(72,8),(74,4),(72,4),(69,4),(67,4)]
T2a = [(64,8),(63,4),(64,4),(72,6),(72,2),(72,8),(69,8),(69,4),(69,4),(63,4),(64,4)]
CHa = ['Bb','Bb','Bb','Bb','Eb','Eb','Bb']
# bars 14-21, then the ending (bars 34-37)
T1b = [(0,2),(71,2),(69,2),(68,2),(67,8),(66,4),(67,4),(76,6),(75,2),(76,8),(74,8),(69,4),(74,4),(76,4),(74,4),
       (68,2),(69,2),(71,2),(72,2),(76,8),(74,8)]
T2b = [(0,2),(71,2),(69,2),(68,2),(64,8),(63,4),(64,4),(72,6),(72,2),(73,8),(72,8),(66,4),(72,4),(71,4),(71,4),
       (68,2),(69,2),(71,2),(72,2),(72,8),(71,8)]
CHb = ['F','Bb','Bb','Bb','G','C','C','F','F','C','F']

# 1: short - drum lead-in, the first line, big final chord
o=[]; t=intro(o); band(o, CHa, t); line(o, T1a, t, 0.75); end=line(o, T2a, t, 0.4); finish(o, end)
save('v6_1_orange_blue_short', o)
# 2: long - drum lead-in, the whole first half to the ending
o=[]; t=intro(o); band(o, CHa+CHb, t)
line(o, T1a+T1b, t, 0.75); end=line(o, T2a+T2b, t, 0.4); finish(o, end)
save('v6_2_orange_blue_long', o)
# 3: short, without the drumline (horns only), to compare
o=[]; t=8*S16; band(o, CHa, t, drums=False); line(o, T1a, t, 0.75); end=line(o, T2a, t, 0.4)
for m in (58,62,65,70,74): mix(o, brass(note(m), 1.5), end, 0.3)
mix(o, brass(note(34), 1.5, 0.7), end, 0.6)
save('v6_3_orange_blue_horns_only', o)
