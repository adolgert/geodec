#ifndef _HDF_IO_H_
#define _HDF_IO_H_ 1

#include <stdexcept>
#include <sstream>
#include <boost/array.hpp>

#include "hdf5.h"

class HDF_cluster_grid_file
{
    hid_t file_id_;
    hid_t dataset_;
public:
    HDF_cluster_grid_file(const std::string& filename, size_t w, size_t h)
    {
        H5open();
        file_id_ = H5Fcreate( filename.c_str(), H5F_ACC_TRUNC,
                              H5P_DEFAULT, H5P_DEFAULT );
        if (file_id_ < 0) {
            std::stringstream msg;
            msg << "Could not open file. Exception " << file_id_;
            throw std::runtime_error(msg.str());
        }

        hsize_t dset_dims[2] = { h, w };
        hid_t dset_create_param_list = H5Pcreate(H5P_DATASET_CREATE);
        if (h>512 || w>512) {
            hsize_t chunk_dims[2] = { 256, 256 };
            H5Pset_chunk(dset_create_param_list, 2, chunk_dims);
            size_t num_chunk_slots = 521;
            size_t cache_bytes = 512*1024*1024;
            herr_t cache_err = H5Pset_chunk_cache(
                                                  dset_create_param_list,
                                                  num_chunk_slots,
                                                  cache_bytes,
                                                  H5D_CHUNK_CACHE_W0_DEFAULT
                                                  );
            if (cache_err<0) {
                std::stringstream msg;
                msg << "Could not set chunk cache " << cache_err;
                throw std::runtime_error(msg.str());
            }
        }

        hid_t dataspace = H5Screate_simple(2, dset_dims, NULL);
        if (dataspace<0) {
            std::stringstream msg;
            msg << "Could not create dataspace. " << dataspace;
            throw std::runtime_error(msg.str());
        }

        hid_t dataset_ = H5Dcreate( file_id_, "cluster_ids", 
                                   H5T_NATIVE_INT, dataspace,
                                   dset_create_param_list, H5P_DEFAULT);
        if (dataset_<0) {
            std::stringstream msg;
            msg << "Could not create dataset. " << dataset_;
            throw std::runtime_error(msg.str());
        }

        H5Sclose(dataspace);
        H5Pclose(dset_create_param_list);
    }

    
    /*!
     * http://www.hdfgroup.org/HDF5/doc/H5.intro.html#Intro-PMSelectPoints
     */
    template<class ITER, class PMAP>
    void write_points(ITER begin, ITER end, const PMAP& vals)
    {
        std::vector<boost::array<size_t,2> > indices;
        std::vector<size_t> usage;
        while (begin!=end) {
            indices.push_back({ *begin });
            usage.push_back( vals(begin->id()) );
        }
        
        hsize_t dim2[] = { indices.size() };
        hsize_t mspace_rank = 1;
        hid_t mid2 = H5Screate_simple( mspace_rank, dim2, NULL);
        
        herr_t ret = H5Sselect_elements(fid, H5S_SELECT_SET, indices.size(),
                                        (const hsize_t **) indices.front());
        ret = H5Dwrite(dataset, H5T_NATIVE_INT, mid2, fid, H5P_DEFAULT, values);

        H5Sclose(mid2);
    }


    ~HDF_cluster_grid_file() {
        if (dataset_>=0) {
            H5Dclose(dataset_);
        }
        if (file_id_>=0) {
            H5Fclose(file_id_);
        }
    }


    
};


#endif // _HDF_IO_H_
