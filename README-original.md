This directory contains C source for the Tomita Natural Language parsing
algorithm.  This project has a long history starting in 10/88 based on a study
of the Tomita algorithm done in 7/87 and reviewed in 9/88.  The source code,
however, was written from scratch between 5/20/93 and 5/23/93.

Possible extensions to this include:
* higher LR(k) parsing ability
* incremental parsing table generation
* optimization
* table-packing
* actions/disambiguation rules, lexical rules, etc.

This implementation of the Tomita Algorithm has not been formally verified with
respect to cyclic grammars, but hours of searching for counter-examples has
failed to turn up any negative results.

To install this software, make sure you have an ANSI-C compiler.  Edit the
Makefile, if necessary, and compile by typing `make`.

The following files are included:
```
    The source code .......... Makefile tom.c
    This readme .............. README.md
docs:
    Documentation ............ tom.doc
    The Algorithm ............ tomita.ref
examples:
    Sample grammars .......... gram0 gram1 gram2 gram3
    Sample parsing table ..... gram0.tab
    Sample input and output .. in out
```

The file `gram0.tab` was created by entering the following command line and
typing an empty line to terminate parsing:
```
    ./tom -c examples/gram0 >examples/gram0.tab
```

The file `out` was created with the following command line:
```
    ./tom -s examples/gram0 <examples/in >examples/out
```
