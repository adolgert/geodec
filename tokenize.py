import sys
import logging
import traceback

logger=logging.getLogger(__file__)


tokens = (
    'SCOPE', 'WITHBRACKET', 'LBRACKET', 'RBRACKET',
    'LPAREN', 'RPAREN',
    'LANGLE', 'RANGLE', 'EQUALS', 'COMMA',
    'OPERATOR','MODIFIER', 'ATOMIC',
    'NAME'
)

t_SCOPE = r'::'
t_WITHBRACKET = r'\[with'
t_LBRACKET = r'\['
t_RBRACKET = r'\]'
t_LPAREN = r'\('
t_RPAREN = r'\)'
t_LANGLE = r'<'
t_RANGLE = r'>'
t_EQUALS = r'='
t_OPERATOR = r'operator'
t_MODIFIER = r'long|short|const|unsigned|static|volatile|\&|\*'
t_ATOMIC = r'byte|int|double|char|float|void|size_t'
t_NAME = r'[a-zA-Z_][a-zA-Z0-9_]*'
t_COMMA = r','

t_ignore = ' \t'

def t_error(t):
    logger.error('Cannot parse %s' % t.value[0])
    t.lexer.skip(1)

import ply.lex as lex
lexer = lex.lex()


def p_error(p):
    logger.error('syntax error: %s' % str(p))

precedence = (
    ('left', 'SCOPE'),
    ('left', 'LANGLE', 'RANGLE'),
    )



class st_node(object):
    def __init__(self,type_=None, value=None):
        self.parent=None
        self.child=list()
        self.type=type_
        self.value=value

    def append(self,child):
        self.child.append(child)
        child.parent=self

    def __repr__(self):
        if self.value:
            return '%s: %s' % (self.type,self.value)
        else:
            return self.type


def node_out(parent, out, tab=''):
    if parent.value:
        out('%s%s: %s' % (tab, parent.type, parent.value))
    else:
        out('%s%s' % (tab, parent.type))
    for c in parent.child:
        node_out(c, out, tab+'  ')


def p_blurb_qualified(t):
    'blurb : qualified'
    t[0]=t[1]


def p_blurb_equality(t):
    'blurb : qualified with'
    t[0]=st_node('withed')
    t[0].append(t[1])
    t[0].append(t[2])


def p_qualified_qualified(t):
    'qualified : qualified MODIFIER'
    t[0]=t[1]
    t[0].append(st_node('modifier',t[2]))


def p_qualified_modifier(t):
    'qualified : MODIFIER type'
    t[0]=st_node('qualified', None)
    t[0].append(st_node('modifier', t[1]))
    t[0].append(t[2])


def p_qualified_modifierafter(t):
    'qualified : type MODIFIER'
    t[0]=st_node('qualified', None)
    logger.debug('modifierafter: %s %s' % (t[1],t[2]))
    t[0].append(t[1])
    t[0].append(st_node('modifier', t[2]))


def p_qualified_type(t):
    'qualified : type'
    logger.debug('qualified %s' % str([t[1]]))
    t[0]=t[1]

def p_type_angle_list(t):
    'type : type LANGLE typelist RANGLE'
    t[0]=st_node('templated',None)
    t[0].append(t[1])
    t[0].append(t[3])

def p_type_angle(t):
    'type : type LANGLE type RANGLE'
    t[0]=st_node('templated',None)
    t[0].append(t[1])
    t[0].append(t[3])


def p_type_scope(t):
    'type : type SCOPE type'
    t[0]=st_node('scope',None)
    t[0].append(t[1])
    t[0].append(t[3])


def p_typelist_comma(t):
    'typelist : typelist COMMA type'
    t[0]=t[1]
    t[0].append(t[3])

def p_typelist_type(t):
    'typelist : type'
    t[0]=st_node('typelist')
    t[0].append(t[1])

def p_type_name(t):
    'type : NAME'
    logger.debug('name %s' % str([t[1]]))
    t[0]=st_node('name',t[1])



def p_operator_func(t):
    'operator : OPERATOR LPAREN RPAREN'
    logger.debug('operator()')
    t[0]=st_node('name', 'operator()')


def p_type_atomic(t):
    'type : ATOMIC'
    t[0]=st_node('name',t[1])

# Handle [with ] clauses.

#def p_equals_partial(t):
#    'assign : type EQUALS'
#    logger.debug('assign: %s' % str(t[1]))

def p_with_equality(t):
    'with : WITHBRACKET equalitylist RBRACKET'
    t[0]=t[2]
    t[0].type='with'

def p_equalitylist_equality(t):
    'equalitylist : equality'
    t[0]=st_node('equality_list')
    t[0].append(t[1])

def p_equalitylist_comma(t):
    'equalitylist : equalitylist COMMA equality'
    t[0]=t[1]
    t[0].append(t[3])

def p_equality_equals(t):
    'equality : qualified EQUALS qualified'
    logger.debug('equality_equals %s' % str([t[1],t[3]]))
    t[0]=st_node('equality')
    t[0].append(t[1])
    t[0].append(t[3])


import ply.yacc as yacc
parser=yacc.yacc()



_test_strings = (
    'void',
    'unsigned char',
    'const Region&',
    'blah::duh',
    'const operator()',
    'gdal::caster<double>',
    'std::vector<int,std::alloc>',
    'int [with Region=blah]',
    'double [with Region = CGAL::Polyhedron_3<CGAL::Simple_cartesian<double>>, Dude=int]',
    'void geodec::disjoint_set_cluster<Region, Compare>::operator()(const Region&) [with Region = CGAL::Polyhedron_3<CGAL::Simple_cartesian<double>, land_use_items>, Compare = compare_land_uses<boost::associative_property_map<std::map<long unsigned int, unsigned char> > >]',
)

def test_lex():
    lexer.input(_test_strings[4])

    while True:
        tok = lexer.token()
        if not tok: break
        print tok


def test_yacc():
    for s in _test_strings[:]:
        print '====', s
        result = parser.parse(s)
        if result:
            node_out(result, lambda x: logger.info('%s' % (x,)))



if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG)

    test_lex()
    #test_yacc()
