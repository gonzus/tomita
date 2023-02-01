## ToDo 🔨

### Definitely 👍

* Make multi-step work possible.  This is hampered today by the fact that a
  `parser` needs a `grammar` and a `forest` needs a `parser` (and a `grammar`).
  * Step 1: processing a `grammar` definition and generating the `parser` for
    the given `grammar`.
  * Step 2: using the `parser` to process a file and generate a parsing
    `forest`, corresponding to all possible parse trees for the input.  This
    step should print information about ambiguities in the input.
  * These steps should be "`make`-friendly" -- if a grammar definition has not
    changed, we should not have to regenerate the corresponding `parser`.
* Define a sane way for step #2 to work.
  * Use code generation / templates?  This forces to have a template for each
    target language we wish to support -- but see below.
  * Maybe generate the parsing tables in a "universal" format (JSON?).  This
    forces to implement, in each target language we wish to support, the
    "driver" that reads this information and uses it to run the parser.
* Add documentation.

### Probably 🤔

* Support some form of lexing.  The current behavior is:
  * The tokens are defined as part of the grammar.
  * The driver reads strings separated by white space and treats each string as
    a token to be passed to the parser.
* Map the implementation to some form of reference document that describes the
  algorithm.  It should be possible to map each major routine to each part of
  the algorithm.

### Maybe ❓

* Not sure yet...

### Already done 🔥
* Print information about any conflicts found in the grammar (`shift/reduce`,
  `reduce/reduce`) while generating the parser.
* Support `yacc` syntax in grammar files -- easier to copy / paste.
* Support comments in grammar files -- probably with `#`.
* Use intermediate files (branch `using-files`):
  * Unify `mem.[hc]` and `memory.[hc]`.
  * Get rid of `console.[hc]`?
  * Put more constants in `tomita.h` and **use them**.
* Add unit tests: *yes* **yes** ***yes***.
