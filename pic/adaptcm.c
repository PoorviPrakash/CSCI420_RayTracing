#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
/* #include "xpic.h" */
#include "adaptcm.h"
#include "xpic.h"

static int nclosest = 0;
static int npixels;
double pqweight[] = {.8, 1., .6};
int pqnlev[] = {1<<PQXN, 1<<PQYN, 1<<PQZN};
int pqshift[] = {0, PQXN, PQXN+PQYN};
int pqmask[] = {PQXMASK, PQYMASK, PQZMASK};
char *pqname[] = {"RED", "GRE", "BLU"};


prequantize_pixel(c)
register Pixel1_rgb *c;
{
    register int x, y, z;

    x = c->r>>(PVN-PQXN);
    y = c->g>>(PVN-PQYN);
    z = c->b>>(PVN-PQZN);
/*
    if (x<0) x = 0; else if (x>PQXMASK) x = PQXMASK;
    if (y<0) y = 0; else if (y>PQYMASK) y = PQYMASK;
    if (z<0) z = 0; else if (z>PQZMASK) z = PQZMASK;
*/
    return PQZYX(x, y, z);
}

prequantize_color(c)
register Rgbcolor *c;
{
    register int x, y, z;

    x = c->r>>(PVN-PQXN);
    y = c->g>>(PVN-PQYN);
    z = c->b>>(PVN-PQZN);
    if (x<0) x = 0; else if (x>PQXMASK) x = PQXMASK;
    if (y<0) y = 0; else if (y>PQYMASK) y = PQYMASK;
    if (z<0) z = 0; else if (z>PQZMASK) z = PQZMASK;
    return PQZYX(x, y, z);
}

quantpic_nodith(s, d, q, root, cm)
Pic *s, *d;
Quantization *q;
register Rgbcolor *cm;
Node *root;
{
    register int qc, pv, *quanttab;
    register Pixel1_rgb *ap;
    register Pixel1 *bp, *dp;
    int nx, ny, x, y, k;
    Pixel1_rgb *aline;
    Pixel1 *bline = 0;

    nx = s->nx;  ny = s->ny;

    /* initialize quantization table */
    ALLOC(quanttab, int, PQNUM);
    for (qc=0; qc<PQNUM; qc++)
	quanttab[qc] = -1;

    printf("quantizing to %d colors without dither\n", q->ncol);
    aline =  (Pixel1_rgb *)s->pix;

    ALLOC(bline, Pixel1, nx);
    for (dp=d->pix, y=0; y<ny; y++) {
	if (y%10==0) printf("%d\r", y);
	for (ap=aline, bp=bline, x=0; x<nx; x++, ap++) {
	    qc = prequantize_pixel(ap);
	    pv = quanttab[qc];
	    if (pv<0)		/* fill quantization table dynamically */
		quanttab[qc] = pv =
		    treeclosest(root, qc);
	    *bp++ = q->pv[pv];
	}

	for (k=(y+1)*d->ny/s->ny - y*d->ny/s->ny; --k>=0;)	
	  for (x=0; x<d->nx; x++)
	    *dp++ = bline[x*s->nx/d->nx];

	aline += s->nx;
    }
    free(quanttab);
    free(bline);
}

quantpic_dith(s, d, quant, root, cm)
Pic *s, *d;
Quantization *quant;
register Rgbcolor *cm;
Node *root;
{
    register int qc, pv, *quanttab;
    register Pixel1_rgb *ap;
    register Pixel1 *bp, *dp;
    int nx, ny, x, y, v, k;
    Pixel1_rgb *aline;
    Pixel1 *bline = 0;
    Rgbcolor i, e, *buf, *p, *q, *P, *Q, *R;

    nx = s->nx;  ny = s->ny;

    /* initialize quantization table */
    ALLOC(quanttab, int, PQNUM);
    for (qc=0; qc<PQNUM; qc++)
	quanttab[qc] = -1;

    printf("quantizing to %d colors with dither\n", quant->ncol);
    aline = (Pixel1_rgb *)s->pix;

    ALLOC(bline,Pixel1,nx);
    /* allocate nx+1 pixels per row of buf to allow for rightward propagation*/
    ALLOC(buf, Rgbcolor, 2*(nx+1));
    bzero(buf, 2*(nx+1)*sizeof(Rgbcolor));
    P = &buf[0];
    Q = &buf[nx+1];

    for (dp=d->pix, y=0; y<ny; y++, R=P, P=Q, Q=R) {
	if (y%10==0) printf("%d\r", y);
	Q->r = Q->g = Q->b = 0;
	for (ap=aline, bp=bline, p=P, q=Q, x=0; x<nx; x++, p++, q++, ap++) {
	    /* ppixel("old=", ap); */
	    i.r = ap->r+p->r;	/* ideal color */
	    i.g = ap->g+p->g;
	    i.b = ap->b+p->b;
	    /* pcolor(" ideal=", &i); */
	    if (i.r<0) i.r = 0; else if (i.r>PVMAX) i.r = PVMAX;
	    if (i.g<0) i.g = 0; else if (i.g>PVMAX) i.g = PVMAX;
	    if (i.b<0) i.b = 0; else if (i.b>PVMAX) i.b = PVMAX;
	    /* pcolor(" clip=", &i); */
	    qc = prequantize_color(&i);
	    pv = quanttab[qc];	/* nearest to ideal */
	    if (pv<0)		/* fill quantization table dynamically */
		quanttab[qc] = pv =
		    treeclosest(root, qc);
	    e.r = i.r-cm[pv].r;
	    e.g = i.g-cm[pv].g;		/* error */
	    e.b = i.b-cm[pv].b;
	    /* pcolor(" quant=", &cm[pv]); */
	    /* pcolor(" err=", &e); */
	    /* printf("\n"); */
	    *bp++ = quant->pv[pv];
	    v = e.r*3>>3; p[1].r += v; q[0].r += v; q[1].r = e.r-(v<<1);
	    v = e.g*3>>3; p[1].g += v; q[0].g += v; q[1].g = e.g-(v<<1);
	    v = e.b*3>>3; p[1].b += v; q[0].b += v; q[1].b = e.b-(v<<1);
	}

	for (k=(y+1)*d->ny/s->ny - y*d->ny/s->ny; --k>=0;)	
	  for (x=0; x<d->nx; x++)
	    *dp++ = bline[x*s->nx/d->nx];
	
	aline += s->nx;
    }
    free(quanttab);
    free(bline);
    free(buf);
}

static treeclosest(root, qc)
Node *root;
int qc;
{
    int coo[3];
    register Node *np;

    nclosest++;
    coo[0] = PQX(qc);
    coo[1] = PQY(qc);
    coo[2] = PQZ(qc);
    for (np=root; np->pv<0;)
	np = coo[np->dim]<np->thresh ? np->left : np->right;
    return np->pv;
}

int *histinit()
{
    int *hist;

    ALLOC(hist, int, PQNUM);
    bzero(hist, PQNUM*sizeof(int));
    return hist;
}

histpic(hist, p)
int *hist;
Pic *p;
{
    int nx, ny, x, y, qc;
    Pixel1_rgb *line;

    nx = p->nx;  ny = p->ny;
    npixels += nx*ny;

    line = (Pixel1_rgb*) p->pix;
    for (y=0; y<ny; y++) {
	if (y%10==0) printf("%d\r", y);

	for (x=0; x<nx; x++) {
	    qc = prequantize_pixel(&line[x]);
	    hist[qc]++;
	}
	line += nx;
    }
}

static unpercep(x, y, z, r, g, b)
double x, y, z;
int *r, *g, *b;
{
    *r = x * (1<<PVN-PQXN);
    *g = y * (1<<PVN-PQYN);
    *b = z * (1<<PVN-PQZN);
}

Node *mediancut(hist, nwant, ngotp, cm)
register int *hist;
int nwant, *ngotp;
Rgbcolor *cm;
{
    register int qc;
    register Pqcolor *o;
    register Leaf *lp, *lprev;
    int nused, n, i, pv, max, r, g, b, sx, sy, sz;
    Pqcolor *off;
    Leaf *firstleaf, *prev;
    Node *root;

    if (nwant==0) nwant=256;
    assert(sizeof(Node)==sizeof(Leaf));
    for (i=0; i<3; i++) {
	pqweight[i] *= PQLEVMAX/pqnlev[i];
	pqweight[i] *= pqweight[i];	/* it is used to scale variance */
    }
    /* printf("pqweight: %g %g %g\n", pqweight[0], pqweight[1], pqweight[2]); */

    /* make packed histogram: list of offsets into hist */
    for (nused=0, qc=0; qc<PQNUM; qc++)
	if (hist[qc]>0)
	    nused++;
    printf("%d pixels, used %d colors out of %d possible\n",
	npixels, nused, PQNUM);
    ALLOC(off, Pqcolor, nused);
    for (i=0, qc=0; qc<PQNUM; qc++)
	if (hist[qc]>0)
	    off[i++] = qc;

    ALLOC(lp, Leaf, 1);
    lp->next = 0;
    lp->freq = npixels;
    lp->o1 = off;
    lp->o2 = off+nused;
    root = (Node *)lp;

    /* the following Leaf is a dummy; it makes splitbox's life easier */
    ALLOC(firstleaf, Leaf, 1);
    firstleaf->next = lp;

    for (n=1; n<nwant; n++) {		/* make nwant leaves */
	max = -1;
	for (lprev=firstleaf; lprev->next; lprev=lp) {
	    lp = lprev->next;
	    if (lp->freq>max && lp->o2-lp->o1>1) {
		max = lp->freq;
		prev = lprev;		/* find the biggest unsplit box */
	    }
	}
	if (max<0) {
	    printf("quit early, after %d colors\n", n);
	    break;
	}
	splitbox(hist, prev);
    }
    printf("memory in bytes: %d for hist, %d for off, %d for %d-node k-d tree\n",
	PQNUM*sizeof(int), nused*sizeof(Pqcolor), (2*n-1)*sizeof(Node), 2*n-1);

    /* go through leaves finding centroid color of each to build colormap */
    /* find centroid in perceptual space and then transform to rgb space */
    for (lp=firstleaf->next, pv=0; pv<n; pv++, lp=lp->next) {
	lp->pv = pv;
	sx = sy = sz = lp->freq/2;
	for (i=0, o=lp->o1; o<lp->o2; o++) {
	    sx += hist[*o]*PQX(*o);
	    sy += hist[*o]*PQY(*o);
	    sz += hist[*o]*PQZ(*o);
	    i += hist[*o];	/*??*/
	}
	assert(i==lp->freq);
	if (lp->freq==0) printf("disaster at pv %d, freq=0, o=%d-%d\n",
	    pv, lp->freq, lp->o2-lp->o1);
	unpercep((double)sx/lp->freq, (double)sy/lp->freq, (double)sz/lp->freq,
	    &r, &g, &b);
    	cm[pv].r = r;
	cm[pv].g = g;
	cm[pv].b = b;
    }
    assert(!lp);

    *ngotp = n;
    free(off);
    return root;
}

/* split prev->next
 * takes a pointer to the Leaf previous to the one being split
 * to facilitate updating the chain of leaves */

static splitbox(hist, prev)
int *hist;
Leaf *prev;
{
    register Pqcolor *o;
    register int k, shift, mask, freq;
    register Leaf *fp;
    register bucket *b;
    int count, t, dim, thresh, x, y, z;
    int s, sx, sy, sz, sxx, syy, szz;
    double varx, vary, varz;
    Pqcolor *omid;
    Leaf *lp, *rp;
    Node *np;
    bucket buck[PQLEVMAX];

    fp = prev->next;			/* father box */

    /* compute variances in x, y, and z */
    sx = sy = sz = sxx = syy = szz = 0;
    s = 0;
    for (o=fp->o1; o<fp->o2; o++) {
	x = PQX(*o);
	y = PQY(*o);
	z = PQZ(*o);
	freq = hist[*o];
	s += freq;	/*??*/
	sx += freq*x; sxx += freq*x*x;
	sy += freq*y; syy += freq*y*y;
	sz += freq*z; szz += freq*z*z;
    }
    assert(s==fp->freq);
    s = fp->freq;
    varx = (double)sx/s; varx = (double)sxx/s - varx*varx;
    vary = (double)sy/s; vary = (double)syy/s - vary*vary;
    varz = (double)sz/s; varz = (double)szz/s - varz*varz;

    /* find dominant dimension */
    varx *= pqweight[0];
    vary *= pqweight[1];
    varz *= pqweight[2];
    dim = varx>=vary  ?  varx>=varz ? 0 : 2  :  vary>=varz ? 1 : 2;
    shift = pqshift[dim];
    mask = pqmask[dim];

    /* initialize buckets */
    for (k=0; k<pqnlev[dim]; k++) {
	buck[k].num = 0;
	buck[k].freq = 0;
    }

    /* parcel colors into buckets according to the dominant dimension,
     * counting number of pixels per bucket */
    for (o=fp->o1; o<fp->o2; o++) {
	k = *o>>shift & mask;
	b = &buck[k];
	b->num++;
	b->freq += hist[*o];
    }

    /* find median along dominant dimension */
    for (count=fp->freq/2, omid=fp->o1, b=buck; count>0; b++) {
	omid += b->num;
	count -= b->freq;
    }
    assert(b>buck);
    if (b>buck && count+b[-1].freq<-count) {	/* back up one? */
	b--;
	omid -= b->num;
	count += b->freq;
    }
    thresh = b-buck;

    /* allocate left and right sons */
    ALLOC(lp, Leaf, 1);
    ALLOC(rp, Leaf, 1);
    rp->next = fp->next;
    lp->next = rp;
    prev->next = lp;			/* bypass fp */
    lp->freq = fp->freq/2-count;
    rp->freq = (fp->freq+1)/2+count;
    lp->o1 = fp->o1;
    lp->o2 = omid;
    rp->o1 = omid;
    rp->o2 = fp->o2;

    /* father Leaf fp becomes Node np */
    np = (Node *)fp;
    np->left = (Node *)lp;
    np->right = (Node *)rp;
    np->dim = dim;
    np->thresh = thresh;
    np->pv = -1;			/* mark as non-terminal */

    /* swap colors in offset table until they're properly segregated into L&R */
    for (count=0, o=lp->o1;; o++, omid++, count++) {
	for (; o<lp->o2 && (*o>>shift&mask)<thresh; o++);
	if (o>=lp->o2) break;
	for (; (*omid>>shift&mask)>=thresh; omid++);
	count++;	/*??*/
	t = *o;				/* swap *o and *omid */
	*o = *omid;
	*omid = t;
    }
    for (; omid<rp->o2 && (*omid>>shift&mask)>=thresh; omid++);
    assert(omid==rp->o2);

    for (count=0, o=lp->o1; o<lp->o2; o++) {
	count += hist[*o];
	if ((*o>>shift&mask)>=thresh) printf("err on left\n");
    }
    assert(count==lp->freq);
    for (count=0, o=rp->o1; o<rp->o2; o++) {
	count += hist[*o];
	if ((*o>>shift&mask)<thresh) printf("err on right\n");
    }
    assert(count==rp->freq);
}




