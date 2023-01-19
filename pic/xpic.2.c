/*
 * xpic: subroutines for displaying 8 or 24-bit pictures in X windows.
 *
 * possible features to add:
 *	better image magnification: bilinear interp
 *	better quantization (use median cut algorithm?)
 *	handle gamma correction better?
 *		Currently, the code ignores gamma for colormap-less displays
 *		(TrueColor or StaticGray), and doesn't take gamma correction
 *		into account when dithering.
 *
 * Paul Heckbert	18 Jan 94, 25 Jan 94
 *
 * 4/20/95 switched from rand() to random() because former has poor low bits.
 */

#include <stdio.h>
#include <assert.h>
#include <math.h>

#include <X11/Xlib.h>		/* basic X datatypes */
#include <X11/Xutil.h>
#include <X11/Xos.h>		/* X portability */

#include "xpic.h"

#define UNDEF 0			/* magic value signifying undefined */


/*
 * a list of reasonable quantizations with a range of number of colors
 * from 256 to 8. Useful when other X applications are using a lot of
 * the colormap, or when depth is only 4 bits, say.
 * We weight green most and blue least to reflect human color sensitivity.
 * This list should be sorted in decreasing order by nr*ng*nb.
 */
static struct {int nr, ng, nb;} levels[]
    /* nr = #levels of red, ng = #levels of green, nb = #levels of blue */
= { 
    8, 8, 4,	/* standard 3-bits-red, 3-bits-green, 2-bits-blue combo */
    7, 8, 4,
    6, 8, 4,
    6, 7, 4,
    5, 7, 4,
    5, 7, 3,
    4, 7, 3,
    4, 6, 3,
    4, 6, 2,
    4, 5, 2,
    3, 5, 2,
    3, 4, 2,
    3, 3, 2,
    2, 3, 2,
    2, 2, 2,
};
#define NLEVELS (sizeof levels/sizeof levels[0])

static Display *disp;
static int scr;


/*------------------------- halftoning stuff -------------------------*/

#ifdef HALFTONE
void compute_halftone (PPixel *img, int width, int height, PPixel *halftone)
{

int halfon;                     /* threshold for pixel being turned on */
int allon;                      /* maximum possible input gray level */
int irow,icol;                  /* row and column counters */
int halftone_rows;              /* number of rows in halftone image */
int halftone_columns;           /* number of columns in halftone image */
int max_index;                  /* max (halftone_rows,halftone_columns) */
int *index_lookup;              /* gives index of img for index of halftone */
int halftone_offset;            /* offset into a row of halftone image */
int img_row;                    /* row index of the input image */
int value;                      /* gray level attempting to be represented */
int error;                      /* error in binary representation of value */
int column_error;               /* error to propagate to next column */
int *row_error;                 /* array of current row errors */
int *next_row_error;            /* error to propagate to next row */
int *temp_error;                /* for exchanging arrays */

halftone_rows = height;
halftone_columns = width;

  /* allocate space for error arrays */
  if ((row_error = (int *) malloc ((halftone_columns+1) * sizeof (int))) == 0)
        fprintf (stderr,"compute_halftone: cannot allocate enough memory\n");

  if ((next_row_error = (int *) malloc ((halftone_columns+1) * sizeof (int))) == 0)
        fprintf (stderr,"compute_halftone: cannot allocate enough memory\n");

  /* lookup table giving index to pick out of img for an index in halftone */
  if (halftone_rows > halftone_columns) max_index = halftone_rows;
  else max_index = halftone_columns;
  if ((index_lookup = (int *) malloc ((max_index) * sizeof (int))) == 0)
        fprintf (stderr,"compute_halftone: cannot allocate enough memory\n");
  for (icol = 0; icol < max_index; icol++)
        index_lookup[icol] = icol;

  /* zero errors */
  for (next_row_error[0]=0,icol=0; icol<halftone_columns; icol++) row_error[icol]=0;

  /* do the halftoning */
  halfon = 128; allon = 255;

  for (irow = 0; irow < halftone_rows; irow++)
    {
    img_row = index_lookup[irow];
    halftone_offset = irow * halftone_columns;
    column_error = 0;
    for (icol = 0; icol < halftone_columns; icol++)
      {
      value = (int)(*(img+width*img_row+index_lookup[icol])
               + column_error + row_error[icol]);
      if (value >= halfon)
        {
        halftone[halftone_offset + icol] = 1;
        error = value - allon;
        }
      else
        {
        halftone[halftone_offset + icol] = 0;
        error = value;
        }
      column_error = 3 * error >> 3;  /* 3/8 * error */
      next_row_error[icol] += column_error;
      next_row_error[icol+1] = error >> 2;    /* 1/4 * error */
      }

  /* exchange row_err and next_row_err, zero next_row_err[0] */
    temp_error = row_error;
    row_error = next_row_error;
    next_row_error = temp_error;
    next_row_error[0] = 0;
    }

  free (row_error);
  free (next_row_error);
  free (index_lookup);

  return;
}
#endif /*HALFTONE*/

/*------------------------- X colormap stuff -------------------------*/

static char *visual_class_name(int clss) {
    switch (clss) {
	case PseudoColor: return "PseudoColor";
	case GrayScale:   return "GrayScale";
	case StaticGray:  return "StaticGray";
	case StaticColor: return "StaticColor";
	case DirectColor: return "DirectColor";
	case TrueColor:   return "TrueColor";
	default:          return "unknown_class!";
    }
}

/*
 * store_grays: set colors in q->colormap with a ramp of q->ncol grays,
 * with pixel values to use in q->pv array.
 * Used for GrayScale and DirectColor visual classes, primarily,
 * and when a PseudoColor user requests grayscale.
 */
static void store_grays(Quantization *q) {
    int i, v, u;
    XColor color[256];

    for (i=0; i<q->ncol; i++) {
	v = q->pv[i];
	    /* following line is stupid! why does X require this nonsense? */
	if (q->depth==24) color[i].pixel = v<<16 | v<<8 | v;
	else		  color[i].pixel = v;
	u = 65536.*pow((i+.5)/q->ncol, 1./q->gamma);
	color[i].red   = u;
	color[i].green = u;
	color[i].blue  = u;
	color[i].flags = DoRed|DoGreen|DoBlue;
    }
    XStoreColors(disp, q->colormap, color, q->ncol);
}

/*
 * store_colors: set colors in q->colormap with number of levels of red,
 * green, and blue = (q->nr,q->ng,q->nb), respectively, and pixel values
 * in q->pv array.
 * Used for PseudoColor visual class.
 */
static void store_colors(Quantization *q) {
    int i;
    float r, g, b;
    XColor color[256];

    for (i=0; i<q->ncol; i++) {
	color[i].pixel = q->pv[i];
	r = (i/(q->nb*q->ng)+.5)/q->nr;		/* between 0 and 1 */
	g = (i/q->nb%q->ng+.5)/q->ng;
	b = (i%q->nb+.5)/q->nb;
	color[i].red   = 65535.*pow(r, 1./q->gamma);
	color[i].green = 65535.*pow(g, 1./q->gamma);
	color[i].blue  = 65535.*pow(b, 1./q->gamma);
	/*
	printf("cm[%3d]=cm[%d,%d,%d]=(%3d,%3d,%3d) pv=%d\n",
	    i, i/(q->nb*q->ng), i/q->nb%q->ng, i%q->nb,
	    color[i].red>>8, color[i].green>>8, color[i].blue>>8, q->pv[i]);
	*/
	color[i].flags = DoRed|DoGreen|DoBlue;
    }
    XStoreColors(disp, q->colormap, color, q->ncol);
}

/*
 * store_adaptcm_colors: set colors in q->colormap according to colors in cm
 * and pixel values in q->pv array.
 * Used for PseudoColor visual class.
 */
void store_adaptcm_colors(Quantization *q, Rgbcolor *cm) {
    int i;
    static XColor color[256];
    float r, g, b;

    for (i=0; i<q->ncol; i++) {
	color[i].pixel = q->pv[i];

	r = cm[i].r/256.0; g = cm[i].g/256.0; b = cm[i].b/256.0;
	color[i].red   = (unsigned short)(65535.*pow(r, 1./q->gamma));
	color[i].green = (unsigned short)(65535.*pow(g, 1./q->gamma));
	color[i].blue  = (unsigned short)(65535.*pow(b, 1./q->gamma));

	/*fprintf(stderr,"cm[%3d]=cm[%d,%d,%d]=(%3d,%3d,%3d) pv=%d\n",
	  i, i/(q->nb*q->ng), i/q->nb%q->ng, i%q->nb,
	  color[i].red>>8, color[i].green>>8, color[i].blue>>8, q->pv[i]);*/

	color[i].flags = DoRed|DoGreen|DoBlue;
    }

    XStoreColors(disp, q->colormap, color, q->ncol);
}

/* available_colors: find how many colors I can grab from default colormap */
static int available_colors() {
    int n0, n1, ncolor;
    unsigned long pv[256];

    n0 = 0;
    n1 = 257;
    while (n1-n0>1) {		/* binary search */
	ncolor = n0+n1>>1;
	if (XAllocColorCells(disp, DefaultColormap(disp, scr),
	    /*contig. planes?*/ 0, /*plane masks*/ 0, /*nplanes*/ 0,
	    /*list of pixel values*/ pv, ncolor)) {
		n0 = ncolor;
		XFreeColors(disp, DefaultColormap(disp, scr),
		    pv, ncolor, /*plane mask*/ 0);
	}
	else n1 = ncolor;
    }
    /* leaves colors unallocated */
    return n0;
}

/* NEW VERSION!!
 * modify_colormap: find the best quantization scheme with no more colors
 * than the number available in the default colormap or the number requested,
 * and set those colors in the default colormap.
 * The requested maximum number of colors is provided in q->ncol_req
 *	(q->ncol_req=0 means no upper limit)
 * If q->gray!=0 then creates a gray colormap, else a color one.
 * Puts the chosen quantization parameters in structure q, sets q->colormap.
 */
static void new_modify_colormap(Quantization *q) {
    int i, ncol_avail, alloc_ok;

    ncol_avail = available_colors();
    printf("%d colors available, ", ncol_avail);
    if (q->ncol_req)
	if (q->ncol_req>ncol_avail) {
	    fprintf(stderr, "but you requested %d colors; can't do it\n",
		q->ncol_req);
	    exit(1);
	}
	else ncol_avail = q->ncol_req;	/* user asked for <= number possible */

    if (q->gray) {
	q->ncol = ncol_avail;
	printf("using %d gray levels\n", q->ncol);
    }
    else if (!q->adaptcm) {
	/* find largest combination of #levels of R, G, and B that will fit */
	for (i=0; i<NLEVELS &&
	    levels[i].nr*levels[i].ng*levels[i].nb > ncol_avail; i++);
	if (i>=NLEVELS) {
	    fprintf(stderr, "I can't do anything with %d colors\n", ncol_avail);
	    exit(1);
	}

	/* set the number of levels of red, green, and blue that we will use */
	q->nr = levels[i].nr;
	q->ng = levels[i].ng;
	q->nb = levels[i].nb;
	q->ncol = q->nr*q->ng*q->nb;
	printf("using %dx%dx%d=%d colors\n", q->nr, q->ng, q->nb, q->ncol);
    }
    else {
      q->ncol = ncol_avail;  /* adaptcm; we'll choose colors later */
      printf("allocating %d colors\n", q->ncol);      
    }

    /* request ncol pixel values, put a list of them in pv */
    alloc_ok = XAllocColorCells(disp, DefaultColormap(disp, scr),
	/*contig. planes?*/ 0, /*plane masks*/0, /*nplanes*/ 0,
	/*list of pixel values*/ q->pv, q->ncol);
    assert(alloc_ok);

    q->colormap = DefaultColormap(disp, scr);
    if (q->gray) store_grays(q); else if (!q->adaptcm) store_colors(q);
    /* else adaptcm; we'll choose colors later */
}

/* NEW VERSION!!
 * new_colormap: create a new Colormap resource containing a colormap:
 *    if (gray) then 8-bit grayscale,
 *    else 3 bits red, 3 bits green, 2 bits blue
 * install it in hardware, and set q->colormap.
 * Puts the chosen quantization parameters in structure q.
 * Used for either PseudoColor, GrayScale, or DirectColor visual classes.
 */
static void new_new_colormap(Quantization *q) {
    int dummy, v;

    assert(q->depth>=8);
    assert(q->visual->map_entries>=256);

    /* create new Colormap resource */
    dummy = q->visual->class==TrueColor || q->visual->class==StaticGray ||
	q->visual->class==StaticColor;	/* not really a colormap at all? */
    q->colormap = XCreateColormap(disp, RootWindow(disp, scr),
	q->visual, dummy ? AllocNone : AllocAll);
    q->ncol = 256;
    if (dummy) return;

    printf("using 256 colors\n");
    for (v=0; v<256; v++) q->pv[v] = v;		/* for store_grays or _colors */
    if (q->gray || q->depth==24)
	store_grays(q);
    else if (!q->adaptcm) {
        q->nr = q->ng = 8;	/* 3 bits red, 3 green, 2 blue*/
	q->nb = 4;
	store_colors(q);
    }
    /* else, adaptcm; we'll choose colors later */
    XInstallColormap(disp, q->colormap);	/* put cm into hardware! */
}

/* OLD VERSION!!!
 * modify_colormap: find the best quantization scheme with no more colors
 * than the number available in the default colormap or the number requested,
 * and set those colors in the default colormap.
 * The requested maximum number of colors is provided in q->ncol_req
 *	(q->ncol_req=0 means no upper limit)
 * If q->gray!=0 then creates a gray colormap, else a color one.
 * Puts the chosen quantization parameters in structure q, sets q->colormap.
 */
static void modify_colormap(Quantization *q) {
    int i, ncol_avail, alloc_ok;

    ncol_avail = available_colors();
    printf("%d colors available, ", ncol_avail);
    if (q->ncol_req)
	if (q->ncol_req>ncol_avail) {
	    fprintf(stderr, "but you requested %d colors; can't do it\n",
		q->ncol_req);
	    exit(1);
	}
	else ncol_avail = q->ncol_req;	/* user asked for <= number possible */

    if (q->gray) {
	q->ncol = ncol_avail;
	printf("using %d gray levels\n", q->ncol);
    }
    else {
	/* find largest combination of #levels of R, G, and B that will fit */
	for (i=0; i<NLEVELS &&
	    levels[i].nr*levels[i].ng*levels[i].nb > ncol_avail; i++);
	if (i>=NLEVELS) {
	    fprintf(stderr, "I can't do anything with %d colors\n", ncol_avail);
	    exit(1);
	}

	/* set the number of levels of red, green, and blue that we will use */
	q->nr = levels[i].nr;
	q->ng = levels[i].ng;
	q->nb = levels[i].nb;
	q->ncol = q->nr*q->ng*q->nb;
	printf("using %dx%dx%d=%d colors\n", q->nr, q->ng, q->nb, q->ncol);
    }

    /* request ncol pixel values, put a list of them in pv */
    alloc_ok = XAllocColorCells(disp, DefaultColormap(disp, scr),
	/*contig. planes?*/ 0, /*plane masks*/0, /*nplanes*/ 0,
	/*list of pixel values*/ q->pv, q->ncol);
    assert(alloc_ok);

    q->colormap = DefaultColormap(disp, scr);
    if (q->gray) store_grays(q); else store_colors(q);
}

/* OLD VERSION !!!!!!!!!
 * new_colormap: create a new Colormap resource containing a colormap:
 *    if (gray) then 8-bit grayscale,
 *    else 3 bits red, 3 bits green, 2 bits blue
 * install it in hardware, and set q->colormap.
 * Puts the chosen quantization parameters in structure q.
 * Used for either PseudoColor, GrayScale, or DirectColor visual classes.
 */
static void new_colormap(Quantization *q) {
    int dummy, v;

    assert(q->depth>=8);
    assert(q->visual->map_entries>=256);

    /* create new Colormap resource */
    dummy = q->visual->class==TrueColor || q->visual->class==StaticGray ||
	q->visual->class==StaticColor;	/* not really a colormap at all? */
    q->colormap = XCreateColormap(disp, RootWindow(disp, scr),
	q->visual, dummy ? AllocNone : AllocAll);
    q->ncol = 256;
    if (dummy) return;

    printf("using 256 colors\n");
    for (v=0; v<256; v++) q->pv[v] = v;		/* for store_grays or _colors */
    if (q->gray || q->depth==24)
	store_grays(q);
    else {
	q->nr = q->ng = 8;			/* 3 bits red, 3 green, 2 blue*/
	q->nb = 4;
	store_colors(q);
    }
    XInstallColormap(disp, q->colormap);	/* put cm into hardware! */
}

/* NEW VERSION!!
 * xpic_colormap_init: initialize colors in colormap according to capabilities
 * of X window system, and desires of caller:
 *	q->gray=1 requests grayscale
 *	q->ncol_req specifies a maximum number of colors to use, if nonzero
 *	q->newcm specifies whether a new colormap should be created,
 *	    or default colormap should be modified.
 */

static void new_xpic_colormap_init(Quantization *q) {
    if (q->depth<8) {
	fprintf(stderr,
	"I don't do %d-bit graphics, sorry. Try me on another workstation\n",
	    q->depth);
	exit(1);
    }

    switch (q->visual->class) {
	case GrayScale:			/* grayscale with colormap */
	    q->gray = 1;

	    /* adaptive colormap quantization not implemented for this case */
	    q->adaptcm = 0;

	    /* fall through */
	case PseudoColor:		/* colormapped */
	    if (q->newcm) new_new_colormap(q);
	    else new_modify_colormap(q);
	    break;
	case StaticGray:		/* grayscale without colormap */
            assert(q->depth==8);
	    q->gray = 1;

	    /* adaptive colormap quantization not implemented for this case */
	    q->adaptcm = 0;

	    if (q->newcm) new_new_colormap(q);	/* create dummy colormap */
	    break;
	case DirectColor:		/* segregated with colormap */
	    assert(q->depth==24);
	    new_new_colormap(q);
	    break;
	case TrueColor:			/* segregated without colormap */
	    assert(q->depth==24);
	    if (q->newcm) new_new_colormap(q);	/* create dummy colormap */
	    break;
	default:
	    printf("I don't know how to deal with Visual class %s\n",
		visual_class_name(q->visual->class));
	    exit(1);
    }
}

/* OLD VERSION!!!!!
 * xpic_colormap_init: initialize colors in colormap according to capabilities
 * of X window system, and desires of caller:
 *	q->gray=1 requests grayscale
 *	q->ncol_req specifies a maximum number of colors to use, if nonzero
 *	q->newcm specifies whether a new colormap should be created,
 *	    or default colormap should be modified.
 */

static void xpic_colormap_init(Quantization *q) {
    if (q->depth<8) {
	fprintf(stderr,
	"I don't do %d-bit graphics, sorry. Try me on another workstation\n",
	    q->depth);
	exit(1);
    }

    switch (q->visual->class) {
	case GrayScale:			/* grayscale with colormap */
	    q->gray = 1;
	    /* fall through */
	case PseudoColor:		/* colormapped */
	    if (q->newcm) new_colormap(q);
	    else modify_colormap(q);
	    break;
	case StaticGray:		/* grayscale without colormap */
            assert(q->depth==8);
	    q->gray = 1;
	    if (q->newcm) new_colormap(q);	/* create dummy colormap */
	    break;
	case DirectColor:		/* segregated with colormap */
	    assert(q->depth==24);
	    new_colormap(q);
	    break;
	case TrueColor:			/* segregated without colormap */
	    assert(q->depth==24);
	    if (q->newcm) new_colormap(q);	/* create dummy colormap */
	    break;
	default:
	    printf("I don't know how to deal with Visual class %s\n",
		visual_class_name(q->visual->class));
	    exit(1);
    }
}

/*------------------------- general X setup -------------------------*/

Atom WMDeleteWindow;

void xpic_init(Display *d, int s) {
    disp = d;
    scr = s;

    WMDeleteWindow = XInternAtom(disp, "WM_DELETE_WINDOW", False);
}

/*
 * xpic_visual_init: sets q->depth and q->visual
 */
static void xpic_visual_init(Quantization *q) {
    XVisualInfo vinfo;

    if (q->depth_req!=0 && q->depth_req!=8 && q->depth_req!=24) {
	fprintf(stderr, "Sorry, my code doesn't do depth %d\n", q->depth_req);
	exit(1);
    }
    printf("default visual is %d bit %s, ", DefaultDepth(disp, scr),
	visual_class_name(DefaultVisual(disp, scr)->class));

    /*
     * rather than use the DefaultVisual, which is sometimes 8 bits even
     * on 24-bit workstations, we'll explicitly request what we want
     */
    q->visual = 0;
    if (q->depth_req==24 || q->depth_req==UNDEF)	/* try to get 24 bits */
	if (q->gamma==1. &&
		XMatchVisualInfo(disp, scr, 24, TrueColor, &vinfo) ||
	    XMatchVisualInfo(disp, scr, 24, DirectColor, &vinfo))
		q->visual = vinfo.visual;
	/* try to use TrueColor if gamma=1 (looks better on sun4's)
	 * otherwise use DirectColor, which has a colormap */
    if (q->depth_req==8 || !q->visual)			/* try to get 8 bits */
	if (XMatchVisualInfo(disp, scr, 8, PseudoColor, &vinfo) ||
	    XMatchVisualInfo(disp, scr, 8, GrayScale, &vinfo) ||
	    XMatchVisualInfo(disp, scr, 8, StaticGray, &vinfo))
		q->visual = vinfo.visual;
	else {
	    fprintf(stderr, "X can't get depth %s\n",
		q->depth_req==24 ? "24" : q->depth_req==8 ? "8" : "24 or 8");
	    exit(1);
	}

    q->depth = vinfo.depth;
    printf("got visual with %d bit %s\n",
	q->depth, visual_class_name(q->visual->class));
    if (q->depth==24)
	/* we'll assume that byte order is either 0BGR or 0RGB */
	if (vinfo.red_mask==0x0000ff) q->bgr = 1;
	else if (vinfo.red_mask==0xff0000) q->bgr = 0;
	else fprintf(stderr,
	    "warning: this display has RGB masks of (%06x,%06x,%06x)\n",
	    vinfo.red_mask, vinfo.green_mask, vinfo.blue_mask);

    /* X won't allow window to have parent's colormap if visuals differ */
    if (q->visual->class != DefaultVisual(disp,scr)->class ||
	q->depth != DefaultDepth(disp, scr))
	    q->newcm = 1;
}

/* NEW VERSION!!
 * xpic_color_init: initialize q->depth, q->visual, and q->colormap
 * according to capabilities of X window system, and desires of caller:
 *	q->gray=1 requests grayscale
 *	q->ncol_req specifies a maximum number of colors to use, if nonzero
 *	q->newcm specifies whether a new colormap should be created,
 *	    or default colormap should be modified.
 *	q->depth_req=24 or 8 specifically requests 24 bits or 8 bits
 *	    q->depth_req=0 means use 24 if you can, else 8.
 */
void new_xpic_color_init(Quantization *q) {
    xpic_visual_init(q);	/* sets q->depth, q->visual */

    /* if TrueColor, then no need to do colormap quantization */
    if (q->depth==24) q->adaptcm=0;

/* changes made inside here for adaptcm */
    new_xpic_colormap_init(q);	/* sets q->colormap */
}

/* OLD VERSION!!!
 * xpic_color_init: initialize q->depth, q->visual, and q->colormap
 * according to capabilities of X window system, and desires of caller:
 *	q->gray=1 requests grayscale
 *	q->ncol_req specifies a maximum number of colors to use, if nonzero
 *	q->newcm specifies whether a new colormap should be created,
 *	    or default colormap should be modified.
 *	q->depth_req=24 or 8 specifically requests 24 bits or 8 bits
 *	    q->depth_req=0 means use 24 if you can, else 8.
 */
void xpic_color_init(Quantization *q) {
    xpic_visual_init(q);	/* sets q->depth, q->visual */
    xpic_colormap_init(q);	/* sets q->colormap */
}


/*
 * xpic_window_create: creates a window wx*wy pixels in size,
 * according to quantization parameters in q.  Doesn't map it.
 * xpic_color_init should be called before this is called.
 */
Window xpic_window_create(char *winname, Window parent,
			  int x, int y, int wx, int wy,
			  Quantization *q)
{
    XSetWindowAttributes attr;
    Window win;

    /*
     * note: if you try to create a window with a 24 bit visual when the
     * default visual is 8 bits (or more precisely, when the parent window
     * has a different visual or depth) and you don't explicitly set the
     * colormap and border, you will die with a BadMatch error.
     * You can thank the brain-damaged designers of X Windows for this.
     */
    attr.colormap = q->colormap;
    attr.border_pixel = 0;
    win = XCreateWindow(disp, parent, 
	/*origin*/ x, y, /*width,height*/ wx, wy,
	/*borderwidth*/ 0, /*depth*/ q->depth, /*class*/ InputOutput, q->visual,
	/*window attribute mask*/ CWColormap|CWBorderPixel, &attr);
    XStoreName(disp, win, winname);
    XSetIconName(disp, win, winname);

    XSetWMProtocols(disp, win, &WMDeleteWindow, 1);

    return win;
}

/*---------------------- Pic resampling & quantization ----------------------*/

/*
 * resample_and_quantize: Resample and quantize source picture s
 *   to create destination picture d,
 *   doing quantization according to parameters in q structure.
 * Source picture has size s->nx*s->ny and has pixels in s->pix.
 *   s is a 24 bit picture, and s->pix has s->nx*s->ny*3 bytes.
 * Destination picture has size d->nx*d->ny and we write pixels into d->pix.
 *   d->pix should have d->ny*d->nx bytes if q->depth==8,
 *   and d->ny*d->nx*4 bytes if  q->depth==24.
 *   It's 4, not 3, because pixels are 4 bytes (longs), not 3 bytes.
 *
 * This routine does quick & dirty resampling: box reconstruction.
 * Bilinear, with dither after resampling (not before) would look better.
 */

void resample_and_quantize(register Pic *s, register Pic *d,
			   register Quantization *q, Node *tree, Rgbcolor *cm)
{
  if (q->depth==24)
    resample_and_quantize24(s,d,q);
  else
    if (q->adaptcm)
      if (q->dither)
	quantpic_dith(s,d,q,tree,cm);
      else
	quantpic_nodith(s,d,q,tree,cm);
    else
      resample_and_quantize8(s,d,q);
}


/*
 * resample_and_quantize24: Resample and quantize source picture s
 *   to create destination picture d, with depth 24 bits,
 *   doing quantization according to parameters in q structure.
 * Source picture has size s->nx*s->ny and has pixels in s->pix.
 *   s is a 24 bit picture, and s->pix has s->nx*s->ny*3 bytes.
 * Destination picture has size d->nx*d->ny and we write pixels into d->pix.
 *   d->pix should have 
 *   d->ny*d->nx*4 bytes since q->depth==24.
 *   It's 4, not 3, because pixels are 4 bytes (longs), not 3 bytes.
 *
 * This routine does quick & dirty resampling: box reconstruction.
 * Bilinear, with dither after resampling (not before) would look better.
 */

void resample_and_quantize24(register Pic *s, register Pic *d,
			     register Quantization *q) {
  register Pixel1 *rp, *dp;
  register int x;
  int k, y;
  Pixel1 *row = 0;

fprintf(stderr,"inside resample_and_quantize\n");
  assert(q->depth==24);                /* 24 bit color */
  for (row=s->pix, dp=d->pix, y=0; y<s->ny; y++, row+=s->nx*3) {
    /*
     * scanline y of file is in row, now output the appropriate number
     * of output scanlines, resampling in x
     */
    for (k=(y+1)*d->ny/s->ny - y*d->ny/s->ny; --k>=0;)
      for (x=0; x<d->nx; x++, dp+=4) {
	rp = &row[x*s->nx/d->nx*3];
	dp[0] = 0;
	if (q->bgr) {	/* 0BGR (SGI's) */
	  dp[1] = rp[2];
	  dp[2] = rp[1];
	  dp[3] = rp[0];
	}
	else {		/* 0RGB (HP's) */
	  dp[1] = rp[0];
	  dp[2] = rp[1];
	  dp[3] = rp[2];
	}
      }
  }
}

/*
 * resample_and_quantize8: Resample and quantize source picture s
 *   to create destination picture d, with depth 8 bits
 *   doing quantization according to parameters in q structure.
 * Source picture has size s->nx*s->ny and has pixels in s->pix.
 *   s is a 24 bit picture, and s->pix has s->nx*s->ny*3 bytes.
 * Destination picture has size d->nx*d->ny and we write pixels into d->pix.
 *   d->pix should have d->ny*d->nx bytes since q->depth==8,
 *
 * This routine does quick & dirty resampling: box reconstruction.
 * Bilinear, with dither after resampling (not before) would look better.
 */
void resample_and_quantize8(register Pic *s, register Pic *d,
register Quantization *q) {
    register Pixel1 *sp, *rp, *dp;
    register int r, g, b, x, hr, hg, hb;
    int k, y;
    Pixel1 *row = 0;

    assert(q->depth==8);

    ALLOC(row, Pixel1, q->depth/8*s->nx);

    if (!q->gray && q->dither) {
	hr = 128/q->nr;
	hg = 128/q->ng;
	hb = 128/q->nb;
    }

    for (sp=s->pix, dp=d->pix, y=0; y<s->ny; y++) {
	if (q->gray)
	    if (q->ncol==256)		/* 8 bit grayscale */
		for (rp=row, x=s->nx; --x>=0; sp+=3)
		    *rp++ = (sp[0]*19661 + sp[1]*38666 + sp[2]*7209)>>16;
			/* luminance = .30*R + .59*G + .11*B */
	    else if (q->dither)		/* ncol-level grayscale, dithered */
		for (rp=row, x=s->nx; --x>=0; sp+=3) {
		    r = (sp[0]*4915 + sp[1]*9667 + sp[2]*1802)*q->ncol
			+ ((random()&511)-256<<13) >> 22;
		    if (r<0) r = 0; else if (r>=q->ncol) r = q->ncol-1;
		    *rp++ = q->pv[r];
		}
	    else			/* ncol-level grayscale, undithered */
		for (rp=row, x=s->nx; --x>=0; sp+=3)
		    *rp++ = q->pv[(sp[0]*4915 + sp[1]*9667 + sp[2]*1802)*
			q->ncol>>22];
	else if (q->ncol==256)
	    if (q->dither)		/* 8*8*4-level color, dithered */
		for (rp=row, x=s->nx; --x>=0; sp+=3) {
		    /* add random noise in the range [-16,16) to red & green,
		     * in the range [-32,32) to blue, before quantizing */
		    r = sp[0] + (random()&31) - 16;
		    if (r<0) r = 0; else if (r>255) r = 255;
		    g = sp[1] + (random()&31) - 16;
		    if (g<0) g = 0; else if (g>255) g = 255;
		    b = sp[2] + (random()&63) - 32;
		    if (b<0) b = 0; else if (b>255) b = 255;
		    *rp++ = r&0xe0 | (g&0xe0)>>3 | (b&0xc0)>>6;
		}
	    else			/* 8*8*4-level color, undithered */
		for (rp=row, x=s->nx; --x>=0; sp+=3)
		    *rp++ = sp[0]&0xe0 | (sp[1]&0xe0)>>3 | (sp[2]&0xc0)>>6;
	else
	    if (q->dither)		/* nr*ng*nb-level color, dithered */
		for (rp=row, x=s->nx; --x>=0; sp+=3) {
		    /* add random noise in the range [-hr,hr) to red,
		     * [-hg,hg) for green, [-hb,hb) for blue, before quant. */
		    r = sp[0] + (random()&255)/q->nr - hr;
		    if (r<0) r = 0; else if (r>255) r = 255;
		    g = sp[1] + (random()&255)/q->ng - hg;
		    if (g<0) g = 0; else if (g>255) g = 255;
		    b = sp[2] + (random()&255)/q->nb - hb;
		    if (b<0) b = 0; else if (b>255) b = 255;
		    *rp++ = q->pv[((r*q->nr>>8)*q->ng
			+ (g*q->ng>>8))*q->nb + (b*q->nb>>8)];
		}
	    else			/* nr*ng*nb-level color, undithered */
		for (rp=row, x=s->nx; --x>=0; sp+=3)
		    *rp++ = q->pv[((sp[0]*q->nr>>8)*q->ng
			+ (sp[1]*q->ng>>8))*q->nb + (sp[2]*q->nb>>8)];

	/*
	 * scanline y of file is in row, now output the appropriate number
	 * of output scanlines, resampling in x
	 */
	for (k=(y+1)*d->ny/s->ny - y*d->ny/s->ny; --k>=0;)
	    for (x=0; x<d->nx; x++)
		*dp++ = row[x*s->nx/d->nx];
    }
    free(row);
}

/* OLD VERSION!!
 * xpic_resample_and_quantize: Resample and quantize source picture s
 *   to create destination picture d,
 *   doing quantization according to parameters in q structure.
 * Source picture has size s->nx*s->ny and has pixels in s->pix.
 *   s is a 24 bit picture, and s->pix has s->nx*s->ny*3 bytes.
 * Destination picture has size d->nx*d->ny and we write pixels into d->pix.
 *   d->pix should have d->ny*d->nx bytes if q->depth==8,
 *   and d->ny*d->nx*4 bytes if q->depth==24.
 *   It's 4, not 3, because pixels are 4 bytes (longs), not 3 bytes.
 *
 * This routine does quick & dirty resampling: box reconstruction.
 * Bilinear, with dither after resampling (not before) would look better.
 */
void xpic_resample_and_quantize(register Pic *s, register Pic *d,
register Quantization *q) {
    register Pixel1 *sp, *rp, *dp;
    register int r, g, b, x, hr, hg, hb;
    int k, y;
    Pixel1 *row = 0;

    if (q->depth==24) {		/* 24 bit color */
	for (row=s->pix, dp=d->pix, y=0; y<s->ny; y++, row+=s->nx*3) {
	    /*
	     * scanline y of file is in row, now output the appropriate number
	     * of output scanlines, resampling in x
	     */
	    for (k=(y+1)*d->ny/s->ny - y*d->ny/s->ny; --k>=0;)
		for (x=0; x<d->nx; x++, dp+=4) {
		    rp = &row[x*s->nx/d->nx*3];
		    dp[0] = 0;
		    if (q->bgr) {	/* 0BGR (SGI's) */
			dp[1] = rp[2];
			dp[2] = rp[1];
			dp[3] = rp[0];
		    }
		    else {		/* 0RGB (HP's) */
			dp[1] = rp[0];
			dp[2] = rp[1];
			dp[3] = rp[2];
		    }
		}
	}
	return;
    }

    /* else handle 8-bit cases */

    ALLOC(row, Pixel1, q->depth/8*s->nx);

    if (!q->gray && q->dither) {
	hr = 128/q->nr;
	hg = 128/q->ng;
	hb = 128/q->nb;
    }

    for (sp=s->pix, dp=d->pix, y=0; y<s->ny; y++) {
	if (q->gray)
	    if (q->ncol==256)		/* 8 bit grayscale */
		for (rp=row, x=s->nx; --x>=0; sp+=3)
		    *rp++ = (sp[0]*19661 + sp[1]*38666 + sp[2]*7209)>>16;
			/* luminance = .30*R + .59*G + .11*B */
	    else if (q->dither)		/* ncol-level grayscale, dithered */
		for (rp=row, x=s->nx; --x>=0; sp+=3) {
		    r = (sp[0]*4915 + sp[1]*9667 + sp[2]*1802)*q->ncol
			+ ((random()&511)-256<<13) >> 22;
		    if (r<0) r = 0; else if (r>=q->ncol) r = q->ncol-1;
		    *rp++ = q->pv[r];
		}
	    else			/* ncol-level grayscale, undithered */
		for (rp=row, x=s->nx; --x>=0; sp+=3)
		    *rp++ = q->pv[(sp[0]*4915 + sp[1]*9667 + sp[2]*1802)*
			q->ncol>>22];
	else if (q->ncol==256)
	    if (q->dither)		/* 8*8*4-level color, dithered */
		for (rp=row, x=s->nx; --x>=0; sp+=3) {
		    /* add random noise in the range [-16,16) to red & green,
		     * in the range [-32,32) to blue, before quantizing */
		    r = sp[0] + (random()&31) - 16;
		    if (r<0) r = 0; else if (r>255) r = 255;
		    g = sp[1] + (random()&31) - 16;
		    if (g<0) g = 0; else if (g>255) g = 255;
		    b = sp[2] + (random()&63) - 32;
		    if (b<0) b = 0; else if (b>255) b = 255;
		    *rp++ = r&0xe0 | (g&0xe0)>>3 | (b&0xc0)>>6;
		}
	    else			/* 8*8*4-level color, undithered */
		for (rp=row, x=s->nx; --x>=0; sp+=3)
		    *rp++ = sp[0]&0xe0 | (sp[1]&0xe0)>>3 | (sp[2]&0xc0)>>6;
	else
	    if (q->dither)		/* nr*ng*nb-level color, dithered */
		for (rp=row, x=s->nx; --x>=0; sp+=3) {
		    /* add random noise in the range [-hr,hr) to red,
		     * [-hg,hg) for green, [-hb,hb) for blue, before quant. */
		    r = sp[0] + (random()&255)/q->nr - hr;
		    if (r<0) r = 0; else if (r>255) r = 255;
		    g = sp[1] + (random()&255)/q->ng - hg;
		    if (g<0) g = 0; else if (g>255) g = 255;
		    b = sp[2] + (random()&255)/q->nb - hb;
		    if (b<0) b = 0; else if (b>255) b = 255;
		    *rp++ = q->pv[((r*q->nr>>8)*q->ng
			+ (g*q->ng>>8))*q->nb + (b*q->nb>>8)];
		}
	    else			/* nr*ng*nb-level color, undithered */
		for (rp=row, x=s->nx; --x>=0; sp+=3)
		    *rp++ = q->pv[((sp[0]*q->nr>>8)*q->ng
			+ (sp[1]*q->ng>>8))*q->nb + (sp[2]*q->nb>>8)];

	/*
	 * scanline y of file is in row, now output the appropriate number
	 * of output scanlines, resampling in x
	 */
	for (k=(y+1)*d->ny/s->ny - y*d->ny/s->ny; --k>=0;)
	    for (x=0; x<d->nx; x++)
		*dp++ = row[x*s->nx/d->nx];
    }
    free(row);
}

/*------------------------- send a Pic to X -------------------------*/


static Pic *dpic = 0;	/* allocated by xpic_send, which tries to re-use */
static int dsize;	/* size of dpic->pix */

void xpic_send_free() {	/* call this after last call to xpic_send */
    pic_free(dpic);
    dpic = 0;
}

/* NEW VERSION!!
 * xpic_send: Send a picture in s to a Drawable (Window or Pixmap),
 * resampled to size wx*wy and quantized according to the
 * quantization parameters in q.
 * xpic_color_init should be called before this is called.
 */
void new_xpic_send(Pic *s, Drawable drawable, GC gc,
int wx, int wy, Quantization *q, Node *tree, Rgbcolor *cm) {
    static XImage *xim;

    /* allocate memory for resampled pic, if we don't have enough already */
    if (dpic && wy*wx*(q->depth==24 ? 4 : 1) > dsize)
	xpic_send_free();
    if (!dpic) {
        dpic = pic_alloc(wx, wy, q->depth==24 ? 4 : 1, NULL);
	dsize = wy*wx*(q->depth==24 ? 4 : 1);
    }

    /* resample and quantize picture s to desired size & depth */
    resample_and_quantize(s, dpic, q, tree, cm);

    /* create some dopey X data structures */
    xim = XCreateImage(disp, q->visual, q->depth,
	/*format*/ ZPixmap, /*offset*/ 0, /*pixel array*/ dpic->pix,
	/*width,height*/ wx, wy, /*alignment*/ 8, /*inter-line skip*/ 0);
    assert(xim);

    /* don't use server defaults for these, since we know what we have
     * (these lines needed for Linux) */
    if (q->depth==24)
    {
      xim->byte_order = MSBFirst;
      xim->bitmap_unit = xim->bitmap_pad = xim->bits_per_pixel = 32;
      xim->bytes_per_line = wx*4;
    }

    /* send image in pixel array dpic->pix to drawable in X server */
    XPutImage(disp, drawable, gc, xim, /*src xy*/ 0, 0, /*dst xy*/ 0, 0,
	/*width,height*/ wx, wy);
    XFree((char *)xim);
}

/* OLD VERSION!!
 * xpic_send: Send a picture in s to a Drawable (Window or Pixmap),
 * resampled to size wx*wy and quantized according to the
 * quantization parameters in q.
 * xpic_color_init should be called before this is called.
 */
void xpic_send(Pic *s, Drawable drawable, GC gc,
int wx, int wy, Quantization *q) {
    static XImage *xim;

    /* allocate memory for resampled pic, if we don't have enough already */
    if (dpic && wy*wx*(q->depth==24 ? 4 : 1) > dsize)
	xpic_send_free();
    if (!dpic) {
	dpic = pic_alloc(wx, wy, q->depth==24 ? 4 : 1, 0);
	dsize = wy*wx*(q->depth==24 ? 4 : 1);
    }

    /* resample and quantize picture s to desired size & depth */
    xpic_resample_and_quantize(s, dpic, q);

    /* create some dopey X data structures */
    xim = XCreateImage(disp, q->visual, q->depth,
	/*format*/ ZPixmap, /*offset*/ 0, /*pixel array*/ dpic->pix,
	/*width,height*/ wx, wy, /*alignment*/ 8, /*inter-line skip*/ 0);
    assert(xim);

    /* don't use server defaults for these, since we know what we have
     * (these lines needed for Linux) */
    if (q->depth==24)
    {
      xim->byte_order = MSBFirst;
      xim->bitmap_unit = xim->bitmap_pad = xim->bits_per_pixel = 32;
      xim->bytes_per_line = wx*4;
    }

    /* send image in pixel array dpic->pix to drawable in X server */
    XPutImage(disp, drawable, gc, xim, /*src xy*/ 0, 0, /*dst xy*/ 0, 0,
	/*width,height*/ wx, wy);
    XFree((char *)xim);
}
