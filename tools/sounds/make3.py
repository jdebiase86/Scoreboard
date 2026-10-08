from lib import *
random.seed(23)
# warmer organ: fewer high drawbars, so it isn't shrill
ORG3={1:1.0,2:0.8,3:0.35,4:0.18,6:0.06}
def organ3(f, dur):
    n=int(dur*R); e=adsr(n,0.012,0,1,0.05)
    return tone(lambda t: f*(1+0.004*math.sin(TAU*5*t)), dur, lambda k,t: ORG3.get(k,0)/2.4, e)
def charge3(base):
    out=[]; t=0
    for semi,d in [(0,0.14),(5,0.14),(9,0.14),(12,0.36),(9,0.16),(12,0.9)]:
        mix(out, organ3(note(base+semi), d*0.95), t); t+=d
    return out
save('v3_1_home_run_charge_lower', charge3(60))

# grand slam: no hiss - a low "roar" (voices, no top end) and claps instead of the old crowd
def roar(dur):
    n=int(dur*R); x=biquad(biquad(noise(dur),'lp',700,0.7),'lp',700,0.7); out=[]
    for i in range(n):
        t=i/R; e=min(1,t/(dur*0.5))*min(1,(dur-t)/0.8)
        out.append(e*x[i]*(1+0.25*math.sin(TAU*2.3*t)))
    return out
def claps(dur, rate=7):
    out=[0.0]*int(dur*R)
    t=0.0
    while t<dur-0.1:
        c=biquad([random.uniform(-1,1)*math.exp(-i/R*90) for i in range(int(0.04*R))],'bp',1400,1.0)
        mix(out, c, t, random.uniform(0.3,0.7)); t+=random.uniform(0.3,1.0)/rate
    return out
gs=[]; mix(gs, roar(5.0), 0, 0.7); mix(gs, claps(5.0, 14), 0, 0.25)
mix(gs, charge3(60), 0.3); mix(gs, charge3(62), 2.2)
for m in (55,59,62,67,71): mix(gs, organ3(note(m), 1.4), 4.0, 0.5)
save('v3_2_grand_slam_no_hiss', gs)

# buzzer: deeper and buzzier - low raspy square + saw, little filtering
def buzz3(dur):
    n=int(dur*R); e=adsr(n,0.008,0,1,0.05); out=[0.0]*n
    for f,a,odd in ((110,1.0,True),(130.8,0.7,True),(55,0.5,False)):
        v=tone(lambda t,f=f: f, dur, (lambda k,t: (1/k**0.8) if k%2 else 0) if odd else (lambda k,t: 1/k), e)
        for i in range(n): out[i]+=a*v[i]
    # rasp: a fast flutter on top
    for i in range(n): out[i]*=1+0.18*math.sin(TAU*45*i/R)
    return biquad(out,'lp',3500,0.7)
save('v3_7_buzzer_deeper', buzz3(1.6))

# swish A: quick "tsshh" falling, like the ball dropping through the net
def swishA():
    d=0.38; n=int(d*R); x=noise(d); out=[]; y1=y2=x1=x2=0.0
    for i in range(n):
        t=i/n; f0=5200-3800*t; q=1.4
        w=TAU*f0/R; c,s=math.cos(w),math.sin(w); a=s/(2*q)
        b0,b2=a/(1+a),-a/(1+a); a1,a2=-2*c/(1+a),(1-a)/(1+a)
        o=b0*x[i]+b2*x2-a1*y1-a2*y2; x2,x1,y2,y1=x1,x[i],y1,o
        env=min(1,t*12)*(1-t)**1.6
        out.append(o*env)
    return out
sa=[]; mix(sa, swishA(), 0); mix(sa, roar(1.3), 0.3, 0.5); mix(sa, claps(1.3, 16), 0.3, 0.3)
save('v3_8a_swish_falling', sa)
# swish B: two quick net brushes "swish-ish" then a bright ding
sb=[]; mix(sb, swishA()[:int(0.22*R)], 0); mix(sb, swishA(), 0.12, 0.8)
bell=[math.exp(-i/R*4)*(math.sin(TAU*1568*i/R)+0.4*math.sin(TAU*3136*i/R)) for i in range(int(1.0*R))]
mix(sb, bell, 0.45, 0.35)
save('v3_8b_swish_double_with_ding', sb)

# win song A: brass melody - "C E G C, A B C" with drums and a big ending
def tim(f, dur=0.6):
    return [math.sin(TAU*f*i/R)*math.exp(-i/R*6) for i in range(int(dur*R))]
def cym(dur=1.6):
    x=biquad(noise(dur),'hp',3500,0.7); return [v*math.exp(-i/R*2.5) for i,v in enumerate(x)]
wa=[]; t=0
for m,d in ((60,0.16),(64,0.16),(67,0.16),(72,0.45),(69,0.16),(71,0.16),(72,1.2)):
    mix(wa, brass(note(m), d*0.92), t); mix(wa, brass(note(m-12), d*0.92), t, 0.4); t+=d
for m in (64,67): mix(wa, brass(note(m), 1.2), t-1.2, 0.4)
mix(wa, tim(note(36)), 0, 0.6); mix(wa, tim(note(43)), 0.48, 0.5); mix(wa, tim(note(36),1.0), t-1.2, 0.9)
mix(wa, cym(), t-1.2, 0.3)
save('v3_10a_win_song_brass_tune', wa)
# win song B: organ "victory lap" - quick rising run, then three big chords (da, da, DAAA)
wb=[]; t=0
for m in (60,62,64,65,67,69,71,72):
    mix(wb, organ3(note(m), 0.075), t); t+=0.07
for ms,d in (((67,71,74),0.22),((67,71,74),0.22),((72,76,79),1.3)):
    for m in ms: mix(wb, organ3(note(m), d*0.9), t, 0.5); 
    for m in ms: mix(wb, brass(note(m), d*0.9), t, 0.35)
    t+=d+0.04
mix(wb, tim(note(36),1.0), t-1.34, 0.9); mix(wb, cym(), t-1.34, 0.25)
save('v3_10b_win_song_organ_chords', wb)
