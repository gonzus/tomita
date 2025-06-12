# Playground for GLR parsing algorithms

## What

This repo allows me (and you) to play around with grammars, parsers and parse forests.
The focus is on [GLR parsing](https://en.wikipedia.org/wiki/GLR_parser),
using an algorithm devised by [Masaru Tomita](https://en.wikipedia.org/wiki/Masaru_Tomita).

## Why

Because I love parsers, programming languages and natural language processing.

## How

All the code is written in C and a `Makefile` is provided.
You can type `make help` to see a list of the supported targets:
```
$ make help
all                             (re)build everything
clean                           clean everything
help                            display this help
test                            run all tests
tests                           build all tests
tom                             build main executable
```

Examples are in directory `examples`. One possible run could be:
```
$ ./tom -f examples/germany.gram -g -t -s examples/germany.inp
```

Running with flag `-?` prints a usage message:
```
$ ./tom -?
Usage: ./tom -f file [-gts] file ...
   -f  use this grammar file (required)
   -g  display input grammar
   -t  display parsing table
   -s  display parsing stack
   -?  print this help
```

If running on a Mac, it is recommended to set environment variable
`MallocNanoZone` to `0`, to avoid seeing these useless warnings:
```
tomita(87311,0x1f070df00) malloc: nano zone abandoned due to inability to reserve vm space.
```

## Whence

This code was not originally written by me,
but I have made extensive modifications and (hopefully) improvements.
The original code was found
[here](http://www.cs.cmu.edu/afs/cs/project/ai-repository/ai/areas/nlp/parsing/tom/).
Please see file `README-original.md` for more details.
