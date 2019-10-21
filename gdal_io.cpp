#include "gdal_io.hpp"
#include "gdal_io_impl.hpp"

namespace geodec
{

    gdal_file::gdal_file(const std::string& filename) : pimpl(new impl(filename) )
    {
        size_=pimpl->size();
        transform_=pimpl->coord_projected_transform();
    }

    gdal_file::~gdal_file() {}

    boost::array<size_t,4> gdal_file::next_block()
    {
        return pimpl->next_block();
    }

    std::vector<boost::array<double,3>> gdal_file::get_row(size_t iy)
    {
        return pimpl->get_row(iy);
    }

}
