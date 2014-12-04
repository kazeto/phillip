Phillip
====


About
----
Phillip is an integrated library for logical inference in C++.


Install
----

### Linux, OS X

1. Install ILP-solver (Gurobi 5.6 or LP-Solve 5.5) which you want to use.
2. Move to the directory where Phillip is installed.
2. Execute `python tools/configure.py`. Then makefile will be created.
3. Configure environment variables:  
    - If you use LP-Solve, add the path of the header directory of LP-Solve to `CPLUS_INCLUDE_PATH`.
    - If you use Gurobi optimizer, add the path of directory of Gurobi to `GUROBI_HOME`, `$GUROBI_HOME/include` to `CPLUS_INCLUDE_PATH` and `$GUROBI_HOME/lib` to `LIBRARY_PATH` and `LD_LIBRARY_PATH`.
4. Execute `make`.
5. (Optional) Execute `make test`.

### Windows

1. Install ILP-solver (Gurobi 5.6 or LP-Solve 5.5) which you want to use.
2. Open ./vs/phillip.sln with Visual C++.
3. Configure property of the project.  
    - Under the construction... X(
4. Build the project of phillip on Visual C++.


Usage
----
$ bin/phil [options] [input]


Options
----

### Common options

* `-m MODE` :  
    Sets the execution mode.  
    You can use following modes:

    * `-m inference` :  
        A mode to perform inference.

    * `-m compile_kb` :  
        A mode to compile knowledge-base.

* `-l PATH` :  
    Loads a config file.  
    A config file includes command options in each of lines.

* `-p NAME=VALUE` :  
    Sets a parameter.  

* `-f NAME` :  
    Sets a flag.

* `-v INT` :  
    Sets verbosity.  
    Available value of verbosity is from 0 to 5.

### Options for inference mode

* `-c TYPE=NAME` :  
    Sets a component into Phillip.  
    Phillip needs following 3 type components to perform inference.

    * ` -c lhs=NAME` :  
        Components for generating latent hypotheses sets.

        * `-c lhs=depth` :  
            Generates latent hypotheses sets in a manner similar to Henry.

        * `-c lhs=a*` :  
            Generates latent hypotheses sets in a manner based on A* search.

    * `-c ilp=NAME` :  
        Components for conversion latent hypotheses sets into ILP problems.

	* `-c ilp=costed` :  
	    Does conversion based on cost-based abduction.

	* `-c ilp=weighted` :  
	    Does conversion based on weighted-abduction..

    * `-c sol=NAME` :  
        Components for optimizing ILP problems.

	* `-c sol=null` :  
	    Does nothing.

        * `-c sol=gurobi` :  
            Optimizes ILP problems with Gurobi optimizer.

        * `-c sol=lpsolve` :  
            Optimizes ILP problems with LP-Solve.5.5.

* `-k PATH` :  
    Sets prefix of path of compiled knowledge-base.

* `-T INT` :  
    Sets timeout of each process in second.

### Options for compiling knowledge-base

* `-d TYPE`  
    Specifies a type of distance provider.  
    You can use following types:

    * `-d basic`  
        Distance of each axiom is fixed to 1.0.

    * `-d cost`
        Distance of each axiom is equal to cost of the axiom.

* `-k PATH` :  
    Sets prefix of path of compiled knowledge-base.

* `-p kb_thread_num=INT` :
    Sets the number of parallel threads for compiling.
    