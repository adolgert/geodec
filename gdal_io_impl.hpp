#ifndef _GDAL_IO_IMPL_HPP_
#define _GDAL_IO_IMPL_HPP_ 1

#include <boost/array.hpp>
#include <boost/tuple/tuple.hpp>
#include "gdal/gdal.h"
#include "gdal/gdal_priv.h"
#include "gdal/ogr_api.h"
#include "gdal_io.hpp"
#include "gdal/ogr_spatialref.h"
#include "gdal/ogrsf_frmts.h"


namespace geodec
{

	/*! This iterates over a matrix of blocks.
	 *  It does not embody iterator concepts. More ad-hoc.
	 */
    class choose_block {
        boost::array<size_t,2> cnt_;
        boost::array<size_t,2> cur_;
    public:
        choose_block() {}
        choose_block(boost::array<size_t,2> cnt) : cnt_(cnt)
        {
            cur_[0]=0;
            cur_[1]=0;
        }
        boost::array<size_t,2> next() {
            auto val=cur_;
            cur_[0]++;
            if (cur_[0]>cnt_[0]) {
                cur_[0]=0;
                cnt_[1]++;
            }
            return val;
        }
        boost::array<size_t,2> end() { return cnt_; }
    };



    class gdal_file::impl
    {
        GDALDataset* dataset_;
        GDALRasterBand* raster_band_;
        boost::array<size_t,2> block_size_;
        boost::array<size_t,2> block_cnt_;
        boost::array<size_t,2> size_;
        unsigned char* block_buffer_;
        choose_block block_order_;
        OGRSpatialReference UTM_;
        OGRCoordinateTransformation* coord_xform_;
        boost::array<double,6> geo_xform_;
    public:
        impl(const std::string& filename);
        ~impl();
		//! Gets the coordinates of the next block to read and loads data.
        boost::array<size_t,4> next_block();
		//! Retrieve a row of data from that block.
        std::vector<boost::array<double,3>> get_row(size_t iy);
		//! The extent of the whole data array.
        boost::array<size_t,2> size();
		//! GDAL params to transform from matrix location to projected coords.
        boost::array<double,6> coord_projected_transform();
		void coordinate_transform();
    };
}


#endif // _GDAL_IO_IMPL_HPP_
