#!/usr/bin/python -S
# -*- coding: utf-8 -*-

## @author Kazeto Yamamoto
## A program to convert Phillip's output into HTML file.

import sys, math, argparse
import xml.etree.ElementTree as et
from collections import defaultdict

import phil


class ProofGraph(phil.ProofGraph):
    
    def __init__(self, pg):
        phil.ProofGraph.__init__(self, pg)
        self.node2id = dict()
        self.relay_id_offset = math.pow(10, math.ceil(math.log10(max(self.nodes.keys()) + 1)))
    
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


    def html(self, suffix=''):
        return ('''
  <style type=\"text/css\">
    #visualization@SUF {
      width: 1200px;
      height: 1000px;
      margin: 40p;
      border: 1px solid lightgray;
    }
  </style>
  
<h1>%s</h1>
<ul>
  <li>state = <b>%s</b></li>
  <li>objective = <b>%f</b></li>
  <li>time: lhs = <b>%s</b>, ilp = <b>%s</b>, sol = <b>%s</b>, all = <b>%s</b></li>
  <li>timeout: lhs = <b>%s</b>, ilp = <b>%s</b>, sol = <b>%s</b>, all = <b>%s</b></li>
</ul>

<form name=\"configVis@SUF\">
  <input type=\"checkbox\" value=\"NonActiveEntities\">Non active entities
  <input type=\"checkbox\" value=\"Hierarchical layout\" checked=\"checked\">Hierarchical layout
  <input type=\"button\" value=\"Reload\" onclick=\"updateNetwork@SUF()\">
</form>
<div id=\"visualization@SUF\"></div>

<script type=\"text/javascript\">
  // create an array with nodes
  var nodes@SUF = [
    %s
  ];

  // create an array with edges
  var edges@SUF = [
    %s
  ];

  function updateNetwork@SUF(){
    var data = {
      nodes: nodes@SUF.concat(),
      edges: edges@SUF.concat(),
    };
    var options = {
      stabilize: false,
      repulsion: {
        springLengh: 300,
        nodeDistance: 300
      }
    };

    if (!document.configVis@SUF.elements[0].checked)
    {
      for (var i = data.nodes.length - 1; i >= 0; --i)
        if (!data.nodes[i].isActive)
          data.nodes.splice(i, 1);
      for (var i = data.edges.length - 1; i >= 0; --i)
        if (!data.edges[i].isActive)
          data.edges.splice(i, 1);
    }

    if (document.configVis@SUF.elements[1].checked)
    {
      for (var i = 0; i < data.nodes.length; ++i)
        data.nodes[i].level = data.nodes[i].depth;
      options.hierarchicalLayout = {
        levelSeparation: 300,
        nodeSpacing: 10,
        direction: \"LR\",
      };
    }
    
    var container = document.getElementById('visualization@SUF');
    var network = new vis.Network(container, data, options);
  }

  updateNetwork@SUF();
</script>
''' % (
self.name, self.state, self.objective,
self.time['lhs'], self.time['ilp'],
self.time['sol'], self.time['all'],
self.timeout['lhs'], self.timeout['ilp'],
self.timeout['sol'], self.timeout['all'],
",\n    ".join(self.str_nodes),
",\n    ".join(self.str_edges))).replace('@SUF', suffix)

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
                                    if not k in phil.DEFAULT_NODE_ATTRIBUTES]))),
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

        

def convert_to_html(conf, graphs):
    return """
<!doctype html>
<html>
<head>
  <meta http-equiv=\"Content-Type\" content=\"text/html;charset=utf-8\">
  <title>%s</title>

  <script type=\"text/javascript\" src=\"./visjs/dist/vis.min.js\"></script>

</head>

<body>

%s

</body>
</html>
""" % (
(graphs[0].name if len(graphs) == 1 else "Phillip"),
'\n<hr>\n'.join([g.html('_%d' % i) for i, g in enumerate(graphs)]))


def main():
    parser = argparse.ArgumentParser(description="Visualize an output of Phillip.")
    parser.add_argument(
        'input', nargs='?',
        help='A file path of Phillip\'s output file in XML format. (default: reads from stdin)')
    parser.add_argument(
        '--split', '-s',
        help='Generate HTMLs for each proof-graph, where SPLIT is prefix of file path')
    
    args = parser.parse_args()
    
    if args.input:
        root = et.parse(args.input).getroot()
    else:
        root = et.fromstring(sys.stdin.read())

    conf = phil.Configure(root)
    graphs = [ProofGraph(pg) for pg in root.getiterator('proofgraph')]

    if args.split:
        prefix = args.split.strip('html')
        if prefix:
            if not prefix[-1] in ['.', '/']:
                prefix += '.'
                
        for i, pg in enumerate(graphs):                    
            with open('%s%d.html' % (prefix, i), 'w') as fo:
                fo.write(convert_to_html(conf, [pg,]))
                
    else:        
        print convert_to_html(conf, graphs)

    
if(__name__=='__main__'):
    main()
