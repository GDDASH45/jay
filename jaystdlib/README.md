# Jay Standard Library

The Jay standard library is implemented in C and is built into the interpreter.
Its native implementations live in this directory, separately from the parser
and interpreter core.

## I/O

| Function | Description |
| --- | --- |
| `print(...values)` | Print zero or more values, then a newline |
| `putchar(char_or_int)` | Write one character without adding a newline |
| `puts(str)` | Write a string followed by a newline |
| `putnum(int)` | Write an integer followed by a newline |

Example:

```jay
print("Jay standard library")
putchar('J')
puts("ay")
putnum(42)
```

The native library dispatcher is declared in `jaystdlib.h`. Add new standard
library implementations here rather than adding more built-ins to the parser.

SPDX-License-Identifier: GPL-2.0-only
