import math, random, wave, array
R = 16000; NY = 7000   # keep every partial under 7 kHz (no aliasing hiss)
random.seed(11)
TAU = 2*math.pi
def mix(dst, src, at=0.0, gain=1.0):
    o = int(at*R)
    if len(dst) < o+len(src): dst.extend([0.0]*(o+len(src)-len(dst)))
    for i, v in enumerate(src): dst[o+i] += v*gain
    return dst
def note(m): return 440*2**((m-69)/12)
def adsr(n, a, d, s, r):
    A, D, Rr = max(1,int(a*R)), max(1,int(d*R)), max(1,int(r*R)); out=[]
    for i in range(n):
        if i < A: e = 0.5-0.5*math.cos(math.pi*i/A)
        elif i < A+D: e = 1-(1-s)*(i-A)/D
        else: e = s
        if i > n-Rr: e *= 0.5-0.5*math.cos(math.pi*(n-i)/Rr)
        out.append(e)
    return out
def biquad(x, kind, f0, q):
    w = TAU*f0/R; c, s = math.cos(w), math.sin(w); a = s/(2*q)
    if kind == 'lp': b0,b1,b2 = (1-c)/2, 1-c, (1-c)/2
    elif kind == 'hp': b0,b1,b2 = (1+c)/2, -(1+c), (1+c)/2
    else: b0,b1,b2 = a, 0, -a   # band-pass
    a0,a1,a2 = 1+a, -2*c, 1-a
    b0,b1,b2,a1,a2 = b0/a0,b1/a0,b2/a0,a1/a0,a2/a0
    y=[]; x1=x2=y1=y2=0.0
    for v in x:
        o = b0*v+b1*x1+b2*x2-a1*y1-a2*y2
        x2,x1,y2,y1 = x1,v,y1,o; y.append(o)
    return y
def tone(freqfn, dur, amps, envl=None):
    """additive voice: freqfn(t) -> Hz; amps(k, t) -> partial k amplitude; band-limited"""
    n=int(dur*R); out=[]; ph=0.0
    for i in range(n):
        t=i/R; f=freqfn(t); ph+=TAU*f/R
        v=0.0; k=1
        while k*f < NY and k <= 24:
            a=amps(k,t)
            if a: v+=a*math.sin(k*ph)
            k+=1
        out.append(v*(envl[i] if envl else 1))
    return out
def normalize(x, peak=0.92):
    m=max(abs(v) for v in x) or 1
    return [v*peak/m for v in x]
def fade_ends(x, a=0.004, r=0.03):
    A,Rr=int(a*R),int(r*R)
    for i in range(min(A,len(x))): x[i]*=i/A
    for i in range(min(Rr,len(x))): x[-1-i]*=i/Rr
    return x
def save(name, x):
    x=fade_ends(normalize(x))
    a=array.array('h',[int(max(-1,min(1,v))*32000) for v in x])
    with wave.open(name+'.wav','wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(R); w.writeframes(a.tobytes())
    print(name, round(len(x)/R,2),'s')

# organ: drawbar partials, slight vibrato, band-limited
ORG={1:1.0,2:0.7,3:0.45,4:0.35,6:0.2,8:0.15}
def organ(f, dur):
    n=int(dur*R); e=adsr(n,0.012,0,1,0.05)
    return tone(lambda t: f*(1+0.004*math.sin(TAU*5*t)), dur, lambda k,t: ORG.get(k,0)/2.6, e)
# brass: harmonics ~1/k, brightness opens on the attack, small pitch scoop, 3 detuned voices
def brass(f, dur, bright=1.0):
    n=int(dur*R); e=adsr(n,0.035,0.1,0.75,0.08); out=[0.0]*n
    for det in (-0.004,0,0.005):
        v=tone(lambda t: f*(1+det)*(1-0.03*math.exp(-t*40)), dur,
               lambda k,t: (1/k)*min(1,(0.25+t*14)*bright*6/(k+2)) , e)
        for i in range(n): out[i]+=v[i]/3
    return out
def noise(dur): return [random.uniform(-1,1) for _ in range(int(dur*R))]
def crowd(dur):
    n=int(dur*R); a=biquad(noise(dur),'bp',900,0.6); b=biquad(noise(dur),'bp',2200,0.8); out=[]
    for i in range(n):
        t=i/R; e=min(1,t/(dur*0.5))*min(1,(dur-t)/0.8)
        w=1+0.2*math.sin(TAU*2.7*t)+0.12*math.sin(TAU*6.1*t)
        out.append(e*w*(a[i]+0.5*b[i]))
    return out

# 1 Home run: organ Charge!
def charge(base=67):
    out=[]; t=0
    for semi,d in [(0,0.14),(5,0.14),(9,0.14),(12,0.36),(9,0.16),(12,0.9)]:
        mix(out, organ(note(base+semi), d*0.95), t); t+=d
    return out
save('v2_1_home_run_organ_charge', charge())

# 2 Grand slam: crowd builds, Charge twice, big organ chord
gs=[]; mix(gs, crowd(5.0), 0, 0.35)
mix(gs, charge(67), 0.3); mix(gs, charge(69), 2.2)
for m in (62,66,69,74,78): mix(gs, organ(note(m), 1.4), 4.0, 0.45)
save('v2_2_grand_slam', gs)

# 3 Touchdown: new fanfare - two rising calls then a held chord, with timpani
def timp(f, dur=0.6):
    return [math.sin(TAU*f*i/R*(1+0.1*math.exp(-i/R*20)))*math.exp(-i/R*6) + 0.3*random.uniform(-1,1)*math.exp(-i/R*60) for i in range(int(dur*R))]
td=[]; t=0
call=[(0,0.11),(0,0.11),(0,0.11),(4,0.5)]
for base in (60,62):
    for semi,d in call:
        m=base+semi; mix(td, brass(note(m), d*0.9), t); mix(td, brass(note(m-12), d*0.9), t, 0.5); t+=d
    mix(td, timp(note(base-24)), t-0.5, 0.7); t+=0.05
for m in (64,67,72,76): mix(td, brass(note(m), 1.3), t, 0.45)
mix(td, timp(note(36)), t, 0.9); mix(td, timp(note(43),0.4), t+0.25, 0.6)
save('v2_3_touchdown_fanfare', td)

# 5 Hockey goal horn (no crowd tail now)
def horn(dur):
    n=int(dur*R); e=adsr(n,0.08,0,1,0.35); out=[0.0]*n
    for f,a in ((174,1.0),(175.4,0.8),(220,0.5),(261.6,0.4)):
        v=tone(lambda t,f=f: f, dur, lambda k,t: (1/k) if k<=20 else 0, e)
        for i in range(n): out[i]+=a*v[i]
    return biquad(out,'lp',2500,0.7)
save('v2_5_hockey_goal_horn', horn(3.2))

# 6 Referee whistle: pea trill (fast warble) + breath, two blasts
def whistle(dur):
    n=int(dur*R); e=adsr(n,0.01,0,1,0.04); ph=0; out=[]
    br=biquad(noise(dur),'bp',3000,3)
    for i in range(n):
        t=i/R; tr=math.sin(TAU*34*t)
        f=2850+260*tr; ph+=TAU*f/R
        out.append(e[i]*((0.75+0.25*tr)*math.sin(ph)+0.25*br[i]))
    return out
wh=[]; mix(wh, whistle(0.22), 0); mix(wh, whistle(0.85), 0.36)
save('v2_6_referee_whistle', wh)

# 7 Buzzer: arena horn - harsh, loud, two clashing low tones
def buzz(dur):
    n=int(dur*R); e=adsr(n,0.01,0,1,0.06); out=[0.0]*n
    for f,a in ((233,1.0),(277,0.8),(116.5,0.6)):
        v=tone(lambda t,f=f: f, dur, lambda k,t: (1.0/k**0.6) if k%2 else 0.15/k, e)   # square-ish, nasal
        for i in range(n): out[i]+=a*v[i]
    return biquad(out,'bp',900,0.5)
save('v2_7_buzzer', buzz(1.5))

# 8 Swish: a quick "shhh-wip" of the net, then the crowd pops
def swish():
    d=0.32; n=int(d*R); x=noise(d); out=[]
    # band-pass sweeping up then down (two passes, blended)
    a=biquad(x,'bp',2600,1.2); b=biquad(x,'bp',4800,1.5)
    for i in range(n):
        t=i/n; env=math.sin(math.pi*t)**2*(1-0.4*t)
        out.append(env*((1-t)*a[i]+t*b[i]*1.3))
    return out
sw=[]; mix(sw, swish(), 0); mix(sw, crowd(1.6), 0.25, 0.4)
save('v2_8_swish_three_pointer', sw)

# 9 Sad trombone: deeper, with a "wah" mute on each note
def tbone(m, dur, last=False):
    n=int(dur*R); e=adsr(n,0.05,0.05,0.9,0.12); f0=note(m)
    def fr(t):
        f=f0*(1-0.02*math.exp(-t*20))
        if last: f*= (1-0.05*t/dur)*(1+0.03*math.sin(TAU*5.5*t))
        return f
    def amps(k,t):
        # "wah": brightness opens then closes each note (cup mute)
        wah = (math.sin(math.pi*min(1,t/(dur*0.8))) if not last else 0.5+0.5*math.sin(TAU*2.2*t))
        cut = 2+10*wah
        return (1/k)*math.exp(-max(0,k-cut)*0.7)
    return tone(fr, dur, amps, e)
st=[]; t=0
for m,d,l in ((43,0.5,False),(42,0.5,False),(41,0.5,False),(40,1.7,True)):
    mix(st, tbone(m,d,l), t); t+=d+0.06
save('v2_9_sad_trombone_they_scored', st)

# 10 Win song: new - drum roll, three rising chord stabs, big final chord + cymbal
def snare_roll(dur):
    n=int(dur*R); x=biquad(noise(dur),'bp',2500,0.7); out=[]
    for i in range(n):
        t=i/R; hits=0.55+0.45*abs(math.sin(math.pi*18*t)); out.append(x[i]*hits*(0.2+0.8*t/dur))
    return out
def cymbal(dur=1.6):
    x=biquad(noise(dur),'hp',4000,0.7); return [v*math.exp(-i/R*2.2) for i,v in enumerate(x)]
def chord(ms, dur, g=1.0):
    out=[]
    for m in ms: mix(out, brass(note(m), dur), 0, g/len(ms)*2)
    for m in ms: mix(out, organ(note(m-12), dur), 0, g/len(ms))
    return out
win=[]; mix(win, snare_roll(0.9), 0, 0.5)
mix(win, chord((65,69,72),0.22), 0.9); mix(win, chord((67,71,74),0.22), 1.15)
mix(win, chord((72,76,79,84),1.5), 1.45, 1.2); mix(win, cymbal(), 1.45, 0.35)
mix(win, timp(note(36)), 1.45, 0.8)
save('v2_10_win_song', win)
