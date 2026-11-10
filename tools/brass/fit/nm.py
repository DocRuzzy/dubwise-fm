import numpy as np
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
