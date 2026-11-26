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

A volatile variable must be read from its underlying storage whenever it is accessed, rather than relying on a cached value.

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

Semicolons are optional.

Both styles are valid:

```jay
int x = 10
int y = 20
```

```jay
int x = 10;
int y = 20;
```