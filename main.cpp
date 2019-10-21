#define BOOST_TEST_DYN_LINK
#include <iostream>
#include <boost/test/included/unit_test.hpp>
#include <boost/bind.hpp>
#include <boost/program_options.hpp>
#include "simplex.hpp"

using namespace std;
using namespace geodec;
namespace po = boost::program_options;
using namespace boost::unit_test;


void test_empty() {}


void test_simplex()
{
    simplex<int,0> pt;
    int val[]={ 3 };
    pt.assign(val, 0);

    simplex<int,2> face;
    int vals[]={ 0, 2, 1 };
    face.assign(vals, 0);
    auto edges = face.boundary();
}


bool init_function()
{
    framework::master_test_suite().
        add( BOOST_TEST_CASE( test_empty ));
    //framework::master_test_suite().
    //    add( BOOST_TEST_CASE( test_simplex ));
}



int main(int argc, char* argv[])
{
    po::options_description desc("Test runner.");
    desc.add_options()
        ("help","This runs test code.")
        ;
    po::variables_map vm;
    auto parsed_options=po::parse_command_line(argc, argv,desc);
    po::store(parsed_options, vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return(0);
    }
      
    ::boost::unit_test::unit_test_main( &init_function, argc, argv );

    simplex<int,0> pt;
    int val[]={ 3 };
    pt.assign(val, 0);

    simplex<int,2> face;
    int vals[]={ 0, 2, 1 };
    face.assign(vals, 0);
    auto edges = face.boundary();

    typedef boost::array<double,3> vertex_type;
    typedef std::vector<vertex_type> vertices_type;

    vertices_type vertices;
    
    typedef std::vector<simplex<int,2> > elements_type;
    
    elements_type elements;

    simplicial_mesh<vertices_type,elements_type> mesh(vertices, elements);

    return 0;
}
