#!/usr/bin/env python3
"""Reuse exact linked fixture compiler/archive settings without editing frozen CMake."""
import pathlib, re, shlex, subprocess, sys
build=pathlib.Path('/build-on/programs/rpc_directory_client')
out=pathlib.Path('/source/tests/rpc_directory/adversarial/artifacts'); out.mkdir(exist_ok=True)
flags=(build/'CMakeFiles/rpc_directory_loopback_test.dir/flags.make').read_text()
args=[]
for name in ('CXX_FLAGS','CXX_DEFINES','CXX_INCLUDES'):
    args+=shlex.split(re.search(r'^'+name+r' = (.*)$',flags,re.M).group(1))
# Never instrument only a fixture and claim the linked node/FC archives were instrumented.
name=sys.argv[1] if len(sys.argv)>1 else 'wire_test'
if name not in ('wire_test','cache_atomic_test'): raise SystemExit('unknown owned fixture')
cmd=['/usr/bin/c++']+args+['-UNDEBUG','-c','/source/tests/rpc_directory/adversarial/'+name+'.cpp','-o',str(out/(name+'.o'))]
link=shlex.split((build/'CMakeFiles/rpc_directory_loopback_test.dir/link.txt').read_text())
link=[str(out/(name+'.o')) if x.endswith('loopback_test.cpp.o') else x for x in link]
link[link.index('-o')+1]=str(out/name)
with (out/(name+'-build.log')).open('w') as log:
    for command in (cmd,link):
        log.write(' '.join(shlex.quote(x) for x in command)+'\n'); log.flush()
        subprocess.run(command,cwd=str(build),stdout=log,stderr=subprocess.STDOUT,check=True)
print(out/name)
