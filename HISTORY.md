# 2015/02/01 (Phil.2.50)

- Improved efficiency of A* search-based enumerator!
-- You can see the comparison with Phil.2.41 [here](http://www.cl.ecei.tohoku.ac.jp/~kazeto/phillip/20150201_comparison.pdf).
-- We uses 1 million axioms in this comparison.
- Updated the version of compiled knowledge base.
-- So please recompile your knowledge base.
- Abolished the function to cache axioms in memory.
-- (Because this function was not effective in efficiency.)
- Deleted doc/manual_ja
-- (Because this manual was too old.)
- Bug fix


# 2015/01/15 (Phil.2.41)

- Added the function to cache axioms in memory.
- Bug fix


# 2015/01/15 (Phil.2.40)

- Revised tools/out2html.py so that the hierarchical layout is default.
- Bug fix


# 2015/01/14 (Phil.2.32)

- Added some pre-processor for an efficiency evaluation
- Revised the definition of phil::phillip_main_t for customization by an user.
- Abolished the function to parallelization the whole of inference.
- Bug fix


# 2015/01/14 (Phil.2.31)

- Revised the header of XML output to include information of the knowledge base used.
- Revised -T option.
-- BEFORE: "-T time" equals to "-T lhs=time -T ilp=time -T sol=time".
-- AFTER: "-T time" sets timeout to the whole of inference.
- Added a new method phil::ilp::ilp_constraint_t::set_name.

