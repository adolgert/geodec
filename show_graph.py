import sys
import logging
import matplotlib.pyplot as plt
import networkx as nx

logger=logging.getLogger(__file__)


def read_file(stream):
    G=nx.Graph()

    class State:
        START=0
        VERTICES=1
        EDGES=2
        FACES=3

    verts=dict()
    faces=dict()
    state=State.START
    for line in stream:
        vals=line.split()

        if state is State.START:
            if vals[0]=='vertices':
                state=State.VERTICES
        elif state is State.VERTICES:
            if vals[0]=='edges':
                state=State.EDGES
            elif len(vals)==4:
                id=int(vals[0])
                G.add_node(id)
                verts[id]=[float(x) for x in vals[1:]]
        elif state is State.EDGES:
            if vals[0]=='facets':
                state=State.FACES
            elif len(vals)==2:
                G.add_edge(int(vals[0]),int(vals[1]))
        elif state is State.FACES:
            if len(vals)>=4:
                faces[int(vals[0])]=[int(x) for x in vals[1:]]
                logger.debug('found face')
        else:
            logger.warning('line not understood %s' % line)
    return(G, verts, faces)


if __name__ == '__main__':
    G,pos,faces=read_file(file(sys.argv[1],'r'))

    # Draw the nodes with edges.
    xy=dict()
    for id,loc in pos.iteritems():
        xy[id]=loc[0:2]

    nx.draw(G,xy)

    # Make a separate set of nodes to represent faces.
    F=nx.Graph()
    fxy=dict()
    for fid,verts in faces.iteritems():
        F.add_node(fid)
        x=0.0
        y=0.0
        for v in verts:
            x+=xy[v][0]
            y+=xy[v][1]
        x=x/len(verts)
        y=y/len(verts)
        fxy[fid]=[x,y]

    nx.draw(F,fxy, node_color='y')

    plt.draw()
    plt.show()
