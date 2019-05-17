Matmake
======================
A fast and simple but powerful dependency calculating build system.
It currently works with unix, but is planned to be expanded to win and osx in the future.

Design goals:
------
* Automatic dependency checks
	* Source files is parsed to find dependencies
* Single command builds
	* Configure step is and should allways be optional
* Fast to setup new c++ projects
	* one single command to generate a start for a project
* Should handle when the project and complexity grows
* Simple beautiful syntax
	* A minimal amount of function names is used
	* Built in commands are lower case
	* Built in syntax, and most commands should be available through `matmake --help'
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
New files are recagnized Automatically.

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

Install matmake on your system (globaly)
-------------------------------------------
```sh
cd /tmp/
git clone https://github.com/mls-m5/matmake.git
cd matmake
make install
```


To use the older makefile based system "matdep"
----------------------------------------------
In the example below the variable CPP_FILES contains names to all source files that you want to include in the project
in the script above all the source files is located in a folder named "src"
An example of content of makefile can be found in Makefile-example.txt


```sh
git init
git submodule add https://github.com/mls-m5/matmake.git
make -C matmake/ init-matdep-project
```

And matmake will create a simple matmake project for you. You never have to change anything inside the matmake folder, all the other files belongs to your project.



How does it look?
==========

#### A very simple Matmakefile
compiles to executable `program`:

```
flags += -std=c++14       # global flags added to all targets
program.src += src/*.cpp  # include all sources in src
program.exe = %           # set the output to the same name as the target (this line can be omitted)
```

#### More complex examples.

Heres how to compile the the SDL2 (https://www.libsdl.org/download-2.0.php) source code to a so-file:

```
sdl.flags = -Iinclude  -fPIC

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

sdl.dll = SDL2             # sets output to be a library
sdl.dir = bin              # set output directory

```

Compile Bullet Physics (https://github.com/bulletphysics/bullet3):

```
flags = -Iinclude  -fPIC
flags += -Isrc/

dynamics.src +
	src/BulletDynamics/**.cpp #recursive search
	src/BulletCollision/**.cpp
	src/LinearMath/**.cpp

dynamics.libs += -pthread -ldl

dynamics.dll = bullet3
```

#### Inheritance:
Create several projects with shared settings

```

# global settings are inherited by all targets by default
flags += -Wall             # global c++ and c flags
cppflags += -std=c++14     # global flags for c++ only

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



