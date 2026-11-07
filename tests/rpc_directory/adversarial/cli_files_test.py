#!/usr/bin/env python3
"""Real CLI file handling. No public endpoints: cache entries use fictional documentation IPs."""
import hashlib,json,os,pathlib,resource,subprocess,sys,tempfile,time
client=sys.argv[1]
root=pathlib.Path(__file__).resolve().parent/'artifacts'; root.mkdir(exist_ok=True)
results=[]
def limited(): resource.setrlimit(resource.RLIMIT_AS,(512*1024*1024,512*1024*1024))
with tempfile.TemporaryDirectory(dir=str(root)) as d:
    base=pathlib.Path(d)
    for name,data in [('stale',str(int(time.time())-7200).encode()+b'\n192.0.2.1:1234\n'),('corrupt',b'fictional-corrupt\n'),('future',b'999999999999\n'),('private',str(int(time.time())).encode()+b'\n127.0.0.1:1234\n'),('oversized',b'x'*8193)]:
        if name not in ('corrupt','oversized'): data=b'supporters-v2\n'+data
        state=base/name; state.mkdir(); cache=state/'rpc_search_supporters.v2'; cache.write_bytes(data)
        before=hashlib.sha256(data).hexdigest()
        r=subprocess.run([client,'--search',str(state),'1','--seed','127.0.0.1:1'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=6)
        assert r.returncode==3 and json.loads(r.stdout)==[],(name,r.returncode,r.stderr)
        assert hashlib.sha256(cache.read_bytes()).hexdigest()==before
        results.append({'case':'CLI-cache-'+name,'status':'PASS','old_sha256':before})
    # A sparse oversized cache must be read only through the fixed 8193-byte buffer.
    state=base/'sparse'; state.mkdir(); cache=state/'rpc_search_supporters.v2'
    with cache.open('wb') as f: f.truncate(1024*1024*1024)
    r=subprocess.run([client,'--search',str(state),'1','--seed','127.0.0.1:1'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=6,preexec_fn=limited)
    assert r.returncode==3,(r.returncode,r.stderr)
    results.append({'case':'CLI-cache-1GiB-512MiB-AS','status':'PASS'})
    baseline_peak_kib=resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
    seed=base/'oversized-seeds'
    with seed.open('wb') as f: f.truncate(1024*1024*1024)
    r=subprocess.run([client,'--search',str(base/'seed-state'),'1','--seed-file',str(seed)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=6,preexec_fn=limited)
    peak_kib=resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
    # Seed parsing precedes node construction: reject length after <=254 bytes, not a huge getline.
    results.append({'case':'CLI-seed-file-1GiB-512MiB-AS','status':'PASS' if r.returncode==2 and peak_kib<32768 else 'RED','expected_exit':2,'actual_exit':r.returncode,'maximum_allowed_peak_kib':32768,'actual_peak_kib':peak_kib,'prior_children_peak_kib':baseline_peak_kib,'stderr':r.stderr.decode(errors='replace')})
(root/'cli-files-results.json').write_text(json.dumps(results,indent=2))
for row in results: print(json.dumps(row))
sys.exit(any(row['status']=='RED' for row in results))
