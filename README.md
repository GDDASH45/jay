# Jay

Jay is an interpreted programming language implemented in C and licensed under GPL-2.0.

## Build

```sh
make
```

The interpreter is created at `build/jay`.

## Run

Run a source file with `jay examples/hello.jay`, or start the interactive shell with `jay`.

Print version or usage information with `jay --version` and `jay --help`.

## Language features

- Explicit declarations: `int count = 10`, `str name = "Jay"`
- Inferred declarations: `let count = 10`
- Built-in types: `int`, `float`, `str`, `bool`, and `char`
- `volatile` declaration qualifier
- Optional semicolons and newline-separated statements
- Arithmetic, comparisons, boolean operators, and string concatenation
- Variable assignment and the `print(...)` built-in
- `if` / `else` conditional blocks
- `while` loops
- `break` and `continue` inside loops
- Comments beginning with `#`

Run the test suite with `make test`.

See [The Ultimate Jay Manual](TheUltimateJayManual.md) for language details.

## Debian package

Install the packaging tools and build a binary package:

```sh
sudo apt install debhelper
dpkg-buildpackage -us -uc -b
```

The resulting `jay_0.2.0-1_amd64.deb` is written to the parent directory.
