#include <stdio.h>
#include <tiffio.h>
#include "pic.h"


/*
 * tiff: subroutines for reading and writing TIFF picture files.
 *       uses libtiff by Sam Leffler.
 *
 * Michael Garland      17 Jan 96
 *
 *  - extended to handle 1, 3, 4 samples per pixel
 *            Jovan Popovic
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

	    if (pic->bpp<3) continue;

	    pic->pix[pic_index++] = TIFFGetG(pixel);
	    pic->pix[pic_index++] = TIFFGetB(pixel);

	    if (pic->bpp<4) continue;

	    pic->pix[pic_index++] = TIFFGetA(pixel);

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

    uint32 samples_per_pixel = pic->bpp;
    uint32 w = pic->nx;
    uint32 h = pic->ny;
    uint32 scanline_size = samples_per_pixel * w;
    uint32 y;
    char *scanline, *scanline_buf;

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
    if (samples_per_pixel<3)
	TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    else
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

    scanline_buf = (char *)malloc(scanline_size);
    if (!scanline_buf) {
	fprintf(stderr, "TIFF: Can't allocate scanline buffer!\n");
	return FALSE;
    }
    scanline = pic->pix;
    for(y=0; y<h; y++)
    {
	memcpy(scanline_buf, scanline, scanline_size);
	TIFFWriteScanline(tif, scanline_buf, y, 0);
	    /* note that TIFFWriteScanline modifies the buffer you pass it */
	scanline += scanline_size;
    }
    free(scanline_buf);

    TIFFClose(tif);

    return TRUE;
}

Pic *tiff_read(char *file, Pic *opic)
{
    TIFF *tif;
    unsigned int npixels;
    short samples;
    uint32 *raster;
    int result;
    uint32 w, h;

    Pic *pic;

    tif = TIFFOpen(file, "r");
    if( !tif )
	return NULL;

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samples);

    npixels = w*h;

    raster = (uint32 *)_TIFFmalloc(npixels * sizeof(uint32));
    if( !raster )
	return NULL;

    printf("reading TIFF file %s: %dx%d (%d-bit) pixels\n", file, w, h,
	   8*samples);
    result = TIFFReadRGBAImage(tif, w, h, raster, TRUE);
    
    pic = pic_alloc(w, h, samples, opic);

    unpack_tiff_raster(pic, raster);

    _TIFFfree(raster);
    TIFFClose(tif);

    return pic;
}
