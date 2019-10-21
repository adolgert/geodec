#include <vector>
#include <fstream>
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include <boost/property_map/vector_property_map.hpp>
#include <iostream>
#include <utility>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/linear_congruential.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/iterator/counting_iterator.hpp>
#include "geo_polyhedron.hpp"
#include "union_find.hpp"
#include "quad_complex.hpp"
#include "generate_land.hpp"
#include "gdal_io.hpp"


using namespace geodec;


BOOST_AUTO_TEST_CASE( test_generate_polyhedron )
{
	size_t w=10;
	size_t h=20;
    // Make a complex to put them on.
    Polyhedron P;
    Build_grid<HalfedgeDS> build_grid(w,h);
    P.delegate( build_grid );
    BOOST_CHECK_EQUAL(examine_polyhedron_grid<Polyhedron>(P), true);
}



BOOST_AUTO_TEST_CASE( test_single_land_type )
{
    size_t w=3, h=5;
    typedef std::map<size_t,unsigned char> use_type;
    typedef boost::associative_property_map<use_type> use_map_type;
    use_type land_use;
    use_map_type land_use_map(land_use);
    for (size_t gen_idx=0; gen_idx<w*h; gen_idx++) {
        land_use[gen_idx]=3;
    }
    std::unique_ptr<Polyhedron> P = grid2d<Polyhedron>(w,h);
    compare_land_uses<use_map_type> comparison(land_use_map);
    geodec::disjoint_set_cluster<Polyhedron,compare_land_uses<use_map_type>>
        dsc(comparison);
    dsc(*P);
    auto elem_begin=boost::counting_iterator<size_t>(0);
    auto elem_end=boost::counting_iterator<size_t>(w*h);
    BOOST_CHECK_EQUAL(dsc.dset_.count_sets(elem_begin,elem_end), 1);
}


BOOST_AUTO_TEST_CASE( test_row_land_type )
{
    size_t w=3, h=5;
    // 0 1 2
    // 3 4 5
    // 6 7 8
    // 9 10 11
    // 12 13 14
    typedef std::map<size_t,unsigned char> use_type;
    typedef boost::associative_property_map<use_type> use_map_type;
    use_type land_use;
    use_map_type land_use_map(land_use);
    size_t land_type = 0;
    for (size_t row_idx=0; row_idx<h; row_idx++) {
        for (size_t col_idx=0; col_idx<w; col_idx++) {
            land_use[row_idx*w+col_idx]=land_type+row_idx;
            std::cout << row_idx*w+col_idx << " " << row_idx << std::endl;
        }
    }
    std::unique_ptr<Polyhedron> P = grid2d<Polyhedron>(w,h);
    compare_land_uses<use_map_type> comparison(land_use_map);
    geodec::disjoint_set_cluster<Polyhedron,compare_land_uses<use_map_type>>
        dsc(comparison);
    dsc(*P);
    auto elem_begin=boost::counting_iterator<size_t>(0);
    auto elem_end=boost::counting_iterator<size_t>(w*h);
    BOOST_CHECK_EQUAL(dsc.dset_.count_sets(elem_begin,elem_end), h);
}



BOOST_AUTO_TEST_CASE( test_checker_land_type )
{
    size_t w=3, h=5;
    typedef std::map<size_t,unsigned char> use_type;
    typedef boost::associative_property_map<use_type> use_map_type;
    use_type land_use;
    use_map_type land_use_map(land_use);
    bool even=true;
    size_t land_type=13;
    size_t add=0;
    for (size_t row_idx=0; row_idx<w; row_idx++) {
        if (even) add=0;
        else add=1;
        for (size_t col_idx=0; col_idx<h; col_idx++) {
            land_use[row_idx*h+col_idx]=land_type+add;
            add=1-add;
        }
        even=!even;
    }
    std::unique_ptr<Polyhedron> P = grid2d<Polyhedron>(w,h);
    compare_land_uses<use_map_type> comparison(land_use_map);
    geodec::disjoint_set_cluster<Polyhedron,compare_land_uses<use_map_type>>
        dsc(comparison);
    dsc(*P);
    auto elem_begin=boost::counting_iterator<size_t>(0);
    auto elem_end=boost::counting_iterator<size_t>(w*h);
    BOOST_CHECK_EQUAL(dsc.dset_.count_sets(elem_begin,elem_end), w*h);
}


/*! Adding a single quad to a grid.
 *  The question is how incremental_builder indexes vertices.
 *  The answer is that the size_t index you pass to the incremental
 *  builder is the index within the storage container of the
 *  vertex or face. This index skips over erased vertices or faces.
 *  It is not the id() at all.
 */
template<class HDS>
class Add_to_grid : public CGAL::Modifier_base<HDS> {
public:
    Add_to_grid() {}
    void operator() (HDS& hds) {
        typedef typename HDS::Vertex::Point Point;
        CGAL::Polyhedron_incremental_builder_3<HDS> B(hds, true);
        B.begin_surface(2,1,6, B.ABSOLUTE_INDEXING);

        std::map<size_t,size_t> id_index;
        size_t store_idx=0;
        for (auto v=hds.vertices_begin(); v!=hds.vertices_end(); v++) {
            id_index[v->id()]=store_idx;
            store_idx++;
        }

        std::cout << "size of vertices " << hds.size_of_vertices() << std::endl;
        std::cout << "capacity vertices " << hds.capacity_of_vertices()
                  << std::endl;
        size_t a_idx=hds.size_of_vertices();
        auto add_vert_a = B.add_vertex( Point(4,2,0) );
        auto add_vert_b = B.add_vertex( Point(4,3,0) );
        add_vert_a->id()=30;
        add_vert_b->id()=31;

        // 3x5 grid has 4x6=24 vertices but one removed, so two added
        // are at zero-based index 23 & 24.
        auto facet=B.begin_facet();
        std::cout << "vert 11 is " << id_index[11] << std::endl;
        B.add_vertex_to_facet(id_index[11]); // index 10
        B.add_vertex_to_facet(id_index[15]); // index 14
        B.add_vertex_to_facet(a_idx+1);
        B.add_vertex_to_facet(a_idx);
        B.end_facet();
        facet->id()=20;

        B.end_surface();
    }
};


BOOST_AUTO_TEST_CASE( test_two_structures )
{
    size_t w=3, h=5;
    std::unique_ptr<Polyhedron> P = grid2d<Polyhedron>(w,h);
    typedef Polyhedron::Facet_iterator Facet_iterator;
    typedef Polyhedron::Halfedge_around_facet_circulator HF_circulator;
    typedef Polyhedron::Halfedge_handle Halfedge_handle;
    
	std::vector<size_t> id_to_delete;
	id_to_delete.push_back(6);
	id_to_delete.push_back(7);
	id_to_delete.push_back(3);
	
	std::vector<Facet_iterator> to_delete;
	
    // Delete some facets. Deleting a facet removes edges between two
    // deleted facets. It also removes vertices with no edges.
    // This operation will result in losing a vertex, id=8.
	// Separate finding facets from deleting because the list structure
	// is unstable under deletions. It will give a null pointer for the
	// next access after an erasure.
	size_t facet_idx=0;
    for ( Facet_iterator f=P->facets_begin(); f!=P->facets_end(); f++) {
		if (f!=Polyhedron::Facet_handle()) {
			bool should_delete=std::find(id_to_delete.begin(), id_to_delete.end(),
				f->id()) != id_to_delete.end();
        	if (should_delete) {
				to_delete.push_back(f);
        	}
        }
		facet_idx++;
    }
	for (auto del_this=to_delete.rbegin(); del_this!=to_delete.rend(); del_this++)
	{
    	auto circ=(*del_this)->facet_begin();
    	P->erase_facet(circ);
	}
    
    std::ofstream file_out("sample.txt");
    write_complex(P, file_out);

    Add_to_grid<Polyhedron::HalfedgeDS> add_edges;
    P->delegate(add_edges);
    std::ofstream modified("modified.txt");
    write_complex(P, modified);

    //HF_circulator circ = f->facet_begin();
    //Halfedge_handle opp = circ->opposite();
    //if (!opp->is_border()) {
    //    P->erase_facet(opp);
    // }
}




BOOST_AUTO_TEST_CASE( test_double_attach )
{
    size_t w=3, h=5;
    std::unique_ptr<Polyhedron> P = grid2d<Polyhedron>(w,h);
    std::unique_ptr<Polyhedron> Q = grid2d<Polyhedron>(w,h);
    typedef Polyhedron::Facet_iterator Facet_iterator;
    typedef Polyhedron::Halfedge_around_facet_circulator HF_circulator;
    typedef Polyhedron::Halfedge_handle Halfedge_handle;
}



BOOST_AUTO_TEST_CASE( gdal_io )
{
    // grid_from_file is in quad_complex.hpp.
	std::unique_ptr<Polyhedron> P = grid_from_file<Polyhedron>("blah.tif");
}
