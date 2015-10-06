# -*- coding: utf-8 -*-

import sys, re, math, argparse, time
import util

re_pg_begin = re.compile(
    r'^<proofgraph name="(.*?)" state="(.*?)" objective="([\-0-9.]+)">')
re_time = re.compile(r'^<time lhs="([0-9.\-e]+)" ilp="([0-9.\-e]+)" sol="([0-9.\-e]+)" all="([0-9.\-e]+)">')
re_timeout = re.compile(r'^<timeout lhs="(.+?)" ilp="(.+?)" sol="(.+?)" all="(.+?)">')
re_req_gen = re.compile(r'^<label .* satisfied="(yes|no)">')
re_req_true = re.compile(r'^<label .* satisfied="(.+?)" gold="yes">')
re_req_false = re.compile(r'^<label .* satisfied="(.+?)" gold="no">')
re_literals = re.compile(r'^<literals num="(\d+)">')
re_explanations = re.compile(r'^<explanations num="(\d+)">')
re_unifications = re.compile(r'^<unifications num="(\d+)">')
re_pg_end = re.compile(r'^</proofgraph>')

re_lit = re.compile(r'^<literal ')
re_obs = re.compile(r'^<literal .*type="observable".*$')
re_exp = re.compile(r'^<explanation ')
re_uni = re.compile(r'^<unification ')


def parse(path):
    sys.stderr.write('%s Reading \"%s\"\n' % (time.strftime('%x %X]'), path))
    out = []

    fin = open(path) if path else sys.stdin
    for line in fin:
        if re_lit.match(line):
            if re_obs.match(line):
                e['observations'] += 1
            continue
        if re_exp.match(line): continue;
        if re_uni.match(line): continue;

        m = re_literals.match(line)
        if m:
            e['literals'] = int(m.group(1))
            continue

        m = re_explanations.match(line)
        if m:
            e['explanations'] = int(m.group(1))
            continue

        m = re_unifications.match(line)
        if m:
            e['unifications'] = int(m.group(1))
            continue

        m = re_pg_begin.match(line)
        if m:
            e = { 'name'      : m.group(1),
                  'state'     : m.group(2),
                  'objective' : float(m.group(3)),
                  'observations' : 0 }
            continue

        m = re_time.match(line)
        if m:
            e['time.lhs'] = float(m.group(1))
            e['time.ilp'] = float(m.group(2))
            e['time.sol'] = float(m.group(3))
            e['time.all'] = float(m.group(4))
            continue

        m = re_timeout.match(line)
        if m:
            e['t.o.lhs'] = m.group(1)
            e['t.o.ilp'] = m.group(2)
            e['t.o.sol'] = m.group(3)
            e['t.o.all'] = m.group(4)
            continue

        m = re_req_gen.match(line)
        if m: e['sol.gen'] = m.group(1);

        m = re_req_true.match(line)
        if m:
            if m.group(1) == 'yes':
                e['sol.true'] = 'yes';

        m = re_req_false.match(line)
        if m:
            if m.group(1) == 'yes':
                e['sol.false'] = 'yes'

        m = re_pg_end.match(line)
        if m:
            e['index'] = len(out)
            e['hypotheses'] = e['literals'] - e['observations']

            # SET ANSWER STATE
            e['answer'] = 'none'
            if 'sol.gen' in e:
                e['answer'] = e['sol.gen']
            if 'sol.true' in e:
                if e['sol.true'] == 'yes':
                    e['answer'] = 'true'
            if 'sol.false' in e:
                if e['sol.false'] == 'yes':
                    e['answer'] = 'false'
            
            out.append(e)
            sys.stderr.write('read %d problems\r' % len(out))
            sys.stderr.flush()

    return out


def write(parsed):
    excluded = set()    

    print '# of XMLs =', len(parsed)
    print
    
    for path, entities in parsed:
        print 'Summary of', path

        tab = util.SimpleTable(header=[
            'idx', 'name', 'state', 'answer', 'objective',
            'obs#', 'hyp#', 'chain#', 'unify#',
            'time(lhs)', 'time(ilp)', 'time(sol)', 'time(all)',
            't.o.(lhs)', 't.o.(ilp)', 't.o.(sol)', 't.o.(all)'])

        for e in entities:
            tab.add_row([
                e['index'], e['name'], e['state'], e['answer'], e['objective'],
                e['observations'], e['hypotheses'], e['explanations'], e['unifications'],
                e['time.lhs'], e['time.ilp'], e['time.sol'], e['time.all'],
                e['t.o.lhs'], e['t.o.ilp'], e['t.o.sol'], e['t.o.all']])

        is_available = lambda e: e['state'] != 'not-available'
        filtered = filter(is_available, entities)

        n_true = len(filter(lambda e: e['answer'] == 'true', filtered))
        n_false = len(filter(lambda e: e['answer'] == 'false', filtered))
        answer = '%d/%d/%d' % (
            n_true, n_false, len(entities) - (n_true + n_false))
        
        tab.set_footer([
            'all', '---', '---', answer, '---',
            sum([e['observations'] for e in filtered]) / len(filtered),
            sum([e['hypotheses'] for e in filtered]) / len(filtered),
            sum([e['explanations'] for e in filtered]) / len(filtered),
            sum([e['unifications'] for e in filtered]) / len(filtered),
            sum([e['time.lhs'] for e in filtered]) / len(filtered),
            sum([e['time.ilp'] for e in filtered]) / len(filtered),
            sum([e['time.sol'] for e in filtered]) / len(filtered),
            sum([e['time.all'] for e in filtered]) / len(filtered),
            len(filter(lambda e: e['t.o.lhs'] == 'yes', entities)),
            len(filter(lambda e: e['t.o.ilp'] == 'yes', entities)),
            len(filter(lambda e: e['t.o.sol'] == 'yes', entities)),
            len(filter(lambda e: e['t.o.all'] == 'yes', entities))])

        tab.print_table()
        print    


def main():
    parser = argparse.ArgumentParser(
        description="Summarize an output of Phillip ver.3.13 or later.")
    parser.add_argument(
        'input', nargs='*',
        help='Paths of XML files which Phillip outputs. (default: reads from stdin)')

    args = parser.parse_args()

    parsed = []
    if args.input:
        for opt in args.input:
            parsed.append([opt, parse(opt)])        
    else:
        parsed.append(['stdin', parse('')])

    sys.stderr.write('%s Completed reading XML files.\n' %
                     (time.strftime('%x %X]')))
    
    write(parsed)
    
    
if(__name__=='__main__'):
    main()
