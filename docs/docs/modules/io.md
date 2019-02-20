---
permalink: /docs/modules/io
base: /docs
---

"io" - The EEL Built-in IO Library
==================================


Introduction
------------
The built-in IO library basically corresponds to C's stdio.
It contains types and functions for basic file/stread I/O.



### class file

```eel
class file
```

position:	Current position (R/W)

```eel
class memfile
```

position:	Current position (R/W)
buffer:		Memory buffer (dstring)



### stdin, stdout, stderr

```eel
function stdin;
function stdout;
function stderr;
```

Returns the standard input, output and error files
respectively, if available, otherwise nil.



### open()

```eel
function open(path)[mode];
```

Opens file "path" in "mode". Arguments are identical to those
of the C function 'fopen', except that they are EEL strings
rather than C strings.



### close()
```eel
procedure close(f);
```

Close file 'f'.



### read()

```eel
function read(f);
function read(f, length);
function read(f, type);
```

Reads from file 'f'. The (f) version reads one byte and
returns it's value (treating the byte as an unsigned 8 bit
integer) as an EEL integer.

The (f, length) version reads 'length' bytes and returns
them as a dstring.

The (f, type) version reads from 'f', deserializing the
data into one item of 'type', and returns a value of that
type. The number of bytes read depends on 'type', which must
be a type with (de)serialization medamethods.



### write()
```eel
function write(f)<data>
```

Write 'data' (any number of arguments of types that can be
serialized) to file 'f'. Returns the number of bytes written.