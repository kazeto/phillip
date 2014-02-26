# -*- coding: utf-8 -*-

import sys, random


def main():
    num_of_pred = int( sys.argv[1] )
    num_of_literal = int( sys.argv[2] )
    var_list = ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
                'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
                'u', 'v', 'w', 'x', 'y', 'z' ]
    
    out = "(O "
    if len(sys.argv) >= 4:
        out += "(name %s) " % sys.argv[3]
    out += "(^"
    for i in xrange(num_of_pred):
        p = random.randint(0, num_of_pred-1)
        out += " (p%d %s)" % (p, random.choice(var_list))
    out += " ) )"

    print out
    
    
if(__name__=='__main__'):
    main()
