Matmake
======================
A simple dependency calculating system for makefiles (in linux)

In the example below the variable CPP_FILES contains names to all source files that you want to include in the project

in the script above all the source files is located in a folder named "src"

An example of content of makefile can be found in Makefile-example.txt

or just run ```make -C matmake/ init-project``` to get a standard project setup.


If you are reading this from a web browser
----------------------------------------
```sh
git init
git submodule add https://github.com/mls-m5/matmake.git
make -C matmake/ init-project
```

If you have matmake installed on your system
----------------------------------------
```sh
matmake --init   [optional directory]
```


To use the older makefile based system "matdep"
----------------------------------------------
```sh
git init
git submodule add https://github.com/mls-m5/matmake.git
make -C matmake/ init-matdep-project
```

And matmake will create a simple matmake project for you. You never have to change anything inside the matmake folder, all the other files belongs to your project.


Setup cmake project (Alternative to makefile + matmake)
-----------------
When using cmake you do not need the matmake project after the initial setup.

```sh
git init
git submodule add https://github.com/mls-m5/matmake.git
make -C matmake/ init-cmake-project
```


###To refresh cmake files (for example when adding or removing files
```
make -C matmake/ refresh-cmake-files
```
