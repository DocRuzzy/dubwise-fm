# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dubwise FM contributors
# REED's reed puff and body, fitted note by note to the real long notes (fese, pinevoc), each note's spectrum smoothed
# over a third of an octave (roadmap 7.3; tools/melodica/README.md). Slow (minutes): pure numpy.
#   python3 -I tools/melodica/fit_puff.py samples/melodica
import json, sys, os, numpy as np
SAMPLES = sys.argv[1] if len(sys.argv) > 1 else 'samples/melodica'
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harm import load, spec, harm_amp
import glob
SR=44100.0
notes=[]
for fn in sorted(glob.glob(SAMPLES + "/fese*/*.wav") + glob.glob(SAMPLES + "/pinevoc*/*.wav")):
    x,sr=load(fn); F,S=spec(x,sr)
    from harm import f0_est
    f0=f0_est(F,S)
    K=int(min(40, 12000/f0))
    hs=np.array([harm_amp(F,S,k*f0,f0*0.03) for k in range(1,K+1)])
    hd=20*np.log10(hs/hs.max()+1e-9)
    notes.append((os.path.basename(fn),f0,hd))
print("notes",[(n,round(f)) for n,f,_ in notes])
def pulse_harm(delta,p,K):
    N=4096; t=np.arange(N)/N
    u=np.where(t<delta, np.sin(np.pi*t/delta)**p, 0.0)
    U=np.abs(np.fft.rfft(u))[1:K+1]; return 20*np.log10(U+1e-12)
def resp(b,a,fr):
    z=np.exp(-1j*2*np.pi*fr/SR); return 20*np.log10(np.abs((b[0]+b[1]*z+b[2]*z*z)/(a[0]+a[1]*z+a[2]*z*z)))
def peak(fc,q,g,fr):
    A=10**(g/40); w0=2*np.pi*fc/SR; al=np.sin(w0)/(2*q); c=np.cos(w0)
    return resp([1+al*A,-2*c,1-al*A],[1+al/A,-2*c,1-al/A],fr)
def lpf(fc,q,fr):
    w0=2*np.pi*fc/SR; al=np.sin(w0)/(2*q); c=np.cos(w0)
    return resp([(1-c)/2,1-c,(1-c)/2],[1+al,-2*c,1-al],fr)
def nm(fun,x0,step,it=6000):
    n=len(x0); pts=[np.array(x0,float)]
    for i in range(n):
        x=np.array(x0,float); x[i]+=step[i]; pts.append(x)
    vals=[fun(p) for p in pts]
    for _ in range(it):
        o=np.argsort(vals); pts=[pts[i] for i in o]; vals=[vals[i] for i in o]
        c=np.mean(pts[:-1],axis=0); xr=c+(c-pts[-1]); fr_=fun(xr)
        if fr_<vals[0]:
            xe=c+2*(c-pts[-1]); fe=fun(xe)
            pts[-1],vals[-1]=(xe,fe) if fe<fr_ else (xr,fr_)
        elif fr_<vals[-2]: pts[-1],vals[-1]=xr,fr_
        else:
            xc=c+0.5*(pts[-1]-c); fc=fun(xc)
            if fc<vals[-1]: pts[-1],vals[-1]=xc,fc
            else:
                pts=[pts[0]+0.5*(p-pts[0]) for p in pts]; vals=[fun(p) for p in pts]
    i=int(np.argmin(vals)); return pts[i],vals[i]
def smooth(fr, db):                               # each note's levels smoothed over a third of an octave (power)
    out=np.empty_like(db); p=10**(db/10)
    for i,f in enumerate(fr):
        m=(fr>f*2**(-1/6))&(fr<f*2**(1/6)); out[i]=10*np.log10(p[m].mean())
    return out
SM=[(n,f0,smooth(np.arange(1,len(hd)+1)*f0,hd)) for n,f0,hd in notes]
def cost(p, detail=False):
    delta,pw,f1,q1,g1,fl,ql=p
    if not(0.03<delta<0.6 and 1.0<=pw<4 and 200<f1<6000 and 0.3<q1<6 and -15<g1<20 and 1500<fl<16000 and 0.4<ql<4): return 1e9
    tot=0; n=0; per=[]
    for name,f0,sd in SM:
        K=len(sd); fr=np.arange(1,K+1)*f0
        m=smooth(fr,pulse_harm(delta,pw,K))+peak(f1,q1,g1,fr)+lpf(fl,ql,fr)
        w=(sd>-60).astype(float); off=np.sum((sd-m)*w)/w.sum(); e=(sd-m-off)*w
        tot+=(e*e).sum(); n+=w.sum(); per.append((name,np.sqrt((e*e).sum()/w.sum())))
    return (tot/n, per) if detail else tot/n
best=None
for d in (0.08,0.12,0.2):
  for pw in (1.0,2.0):
    for fl in (2500,5000):
        p,v=nm(cost,[d,pw,2500,1.0,3,fl,1.0],[0.03,0.4,500,0.3,3,800,0.3],3000)
        if best is None or v<best[1]: best=(p,v)
p,v=best
print("rms %.2f dB  delta %.3f  shape sin^%.2f  peak %.0f Hz Q %.2f %+.1f dB  LPF %.0f Hz Q %.2f"%(np.sqrt(v),*p))
_,per=cost(p,True)
for n,e in per: print("  %-32s %.1f dB"%(n,e))
