#include <stdio.h>
#include <stdlib.h>
//#include <tiffio.h>
#include "jpeglib.h"
#include "pic.h"

#define QUALITY 95
/*
 * jpeg: subroutines for reading and writing JFIF (JPEG standard)  picture 
 *       files.  Code adjusted from the example.c provided with libjpeg 
 *       library uses libjpeg by IJG
 *
 * Christopher Rodriguez     Jan 31, 2000
 *
 */

int jpeg_write(char *filename, Pic *pic) {
  JSAMPLE *image_buffer = pic->pix;
  int image_width = pic->nx;
  int image_height = pic->ny;
  int row_stride;

  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;

  FILE *outfile;
  JSAMPROW row_pointer[1];

  if (pic->bpp != 3) {
    fprintf(stderr, "Cannot create jpeg from this Pic.\n");
    fprintf(stderr, "Need bits per pixel to be 3.\n");
    return FALSE;
  }
    
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);
  
  if ((outfile = fopen(filename, "wb")) == NULL) {
    fprintf(stderr, "can't open file for output: %s\n", filename);
    exit(1);
  }
  
  jpeg_stdio_dest(&cinfo, outfile);

  cinfo.image_width = image_width; 	/* image width and height, in pixels */
  cinfo.image_height = image_height;
  cinfo.input_components = 3;		/* # of color components per pixel */
  cinfo.in_color_space = JCS_RGB; 	/* colorspace of input image */
  
  jpeg_set_defaults(&cinfo);
  
  jpeg_set_quality(&cinfo, QUALITY, TRUE);

  jpeg_start_compress(&cinfo, TRUE);

  row_stride = image_width * 3;

  while(cinfo.next_scanline < cinfo.image_height) {
    row_pointer[0] = &image_buffer[cinfo.next_scanline * row_stride];
    (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  jpeg_finish_compress(&cinfo);

  fclose(outfile);
  
  jpeg_destroy_compress(&cinfo);

  return TRUE;
}

Pic *jpeg_read(char *filename, Pic *opic) {
  Pic *retval = NULL;
  int image_width;
  int image_height;

  struct jpeg_decompress_struct cinfo;

  struct jpeg_error_mgr jerr;

  FILE * infile;		/* source file */
  JSAMPROW row_pointer[1];	/* pointer to JSAMPLE row[s] */
  int row_stride;		/* physical row width in output buffer */
  int crows=0;

  /* In this example we want to open the input file before doing anything else,
   * so that the setjmp() error recovery below can assume the file is open.
   * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
   * requires it in order to read binary files.
   */

  if ((infile = fopen(filename, "rb")) == NULL) {
    fprintf(stderr, "can't open %s\n", filename);
    return 0;
  }

  /* Step 1: allocate and initialize JPEG decompression object */

  /* We set up the normal JPEG error routines, then override error_exit. */
  cinfo.err = jpeg_std_error(&jerr);

  /* Now we can initialize the JPEG decompression object. */
  jpeg_create_decompress(&cinfo);

  /* Step 2: specify data source (eg, a file) */
  jpeg_stdio_src(&cinfo, infile);

  /* Step 3: read file parameters with jpeg_read_header() */
  (void) jpeg_read_header(&cinfo, TRUE);

  /* We can ignore the return value from jpeg_read_header since
   *   (a) suspension is not possible with the stdio data source, and
   *   (b) we passed TRUE to reject a tables-only JPEG file as an error.
   * See libjpeg.doc for more info.
   */

  /* Step 4: set parameters for decompression */

  /* In this example, we don't need to change any of the defaults set by
   * jpeg_read_header(), so we do nothing here.
   */

  /* Step 5: Start decompressor */

  (void) jpeg_start_decompress(&cinfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */

  /* We may need to do some setup of our own at this point before reading
   * the data.  After jpeg_start_decompress() we have the correct scaled
   * output image dimensions available, as well as the output colormap
   * if we asked for color quantization.
   * In this example, we need to make an output work buffer of the right size.
   */ 
  /* JSAMPLEs per row in output buffer */
  row_stride = cinfo.output_width * cinfo.output_components;
  /* Make a one-row-high sample array that will go away when done with image */
  retval = pic_alloc(cinfo.image_width, cinfo.image_height, 
		     cinfo.output_components, opic);

  /* Step 6: while (scan lines remain to be read) */
  /*           jpeg_read_scanlines(...); */

  /* Here we use the library's state variable cinfo.output_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   */
  while (cinfo.output_scanline < cinfo.output_height) {
    /* jpeg_read_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could ask for
     * more than one scanline at a time if that's more convenient.
     */
    row_pointer[0] = & retval->pix[crows * row_stride];
    (void) jpeg_read_scanlines(&cinfo, row_pointer, 1);
    
    crows++;
  }

  /* Step 7: Finish decompression */

  (void) jpeg_finish_decompress(&cinfo);
  /* We can ignore the return value since suspension is not possible
   * with the stdio data source.
   */

  /* Step 8: Release JPEG decompression object */

  /* This is an important step since it will release a good deal of memory. */
  jpeg_destroy_decompress(&cinfo);

  /* After finish_decompress, we can close the input file.
   * Here we postpone it until after no more JPEG errors are possible,
   * so as to simplify the setjmp error logic above.  (Actually, I don't
   * think that jpeg_destroy can do an error exit, but why assume anything...)
   */
  fclose(infile);

  /* At this point you may want to check to see whether any corrupt-data
   * warnings occurred (test whether jerr.pub.num_warnings is nonzero).
   */

  /* And we're done! */
  return retval;


}

int jpeg_get_size(char *file, int *nx, int *ny) {
  Pic *retval = NULL;

  struct jpeg_decompress_struct cinfo;

  FILE * infile;		/* source file */

  /* In this example we want to open the input file before doing anything else,
   * so that the setjmp() error recovery below can assume the file is open.
   * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
   * requires it in order to read binary files.
   */

  if ((infile = fopen(file, "rb")) == NULL) {
    fprintf(stderr, "can't open %s\n", file);
    return 0;
  }

  /* Now we can initialize the JPEG decompression object. */
  jpeg_create_decompress(&cinfo);

  /* Step 2: specify data source (eg, a file) */
  jpeg_stdio_src(&cinfo, infile);

  /* Step 3: read file parameters with jpeg_read_header() */
  (void) jpeg_read_header(&cinfo, TRUE);

	*nx = cinfo.image_width;
	*ny = cinfo.image_height;
	
  /* This is an important step since it will release a good deal of memory. */
  jpeg_destroy_decompress(&cinfo);

  /* After finish_decompress, we can close the input file.
   * Here we postpone it until after no more JPEG errors are possible,
   * so as to simplify the setjmp error logic above.  (Actually, I don't
   * think that jpeg_destroy can do an error exit, but why assume anything...)
   */
  fclose(infile);

	return 1;
}
