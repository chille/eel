---
permalink: /download
base: /download
---

Get EEL
=======

(The instructions below apply to EEL 0.3.6. EEL 0.3.7 will use CMake instead of Autotools, and eelbox has been integrated as a set of optional modules.)

Installing, Un*x-like systems
Unpack to wherever you keep sources.
In the top dir:

	'./configure'
	'make'
As root: (or add options for user install)

	'make install'
In the 'eelbox' subdir:

	'./configure'
	'make'
	'make install'
Testing
In the 'test' subdir;
To test EEL itself:

	'eel test'
VM benchmarks:

	'eel bench'
Building against the EEL library:

	'./configure'
	'make'
	'./eeltest'
In the 'eelbox' subdir:

	'eelbox test'
(Some of the tests need to be stopped by closing the window, or hitting the escape key.)

	'eelbox chat'
(Tip: Fire up two instances!)

Requirements
------------
EEL should be reasonably easy to build on any platform with a C compiler. To do interesting stuff, SDL 1.2 with SDL_image and SDL_net, and Audiality 2 are nice to have too.

Licensing
---------

EEL was originally released under the [GNU LGPL](http://www.gnu.org/licenses/lgpl-2.1.html), which applies up to and including version 0.3.6. As of version 0.3.7, EEL is available under the zlib license:

> Copyright 2011-2014 David Olofson david@olofson.net
>
> This software is provided 'as-is', without any express or implied warranty. In no event will the authors be held liable for any damages arising from the use of this software.
>
> Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:
>
> 1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
>
> 2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
>
> 3. This notice may not be removed or altered from any source distribution.