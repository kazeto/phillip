# 2015/02/27 (Phil.2.62)

- Added an example of user-defined ilp-converter.
    - Look `examples/my_ilp/main.cpp`
- Revised `tools/configure.py` so that it generates a makefile for examples.
- Refactored some source codes.


# 2015/02/09 (Phil.2.61)

- Revised `tools/configure.py` so that a makefile uses Gurobi Optimizer 6.0.
- Revised the objective function of setting stop-words.
- Added the function to check whether given literals (requirements) are included in the solution hypothesis.
    - You can activate this by using `req` function in an observation:
    ```
        (O (name foo)
           (req (p X Y) (= s Y))
           (^ ...))
    ```
    - The result of a check is written as `requirements` element in XML output.
- Added the option `-P` to get a pseudo positive sample.
    - When this option is active, a solution hypothesis is forced to include the requirements.
    - This option is equal to `-f get_pseudo_positive`.


# 2015/02/04 (Phil.2.60)

- Added the function which automatically sets stop-words on compiling knowledge base.
- Added the option `-f disable_stop_word` to turn off the above function.
- Abolished `ignore` operator, which sets user-defined stop-words.
- Added the virtual method `ilp_solver_t::solve` and its implementations.


# 2015/02/03 (Phil.2.51)

- Added `-t NUM` option which specifies the number of threads for parallelization.
    - This option equals to `-p kb_thread_num=NUM -p gurobi_thread_num=NUM`
- Added `-h` option which print a simple help.


# 2015/02/01 (Phil.2.50)

- Improved efficiency of A* search-based enumerator!
    - You can see the comparison with Phil.2.41 [here](http://www.cl.ecei.tohoku.ac.jp/~kazeto/phillip/20150201_comparison.pdf).
    - We uses 1 million axioms in this comparison.
- Updated the version of compiled knowledge base.
    - So please recompile your knowledge base.
- Abolished the function to cache axioms in memory.
    - (Because this function was not effective in efficiency.)
- Deleted `doc/manual_ja`
    - (Because this manual was too old.)
- Bug fix


# 2015/01/15 (Phil.2.41)

- Added the function to cache axioms in memory.
- Bug fix


# 2015/01/15 (Phil.2.40)

- Revised `tools/out2html.py` so that the hierarchical layout is default.
- Bug fix


# 2015/01/14 (Phil.2.32)

- Added some pre-processor for an efficiency evaluation
- Revised the definition of `phil::phillip_main_t` for customization by an user.
- Abolished the function to parallelization the whole of inference.
- Bug fix


# 2015/01/14 (Phil.2.31)

- Revised the header of XML output to include information of the knowledge base used.
- Revised `-T` option.
    - BEFORE: `-T time` equals to `-T lhs=time -T ilp=time -T sol=time`.
    - AFTER: `-T time` sets timeout to the whole of inference.
- Added a new method `phil::ilp::ilp_constraint_t::set_name`.

