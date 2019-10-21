#include <iostream>
#include <utility>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/linear_congruential.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/property_map/property_map.hpp>
#include "quad_complex.hpp"
#include "union_find.hpp"
#include "generate_land.hpp"
#include "geo_polyhedron.hpp"


using namespace geodec;


int main() {
    // Generate some fake land use values.
    size_t w=10, h=20;
    boost::mt19937 rng;
    typedef std::map<size_t,unsigned char> use_type;
    typedef boost::associative_property_map<use_type> use_map_type;
    use_type land_use;
    use_map_type land_use_map(land_use);
    boost::uniform_int<unsigned char> rand_usage(1,10);
    for (size_t gen_idx=0; gen_idx<w*h; gen_idx++) {
        std::cout << gen_idx << " ";
        land_use.insert(std::make_pair(gen_idx, rand_usage(rng)));
    }
    std::cout << std::endl;

    std::unique_ptr<Polyhedron> P = grid2d<Polyhedron>(w,h);

    compare_land_uses<use_map_type> comparison(land_use_map);
    geodec::disjoint_set_cluster<Polyhedron,compare_land_uses<use_map_type>>
        dsc(comparison);
    dsc(*P);

    return 0;
}
