Phillip
====

About
----
Phillip is an integrated library for logical inference in C++.

Usage
----
$ phil [options] [input]

### Options

* -m <mode> : Set the execution mode. You can use following modes.
    * inference : Perform inference.
    * compile_kb : Compile knowledge-base.
* -s <path> : Load a setting file.
* -p <name>=<value> : Set a parameter.
* -f <name> : Set a flag.
* -v <int> : Set verbosity.
* -c <type>=<name> : Set a component into Phillip. Phillip needs following 3 type components.
    * lhs : Components for making latent hypotheses sets.
    * ilp : Components for conversion latent hypotheses sets into ILP problems.
    * sol : Components for solving ILP problems.
* -k <path> : Set a filename of knowledge-base.
* -T <int> : Set timeout.