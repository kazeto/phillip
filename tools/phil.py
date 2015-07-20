# -*- coding: utf-8 -*-


import xml.etree.ElementTree as et

DEFAULT_NODE_ATTRIBUTES = set(['id', 'type', 'depth', 'active'])


def splitHypernode2Node(s):
    idx1 = s.find('{') + 1
    idx2 = s.find('}')
    return [int(x) for x in s[idx1:idx2].split(',')]


class Node:
    def __init__(self, elem):
        self.attr = dict(elem.items())
        self.id = int(self.attr['id'])
        self.depth = int(self.attr['depth'])
        self.active = (self.attr['active'] == 'yes')
        self.type = self.attr['type']
        self.literal = elem.text[:elem.text.rfind(':')].replace('\'', '_qt_')


class Chain:
    def __init__(self, elem):
        self.attr = dict(elem.items())
        self.id = int(self.attr['id'])
        self.tail = splitHypernode2Node(self.attr['tail'])
        self.head = splitHypernode2Node(self.attr['head'])
        self.active = (self.attr['active'] == 'yes')
        self.backward = (self.attr['backward'] == 'yes')
        self.text = elem.text

    def axiom(self):
        return self.attr['axiom']


class Unify:
    def __init__(self, elem):
        self.attr = dict(elem.items())
        self.unified = (int(self.attr['l1']), int(self.attr['l2']))
        self.unifier = [x.strip() for x in self.attr['unifier'].split(',')]
        self.active = (self.attr['active'] == 'yes')


class ProofGraph:
    
    ## @param pg An elment of ElementTree which is tagged "proofgraph".
    def __init__(self, pg):
        nodes = [Node(n) for n in pg.find('literals').getiterator('literal')]
        chains = [Chain(e) for e in pg.getiterator('explanation')]

        self.name = pg.get('name')
        self.state = pg.get('state')
        self.objective = float(pg.get('objective'))
        self.time = dict(pg.find('time').items())
        self.timeout = dict(pg.find('timeout').items())
        
        self.nodes = dict([(n.id, n) for n in nodes])
        self.chains = dict([(c.id, c) for c in chains])
        self.unifs = [Unify(u) for u in pg.getiterator('unification')]


class Configure:
    def __init__(self, root):
        conf = root.find('configure')
        self.components = dict(conf.find('components').items())
        self.params = dict(conf.find('params').items())


