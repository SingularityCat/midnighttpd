#!/usr/bin/env python3
import sys
import json

def mktree(ptset):
    root = {}
    toks = []
    pts = iter(ptset)
    for tok, path in pts:
        node = root
        for char in path:
            if char in node:
                node = node[char]
            else:
                node[char] = node = {}
        node[None] = tok
        toks.append(tok)
    return root, toks

def statmat(tree, tab="    ", depth=1):
    idt = tab * depth
    print(idt + "c = *str++;")
    print(idt + "switch(c)")
    print(idt + "{")
    for c in sorted(tree.keys(), key=lambda k: (k is None, k)):
        if c is None:
            print(idt + tab +"default:")
            print(idt + tab + tab +"res = {0};".format(tree[c]))
            break
        print(idt + tab + "case '{0}':".format(c))
        statmat(tree[c], tab, depth+3)
    print(idt + "}")
    if depth > 1:
        print(idt + "break;")

def wrapmat(tree, toks, mtype, mident, tab="  "):
    if len(toks) == 0:
        return

    print("enum {0}".format(mtype))
    print("{")
    toks = iter(toks)
    tok = next(toks)
    print(tab + tok + " = 1", end="")
    for tok in toks:
        print(",")
        print(tab + tok, end="")
    print("\n};")

    print("static inline enum {0} {1}(const char *str, char * const *osp)".format(mtype, mident))
    print("{")
    print(tab + "enum {0} res = 0;".format(mtype))
    print(tab + "char c;")
    statmat(tree, tab)
    print()
    print(tab + "if(osp)")
    print(tab + "{")
    print(tab + tab + "*osp = str;")
    print(tab + "}")
    print()
    print(tab + "return res;")
    print("}")
    print()

source = json.load(sys.stdin)
tree, toks = mktree(source["enumeration"].items())
wrapmat(tree, toks, source["enum_name"], source["func_name"])
