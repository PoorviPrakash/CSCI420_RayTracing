#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>

#include "pic.h"



/*
 * ppm: subroutines for reading PPM picture files
 *
 * Paul Heckbert	18 Jan 94
 *
 * Michael Garland      17 Jan 96 : Added ppm_write
 */




/* ppm_get_token: read token from PPM file in stream fp into "char tok[len]" */
char *ppm_get_token(FILE *fp, char *tok, int len) {
    char *t;
    int c;

    for (;;) {			/* skip over whitespace and comments */
	while (isspace(c = getc(fp)));
	if (c!='#') break;
	do c = getc(fp); while (c!='\n' && c!=EOF);	/* gobble comment */
	if (c==EOF) break;
    }

    t = tok;
    if (c!=EOF) {
	do {
	    *t++ = c;
	    c = getc(fp);
	} while (!isspace(c) && c!='#' && c!=EOF && t-tok<len-1);
	if (c=='#') ungetc(c, fp);	/* put '#' back for next time */
    }
    *t = 0;
    return tok;
}

/* ppm_get_size: get size in pixels of PPM picture file */
int ppm_get_size(char *file, int *nx, int *ny) {
    char tok[20];
    FILE *fp;

    if ((fp = fopen(file, "r")) == NULL) {
	fprintf(stderr, "can't read PPM file %s\n", file);
	return 0;
    }
    if (strcmp(ppm_get_token(fp, tok, sizeof tok), "P6")) {
	fprintf(stderr, "%s is not a valid binary PPM file, bad magic#\n",
	    file);
	fclose(fp);
	return 0;
    }
    if (sscanf(ppm_get_token(fp, tok, sizeof tok), "%d", nx) != 1 ||
	sscanf(ppm_get_token(fp, tok, sizeof tok), "%d", ny) != 1) {
	    fprintf(stderr, "%s is not a valid PPM file: bad size\n", file);
	    fclose(fp);
	    return 0;
    }
    fclose(fp);
    return 1;
}

/*
 * ppm_read: read a PPM file into memory.
 * If opic!=0, then picture is read into opic->pix (after checking that
 * size is sufficient), else a new Pic is allocated.
 * Returns Pic pointer on success, 0 on failure.
 */
Pic *ppm_read(char *file, Pic *opic) {
    FILE *fp;
    char tok[20];
    int pvmax;
    Pic *p;

    ALLOC(p, Pic, 1);
    fp = 0;

    /* open PPM file */
    if ((fp = fopen(file, "r")) == NULL) {
	fprintf(stderr, "can't read PPM file %s\n", file);
	goto badfile;
    }

    /* read PPM header */
    if (strcmp(ppm_get_token(fp, tok, sizeof tok), "P6")) {
	fprintf(stderr, "%s not a valid binary PPM file, bad magic#\n", file);
	goto badfile;
    }
    if (sscanf(ppm_get_token(fp, tok, sizeof tok), "%d", &p->nx) != 1 ||
	sscanf(ppm_get_token(fp, tok, sizeof tok), "%d", &p->ny) != 1 ||
	sscanf(ppm_get_token(fp, tok, sizeof tok), "%d", &pvmax) != 1) {
	    fprintf(stderr, "%s is not a valid PPM file: bad size\n", file);
	    goto badfile;
    }
    if (pvmax!=255) {
	fprintf(stderr, "%s does not have 8-bit components: pvmax=%d\n",
	    file, pvmax);
	goto badfile;
    }

    printf("reading PPM file %s: %dx%d pixels\n", file, p->nx, p->ny);
    if (opic && opic->nx*opic->ny>=p->nx*p->ny) p->pix = opic->pix;
	/* now opic and p have a common pix array (caution when freeing) */
    else ALLOC(p->pix, Pixel1, p->ny*p->nx*3);

    if (fread(p->pix, p->nx*3, p->ny, fp) != p->ny) {	/* read pixels */
	fprintf(stderr, "premature EOF on file %s\n", file);
	goto badfile;
    }
    fclose(fp);
    return p;

badfile:
    free(p);
    if (fp) fclose(fp);
    return 0;
}



int ppm_write(char *file, Pic *pic)
{
    FILE *ppm;
    char *data;
    int i, max;
    int written;
    int npixels = (pic->nx) * (pic->ny);

    /* Compute the actual maximum color component */
    max = 0;
    for(i=0; i<npixels; i++)
    {
	if( pic->pix[i] > max )
	    max = pic->pix[i];
    }

    /* Open the file for output */
    ppm = fopen(file, "w");
    if( !ppm )
	return FALSE;

    /* Always write a raw PPM file */
    fprintf(ppm, "P6 %d %d %d\n", pic->nx, pic->ny, max);
    
    data = pic->pix;
    while( npixels > 0 )
    {
	written = fwrite(data, sizeof(char), npixels, ppm);
	npixels -= written;
	data += written;
    }

    fclose(ppm);

    return TRUE;
}
