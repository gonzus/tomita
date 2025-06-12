# ToDo 🔨

## Definitely 👍

* Allow specifying grammar elements with full words:
  * Right now we assume non-terminals are a single uppercase character.
  * Right now we assume terminals are a single lowercase character
    (surrounded by double quotes it seems?).
  * We would need a way to specify terminals (tokens);
    then non-terminals are "the rest".
* Add support for quantifiers in a grammar definition:
  * zero or one, AKA optional: `A?`, `(A b C)?`.
  * zero or more, AKA [Kleene star](https://en.wikipedia.org/wiki/Kleene_star):
    `A*`, `(A b C)*`.
  * one or more, AKA [Kleene plus](https://en.wikipedia.org/wiki/Kleene_star#Kleene_plus):
    `A+`, `(A b C)+`.
* Define a sane way to add actions to the grammar,
  so that the generated parser can do more than just parsing:
  * Use code generation / templates?  This forces us to have a template for
    each target language we wish to support, and to be able to at least
    understand some of the syntax for each language.
  * Maybe generate the parsing tables in a "universal" format (JSON?).  This
    forces us to implement, in each target language we wish to support, the
    "driver" that reads this information and uses it to run the parser.
* Add documentation.

## Probably 🤔

* Map the implementation to some form of reference document that describes the
  algorithm.  It should be possible to map each major routine to each part of
  the algorithm.

## Maybe ❓

* Support some form of lexing.  The current system behaves as a [scannerless
  parser](https://en.wikipedia.org/wiki/Scannerless_parsing):
  * The tokens are defined as part of the grammar.
  * The driver reads strings separated by white space and treats each string as
    a token to be passed to the parser.
  * It *might be faster*, but it is also **less powerful**.

## Already done 🔥
* Print information about any conflicts found in the grammar (`shift/reduce`,
  `reduce/reduce`) while generating the parser.
* Support `yacc` syntax in grammar files -- easier to copy / paste.
* Support comments in grammar files -- probably with `#`.
* Use intermediate files (branch `using-files`):
  * Unify `mem.[hc]` and `memory.[hc]`.
  * Get rid of `console.[hc]`?
  * Put more constants in `tomita.h` and **use them**.
* Add unit tests: *yes* **yes** ***yes***.
* Make multi-step work possible.  This is hampered today by the fact that a
  `parser` needs a `grammar` and a `forest` needs a `parser` (and a `grammar`).
  * Step 1: processing a `grammar` definition and generating the `parser` for
    the given `grammar`.
  * Step 2: using the `parser` to process a file and generate a parsing
    `forest`, corresponding to all possible parse trees for the input.  This
    step should print information about ambiguities in the input.
  * These steps should be "`make`-friendly" -- if a grammar definition has not
    changed, we should not have to regenerate the corresponding `parser`.
* Add support for `<-` (same as `:`) and `/` (same as `|`) in grammars, so that
  it is easier to play around with, for example, the [Zig
  grammar](https://ziglang.org/documentation/master/#Grammar), which is defined
  using [PEG format](https://en.wikipedia.org/wiki/Parsing_expression_grammar).
