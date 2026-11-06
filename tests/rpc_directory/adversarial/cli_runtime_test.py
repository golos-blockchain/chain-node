#!/usr/bin/env python3
"""Real CLI socket caps while owned TCP peers stall encrypted handshake (not encrypted-message coverage)."""
import json,pathlib,select,signal,socket,subprocess,sys,tempfile,time
root=pathlib.Path(__file__).resolve().parent/'artifacts'; root.mkdir(exist_ok=True)
listeners=[]; peers=[]; total=0; peak=0
try:
    args=[]
    for i in range(24):
        s=socket.socket(); s.bind(('127.0.0.1',0)); s.listen(); s.setblocking(False); listeners.append(s)
        args+=['--seed','127.0.0.1:'+str(s.getsockname()[1])]
    with tempfile.TemporaryDirectory(dir=str(root)) as state:
        child=subprocess.Popen([sys.argv[1],'--search',state,'30']+args,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        start=time.monotonic()
        while time.monotonic()-start<18:
            ready=select.select(listeners+peers,[],[],0.02)[0]
            for p in list(peers):
                if p in ready:
                    try:
                        data=p.recv(4096)
                        if not data: peers.remove(p); p.close()
                    except ConnectionResetError: peers.remove(p); p.close()
            for s in listeners:
                if s in ready:
                    p,_=s.accept(); p.setblocking(False); peers.append(p); total+=1
            peak=max(peak,len(peers))
        cancel=time.monotonic(); child.send_signal(signal.SIGINT); out,err=child.communicate(timeout=6)
        result={'attempts':total,'max_live_sockets':peak,'cancel_seconds':time.monotonic()-cancel,'exit':child.returncode}
        (root/'cli-runtime-results.json').write_text(json.dumps(result,indent=2))
        print(json.dumps(result))
        assert total==16 and peak<=2 and child.returncode==130,result
finally:
    if 'child' in locals() and child.poll() is None: child.kill(); child.wait()
    for p in peers+listeners: p.close()
