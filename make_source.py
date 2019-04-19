#!/usr/bin/python3

import os
import re
import sys

def make_setup(cython_modules):
    import_rx = None
    pack_rx = None
    if len(cython_modules):
        import_rx = re.compile("import setuptools")
        pack_rx = re.compile("\\s+packages=")

    with open("setup.in") as f:
        with open("setup.py", "w") as g:
            for ln in f:
                g.write(ln)
                if import_rx and import_rx.match(ln):
                    g.write("from Cython.Build import cythonize\n")

                if pack_rx and pack_rx.match(ln):
                    paths = [ "mueddit/%s.pyx" % m for m in cython_modules ]
                    for p in paths:
                        if not os.path.isfile(p):
                            raise Exception("source file %s not found" % p)

                    quoted_paths = [ "\"%s\"" % p for p in paths ]
                    if len(quoted_paths) == 1:
                        cythonize_arg = quoted_paths[0]
                    else:
                        cythonize_arg = "[%s]" % ", ".join(quoted_paths)

                    g.write("ext_modules = cythonize(%s),\n" % cythonize_arg)

def make_top(with_cython):
    import_rx = None
    if with_cython:
        import_rx = re.compile("import")

    with open("mueddit/mueddi.in") as f:
        with open("mueddit/mueddi.py", "w") as g:
            for ln in f:
                if import_rx and import_rx.match(ln):
                    g.write("import pyximport; pyximport.install()\n")

                g.write(ln)

if __name__ == '__main__':
    cython_modules = sys.argv[1:]
    make_setup(cython_modules)
    # pyximport is unnecessary with setup support
    # make_top(len(cython_modules))
