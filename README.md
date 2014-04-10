Phillip
====


About
----
Phillip is an integrated library for logical inference in C++.


Install
----

### Linux, OS X

1. Install ILP-solver which you want to use.
2. Configure ./Makefile.
3. Make.

### Windows

1. Install ILP-solver which you want to use.
2. Open ./vs/phillip.sln with Visual C++.
3. Configure property of the project.
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
        Components for making latent hypotheses sets.

        * `-c lhs=abduction` :  
            A component for basic abductive inference.

        * `-c lhs=deduction` :  
            A component for basic deductive inference.

        * `-c lhs=bidirection` :  
            A component for basic bi-directional inference.

    * `-c ilp=NAME` :  
        Components for conversion latent hypotheses sets into ILP problems.

	* `-c ilp=costed` :
	    A component for cost-based convension.

	* `-c ilp=weighted` :  
	    A component for weight-based convension.

    * `-c sol=NAME` :  
        Components for optimizing ILP problems.

	* `-c sol=null` :
	    A component to do nothing.

        * `-c sol=gurobi` :  
            A component to optimize ILP problems with Gurobi optimizer.

        * `-c sol=lpsolve` :  
            A component to optimize ILP problems with LP-Solve.5.5.

* `-k PATH` :  
    Sets prefix of path of compiled knowledge-base.

* `-T INT` :  
    Sets timeout of inference.

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
