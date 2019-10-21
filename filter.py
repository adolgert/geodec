# -*- coding: utf-8 -*-

import sys
import os
import subprocess
import logging
import re
import StringIO
import collections
import termcolor


logger=logging.getLogger(__file__)


class st_node(object):
    def __init__(self,type_=None, value=None):
        self.parent=None
        self.child=list()
        self.type=type_
        self.value=value

    def append(self,child):
        self.child.append(child)
        child.parent=self


def do_level(res, line):
    logger.debug('level %s' % res.group('level'))
    logger.debug('groups %s' % str(res.groups()))
    logger.debug('groups %s %s' % (str(res.span()), str(res.end())))
    l=st_node('message', res.groups())
    parse_line(l, line[res.end():])
    return l


def do_col(res, line):
    logger.debug('col %s' % str(res.groups()))
    l=st_node('location', res.groups())
    parse_line(l, line[res.end():])
    return l


def do_file(res, line):
    logger.debug('file %s' % str(res.groups()))
    l=st_node('from', res.groups())
    parse_line(l, line[res.end():])
    return l


def do_inc(res, line):
    logger.debug('inc %s' % str(res.groups()))
    l=st_node('include', res.groups())
    parse_line(l, line[res.end():])
    return l


_searches=[
    ['(?P<file>[a-zA-Z0-9\./_]+):(?P<line>\d+):(?P<col>\d+): (?P<level>\w+): ',
     do_level],
    ['(?P<file>[a-zA-Z0-9\./_]+):(?P<line>\d+):(?P<col>\d+):',
     do_col],
    ['(?P<file>[a-zA-Z0-9\./_]+): ',
     do_file],
    ['In file included from (?P<file>[a-zA-Z0-9\./_]+):(?P<line>\d+):(?P<col>\d+)[,:]',
     do_inc],
    ['                 from (?P<file>[a-zA-Z0-9\./_]+):(?P<line>\d+)[,:]',
     do_inc]
]


def detemplatize(parent, line):
    pt=collections.namedtuple('treenode',['parent','child','span'])
    templates=pt(None, list(), None)
    cur=templates
    s=StringIO.StringIO()
    for i, c in enumerate(line):
        if c == '<':
            cur.child.append(s.getvalue())
            s=StringIO.StringIO()
            node=pt(cur, list(), [i,None])
            cur.child.append(node)
            cur=node
        elif c == '>':
            cur.child.append(s.getvalue())
            s=StringIO.StringIO()
            cur.span[1]=i+1
            cur=cur.parent
        else:
            s.write(c)

    return line


_in_quotes=re.compile("‘([^’]+)’")
_name_ish=re.compile('([a-zA-Z0-9:_<>,]+)')

def parse_line(parent, line):
    if not line: return
    logger.debug('parsing line')
    res=_in_quotes.search(line)
    if res:
        b,e=res.span()
        body="%s'%%s'%s" % (line[:b], line[e:])
        body_node=st_node('message_body', body)
        parent.append(body_node)

        quoted=res.group(1)
        quoted=quoted.replace(', ',',')
        quoted=quoted.replace(' >','>')
        logger.debug('quoted: %s' % quoted)

        cur_idx=0
        fi=_name_ish.finditer(quoted)
        for mtype in fi:
            logger.debug('term: %s' % mtype.group(0))
            mb, me=mtype.span()
            body_node.append(st_node('text', quoted[cur_idx:mb]))
            cur_idx=me
            detemplatize(body_node, mtype.group(0))
        body_node.append(st_node('text', line[cur_idx:]))
    else:
        parent.append(st_node('message_body', line))



def node_out(parent, out, tab=''):
    out('%s%s' % (tab, parent.type))
    for c in parent.child:
        node_out(c, out, tab+'  ')


if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG)
    
    res=subprocess.Popen(' '.join(sys.argv[1:]), shell=True,
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    (sout,serr)=res.communicate()

    for s in _searches:
        s[0]=re.compile(s[0])

    unmatched=0
    res=[0]*5
    head=st_node('group')
    for line in sout.split(os.linesep):
        for search, func in _searches:
            res=search.match(line)
            if res:
                logger.debug(line[:60])
                head.append(func(res,line[res.end():]))
            else:
                head.append(st_node('text',line))

    #node_out(head, lambda x: sys.stdout.write('%s%s' % (x, os.linesep)))
