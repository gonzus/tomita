# Playground for GLR parsing algorithms

## What

This repo allows me (and you) to play around with grammars, parsers and parse forests.  The focus is on [GLR parsing](https://en.wikipedia.org/wiki/GLR_parser), using an algorithm devised by [Masaru Tomita](https://en.wikipedia.org/wiki/Masaru_Tomita).

## Why

Because I love parsers, programming languages and natural language processing.

## How

All the code is written in C and a [Makefile](Makefile) is provided. You can type `make help` to see a list of the supported targets, which as of now are:
* `clean`: clean everything
* `all`: (re)build everything
* `tests`: build all tests
* `test`: run all tests
* `tom`: build main executable

## Whence

This code was not originally written by me, but I have made extensive modifications and (hopefully) improvements.  The original code was found [here](http://www.cs.cmu.edu/afs/cs/project/ai-repository/ai/areas/nlp/parsing/tom/).  Please see [here](README-original.md) for more details.
