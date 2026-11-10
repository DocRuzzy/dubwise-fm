import sys, os, json, numpy as np
exec(open("/tmp/claude-0/brass/fitbrass.py").read().split("segs=[]")[0])   # load, harmonics, smooth, nm, RUNS
RUNS2=[("sax/87fb8f8c-359186__mtg__sax-tenor-c3.wav",0.5,4.1,138),("sax/b64132d2-359219__mtg__sax-tenor-c3.wav",0.6,3.7,138),
       ("sax/69e1f8ba-359230__mtg__sax-tenor-c4.wav",0.8,4.3,264),("sax/9185f399-359197__mtg__sax-tenor-c4.wav",0.7,3.9,264),
       ("sax/ff6125ed-359209__mtg__sax-tenor-c5.wav",0.4,2.3,525),("sax/d69e32b4-359243__mtg__sax-tenor-c5.wav",0.4,3.6,558),("sax/43627652-359244__mtg__sax-tenor-d5.wav",1.3,3.8,596),("sax/79cff9a8-359245__mtg__sax-tenor-d5.wav",0.9,1.9,639),("sax/90a952f2-359246__mtg__sax-tenor-e5.wav",0.5,1.9,678),
       ("tpt/78aeaea1-374156__sgossner__trumpet-sustain-c4-sum_shtrumpet_sus_c3_v3_rr1.wav",0.5,5.5,261),
       ("tpt/9ef9b14d-374146__sgossner__trumpet-sustain-c4-sum_shtrumpet_sus_c3_v1_rr1.wav",1.0,10.0,261),
       ("tpt/f474aacc-374145__sgossner__trumpet-sustain-a5-sum_shtrumpet_sus_a4_v1_rr1.wav",0.5,12.0,882),
       ("tpt/24e75547-374147__sgossner__trumpet-sustain-c6-sum_shtrumpet_sus_c5_v1_rr1.wav",0.5,12.0,1050),("tpt/50f7cf88-374157__sgossner__trumpet-sustain-c6-sum_shtrumpet_sus_c5_v3_rr1.wav",0.5,6.0,1050),("tpt/97008ce9-374159__sgossner__trumpet-sustain-d5-sum_shtrumpet_sus_d4_v3_rr1.wav",0.5,6.0,588),("tpt/7a28f72d-374149__sgossner__trumpet-sustain-d5-sum_shtrumpet_sus_d4_v1_rr1.wav",1.0,12.0,588),("tpt/63509656-374151__sgossner__trumpet-sustain-e5-sum_shtrumpet_sus_f4_v1_rr1.wav",5.0,12.0,700),]
# (trumpet and sax only: the lead's register)
segs=[]
for fn,a,b,f0g in RUNS2:
    x,sr=load("/tmp/claude-0/brass/"+fn); f0,hd=harmonics(x[int(a*sr):int(b*sr)], f0g)
    fr=np.arange(1,len(hd)+1)*f0; tag=fn.split("/")[0]+" "+("v3" if "_v3_" in fn else "v1" if "_v1_" in fn else "")+" %.0fHz"%f0
    segs.append((tag,f0,fr,smooth(fr,hd)))
def resp(b,a,fr):
    z=np.exp(-1j*2*np.pi*fr/SR); return 20*np.log10(np.abs((b[0]+b[1]*z+b[2]*z*z)/(a[0]+a[1]*z+a[2]*z*z)))
def peak(fc,q,g,fr):
    A=10**(g/40); w0=2*np.pi*fc/SR; al=np.sin(w0)/(2*q); c=np.cos(w0)
    return resp([1+al*A,-2*c,1-al*A],[1+al/A,-2*c,1-al/A],fr)
def lpf(fc,q,fr):
    w0=2*np.pi*fc/SR; al=np.sin(w0)/(2*q); c=np.cos(w0)
    return resp([(1-c)/2,1-c,(1-c)/2],[1+al,-2*c,1-al],fr)
def hpf(fc,q,fr):
    w0=2*np.pi*fc/SR; al=np.sin(w0)/(2*q); c=np.cos(w0)
    return resp([(1+c)/2,-(1+c),(1+c)/2],[1+al,-2*c,1-al],fr)
SL=np.linspace(0,24,97)                          # the source's fall, dB an octave of harmonic number
def best_s(sd,fr,body):
    k=fr/fr[0]; w=(sd>sd.max()-30).astype(float); best=(1e9,0)
    for s in SL:
        m=-s*np.log2(k)+body; off=np.sum((sd-m)*w)/w.sum(); e=np.sum(((sd-m-off)*w)**2)/w.sum()
        if e<best[0]: best=(e,s)
    return best
def body(p,fr):
    fh,qh,f1,q1,g1,fl,ql=p
    return hpf(fh,qh,fr)+peak(f1,q1,g1,fr)+lpf(fl,ql,fr)
def cost(p,detail=False):
    fh,qh,f1,q1,g1,fl,ql=p
    if not(50<fh<1500 and 0.4<qh<3 and 300<f1<6000 and 0.3<q1<6 and -12<g1<20 and 1000<fl<14000 and 0.4<ql<3): return 1e9
    tot=0; per=[]
    for tag,f0,fr,sd in segs:
        e,s=best_s(sd,fr,body(p,fr)); tot+=e; per.append((tag,np.sqrt(e),s))
    return (tot/len(segs),per) if detail else tot/len(segs)
best=None
for fh in (200,500):
  for f1 in (800,1500):
    for fl in (2500,5000):
        p,v=nm(cost,[fh,0.8,f1,1.0,4,fl,0.8],[80,0.2,300,0.3,3,800,0.2],400)
        if best is None or v<best[1]: best=(p,v)
p,v=best
print("rms %.2f dB  HPF %.0f Hz Q %.2f | peak %.0f Hz Q %.2f %+.1f dB | LPF %.0f Hz Q %.2f"%(np.sqrt(v),*p))
_,per=cost(p,True)
for t,e,s in per: print("  %-24s err %4.1f dB   source fall %4.1f dB/oct"%(t,e,s))
json.dump({"p":[float(x) for x in p],"per":[(t,float(e),float(s)) for t,e,s in per]},open("/tmp/claude-0/brass/fit5.json","w"))
