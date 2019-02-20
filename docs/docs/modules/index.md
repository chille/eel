---
permalink: /docs/modules
base: /docs
---

The parts of EEL
================

**Note:** The information provided in this page is not yet fully implemented in EEL. There is an ongoing project to split EEL into the core, compiler and modules.

EEL is divided into a three different parts; the core, the compiler and a set of dynamically loadable modules.

Core
----
Library name: libeelcore

The core is basically the virtual machine (VM) that runs the bytecode provided by the compiler. The core is cross platform and resource efficient.


Compiler
--------
Library name: libeelcompiler

The compiler takes the EEL code as input and converts it into a bytecode that the VM understands.


Modules
-------
All external functions that are not part of the [EEL language](language) is loaded as a separate module. This module could provide things like file system access or similar.


### builtin
Is compiled and linked directly into the core and could not be loaded as an external module. Provides basic functionallity that is considered part of the EEL language.


### loader
Library name: libeelmoduleloader

This contains the mechanic to used to dynamically load modules. The application implementing EEL must manually inject the module loader, or provide its own, to be able to dynamically load modules.


### system
Library name: libeelmodulesystem

getenv(), setenv(), home directory, directory separator, system()/ShellExecute(), etc


### io
Library name: libeelmoduleio

Reading and writing files.


### dir
Library name: libeelmoduledir

Browsing directories.


### math
Library name: libeelmodulemath

Trigonometry, etc.


### dsp
Library name: libeelmoduledsp

Statistics, polynomials, FFT and complex numbers.


### eelium
Library name: N/A

Eelium consists of a set of C functions providing things like SDL and a some usable EEL code. This should be split into more modules in the future.