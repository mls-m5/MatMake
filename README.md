Matmake
======================
A fast and simple but powerful dependency calculating build system.
It currently works with unix, but is planned to be expanded to win and osx in the future.

Design goals:
------
* Automatic dependency checks
	* Source files is parsed to find dependencies
* Single command builds
	* Configure step is and should always be optional
* Fast to setup new c++ projects
	* one single command to generate a start for a project
* Should handle when the project and complexity grows
* Simple beautiful syntax
	* A minimal amount of function names is used
	* Built in commands are lower case
	* Built in syntax, and most commands should be available through `matmake --help` 
	* Example commands is available through `matmake --example` 
* Fast compile times.
	* Standard is to compile multi threaded
	* Dependency checks is similar to makefiles
* Helpful error messages
* Possibility to be self contained
	* Can be included in project without the need to install anything
* Include sources with wildcard
	* Include whole directories (eg src/\*.cpp or recursively src/\*\*.cpp)
	* (Single file includes is of course supported as well.)
	



Getting started
==========

If you are reading this from a web browser
----------------------------------------
```sh
git init
git submodule add https://github.com/mls-m5/matmake.git
make -C matmake/ init-project
```

This adds the matmake buildsystem to your repository. To build your project you invoke make with "make" as usual.
New files are recognized Automatically.

If you have matmake installed on your system
----------------------------------------
Init project in current folder

```sh
matmake --init
```

or in another folder


```sh
matmake --init    path/for/project
```

Install matmake on your system (globaly) on Linux
-------------------------------------------
```sh
cd /tmp/
git clone https://github.com/mls-m5/matmake.git
cd matmake
make install
```

On windows
-------------
With clang
Make sure to be in a developer console

```sh
path/to/clang++.exe -Isrc -std=c++17 -fexceptions src/matmake.cpp -o matmake.exe
```

How does it look?
==========

#### A very simple Matmakefile
compiles to executable `program`:

```bash
config += c++14 Wall      # configurations converted to compile flags
program.src += src/*.cpp  # include all cpp files in folder src to target program
```

#### More complex examples.

Heres how to compile the the SDL2 (https://www.libsdl.org/download-2.0.php) source code to a so-file:

```bash
sdl.includes = include

sdl.src +=                 # multiline arguments start line with whitespace
	src/timer/unix/*.c 
	src/*.c src/stdlib/*.c src/atomic/*.c src/power/*.c
	src/audio/*.c src/haptic/*.c src/render/*.c src/video/*.c src/timer/*.c
	src/thread/*.c src/render/software/*.c
	src/cpuinfo/*.c src/render/opengl/*.c
	src/joystick/*.c src/libm/*.c src/events/*.c src/file/*.c
	src/thread/generic/*.c src/video/x11/*.c src/video/yuv2rgb/*.c
	src/timer/dummy/*.c src/haptic/dummy/*.c src/video/dummy/*.c 
	src/audio/dummy/*.c  src/joystick/dummy/*.c

sdl.libs += -pthread -ldl  # argument at link time

sdl.out = shared SDL2      # sets output to be a library
                           # -fPIC is automatically added for g++ and clang
sdl.dir = bin              # set output directory

```

Compile Bullet Physics (https://github.com/bulletphysics/bullet3):

```bash

bullet.includes +=
	include
	src

bullet.define += BT_USE_DOUBLE_PRECISION

bullet.src +=
	src/BulletDynamics/**.cpp #recursive search
	src/BulletCollision/**.cpp
	src/LinearMath/**.cpp

bullet.libs += -pthread -ldl

bullet.out = shared bullet # creates a so file (linux) or dll (windows)
```

#### Inheritance:
Create several projects with shared settings

```bash

# global settings are inherited by all targets by default
flags += -Wall             # global c++ and c flags
config += c++14 Wall       # configuration of the project

main.src = src/**.cpp      # ** means recursive search

othertest.inherit = main   # copies all settings from main to new target
othertest.flags += -DTEST  # add some new flags to the project
othertest.cpp = clang++    # specify compiler
othertest.dir = otherbin   # set a new directory for target

```

Future goals:
-----
* Save settings to local .matmake file between builds
	* If you want to set debugging or special settings to the project
	* Save all dependency information in a single file
* Platform agnostic
	* Remove need to manually handle compile flags
* Generate projekt files for platforms that are not terminal friendly
	* Visual studio projects


Why create a new build system?
=========
There is very many build systems out there. These are some dealbreakers for me in other build systems.

* CMake
	* "Globbing" is not recomended, and when used, build does not check for new files.
	* Syntax is ugly
* Meson
	* Does not support wildcard selection of files
* Ninja
	* Not meant to be written by hand
* Scons
	* Slow for big projects
	* Requires python
* Autotools
	* To complex
	* Only for linux (in practice)



