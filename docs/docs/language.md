---
permalink: /docs/language
base: /docs
---

The EEL Language
================


Introduction
------------
EEL, the Extensible Embeddable Language, is a dynamic typed
language with C-like syntax, built-in high level data types
and automatic memory management. It compiles into bytecode
that runs on a custom virtual machine, and can be extended
with custom data types and native functions (C API) at run
time.



Intended use
------------
EEL is designed for hard real time applications, and allows
memory management and other critical tasks to be handled by
custom code or hooked up to services provided by real time
kernels. Though EEL aims to be generally fast and efficient,
the priority is determinism (ie bounded worst case response
time), as opposed to maximum throughput.

One must realize that a language can only *allow*
applications to respond to input in bounded time, and only
within the limits of the underlying operating system and
hardware. EEL cannot magically fix broken application
designs, nor can it turn general purpose operating systems
into real time operating systems.

What EEL *can* do is add a nice, high level "in-line"
scripting/programming language to a real time system,
without automatically making the system non-deterministic.



Operators and their precedence
------------------------------
Operators in the same block have the same priority.

| Prio |   Operator     | Description                   |
|:----:|:--------------:|-------------------------------|
|   1  | (...)          | Parenthesized expressions     |
|      |                |                               |
|   2  | `(<typename>)` | Type casts                    |
|      |                |                               |
|   3  | `-`            | Unary minus                   |
|      | `typeof`       | Determine type of object      |
|      | `sizeof`       | Determine max index of object |
|      | `~`            | Bitwise NOT                   |
|      |                |                               |
|   4  | `**`           | power       right associative |
|      |                |                               |
|   5  | `%`            | Modulus                       |
|      | `/`            | Division                      |
|      | `*`            | Multiplication                |
|      | `&`            | Bitwise AND                   |
|      |                |                               |
|   6  | `-`            | Subtraction                   |
|      | `+`            | Addition                      |
|      | `|`            | Bitwise OR                    |
|      | `^`            | Bitwise XOR                   |
|      |                |                               |
|   7  | `<<`           | Bitwise left shift            |
|      | `>>`           | Bitwise right shift           |
|      | `rol`          | Bitwise rotate left           |
|      | `ror`          | Bitwise rotate right          |
|      | `><`           | Bitreverse                    |
|      |                |                               |
|   8  | `|<`           | Select lowest value           |
|      | `>|`           | Select highest value          |
|      |                |                               |
|   9  | `<`            | Less than                     |
|      | `<=`           | Less than or equal to         |
|      | `>`            | Greater than                  |
|      | `>=`           | Greater than or equal to      |
|      |                |                               |
|  10  | `==`           | Equal                         |
|      | `!=`           | Not equal                     |
|      |                |                               |
|  11  | `not`          | Boolean NOT                   |
|      |                |                               |
|  12  | `and`          | Boolean AND                   |
|      |                |                               |
|  14  | `or`           | Boolean OR                    |
|      |`      xor     `| Boolean XOR                   |



Built-in types
--------------

| Name       | Description                              |
|------------|------------------------------------------|
| real       | Floating point value (normally 64 bits)
| integer    | Integer value (normally 32 bits)
| boolean    | Boolean value ('false' or 'true')
| typeid     | Type/class ID (integer "magic" value)
| string     | Pooled, hased constant string
| dstring    | Dynamic "Pascal" string/buffer
| vector     | 1D array of real (normally 64 bit) values
| vector_f   | 1D array of single precision values
| vector_d   | 1D array of double precision values
| vector_s8  | 1D array of signed 8, 16 or 32 bit values
| vector_s16 | 
| vector_s32 | 
| vector_u8  | 1D array of unsigned 8, 16 or 32 bit values
| vector_u16 | 
| vector_u32 | 
| array      | 1D array of dynamic typed values
| table      | Hash table with <key, value> items, where 'key' and 'value' are dynamic typed values.


Flow control constructs
-----------------------

### do while
{% highlight C %}
do
	<statement>
while <condition>;

	break [<blockname>];

	continue [<blockname>];

	repeat [<blockname>];
{% endhighlight %}

Repeat &lt;statement> while &lt;condition> evaluates to true.

A 'break' statement will terminate the loop.

A 'continue' statement will skip to the end of
&lt;statement>, into the condition test, and (potentially)
reenter the loop body.

A 'repeat' statement will jump to the start of
&lt;statement>, bypassing the loop condition test.



### do until
{% highlight C %}
do
	<statement>
until <condition>;

	break [<blockname>];

	continue [<blockname>];

	repeat [<blockname>];
{% endhighlight %}

Repeat &lt;statement> until &lt;condition> evaluates to true.

A 'break' statement will terminate the loop.

A 'continue' statement will skip to the end of
&lt;statement>, into the condition test, and (potentially)
reenter the loop body.

A 'repeat' statement will jump to the start of
&lt;statement>, bypassing the loop condition test.



### for
{% highlight C %}
for <variable> = <start>, <end>[, <increment>]
	<statement>

	break [<blockname>];

	continue [<blockname>];

	repeat [<blockname>];
{% endhighlight %}

Execute &lt;statement> once for each value, written to
&lt;variable>, in [&lt;start>, &lt;end>] with &lt;increment> steps.
&lt;increment> defaults to 1 if not specified. &lt;increment> can
be negative. Iteration is inclusive, which means that this
construct will always execute &lt;statement> at least once,
with &lt;variable> == &lt;start>, regardless of the other
parameters.

A 'break' statement inside &lt;statement> will terminate
the loop.

A 'continue' statement will end the current iteration,
updating the iteration variable and testing for the end
condition before (potentially) reentering the loop body.

A 'repeat' statement will jump to the start of
&lt;statement>, bypassing the loop condition test and without
changing the iteration variable.



### **TODO:** for x in y
{% highlight C %}
for <variable> in <explist>
	<statement>

	break [<blockname>];

	continue [<blockname>];

	repeat [<blockname>];
{% endhighlight %}

Execute &lt;statement> once for each item in &lt;explist>.
The current item is assigned to &lt;variable> for each
iteration. &lt;explist> can be one or more values or indexable
objects, meaning that this is actually a 2D iteration; one
dimension to iterate over the expressions, and one to
iterate through any indexable objects returned from those
expressions.

A 'break' statement inside &lt;statement> will terminate
the loop.

A 'continue' statement will end the current iteration,
moving to the next item in &lt;explist> and testing for the
end condition before (potentially) reentering the loop
body.

A 'repeat' statement will jump to the start of the loop
body, to reiterate with the same item.



### if
{% highlight C %}
if <condition>
	<statement1>
[else if <condition>
	<statement2>
[else
	<statement3>]]
{% endhighlight %}



### **TODO:** route
{% highlight C %}
route <expression>
	{
		...
	}

	  in <explist>
		<statement>

	  not in <explist>
		<statement>

	  else
		<statement>

	  always
		<statement>

		break [<blockname>];

		repeat [<blockname>];
{% endhighlight %}

Scan the explists for matches to &lt;expression>, executing
the statement of each 'in' section that has a match, and
each 'not in' section that does *not* have a match. The
section explists may contain both constant and variable
expressions. The statement of a matching section will only
be executed once, even if there are multiple hits in its
explist.

An 'else' section's statement is executed only if there
was no hit in any other section before that 'else' section.
There can be more than one 'else' section, and they can be
placed anywhere	in the list, just like '[not] in' sections.

An 'always' section's statement is always executed,
unless the whole statement is terminated with 'break'.
There can be multiple 'always' sections.



### **TODO:** switch
{% highlight C %}
switch <expression>
{
  case <explist>
	<statement>
  [case <explist>
	<statement>
  [...]]
  [else
	<statement>]
}

	break [<blockname>];
{% endhighlight %}

Scan the explists of the 'case' sections until one is found
that contains a match to &lt;expression>, then execute the
stametent of that section. The explists must contain only
constant values. (The compiler should verify that there is
only one occurence of each case value in the entire
'switch' statement.)

The 'else' statement is executed only if no other
section was hit.



### try
{% highlight C %}
try
	<statement1>
[except
	<statement2>]

	throw <expression>;

	retry;			(Only in 'except' blocks)
{% endhighlight %}

Try to execute &lt;statement1>.

If an exception is thrown, that is not caught on lower
levels, execution continues in the statement of the
'except' block.

A 'retry' statement can be issued inside an 'except'
block to rerun the 'try' block of the same try...except
statement.



### while
{% highlight C %}
while <condition>
	<statement>

	break [<blockname>];

	continue [<blockname>];

	repeat [<blockname>];
{% endhighlight %}



While &lt;condition> evaluates to true, execute &lt;statement>.
&lt;statement> may use a 'break' statement to terminate the
loop at any time, or a 'continue' statement to instantly
restart the loop. 'continue' results in &lt;condition> being
tested before (potentially) reentering &lt;statement>. A
'repeat' statement instantly restarts the loop, bypassing
the condition test.



Declarations and definitions
----------------------------
Function/procedure arguments:

| Syntax                            | Description             |
|-----------------------------------|-------------------------|
| `p`                               | No arguments            |
| `p[optional_args]`                | Only optional arguments |
| `p<tuple_args>`                   | Only argument tuples    |
| `p(required_args)`                | Only required arguments |
| `p(required_args)[optional_args]` | Required + optional     |
| `p(required_args)<tuple_args>`    | Required + tuples       |

Arguments are passed as a 1D array of values, which is what
dictates how required, optional and tuple arguments can be
combined. Basically, it has to be possible to determine
which arguments are specified, and which argument is which,
by simply looking at the declaration and the total argument
count. Thus, you cannot mix optional and tuple arguments,
and optional or tuple arguments must come after any
required arguments.

`procedure name<argdeflist>`

`function name<argdeflist>`

Procedures have no return values, whereas functions return
exactly one value. (True multiple returns are dangerous and
not really needed, considering that one can just as easily
return a vector, array or table as a single return value.)


<!--
Reserved Words
--------------
The following keywords are reserved.

* and
* arguments
* array
* as
* boolean
* break
* case
* continue
* do
* dstring
* else
* end
* except
* exception
* export
* false
* for
* function
* if
* import
* in
* include
* integer
* local
* module
* nil
* not
* or
* procedure
* real
* repeat
* retry
* return
* rol
* ror
* shadow
* sizeof
* specified
* static
* string
* switch
* table
* throw
* true
* try
* tuples
* typeid
* typeof
* until
* upvalue
* vector
* vector_d
* vector_f
* vector_s8
* vector_s16
* vector_s32
* vector_u8
* vector_u16
* vector_u32
* while
* xor
-->