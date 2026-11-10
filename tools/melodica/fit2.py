# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dubwise FM contributors
# Part of the REED body fit (roadmap 7.3; tools/melodica/README.md).
import os
OUT = os.environ.get("MEL_OUT", "build/melodica") + "/"
os.makedirs(OUT, exist_ok=True)
import json, numpy as np
J=json.load(open(OUT + "body.json"))
cen=np.array(J["cen"]); B=np.array([np.nan if b is None else b for b in J["B"]]); cnt=np.array(J["cnt"])
m=~np.isnan(B)&(cen>330); f=cen[m]; y=B[m]; wt=np.sqrt(cnt[m])
SR=44100.0
def peak(fc,q,g,fr):
    A=10**(g/40); w0=2*np.pi*fc/SR; al=np.sin(w0)/(2*q); c=np.cos(w0)
    b=np.array([1+al*A,-2*c,1-al*A]); a=np.array([1+al/A,-2*c,1-al/A]); return resp(b,a,fr)
def lpf(fc,q,fr):
    w0=2*np.pi*fc/SR; al=np.sin(w0)/(2*q); c=np.cos(w0)
    b=np.array([(1-c)/2,1-c,(1-c)/2]); a=np.array([1+al,-2*c,1-al]); return resp(b,a,fr)
def resp(b,a,fr):
    z=np.exp(-1j*2*np.pi*fr/SR)
    return 20*np.log10(np.abs((b[0]+b[1]*z+b[2]*z*z)/(a[0]+a[1]*z+a[2]*z*z)))
def model(p,fr):
    f1,q1,g1,f2,q2,g2,fl,ql,G=p
    return G+peak(f1,q1,g1,fr)+peak(f2,q2,g2,fr)+lpf(fl,ql,fr)
def cost(p):
    f1,q1,g1,f2,q2,g2,fl,ql,G=p
    if not(300<f1<f2<6000 and 0.3<q1<8 and 0.3<q2<8 and 1500<fl<12000 and 0.4<ql<3 and -12<g1<15 and -12<g2<15): return 1e9
    e=(model(p,f)-y)*wt; return float((e*e).sum()/(wt*wt).sum())
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
best=None
for f1 in (900,1300,1700):
  for f2 in (2300,2800):
    for fl in (3500,4500,6000):
        p,v=nm(cost,[f1,1.5,6,f2,2,5,fl,0.8,-2],[200,0.5,3,400,0.5,3,800,0.2,2])
        if best is None or v<best[1]: best=(p,v)
p,v=best
print("fit rms error %.2f dB"%np.sqrt(v))
print("peak1 %.0f Hz Q %.2f %+.1f dB | peak2 %.0f Hz Q %.2f %+.1f dB | LPF %.0f Hz Q %.2f | gain %+.1f"%tuple(p))
old=lambda fr: peak(1100,2,6,fr)+peak(2500,3,2+8*64/127,fr)+peak(4200,4,-6+8*64/127,fr)
G_old=np.average(y-old(f),weights=wt)
e_old=np.sqrt(np.average((old(f)+G_old-y)**2,weights=wt))
print("old body's rms error %.2f dB"%e_old)
for fr,yy,mm,oo in zip(f,y,model(p,f),old(f)+G_old): print("%6.0f  measured %6.1f  fit %6.1f  old %6.1f"%(fr,yy,mm,oo))
json.dump([float(x) for x in p],open(OUT + "params.json","w"))
