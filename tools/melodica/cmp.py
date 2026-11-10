# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dubwise FM contributors
# Part of the REED body fit (roadmap 7.3; tools/melodica/README.md).
import os
OUT = os.environ.get("MEL_OUT", "build/melodica") + "/"
os.makedirs(OUT, exist_ok=True)
import json, sys, numpy as np, subprocess
def pooled(H, dedupe=True):
    edges=2**np.arange(np.log2(300),np.log2(10000),1/4); cen=edges*2**(1/8)
    bins=[[] for _ in edges]; seen=[]
    for name,v in H.items():
        hd=np.array(v["hd"])
        if dedupe and "urlande" in name:
            if any(np.max(np.abs(hd[:6]-s))<1.0 for s in seen): continue
            seen.append(hd[:6])
        for f,d in zip(v["hf"],hd):
            i=np.searchsorted(edges,f)-1
            if 0<=i<len(edges) and d>-70: bins[i].append(d)
    return cen,np.array([np.median(b) if len(b)>=3 else np.nan for b in bins])
M=json.load(open(sys.argv[1])); O=json.load(open(sys.argv[2]))
c,m=pooled(M); _,o=pooled(O,False)
ok=~np.isnan(m)&~np.isnan(o); off=np.mean(m[ok]-o[ok])
print("rms difference %.2f dB over %d bands"%(np.sqrt(np.mean((m[ok]-o[ok]-off)**2)),ok.sum()))
for cc,mm,oo,k in zip(c,m,o,ok):
    if k: print("%6.0f Hz real %6.1f  REED %6.1f"%(cc,mm,oo+off))
