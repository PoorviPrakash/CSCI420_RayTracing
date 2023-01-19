#ifndef XPIC_H
#define XPIC_H

#include <X11/Xlib.h>

#include "pic.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {                /* QUANTIZATION PARAMETERS */
    /* set by caller */
    int gray;                   /* display grayscale(1) or color(0)? */
    int ncol_req;               /* requested max. #colors (0 means no limit) */
    int dither;                 /* dither (only relevant for 8-bit) */
    int newcm;                  /* make new colormap(1) or modify default(0)?*/
    int adaptcm;                /* use adaptive(1) or non-adaptive(0) color 
				   quantization.  Not applicable if gray==1 or
				   depth==24 */
    int depth_req;		/* requested depth (0 means try 24, then 8) */
    float gamma;		/* for gamma correction (suggested: 1.0) */

    /* set by code in xpic.c */
    int depth;                  /* depth in bits per pixel */
    int ncol;                   /* number of colors (or gray levels) */
    int nr, ng, nb;             /* number of levels of R, G, and B
                                   (only relevant if depth<=8, gray=0) */
    unsigned long pv[256];	/* list of ncol usable pixel values */
    Colormap colormap;          /* colormap resource containing chosen colors*/
    Visual *visual;		/* chosen "Visual" (pixel format) */
    int bgr;			/* byte order of 24-bit pixels
				 * bgr=1: byte order is 0BGR
				 * bgr=0: byte order is 0RGB */
} Quantization;

typedef struct Node {	/* NONTERMINAL NODE IN K-D TREE */
    struct Node *left;	/* left son */
    struct Node *right;	/* right son */
    int dim;		/* dimension to split: 0=x, 1=y, 2=z */
    int thresh;		/* split point */
    short pv;		/* -1 for nonterminal Nodes */
} Node;

/*------------------------- general X setup -------------------------*/
extern void xpic_init(Display *d, int s);
extern void xpic_color_init(Quantization *q);
extern Window xpic_window_create(char *winname, Window parent,
				 int x, int y,
				 int width, int height,
				 Quantization *q);

/*---------------------- Pic resampling & quantization ----------------------*/
extern void xpic_resample_and_quantize(Pic *s, Pic *d, Quantization *q);

/*---- we do a few different kinds of quantization now *---------/
/* if depth==24 || depth==0
     resample_and_quantize24
   else if depth==8 || depth==0
     if adaptcm 
       if dither
         quantpic_dith
       else 
         quantpic_dith
     else
       resample_and_quantize8
   else
*/
extern void resample_and_quantize24(register Pic *s, register Pic *d,
				    register Quantization *q);
extern void resample_and_quantize8(register Pic *s, register Pic *d,
				   register Quantization *q);
void resample_and_quantize(register Pic *s, register Pic *d,
			   register Quantization *q, Node *tree, Rgbcolor *cm);
int quantpic_nodith(Pic *s, Pic *d, Quantization *q, Node *root, Rgbcolor *cm);
int quantpic_dith(Pic *s, Pic *d, Quantization *quant, Node *root, Rgbcolor *cm);


/*---------------------- Storing the adaptive colormap --------------------*/
void store_adaptcm_colors(Quantization *q, Rgbcolor *cm);

/*------------------------- send a Pic to X -------------------------*/
extern void xpic_send_free();
extern void xpic_send(Pic *s, Drawable drawable, GC gc,
		      int width, int height, Quantization *q);
extern void new_xpic_send(Pic *s, Drawable drawable, GC gc,
			  int wx, int wy, Quantization *q, 
			  Node *tree, Rgbcolor *cm);


extern Atom WMDeleteWindow;

#ifdef __cplusplus
}
#endif
#endif
