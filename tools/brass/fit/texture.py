import sys, os, wave, numpy as np
SR=44100
def rd(fn):
    w=wave.open(fn); x=np.frombuffer(w.readframes(w.getnframes()),np.int16).astype(float)/32768
    return x.reshape(-1,w.getnchannels()).mean(axis=1)
def texture(x, f0):
    # pitch track: per 20 ms frame, autocorrelation peak near the period, parabolic
    hop=int(0.01*SR); N=int(0.04*SR); P=SR/f0; pit=[]; amp=[]
    for s in range(0,len(x)-N-int(2*P),hop):
        seg=x[s:s+N+int(2*P)]
        lo,hi=int(P*0.9),int(P*1.1)+1
        ac=[np.dot(seg[:N],seg[l:l+N]) for l in range(lo,hi+1)]
        i=int(np.argmax(ac)); 
        if 0<i<len(ac)-1:
            y0,y1,y2=ac[i-1],ac[i],ac[i+1]; d=0.5*(y0-y2)/(y0-2*y1+y2)
        else: d=0
        pit.append(SR/(lo+i+d)); amp.append(np.sqrt(np.mean(seg[:N]**2)))
    pit=np.array(pit); amp=np.array(amp)
    c=1200*np.log2(pit/np.median(pit))
    # slow wander (over 0.3 s) and fast jitter (frame to frame, the slow taken off)
    k=30; slow=np.convolve(c,np.ones(k)/k,'same'); fast=c-slow
    a=20*np.log10(amp/np.median(amp)); aslow=np.convolve(a,np.ones(k)/k,'same'); afast=a-aslow
    # noise between the harmonics: energy off the harmonic lines, against all
    seg=x[len(x)//4:len(x)//4+32768]; S=np.abs(np.fft.rfft(seg*np.hanning(len(seg))))**2; f=np.fft.rfftfreq(len(seg),1/SR)
    f0m=np.median(pit); near=np.zeros_like(f,bool)
    for kk in range(1,int(15000/f0m)): near|=np.abs(f-kk*f0m)<f0m*0.12
    m=(f>100)&(f<15000)
    nhr=10*np.log10(S[m&~near].sum()/S[m&near].sum())
    return np.std(slow[k:-k]), np.std(fast[k:-k]), np.std(aslow[k:-k]), np.std(afast[k:-k]), nhr
for lab,fn,f0,a,b in [l.split("|") for l in sys.argv[1:]]:
    x=rd(fn); x=x[int(float(a)*SR):int(float(b)*SR)]
    w,j,aw,af,n=texture(x,float(f0))
    print("%-18s pitch wander %4.1f c  jitter %4.1f c | level wander %4.2f dB  flicker %4.2f dB | noise %5.1f dB"%(lab,w,j,aw,af,n))
