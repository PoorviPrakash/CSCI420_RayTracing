#include <stdio.h>
#include <tiffio.h>
#include "pic.h"


/*
 * tiff: subroutines for reading and writing TIFF picture files.
 *       uses libtiff by Sam Leffler.
 *
 * Michael Garland      17 Jan 96
 *
 */

static void unpack_tiff_raster(Pic *pic, uint32 *raster)
{
    int x,y;
    uint32 pixel;
    uint32 pic_index = 0;

    for(y=(pic->ny-1); y>=0; y--)
	for(x=0; x<pic->nx; x++)
	{
	    pixel = raster[y*pic->nx + x];

	    pic->pix[pic_index++] = TIFFGetR(pixel);
	    pic->pix[pic_index++] = TIFFGetG(pixel);
	    pic->pix[pic_index++] = TIFFGetB(pixel);
	}
}

int tiff_get_size(char *file, int *nx, int *ny)
{
    TIFF *tif = TIFFOpen(file, "r");
    if( !tif )
	return FALSE;

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, nx);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, ny);

    TIFFClose(tif);
    return TRUE;
}


int tiff_write(char *file, Pic *pic)
{
    TIFF *tif;

    uint32 samples_per_pixel = 3;
    uint32 w = pic->nx;
    uint32 h = pic->nx;
    uint32 scanline_size = samples_per_pixel * w;
    uint32 y;
    char *scanline;

    tif = TIFFOpen(file, "w");
    if( !tif )
	return FALSE;

    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, w);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, h);

    /* These are the charateristics of our Pic data */
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, samples_per_pixel);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);

    /*
     * Turn on LZW compression.
     * Shhhhh!  Don't tell Unisys!
     */
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
    /*
     * Predictors:
     *     1 (default) -- No predictor
     *     2           -- Horizontal differencing
     */
    TIFFSetField(tif, TIFFTAG_PREDICTOR, 2);
    
    if( TIFFScanlineSize(tif) != scanline_size )
    {
	fprintf(stderr,
		"TIFF: Mismatch with library's expected scanline size!\n");
	return FALSE;
    }
    
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tif, -1));

    scanline = pic->pix;
    for(y=0; y<h; y++)
    {
	TIFFWriteScanline(tif, scanline, y, 0);
	scanline += w;
    }

    TIFFClose(tif);

    return TRUE;
}

Pic *tiff_read(char *file, Pic *opic)
{
    TIFF *tif;
    unsigned int npixels;
    uint32 *raster;
    int result;
    uint32 w, h;

    Pic *pic;

    tif = TIFFOpen(file, "r");
    if( !tif )
	return NULL;

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);

    npixels = w*h;

    raster = (uint32 *)_TIFFmalloc(npixels * sizeof(uint32));
    if( !raster )
	return NULL;

    result = TIFFReadRGBAImage(tif, w, h, raster, TRUE);
    
    if( opic && npixels == (opic->nx * opic->ny))
	pic = opic;
    else
	pic = pic_alloc(w, h, 3);

    unpack_tiff_raster(pic, raster);


    _TIFFfree(raster);
    TIFFClose(tif);

    return pic;
}
