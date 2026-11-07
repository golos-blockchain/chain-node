"""Extract exact production admission definitions for a listener-free runtime test.

Only the owning class name and the thread-affinity macro are adapted; neither
predicates nor FC future semantics are reimplemented in the test.
"""
from pathlib import Path
import re
import sys

source = Path(sys.argv[1]).read_text()
methods = []
for name in ('is_accepting_new_connections', 'is_wanting_new_connections', 'get_number_of_connections'):
    match = re.search(r'(bool|uint32_t) node_impl::' + name + r'\(\)\s*\{', source)
    if not match:
        raise RuntimeError('Missing production method: ' + name)
    depth = 1
    end = match.end()
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    methods.append(source[match.start():end].replace('node_impl::', 'admission_fixture::'))
Path(sys.argv[2]).write_text('\n'.join(methods) + '\n')
