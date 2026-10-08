import math, random, wave, array
R = 16000
random.seed(7)
def silence(sec): return [0.0]*int(R*sec)
def mix(dst, src, at=0.0, gain=1.0):
    o = int(at*R)
    if len(dst) < o+len(src): dst.extend([0.0]*(o+len(src)-len(dst)))
    for i, v in enumerate(src): dst[o+i] += v*gain
    return dst
def env(n, a=0.01, d=0.0, s=1.0, r=0.05):
    out=[]; A=int(a*R); Rr=int(r*R); D=int(d*R)
    for i in range(n):
        if i < A: e = i/max(A,1)
        elif i < A+D: e = 1-(1-s)*(i-A)/max(D,1)
        else: e = s
        if i > n-Rr: e *= max(0,(n-i)/max(Rr,1))
        out.append(e)
    return out
def note(f): return 440*2**((f-69)/12)   # midi -> Hz
def organ(freq, dur, vib=5.0):
    n=int(dur*R); e=env(n,0.008,0,1,0.04); out=[]
    parts=[(1,1.0),(2,0.7),(3,0.45),(4,0.35),(6,0.2),(8,0.15)]
    ph=0.0
    for i in range(n):
        t=i/R; fv=freq*(1+0.004*math.sin(2*math.pi*vib*t)); ph+=2*math.pi*fv/R
        out.append(e[i]*sum(a*math.sin(k*ph) for k,a in parts)/2.6)
    return out
def brass(freq, dur):
    n=int(dur*R); e=env(n,0.03,0.08,0.8,0.08); out=[]; ph=0.0; lp=0.0
    for i in range(n):
        t=i/R; ph+=freq*(1+0.003*math.sin(2*math.pi*5.5*t))/R; ph%=1.0
        saw=2*ph-1
        cut=0.15+0.35*min(1,t/0.05)   # brighten on attack
        lp+=cut*(saw-lp); out.append(e[i]*lp)
    return out
def noise(dur, cut=0.2):
    n=int(dur*R); lp=0.0; out=[]
    for i in range(n):
        lp+=cut*(random.uniform(-1,1)-lp); out.append(lp)
    return out
def normalize(x, peak=0.92):
    m=max(abs(v) for v in x) or 1
    return [v*peak/m for v in x]
def save(name, x):
    x=normalize(x)
    a=array.array('h',[int(max(-1,min(1,v))*32000) for v in x])
    with wave.open(name+'.wav','wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(R); w.writeframes(a.tobytes())
    print(name, round(len(x)/R,2),'s')

# 1 Home run: stadium organ "Charge!" (traditional bugle call)
def charge(base=67):
    seq=[(0,0.14),(5,0.14),(9,0.14),(12,0.36),(9,0.16),(12,0.9)]
    out=[]; t=0
    for semi,d in seq:
        mix(out, organ(note(base+semi), d*0.95), t); t+=d
    return out
hr=charge(); save('1_home_run_organ_charge', hr)

# 2 Grand slam: crowd swells, Charge twice (second a step higher), big organ chord
def crowd(dur, swell=True):
    n=int(dur*R); a=noise(dur,0.25); b=noise(dur,0.06); out=[]
    for i in range(n):
        t=i/R; e=(min(1,t/(dur*0.6)) if swell else 1)*min(1,(dur-t)/0.6)
        # rumble of many voices: wobbly amplitude
        w=1+0.25*math.sin(2*math.pi*3.1*t)+0.15*math.sin(2*math.pi*5.7*t)
        out.append(e*w*(0.6*a[i]+0.9*b[i]))
    return out
gs=[]; mix(gs, crowd(5.0), 0, 0.55)
mix(gs, charge(67), 0.3); mix(gs, charge(69), 2.2)
for m in (60,64,67,72,76): mix(gs, organ(note(m+2), 1.4), 4.0, 0.5)
save('2_grand_slam_crowd_charge_twice', gs)

# 3 Touchdown: brass fanfare (original)
fan=[(60,0.12),(60,0.12),(60,0.12),(65,0.45),(64,0.14),(65,0.14),(69,0.9)]
td=[]; t=0
for m,d in fan:
    mix(td, brass(note(m),d*0.95),t,1.0); mix(td, brass(note(m-12),d*0.95),t,0.5); t+=d
for m in (65,69,72): mix(td, brass(note(m),0.9), t-0.9, 0.35)
save('3_touchdown_brass_fanfare', td)

# 4 Field goal: kick thump, rising whistle, "doink" bell
fg=[]
th=[math.sin(2*math.pi*(90-40*i/R*10)*i/R)*math.exp(-i/R*25) for i in range(int(0.25*R))]
mix(fg, th, 0)
n=int(0.8*R); ph=0; up=[]
for i in range(n):
    f=700+900*(i/n); ph+=2*math.pi*f/R; up.append(0.35*math.sin(ph)*min(1,i/800)*min(1,(n-i)/800))
mix(fg, up, 0.15)
bell=[]
for i in range(int(1.4*R)):
    t=i/R; bell.append(math.exp(-t*3)*(math.sin(2*math.pi*1046*t)+0.5*math.sin(2*math.pi*2093*t*1.003)+0.3*math.sin(2*math.pi*2794*t)))
mix(fg, bell, 0.95, 0.8)
save('4_field_goal_kick_and_ding', fg)

# 5 Hockey goal horn
def horn(dur):
    n=int(dur*R); out=[]; e=env(n,0.08,0,1,0.3)
    for i in range(n):
        t=i/R; v=0
        for f,a in ((174,1.0),(175.5,0.8),(220,0.55),(261.5,0.45)):
            ph=(f*t)%1.0; v+=a*(2*ph-1)
        out.append(e[i]*v)
    lp=0; o=[]
    for v in out: lp+=0.35*(v-lp); o.append(lp)
    return o
gh=[]; mix(gh, horn(3.2), 0); mix(gh, crowd(3.6, False), 0.2, 0.25)
save('5_hockey_goal_horn', gh)

# 6 Referee whistle (flag / kickoff)
def whistle(dur):
    n=int(dur*R); out=[]; ph=0
    for i in range(n):
        t=i/R; f=2900+120*math.sin(2*math.pi*28*t); ph+=2*math.pi*f/R
        trill=0.65+0.35*math.sin(2*math.pi*28*t)
        out.append(trill*math.sin(ph)*min(1,i/200)*min(1,(n-i)/300))
    return out
wh=[]; mix(wh, whistle(0.25), 0); mix(wh, whistle(0.9), 0.4)
save('6_referee_whistle', wh)

# 7 Buzzer (end of quarter / period)
bz=[]
n=int(1.3*R)
for i in range(n):
    t=i/R; v=(1 if (440*t)%1<0.5 else -1)*0.6+(1 if (466*t)%1<0.5 else -1)*0.4
    bz.append(v*min(1,i/100)*min(1,(n-i)/400))
lp=0; b2=[]
for v in bz: lp+=0.4*(v-lp); b2.append(lp)
save('7_buzzer', b2)

# 8 Swish (3-pointer) then a ding
sw=[]; n=int(0.45*R); lp=0; hp_prev=0
for i in range(n):
    x=random.uniform(-1,1); c=0.6-0.5*(i/n); lp+=c*(x-lp)
    sw.append(lp*math.sin(math.pi*i/n)**1.5)
sw2=[]; mix(sw2, sw, 0)
mix(sw2, [math.exp(-i/R*5)*math.sin(2*math.pi*1568*i/R) for i in range(int(0.9*R))], 0.4, 0.5)
mix(sw2, [math.exp(-i/R*5)*math.sin(2*math.pi*2093*i/R) for i in range(int(0.9*R))], 0.55, 0.5)
save('8_swish_three_pointer', sw2)

# 9 Sad trombone (other team scores on you)
def tbone(f0, dur, wobble=False):
    n=int(dur*R); out=[]; ph=0; e=env(n,0.04,0.05,0.85,0.1); lp=0
    for i in range(n):
        t=i/R; f=f0*(1-0.03*min(1,t/0.1)) * (1+0.035*math.sin(2*math.pi*6*t) if wobble else 1)
        if wobble: f*=1-0.06*(t/dur)
        ph=(ph+f/R)%1; lp+=0.18*((2*ph-1)-lp); out.append(e[i]*lp)
    return out
st=[]; t=0
for m,d,w in ((55,0.42,False),(54,0.42,False),(53,0.42,False),(52,1.4,True)):
    mix(st, tbone(note(m),d,w), t); t+=d+0.06
save('9_sad_trombone_they_scored', st)

# 10 Win song (original victory tune, organ + brass)
win=[]; t=0
mel=[(67,0.15),(72,0.15),(76,0.15),(79,0.3),(76,0.15),(79,0.6),(84,0.9)]
for m,d in mel:
    mix(win, brass(note(m),d*0.95),t,0.9); mix(win, organ(note(m-12),d*0.95),t,0.5); t+=d
save('10_win_song', win)

# 11 Game starting: organ riff (rising, "da da da da DUM")
gsr=[]; t=0
for m,d in ((60,0.16),(64,0.16),(67,0.16),(72,0.16),(71,0.16),(72,0.55)):
    mix(gsr, organ(note(m),d*0.95), t); t+=d
save('11_game_starting_organ', gsr)

# 12 Close game heartbeat (last 2 minutes)
hb=[]
def thump(f=55, dur=0.18):
    return [math.sin(2*math.pi*f*i/R*(1-0.3*i/(dur*R)))*math.exp(-i/R*18)*(1+0.5*math.sin(2*math.pi*2*f*i/R)) for i in range(int(dur*R))]
for k in range(4):
    mix(hb, thump(), k*0.9); mix(hb, thump(48,0.15), k*0.9+0.22, 0.7)
save('12_close_game_heartbeat', hb)

# 13 Crowd roar alone
save('13_crowd_roar', crowd(3.0))
