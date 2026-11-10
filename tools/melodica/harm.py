# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dubwise FM contributors
# Part of the REED body fit (roadmap 7.3; tools/melodica/README.md).
import os
OUT = os.environ.get("MEL_OUT", "build/melodica") + "/"
os.makedirs(OUT, exist_ok=True)
import sys, glob, os, json, wave
import numpy as np
def load(fn):
    w=wave.open(fn); sr=w.getframerate(); n=w.getnframes(); ch=w.getnchannels(); sw=w.getsampwidth()
    raw=w.readframes(n)
    if sw==2: x=np.frombuffer(raw,np.int16).astype(float)/32768
    elif sw==3:
        b=np.frombuffer(raw,np.uint8).reshape(-1,3); x=(b[:,0].astype(np.int32)|(b[:,1].astype(np.int32)<<8)|(b[:,2].astype(np.int32)<<16)); x=np.where(x>=1<<23,x-(1<<24),x)/float(1<<23)
    elif sw==4: x=np.frombuffer(raw,np.int32).astype(float)/2**31
    else: x=(np.frombuffer(raw,np.uint8).astype(float)-128)/128
    x=x.reshape(-1,ch).mean(axis=1); return x,sr
def spec(x,sr):
    n=len(x); seg=x[int(n*0.3):int(n*0.7)]; seg=seg[:min(len(seg),32768)]; seg=seg-seg.mean()
    N=len(seg); S=np.abs(np.fft.rfft(seg*np.hanning(N),8*N)); f=np.fft.rfftfreq(8*N,1/sr)
    return f,S
def harm_amp(f,S,h,tol):
    m=(f>h-tol)&(f<h+tol)
    return S[m].max() if m.any() else 0.0
def f0_est(f,S):
    best=None
    for f0 in np.arange(120,1400,0.5):
        sc=sum(harm_amp(f,S,k*f0,f0*0.03)/k**0.5 for k in range(1,9) if k*f0<16000)
        if best is None or sc>best[1]: best=(f0,sc)
    # an octave down if its odd harmonics are present too (avoid octave-up errors)
    f0=best[0]
    while f0/2>=120:
        sub=f0/2; odd=np.mean([harm_amp(f,S,k*sub,sub*0.03) for k in (1,3,5)])
        evn=np.mean([harm_amp(f,S,k*sub,sub*0.03) for k in (2,4,6)])
        if odd>0.25*evn: f0=sub
        else: break
    return f0
def harmonics(x,sr):
    f,S=spec(x,sr)
    f0=f0_est(f,S)
    # refine f0 on the strongest of the first 4 harmonics
    hs=[]
    for k in range(1,60):
        h=k*f0
        if h>16000: break
        hs.append((h,harm_amp(f,S,h,f0*0.02)))
    a=np.array([v for _,v in hs]); db=20*np.log10(a/np.sqrt((a**2).sum())+1e-12)
    return f0,np.array([h for h,_ in hs]),db
if __name__=="__main__":
    out={}
    for fn in sorted(glob.glob(sys.argv[1]+"/*/*.wav")):
        x,sr=load(fn); f0,hf,hd=harmonics(x,sr)
        out[os.path.basename(fn)]={"f0":f0,"hf":hf.tolist(),"hd":hd.tolist()}
        print("%-36s f0 %7.1f  h1..6 %s"%(os.path.basename(fn)[:36],f0," ".join("%5.1f"%v for v in hd[:6])))
    json.dump(out,open(OUT + "harm.json","w"))
