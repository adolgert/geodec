#ifndef GDAL_IO_IMPL_HPP_
#define GDAL_IO_IMPL_HPP_ 1

#include <sstream>
#include <stdexcept>
#include "ogrsf_frmts.h"
#include "ogr_api.h"
#include "gdal_io_impl.hpp"


namespace geodec
{

    gdal_file::impl::impl(const std::string& filename) {
        OGRRegisterAll();
        GDALAllRegister();
		void* void_ds = GDALOpen(filename.c_str(), GA_ReadOnly);
		if (0==void_ds) {
			std::stringstream msg;
			msg << "Could not open file " << filename;
			throw std::runtime_error(msg.str());
		}
        dataset_ = static_cast<GDALDataset*>(void_ds);
        raster_band_ = dataset_->GetRasterBand(1);
        size_[0]=raster_band_->GetXSize();
        size_[1]=raster_band_->GetYSize();

        int nXBlockSize, nYBlockSize;
        raster_band_->GetBlockSize( &nXBlockSize, &nYBlockSize );
        block_size_[0]=nXBlockSize;
        block_size_[1]=nYBlockSize;
        std::cerr << "block size in x,y " << block_size_[0] << ", "
            << block_size_[1] << std::endl;

        for (size_t cr=0; cr<block_cnt_.size(); cr++) {
            block_cnt_[cr]=(size_[cr] + block_size_[cr] - 1)/
                block_size_[cr];
        }
        int item_bytes=1;
        block_buffer_=(unsigned char*) CPLMalloc(
                                  block_size_[0]*block_size_[1]*item_bytes);
        block_order_=choose_block(block_cnt_);

        geo_xform_=coord_projected_transform();
        this->coordinate_transform(); // sets coord_xform_
    }


    gdal_file::impl::~impl() {
        OGRCoordinateTransformation::DestroyCT(coord_xform_);
        CPLFree(block_buffer_);
        GDALClose(dataset_);
    }


    boost::array<size_t,2> gdal_file::impl::size() { return size_; }

    boost::array<double,6> gdal_file::impl::coord_projected_transform()
    {
        boost::array<double,6> xform;
        auto res = dataset_->GetGeoTransform( &xform[0] );
        if (res != CE_None) {
            std::cerr << "No transform in this tif." << std::endl;
			// Initialize with a simple default transform that says the first
			// coordinate is x and the second coordinate is y.
			xform[0]=0;
			xform[1]=1;
			xform[2]=0;
			xform[3]=0;
			xform[4]=0;
			xform[5]=1;
        }
        return xform;   
    }


	/*! Loads data from a block and returns its extents.
	 *  Before this is called, there is no loaded data.
	 *  \returns Array of (x start, y start, x width, y height)
	 *  If there are no more blocks, then returns array with [2] == 0.
	 */
    boost::array<size_t,4>
    gdal_file::impl::next_block()
    {
        boost::array<size_t,4> indices;
        boost::array<size_t,2> block_idx=block_order_.next();
        if (block_idx!=block_order_.end()) {
            raster_band_->ReadBlock( block_idx[0], block_idx[1],
                                    block_buffer_);
            for (size_t c=0; c<2; c++) {
                indices[c]=block_idx[c]*block_size_[c];
				// Last block in a row or col may be smaller.
				indices[c+2]=block_size_[c];
                if (indices[c]+indices[c+2] > size_[c]) {
                    indices[c+2]=size_[c]-indices[c]*block_size_[c];
                }
            }
        } else {
            indices[2]=0;
        }
        return indices;
    };


    std::vector<boost::array<double,3>>
    gdal_file::impl::get_row(size_t iy)
    {
        std::vector<boost::array<double,3> > coords(block_size_[0]);

        size_t off=iy*size_[0];

        for (size_t ix=0; ix<block_size_[0]; ix++)
        {
            double proj_x, proj_y;
            proj_x=geo_xform_[0]+ix*geo_xform_[1]+iy*geo_xform_[2];
            proj_y=geo_xform_[3]+ix*geo_xform_[4]+iy*geo_xform_[5];
            
            coord_xform_->Transform( 1, &coords[ix][0],
					&coords[ix][1], &coords[ix][2]);
        }

		return coords;
    }



    void gdal_file::impl::coordinate_transform()
    {
        OGRSpatialReference srs;
        const char* proj=dataset_->GetProjectionRef();
        if (NULL == proj) {
            std::cout << "Null projection" << std::endl;
        }
        srs.importFromWkt(const_cast<char**>(&proj));

        UTM_.SetProjCS("UTM 17 / WGS84");
        UTM_.SetWellKnownGeogCS( "WGS84" );
        UTM_.SetUTM( 17 );

        OGRSpatialReference* pLatLong = UTM_.CloneGeogCS();
        coord_xform_ = OGRCreateCoordinateTransformation( &UTM_, pLatLong);
		if (0==coord_xform_) {
			std::cerr << "Could not read a coordinate transformation from "
				      << "this file." << std::endl;
		}
    }
}


#endif // GDAL_IO_IMPL_HPP_
