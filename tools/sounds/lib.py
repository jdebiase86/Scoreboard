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

