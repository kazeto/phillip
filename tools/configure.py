# -*- coding: utf-8 -*-

import sys


def get_target():
    target = raw_input('--> TARGET [default=bin/phil]: ')    
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


def main():
    print '*** Configuration of Phillip ***'

    _disable_rm = ('--disable-reachable-matrix' in sys.argv)
    _disable_cut_lhs = ('--disable-cutting-lhs' in sys.argv)

    fout = open('Makefile', 'w')
    def write(strs): fout.write('\n'.join(strs) + '\n\n');

    target = get_target()
    write(['TARGET = %s' % target,
           'CXX = g++'])
    
    write(['SOURCE = $(shell find src -name *.cpp)',
           'HEDDER = $(shell find src -name *.h)',
           'OBJS = $(SOURCE:.cpp=.o)'])

    write(['OPTS = -O2 -std=c++11 -g',
           'IDFLAGS =',
           'LDFLAGS ='])

    if ask_yes_no('USE-LP-SOLVE'):
        write(['# USE-LP-SOLVE',
               'OPTS += -DUSE_LP_SOLVE',
               'LDFLAGS += -llpsolve55'])
        
    if ask_yes_no('USE-GUROBI'):
        write(['# USE-GUROBI',
               'OPTS += -DUSE_GUROBI',
               'LDFLAGS += -lgurobi_c++ -lgurobi56 -lpthread'])

    if _disable_rm or _disable_cut_lhs:
        disp = ['# FOR DEBUG']
        if _disable_rm:
            disp.append('OPTS += -DDISABLE_REACHABLE_MATRIX')
        if _disable_cut_lhs:
            disp.append('OPTS += -DDISABLE_CUTTING_LHS')
        write(disp)

    disp = ['all: $(OBJS)']
    if get_dir(target):
        disp.append('\tmkdir -p %s' % get_dir(target))
    disp.append('\t$(CXX) $(OPTS) $(OBJS) $(IDFLAGS) $(LDFLAGS) -o $(TARGET)')
    write(disp)

    write(['.cpp.o:',
           '\t$(CXX) $(OPTS) $(IDFLAGS) -c -o $(<:.cpp=.o) $<'])

    write(['clean:',
           '\trm -f $(TARGET)',
           '\trm -f $(OBJS)'])

    write(['tar:',
           '\ttar cvzf $(TARGET).tar.gz $(SOURCE) $(HEDDER) Makefile'])

    write(['test:',
           '\t$(TARGET) -l conf/toy.compile.conf',
           '\t$(TARGET) -l conf/toy.inference.conf'])

    
if(__name__=='__main__'):
    main()
