Phillip
====

About
----
Phillip is an integrated library for logical inference in C++.

Usage
----
$ phil [options] [input]

Options
----

### Common options

* `-m MODE` :  
    Set the execution mode.  
    You can use following modes:

    * `-m inference` :  
        A mode to perform inference.

    * `-m compile_kb` :  
        A mode to compile knowledge-base.

* `-l PATH` :  
    Load a config file.  
    A config file includes command options in each of lines.

* `-p NAME=VALUE` :  
    Set a parameter.  
    To know available parameters, see this page.

* `-f NAME` :  
    Set a flag.

* `-v INT` :  
    Set verbosity.  
    Available value of verbosity is from 0 to 5.

### Options for inference mode

* `-c TYPE=NAME` :  
    Set a component into Phillip.  
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

    * `-c sol=NAME` :  
        Components for optimizing ILP problems.

        * `-c sol=gurobi` :  
            A component to optimize ILP problems with Gurobi optimizer.

        * `-c sol=lpsolve` :  
            A component to optimize ILP problems with LP-Solve.5.5.

* `-k PATH` :  
    Set prefix of path of compiled knowledge-base.

* `-T INT` :  
    Set timeout of inference.

### Options for compiling knowledge-base

* `-k PATH` :  
    Set prefix of path of compiled knowledge-base.
