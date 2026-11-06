#!/usr/bin/env python3
"""Compile shipping CLI into independent output; do not mutate parent's build targets."""
import pathlib,re,shlex,subprocess,hashlib,json
root=pathlib.Path('/source'); build=pathlib.Path('/build-on/programs/rpc_directory_client')
out=pathlib.Path('/logs/final-overlap-linux'); out.mkdir(exist_ok=True)
records=[]
for target,source,obj,name in [('golos-rpc-directory-client','programs/rpc_directory_client/main.cpp','main.cpp.o','golos-rpc-directory-client'),('rpc_directory_loopback_test','tests/rpc_directory/loopback_test.cpp','loopback_test.cpp.o','rpc_directory_loopback_test')]:
 d=build/('CMakeFiles/'+target+'.dir'); flags=(d/'flags.make').read_text(); args=[]
 for key in ('CXX_FLAGS','CXX_DEFINES','CXX_INCLUDES'): args+=shlex.split(re.search(r'^'+key+r' = (.*)$',flags,re.M).group(1))
 subprocess.run(['/usr/bin/c++']+args+['-c',str(root/source),'-o',str(out/obj)],check=True)
 link=shlex.split((d/'link.txt').read_text()); link=[str(out/obj) if x.endswith(obj) else x for x in link]; link[link.index('-o')+1]=str(out/name)
 subprocess.run(link,cwd=str(build),check=True)
 records.append({'artifact':str(out/name),'sha256':hashlib.sha256((out/name).read_bytes()).hexdigest()})
(out/'source-pin.json').write_text(json.dumps({'artifacts':records,'source':{p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in ['programs/rpc_directory_client/main.cpp','libraries/network/include/golos/network/rpc_search.hpp','libraries/network/node.cpp']}},indent=2))
print(json.dumps(records))
