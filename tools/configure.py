# -*- coding: utf-8 -*-

import sys


def get_bin_target():
    target = raw_input('--> BINARY TARGET [default=bin/phil]: ')    
    return target if target else 'bin/phil'


def get_dir(path):
    idx = path.rfind('/')
    if idx > 0:
        if not path[:idx] in ('.', '..'):
            return path[:idx]
    return ''


def ask_yes_no(query):
    while True:
        ans = raw_input('--> %s [y/n]: ' % query).lower()
        if ans in ('yes', 'y'): return True;
        if ans in ('no', 'n'):  return False;


def write_main(target_bin, do_use_lpsolve, do_use_gurobi):
    cmd_test = ''
    
    with open('Makefile', 'w') as fout:
        fout.write('\n'.join([
            'CXX = g++',
            '',
            'TARGET_BIN = %s' % target_bin,
            'TARGET_LIB = lib/libphil.a',
            '',
            'SRCS_BIN = $(shell find src -name *.cpp)',
            'SRCS_LIB = $(shell find src -name *.cpp -and ! -name main.cpp)',
            'HEDDER = $(shell find src -name *.h)',
            'OBJS_BIN = $(SRCS_BIN:.cpp=.o)',
            'OBJS_LIB = $(SRCS_LIB:.cpp=.lo)',
            '',
            'OPTS = -O2 -std=c++11 -g',
            'IDFLAGS =',
            'LDFLAGS =',
            '',
            '# USE-LP-SOLVE',
            ('' if do_use_lpsolve else '# ') + 'OPTS += -DUSE_LP_SOLVE',
            ('' if do_use_lpsolve else '# ') + 'LDFLAGS += -llpsolve55',
            '',
            '# USE-GUROBI',
            ('' if do_use_gurobi else '# ') + 'OPTS += -DUSE_GUROBI',
            ('' if do_use_gurobi else '# ') + 'LDFLAGS += -lgurobi_c++ -lgurobi60 -lpthread',
            '',
            'all: $(OBJS_BIN)' +
            (('\n\tmkdir -p %s' % get_dir(target_bin)) if get_dir(target_bin) else ''),
            '\t$(CXX) $(OPTS) $(OBJS_BIN) $(IDFLAGS) $(LDFLAGS) -o $(TARGET_BIN)',
            '',
            '.cpp.o:',
            '\t$(CXX) $(OPTS) $(IDFLAGS) -c -o $(<:.cpp=.o) $<',
            '',
            'lib: $(OBJS_LIB)',
            '\tmkdir -p lib',
            '\tar rcs $(TARGET_LIB) $(OBJS_LIB)',
            '',
            '%.lo:%.cpp',
            '\t$(CXX) $(OPTS) $(IDFLAGS) -fPIC -c -o $(<:.cpp=.lo) $<',
            '',
            'clean:',
            '\trm -f $(TARGET_BIN)',
            '\trm -f $(TARGET_LIB)',
            '\trm -f $(OBJS_BIN)',
            '\trm -f $(OBJS_LIB)',
            '',
            'test:',
            '\t$(TARGET_BIN) -m infer -k data/compiled/kb '
            '-c dist=basic -c tab=null -c lhs=a* -c ilp=weighted -c sol=gurobi '
            '-p kb_max_distance=4 -p max_distance=4.0 '
            '-f do_compile_kb -v 5 data/toy.lisp']))


def write_example(dir, do_use_lpsolve, do_use_gurobi):
    with open(dir + '/Makefile', 'w') as fout:
        fout.write('\n'.join([
            'CXX = g++',
            'TARGET = ./phil',
            '',
            'SRCS = main.cpp',
            'HEDDER = $(shell find ../../src -name *.h)',
            'OBJS = $(SRCS:.cpp=.o)',
            '',
            'OPTS = -O2 -std=c++11 -g',
            'IDFLAGS = -I ../../src',
            'LDFLAGS = -L ../../lib -lphil',
            '',
            '# USE-LP-SOLVE',
            ('' if do_use_lpsolve else '# ') + 'OPTS += -DUSE_LP_SOLVE',
            ('' if do_use_lpsolve else '# ') + 'LDFLAGS += -llpsolve55',
            '',
            '# USE-GUROBI',
            ('' if do_use_gurobi else '# ') + 'OPTS += -DUSE_GUROBI',
            ('' if do_use_gurobi else '# ') + 'LDFLAGS += -lgurobi_c++ -lgurobi60 -lpthread',
            '',
            'all: $(OBJS)',
            '\t$(CXX) $(OPTS) $(OBJS) $(IDFLAGS) $(LDFLAGS) -o $(TARGET)',
            '',
            '.cpp.o:',
            '\t$(CXX) $(OPTS) $(IDFLAGS) -c -o $(<:.cpp=.o) $<',
            '',
            'clean:',
            '\trm -f $(TARGET)',
            '\trm -f $(OBJS)',
            '',]))


def main():
    print '*** Configuration of Phillip ***'

    target = get_bin_target()
    
    do_use_lpsolve = ask_yes_no('USE-LP-SOLVE')
    do_use_gurobi = ask_yes_no('USE-GUROBI')

    write_main(target, do_use_lpsolve, do_use_gurobi)
    write_example('examples/my_ilp', do_use_lpsolve, do_use_gurobi)

    
if(__name__=='__main__'):
    main()
