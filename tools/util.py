# -*- coding: utf-8 -*-


import xml.etree.ElementTree as et
from itertools import izip_longest
from unicodedata import east_asian_width


## Thanks for https://github.com/akiyoko/python-simple-table
class SimpleTable(object):
    """
    SimpleTable

    Print a simple table as follows:
    +--------------+----------+----------+
    | Header 1     | Header 2 | Header 3 |
    +--------------+----------+----------+
    | aaa          | bbb      | ccc      |
    | aaaaaaaaaaaa | bb       | ccccc    |
    | a            | b        |          |
    +--------------+----------+----------+
    """

    def __init__(self, header=None, rows=None):
        self.header = header or ()
        self.rows = rows or []
        self.footer = ()

    def set_header(self, header):
        self.header = header

    def set_footer(self, footer):
        self.footer = footer

    def add_row(self, row):
        self.rows.append(row)

    def _calc_maxes(self):
        array = [self.header] + self.rows + [self.footer]
        return [max(self._unicode_width(s) for s in ss)
                for ss in izip_longest(*array, fillvalue='')]
    
    def _unicode_width(self, s, width={'F': 2, 'H': 1, 'W': 2, 'Na': 1, 'A': 2, 'N': 1}):
        s = self._format(s, 0)
        return sum(width[east_asian_width(c)] for c in s)

    def _format(self, s, n):
        if isinstance(s, int):
            return unicode(s).rjust(n)
        if isinstance(s, float):
            return unicode('%.3f' % s).rjust(n)
        else:
            return unicode(s).ljust(n)
        
    def _get_printable_row(self, row, maxes):
        return '| ' + \
               ' | '.join([self._format(r, m)
                           for r, m in izip_longest(row, maxes, fillvalue='')]) + ' |'

    def _get_printable_header(self, maxes):
        return self._get_printable_row(self.header, maxes)
    
    def _get_printable_footer(self, maxes):
        return self._get_printable_row(self.footer, maxes)
        
    def _get_printable_border(self, maxes):
        return '+-' + '-+-'.join(['-' * m for m in maxes]) + '-+'

    def get_table(self):
        lines = []
        maxes = self._calc_maxes()
        
        if self.header:
            lines.append(self._get_printable_border(maxes))
            lines.append(self._get_printable_header(maxes))
            
        lines.append(self._get_printable_border(maxes))
        
        for row in self.rows:
            lines.append(self._get_printable_row(row, maxes))
            
        if self.footer:
            lines.append(self._get_printable_border(maxes))
            lines.append(self._get_printable_footer(maxes))
            
        lines.append(self._get_printable_border(maxes))
        
        return lines

    def print_table(self):
        lines = self.get_table()
        for line in lines:
            print(line)
                
                                                                                                                   

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


