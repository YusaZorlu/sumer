#!/usr/bin/env python3
"""Lists string literals in the game sources (and the refusals server.py sends) that have no Turkish entry in turkish.c.

Usage: trcheck.py <srcdir>
Adjacent literals ("a" "b") are joined, as the C compiler does.
"""
import os
import re
import sys

src = sys.argv[1]
LIT = re.compile(r'"((?:[^"\\\n]|\\.)*)"')


def literals(text):
    # Drop comments and #include lines, then join adjacent literals.
    text = re.sub(r'//[^\n]*', '', text)
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    text = re.sub(r'^\s*#\s*include[^\n]*', '', text, flags=re.M)
    out = []
    pos = 0
    while True:
        m = LIT.search(text, pos)
        if not m:
            break
        s = m.group(1)
        end = m.end()
        while True:
            n = re.match(r'\s*"((?:[^"\\\n]|\\.)*)"', text[end:])
            if not n:
                break
            s += n.group(1)
            end += n.end()
        out.append(s)
        pos = end
    return out


tr = open(os.path.join(src, 'turkish.c'), encoding='utf-8').read()
keys = set()
for m in re.finditer(r'\{\s*"((?:[^"\\\n]|\\.)*)"\s*,\s*"((?:[^"\\\n]|\\.)*)"\s*\}', tr):
    keys.add(m.group(1))

# shaders.c is GLSL, never shown to a player.
SKIP = ('turkish.c', 'shaders.c')
missing = {}
for name in sorted(os.listdir(src)):
    if not name.endswith('.c') or name in SKIP:
        continue
    for s in literals(open(os.path.join(src, name), encoding='utf-8').read()):
        if s in keys or not re.search(r'[A-Za-z]{2}', s):
            continue
        missing.setdefault(s, name)

# What server.py refuses with is shown on the title screen in the player's language too.
server = os.path.join(src, '..', 'server.py')
said = re.findall(r'text\("((?:[^"\\\n]|\\.)*)"\)', open(server, encoding='utf-8').read()) if os.path.exists(server) else []
for s in said:
    if s not in keys and re.search(r'[A-Za-z]{2}', s):
        missing.setdefault(s, 'server.py')

for s, name in sorted(missing.items(), key=lambda kv: (kv[1], kv[0])):
    print(f'{name}: "{s}"')
print(f'-- {len(missing)} literals without a Turkish entry', file=sys.stderr)

used = set(said)
for name in os.listdir(src):
    if name.endswith('.c') and name not in SKIP:
        used.update(literals(open(os.path.join(src, name), encoding='utf-8').read()))
for k in sorted(keys - used):
    print(f'unused: "{k}"', file=sys.stderr)
