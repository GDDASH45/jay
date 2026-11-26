# Jay

Jay is an interpreted programming language implemented in C.

## Build

```sh
make
```

The interpreter is created at `build/jay`.

## Run

Run a source file:

```sh
./build/jay examples/hello.jay
```

Start the interactive shell:

```sh
./build/jay
```

Print version or usage information:

```sh
./build/jay --version
./build/jay --help
```

## Current language features

- Explicit declarations: `int count = 10`, `str name = "Jay"`
- Inferred declarations: `let count = 10`
- Built-in types: `int`, `float`, `str`, `bool`, and `char`
- `volatile` declaration qualifier
- Optional semicolons and newline-separated statements
- Arithmetic, comparisons, boolean operators, and string concatenation
- Variable assignment and the `print(...)` built-in
- Comments beginning with `#`

Jay is under active development. Functions, control-flow statements, arrays, and user-defined types are not implemented yet.

See [The Ultimate Jay Manual](TheUltimateJayManual.md) for the language notes.
