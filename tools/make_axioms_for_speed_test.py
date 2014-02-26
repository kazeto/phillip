# -*- coding: utf-8 -*-


import sys, math, random


def main():
    num_of_axiom = int( sys.argv[1] )
    num_of_pred  = num_of_axiom / 4
    added_axiom_set = set()

    if num_of_axiom <= 0:
        raise ValueError
    
    for i in xrange( num_of_axiom ):
        name = "axiom%d" % i
        while 1:
            a = random.randint(0, num_of_pred-1)
            b = random.randint(0, num_of_pred-1)
            t = tuple([a,b])
            if not t in added_axiom_set:
                added_axiom_set.add(t)
                out = "(B (name %s) (=> (p%d x) (p%d x)))" % (name, a, b)
                print out
                break

    
if(__name__=='__main__'):
    main()
