COMPILER=g++-4.6
COMPILER_CLANG=clang++
STANDARD=-std=c++0x
OPTS=-g $(STANDARD)
BOOST_INC=-L/opt/include/boost
BOOST_LIB=-L/opt/lib
CGAL_INC=-I/usr/include
CGAL_LIB=-lCGAL_Core -lCGAL
#-lCGAL_Qt4

simplex: main.cpp simplex.hpp
	$(COMPILER) $(OPTS) $(BOOST_INC) $(BOOST_LIB) main.cpp -o simplex

rips: rips.cpp union_find.hpp
	$(COMPILER) $(OPTS) -frounding-math rips.cpp $(CGAL_INC) $(CGAL_LIB) -o rips

rips_clang: rips.cpp union_find.hpp
	$(COMPILER_CLANG) $(OPTS) -frounding-math rips.cpp $(CGAL_INC) $(CGAL_LIB) -o rips
