#!/usr/bin/env python3
"""Render the stick-figure poses of src/apps/fitness/Figure.cpp to a PNG sheet, to tune them on a PC.

Usage: python3 scripts/preview_figures.py out.png [Name,Name,...]
Needs Pillow. The lengths and the ground rule copy src/apps/fitness/Figure.cpp; keep them the same.
"""
import math, re, sys
from PIL import Image, ImageDraw
src = open('src/apps/fitness/Figure.cpp').read()
names = re.findall(r'// (\w+)\s*\{(\d+),\s*(true|false),\s*Prop::(\w+),\s*(-?\d+),\s*(-?\d+),\s*(\d+),\s*\{(.*?\})\}\}', src, re.S)
W,H,G=180,150,146; LR=4
def step(p,a,l): r=math.radians(a); return (p[0]+l*math.sin(r), p[1]+l*math.cos(r))
def mixa(a,b,f):
    d=math.fmod(b-a,360)
    if d>180: d-=360
    if d<-180: d+=360
    return a+d*f
def frame(poses,front,prop,px,py,t):
    n=len(poses); i=int(t)%n; f=t-int(t); f=f*f*(3-2*f)
    p0,p1=poses[i],poses[(i+1)%n]
    v=[p0[0]+(p1[0]-p0[0])*f]
    for k in range(1,11):
        h0=p0[1] if k==10 and p0[k]==0 else p0[k]; h1=p1[1] if k==10 and p1[k]==0 else p1[k]
        v.append(mixa(h0,h1,f))
    hip=(v[0],0); neck=step(hip,v[1],36); head=step(neck,v[10],12)
    eF=step(neck,v[2],20); hF=step(eF,v[3],20); eN=step(neck,v[4],20); hN=step(eN,v[5],20)
    kF=step(hip,v[6],29); fF=step(kF,v[7],29); kN=step(hip,v[8],29); fN=step(kN,v[9],29)
    low=max([head[1]+9]+[j[1]+LR for j in (hip,neck,eF,hF,eN,hN,kF,fF,kN,fN)])
    dy=G-low
    im=Image.new('RGB',(W,H),'black'); d=ImageDraw.Draw(im)
    d.rectangle([6,G,W-6,G+3],fill=(40,40,40))
    if prop=='Wall': d.rectangle([px-4,4,px,G],fill=(40,40,40))
    if prop=='Chair': d.rectangle([px,py,px+40,py+5],fill=(40,40,40)); d.rectangle([px+2,py,px+6,G],fill=(40,40,40)); d.rectangle([px+34,py,px+38,G],fill=(40,40,40))
    far=(255,255,255) if front else (128,128,128)
    def L(a,b,c): d.line([(a[0],a[1]+dy),(b[0],b[1]+dy)],fill=c,width=8); [d.ellipse([q[0]-4,q[1]+dy-4,q[0]+4,q[1]+dy+4],fill=c) for q in (a,b)]
    for a,b in ((neck,eF),(eF,hF),(hip,kF),(kF,fF)): L(a,b,far)
    for a,b in ((hip,neck),(hip,kN),(kN,fN),(neck,eN),(eN,hN)): L(a,b,(255,255,255))
    d.ellipse([head[0]-9,head[1]+dy-9,head[0]+9,head[1]+dy+9],fill=(255,255,255))
    return im
out=sys.argv[1]; sel=sys.argv[2].split(',') if len(sys.argv)>2 else None
rows=[]
for nm,per,front,prop,px,py,cnt,body in names:
    if sel and nm not in sel: continue
    poses=[list(map(int,m.split(','))) for m in re.findall(r'\{(-?\d+(?:,\s*-?\d+){10})\}',body)]
    assert len(poses)==int(cnt),(nm,len(poses),cnt)
    ts=[i*0.5 for i in range(2*len(poses))] if len(poses)>1 else [0]
    rows.append((nm,[frame(poses,front=='true',prop,int(px),int(py),t) for t in ts]))
cols=max(len(r[1]) for r in rows)
sheet=Image.new('RGB',(130+cols*(W+4),len(rows)*(H+4)),(20,20,60)); d=ImageDraw.Draw(sheet)
for r,(nm,ims) in enumerate(rows):
    d.text((4,r*(H+4)+H//2),nm,fill='white')
    for c,im in enumerate(ims): sheet.paste(im,(130+c*(W+4),r*(H+4)))
sheet.save(out); print(len(rows),'rows')
