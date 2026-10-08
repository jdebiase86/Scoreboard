from lib import *
ORG3={1:1.0,2:0.8,3:0.35,4:0.18,6:0.06}
def organ3(f, dur):
    n=int(dur*R); e=adsr(n,0.012,0,1,0.05)
    return tone(lambda t: f*(1+0.004*math.sin(TAU*5*t)), dur, lambda k,t: ORG3.get(k,0)/2.4, e)
def charge3(base):
    out=[]; t=0
    for semi,d in [(0,0.14),(5,0.14),(9,0.14),(12,0.36),(9,0.16),(12,0.9)]:
        mix(out, organ3(note(base+semi), d*0.95), t); t+=d
    return out
def tim(f, dur=0.7): return [math.sin(TAU*f*i/R)*math.exp(-i/R*6) for i in range(int(dur*R))]
# grand slam, organ only: Charge, Charge a step higher, then a big held chord with a drum hit
gs=[]; mix(gs, charge3(60), 0); mix(gs, charge3(62), 1.9)
for m in (55,59,62,67,71): mix(gs, organ3(note(m), 1.5), 3.75, 0.5)
mix(gs, tim(note(43)), 3.75, 0.5)
save('v4_2_grand_slam_organ_only', gs)

def buzzy(freqs, dur, tilt, rasp=0.0, rasp_hz=0):
    n=int(dur*R); e=adsr(n,0.006,0,1,0.04); out=[0.0]*n
    for f,a in freqs:
        v=tone(lambda t,f=f: f, dur, lambda k,t: 1/k**tilt, e)
        for i in range(n): out[i]+=a*v[i]
    if rasp:
        for i in range(n): out[i]*=1+rasp*(1 if math.sin(TAU*rasp_hz*i/R)>0 else -1)
    return out
# A: arena horn - two notes a little apart, very bright (all harmonics strong)
save('v4_7a_buzzer_arena_horn', buzzy(((185,1.0),(196,0.9)), 1.5, 0.35))
# B: old gym buzzer - electric, raspy on/off flutter
save('v4_7b_buzzer_gym_electric', biquad(buzzy(((120,1.0),(240,0.4)), 1.5, 0.25, 0.35, 60),'lp',5000,0.7))
# C: deep air-horn blast with a bit of growl
save('v4_7c_buzzer_deep_blast', buzzy(((98,1.0),(147,0.7),(99.5,0.6)), 1.6, 0.45, 0.12, 30))
