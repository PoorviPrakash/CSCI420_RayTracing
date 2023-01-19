#include "xpic.h"

#define PVN 8		/* number of bits per pixel */
#define PVMAX ((1<<PVN)-1)

#define PQXN 6
#define PQYN 7
#define PQZN 6

#define PQLEVMAX (1<<PQYN)
#define PQN (PQXN+PQYN+PQZN)
#define PQNUM (1<<PQN)
typedef int Pqcolor;	/* must have at least PQN bits */
/* histogram takes Pqcolor in, frequency out */
/* offset array takes index in, Pqcolor out */

#define PQXMASK ((1<<PQXN)-1)
#define PQYMASK ((1<<PQYN)-1)
#define PQZMASK ((1<<PQZN)-1)
#define PQZYX(x, y, z) (((z)<<PQYN | (y))<<PQXN | (x))
#define PQX(qc) (PQXMASK & (qc))
#define PQY(qc) (PQYMASK & (qc)>>PQXN)
#define PQZ(qc) (PQZMASK & (qc)>>PQXN+PQYN)
#define PQR(qc) (PQX(qc)<<(PVN-PQXN) | 1<<(PVN-PQXN-1))
#define PQG(qc) (PQY(qc)<<(PVN-PQYN) | 1<<(PVN-PQYN-1))
#define PQB(qc) (PQZ(qc)<<(PVN-PQZN) | 1<<(PVN-PQZN-1))

typedef struct Leaf {	/* TERMINAL NODE IN K-D TREE */
    struct Leaf *next;	/* next in leaf chain */
    int freq;		/* number of pixels in this box */
    Pqcolor *o1;	/* first offset in box */
    Pqcolor *o2;	/* first offset in next box */
    short pv;		/* pixel value for final quantization */
} Leaf;
typedef struct {	/* BUCKET */
    int num;		/* number of Pqcolors in this bucket */
    int freq;		/* number of pixels in this bucket */
} bucket;

static treeclosest();
static exhaustiveclosest();
extern int pqnlev[], pqshift[], pqmask[];
extern char *pqname[];
static splitbox();
int *histinit();
Node *mediancut();


