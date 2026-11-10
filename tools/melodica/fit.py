# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dubwise FM contributors
# Part of the REED body fit (roadmap 7.3; tools/melodica/README.md).
import os
OUT = os.environ.get("MEL_OUT", "build/melodica") + "/"
os.makedirs(OUT, exist_ok=True)
import json, numpy as np
H=json.load(open(OUT + "harm.json"))
# real notes, and one of each distinct urlande pattern
keep=[]; seen=[]
for name,v in H.items():
    hd=np.array(v["hd"][:6])
    if "urlande" in name:
        if any(np.max(np.abs(hd-s))<1.0 for s in seen): continue
        seen.append(hd)
    keep.append((name,v))
print("notes used:",len(keep))
D=0.525; W=0.08
def src_harm(f0, n):
    N=8192; t=np.arange(N)/N
    u=np.where(t<D,(t/D)**1.5,np.where(t<D+W,1-(t-D)/W,0.0))
    U=np.abs(np.fft.rfft(u-u.mean()))[1:n+1]
    db=20*np.log10(U/np.sqrt((U**2).sum())+1e-12)
    return np.arange(1,n+1)*f0, db
edges=2**np.arange(np.log2(150),np.log2(10000),1/6); cen=edges*2**(1/12)
diff=[[] for _ in edges]; meas=[[] for _ in edges]
for name,v in keep:
    hf=np.array(v["hf"]); hd=np.array(v["hd"]); n=len(hf)
    sf,sd=src_harm(v["f0"],n)
    ok=hd>-70
    for f,m,s,o in zip(hf,hd,sd,ok):
        if not o: continue
        i=np.searchsorted(edges,f)-1
        if 0<=i<len(edges): diff[i].append(m-s); meas[i].append(m)
B=np.array([np.median(d) if len(d)>=4 else np.nan for d in diff])
cnt=np.array([len(d) for d in diff])
for c,b,k in zip(cen,B,cnt): print("%7.0f Hz body %6.1f dB (%d)"%(c,b,k))
json.dump({"cen":cen.tolist(),"B":[None if np.isnan(b) else float(b) for b in B],"cnt":cnt.tolist()},open(OUT + "body.json","w"))
