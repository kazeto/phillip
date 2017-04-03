# -*- coding: utf-8 -*-

## @author Kazeto Yamamoto
## A program to convert Henry's output into Graphviz-formatted file.

import sys
import xml.etree.ElementTree as et
from collections import defaultdict


FONT = '\"Consolas\"'
COLOR = {
    'black' : '\"#000000\"',
    'gray' : '\"#a0a0a0\"'}
DO_INCLUDE_NON_ACTIVES = False


def splitHypernode2Node(s):
    idx1 = s.find('{') + 1
    idx2 = s.find('}')
    return [int(x) for x in s[idx1:idx2].split(',')]


class Node:
    def __init__(self, elem):
        self.__attr = dict(elem.items())
        self.id = int(self.__attr['id'])
        self.depth = int(self.__attr['depth'])
        self.active = (self.__attr['active'] == 'yes')
        self.type = self.__attr['type']
        self.literal = elem.text


class Chain:
    def __init__(self, elem):
        self.__attr = dict(elem.items())
        self.id = int(self.__attr['id'])
        self.tail = splitHypernode2Node(self.__attr['tail'])
        self.head = splitHypernode2Node(self.__attr['head'])
        self.active = (self.__attr['active'] == 'yes')
        self.backward = (self.__attr['backward'] == 'yes')
        self.text = elem.text

    def axiom(self):
        return self.__attr['axiom']


class Unify:
    def __init__(self, elem):
        self.__attr = dict(elem.items())
        self.unified = (int(self.__attr['l1']), int(self.__attr['l2']))
        self.unifier = [x.strip() for x in self.__attr['unifier'].split(',')]
        self.active = (self.__attr['active'] == 'yes')


class ProofGraph:
    def __init__(self, tree):
        root = tree.getroot()
        nodes = [Node(n) for n in root.getiterator('literal')]
        chains = [Chain(e) for e in root.getiterator('explanation')]
        unifs = [Unify(u) for u in root.getiterator('unification')]
        
        is_active = lambda x: x.active or DO_INCLUDE_NON_ACTIVES
        
        self.nodes  = dict([(n.id, n) for n in nodes if is_active(n)])
        self.chains = dict([(c.id, c) for c in chains if is_active(c)])
        self.unifs  = filter(is_active, unifs)


class Graphviz(ProofGraph):
    def __init__(self, tree):
        ProofGraph.__init__(self, tree)
    
    def write(self, fout):
        nodes = ['  %s\n' % self._str_node(n) for n in self.nodes.itervalues()]
        nodes += ['  %s\n' % self._str_relay(c)
                  for c in self.chains.itervalues() if self._do_relay(c)]

        edges = []
        for c in self.chains.itervalues():
            n = (len(c.tail) + len(c.head)) if self._do_relay(c) else 1
            edges += ['  %s\n' % self._str_chain(c, i) for i in xrange(n)]
            
        unifs = ['  %s\n' % self._str_unify(u) for u in self.unifs]

        ranks = defaultdict(list)
        for n in self.nodes.itervalues():
            if n.depth >= 0:
                ranks[n.depth].append('node_%d' % n.id)
        
        fout.write('digraph proofgraph{\n'
                   '  graph [rankdir = LR];\n')
        fout.write(''.join(nodes))
        fout.write(''.join(edges))
        fout.write(''.join(unifs))

        # for d, ns in ranks.iteritems():
        #     fout.write('  {rank = same; %s}\n' % '; '.join(ns))

        fout.write('}\n')

    def _str_node(self, n):
        props = {
            'shape' : 'box',
            'color' : COLOR['black' if n.active else 'gray'],
            'peripheries' : '2' if n.type.startswith('observable') else '1',
            'fontname' : FONT,
            'fontsize' : '11',
            'fontcolor' : COLOR['black' if n.active else 'gray'],
            'label' : '\"%s\"' % n.literal}
        return 'node_%d [%s];' % (n.id, self._join_props(props))

    def _str_relay(self, c):
        props = {
            'shape' : 'point',
            'color' : COLOR['black' if c.active else 'gray']}
        return 'edge_%d [%s];' % (c.id, self._join_props(props))

    def _str_chain(self, c, i):
        props = {
            'fontname' : FONT,
            'fontsize' : '10',
            'fontcolor' : COLOR['black' if c.active else 'gray'],
            'label' : '\"%s\"' % c.axiom(),
            'color' : COLOR['black' if c.active else 'gray']}
        if self._do_relay(c):
            if i != 0: del props['label'];
            if i < len(c.tail):
                props['dir'] = 'back' if c.backward else 'forward'
                return 'node_%d -> edge_%d [%s];' % (c.tail[i], c.id, self._join_props(props))
            else:
                props['dir'] = 'back' if c.backward else 'forward'
                return 'edge_%d -> node_%d [%s];' % \
                       (c.id, c.head[i - len(c.tail)], self._join_props(props))
        else:
            props['dir'] = 'back' if c.backward else 'forward'
            return 'node_%d -> node_%d [%s];' % \
                   (c.tail[0], c.head[0], self._join_props(props))

    def _str_unify(self, u):
        props = {
            'style' : 'dotted',
            'dir' : 'none',
            'color' : COLOR['black' if u.active else 'gray'],
            'fontname' : FONT,
            'fontsize' : '10',
            'fontcolor' : COLOR['black' if u.active else 'gray'],
            'label' : '\"%s\"' % '\\n'.join(u.unifier).replace(':', ':$')}
        return 'node_%d -> node_%d [%s];' % \
               (u.unified[0], u.unified[1], self._join_props(props))

    def _do_relay(self, c):
        return (len(c.tail) > 1 or len(c.head) > 1)
    
    def _join_props(self, props, delim = ', '):
        return delim.join([('%s = %s' % (k, v)) for k, v in props.iteritems()])


def convert(filename):
    tree = et.parse(filename)
    gv = Graphviz(tree)
    with open(filename + '.dot', 'w') as fout:
        gv.write(fout)


def main():
    if len(sys.argv) > 1:
        for arg in sys.argv[1:]:
            convert(arg)

    
if(__name__=='__main__'):
    main()
