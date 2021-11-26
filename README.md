Winter
======
Winter is a high performance functional programming language, designed to be embedded in C++ applications.

It can run on CPUs via LLVM JIT compilation, and a subset of it can run on GPUs via translation to OpenCL.

It serves as the shading language and implementation language for parts of Indigo Renderer (https://www.indigorenderer.com/) and also for Chaotica Fractals (https://www.chaoticafractals.com/).

# Types

## bool

Boolean values - e.g. ```true``` and ```false```.

## int

Integer types, usually corresponding to machine register widths.

```int``` is a 32-bit signed integer, with values ranging from 
-2147483648 to 2147483647 (inclusive).

```uint``` is an unsigned 32-bit integer, with values ranging from 
0 to 4294967295 (inclusive).

Integer types can also be written with a suffix representing the bit width, for example

```int32```, ```int64```, ```uint16``` etc.

Currently supported bit widths are 16, 32, and 64.

## float

32-bit floating point IEEE 754 format number.

Winter does not provide any exceptions from the IEEE 754 standard however.

## double

64-bit double-precision floating point IEEE 754 format number.

## real

An alias (another name) for ```float``` or ```double```, depending on the runtime configuration option
```real_is_double```.

## char

A Unicode character.

## string

A string of Unicode characters.

## array

A fixed-length array of N items of some type, for example:

```array<int, 10>```

Is an array of 10 integers.


## varray

A variable-length array items of some type, for example:

```varray<int>```

Is an array of integers.  The variable-length aspect means the length of the varray
is not known at compile time, but only at run time.


## vector

A fixed-length vector of N items of some type, for example:

```vector<float, 4>```

Is a vector of 4 floats.  Vectors are similar to small arrays.  They are designed to be stored
in vector registers and be operated on by SIMD instructions when possible.

The number of elements must be > 0 and < 128.

## tuple

A tuple (short ordered list) of types, for example

```tuple<int, float>```

Is a tuple with the first element type int and the second float.


## function

A function that takes zero or more arguments and returns a value.

```function<arg0, arg1... argN, return_type>```

For example, the square function, which maps a float to a float, would have type

```function<float, float>```


## opaque / voidptr

A type of which values are not operable on by the program.  Used for data (for example pointers)
being passed through the program.

------

# Literals

In general, all Winter values can be be written with literals, e.g. can be written directly in the program.

## bool literal

```false``` and ```true```.

## int literal

Int literals can be written in hexadecimal format (base 16) or decimal format (base 10).

Hexadecimal int literals have a ```0x``` prefix, and may contain the lower or upper case 
letters ```a``` to ```f```, for example

```0xDEADBEEF```

Int literals may have an ```i``` or ```u``` suffix followed by the desired bit width for the value,
for example

```100i16``` is a 16-bit signed integer with value 100,

```1152921504606846976u64``` is a 64-bit unsigned integer with value 1152921504606846976.

The bit width must be 16, 32, or 64.

## float literal

These have a similar syntax to C++:

```[sign] [digits] [.digits] [ { e | E }[sign]digits]f```

For example

```1.23f```

```1.23e45f```

```1.23e-45f```

Floating-point literals without an ```f``` suffix are single-precision if the floating_point_literals_default_to_double configuration option is false.

## double literal

These have a similar syntax to C++:

```[sign] [digits] [.digits] [ { e | E }[sign]digits]d```

For example

```1.23d```

```1.23e45d```

```1.23e-45d```

Floating-point literals without a ```d``` suffix are double-precision if the 
floating_point_literals_default_to_double configuration option is true.
By default floating_point_literals_default_to_double is true, so by default
floating-point literals are double-precision.

## char literal

Character literals are enclosed with single quotes, like so:

```'a'```

Various escape sequences are also allowed, much like C++:

```'\''``` is a single-quote character

```'\\'``` is a single backslash character

```'\n'``` is a newline character

```'\r'``` is a carriage return character

```'\t'``` is a tab character

Unicode characters can also be written with the numeric code point:

```'\u{7FFF}'```

Where the hexadecimal value in the curly braces gives the Unicode code point.

## string literal

Character literals are enclosed with double quotes, like so:

```"hello"```

The same escape sequences beginning with the backslash character, as for char literals are also allowed in strings, and in addition the 
escape sequence

```\"```

for example

```"hello \n\n  \t  \u{7FFF}"```

## Array Literal

An array literal is written using square brackets, with an ```a``` suffix.

```
[1, 2, 3]a
```
Array literals can also be written with one element, and an integer N at the end of the suffix.  In this case an array
of N elements, all copies of the first element is created.  For example:

```
[1.0]a100 # Creates an array of 100 elements, each with the value 1.0
```

NOTE: Syntax for array literals may change.

## VArray Literal

A varray (variable-length array) literal is written using square brackets, with a ```va``` suffix.

```
[1, 2, 3]va
```

Varray literals can also be written with one element, and an integer N at the end of the suffix.  In this case a varray
of N elements, all copies of the first element is created.  For example:

```
[1.0]va100 # Creates a varray of 100 elements, each with the value 1.0
```

NOTE: Syntax for varray literals may change.

## Vector Literal

A vector literal is written using square brackets, with a ```v``` suffix.

```
[1, 2, 3]v
```

Vector literals can also be written with one element, and an integer N at the end of the suffix.  In this case a vector
of N elements, all copies of the first element is created.  For example:

```
[1.0]v8 # Creates a vector of 8 elements, each with the value 1.0
```

NOTE: Syntax for vector literals may change.

## Tuple Literal

An tuple literal is written using square brackets, with a ```t``` suffix.

```
[1, 2, 3]t
```

Tuple literals can also be written using parentheses containing more than 
one element, for example

```
(1, 2)
```

## Struct literals

A structure value can be created using the name of the structure as a function, followed
by a comma-separated list of values to use for structure members.  

For example

```
struct Complex { float re, float im }

def addComplex(Complex a, Complex b) : Complex(a.re + b.re, a.im + b.im)
```
In this example addComplex uses a constructor/literal call to construct a new Complex value and
return it from the function.


## Function literals

Function literals, also called lambda expressions, are a way to define a function without 
giving it a name.

For example, to define a function that is the same as the square function:

```\(float x) float : x*x```

The backslash is designed to be reminiscent of the lambda symbol.

The return type can also be omitted, in which case it will be deduced:

```\(float x) : x*x```

It is also permissable to use the following arrow syntax:

```\(float x) -> x*x```

Function literals can be used anywhere a function expression is expected, and the result
of a function expression can be 'called' like a normal function:

```
function<float, float> square = \(float x) -> x*x

square(3.0f) # returns 9.0f
```
or
```
(\(float x) -> x*x)(3.0f) # returns 9.0f
```


## If expression

If expressions are written like so:

```
if cond then a else b
```
for example

```
if x < 1 then y + 2 else z * 2
```
The `then` keyword may be omitted in some cases, 
however its omission may result in syntatic ambiguities in some cases, in which case it is required.

An example without it:

```
if some_bool 
    y + 2 
else 
    z * 2
```

## Let blocks

Let blocks introduce one or more named variables into an expression, for example

```
let
	a = 5
in
	a + 10
```
will return the value 15.

Multiple let clauses can be listed after each other separated just by whitespace, for example

```
let
	a = 5
	b = 10
in
	a + b
```

The type of the introduced variables can also be explictly written, for example

```
let
	float x = 1.0f
in
	x + 2.0f
```

The right side of the ```=``` can be an arbitrary expression, for example

```
let
	a = f(1, 2) + 3
in
	a + b
```

## Function definition

Functions are defined using the ```def``` keyword, for example

```def square(float x) float : x*x```

the parts of the definition are, from left to right:

* The ```def``` keyword that begins the definition
* The name of the function, in this case 'square'
* ```(float x)```: A comma-separated list of arguments with their types preceding them, enclosed by parentheses 
* ```float```: The return type.  Explicitly writing the return type like this is optional. 
 If not explicitly written it will be deduced by the compiler.
* ```:``` A colon character
* The function body expression, in this case ```x*x```

An example of leaving off the function return type:

```def square(float x) : x*x```

## Function overloading

It is possible to define two different functions with the same name, but with differing argument types.
The correct function to call will be determined at compile time based on the types of the arguments.

For example:

```
def square(float x) float : x*x
def square(int x) int : x*x

square(1.23f) # calls the float -> float function
square(1) # calls the int -> int function
```

## Struct definition

Struct values bundle a few values together into a single value.  
They are similar to C++ structs.

```struct Complex { float re, float im }```

Structs are defined with the ```struct``` keyword, followed by the name of the structure.
Then inside the curly braces, a comma-separated list of structure members is written.



## Accessing a structure element

A structure element can be accessed by writing a dot, and then the name of the structure member, for example

```
struct Complex { float re, float im }

def getRealPart(Complex c) : c.re
```

## Accessing a tuple element

A tuple element can be accessed using square bracket indexing syntax:

```
pair = (a, b)
pair[0] # returns a
```
The expression in the square brackets must be computable at compile time.

## Destructuring assignment

Destructuring assignment allows the elements of a tuple to be assigned to more than one variable in one line of code.
For example

```
a, b = (3, 4)  # Assigns 3 to a and 4 to b.
```

Destructing assignment can be used with tuples returned from functions, for example:

```
def f(float x) tuple<float, float> : (x + 1.0f, x + 2.0f)

let a, b = f(x)  # Assigns x + 1.0f to a, x + 2.0f to b.
```

## Named constants

Named constants can be used to define constant values in a program.
Unlike variables they can only be written at file scope, not in a function.

```N = 1000```

```MY_CONST = f(a, b) + 5```

The right side of the named constant definition can be an arbitrary expression.

The names need not be in upper case.

Named constants can also be defined with an explicit type, for example

```int N = 1000```


## Ternary conditional operator

This behaves much like the ternary conditional operator in C++:

```cond ? a : b```

Returns a if cond is true, otherwise b.


## Arithmetic and other operaters

These operators are generally defined in the same way as for C++.

The following operators are supported:

`+`: addition

`*`: multiplication

`-`: (binary, e.g. two operand) subtraction

`/`: division

`-`: unary minus

`==`: equality comparison

`!=`: inequality comparison

`<`: less than

`>`: greater than

`<=`: less than or equal to

`>=`: greater than or equal to

`!`: logical negation

`|`: bitwise OR

`&`: bitwise AND

`^`: bitwise XOR

`<<`: bitwise left-shift

`>>`: bitwise right-shift


## Operator overloading

It is possible to define your own operators such as +, - etc... for your own structure types.

This is done by defining a function such as op_add.  For example:

```
struct s { float x, float y } # Define a structure

def op_add(s a, s b) : s(a.x + b.x, a.y + b.y) # Define addition operator for structure.

s(1, 2) + s(3, 4) # You can now use the '+' operator to add your structures.
```

The operators that can be overloaded are:


`+`: ```def op_add(S a, S b) S```

`*`: ```def op_add(S a, S b) S```

`-`: ```def op_sub(S a, S b) S```

`/`: ```def op_div(S a, S b) S```

`-`: ```def op_unary_minus(S a) S```

`==`: ```def op_eq(S a, S b) S```

`!=`: ```def op_neq(S a, S b) S```

`<`: ```def op_lt(S a, S b) S```

`>`: ```def op_gt(S a, S b) S```

`<=`: ```def op_lte(S a, S b) S```

`>=`: ```def op_gte(S a, S b) S```



------


# Built-in Functions

## Maths Functions

These functions are mostly the same as in the C and C++ standard libraries.
They never throw any kind of exception however, or set any kind of flag.

### floor

float -> float, double -> double, vector<float, N> -> vector<float, N>, vector<double, N> -> vector<double, N>

### ceil

float -> float, double -> double, vector<float, N> -> vector<float, N>, vector<double, N> -> vector<double, N>

### sqrt

float -> float, double -> double, vector<float, N> -> vector<float, N>, vector<double, N> -> vector<double, N>

### sin

float -> float, double -> double, vector<float, N> -> vector<float, N>, vector<double, N> -> vector<double, N>

### cos

float -> float, double -> double, vector<float, N> -> vector<float, N>, vector<double, N> -> vector<double, N>

### exp

float -> float, double -> double, vector<float, N> -> vector<float, N>, vector<double, N> -> vector<double, N>

### pow

(float, float) -> float, (double, double)-> double, 
(vector<float, N>, vector<float, N>) -> vector<float, N>, 
(vector<double, N>, vector<double, N>) -> vector<double, N>

### ```_frem_```

(float, float) -> float, (double, double)-> double, 
(vector<float, N>, vector<float, N>) -> vector<float, N>, 
(vector<double, N>, vector<double, N>) -> vector<double, N>

Same semantics as the C/C++ function fmod.  NOTE: Name of this may change.

### log

float -> float, double -> double, vector<float, N> -> vector<float, N>, vector<double, N> -> vector<double, N>


### abs

float -> float, double -> double, vector<float, N> -> vector<float, N>, vector<double, N> -> vector<double, N>

### tan

float -> float, double -> double

### asin

float -> float, double -> double

### acos

float -> float, double -> double

### atan

float -> float, double -> double

### sinh

float -> float, double -> double

### asinh

float -> float, double -> double

### cosh

float -> float, double -> double

### acosh

float -> float, double -> double

### tanh

float -> float, double -> double

### atanh

float -> float, double -> double

### atan2

(float, float) -> float, (double, double) -> double


### truncateToInt

float -> int, double -> int, vector<float, N> -> vector<int, N>, vector<double, N> -> vector<int, N>

Removes the digits to the right of the floating point, then returns the closest integer to that value.

### sign

float -> float, double -> double, vector<float, N> -> vector<float, N>, vector<double, N> -> vector<double, N>

```
sign(x) :
if x is < 0.0, returns -1.0,
if x = -0.0, returns -0.0
if x = +0.0, returns +0.0
if x is > 0.0 returns 1.0
```

### mod

float -> float, double -> double, int -> int

Computes the Euclidean modulo of the argument. For example

```
mod(2.3f, 1.f)  # = 0.3f

mod(-2.3f, 1.f)  # = 0.7f
```

The result of mod(x, y) will be in [0, y) for positive y.


### isFinite

float -> bool, double -> bool

Returns true if the argument is finite, e.g. not infinite nor NaN, and false otherwise.

For example:

```
isFinite(1.23f) # = true

isFinite(1.0f / 0.f) # = false
```

### isNAN

float -> bool, double -> bool

Returns true if the argument is a Not-A-Number value (NAN), and false otherwise.


```
isNAN(1.23f) # = false

isNAN(1.0f / 0.f) # = false (argument is +Inf)

isNAN(0.0f / 0.f) # = true (argument is NAN)
```

### toFloat

int -> float, vector<int, N> -> vector<float, N>

Returns the closest representable float to the input value.

### toDouble

int -> double, vector<int, N> -> vector<double, N>

Returns the closest representable double to the input value.

### toReal

int -> real, vector<int, N> -> vector<real, N>

Returns the closest representable real (may be float or double) to the input value.

### toInt64

int32 -> int64, vector<int32, N> -> vector<int64, N>

Returns a 64-bit integer with the same value as the input value.



## Collection functions

### length

array<e, N> -> int64, varray<e, N> -> int64, tuple<...> -> int64, vector<e, N> ->int64

Returns the length of the array, or num elements of the vector or tuple.

### elem

Returns the i-th element of the array, vector or tuple.
For example

```elem([10, 20, 30]a, 1) # returns 20```

## String functions

### elem

(string, int64) -> char

Returns the i-th character of the string
For example

```elem("hello", 1i64) # returns 'e'```


### concatStrings

Concatenates two strings (joins them together).

For example:

```concatStrings("a", "b") # returns "ab"```

### length

string -> int

Returns the number of characters in the string

## Char functions

### codePoint

char -> int

Returns the Unicode code point for the Unicode character.

For example:

```codePoint('a') # returns 0x61 (Ascii and Unicode code point for 'a')```

```codePoint('\u{393}') # returns 0x393 ```

### toString

char -> string

Returns a string with a single character, equal to the argument.

```toString('a') # returns "a"```



-----
## Higher Order Functions

### fold

```fold(function<State, T, State> f, array<T> array, State initial_state) State```

The first argument to fold is a function that takes the current state, the current element of the collection,
and returns a new state.

The second argument to fold is the collection being operated on.

The third argument is the initial state.

For example:

```
def sum(int running_sum, int elem) int :
	running_sum + elem

fold(sum, [0, 1, 2, 3, 4, 5]a, 0) # returns 15
```

### iterate

``` iterate(function<State, int, tuple<State, bool> > f, State initial_state)```

The first argument to iterate is a function that takes the current state, 
the current iteration index, and returns a pair tuple containing the new state
and a boolean which signals whether to keep iterating.

The second argument to iterate is the initial state.

For example, this usage of iterate effectively counts to 100:

```
def f(int current_state, int i) tuple<int, bool> :
	if i >= 100
		(current_state, false) # false means break
	else
		(current_state + 1, true) # true means continue

iterate(f, 0) # returns 100
```
And is roughly equivalent to the C code
```
int current_state = 0;
int i = 0;
while(1)
{
	if i >= 100
		break;
	else
	{
		current_state = current_state + 1;
		i++;
	}
}
```


There is also an alternative form of iterate that allows extra arguments to be passed to the 
first-arg function:

``` iterate(function<State, int, inv_data_0, inv_data_1 ... inv_data_N, State>, State, inv_data_0, inv_data_1 ... inv_data_N)```

In this example the extra 'step' argument is used:

```
def f(int current_state, int i, int step) tuple<int, bool> :
	if i >= 100
		(current_state, false)
	else
		(current_state + step, true) # Add step to the current state

iterate(f, 0, 2) # Use 2 as the step.  Returns 200.
```
