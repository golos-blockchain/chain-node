#!/usr/bin/env python3
"""NONSHIPPING TEST BUILD: owned loopback policy only, no production CLI bypass.
Recompile the actual CLI translation unit against a temporary policy overlay;
all transport/node/archive code and cache publication is production code.
"""
import pathlib,re,shlex,subprocess,tempfile,json,signal
root=pathlib.Path('/source'); build=pathlib.Path('/build-on/programs/rpc_directory_client')
out=root/'tests/rpc_directory/adversarial/artifacts'; out.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory() as tmp:
 tmp=pathlib.Path(tmp); overlay=tmp/'golos/network'; overlay.mkdir(parents=True)
 header=(root/'libraries/network/include/golos/network/rpc_search.hpp').read_text()
 header=header.replace('static bool public_endpoint(const std::string& endpoint) {','static bool public_endpoint(const std::string& endpoint) {\n        if(endpoint.compare(0,10,"127.0.0.1:")==0) return true; // NONSHIPPING OWNED TEST ONLY')
 (overlay/'rpc_search.hpp').write_text(header)
 flags=(build/'CMakeFiles/golos-rpc-directory-client.dir/flags.make').read_text(); args=[]
 for name in ('CXX_FLAGS','CXX_DEFINES','CXX_INCLUDES'): args+=shlex.split(re.search(r'^'+name+r' = (.*)$',flags,re.M).group(1))
 subprocess.run(['/usr/bin/c++','-I'+str(tmp)]+args+['-c',str(root/'programs/rpc_directory_client/main.cpp'),'-o',str(tmp/'cli.o')],check=True)
 link=shlex.split((build/'CMakeFiles/golos-rpc-directory-client.dir/link.txt').read_text()); link=[str(tmp/'cli.o') if x.endswith('main.cpp.o') else x for x in link]; link[link.index('-o')+1]=str(tmp/'test-policy-cli')
 subprocess.run(link,cwd=str(build),check=True)
 server=subprocess.Popen([str(build/'rpc_directory_loopback_test'),'--serve'],stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,universal_newlines=True)
 try:
  provider,ordinary=server.stdout.readline().strip().split()
  state=tmp/'state'
  def run(seed):
   seeds=seed if isinstance(seed,list) else [seed]
   r=subprocess.run([str(tmp/'test-policy-cli'),'--search',str(state),'3']+sum((['--seed',s] for s in seeds),[]),stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=8)
   assert r.returncode==0,(r.returncode,r.stderr.decode()); assert json.loads(r.stdout)[0]['trust']=='UNVERIFIED_PEER_ANNOUNCEMENT'
  state.mkdir(); legacy=state/'rpc_search_peers.v1'; legacy.write_text(str(int(__import__('time').time()))+'\n'+provider+'\n')
  ignored=subprocess.run([str(tmp/'test-policy-cli'),'--search',str(state),'1','--seed','127.0.0.1:1'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=5)
  assert ignored.returncode==3 and not (state/'rpc_search_supporters.v2').exists(),'legacy referral cache must fail closed'
  run(provider)
  cache=state/'rpc_search_supporters.v2'
  assert cache.exists(),'explicit provider without provider referral must be cached (v2)'
  lines=cache.read_text().splitlines(); assert lines[0]=='supporters-v2' and lines[2:]==[provider],lines
  run(['127.0.0.1:'+str(p) for p in range(1,17)]) # Cache must precede full attempt-budget ordinary seeds.
  print('NONSHIPPING owned-loopback CLI explicit provider cache + restart dead ordinary seed: PASS')
  # Ordinary seed returns provider plus other ordinary peers; none is itself cache support.
  state=tmp/'ordinary-state'; run(ordinary)
  lines=(state/'rpc_search_supporters.v2').read_text().splitlines()
  assert lines[2:]==[provider],lines
  print('NONSHIPPING owned-loopback CLI ordinary referrals exclude nonsupporters: PASS')
  # Reverse direction: a valid but dead cache must not starve bootstrap or its supporter referral.
  for count in (16,64):
   state=tmp/('dead-cache-'+str(count)); state.mkdir()
   cache=state/'rpc_search_supporters.v2'
   cache.write_text('supporters-v2\n'+str(int(__import__('time').time()))+'\n'+''.join('127.0.0.1:'+str(p)+'\n' for p in range(1,count+1)))
   run(ordinary)
   assert cache.read_text().splitlines()[2:]==[provider],cache.read_text()
   print('NONSHIPPING encrypted CLI '+str(count)+' dead cache -> bootstrap -> supporter referral: PASS')
  # The only working provider overlaps admitted cached #22. Bootstrap must promote it.
  state=tmp/'overlap-cache-22'; state.mkdir()
  cache=state/'rpc_search_supporters.v2'
  dead=['127.0.0.1:'+str(p) for p in range(1,65)]
  dead[21]=provider
  cache.write_text('supporters-v2\n'+str(int(__import__('time').time()))+'\n'+'\n'.join(dead)+'\n')
  run(ordinary)
  assert cache.read_text().splitlines()[2:]==[provider],cache.read_text()
  print('NONSHIPPING encrypted actual CLI cached #22 overlapping bootstrap referral: PASS')
  subprocess.run(['/usr/bin/c++']+args+['-UNDEBUG','-c',str(root/'tests/rpc_directory/adversarial/cache_flood_provider.cpp'),'-o',str(tmp/'flood.o')],check=True)
  flood_link=[str(tmp/'flood.o') if x==str(tmp/'cli.o') else x for x in link]; flood_link[flood_link.index('-o')+1]=str(tmp/'flood-provider')
  subprocess.run(flood_link,cwd=str(build),check=True)
  flood=subprocess.Popen([str(tmp/'flood-provider')],stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,universal_newlines=True)
  try:
   flood_endpoint=flood.stdout.readline().strip(); state=tmp/'flood-state'; run(flood_endpoint)
   lines=(state/'rpc_search_supporters.v2').read_text().splitlines(); assert lines[2:]==[flood_endpoint],lines
   run('127.0.0.1:1')
   print('NONSHIPPING CLI encrypted 40x64 referral flood: only observed provider cached; restart PASS')
  finally:
   flood.send_signal(signal.SIGTERM); flood.wait(timeout=5)
 finally:
  server.send_signal(signal.SIGTERM); server.wait(timeout=5)
