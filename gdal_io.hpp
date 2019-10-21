#ifndef _GDAL_IO_HPP_
#define _GDAL_IO_HPP_ 1

#include <memory>
#include <boost/array.hpp>
#include <boost/tuple/tuple.hpp>

namespace geodec
{

    //! Instantiate identify<TEMP_PARAM> blah; in order to print type of TEMP_PARAM.
    /*! This is for debugging. It convinces the compiler to print the name of a type. */
	template<class INCOMPLETE>
	class identify {
	public:
		identify();
	};

    /*! Read from a file and send information on vertices to a builder.
     *  Works with Add_from_file. This class knows what has been read so
     *  far from the file and decides what to read next.
     */
    class gdal_file {
        class impl;
        std::unique_ptr<impl> pimpl;
        
        boost::array<size_t,2> size_;
        boost::array<double,6> transform_;
    public:
        gdal_file(const std::string& filename);
        ~gdal_file();
        boost::array<size_t,4> next_block();
        std::vector<boost::array<double,3>> get_row(size_t iy);
        template<class BUILDER> bool read_block(BUILDER& builder)
        {
			//identify<BUILDER> what(3);
            // This array is x,y and xsize,ysize.
            boost::array<size_t,4> ind=next_block();
            // This is the buffer of data. iX + iY*xblocksize.
            if (ind[2]==0) {
                return false;
            }
            std::cerr << "reading block " << ind[0] << ", " << ind[1] << std::endl;
            std::cerr << "end block " << ind[2] << ", " << ind[3] << std::endl;
            size_t width=size_[0]; // width in x
            boost::array<double,3> loc;
            for (size_t iy=ind[1]; iy<ind[1]+ind[3]+1; iy++)
            {
                std::cerr << "row " << iy << std::endl;
                auto row = get_row(iy);
				size_t ix = ind[0];
                for (auto pr=row.begin(); pr!=row.end(); pr++)
                {
                    loc[0]=pr->at(0);
                    loc[1]=pr->at(1);
                    loc[2]=0;
                    builder.add_vertex( loc, ix+iy*width );
					ix++;
                }
            }

            boost::array<size_t,4> verts;
            for (size_t py=ind[1]; py<ind[1]+ind[3]; py++)
            {
                for (size_t px=ind[0]; px<ind[0]+ind[2]; px++)
                {
                    verts[0]=px+py*width;
                    verts[1]=(px+1)+py*width;
                    verts[2]=(px+1)+(py+1)*width;
                    verts[3]=px+(py+1)*width;
                    builder.add_face( verts, px+py*width );
                }
            }

            return true;
        }
    };

}


#endif // _GDAL_IO_HPP_
