import sys, glob, os, numpy as np
sys.path.insert(0, "/home/user/dubwise-fm/tools/melodica")
os.environ["MEL_OUT"] = "/tmp/claude-0/mel4"
from harm import load
def f0_frames(x, sr, hop=0.1):
    out=[]
    N=4096
    for s in range(0, len(x)-N, int(hop*sr)):
        seg=x[s:s+N]; seg=seg-seg.mean(); e=np.sqrt(np.mean(seg**2))
        if e < 0.01: out.append((s/sr, 0, e)); continue
        ac=np.correlate(seg, seg, "full")[N-1:]; ac/=ac[0]
        lo, hi = int(sr/1200), int(sr/50)
        bv=ac[lo:hi].max(); best=0
        for l in range(lo+1, hi-1):
            if ac[l]>=0.9*bv and ac[l]>=ac[l-1] and ac[l]>=ac[l+1]: best=l; break
        out.append((s/sr, sr/best if best else 0, e))
    return out
for fn in sorted(glob.glob(sys.argv[1]+"/*/*.wav")):
    x, sr = load(fn)
    fr = f0_frames(x, sr)
    # steady runs: consecutive frames within 1 % of each other, voiced
    runs=[]; cur=[]
    for t,f,e in fr:
        if f and cur and abs(f/cur[-1][1]-1)<0.01: cur.append((t,f,e))
        else:
            if len(cur)>=6: runs.append(cur)
            cur=[(t,f,e)] if f else []
    if len(cur)>=6: runs.append(cur)
    print("%-52s %5.1fs  steady runs: %s"%(os.path.basename(fn)[:52], len(x)/sr,
          ", ".join("%.1f-%.1fs %.0f Hz"%(r[0][0], r[-1][0], np.median([q[1] for q in r])) for r in runs[:10])))
