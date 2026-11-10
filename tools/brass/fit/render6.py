import sys, json, wave, numpy as np
sys.argv_save=list(sys.argv)
exec(open("/tmp/claude-0/brass/fit5.py").read().split("best=None")[0])   # segs, body(), harmonics etc.
P=json.load(open("/tmp/claude-0/brass/fit5.json"))
p=P["p"]; per={t:s for t,e,s in P["per"]}
rng=np.random.default_rng(7)
def residual(tag_sub, slope):                     # the instrument's own pattern: measured less the smooth model, by harmonic
    for (tag,f0,fr,sd),(t2,e,s) in zip(segs,P["per"]):
        if tag_sub in tag:
            x,_=load("/tmp/claude-0/brass/"+RUNS2[[i for i,(t,_,_,_) in enumerate(segs) if t==tag][0]][0])
            break
    # raw (unsmoothed) harmonics of that note
    idx=[i for i,(t,_,_,_) in enumerate(segs) if tag_sub in t][0]
    fn,a,b,f0g=RUNS2[idx]; x,sr=load("/tmp/claude-0/brass/"+fn); f0,hd=harmonics(x[int(a*sr):int(b*sr)],f0g)
    fr=np.arange(1,len(hd)+1)*f0; k=np.arange(1,len(hd)+1)
    m=-s*np.log2(k)+body(p,fr); off=np.mean((hd-m)[:8]); r=np.clip(hd-m-off,-15,10)[:30]
    return r
R_TPT_LOUD=residual("tpt v3 262Hz",0); R_TPT_SOFT=residual("tpt v1 262Hz",0); R_SAX=residual("sax  264Hz",0)
def lowpass_noise(n, hz):
    w=rng.standard_normal(n); a=np.exp(-2*np.pi*hz/SR); y=np.zeros(n); z=0
    for i in range(n): z=a*z+(1-a)*w[i]; y[i]=z
    return y/ (np.std(y)+1e-12)
def coefs(kind,fc,q,g=0):
    w0=2*np.pi*fc/SR; al=np.sin(w0)/(2*q); c=np.cos(w0)
    if kind=="hp": b=[(1+c)/2,-(1+c),(1+c)/2]; a=[1+al,-2*c,1-al]
    elif kind=="lp": b=[(1-c)/2,1-c,(1-c)/2]; a=[1+al,-2*c,1-al]
    elif kind=="bp": b=[al,0,-al]; a=[1+al,-2*c,1-al]
    else:
        A=10**(g/40); b=[1+al*A,-2*c,1-al*A]; a=[1+al/A,-2*c,1-al/A]
    return np.array(b)/a[0], np.array(a)/a[0]
def bq(x,b,a):
    y=np.zeros_like(x); z1=z2=0.0
    for i,v in enumerate(x):
        o=b[0]*v+z1; z1=b[1]*v-a[1]*o+z2; z2=b[2]*v-a[2]*o; y[i]=o
    return y
def bell(y):
    fh,qh,f1,q1,g1,fl,ql=p
    for kind,fc,q,g in (("hp",fh,qh,0),("pk",f1,q1,g1),("lp",fl,ql,0)):
        b,a=coefs(kind,fc,q,g); y=bq(y,b,a)
    return y
def note(f0, slope, resid, textured, secs=1.6, noise_db=-30):
    n=int(SR*secs); t=np.arange(n)/SR
    if textured:                                   # the measured movement: wander ~2 c, jitter ~1.5 c, level 0.8 / 0.2 dB
        cents=3.5*lowpass_noise(n,2.0)+1.2*lowpass_noise(n,30.0)
        lev=10**((0.9*lowpass_noise(n,1.5)+0.06*lowpass_noise(n,25.0))/20)
    else:
        cents=np.zeros(n); lev=np.ones(n)
    ph=2*np.pi*np.cumsum(f0*2**(cents/1200))/SR
    K=int(16000/f0); y=np.zeros(n); k=np.arange(1,K+1)
    amp=10**((-slope*np.log2(k) + (np.pad(resid,(0,max(0,K-len(resid))))[:K] if textured else 0))/20)
    for kk in range(K): y+=amp[kk]*np.cos((kk+1)*ph)
    if textured:                                   # breath: noise pulsed at the note (the reed or lips open once a cycle)
        pulse=0.5+0.5*np.cos(ph); nz=rng.standard_normal(n)*pulse**2
        b,a=coefs("bp",2500,0.7); nz=bq(nz,b,a)
        y=y/np.sqrt(np.mean(y**2)); nz=nz/np.sqrt(np.mean(nz**2))*10**(noise_db/20)
        y=y+nz
    y=bell(y)*lev
    env=np.minimum(1,t/0.04)*np.minimum(1,(secs-t)/0.05)
    return y*env
SR=44100
def rd(fn):
    w=wave.open(fn); x=np.frombuffer(w.readframes(w.getnframes()),np.int16).astype(float)/32768
    return x.reshape(-1,w.getnchannels()).mean(axis=1)
def norm(x,tg=-20):
    r=np.sqrt(np.mean(x[len(x)//4:3*len(x)//4]**2)); y=x*10**(tg/20)/r; f=np.linspace(0,1,441); y[:441]*=f; y[-441:]*=f[::-1]; return y
T="/tmp/claude-0/brass/tpt/"
tv3=rd(T+"78aeaea1-374156__sgossner__trumpet-sustain-c4-sum_shtrumpet_sus_c3_v3_rr1.wav")[SR//2:SR//2+int(SR*1.6)]
tv1=rd(T+"9ef9b14d-374146__sgossner__trumpet-sustain-c4-sum_shtrumpet_sus_c3_v1_rr1.wav")[SR:SR+int(SR*1.6)]
sax=rd("/tmp/claude-0/brass/sax/69e1f8ba-359230__mtg__sax-tenor-c4.wav")[SR:SR+int(SR*1.6)]
gap=np.zeros(SR//2); big=np.zeros(SR)
seq=[tv3, note(261.6,8.5,R_TPT_LOUD,False), note(261.6,8.5,R_TPT_LOUD,True,noise_db=-24), big,
     tv1, note(261.6,14.2,R_TPT_SOFT,True,noise_db=-27), big,
     sax, note(261.6,11.5,R_SAX,False), note(261.6,11.5,R_SAX,True,noise_db=-42)]
y=np.concatenate(sum([[norm(s) if s.any() else s,gap] for s in seq],[])); y=np.clip(y,-1,1)
w=wave.open(sys.argv_save[1],"wb"); w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes((y*32767).astype(np.int16).tobytes()); w.close()
wave_out=sys.argv_save[1]
# the textured model notes alone, for the texture measure
for nm_,s_ in (("m_tpt_loud",seq[2]),("m_sax",seq[9])):
    q=np.clip(norm(s_),-1,1); w=wave.open("/tmp/claude-0/brass/%s.wav"%nm_,"wb"); w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes((q*32767).astype(np.int16).tobytes()); w.close()
print("ok")
