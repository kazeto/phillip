#!/usr/bin/python -S
# -*- coding: utf-8 -*-

## @author Kazeto Yamamoto
## A program to convert Phillip's output into HTML file.
## Usage: $ out2html.py -o out.html out1.xml out2.xml ...

import sys, math
import xml.etree.ElementTree as et
from collections import defaultdict

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
        self.literal = elem.text[:elem.text.rfind(':')]


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
    def __init__(self, root):
        conf = root.find('configure')
        self.components = dict(conf.find('components').items())
        self.params = dict(conf.find('params').items())

        pg = root.find('proofgraph')
        nodes = [Node(n) for n in pg.getiterator('literal')]
        chains = [Chain(e) for e in pg.getiterator('explanation')]

        self.name = pg.get('name')
        self.state = pg.get('state')
        self.objective = float(pg.get('objective'))
        self.time = dict(pg.find('time').items())
        self.timeout = dict(pg.find('timeout').items())
        
        self.nodes = dict([(n.id, n) for n in nodes])
        self.chains = dict([(c.id, c) for c in chains])
        self.unifs = [Unify(u) for u in pg.getiterator('unification')]


class Vis(ProofGraph):
    def __init__(self, root):
        ProofGraph.__init__(self, root)
        self.node2id = dict()
        self.relay_id_offset = math.pow(10, math.ceil(math.log10(max(self.nodes.keys()))))
    
        self.str_nodes = []
        self.str_edges = []

        for i in sorted(self.chains.iterkeys()):
            chain = self.chains[i]
            for j in (chain.tail + chain.head):
                if not j in self.node2id:
                    self.node2id[j] = len(self.node2id)

        for u in self.unifs:
            for i in u.unified:
                if not i in self.node2id:
                    self.node2id[i] = len(self.node2id)
        
        for i in self.nodes.iterkeys():
            if not i in self.node2id:
                self.node2id[i] = len(self.node2id)
        
        for chain in self.chains.itervalues():
            c, r = self.__chain2str(chain)
            self.str_edges += c
            if r: self.str_nodes.append(r);

        self.str_nodes += [self.__node2str(n) for i, n in self.nodes.iteritems()]
        self.str_edges += [self.__unify2str(u) for u in self.unifs]

    def write(self):
        self.__print_html(self.str_nodes, self.str_edges)

    def __node2str(self, n):
        params = {
            'id' : self.node2id[n.id],
            'label' : "\'%s\'" % n.literal,
            'shape': "\'box\'",
            'color': "\'%s\'" % (self.__get_color(n.depth) if n.active
                                 else 'whitesmoke'),
            'fontColor': "\'%s\'" % ('black' if n.active else 'darkgray'),
            'fontSize': "\'20\'",
            'title': ('\"id = <b>%d</b><br>type = <b>%s</b><br>%s"' %
                      (n.id, n.type,
                       '<br>'.join(['%s = <b>%s</b>' % (k, v)
                                    for k, v in n.attr.iteritems()
                                    if not k in DEFAULT_NODE_ATTRIBUTES]))),
            'isActive': 'true' if n.active else 'false',
            'depth' : (n.depth * 2),
            }
        return self.__join_parameters(params)

    def __chain2str(self, c):
        do_relay = lambda c: (len(c.tail) > 1 or len(c.head) > 1)
        chains = []
        relay = None
        params_common = {
            'fontColor': ("\'%s\'" %
                          (('darkred' if c.backward else 'darkblue') if c.active
                           else 'darkgray')),
            'fontSize': "\'20\'",
            'isActive': ('true' if c.active else 'false'),
            }
        params_edge = {
            'color': ("{ color: \'%s\', highlight: \'%s\', hover: \'%s\' }" %
                      (('indianred' if c.backward else 'dodgerblue') if c.active
                       else 'lightgray',
                      (('darkred' if c.backward else 'darkblue') if c.active
                      else ('lightsalmon' if c.backward else 'lightblue')),
                      (('darkred' if c.backward else 'darkblue') if c.active
                      else ('lightsalmon' if c.backward else 'lightblue')))),
            'width': 2,
            'widthSelectionMultiplier': 2
            }

        if not do_relay(c):
            params = {
                'from' : self.node2id[c.tail[0]],
                'to' : self.node2id[c.head[0]],
                'label' : "\'%s\'" % c.axiom(),
                'style' : "\'arrow\'",
                'length' : 300,
                }
            params.update(params_common)
            params.update(params_edge)
            chains.append(self.__join_parameters(params))
        else:
            relay_id = self.relay_id_offset + c.id
            params_relay = {
                'id' : relay_id,
                'label' : "\'%s\'" % c.axiom(),
                'color': "\'%s\'" % (('indianred' if c.backward else 'dodgerblue')
                                     if c.active else 'lightgray'),
                'shape': "\'dot\'",
                'radius': 10,
                'depth' : max([self.nodes[i].depth for i in c.tail]) * 2 + 1,
                }
            params_relay.update(params_common)
            relay = self.__join_parameters(params_relay)
            
            for i in c.tail:
                params_from = {
                    'from' : self.node2id[i],
                    'to' : relay_id,
                    'style' : "\'arrow\'",
                    'length' : 200,
                    }
                params_from.update(params_common)
                params_from.update(params_edge)
                chains.append(self.__join_parameters(params_from))

            for i in c.head:
                params_to = {
                    'from' : relay_id,
                    'to' : self.node2id[i],
                    'style' : "\'arrow\'",
                    'length' : 200,
                    }
                params_to.update(params_common)
                params_to.update(params_edge)
                chains.append(self.__join_parameters(params_to))
        
        return chains, relay

    def __unify2str(self, u):
        params = {
            'from' : self.node2id[u.unified[0]],
            'to' : self.node2id[u.unified[1]],
            'label' : "\'%s\'" % ', '.join(u.unifier),
            'style' : "\'dash-line\'",
            'color': ("{ color: \'%s\', highlight: \'%s\', hover: \'%s\' }" %
                      ('seagreen' if u.active else 'lightgray',
                       'darkgreen' if u.active else 'palegreen',
                       'darkgreen' if u.active else 'palegreen')),
            'fontColor': "\'%s\'" % ('darkgreen' if u.active else 'darkgray'),
            'fontSize': "\'20\'",
            'length' : 300,
            'width': 2,
            'widthSelectionMultiplier': 2,
            'isActive': ('true' if u.active else 'false'),
            }
        return self.__join_parameters(params)

    def __join_parameters(self, params):
        s = ', '.join(['%s : %s' % (k, v) for k, v in params.iteritems()])
        return '{%s}' % s

    def __get_color(self, depth, MIN = 160, MAX = 255):
        h = min(200 + depth * 32, 360)

        if h < 240:
            r = MIN
            g = (240 - h) * (MAX - MIN) /60 + MIN
            b = MAX
        elif h < 300:
            r = (h - 240) * (MAX - MIN) / 60 + MIN
            g = MIN
            b = MAX
        else:
            r = MAX
            g = MIN
            b = (360 - h) * (MAX - MIN) / 60 + MIN

        return "#%02x%02x%02x" % (r, g, b)
        

    def __print_html(self, nodes, edges):
        print """
<!doctype html>
<html>
<head>
  <meta http-equiv=\"Content-Type\" content=\"text/html;charset=utf-8\">
  <title>%s</title>

  <script type=\"text/javascript\" src=\"./visjs/dist/vis.min.js\"></script>

  <style type=\"text/css\">
    #visualization {
      width: 1200px;
      height: 1200px;
      margin: 20p;
      border: 1px solid lightgray;
    }
  </style>
</head>

<body>

<h1>%s</h1>
<ul>
  <li>state = <b>%s</b></li>
  <li>objective = <b>%f</b></li>
  <li>time: lhs = <b>%s</b>, ilp = <b>%s</b>, sol = <b>%s</b>, all = <b>%s</b></li>
  <li>timeout: lhs = <b>%s</b>, ilp = <b>%s</b>, sol = <b>%s</b>, all = <b>%s</b></li>
</ul>

<form name=\"configVis\">
  <input type=\"checkbox\" value=\"NonActiveEntities\">Non active entities
  <input type=\"checkbox\" value=\"Hierarchical layout\" checked=\"checked\">Hierarchical layout
  <input type=\"button\" value=\"Reload\" onclick=\"updateNetwork()\">
</form>
<div id=\"visualization\"></div>

<script type=\"text/javascript\">
  // create an array with nodes
  var nodes = [
    %s
  ];

  // create an array with edges
  var edges = [
    %s
  ];

  function updateNetwork(){
    var data = {
      nodes: nodes.concat(),
      edges: edges.concat(),
    };
    var options = {
      stabilize: false,
      repulsion: {
        springLengh: 300,
        nodeDistance: 300
      }
    };

    if (!document.configVis.elements[0].checked)
    {
      for (var i = data.nodes.length - 1; i >= 0; --i)
        if (!data.nodes[i].isActive)
          data.nodes.splice(i, 1);
      for (var i = data.edges.length - 1; i >= 0; --i)
        if (!data.edges[i].isActive)
          data.edges.splice(i, 1);
    }

    if (document.configVis.elements[1].checked)
    {
      for (var i = 0; i < data.nodes.length; ++i)
        data.nodes[i].level = data.nodes[i].depth;
      options.hierarchicalLayout = {
        levelSeparation: 300,
        nodeSpacing: 10,
        direction: \"LR\",
      };
    }
    
    var container = document.getElementById('visualization');
    var network = new vis.Network(container, data, options);
  }

  updateNetwork();
</script>

</body>
</html>
""" % (
self.name, self.name, self.state, self.objective,
self.time['lhs'], self.time['ilp'], self.time['sol'], self.time['all'],
self.timeout['lhs'], self.timeout['ilp'],
self.timeout['sol'], self.timeout['all'],
",\n    ".join(nodes), ",\n    ".join(edges))


def main():
    if len(sys.argv) > 1:
        tree = et.parse(sys.argv[1])
        vis = Vis(tree.getroot())
    else:
        root = et.fromstring(sys.stdin.read())
        vis = Vis(root)
        
    vis.write()

    
if(__name__=='__main__'):
    main()
