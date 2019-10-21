#ifndef _QUAD_COMPLEX_H_
#define _QUAD_COMPLEX_H_ 1

#include <memory>
#include <set>
#include <utility>
#include <boost/array.hpp>
#include "CGAL/Modifier_base.h"
#include "CGAL/Polyhedron_incremental_builder_3.h"
#include "gdal_io.hpp"

namespace geodec
{

/*! This is just a small helper to Build_grid. */
template<class BUILDER>
void add_vertex_to_facet(BUILDER& B, size_t w, size_t i, size_t j) {
    size_t vertex=i*(w+1)+j;
    //std::cout << "f " << i << " " << j << " vertex " <<
    //    vertex << std::endl;
    B.add_vertex_to_facet(vertex);
}



/*! Build a complex of quadrilateral 2D polygons with a given
 *  width and height.
 *  HDS is a HalfedgeDS type, where DS stands for data structure.
 */
template<class HDS>
class Build_grid : public CGAL::Modifier_base<HDS> {
    size_t _w, _h;
public:
    Build_grid(size_t w, size_t h) : _w(w), _h(h) {}
    void operator() (HDS& hds) {
        CGAL::Polyhedron_incremental_builder_3<HDS> B( hds, true );

        size_t vertex_cnt = (_w+1)*(_h+1);
        size_t facet_cnt = _w*_h;
        size_t halfedge_cnt = 2*facet_cnt + _w + _h;

		// Other mode is absolute indexing, so it includes previous vertices
		// in the HDS.
		int mode=CGAL::Polyhedron_incremental_builder_3<HDS>::RELATIVE_INDEXING;
        B.begin_surface( vertex_cnt, facet_cnt, halfedge_cnt, mode );

        typedef typename HDS::Vertex Vertex;
        typedef typename Vertex::Point Point;
		typedef typename HDS::Face_handle Facet_handle;

        size_t vertex_idx=0;
        for (size_t vi=0; vi<_h+1; vi++) {
            for (size_t vj=0; vj<_w+1; vj++) {
                auto add_vert = B.add_vertex( Point(vj,vi,0) );
                add_vert->id()=vertex_idx;
                vertex_idx++;
            }
        }
        
        size_t facet_idx=0;
        for (size_t fi=0; fi<_h; fi++) {
            for (size_t fj=0; fj<_w; fj++) {
                Facet_handle f = B.begin_facet();
                f->id() = facet_idx;
                // Must add vertices in oriented order.
                add_vertex_to_facet(B, _w, fi,   fj);
                add_vertex_to_facet(B, _w, fi+1, fj);
                add_vertex_to_facet(B, _w, fi+1, fj+1);
                add_vertex_to_facet(B, _w, fi,   fj+1);
                B.end_facet(); // returns halfedge on facet border.
                facet_idx++;
            }
        }

        B.end_surface();
    }
};




/*! Check a complex of four-sided simplices for consistency. */
template<class Poly3>
bool examine_polyhedron_grid(const Poly3& P)
{
    bool verbose=false;
    bool is_valid = P.is_valid(verbose);

    bool is_quad = P.is_pure_quad();
    std::cout << "Is quad " << is_quad << std::endl;

    size_t border_cnt=0;
    size_t internal_cnt=0;
    for (typename Poly3::Halfedge_const_iterator i=P.halfedges_begin();
         i != P.halfedges_end(); i++ )
    {
        if (i->is_border()) {
            border_cnt++;
        } else {
            internal_cnt++;
        }
    }
    std::cout << "border " << border_cnt << " internal " << internal_cnt
              << std::endl;

    size_t face_cnt=0;
    std::cout << "faces on edges: ";
    for (auto f=P.facets_begin();
         f!=P.facets_end(); f++) {
        face_cnt++;
        bool is_border=false;
        auto circulator = f->facet_begin();
        auto circulator_end=circulator;
        do {
            if (circulator->opposite()->is_border()) {
                is_border=true;
            }
        } while ( ++circulator != circulator_end );
        if (is_border) {
            std::cout << f->id() << " ";
        }
    }
    std::cout << std::endl;
    std::cout << "total faces: " << face_cnt << std::endl;
	return (is_valid && is_quad);
}


    /*! Makes a 2D grid.
     *  POLY is a Polyhedron_3.
     */
    template<class POLY>
    std::unique_ptr<POLY> grid2d(size_t w, size_t h)
    {
        // Make a complex to put them on.
        std::unique_ptr<POLY> P(new POLY);
        Build_grid<typename POLY::HalfedgeDS> build_grid(w,h);
        P->delegate( build_grid );
        examine_polyhedron_grid<POLY>(*P);
        return P;
    }



    /*! Add vertices from a file. Works with gdal_file, which calls this back.
     *  This derives from the Modifier_base, which is the only class that has permission
     *  to change the Polyhedron. It adds vertices to the Polyhedron, and it is this
     *  class that tracks vertices that were already in the Polyhedron, so that, when
     *  adding new polygons, any vertices that already are loaded are used, instead of
     *  being created new.
     */
    template<class HDS, class READER>
    class add_from_file : public CGAL::Modifier_base<HDS> {
        READER& reader_;
        std::map<size_t,size_t> id_to_index_;
        size_t running_vertex_;
        CGAL::Polyhedron_incremental_builder_3<HDS>* B_;
    public:
        add_from_file(READER& reader) : reader_(reader) {}
        void operator() (HDS& hds) {
            B_=new CGAL::Polyhedron_incremental_builder_3<HDS>(hds, true);
            
            running_vertex_=0;
            for (auto v=hds.vertices_begin(); v!=hds.vertices_end(); v++) {
                id_to_index_[v->id()]=running_vertex_;
                running_vertex_++;
            }
            std::cout << "Starting with " << running_vertex_ << " vertices in the Polyhedron."
                << std::endl;

            B_->begin_surface(0, 0, 0, B_->ABSOLUTE_INDEXING);
			// Could pass the builder, B_, but we need only a limited part of its
			// interface, so passing _this_ keeps toolkits separate.
			bool success = reader_.read_block(*this);
            B_->end_surface();
            delete B_;
        }

		//! Interface for the file reader to add vertices to the Polyhedron.
        void add_vertex( boost::array<double,3> loc, size_t id)
        {
            typedef typename HDS::Vertex::Point Point;
            // if vertex doesn't already exist:
			if (id_to_index_.find(id)==id_to_index_.end()) {
				auto add_vert_a = B_->add_vertex( Point(loc[0], loc[1], loc[2]) );
            	add_vert_a->id()=id;
            	id_to_index_[id]=running_vertex_;
            	running_vertex_++;
			}
        }

		//! Interface for the file reader to add faces to the Polyhedron.
        void add_face( const boost::array<size_t,4>& ids, size_t id)
        {
            auto facet=B_->begin_facet();
            for (auto pid=ids.cbegin(); pid<ids.cend(); pid++) {
                B_->add_vertex_to_facet(id_to_index_[*pid]);
            }
            facet->id()=id;
            B_->end_facet();
        }
    };



    /*! Makes a grid from a raster datafile.
     *  POLY is a Polyhedron_3.
     *  \returns unique_ptr to a polyhedral structure. This means that who
     *  gets it owns it.
     */
    template<class POLY>
    std::unique_ptr<POLY> grid_from_file(const std::string& filename)
    {
        // Make a complex to which to add blocks.
        std::unique_ptr<POLY> P(new POLY);
		gdal_file reader(filename);
        add_from_file<typename POLY::HalfedgeDS,gdal_file> build_grid(reader);
        P->delegate( build_grid );
        examine_polyhedron_grid<POLY>(*P);
        return P;
    }




    /*! Write a complex to a stream in a very simple way.
     *  This writes vertex coordinates, edges, and faces
     *  to the output stream.
     */
    template<class POLY>
    void write_complex(const POLY& P, std::ostream& out)
    {
        out << "vertices" << std::endl;
        for (auto v=P->vertices_begin(); v!=P->vertices_end(); v++) {
            auto p = v->point();
            out << v->id() << " " << p.x() << " " << p.y() << " " << p.z()
                 << std::endl;
        }

        out << "edges" << std::endl;
        std::set<std::pair<size_t,size_t> > seen;
        for ( auto e=P->halfedges_begin(); e!=P->halfedges_end(); e++) {
            size_t a=e->vertex()->id();
            size_t b=e->opposite()->vertex()->id();
            std::pair<size_t,size_t> edge;
            if (a<b) {
                edge=std::make_pair(a,b);
            } else {
                edge=std::make_pair(b,a);
            }
            auto already=seen.find(edge);
            if (already==seen.end()) {
                out << edge.first << " " << edge.second << std::endl;
                seen.insert(edge);
            } else {
                seen.erase(already);
            }
        }

        out << "facets" << std::endl;
        for (auto f=P->facets_begin(); f!=P->facets_end(); f++) {
            out << f->id() << " ";
            auto fi=f->facet_begin();
            auto fi_end=fi;
            do {
                out << fi->vertex()->id() << " ";
            } while (++fi!=fi_end);
            out << std::endl;
        }
    }
}
#endif // _QUAD_COMPLEX_H_
