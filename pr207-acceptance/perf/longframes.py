import csv,sys,json,glob,os
csv.field_size_limit(1<<30)
def analyze(path):
    rows=list(csv.DictReader(open(path)))
    ft=[]
    for r in rows:
        try: ft.append(float(r['FrameTime']))
        except (TypeError,ValueError,KeyError): pass
    n=len(ft)
    top=sorted(((v,i) for i,v in enumerate(ft)),reverse=True)[:5]
    over100=[(round(v,1),i) for i,v in enumerate(ft) if v>100]
    over33=[(round(v,1),i) for i,v in enumerate(ft) if v>33.4]
    # clusters: runs of >=3 consecutive frames >=20 ms
    cl=[];i=0
    while i<n:
        if ft[i]>=20:
            j=i
            while j<n and ft[j]>=20: j+=1
            if j-i>=3: cl.append((i,j-i,round(max(ft[i:j]),1)))
            i=j
        else: i+=1
    return dict(frames=n,top5=[(round(v,1),i) for v,i in top],over100=over100,n_over33=len(over33),clusters=cl)
if __name__=='__main__':
    for p in sys.argv[1:]:
        print(p); print(json.dumps(analyze(p)))
