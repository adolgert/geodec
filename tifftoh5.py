'''
This converts a GeoTIFF to HDF5 format with blocks.

In this code, there is some confusion between (x,y) and (y,x).
The rule is this: if the coordinates are referenced separately,
they will be X, then Y. If they are in a tuple, they run with
the fastest coordinate last, so it's (z,y,x) or just (y,x).
'''
import sys
import logging
import numpy as np
import struct
import itertools as it
from default_parser import DefaultArgumentParser
import gdal
from gdalconst import GA_ReadOnly, GDT_Byte
import h5py
from h5py import h5f, h5p, h5s, h5t, h5d


logger = logging.getLogger(__file__)


def hash_pixel(x,y):
    '''
    Just want to make an image where the pixel values will look
    messed up if I mix x and y or fail to copy blocks properly,
    so this encodes x and y. Y is the slowly-changing coordinate.
    '''
    return (x*17 + y*3) % 255



def write_tiff_by_line(band):
    # For GDAL, the x-coordinate is the one that runs faster
    # so it stores to numpy arrays as (y,x).
    logger.debug('write_tiff_by_line x %d y %d' % (band.XSize, band.YSize))
    scanline=np.zeros((1,band.XSize), dtype=np.uint8)
    line_cnt=0
    for yidx in range(band.YSize):
        for xidx in range(band.XSize):
            scanline[0,xidx]=hash_pixel(xidx,yidx)
        band.WriteArray(scanline, xoff=0, yoff=yidx)
        line_cnt+=1
    logger.debug('write_tiff_by_line wrote %d y lines' % line_cnt)
        



def read_by_block(band):
    '''
    Read whatever blocksize the file uses.

    Args: band is a gdal.RasterBand of byte values.
    Yields: ((x,y) of corner, numpy array of values)
    '''
    block=band.GetBlockSize()
    logger.info('block size x %d y %d' % (block[0],block[1]))

    # Note that y and x are switched b/c that's how ReadAsArray works.
    main_buf=np.zeros((block[1],block[0]), dtype=np.uint8)

    for (yidx, xidx) in it.product(range(0, band.YSize, block[1]),
                                   range(0, band.XSize, block[0])):
        ysize=min(block[1], band.YSize-yidx)
        xsize=min(block[0], band.XSize-xidx)
        if xsize is block[0] and ysize is block[1]:
            xform_buf=main_buf
        else:
            xform_buf=np.zeros((ysize, xsize),dtype=np.uint8)

        retbuf=band.ReadAsArray(xoff=xidx, yoff=yidx,
                                win_xsize=xsize, win_ysize=ysize,
                                buf_xsize=xsize, buf_ysize=ysize,
                                buf_obj=xform_buf)
        if np.all(xform_buf==0):
            logger.debug('All of the returned block is zero for y %d x %d' %
                (yidx, xidx))
        assert(retbuf is xform_buf)
        yield ((yidx,xidx), (ysize, xsize), xform_buf)



def open_h5(filename, dims, chunks=(512,512)):
    '''
    dims is a tuple of (x,y) dimensions of the dataset to create.
    In HDF5, files are in C order, so the last-listed dimension
    changes the fastest.
    '''
    assert(isinstance(filename,str))
    assert(isinstance(dims,tuple))
    assert(len(dims)==2)
    assert(isinstance(chunks,tuple))
    assert(len(chunks)==len(dims))

    file_access_property_list=h5p.create(h5p.FILE_ACCESS)
    _1MB=1024*1024
    # cache_info is a tuple of
    # (INT # metadata objects, INT # number raw data chunks,
    # UINT nbytes raw data cache, DOUBLE preemption policy for data cache)
    file_access_property_list.set_cache(0, 521, 512*_1MB, .75)

    datafile=h5f.create(filename, h5f.ACC_TRUNC, None,
                         file_access_property_list)


    dataset_create_params=h5p.create(h5p.DATASET_CREATE)
    dataset_create_params.set_chunk(chunks)

    dataspace=h5s.create_simple(dims,dims)

    dtype=np.uint8
    dataset=h5d.create(datafile, 'ds', h5t.STD_U8LE, dataspace,
                       dataset_create_params, None)
    
    return (datafile, dataset)




def read_h5(filename):
    assert(isinstance(filename,str))
    datafile=h5f.open(filename, h5f.ACC_RDONLY)
    dataset=h5d.open(datafile, 'ds')
    dataset_space=dataset.get_space() # returns a SpaceID
    dataset_type=dataset.get_type() # returns a TypeID
    logger.info('datatype %s data rank %d data shape %s' % \
                    (str(dataset.dtype), dataset.rank, str(dataset.shape)))
    buf=np.zeros(dataset.shape, dtype=dataset.dtype)
    logger.info('h5 dataset shape %s' % str(buf.shape))
    memory_space=h5s.ALL
    dataset.read(memory_space, dataset_space, buf)
    datafile.close()
    return buf




def write_block(dataset, corner, blocksize, buffer):
    file_space=dataset.get_space()
    assert(isinstance(file_space,h5s.SpaceID))
    file_slab=file_space.select_hyperslab(corner, blocksize)

    mem_space=h5s.create_simple(blocksize, blocksize)

    dataset.write(mem_space, file_space, buffer)




def copy_file(readf, writef):
    assert(isinstance(readf,str))
    assert(isinstance(writef,str))
    in_ds=gdal.Open(readf, GA_ReadOnly)
    band=in_ds.GetRasterBand(1)
    logger.info('xsize: %d ysize: %d' % (band.XSize, band.YSize))

    file_id, ds_id=open_h5(writef, (band.YSize, band.XSize))

    byte_cnt=0
    for coord, block_size, line in read_by_block(band):
        byte_cnt+=line.size
        write_block(ds_id, coord, block_size, line)

    logger.info('Read %d bytes' % byte_cnt)

    file_id.close()




def verify_file(readf):
    assert(isinstance(readf,str))
    in_ds=gdal.Open(readf, GA_ReadOnly)
    band=in_ds.GetRasterBand(1)
    logger.info('verify xsize: %d ysize: %d' % (band.XSize, band.YSize))

    byte_cnt=0
    for (y, x), (by, bx), line in read_by_block(band):
        byte_cnt+=line.size
        for (j,i) in it.product(range(y,y+by), range(x,x+bx)):
            assert(hash_pixel(i,j)==line[j-y,i-x])

    logger.info('Read %d bytes' % byte_cnt)
    del band
    del in_ds





def test_mini():
    infile='/media/data0/2011_cdls_ohio/2011_cdls_mini.img'
    output='larry.h5'
    copy_file(infile,output)



def test_faux():
    artificial_file='fake.img'
    xsize=517
    ysize=1032
    output='fake.h5'
    driver=gdal.GetDriverByName('HFA') # 'GTiff', 'HFA'
    dataset=driver.Create(artificial_file, xsize=xsize, ysize=ysize,
        bands=1, eType=gdal.GDT_Byte)
    band=dataset.GetRasterBand(1)
    band.Fill(0)
    write_tiff_by_line(band)
    # Closing the file is important for being able to read it properly.
    del band
    del dataset
    del driver
    logger.info('now testing a read of %s' % artificial_file)
    verify_file(artificial_file)
    copy_file(artificial_file, output)
    data=read_h5(output)
    # The HDF5 output is transposed.
    i=0
    for (x,y) in it.product(range(data.shape[0]),range(data.shape[1])):
        if hash_pixel(y,x)!=data[x,y] and i<10:
            logger.error('x %d y %d val %d hash %d' % \
                             (x,y,data[x,y], hash_pixel(x,y)))
            i+=1



def suite():
    import unittest
    suite=unittest.TestSuite()
    loader=unittest.TestLoader()
    suite.addTest(unittest.FunctionTestCase(test_mini))
    suite.addTest(unittest.FunctionTestCase(test_faux))
    return suite



if __name__ == '__main__':
    parser=DefaultArgumentParser(description='convert tiff to hdf5',
                                 suite=suite())
    parser.add_function('convert', 'convert from GeoTIFF to HDF5')
    parser.add_argument('--in', type=str,
                        help='name of input file to convert')
    parser.add_argument('--out', type=str,
                        help='name of output file to write')
    args=parser.parse_args()

    infile=getattr(args,'in') # Because in is a keyword.
    outfile=args.out

    err=False
    if not isinstance(infile,str):
        logger.error('Use --in to choose an input file')
        err=True
    if not isinstance(outfile,str):
        logger.error('Use --out to choose an output file')
        err=True
    if not err:
        copy_file(infile,outfile)

