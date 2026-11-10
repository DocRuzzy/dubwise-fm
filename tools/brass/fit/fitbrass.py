import sys, os, glob, json, numpy as np
sys.path.insert(0, "/home/user/dubwise-fm/tools/melodica")
os.environ["MEL_OUT"] = "/tmp/claude-0/mel4"
from harm import load
exec(open("/tmp/claude-0/mel/nm.py").read())
SR=44100.0
# the steady stretches (file, start, end, approx f0), from the survey
RUNS=[("127709__thatjeffcarter__trombone-notes-01.wav",0.5,1.8,126),("127709__thatjeffcarter__trombone-notes-01.wav",2.6,3.5,146),
      ("127709__thatjeffcarter__trombone-notes-01.wav",4.1,6.3,98),("127710__thatjeffcarter__trombone-drone.wav",10.1,14.8,126),
      ("127710__thatjeffcarter__trombone-drone.wav",35.9,40.6,98),("127710__thatjeffcarter__trombone-drone.wav",40.9,46.2,98),
      ("130083__thatjeffcarter__trombone-4a.wav",5.4,5.9,198),("130084__thatjeffcarter__trombone-3a.wav",0.1,3.1,235),
      ("130084__thatjeffcarter__trombone-3a.wav",3.3,4.8,225),("130085__thatjeffcarter__trombone-2a.wav",6.8,8.4,233),
      ("130086__thatjeffcarter__trombone-1a.wav",3.7,5.7,186),("130085__thatjeffcarter__trombone-2a.wav",3.5,4.6,235)]
def harmonics(x, f0g):
    seg=x-x.mean(); N=len(seg); S=np.abs(np.fft.rfft(seg*np.hanning(N), 4*N)); f=np.fft.rfftfreq(4*N, 1/SR)
    m=(f>f0g*0.9)&(f<f0g*1.1); f0=f[m][np.argmax(S[m])]
    K=int(min(60, 11000/f0)); hs=[]
    for k in range(1,K+1):
        mm=(f>k*f0*0.985)&(f<k*f0*1.015); hs.append(S[mm].max() if mm.any() else 1e-9)
    hs=np.array(hs); return f0, 20*np.log10(hs/hs.max()+1e-9)
def smooth(fr, db):
    p=10**(db/10); out=np.empty_like(db)
    for i,ff in enumerate(fr):
        m=(fr>ff*2**(-1/6))&(fr<ff*2**(1/6)); out[i]=10*np.log10(p[m].mean())
    return out
segs=[]
for fn,a,b,f0g in RUNS:
    x,sr=load("/tmp/claude-0/brass/tb/"+fn); f0,hd=harmonics(x[int(a*sr):int(b*sr)], f0g)
    fr=np.arange(1,len(hd)+1)*f0; segs.append((fn[:30]+" %.1fs"%a, f0, fr, smooth(fr,hd)))
    print("%-36s f0 %6.1f  h1-12: %s"%(segs[-1][0], f0, " ".join("%4.0f"%v for v in hd[:12])))
# the FM brass's source: sin(t + I sin t), its harmonics for an index I (tabulated)
IS=np.linspace(0.1, 10, 100); KMAX=60
TAB=[]
for I in IS:
    t=np.arange(4096)/4096*2*np.pi; y=np.sin(t+I*np.sin(t)); Y=np.abs(np.fft.rfft(y))[1:KMAX+1]
    TAB.append(20*np.log10(Y/Y.max()+1e-9))
TAB=np.array(TAB)
def src(I,K):
    i=np.clip(np.interp(I,IS,np.arange(len(IS))),0,len(IS)-1); lo=int(i); hi=min(lo+1,len(IS)-1); w=i-lo
    return (1-w)*TAB[lo][:K]+w*TAB[hi][:K]
def resp(b,a,fr):
    z=np.exp(-1j*2*np.pi*fr/SR); return 20*np.log10(np.abs((b[0]+b[1]*z+b[2]*z*z)/(a[0]+a[1]*z+a[2]*z*z)))
def peak(fc,q,g,fr):
    A=10**(g/40); w0=2*np.pi*fc/SR; al=np.sin(w0)/(2*q); c=np.cos(w0)
    return resp([1+al*A,-2*c,1-al*A],[1+al/A,-2*c,1-al/A],fr)
def lpf(fc,q,fr):
    w0=2*np.pi*fc/SR; al=np.sin(w0)/(2*q); c=np.cos(w0)
    return resp([(1-c)/2,1-c,(1-c)/2],[1+al,-2*c,1-al],fr)
def bestI(sd, fr, body):                      # each stretch's own index: the best of the table
    K=len(sd); best=(1e9,0)
    for I in IS:
        m=smooth(fr, src(I,K))+body; w=(sd>-60).astype(float); off=np.sum((sd-m)*w)/w.sum(); e=np.sum(((sd-m-off)*w)**2)/w.sum()
        if e<best[0]: best=(e,I)
    return best
def cost(p, detail=False):
    f1,q1,g1,fl,ql=p
    if not(200<f1<5000 and 0.3<q1<6 and -15<g1<20 and 800<fl<12000 and 0.4<ql<3): return 1e9
    tot=0; per=[]
    for name,f0,fr,sd in segs:
        e,I=bestI(sd, fr, peak(f1,q1,g1,fr)+lpf(fl,ql,fr)); tot+=e; per.append((name,np.sqrt(e),I))
    return (tot/len(segs), per) if detail else tot/len(segs)
best=None
for f1 in (500,1000,1500):
    for fl in (2000,4000):
        p,v=nm(cost,[f1,1.0,6,fl,0.8],[200,0.3,3,800,0.2],250)
        if best is None or v<best[1]: best=(p,v)
p,v=best
print("rms %.2f dB   peak %.0f Hz Q %.2f %+.1f dB   LPF %.0f Hz Q %.2f"%(np.sqrt(v),*p))
_,per=cost(p,True)
for n,e,I in per: print("  %-36s err %.1f dB  FM index %.2f"%(n,e,I))
_,per0=cost([1000,1,0,12000,0.7],True)
print("without a body (flat), index free per stretch: mean err %.1f dB"%np.mean([e for _,e,_ in per0]))
json.dump({"p":[float(x) for x in p],"I":[float(I) for _,_,I in per]},open("/tmp/claude-0/brass/fit.json","w"))
