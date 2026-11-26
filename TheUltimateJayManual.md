# The Ultimate Jay Manual

# Types

## Built-in

| Type | Description | Example |
|---|---|---|
| `int` | Integer | `int age = 18` |
| `float` | Floating-point number | `float pi = 3.14` |
| `str` | String | `str name = "Jay"` |
| `bool` | Boolean | `bool enabled = true` |
| `char` | Character | `char letter = 'J'` |

## Volatile

The `volatile` keyword declares a variable whose value may change unexpectedly outside the normal execution flow.

Syntax:

```jay
volatile int status = 0
```

Examples:

```jay
volatile bool running = true
volatile int hardware_status = 1
```

The interpreter evaluates variable reads directly from its variable table. The qualifier is recorded, but hardware access, external mutation, memory barriers, and thread synchronization are not implemented.

`volatile` can be combined with built-in types:

```jay
volatile int counter = 0
volatile float temperature = 25.5
volatile bool enabled = true
```

## Variable declarations

Variables can be declared using an explicit type:

```jay
int age = 18
str name = "Jay"
```

Variables can also use type inference:

```jay
let age = 18
let name = "Jay"
```

## Semicolons

Semicolons are optional. Both styles are valid:

```jay
int x = 10
int y = 20
```

```jay
int x = 10;
int y = 20;
```

## Conditions

Use `if` and `else` to select a block. Conditions must evaluate to `bool`:

```jay
int age = 20
if (age >= 18) {
    print("Adult")
} else {
    print("Minor")
}
```

## Loops

A `while` loop repeats while its condition is `true`:

```jay
int count = 0
while (count < 5) {
    print(count)
    count = count + 1
}
```

Use `break` to leave the nearest loop and `continue` to skip to its next condition check:

```jay
int count = 0
while (count < 10) {
    count = count + 1
    if (count == 3) {
        continue
    }
    if (count == 7) {
        break
    }
    print(count)
}
```

`break` and `continue` are only valid inside a `while` loop. Jay 0.5.0 supports user-defined functions and the native standard-library I/O functions `print()`, `putchar()`, `puts()`, and `putnum()`. Arrays and lexical block scopes are not yet implemented.

## Modules

Jay source files can import modules using the module name:

```jay
import math

print(module_value())
```

The import above loads `/lib/jay/modules/math.jmd`. Module names contain letters, digits, and underscores, and must begin with a letter or underscore. The `.jmd` extension is added automatically.

Imports occupy their own line. A trailing semicolon is optional. Module source is expanded at the import location, so declarations and functions become available to the importing program. A module is loaded only once per program; circular imports are reported as errors.

Set the `JAY_MODULE_DIR` environment variable to use a different module directory. This is useful when developing modules without installing them system-wide.


### Unloading modules

Use `umport` to unload a previously imported module:

```jay
import math
print(square(5))
umport math
# Functions and global variables from math are now unavailable.
import math
print(square(6))
```

Unloading removes functions and global variables declared by that module. It does not reverse side effects that already occurred.
