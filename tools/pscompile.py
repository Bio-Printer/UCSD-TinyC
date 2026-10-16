#!/usr/bin/env python3
"""pscompile.py file.c -- compile one file with the P-System Tiny-C, print its messages"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from selfhost import compile_on_psystem
src = sys.argv[1]
name = os.path.splitext(os.path.basename(src))[0].upper()
ok, tr, ps, info = compile_on_psystem([(name + '.TEXT', src)], name)
i = max(0, tr.find(' bytes free') - 40)
print('\n'.join(l for l in tr[i:].split('\n') if l.strip())[:3000])
