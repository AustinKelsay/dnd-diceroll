import csv,sys,glob
csv.field_size_limit(10**9)
run=sys.argv[1]
for p in sorted(glob.glob(f'.local/perf/{run}/*/*.csv')):
    ft=[]; ev=[]
    for i,r in enumerate(csv.DictReader(open(p))):
        try: ft.append((float(r['FrameTime']),i))
        except (ValueError,TypeError): pass
        if r.get('EVENTS'): ev.append((i,r['EVENTS'][:50]))
    srt=sorted(ft,reverse=True)[:3]
    print(p.split('/')[-1],'frames',len(ft),'top3 ms(idx):',[(round(f,1),i) for f,i in srt],'| >100ms:',[(round(f),i) for f,i in ft if f>100])
