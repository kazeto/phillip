Phillip
====


# About
----
Phillip is the first-ordered abductive reasoner for natural language processing in C++.


# Install
----

## Linux, OS X

1. Install ILP-solver (Gurobi 6.0.X or LP-Solve 5.5) which you want to use.
2. Move to the directory where Phillip is installed.
2. Execute `python tools/configure.py`. Then makefile will be created.
3. Configure environment variables:  
    - If you use LP-Solve, add the path of the header directory of LP-Solve to `CPLUS_INCLUDE_PATH`.
    - If you use Gurobi optimizer, add the path of directory of Gurobi to `GUROBI_HOME`, `$GUROBI_HOME/include` to `CPLUS_INCLUDE_PATH` and `$GUROBI_HOME/lib` to `LIBRARY_PATH` and `LD_LIBRARY_PATH`.
4. Execute `make`.
5. (Optional) Execute `make test`.

## Windows

1. Install ILP-solver (Gurobi 6.0.X or LP-Solve 5.5) which you want to use.
2. Open ./vs/phillip.sln with Visual C++.
3. Configure property of the project.  
    - Under the construction... X(
4. Build the project of phillip on Visual C++.


# Usage
----

## Compile

    $ bin/phil -m compile_kb -k <KB_PREFIX> [OPTIONS] [INPUTS]

Since Phillip uses the compiled knowledge base on inference.
You need to compile your knowledge base at first.  
Besides each time you change the knowledge base, you need to compile it.

## Inference

    $ bin/phil -m inference -c lhs=<NAME> -c ilp=<NAME> -c sol=<NAME> -k <KB_PREFIX> [OPTIONS] [INPUTS]

In detail, please refer to [Phillip Wiki](https://github.com/kazeto/phillip/wiki).


