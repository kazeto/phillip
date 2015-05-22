# 2015/05/22 (Phil.3.10)

- Improved the performance of lhs::a_star_based_enumerator_t.
- Updated the version of compiled knowledge base.
    - So please recompile your knowledge base.
- Modified `tools/out2html.py`, which is the script to visualize output of Phillip.
    - You can see the usage by `python tools/out2html.py -h`.
- Modified some command options:

| Before                                              | After                           |
| --------------------------------------------------- |-------------------------------- |
| `-m inference`                                      | `-m inference` or `-m infer`    |
| `-m compile_kb`                                     | `-m compile_kb` or `-m compile` |
| `-p path_out=<PATH>`                                | `-o <PATH>`                     |
| `-p path_lhs_out=<PATH>`                            | `-o lhs=<PATH>`                 |
| `-p path_ilp_out=<PATH>`                            | `-o ilp=<PATH>`                 |
| `-p path_sol_out=<PATH>`                            | `-o sol=<PATH>`                 |
| `-e <NAME>`                                         | `-t !<NAME>`                    |
| `-o <NAME>`                                         | `-t <NAME>`                     |
| `-P`                                                | `-G`                            |
| `-p kb_thread_num=<INT> -p gurobi_thread_num=<INT>` | `-P <INT>`                      |
| `-p kb_thread_num=<INT>`                            | `-P kb=<INT>`                   |
| `-p gurobi_thread_num=<INT>`                        | `-P grb=<INT>`                  |

- Performed major refactoring. We show some of modified things below:

| Before                                                         | After                                                         |
| -------------------------------------------------------------- | ------------------------------------------------------------- |
| stop_watch_t                                                   | Abolished                                                     |
| `phillip_main_t::timeout_lhs()`                                | Replace to `phillip_main_t::timeout_lhs().get()`              |
| `phillip_main_t::timeout_ilp()`                                | Replace to `phillip_main_t::timeout_ilp().get()`              |
| `phillip_main_t::timeout_sol()`                                | Replace to `phillip_main_t::timeout_sol().get()`              |
| `phillip_main_t::timeout_all()`                                | Replace to `phillip_main_t::timeout_all().get()`              |
| `phillip_main_t::is_timeout_lhs(int)`                          | Replace to `phillip_main_t::timeout_lhs().do_time_out(float)` |
| `phillip_main_t::is_timeout_ilp(int)`                          | Replace to `phillip_main_t::timeout_ilp().do_time_out(float)` |
| `phillip_main_t::is_timeout_sol(int)`                          | Replace to `phillip_main_t::timeout_sol().do_time_out(float)` |
| `phillip_main_t::is_timeout_all(int)`                          | Replace to `phillip_main_t::timeout_all().do_time_out(float)` |
| `phillip_main_t::get_clock_for_lhs()`                          | Abolished                                                     |
| `phillip_main_t::get_clock_for_ilp()`                          | Abolished                                                     |
| `phillip_main_t::get_clock_for_sol()`                          | Abolished                                                     |
| `phillip_main_t::get_clock_for_all()`                          | Abolished                                                     |
| `pg::node_t::evidences()`                                      | Renamed to `pg::node_t::ancestors()`                          |
| `pg::proof_graph_t::enumerate_observations()`                  | Replace to `pg::proof_graph_t::observation_indices()`         |
| `pg::proof_graph_t::do_disregard_hypernode(int)`               | Abolished                                                     |
| `pg::proof_graph_t::enumerate_queries_for_knowledge_base(...)` | Renamed to `pg::proof_graph_t::enumerate_arity_patterns(...)` |

- Changed the format of `req` statement in observations.
    - Each argument of `req` must be a literal or a conjunction.
    - If `req` statement has one argument, it is the gold label.
    - If `req` statement has plural arguments, they are label candidates in some labeling task.
      In this case, you can specify the gold label by adding the optional parameter `:gold`.
    - Example:
    ```lisp
        ; Using -G option, Phillip searches the hypothesis which includes (p X).
        (O (req (p X)) (^ ...))
        ; Using -G option, Phillip searches the hypothesis which includes (p X) and (q Y).
        (O (req (^ (p X) (q Y))) (^ ...))
        ; On default, Phillip searches the hypothesis which includes (p X) or (q Y).
        ; Using -G option, Phillip searches the hypothesis which includes (p X).
        (O (req (p X :gold) (q Y)) (^ ...))
    ```
- Fixed some bugs.
    

# 2015/04/03 (Phil.3.00)

- Added the function of Category-Table.
    - This function enables soft unification and soft backchaining.
    - You can activate this by `-c tab=<NAME>` option.
    - We will add the detail to Phillip's wiki later.
- Updated the version of compiled knowledge base.
    - So please recompile your knowledge base.
- Added an option to use Logarithmic Weighted Abduction, "-f logarithmic_weighted_abduction".
    - This option must be used together with "-c ilp=weighted".
    - We will add the detail to Phillip's wiki later.
    - (But, We need to discuss more the appropriacy of this evaluation function.)
- Added options to print human readable output, "-f human_readable_output" and "-H".
    - Thanks for Katya's feedback.
- Implemented exception handling.
    - If there is any error, an exception of phillip_exception_t is thrown.
- Performed refactoring for some source codes.


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

