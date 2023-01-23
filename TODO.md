## ToDo

### Definitely!

* Support comments in grammar files -- probably with `#`.
* Make multi-step work possible.  This is hampered today by the fact that a
  `forest` needs a `parser` (and a `grammar`), and a `parser` needs a
  `grammar`.
  * Step 1: processing a `grammar` definition and generating the `parser` for
    the given `grammar`.  This step should print information about any
    conflicts found in the grammar (`shift/reduce`, `reduce/reduce`,
    `accept/reduce`).
  * Step 2: using the `parser` to process a file and generate a parsing
    `forest`, corresponding to all possible parse trees for the input.  This
    step should print information about ambiguities in the input.
  * These steps should be "`make`-friendly" -- if a grammar definition has not
    changed, we should not have to regenerate the corresponding `parser`.
* Define a sane way for step #2 to work.
  * Avoid code generation / templates?  This forces to have a template for each
    target language we wish to support -- but see below.
  * Maybe generate the parsing tables in a "universal" format (JSON?).  This
    forces to implement, in each target language we wish to support, the
    "driver" that reads this information and uses it to run the parser.

* Add documentation.

### Probably...

* Support some form of lexing.
* Map the implementation to some form of reference document that describes the
  algorithm.  It should be possible to map each major routine to each part of
  the algorithm.

### Maybe?

* Not sure yet...
