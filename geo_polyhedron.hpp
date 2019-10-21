#ifndef _GEO_POLYHEDRON_HPP_
#define _GEO_POLYHEDRON_HPP_ 1

#include "CGAL/HalfedgeDS_default.h"
#include "CGAL/HalfedgeDS_decorator.h"
#include "CGAL/Simple_cartesian.h"
#include "CGAL/Polyhedron_3.h"
#include "CGAL/Polyhedron_items_with_id_3.h"


struct Traits { typedef int Point_2; };
typedef CGAL::HalfedgeDS_default<Traits> HDS;
typedef CGAL::HalfedgeDS_decorator<HDS> Decorator;



template <class Refs>
struct land_use_face : public CGAL::HalfedgeDS_face_base<Refs> {
    size_t id;
};


template <class Refs, class Traits>
struct land_use_vertex : public CGAL::HalfedgeDS_vertex_base<Refs, CGAL::Tag_true> {
    size_t id;
};




struct land_use_items : public CGAL::Polyhedron_items_3 {
    template <class Refs, class Traits>
    struct Face_wrapper {
        typedef land_use_face<Refs> Face;
    };

    template <class Refs, class Traits>
    struct Vertex_wrapper {
        typedef land_use_vertex<Refs,Traits> Vertex;
    };
};


typedef CGAL::Simple_cartesian<double> Kernel;
typedef CGAL::Polyhedron_3<Kernel,CGAL::Polyhedron_items_with_id_3> Polyhedron;
typedef Polyhedron::Halfedge_handle Halfedge_handle;
typedef Polyhedron::Facet_handle Facet_handle;



typedef Polyhedron::HalfedgeDS HalfedgeDS;

#endif // _GEO_POLYHEDRON_HPP_
