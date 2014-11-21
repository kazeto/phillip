#!/usr/bin/python -S
# -*- coding: utf-8 -*-

## @author Kazeto Yamamoto
## A program to convert Phillip's output into HTML file.
## Usage: $ out2html.py -o out.html out1.xml out2.xml ...

import sys, math
import xml.etree.ElementTree as et
from collections import defaultdict


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
    def __init__(self, pg):
        nodes = [Node(n) for n in pg.getiterator('literal')]
        chains = [Chain(e) for e in pg.getiterator('explanation')]

        self.nodes = dict([(n.id, n) for n in nodes])
        self.chains = dict([(c.id, c) for c in chains])
        self.unifs = [Unify(u) for u in pg.getiterator('unification')]


class Vis(ProofGraph):
    def __init__(self, pg):
        ProofGraph.__init__(self, pg)
    
    def write(self):
        processed = set()
        
        nodes = []
        edges = []
        
        for c in self.chains.itervalues():
            for i in (c.tail + c.head):
                if not i in processed:
                    nodes.append(self.__node2str(self.nodes[i]))
                    processed.add(i)
            
            c, r = self.__chain2str(c)
            edges += c
            if r: nodes.append(r)

        nodes += [self.__node2str(n) for i, n in self.nodes.iteritems()
                  if not i in processed]
        edges += [self.__unify2str(u) for u in self.unifs]

        self.__print_html(nodes, edges)

    def __node2str(self, n):
        params = {
            'id' : n.id,
            'label' : "\'%s\'" % n.literal,
            'shape': "\'box\'",
            'color': "\'%s\'" % (self.__get_color(n.depth) if n.active else '#eeeeee'),
            'fontColor': "\'%s\'" % ('black' if n.active else 'gray'),
            'fontSize': "\'20\'",
            'isActive': 'true' if n.active else 'false',
            'depth' : (n.depth * 2),
            }
        return self.__join_parameters(params)

    def __chain2str(self, c):
        do_relay = lambda c: (len(c.tail) > 1 or len(c.head) > 1)
        chains = []
        relay = None
        params_common = {
            'color': "\'%s\'" % ('black' if c.active else 'gray'),
            'fontColor': "\'%s\'" % ('black' if c.active else 'gray'),
            'fontSize': "\'20\'",
            'isActive': ('true' if c.active else 'false'),
            }

        if not do_relay(c):
            params = {
                'from' : c.tail[0],
                'to' : c.head[0],
                'label' : "\'%s\'" % c.axiom(),
                'style' : "\'arrow\'",
                'length' : 300,
                }
            params.update(params_common)
            chains.append(self.__join_parameters(params))
        else:
            relay_id = math.pow(10, math.ceil(math.log10(len(self.nodes)))) + c.id
            params_relay = {
                'id' : relay_id,
                'label' : "\'%s\'" % c.axiom(),
                'shape': "\'dot\'",
                'radius': 10,
                'depth' : max([self.nodes[i].depth for i in c.tail]) * 2 + 1,
                }
            params_relay.update(params_common)
            relay = self.__join_parameters(params_relay)
            
            for i in c.tail:
                params_from = {
                    'from' : i,
                    'to' : relay_id,
                    'style' : "\'arrow\'",
                    'length' : 200,
                    }
                params_from.update(params_common)
                chains.append(self.__join_parameters(params_from))

            for i in c.head:
                params_to = {
                    'from' : relay_id,
                    'to' : i,
                    'style' : "\'arrow\'",
                    'length' : 200,
                    }
                params_to.update(params_common)
                chains.append(self.__join_parameters(params_to))
        
        return chains, relay

    def __unify2str(self, u):
        params = {
            'from' : u.unified[0],
            'to' : u.unified[1],
            'label' : "\'%s\'" % ', '.join(u.unifier),
            'style' : "\'dash-line\'",
            'color': "\'%s\'" % ('black' if u.active else 'gray'),
            'fontColor': "\'%s\'" % ('black' if u.active else 'gray'),
            'fontSize': "\'20\'",
            'length' : 300,
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
  <title>Result</title>

  <script type=\"text/javascript\" src=\"./visjs/dist/vis.min.js\"></script>

  <style type=\"text/css\">
    #mynetwork {
      width: 980px;
      height: 630px;
      border: 0px solid lightgray;
    }
  </style>
</head>

<body>

<form name=\"configVis\">
  <input type=\"checkbox\" value=\"NonActiveEntities\">Non active entities
  <input type=\"checkbox\" value=\"Hierarchical layout\">Hierarchical layout
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
""" % (",\n    ".join(nodes), ",\n    ".join(edges))


def main():
    tree = et.parse(sys.argv[1])
        
    for pg in tree.getroot().getiterator('proofgraph'):
        vis = Vis(pg)
        vis.write()


    
if(__name__=='__main__'):
    main()
