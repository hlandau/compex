compex
======

compex is a GCC plugin for extracting type information from C++.

The plugin itself dumps information about structures, fields and methods in
YAML format, allowing you to easily build reflection facilities with C++.

A Python 3 helper script is included to convert to JSON if preferred.

You can also annotate structures, fields and methods with arbitrary tags.

Because compex is a GCC plugin, type information can be dumped as you compile
files, eliminating the need to parse your source code twice and eliminate
compile times.

Because compex uses GCC's C++ parsing facilities, it understands all of C++,
unlike tools such as Qt's moc, which can only parse limited subsets of C++.

compex is a very small codebase which can be easily customized or adapted. It
may also serve as interesting reading for those interested in writing GCC
plugins.

Since compex relies upon the C++11 attribute syntax, use of C++11 is required
for source code processed by compex.

compex is entirely unrelated to C++ RTTI (dynamic_cast) and does not require it
in any way.

Building
--------
Change into the `gcc` directory and run `make`. Run `make install` to install.
The following are the default installation parameters, which you may override
by passing them to make:

    PREFIX=/usr/local
    BINPATH=$(PREFIX)/bin
    INCPATH=$(PREFIX)/include
    LIBPATH=$(PREFIX)/lib
    DESTDIR=                    # sandbox installation path

You can then run `g++` using the plugin directly:

    g++ -c -fplugin=/usr/local/lib/compex_gcc.so file.cpp

or, after installation, using `compex-config` (recommended):

    g++ -c `compex-config --gcc` file.cpp

compex output is sent to stdout by default. To redirect it to a file, pass `-o
filename` to `compex-config`:

    g++ -c `compex-config --gcc -o file.info` file.cpp

You can alternatively pass `-fplugin-arg-compex_gcc-o=filename` directly to g++.

If you want to run compex without producing normal object code output, pass
`-S -o /dev/null` to g++.

Example Input Programs; Example Output
--------------------------------------
See the `doc/examples` directory for example input programs and their
corresponding output.

Usage
-----
The header file `<compex.h>` provides macros for the use of the compex
attributes. See `compex.h` for details.

Specify `COMPEX_TAG()` on any structure you want metadata to be dumped for. For
example:

    #include <compex.h>

    struct COMPEX_TAG() my_struct {
      int x, y, z;
    };

Metadata will be output for any structure with at least one `COMPEX_TAG()` tag.
The `COMPEX_TAG()` macro expands to nothing when compex is not used.

You can also specify metadata to be attached to a construct using `COMPEX_TAG()`;
simply pass a number of items to `COMPEX_TAG()`. Each should be an integer or a
string literal. For example:

    struct COMPEX_TAG("theAnswer", 42) my_struct {
      // ...
    };

The arguments passed to `COMPEX_TAG()` become entries in a list in the output.
If you specify `COMPEX_TAG()` multiple times for the same element, the
arguments to each such tag are separate lists. A list is not emitted if no
arguments are specified.

For example, if you specified
`COMPEX_TAG() COMPEX_TAG("a", "b", 42) COMPEX_TAG(1, 2, "c")`,
the 'tags' member of the ouptut for the structure would look something like
this:

    tags:
      - - a
        - b
        - 42
      - - 1
        - 2
        - c

You can also use the C++11 attributes directly, but this is not recommended:

    struct [[compex::tag("foo")]] my_struct {
      // ...
    };

Extras
------
The barebones script `script/compex-convert` can be used to convert the YAML
output into JSON, if desired.

Colophon
--------
© 2014 Hugo Landau <hlandau@devever.net>

Licenced under the LGPLv3 or later. See `doc/COPYING`.

Send bug reports or patches to <hlandau@devever.net>.
