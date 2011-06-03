/*
*class++
*  Name:
*     PolyMap

*  Purpose:
*     Map coordinates using polynomial functions.

*  Constructor Function:
c     astPolyMap
f     AST_POLYMAP

*  Description:
*     A PolyMap is a form of Mapping which performs a general polynomial
*     transformation.  Each output coordinate is a polynomial function of
*     all the input coordinates. The coefficients are specified separately
*     for each output coordinate. The forward and inverse transformations
*     are defined independantly by separate sets of coefficients.

*  Inheritance:
*     The PolyMap class inherits from the Mapping class.

*  Attributes:
*     The PolyMap class does not define any new attributes beyond
*     those which are applicable to all Mappings.

*  Functions:
c     In addition to those functions applicable to all Objects, the
c     following functions may also be applied to all Mappings:
f     In addition to those routines applicable to all Objects, the
f     following routines may also be applied to all Mappings:
*
c     - astPolyTran: Fit a PolyMap inverse or forward transformation
f     - AST_POLYTRAN: Fit a PolyMap inverse or forward transformation

*  Copyright:
*     Copyright (C) 1997-2006 Council for the Central Laboratory of the
*     Research Councils
*     Copyright (C) 2011 Science & Technology Facilities Council.
*     All Rights Reserved.

*  Licence:
*     This program is free software; you can redistribute it and/or
*     modify it under the terms of the GNU General Public Licence as
*     published by the Free Software Foundation; either version 2 of
*     the Licence, or (at your option) any later version.
*
*     This program is distributed in the hope that it will be
*     useful,but WITHOUT ANY WARRANTY; without even the implied
*     warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
*     PURPOSE. See the GNU General Public Licence for more details.
*
*     You should have received a copy of the GNU General Public Licence
*     along with this program; if not, write to the Free Software
*     Foundation, Inc., 59 Temple Place,Suite 330, Boston, MA
*     02111-1307, USA

*  Authors:
*     DSB: D.S. Berry (Starlink)

*  History:
*     27-SEP-2003 (DSB):
*        Original version.
*     13-APR-2005 (DSB):
*        Changed the keys used by the Dump/astLoadPolyMap functions. They
*        used to exceed 8 characters and consequently caused problems for
*        FitsChans.
*     20-MAY-2005 (DSB):
*        Correct the indexing of keywords produced in the Dump function.
*     20-APR-2006 (DSB):
*        Guard against undefined transformations in Copy.
*     10-MAY-2006 (DSB):
*        Override astEqual.
*     4-JUL-2008 (DSB):
*        Fixed loop indexing problems in Equal function.
*     27-MAY-2011 (DSB):
*        Added public method astPolyTran.
*class--
*/

/* Module Macros. */
/* ============== */
/* Set the name of the class we are implementing. This indicates to
   the header files that define class interfaces that they should make
   "protected" symbols available. */
#define astCLASS PolyMap

/* Macros which return the maximum and minimum of two values. */
#define MAX(aa,bb) ((aa)>(bb)?(aa):(bb))
#define MIN(aa,bb) ((aa)<(bb)?(aa):(bb))

/* Macro to check for equality of floating point values. We cannot
compare bad values directory because of the danger of floating point
exceptions, so bad values are dealt with explicitly. */
#define EQUAL(aa,bb) (((aa)==AST__BAD)?(((bb)==AST__BAD)?1:0):(((bb)==AST__BAD)?0:(fabs((aa)-(bb))<=1.0E5*MAX((fabs(aa)+fabs(bb))*DBL_EPSILON,DBL_MIN))))

/* Include files. */
/* ============== */
/* Interface definitions. */
/* ---------------------- */

#include "globals.h"             /* Thread-safe global data access */
#include "error.h"               /* Error reporting facilities */
#include "memory.h"              /* Memory allocation facilities */
#include "object.h"              /* Base Object class */
#include "pointset.h"            /* Sets of points/coordinates */
#include "mapping.h"             /* Coordinate mappings (parent class) */
#include "cmpmap.h"              /* Compound mappings */
#include "polymap.h"             /* Interface definition for this class */
#include "unitmap.h"             /* Unit mappings */
#include "levmar.h"              /* Levenberg - Marquardt minimization */

/* Error code definitions. */
/* ----------------------- */
#include "ast_err.h"             /* AST error codes */

/* C header files. */
/* --------------- */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* Module Variables. */
/* ================= */

/* Address of this static variable is used as a unique identifier for
   member of this class. */
static int class_check;

/* Pointers to parent class methods which are extended by this class. */
static AstPointSet *(* parent_transform)( AstMapping *, AstPointSet *, int, AstPointSet *, int * );


#ifdef THREAD_SAFE
/* Define how to initialise thread-specific globals. */
#define GLOBAL_inits \
   globals->Class_Init = 0;

/* Create the function that initialises global data for this module. */
astMAKE_INITGLOBALS(PolyMap)

/* Define macros for accessing each item of thread specific global data. */
#define class_init astGLOBAL(PolyMap,Class_Init)
#define class_vtab astGLOBAL(PolyMap,Class_Vtab)


#include <pthread.h>


#else


/* Define the class virtual function table and its initialisation flag
   as static variables. */
static AstPolyMapVtab class_vtab;   /* Virtual function table */
static int class_init = 0;       /* Virtual function table initialised? */

#endif


/* Type Definitions */
/* ================ */

/* Structure used to pass data to the Levenberg - Marquardt non-linear
   minimization algorithm. */
typedef struct LevMarData {
   int order1;     /* Max power of X1, plus one. */
   int order2;     /* Max power of X2, plus one. */
   int nsamp;      /* No. of polynomial samples to fit */
   int init_jac;   /* Has the constant Jacobian been found yet? */
   double *xp1;    /* Pointer to powers of X1 (1st poly i/p) at all samples */
   double *xp2;    /* Pointer to powers of X2 (2nd poly i/p) at all samples */
   double *y[ 2 ]; /* Pointers to Y1 and Y2 values at all samples */
} LevMarData;



/* External Interface Function Prototypes. */
/* ======================================= */
/* The following functions have public prototypes only (i.e. no
   protected prototypes), so we must provide local prototypes for use
   within this module. */
AstPolyMap *astPolyMapId_( int, int, int, const double[], int, const double[], const char *, ... );


/* Prototypes for Private Member Functions. */
/* ======================================== */
static AstPointSet *Transform( AstMapping *, AstPointSet *, int, AstPointSet *, int * );
static AstPolyMap *PolyTran( AstPolyMap *, int, double, double *, double *, int * );
static double *FitPoly2D( int, int, double, int, int, double **, int *, int * );
static double **SamplePoly( AstPolyMap *, int, int, double **, double *, double *, int, int *, int * );
static int Equal( AstObject *, AstObject *, int * );
static int MapMerge( AstMapping *, int, int, int *, AstMapping ***, int **, int * );
static void Copy( const AstObject *, AstObject *, int * );
static void CreateInverse( AstPolyMap *, int, double, double *, double *, int * );
static void Delete( AstObject *obj, int * );
static void Dump( AstObject *, AstChannel *, int * );
static void FreeArrays( AstPolyMap *, int, int * );
static void LMFunc(  double *, double *, int, int, void * );
static void LMJacob( double *, double *, int, int, void * );
static void StoreArrays( AstPolyMap *, int, int, const double *, int * );
static int GetTranForward( AstMapping *, int * );
static int GetTranInverse( AstMapping *, int * );



/* Member functions. */
/* ================= */

static void CreateInverse( AstPolyMap *this, int forward, double acc,
                           double *lbnd, double *ubnd, int *status ){
/*
*  Name:
*     CreateInverse

*  Purpose:
*     Create a new inverse or forward transformation for a PolyMap.

*  Type:
*     Private function.

*  Synopsis:
*     void CreateInverse( AstPolyMap *this, int forward, double acc,
*                         double *lbnd, double *ubnd, int *status )

*  Description:
*     This function creates a new forward or inverse transformation for
*     the supplied PolyMap (replacing any existing transformation), by
*     sampling the other transformation and performing a least squares
*     polynomial fit to the sample positions and values.
*
*     The transformation to create is specified by the "forward" parameter.
*     In what follows "X" refers to the inputs of the PolyMap, and "Y" to
*     the outputs of the PolyMap. The forward transformation transforms
*     input values (X) into output values (Y), and the inverse transformation
*     transforms output values (Y) into input values (X). Within a PolyMap,
*     each transformation is represented by an independent set of
*     polynomials: Y=P_f(X) for the forward transformation and X=P_i(Y)
*     for the inverse transformation.
*
*     If "forward" is zero then a new inverse transformation is created by
*     first finding the output values (Y) using the forward transformation
*     (which must be available) at a regular grid of points (X) covering a
*     rectangular region of the PolyMap's input space. The coefficients of
*     the required inverse polynomial, X=P_i(Y), are chosen in order to
*     minimise the sum of the squared residuals between the sampled values
*     of X and P_i(Y).
*
*     If "forward" is non-zero then a new forward transformation is created
*     by first finding the input values (X) using the inverse transformation
*     (which must be available) at a regular grid of points (Y) covering a
*     rectangular region of the PolyMap's output space. The coefficients of
*     the required forward polynomial, Y=P_f(X), are chosen in order to
*     minimise the sum of the squared residuals between the sampled values
*     of Y and P_f(X).
*
*     This fitting process is performed repeatedly with increasing
*     polynomial orders (starting with quadratic) until the specified
*     accuracy is acheived.

*  Parameters:
*     this
*        The PolyMap.
*     forward
*        If non-zero, then the forward PolyMap transformation is
*        replaced. Otherwise the inverse transformation is replaced.
*     acc
*        The required accuracy, expressed as a geodesic distance within
*        the PolyMap's input space (if "forward" is zero) or output
*        space (if "forward" is non-zero).
*     lbnd
*        An array holding the lower bounds of a rectangular region within
*        the PolyMap's input space (if "forward" is zero) or output
*        space (if "forward" is non-zero). The new polynomial will be
*        evaluated over this rectangle.
*     ubnd
*        An array holding the upper bounds of a rectangular region within
*        the PolyMap's input space (if "forward" is zero) or output
*        space (if "forward" is non-zero). The new polynomial will be
*        evaluated over this rectangle.
*     status
*        Pointer to the inherited status variable.

*  Notes:
*     - An error is reported if the transformation that is not being
*     replaced is not defined.
*     - An error is reported if the PolyMap does not have equal numbers
*     of inputs and outputs.
*     - An error is reported if the PolyMap has more than 2 inputs or outputs.

*/

/* Local Variables: */
   double **table;
   double *cofs;
   int ndim;
   int ncof;
   int nsamp;
   int order;

/* Check inherited status */
   if( !astOK ) return;

/* Check the PolyMap can be used. */
   ndim = astGetNin( this );
   if( astGetNout( this ) != ndim ) {
      astError( AST__BADNI, "astCreateInverse(%s): Supplied %s has "
                "different number of inputs (%d) and outputs (%d).",
                status, astGetClass( this ), astGetClass( this ), ndim,
                astGetNout( this ) );

   } else if( ndim > 2 ) {
      astError( AST__BADNI, "astCreateInverse(%s): Supplied %s has "
                "too many inputs and outputs (%d) - must be 1 or 2.",
                status, astGetClass( this ), astGetClass( this ), ndim );
   }

   if( forward != astGetInvert( this ) ){
      if( ! this->ncoeff_i ) {
         astError( AST__NODEF, "astCreateInverse(%s): Supplied %s has "
                   "no inverse transformation.", status, astGetClass( this ),
                   astGetClass( this ) );
      }
   } else {
      if( ! this->ncoeff_f  ) {
         astError( AST__NODEF, "astCreateInverse(%s): Supplied %s has "
                   "no forward transformation.", status, astGetClass( this ),
                   astGetClass( this ) );
      }
   }

/* Initialise pointer to work space. */
   table = NULL;

/* Loop over increasing polynomial orders until the required accuracy is
   achieved, up to a maximum of order 20. The "order" value is one more
   than the maximum power in the polynomial (so a quadratic has "order" 3). */
   for( order = 3; order <= 20; order++ ) {

/* Sample the requested polynomial transformation at a grid of points. This
   grid covers the user-supplied region, using 2*order points on each
   axis. If the PolyMap is 1D, then it will be treated as a 2D polynomial
   in which the second output is a unit transformation. */
      table = SamplePoly( this, ndim, !forward, table, lbnd, ubnd, 2*order,
                          &nsamp, status );

/* Fit the polynomial. Always fit a linear polynomial ("order" 2) to any
   dummy second axis. If succesful, replace the PolyMap transformation
   and break out of the order loop. */
      cofs = FitPoly2D( ndim, nsamp, acc, order, order, table, &ncof,
                        status );
      if( cofs ) {
         StoreArrays( this, forward, ncof, cofs, status );
         break;
      }
   }

/* If no fit was produced, report an error. */
   if( !cofs && astOK ) {
      astError( AST__NOFIT, "astCreateInverse(%s): Failed to find a new "
                "%s transformation for the supplied %s: fit failed.",
                status, astGetClass( this ),
                (forward ? "forward" : "forward" ), astGetClass( this ) );
   }

/* Free resources. */
   cofs = astFree( cofs );
   table = astFreeDouble( table );
}

static int Equal( AstObject *this_object, AstObject *that_object, int *status ) {
/*
*  Name:
*     Equal

*  Purpose:
*     Test if two PolyMaps are equivalent.

*  Type:
*     Private function.

*  Synopsis:
*     #include "polymap.h"
*     int Equal( AstObject *this, AstObject *that, int *status )

*  Class Membership:
*     PolyMap member function (over-rides the astEqual protected
*     method inherited from the astMapping class).

*  Description:
*     This function returns a boolean result (0 or 1) to indicate whether
*     two PolyMaps are equivalent.

*  Parameters:
*     this
*        Pointer to the first Object (a PolyMap).
*     that
*        Pointer to the second Object.
*     status
*        Pointer to the inherited status variable.

*  Returned Value:
*     One if the PolyMaps are equivalent, zero otherwise.

*  Notes:
*     - A value of zero will be returned if this function is invoked
*     with the global status set, or if it should fail for any reason.
*/

/* Local Variables: */
   AstPolyMap *that;
   AstPolyMap *this;
   int i, j, k;
   int nin;
   int nout;
   int result;

/* Initialise. */
   result = 0;

/* Check the global error status. */
   if ( !astOK ) return result;

/* Obtain pointers to the two PolyMap structures. */
   this = (AstPolyMap *) this_object;
   that = (AstPolyMap *) that_object;

/* Check the second object is a PolyMap. We know the first is a
   PolyMap since we have arrived at this implementation of the virtual
   function. */
   if( astIsAPolyMap( that ) ) {

/* Get the number of inputs and outputs and check they are the same for both. */
      nin = astGetNin( this );
      nout = astGetNout( this );
      if( astGetNin( that ) == nin && astGetNout( that ) == nout ) {

/* If the Invert flags for the two PolyMaps differ, it may still be possible
   for them to be equivalent. First compare the PolyMaps if their Invert
   flags are the same. In this case all the attributes of the two PolyMaps
   must be identical. */
         if( astGetInvert( this ) == astGetInvert( that ) ) {

            result = 1;

            for( i = 0; i < nout && result; i++ ) {
               if( this->ncoeff_f[ i ] != that->ncoeff_f[ i ] ||
                   this->mxpow_i[ i ] != that->mxpow_i[ i ] ) {
                  result = 0;
               }
            }


            if( this->coeff_f && that->coeff_f ) {
               for( i = 0; i < nout && result; i++ ) {
                  for( j = 0; j < this->ncoeff_f[ i ] && result; j++ ) {
                     if( !EQUAL( this->coeff_f[ i ][ j ],
                                 that->coeff_f[ i ][ j ] ) ) {
                        result = 0;
                     }
                  }
               }
            }

            if( this->power_f && that->power_f ) {
               for( i = 0; i < nout && result; i++ ) {
                  for( j = 0; j < this->ncoeff_f[ i ] && result; j++ ) {
                     for( k = 0; k < nin && result; k++ ) {
                        if( !EQUAL( this->power_f[ i ][ j ][ k ],
                                    that->power_f[ i ][ j ][ k ] ) ) {
                           result = 0;
                        }
                     }
                  }
               }
            }

            for( i = 0; i < nin && result; i++ ) {
               if( this->ncoeff_i[ i ] != that->ncoeff_i[ i ] ||
                   this->mxpow_f[ i ] != that->mxpow_f[ i ] ) {
                  result = 0;
               }
            }


            if( this->coeff_i && that->coeff_i ) {
               for( i = 0; i < nin && result; i++ ) {
                  for( j = 0; j < this->ncoeff_i[ i ] && result; j++ ) {
                     if( !EQUAL( this->coeff_i[ i ][ j ],
                                 that->coeff_i[ i ][ j ] ) ) {
                        result = 0;
                     }
                  }
               }
            }

            if( this->power_i && that->power_i ) {
               for( i = 0; i < nin && result; i++ ) {
                  for( j = 0; j < this->ncoeff_i[ i ] && result; j++ ) {
                     for( k = 0; k < nout && result; k++ ) {
                        if( !EQUAL( this->power_i[ i ][ j ][ k ],
                                    that->power_i[ i ][ j ][ k ] ) ) {
                           result = 0;
                        }
                     }
                  }
               }
            }

/* If the Invert flags for the two PolyMaps differ, the attributes of the two
   PolyMaps must be inversely related to each other. */
         } else {

/* In the specific case of a PolyMap, Invert flags must be equal. */
            result = 0;

         }
      }
   }

/* If an error occurred, clear the result value. */
   if ( !astOK ) result = 0;

/* Return the result, */
   return result;
}

static double *FitPoly2D( int ndim, int nsamp, double acc, int order1,
                          int order2, double **table, int *ncoeff,
                          int *status ){
/*
*  Name:
*     FitPoly2D

*  Purpose:
*     Fit a (2-in,2-out) polynomial to a supplied set of data.

*  Type:
*     Private function.

*  Synopsis:
*     double *FitPoly2D( int ndim, int nsamp, double acc, int order1,
*                        int order2, double **table, int *ncoeff, int *status )

*  Description:
*     This function fits a pair of least squares 2D polynomial surfaces
*     to the positions in a supplied table. For the purposes of this
*     function, the polynomial inputs are refered to as (x1,x2) and the
*     outputs as (y1,y2). So the two polynomials are:
*
*     y1 = P1( x1, x2 )
*     y2 = P2( x1, x2 )
*
*     P1 and P2 have the same maximum powers on each input (specified by
*     the "order" parameters).

*  Parameters:
*     ndim
*        The number of inputs and outputs for Poly - 1 or 2.
*     nsamp
*        The number of (x1,x2,y1,y2) positions in the supplied table.
*     acc
*        The required accuracy, expressed as a geodesic distance within
*        the polynomials output space.
*     order1
*        The maximum power (minus one) of x1 within P1 and P2.
*     order2
*        The maximum power (minus one) of x2 within P1 and P2. Ignored if
*        ndim is 1 (a value of 2 is used).
*     table
*        Pointer to an array of 4 pointers. Each of these pointers points
*        to an array of "nsamp" doubles, being the sampled values for x1,
*        x2, y1 or y2 in that order.
*     ncoeff
*        Pointer to an ant in which to return the number of coefficients
*        described by the returned array. This will be zero if the
*        polynomial could be found with sufficient accuracy.
*     status
*        Pointer to the inherited status variable.

*  Returned Value:
*     A pointer to an array of doubles defining the polynomial in the
*     form required by the PolyMap contructor. The number of coefficients
*     is returned via "ncoeff". If the polynomial could not be found with
*     sufficient accuracy , then NULL is returned. The returned pointer
*     should be freed using astFree when no longer needed.

*/

/* Local Variables: */
   LevMarData data;
   double *coeffs;
   double *pc;
   double *pr;
   double *px1;
   double *px2;
   double *pxp1;
   double *pxp2;
   double *result;
   double info[10];
   double maxterm;
   double term;
   double tv;
   double x1;
   double x2;
   int iout;
   int k;
   int ncof;
   int niter;
   int w1;
   int w2;

/* Termination criteria for the minimisation - see levmar.c */
   double opts[] = { 1E-03, 1E-17, 1E-10, 1E-17 };

/* Initialise returned value */
   result = NULL;
   *ncoeff = 0;

/* Check inherited status */
   if( !astOK ) return result;

/* Set order 2 if poly is 1-D */
   if( ndim == 1 ) order2 = 2;

/* Number of coefficients per poly. */
   ncof = order1*order2;

/* Initialise the elements of the structure. */
   data.order1 = order1;
   data.order2 = order2;
   data.nsamp = nsamp;
   data.init_jac = 1;
   data.xp1 = astMalloc( nsamp*order1*sizeof( double ) );
   data.xp2 = astMalloc( nsamp*order2*sizeof( double ) );
   data.y[ 0 ] = table[ 2 ];
   data.y[ 1 ] = table[ 3 ];

/* Work space to hold coefficients. */
   coeffs = astMalloc( 2*ncof*sizeof( double ) );
   if( astOK ) {

/* Store required squared acccuracy. */
      opts[ 3 ] = acc*acc;

/* Get pointers to the supplied x1 and x2 values. */
      px1 = table[ 0 ];
      px2 = table[ 1 ];

/* Get pointers to the location for the next power of x1 and x2. */
      pxp1 = data.xp1;
      pxp2 = data.xp2;

/* Loop round all samples. */
      for( k = 0; k < nsamp; k++ ) {

/* Get the current x1 and x2 values. */
         x1 = *(px1++);
         x2 = *(px2++);

/* Find all the required powers of x1 and store them in the "xp1"
   comonent of the data structure. */
         tv = 1.0;
         for( w1 = 0; w1 < order1; w1++ ) {
            *(pxp1++) = tv;
            tv *= x1;
         }

/* Find all the required powers of x2 and store them in the "xp2"
   comonent of the data structure. */
         tv = 1.0;
         for( w2 = 0; w2 < order2; w2++ ) {
            *(pxp2++) = tv;
            tv *= x2;
         }
      }

/* The initial guess at the coefficient values represents a unit
   transformation. */
      for( k = 0; k < 2*ncof; k++ ) coeffs[ k ] = 0.0;
      coeffs[ order2 ] = 1.0;
      coeffs[ 1 + ncof ] = 1.0;

/* Find the best coefficients */
      niter = dlevmar_der( LMFunc, LMJacob, coeffs, NULL, 2*ncof, 2*nsamp,
                           10000, opts, info, NULL, NULL, &data );

/* If OK, purge insignificant coefficients */
      if( info[6] != 4 && info[6] != 7 ) {

/* Look at coefficients for each output in turn. */
         for( iout = 0; iout < ndim && astOK; iout++ ) {

/* Pointer to the first coefficient. */
            pc = coeffs + ncof*iout;

/* Look at each coefficient for the current output. */
            for( w1 = 0; w1 < order1; w1++ ) {
               for( w2 = 0; w2 < order2; w2++,pc++ ) {

/* Get pointers to the powers of X1 and X2 appropriate for the current
   coefficient, at the first sample. */
                  pxp1 = data.xp1 + w1;
                  pxp2 = data.xp2 + w2;

/* We find the contribution which this coefficient makes to the total
   polynomial value. Find the maximum contribution made at any sample
   points. */
                  maxterm = 0.0;
                  for( k = 0; k < nsamp; k++ ) {

/* Get the absolute value of the polynomial term that uses the current
   coefficient. */
                     term = fabs( ( *pc )*( *pxp1 )*( *pxp2 ) );

/* Update the maximum term found at any sample. */
                     if( term > maxterm ) maxterm = term;

/* Increment the pointers to refer to the next sample. */
                     pxp1 += order1;
                     pxp2 += order2;
                  }

/* If the maximum contribution made by this term is less than the
   required accuracy, set the coefficient value to zero. */
                  if( maxterm < acc ) *pc = 0.0;
               }
            }
         }

/* Convert the array of coefficients into PolyMap form. */
         result = astMalloc( 2*ncof*(2+ndim)*sizeof( double ) );

         *ncoeff = 0;
         pr = result;
         pc = coeffs;
         for( iout = 0; iout < ndim && astOK; iout++ ) {
            for( w1 = 0; w1 < order1; w1++ ) {
               for( w2 = 0; w2 < order2; w2++,pc++ ) {
                  if( *pc != 0.0 ) {
                     *(pr++) = *pc;
                     *(pr++) = iout + 1;
                     *(pr++) = w1;
                     if( ndim > 1 ) *(pr++) = w2;
                     (*ncoeff)++;
                  }
               }
            }
         }

/* Truncate the returned array. */
         result = astRealloc( result, (*ncoeff)*( 2 + ndim )*sizeof( double ) );
      }
   }

/* Free resources. */
   coeffs = astFree( coeffs );
   data.xp1 = astFree( data.xp1 );
   data.xp2 = astFree( data.xp2 );

/* Return the coefficient array. */
   return result;

}

static void FreeArrays( AstPolyMap *this, int forward, int *status ) {
/*
*  Name:
*     FreeArrays

*  Purpose:
*     Free the dynamic arrays contained within a PolyMap

*  Type:
*     Private function.

*  Synopsis:
*     void FreeArrays( AstPolyMap *this, int forward, int *status )

*  Description:
*     This function frees all the dynamic arrays allocated as part of a
*     PolyMap.

*  Parameters:
*     this
*        Pointer to the PolyMap.
*     forward
*        If non-zero, the arrays for the forward transformation are freed.
*        Otherwise, the arrays for the inverse transformation are freed.
*     status
*        Pointer to the inherited status variable.

*  Returned Value:
*     void

*  Notes:
*     This function attempts to execute even if the global error status is
*     set.
*/

/* Local Variables: */
   int nin;                     /* Number of inputs */
   int nout;                    /* Number of outputs */
   int i;                       /* Loop count */
   int j;                       /* Loop count */

/* Get the number of inputs and outputs of the uninverted Mapping. */
   nin = ( (AstMapping *) this )->nin;
   nout = ( (AstMapping *) this )->nout;

/* Free the dynamic arrays for the forward transformation. */
   if( forward ) {

      if( this->coeff_f ) {
         for( i = 0; i < nout; i++ ) {
            this->coeff_f[ i ] = astFree( this->coeff_f[ i ] );
         }
         this->coeff_f = astFree( this->coeff_f );
      }

      if( this->power_f ) {
         for( i = 0; i < nout; i++ ) {
            if( this->ncoeff_f && this->power_f[ i ] ) {
               for( j = 0; j < this->ncoeff_f[ i ]; j++ ) {
                  this->power_f[ i ][ j ] = astFree( this->power_f[ i ][ j ] );
               }
            }
            this->power_f[ i ] = astFree( this->power_f[ i ] );
         }
         this->power_f = astFree( this->power_f );
      }

      this->ncoeff_f = astFree( this->ncoeff_f );
      this->mxpow_f = astFree( this->mxpow_f );

/* Free the dynamic arrays for the inverse transformation. */
   } else {

      if( this->coeff_i ) {
         for( i = 0; i < nin; i++ ) {
            this->coeff_i[ i ] = astFree( this->coeff_i[ i ] );
         }
         this->coeff_i = astFree( this->coeff_i );
      }

      if(this->power_i ) {
         for( i = 0; i < nin; i++ ) {
            if( this->ncoeff_i && this->power_i[ i ] ) {
               for( j = 0; j < this->ncoeff_i[ i ]; j++ ) {
                  this->power_i[ i ][ j ] = astFree( this->power_i[ i ][ j ] );
               }
            }
            this->power_i[ i ] = astFree( this->power_i[ i ] );
         }
         this->power_i = astFree( this->power_i );
      }

      this->ncoeff_i = astFree( this->ncoeff_i );
      this->mxpow_i = astFree( this->mxpow_i );
   }
}

static int GetTranForward( AstMapping *this, int *status ) {
/*
*
*  Name:
*     GetTranForward

*  Purpose:
*     Determine if a PolyMap defines a forward coordinate transformation.

*  Type:
*     Private function.

*  Synopsis:
*     #include "mapping.h"
*     int GetTranForward( AstMapping *this, int *status )

*  Class Membership:
*     PolyMap member function (over-rides the astGetTranForward method
*     inherited from the Mapping class).

*  Description:
*     This function returns a value indicating if the PolyMap is able
*     to perform a forward coordinate transformation.

*  Parameters:
*     this
*        Pointer to the PolyMap.
*     status
*        Pointer to the inherited status variable.

*  Returned Value:
*     Zero if the forward coordinate transformation is not defined, or 1 if it
*     is.

*  Notes:
*     -  A value of zero will be returned if this function is invoked with the
*     global error status set, or if it should fail for any reason.
*/

/* Local Variables: */
   AstPolyMap *map;            /* Pointer to PolyMap to be queried */

/* Check the global error status. */
   if ( !astOK ) return 0;

/* Obtain a pointer to the PolyMap. */
   map = (AstPolyMap *) this;

/* Return the result. */
   return map->ncoeff_f ? 1 : 0;
}

static int GetTranInverse( AstMapping *this, int *status ) {
/*
*
*  Name:
*     GetTranInverse

*  Purpose:
*     Determine if a PolyMap defines an inverse coordinate transformation.

*  Type:
*     Private function.

*  Synopsis:
*     #include "matrixmap.h"
*     int GetTranInverse( AstMapping *this, int *status )

*  Class Membership:
*     PolyMap member function (over-rides the astGetTranInverse method
*     inherited from the Mapping class).

*  Description:
*     This function returns a value indicating if the PolyMap is able
*     to perform an inverse coordinate transformation.

*  Parameters:
*     this
*        Pointer to the PolyMap.
*     status
*        Pointer to the inherited status variable.

*  Returned Value:
*     Zero if the inverse coordinate transformation is not defined, or 1 if it
*     is.

*  Notes:
*     -  A value of zero will be returned if this function is invoked with the
*     global error status set, or if it should fail for any reason.
*/

/* Local Variables: */
   AstPolyMap *map;            /* Pointer to PolyMap to be queried */

/* Check the global error status. */
   if ( !astOK ) return 0;

/* Obtain a pointer to the PolyMap. */
   map = (AstPolyMap *) this;

/* Return the result. */
   return map->ncoeff_i ? 1 : 0;
}

void astInitPolyMapVtab_(  AstPolyMapVtab *vtab, const char *name, int *status ) {
/*
*+
*  Name:
*     astInitPolyMapVtab

*  Purpose:
*     Initialise a virtual function table for a PolyMap.

*  Type:
*     Protected function.

*  Synopsis:
*     #include "polymap.h"
*     void astInitPolyMapVtab( AstPolyMapVtab *vtab, const char *name )

*  Class Membership:
*     PolyMap vtab initialiser.

*  Description:
*     This function initialises the component of a virtual function
*     table which is used by the PolyMap class.

*  Parameters:
*     vtab
*        Pointer to the virtual function table. The components used by
*        all ancestral classes will be initialised if they have not already
*        been initialised.
*     name
*        Pointer to a constant null-terminated character string which contains
*        the name of the class to which the virtual function table belongs (it
*        is this pointer value that will subsequently be returned by the Object
*        astClass function).
*-
*/

/* Local Variables: */
   astDECLARE_GLOBALS            /* Pointer to thread-specific global data */
   AstObjectVtab *object;        /* Pointer to Object component of Vtab */
   AstMappingVtab *mapping;      /* Pointer to Mapping component of Vtab */

/* Check the local error status. */
   if ( !astOK ) return;


/* Get a pointer to the thread specific global data structure. */
   astGET_GLOBALS(NULL);

/* Initialize the component of the virtual function table used by the
   parent class. */
   astInitMappingVtab( (AstMappingVtab *) vtab, name );

/* Store a unique "magic" value in the virtual function table. This
   will be used (by astIsAPolyMap) to determine if an object belongs
   to this class.  We can conveniently use the address of the (static)
   class_check variable to generate this unique value. */
   vtab->id.check = &class_check;
   vtab->id.parent = &(((AstMappingVtab *) vtab)->id);

/* Initialise member function pointers. */
/* ------------------------------------ */
/* Store pointers to the member functions (implemented here) that provide
   virtual methods for this class. */
   vtab->PolyTran = PolyTran;

/* Save the inherited pointers to methods that will be extended, and
   replace them with pointers to the new member functions. */
   object = (AstObjectVtab *) vtab;
   mapping = (AstMappingVtab *) vtab;

   parent_transform = mapping->Transform;
   mapping->Transform = Transform;
   mapping->GetTranForward = GetTranForward;
   mapping->GetTranInverse = GetTranInverse;

/* Store replacement pointers for methods which will be over-ridden by
   new member functions implemented here. */
   object->Equal = Equal;
   mapping->MapMerge = MapMerge;

/* Declare the destructor and copy constructor. */
   astSetDelete( (AstObjectVtab *) vtab, Delete );
   astSetCopy( (AstObjectVtab *) vtab, Copy );

/* Declare the class dump function. */
   astSetDump( vtab, Dump, "PolyMap", "Polynomial transformation" );

/* If we have just initialised the vtab for the current class, indicate
   that the vtab is now initialised, and store a pointer to the class
   identifier in the base "object" level of the vtab. */
   if( vtab == &class_vtab ) {
      class_init = 1;
      astSetVtabClassIdentifier( vtab, &(vtab->id) );
   }
}

static void LMFunc(  double *p, double *hx, int m, int n, void *adata ){
/*
*  Name:
*     LMFunc

*  Purpose:
*     Evaluate a test polynomal.

*  Type:
*     Private function.

*  Synopsis:
*     void LMFunc(  double *p, double *hx, int m, int n, void *adata )

*  Description:
*     This function finds the residuals implied by a supplied set of
*     candidate polynomial coefficients. Each residual is a candidate
*     polynomial (either P1 or P2) evaluated at one of the sample points
*     (x1,x2), minus the supplied target value for the polynomial at that
*     test point.
*
*     The minimisation process minimises the sum of the squared residuals.

*  Parameters:
*     p
*        An array of "m" candidate polynomial coefficients. The
*        coefficient of (x1^j*x2^k) for polynomial Pi is stored in
*        element ( k + j*order[1] + i*order[0]*order[1]).
*     hx
*        An array in which to return the "n" residuals. The residual at
*        sample "k" for polynomial "i" is returned in element (k + nsamp*i).
*     m
*        The length of the "p" array. This should be equal to 2*order1*order2.
*     n
*        The length of the "hx" array. This should be equal to 2*nsamp.
*     adata
*        Pointer to a structure holdin gthe sample positions and values,
*        and other information.

*/

/* Local Variables: */
   LevMarData *data;
   double *px1;
   double *px20;
   double *px2;
   double *py;
   double *vp0;
   double *vp;
   double *vr;
   double res;
   int iout;
   int k;
   int w1;
   int w2;

/* Get a pointer to the data structure. */
   data = (LevMarData *) adata;

/* Initialise a pointer to the current returned residual value. */
   vr = hx;

/* Initilise a pointer to the first coefficient (the  constant term) for the
   current polynomial output  coordinate. */
   vp0 = p;

/* Loop over each polynomial output coordinate. */
   for( iout = 0; iout < 2; iout++ ) {

/* Initialise a pointer to the sampled Y values for the first polynomial
   output. */
      py = data->y[ iout ];

/* Get a pointer to the value holding the second polynomial input value,
   raised to the power zero, at the first sample. */
      px20 = data->xp2;

/* Get a pointer to the value holding the first polynomial input value,
   raised to the power zero, at the first sample. */
      px1 = data->xp1;

/* Loop over the index of the sample to which this residual refers. */
      for( k = 0; k < data->nsamp; k++ ) {

/* Reset the pointer to the first coefficient (the  constant term) for the
   current polynomial output  coordinate. */
         vp = vp0;

/* Initialise this residual to hold the sampled Y value. Increment the
   pointer to the next sampled value for the current polynomial output. */
         res = -( *(py++) );

/* Loop round every power of X1 - the first polynomial input coordinate. */
         for( w1 = 0; w1 < data->order1; w1++ ){

/* Reset the pointer to the value holding the second polynomial input
   value, raised to the power zero. */
            px2 = px20;

/* Loop round every power of X2 - the second polynomial input coordinate.*/
            for( w2 = 0; w2 < data->order2; w2++ ){

/* Increment the current residual by the current term of the polynomial.
   Also update the pointer to the next coefficient, and the pointer to the
   next power of X2. */
               res += ( *(vp++) )*( *px1 )*( *(px2++) );
            }

/* Increment the pointer to the value of the first polynomial input value,
   raised to the next power "w1". */
            px1++;
         }

/* Store the complete residual in the returned array, and increment the
   pointer to the next residual. */
         *(vr++) = res;
/* Increment the pointer to the value holding the second polynomial input
   value, raised to the power zero, so that it refers to the next sampled
   value. */
         px20 += data->order2;
      }

/* Get a pointer to the first coefficient (the  constant term) for the
   next polynomial output  coordinate. */
      vp0 += data->order1*data->order2;
   }
}

static void LMJacob( double *p, double *jac, int m, int n, void *adata ){
/*
*  Name:
*     LMJacob

*  Purpose:
*     Evaluate the Jacobian matrix of a test polynomal.

*  Type:
*     Private function.

*  Synopsis:
*     void LMJacob( double *p, double *jac, int m, int n, void *adata )

*  Description:
*     This function finds the Jacobian matrix that describes the rate of
*     change of every residual with respect to every polynomial coefficient.
*     Each residual is a candidate polynomial (either P1 or P2) evaluated
*     at one of the sample points (x1,x2), minus the supplied target value
*     for the polynomial at that test point.
*
*     For a polynomial the Jacobian matrix is constant (i.e. does not
*     depend on the values of the polynomial coefficients). So we only
*     evaluate it on the first call.

*  Parameters:
*     p
*        An array of "m" candidate polynomial coefficients. The
*        coefficient of (x1^j*x2^k) for polynomial Pi is stored in
*        element ( k + j*order[1] + i*order[0]*order[1]).
*     jac
*        An array in which to return the "m*n" elements of the Jacobian
*        matrix. The rate of change of residual "r" with respect to
*        coefficient "c" is returned in element "r + c*n". The residual
*        at sample "k" of polynomial Pi has an "r" index of (k + nsamp*i).
*        The coefficient of (x1^j*x2^k) for polynomial Pi has a "c" index
*        of ( k + j*order[1] + i*order[0]*order[1]).
*     m
*        The number of coefficients. This should be equal to 2*order1*order2.
*     n
*        The number of residuals. This should be equal to 2*nsamp.
*     adata
*        Pointer to a structure holdin gthe sample positions and values,
*        and other information.

*/

/* Local Variables: */
   LevMarData *data;
   double *pj;
   int iout;
   int k;
   int ncof;
   int vp;
   int vr;
   int w1;
   int w2;

/* Get a pointer to the data structure. */
   data = (LevMarData *) adata;

/* The Jacobian of the residuals with respect to the polynomial
   coefficients is constant (i.e. does not depend on the values of the
   polynomial coefficients). So we only need to calculate it once. If
   this is the first call, calculate the Jacobian and return it in "jac".
   otherwise, just return immediately retaining the supplied "jac" values
   (which will be the values returned by the previous call to this
   function). */
   if( data->init_jac ) {
      data->init_jac = 0;

/* Store the number of coefficients in one polynomial. */
      ncof = data->order1*data->order2;

/* Store a pointer to the next element of the returned Jacobian. */
      pj = jac;

/* Loop over all residuals. */
      for( vr = 0; vr < n; vr++ ) {

/* Calculate the polynomial output index, and sample index, that creates
   the current residual. */
         iout = vr/data->nsamp;
         k = vr - iout*data->nsamp;

/* Loop over all parameters (i.e. polynomial coefficients). */
         for( vp = 0; vp < m; vp++ ) {

/* If this coefficient is not used in the creation of the current
   polynomial output value, then the Jacobian value is zero. */
            if( vp/ncof != iout ) {
               *(pj++) = 0.0;

/* Otherwise, get the powers of the two polynomial inputs, to which
   the current coefficient relates. */
            } else {
               w1 = ( vp - iout*ncof )/data->order2;
               w2 = vp - iout*ncof - w1*data->order2;

/* Store the Jacobian. */
               *(pj++) = (data->xp1)[ w1 + k*data->order1 ]*
                          (data->xp2)[ w2 + k*data->order2 ];
            }
         }
      }
   }
}

static int MapMerge( AstMapping *this, int where, int series, int *nmap,
                     AstMapping ***map_list, int **invert_list, int *status ) {
/*
*  Name:
*     MapMerge

*  Purpose:
*     Simplify a sequence of Mappings containing a PolyMap.

*  Type:
*     Private function.

*  Synopsis:
*     #include "mapping.h"
*     int MapMerge( AstMapping *this, int where, int series, int *nmap,
*                   AstMapping ***map_list, int **invert_list, int *status )

*  Class Membership:
*     PolyMap method (over-rides the protected astMapMerge method
*     inherited from the Mapping class).

*  Description:
*     This function attempts to simplify a sequence of Mappings by
*     merging a nominated PolyMap in the sequence with its neighbours,
*     so as to shorten the sequence if possible.
*
*     In many cases, simplification will not be possible and the
*     function will return -1 to indicate this, without further
*     action.
*
*     In most cases of interest, however, this function will either
*     attempt to replace the nominated PolyMap with a Mapping which it
*     considers simpler, or to merge it with the Mappings which
*     immediately precede it or follow it in the sequence (both will
*     normally be considered). This is sufficient to ensure the
*     eventual simplification of most Mapping sequences by repeated
*     application of this function.
*
*     In some cases, the function may attempt more elaborate
*     simplification, involving any number of other Mappings in the
*     sequence. It is not restricted in the type or scope of
*     simplification it may perform, but will normally only attempt
*     elaborate simplification in cases where a more straightforward
*     approach is not adequate.

*  Parameters:
*     this
*        Pointer to the nominated PolyMap which is to be merged with
*        its neighbours. This should be a cloned copy of the PolyMap
*        pointer contained in the array element "(*map_list)[where]"
*        (see below). This pointer will not be annulled, and the
*        PolyMap it identifies will not be modified by this function.
*     where
*        Index in the "*map_list" array (below) at which the pointer
*        to the nominated PolyMap resides.
*     series
*        A non-zero value indicates that the sequence of Mappings to
*        be simplified will be applied in series (i.e. one after the
*        other), whereas a zero value indicates that they will be
*        applied in parallel (i.e. on successive sub-sets of the
*        input/output coordinates).
*     nmap
*        Address of an int which counts the number of Mappings in the
*        sequence. On entry this should be set to the initial number
*        of Mappings. On exit it will be updated to record the number
*        of Mappings remaining after simplification.
*     map_list
*        Address of a pointer to a dynamically allocated array of
*        Mapping pointers (produced, for example, by the astMapList
*        method) which identifies the sequence of Mappings. On entry,
*        the initial sequence of Mappings to be simplified should be
*        supplied.
*
*        On exit, the contents of this array will be modified to
*        reflect any simplification carried out. Any form of
*        simplification may be performed. This may involve any of: (a)
*        removing Mappings by annulling any of the pointers supplied,
*        (b) replacing them with pointers to new Mappings, (c)
*        inserting additional Mappings and (d) changing their order.
*
*        The intention is to reduce the number of Mappings in the
*        sequence, if possible, and any reduction will be reflected in
*        the value of "*nmap" returned. However, simplifications which
*        do not reduce the length of the sequence (but improve its
*        execution time, for example) may also be performed, and the
*        sequence might conceivably increase in length (but normally
*        only in order to split up a Mapping into pieces that can be
*        more easily merged with their neighbours on subsequent
*        invocations of this function).
*
*        If Mappings are removed from the sequence, any gaps that
*        remain will be closed up, by moving subsequent Mapping
*        pointers along in the array, so that vacated elements occur
*        at the end. If the sequence increases in length, the array
*        will be extended (and its pointer updated) if necessary to
*        accommodate any new elements.
*
*        Note that any (or all) of the Mapping pointers supplied in
*        this array may be annulled by this function, but the Mappings
*        to which they refer are not modified in any way (although
*        they may, of course, be deleted if the annulled pointer is
*        the final one).
*     invert_list
*        Address of a pointer to a dynamically allocated array which,
*        on entry, should contain values to be assigned to the Invert
*        attributes of the Mappings identified in the "*map_list"
*        array before they are applied (this array might have been
*        produced, for example, by the astMapList method). These
*        values will be used by this function instead of the actual
*        Invert attributes of the Mappings supplied, which are
*        ignored.
*
*        On exit, the contents of this array will be updated to
*        correspond with the possibly modified contents of the
*        "*map_list" array.  If the Mapping sequence increases in
*        length, the "*invert_list" array will be extended (and its
*        pointer updated) if necessary to accommodate any new
*        elements.
*     status
*        Pointer to the inherited status variable.

*  Returned Value:
*     If simplification was possible, the function returns the index
*     in the "map_list" array of the first element which was
*     modified. Otherwise, it returns -1 (and makes no changes to the
*     arrays supplied).

*  Notes:
*     - A value of -1 will be returned if this function is invoked
*     with the global error status set, or if it should fail for any
*     reason.
*/

/* Local Variables: */
   AstPolyMap *pmap0;    /* Nominated PolyMap */
   AstPolyMap *pmap1;    /* Neighbouring PolyMap */
   int i;                /* Index of neighbour */
   int iax_in;           /* Index of input coordinate */
   int iax_out;          /* Index of output coordinate */
   int ico;              /* Index of coefficient */
   int inv0;             /* Supplied Invert flag for nominated PolyMap */
   int inv1;             /* Supplied Invert flag for neighbouring PolyMap */
   int nc;               /* Number of coefficients */
   int nin;              /* Number of input coordinates for nominated PolyMap */
   int nout;             /* Number of output coordinates for nominated PolyMap */
   int ok;               /* Are PolyMaps equivalent? */
   int result;           /* Result value to return */
   int swap0;            /* Swap inputs and outputs for nominated PolyMap? */
   int swap1;            /* Swap inputs and outputs for neighbouring PolyMap? */

/* Initialise. */
   result = -1;

/* Check the global error status. */
   if ( !astOK ) return result;

/* Save a pointer to the nominated PolyMap. */
   pmap0 = (AstPolyMap *) ( *map_list )[ where ];

/* The only simplification which can currently be performed is to merge a PolyMap
   with its own inverse. This can only be done in series. Obviously,
   there are potentially other simplications which could be performed, but
   time does not currently allow these to be coded. */
   if( series ) {

/* Set a flag indicating if "input" and "output" needs to be swapped for
   the nominated PolyMap. */
      inv0 = ( *invert_list )[ where ];
      swap0 = ( inv0 != astGetInvert( pmap0 ) );

/* Get the number of inputs and outputs to the nominated PolyMap. */
      nin = !swap0 ? astGetNin( pmap0 ) : astGetNout( pmap0 );
      nout = !swap0 ? astGetNout( pmap0 ) : astGetNin( pmap0 );

/* Check each neighbour. */
      for( i = where - 1; i <= where + 1; i += 2 ) {

/* Continue with the next pass if the neighbour does not exist. */
         if( i < 0 || i >= *nmap ) continue;

/* Continue with the next pass if this neighbour is not a PermMap. */
         if( strcmp( "PolyMap", astGetClass( ( *map_list )[ i ] ) ) ) continue;

/* Get a pointer to it. */
         pmap1 = (AstPolyMap *) ( *map_list )[ i ];

/* Check it is used in the opposite direction to the nominated PolyMap. */
         if( ( *invert_list )[ i ] == ( *invert_list )[ where ] ) continue;

/* Set a flag indicating if "input" and "output" needs to be swapped for
   the neighbouring PolyMap. */
         inv1 = ( *invert_list )[ i ];
         swap1 = ( inv1 != astGetInvert( pmap1 ) );

/* Check the number of inputs and outputs are equal to the nominated
   PolyMap. */
         if( astGetNin( pmap1 ) != (!swap1 ? nin : nout ) &&
             astGetNout( pmap1 ) != (!swap1 ? nout : nin ) ) continue;

/* Check the forward coefficients are equal. */
         ok = 1;
         for( iax_out = 0; iax_out < nout && ok; iax_out++ ) {
            nc = pmap1->ncoeff_f[ iax_out ];
            if( nc != pmap0->ncoeff_f[ iax_out ] ) continue;

            for( ico = 0; ico < nc && ok; ico++ ) {

               if( !EQUAL( pmap1->coeff_f[ iax_out ][ ico ],
                           pmap0->coeff_f[ iax_out ][ ico ] ) ){
                  ok = 0;

               } else {
                  for( iax_in = 0; iax_in < nin && ok; iax_in++ ) {
                     ok = ( pmap1->power_f[ iax_out ][ ico ][ iax_in ] ==
                            pmap0->power_f[ iax_out ][ ico ][ iax_in ] );
                  }
               }
            }
         }
         if( !ok ) continue;

/* Check the inverse coefficients are equal. */
         ok = 1;
         for( iax_in = 0; iax_in < nin && ok; iax_in++ ) {
            nc = pmap1->ncoeff_i[ iax_in ];
            if( nc != pmap0->ncoeff_i[ iax_in ] ) continue;

            for( ico = 0; ico < nc && ok; ico++ ) {

               if( !EQUAL( pmap1->coeff_i[ iax_in ][ ico ],
                           pmap0->coeff_i[ iax_in ][ ico ] ) ){
                  ok = 0;

               } else {
                  for( iax_out = 0; iax_out < nout && ok; iax_out++ ) {
                     ok = ( pmap1->power_i[ iax_in ][ ico ][ iax_out ] ==
                            pmap0->power_i[ iax_in ][ ico ][ iax_out ] );
                  }
               }
            }
         }
         if( !ok ) continue;

/* If we get this far, then the nominated PolyMap and the current
   neighbour cancel each other out, so replace each by a UnitMap. */
         (void) astAnnul( pmap0 );
         (void) astAnnul( pmap1 );
         if( i < where ) {
            ( *map_list )[ where ] = (AstMapping *) astUnitMap( nout, "", status );
            ( *map_list )[ i ] = (AstMapping *) astUnitMap( nout, "", status );
            ( *invert_list )[ where ] = 0;
            ( *invert_list )[ i ] = 0;
            result = i;
         } else {
            ( *map_list )[ where ] = (AstMapping *) astUnitMap( nin, "", status );
            ( *map_list )[ i ] = (AstMapping *) astUnitMap( nin, "", status );
            ( *invert_list )[ where ] = 0;
            ( *invert_list )[ i ] = 0;
            result = where;
         }

/* Leave the loop. */
         break;
      }
   }

/* Return the result. */
   return result;
}

static AstPolyMap *PolyTran( AstPolyMap *this, int forward, double acc,
                             double *lbnd, double *ubnd, int *status ){
/*
*++
*  Name:
c     astPolyTran
f     AST_POLYTRAN

*  Purpose:
*     Fit a PolyMap inverse or forward transformation.

*  Type:
*     Public function.

*  Synopsis:
c     #include "polymap.h"
c     AstPolyMap *PolyTran( AstPolyMap *this, int forward, double acc,
c                           double *lbnd, double *ubnd, int *status )
f     RESULT = AST_POLYTRAN( THIS, FORWARD, ACC, LBND, UBND, STATUS )

*  Class Membership:
*     PolyMap method.

*  Description:
*     This function creates a new PolyMap which is a copy of the supplied
*     PolyMap, in which a specified transformation (forward or inverse)
*     has been replaced by a new polynomial function. The
*     coefficients of the new transformation are estimated by sampling
*     the other transformation and performing a least squares polynomial
*     fit in the opposite direction to the sampled positions and values.
*
*     The transformation to create is specified by the
c     "forward" parameter.
f     FORWARD parameter.
*     In what follows "X" refers to the inputs of the PolyMap, and "Y" to
*     the outputs of the PolyMap. The forward transformation transforms
*     input values (X) into output values (Y), and the inverse transformation
*     transforms output values (Y) into input values (X). Within a PolyMap,
*     each transformation is represented by an independent set of
*     polynomials: Y=P_f(X) for the forward transformation and X=P_i(Y)
*     for the inverse transformation.
*
c     If "forward" is zero,
f     If FORWARD is .FALSE.,
*     a new inverse transformation is created by
*     first finding the output values (Y) using the forward transformation
*     (which must be available) at a regular grid of points (X) covering a
*     rectangular region of the PolyMap's input space. The coefficients of
*     the required inverse polynomial, X=P_i(Y), are chosen in order to
*     minimise the sum of the squared residuals between the sampled values
*     of X and P_i(Y).
*
c     If "forward" is non-zero,
f     If FORWARD is .TRUE.,
*     a new forward transformation is created
*     by first finding the input values (X) using the inverse transformation
*     (which must be available) at a regular grid of points (Y) covering a
*     rectangular region of the PolyMap's output space. The coefficients of
*     the required forward polynomial, Y=P_f(X), are chosen in order to
*     minimise the sum of the squared residuals between the sampled values
*     of Y and P_f(X).
*
*     This fitting process is performed repeatedly with increasing
*     polynomial orders (starting with quadratic) until the specified
*     accuracy is achieved (up to a maximum of 20).

*  Parameters:
c     this
f     THIS = INTEGER (Given)
*        Pointer to the original Mapping.
c     forward
f     FORWARD = LOGICAL (Given)
c        If non-zero,
f        If .TRUE.,
*        the forward PolyMap transformation is replaced. Otherwise the
*        inverse transformation is replaced.
c     acc
f     ACC = DOUBLE (Given)
*        The required accuracy, expressed as a geodesic distance within
*        the PolyMap's input space (if
c        "forward" is zero)  or output space (if "forward" is non-zero).
f        FORWARD is .FALSE.) or output space (if FORWARD is .TRUE.).
c     lbnd
f     LBND( * ) = DOUBLE PRECISION (Given)
c        Pointer to an
f        An
*        array holding the lower bounds of a rectangular region within
*        the PolyMap's input space (if
c        "forward" is zero)  or output space (if "forward" is non-zero).
f        FORWARD is .FALSE.) or output space (if FORWARD is .TRUE.).
*        The new polynomial will be evaluated over this rectangle. The
*        length of this array should equal the value of the PolyMap's Nin
*        or Nout attribute, depending on
c        "forward".
f        FORWARD.
c     ubnd
f     UBND( * ) = DOUBLE PRECISION (Given)
c        Pointer to an
f        An
*        array holding the upper bounds of a rectangular region within
*        the PolyMap's input space (if
c        "forward" is zero)  or output space (if "forward" is non-zero).
f        FORWARD is .FALSE.) or output space (if FORWARD is .TRUE.).
*        The new polynomial will be evaluated over this rectangle.  The
*        length of this array should equal the value of the PolyMap's Nin
*        or Nout attribute, depending on
c        "forward".
f        FORWARD.
f     STATUS = INTEGER (Given and Returned)
f        The global status.

*  Returned Value:
c     astPolyTran()
f     AST_POLYTRAN = INTEGER
*        A pointer to the new PolyMap.

*  Notes:
*     - This function can only be used on 1D or 2D PolyMaps which have
*     the same number of inputs and outputs.
*     - An error will be reported if a successfull fit cannot be found.
*     - A null Object pointer (AST__NULL) will be returned if this
c     function is invoked with the AST error status set, or if it
f     function is invoked with STATUS set to an error value, or if it
*     should fail for any reason.
*--
*/

/* Local Variables: */
   AstPolyMap *result;

/* Initialise. */
   result = NULL;

/* Check the inherited status. */
   if ( !astOK ) return result;

/* Take a copy of the supplied PolyMap. */
   result = astCopy( this );

/* Replace the required transformation. */
   CreateInverse( result, forward, acc, lbnd, ubnd, status );

/* If an error occurred, annul the returned PolyMap. */
   if ( !astOK ) result = astAnnul( result );

/* Return the result. */
   return result;
}

static void StoreArrays( AstPolyMap *this, int forward, int ncoeff,
                         const double *coeff, int *status ){
/*
*  Name:
*     StoreArrays

*  Purpose:
*     Store the dynamic arrays for a single transformation within a PolyMap

*  Type:
*     Private function.

*  Synopsis:
*     #include "polymap.h"
*     void StoreArrays( AstPolyMap *this, int forward, int ncoeff,
*                       const double *coeff, int *status )

*  Class Membership:
*     PolyMap initialiser.

*  Description:
*     This function sets up the arrays within a PolyMap structure that
*     describes either the forward or inverse transformation.

*  Parameters:
*     this
*        The PolyMap.
*     forward
*        If non-zero, replace the forward transformation. Otherwise,
*        replace the inverse transformation.
*     ncoeff
*        The number of non-zero coefficients necessary to define the
*        specified transformation of the PolyMap. If zero is supplied, the
*        transformation will be undefined.
*     coeff
*        An array containing "ncof*( 2 + nin )" elements. Each group
*	 of "2 + nin" adjacent elements describe a single coefficient of
*	 the transformation. Within each such group, the first
*	 element is the coefficient value; the next element is the
*	 integer index of the PolyMap output which uses the coefficient
*	 within its defining polynomial (the first output has index 1);
*	 the remaining elements of the group give the integer powers to
*	 use with each input coordinate value (powers must not be
*	 negative).
*     status
*        Pointer to inherited status.
*/

/* Local Variables: */
   const double *group;          /* Pointer to start of next coeff. description */
   int *pows;                    /* Pointer to powers for current coeff. */
   int gsize;                    /* Length of each coeff. description */
   int i;                        /* Loop count */
   int ico;                      /* Index of next coeff. for current input or output */
   int iin;                      /* Input index extracted from coeff. description */
   int iout;                     /* Output index extracted from coeff. description */
   int j;                        /* Loop count */
   int nin;                      /* Number of inputs */
   int nout;                     /* Number of outputs */
   int pow;                      /* Power extracted from coeff. description */

/* Check the global status. */
   if ( !astOK ) return;

/* Get the number of inputs and outputs. */
   nin = astGetNin( this );
   nout = astGetNout( this );

/* First Free any existing arrays. */
   FreeArrays( this, forward, status );

/* Now initialise the forward transformation, if required. */
   if( forward && ncoeff > 0 ) {

/* Create the arrays decribing the forward transformation. */
      this->ncoeff_f = astMalloc( sizeof( int )*(size_t) nout );
      this->mxpow_f = astMalloc( sizeof( int )*(size_t) nin );
      this->power_f = astMalloc( sizeof( int ** )*(size_t) nout );
      this->coeff_f = astMalloc( sizeof( double * )*(size_t) nout );
      if( astOK ) {

/* Initialise the count of coefficients for each output coordinate to zero. */
         for( i = 0; i < nout; i++ ) this->ncoeff_f[ i ] = 0;

/* Initialise max power for each input coordinate to zero. */
         for( j = 0; j < nin; j++ ) this->mxpow_f[ j ] = 0;

/* Scan through the supplied forward coefficient array, counting the
   number of coefficients which relate to each output. Also find the
   highest power used for each input axis. Report errors if any unusable
   values are found in the supplied array. */
         group = coeff;
         gsize = 2 + nin;
         for( i = 0; i < ncoeff && astOK; i++, group += gsize ) {

            iout = floor( group[ 1 ] + 0.5 );
            if( iout < 1 || iout > nout ) {
               astError( AST__BADCI, "astInitPolyMap(%s): Forward "
                         "coefficient %d referred to an illegal output "
                         "coordinate %d.", status, astGetClass( this ), i + 1,
                         iout );
               astError( AST__BADCI, "This number should be in the "
                         "range 1 to %d.", status, nout );
               break;
            }

            this->ncoeff_f[ iout - 1 ]++;

            for( j = 0; j < nin; j++ ) {
               pow = floor( group[ 2 + j ] + 0.5 );
               if( pow < 0 ) {
                  astError( AST__BADPW, "astInitPolyMap(%s): Forward "
                            "coefficient %d has a negative power (%d) "
                            "for input coordinate %d.", status,
                            astGetClass( this ), i + 1, pow, j + 1 );
                  astError( AST__BADPW, "All powers should be zero or "
                            "positive." , status);
                  break;
               }
               if( pow > this->mxpow_f[ j ] ) this->mxpow_f[ j ] = pow;
            }
         }

/* Allocate the arrays to store the input powers associated with each
   coefficient, and the coefficient values. Reset the coefficient count
   for each axis to zero afterwards so that we can use the array as an index
   to the next vacant slot withint he following loop. */
         for( i = 0; i < nout; i++ ) {
            this->power_f[ i ] = astMalloc( sizeof( int * )*
                                           (size_t) this->ncoeff_f[ i ] );
            this->coeff_f[ i ] = astMalloc( sizeof( double )*
                                           (size_t) this->ncoeff_f[ i ] );
            this->ncoeff_f[ i ] = 0;
         }

         if( astOK ) {

/* Extract the coefficient values and powers form the supplied array and
   store them in the arrays created above. */
            group = coeff;
            for( i = 0; i < ncoeff && astOK; i++, group += gsize ) {
               iout = floor( group[ 1 ] + 0.5 ) - 1;
               ico = ( this->ncoeff_f[ iout ] )++;
               this->coeff_f[ iout ][ ico ] = group[ 0 ];

               pows = astMalloc( sizeof( int )*(size_t) nin );
               this->power_f[ iout ][ ico ] = pows;
               if( astOK ) {
                  for( j = 0; j < nin; j++ ) {
                     pows[ j ] = floor( group[ 2 + j ] + 0.5 );
                  }
               }
            }
         }
      }
   }

/* Now initialise the inverse transformation, if required. */
   if( !forward && ncoeff > 0 ) {

/* Create the arrays decribing the inverse transformation. */
      this->ncoeff_i = astMalloc( sizeof( int )*(size_t) nin );
      this->mxpow_i = astMalloc( sizeof( int )*(size_t) nout );
      this->power_i = astMalloc( sizeof( int ** )*(size_t) nin );
      this->coeff_i = astMalloc( sizeof( double * )*(size_t) nin );
      if( astOK ) {

/* Initialise the count of coefficients for each input coordinate to zero. */
         for( i = 0; i < nin; i++ ) this->ncoeff_i[ i ] = 0;

/* Initialise max power for each output coordinate to zero. */
         for( j = 0; j < nout; j++ ) this->mxpow_i[ j ] = 0;

/* Scan through the supplied inverse coefficient array, counting the
   number of coefficients which relate to each input. Also find the
   highest power used for each output axis. Report errors if any unusable
   values are found in the supplied array. */
         group = coeff;

         gsize = 2 + nout;
         for( i = 0; i < ncoeff && astOK; i++, group += gsize ) {

            iin = floor( group[ 1 ] + 0.5 );
            if( iin < 1 || iin > nin ) {
               astError( AST__BADCI, "astInitPolyMap(%s): Inverse "
                         "coefficient %d referred to an illegal input "
                         "coordinate %d.", status, astGetClass( this ),
                         i + 1, iin );
               astError( AST__BADCI, "This number should be in the "
                         "range 1 to %d.", status, nin );
               break;
            }

            this->ncoeff_i[ iin - 1 ]++;

            for( j = 0; j < nout; j++ ) {
               pow = floor( group[ 2 + j ] + 0.5 );
               if( pow < 0 ) {
                  astError( AST__BADPW, "astInitPolyMap(%s): Inverse "
                            "coefficient %d has a negative power (%d) "
                            "for output coordinate %d.", status,
                            astGetClass( this ), i + 1, pow, j + 1 );
                  astError( AST__BADPW, "All powers should be zero or "
                            "positive." , status);
                  break;
               }
               if( pow > this->mxpow_i[ j ] ) this->mxpow_i[ j ] = pow;
            }
         }

/* Allocate the arrays to store the output powers associated with each
   coefficient, and the coefficient values. Reset the coefficient count
   for each axis to zero afterwards so that we can use the array as an index
   to the next vacant slot within the following loop. */
         for( i = 0; i < nin; i++ ) {
            this->power_i[ i ] = astMalloc( sizeof( int * )*
                                           (size_t) this->ncoeff_i[ i ] );
            this->coeff_i[ i ] = astMalloc( sizeof( double )*
                                           (size_t) this->ncoeff_i[ i ] );
            this->ncoeff_i[ i ] = 0;
         }

         if( astOK ) {

/* Extract the coefficient values and powers form the supplied array and
   store them in the arrays created above. */
            group = coeff;
            for( i = 0; i < ncoeff && astOK; i++, group += gsize ) {
               iin = floor( group[ 1 ] + 0.5 ) - 1;
               ico = ( this->ncoeff_i[ iin ] )++;
               this->coeff_i[ iin ][ ico ] = group[ 0 ];

               pows = astMalloc( sizeof( int )*(size_t) nout );
               this->power_i[ iin ][ ico ] = pows;
               if( astOK ) {
                  for( j = 0; j < nout; j++ ) {
                     pows[ j ] = floor( group[ 2 + j ] + 0.5 );
                  }
               }
            }
         }
      }
   }
}

static AstPointSet *Transform( AstMapping *this, AstPointSet *in,
                               int forward, AstPointSet *out, int *status ) {
/*
*  Name:
*     Transform

*  Purpose:
*     Apply a PolyMap to transform a set of points.

*  Type:
*     Private function.

*  Synopsis:
*     #include "polymap.h"
*     AstPointSet *Transform( AstMapping *this, AstPointSet *in,
*                             int forward, AstPointSet *out, int *status )

*  Class Membership:
*     PolyMap member function (over-rides the astTransform protected
*     method inherited from the Mapping class).

*  Description:
*     This function takes a PolyMap and a set of points encapsulated in a
*     PointSet and transforms the points.

*  Parameters:
*     this
*        Pointer to the PolyMap.
*     in
*        Pointer to the PointSet holding the input coordinate data.
*     forward
*        A non-zero value indicates that the forward coordinate transformation
*        should be applied, while a zero value requests the inverse
*        transformation.
*     out
*        Pointer to a PointSet which will hold the transformed (output)
*        coordinate values. A NULL value may also be given, in which case a
*        new PointSet will be created by this function.
*     status
*        Pointer to the inherited status variable.

*  Returned Value:
*     Pointer to the output (possibly new) PointSet.

*  Notes:
*     -  A null pointer will be returned if this function is invoked with the
*     global error status set, or if it should fail for any reason.
*     -  The number of coordinate values per point in the input PointSet must
*     match the number of columns in the PolyMap being applied.
*     -  The number of coordinate values per point in the output PointSet will
*     equal the number of rows in the PolyMap being applied.
*     -  If an output PointSet is supplied, it must have space for sufficient
*     number of points and coordinate values per point to accommodate the
*     result. Any excess space will be ignored.
*/

/* Local Variables: */
   AstPointSet *result;          /* Pointer to output PointSet */
   AstPolyMap *map;              /* Pointer to PolyMap to be applied */
   double **coeff;               /* Pointer to coefficient value arrays */
   double **ptr_in;              /* Pointer to input coordinate data */
   double **ptr_out;             /* Pointer to output coordinate data */
   double **work;                /* Pointer to exponentiated axis values */
   double *outcof;               /* Pointer to next coefficient value */
   double *pwork;                /* Pointer to exponentiated axis values */
   double outval;                /* Output axis value */
   double term;                  /* Term to be added to output value */
   double x;                     /* Input axis value */
   double xp;                    /* Exponentiated input axis value */
   int ***power;                 /* Pointer to coefficient power arrays */
   int **outpow;                 /* Pointer to next set of axis powers */
   int *mxpow;                   /* Pointer to max used power for each input */
   int *ncoeff;                  /* Pointer to no. of coefficients */
   int in_coord;                 /* Index of output coordinate */
   int ico;                      /* Coefficient index */
   int ip;                       /* Axis power */
   int nc;                       /* No. of coefficients in polynomial */
   int ncoord_in;                /* Number of coordinates per input point */
   int ncoord_out;               /* Number of coordinates per output point */
   int npoint;                   /* Number of points */
   int out_coord;                /* Index of output coordinate */
   int point;                    /* Loop counter for points */
   int pow;                      /* Next axis power */

/* Check the global error status. */
   if ( !astOK ) return NULL;

/* Obtain a pointer to the PolyMap. */
   map = (AstPolyMap *) this;

/* Apply the parent mapping using the stored pointer to the Transform member
   function inherited from the parent Mapping class. This function validates
   all arguments and generates an output PointSet if necessary, but does not
   actually transform any coordinate values. */
   result = (*parent_transform)( this, in, forward, out, status );

/* We will now extend the parent astTransform method by performing the
   calculations needed to generate the output coordinate values. */

/* Determine the numbers of points and coordinates per point from the input
   and output PointSets and obtain pointers for accessing the input and
   output coordinate values. */
   ncoord_in = astGetNcoord( in );
   ncoord_out = astGetNcoord( result );
   npoint = astGetNpoint( in );
   ptr_in = astGetPoints( in );
   ptr_out = astGetPoints( result );

/* Determine whether to apply the forward or inverse mapping, according to the
   direction specified and whether the mapping has been inverted. */
   if ( astGetInvert( map ) ) forward = !forward;

/* Get a pointer to the arrays holding the required coefficient values
   and powers, according to the direction of mapping required. */
   if ( forward ) {
      ncoeff = map->ncoeff_f;
      coeff = map->coeff_f;
      power = map->power_f;
      mxpow = map->mxpow_f;
   } else {
      ncoeff = map->ncoeff_i;
      coeff = map->coeff_i;
      power = map->power_i;
      mxpow = map->mxpow_i;
   }

/* Allocate memory to hold the required powers of the input axis values. */
   work = astMalloc( sizeof( double * )*(size_t) ncoord_in );
   for( in_coord = 0; in_coord < ncoord_in; in_coord++ ) {
      work[ in_coord ] = astMalloc( sizeof( double )*(size_t) ( mxpow[ in_coord ] + 1 ) );
   }

/* Perform coordinate arithmetic. */
/* ------------------------------ */
   if ( astOK ) {

/* Loop to apply the polynomial to each point in turn.*/
      for ( point = 0; point < npoint; point++ ) {

/* Find the required powers of the input axis values and store them in
   the work array. */
         for( in_coord = 0; in_coord < ncoord_in; in_coord++ ) {
            pwork = work[ in_coord ];
            pwork[ 0 ] = 1.0;
            x = ptr_in[ in_coord ][ point ];
            if( x == AST__BAD ) {
               for( ip = 1; ip <= mxpow[ in_coord ]; ip++ ) pwork[ ip ] = AST__BAD;
            } else {
               for( ip = 1; ip <= mxpow[ in_coord ]; ip++ ) {
                  pwork[ ip ] = pwork[ ip - 1 ]*x;
               }
            }
         }

/* Loop round each output. */
         for( out_coord = 0; out_coord < ncoord_out; out_coord++ ) {

/* Initialise the output value. */
            outval = 0.0;

/* Get pointers to the coefficients and powers for this output. */
            outcof = coeff[ out_coord ];
            outpow = power[ out_coord ];

/* Loop round all polynomial coefficients.*/
            nc = ncoeff[ out_coord ];
            for ( ico = 0; ico < nc && outval != AST__BAD;
                  ico++, outcof++, outpow++ ) {

/* Initialise the current term to be equal to the value of the coefficient.
   If it is bad, store a bad output value. */
               term = *outcof;
               if( term == AST__BAD ) {
                  outval = AST__BAD;

/* Otherwise, loop round all inputs */
               } else {
                  for( in_coord = 0; in_coord < ncoord_in; in_coord++ ) {

/* Get the power of the current input axis value used by the current
   coefficient. If it is zero, pass on. */
                     pow = (*outpow)[ in_coord ];
                     if( pow > 0 ) {

/* Get the axis value raised to the appropriate power. */
                        xp = work[ in_coord ][ pow ];

/* If bad, set the output value bad and break. */
                        if( xp == AST__BAD ) {
                           outval = AST__BAD;
                           break;

/* Otherwise multiply the current term by the exponentiated axis value. */
                        } else {
                           term *= xp;
                        }
                     }
                  }
               }

/* Increment the output value by the current term of the polynomial. */
               outval += term;

            }

/* Store the output value. */
            ptr_out[ out_coord ][ point ] = outval;

         }
      }
   }

/* Free resources. */
   for( in_coord = 0; in_coord < ncoord_in; in_coord++ ) {
      work[ in_coord ] = astFree( work[ in_coord ] );
   }
   work = astFree( work );

/* Return a pointer to the output PointSet. */
   return result;
}

/* Functions which access class attributes. */
/* ---------------------------------------- */
/* Implement member functions to access the attributes associated with
   this class using the macros defined for this purpose in the
   "object.h" file. For a description of each attribute, see the class
   interface (in the associated .h file). */

/* Copy constructor. */
/* ----------------- */
static void Copy( const AstObject *objin, AstObject *objout, int *status ) {
/*
*  Name:
*     Copy

*  Purpose:
*     Copy constructor for PolyMap objects.

*  Type:
*     Private function.

*  Synopsis:
*     void Copy( const AstObject *objin, AstObject *objout, int *status )

*  Description:
*     This function implements the copy constructor for PolyMap objects.

*  Parameters:
*     objin
*        Pointer to the object to be copied.
*     objout
*        Pointer to the object being constructed.
*     status
*        Pointer to the inherited status variable.

*  Returned Value:
*     void

*  Notes:
*     -  This constructor makes a deep copy, including a copy of the
*     coefficients associated with the input PolyMap.
*/


/* Local Variables: */
   AstPolyMap *in;               /* Pointer to input PolyMap */
   AstPolyMap *out;              /* Pointer to output PolyMap */
   int nin;                      /* No. of input coordinates */
   int nout;                     /* No. of output coordinates */
   int i;                        /* Loop count */
   int j;                        /* Loop count */

/* Check the global error status. */
   if ( !astOK ) return;

/* Obtain pointers to the input and output PolyMaps. */
   in = (AstPolyMap *) objin;
   out = (AstPolyMap *) objout;

/* Nullify the pointers stored in the output object since these will
   currently be pointing at the input data (since the output is a simple
   byte-for-byte copy of the input). Otherwise, the input data could be
   freed by accidient if the output object is deleted due to an error
   occuring in this function. */
   out->ncoeff_f = NULL;
   out->power_f = NULL;
   out->coeff_f = NULL;
   out->mxpow_f = NULL;

   out->ncoeff_i = NULL;
   out->power_i = NULL;
   out->coeff_i = NULL;
   out->mxpow_i = NULL;

/* Get the number of inputs and outputs of the uninverted Mapping. */
   nin = ( (AstMapping *) in )->nin;
   nout = ( (AstMapping *) in )->nout;

/* Copy the number of coefficients associated with each output of the forward
   transformation. */
   if( in->ncoeff_f ) {
      out->ncoeff_f = (int *) astStore( NULL, (void *) in->ncoeff_f,
                                        sizeof( int )*(size_t) nout );

/* Copy the maximum power of each input axis value used by the forward
   transformation. */
      out->mxpow_f = (int *) astStore( NULL, (void *) in->mxpow_f,
                                       sizeof( int )*(size_t) nin );

/* Copy the coefficient values used by the forward transformation. */
      if( in->coeff_f ) {
         out->coeff_f = astMalloc( sizeof( double * )*(size_t) nout );
         if( astOK ) {
            for( i = 0; i < nout; i++ ) {
               out->coeff_f[ i ] = (double *) astStore( NULL, (void *) in->coeff_f[ i ],
                                                     sizeof( double )*(size_t) in->ncoeff_f[ i ] );
            }
         }
      }

/* Copy the input axis powers associated with each coefficient of the forward
   transformation. */
      if( in->power_f ) {
         out->power_f = astMalloc( sizeof( int ** )*(size_t) nout );
         if( astOK ) {
            for( i = 0; i < nout; i++ ) {
               out->power_f[ i ] = astMalloc( sizeof( int * )*(size_t) in->ncoeff_f[ i ] );
               if( astOK ) {
                  for( j = 0; j < in->ncoeff_f[ i ]; j++ ) {
                     out->power_f[ i ][ j ] = (int *) astStore( NULL, (void *) in->power_f[ i ][ j ],
                                                                sizeof( int )*(size_t) nin );
                  }
               }
            }
         }
      }
   }

/* Do the same for the inverse transformation. */
   if( in->ncoeff_i ) {
      out->ncoeff_i = (int *) astStore( NULL, (void *) in->ncoeff_i,
                                        sizeof( int )*(size_t) nin );

      out->mxpow_i = (int *) astStore( NULL, (void *) in->mxpow_i,
                                       sizeof( int )*(size_t) nout );

      if( in->coeff_i ) {
         out->coeff_i = astMalloc( sizeof( double * )*(size_t) nin );
         if( astOK ) {
            for( i = 0; i < nin; i++ ) {
               out->coeff_i[ i ] = (double *) astStore( NULL, (void *) in->coeff_i[ i ],
                                                     sizeof( double )*(size_t) in->ncoeff_i[ i ] );
            }
         }
      }

      if( in->power_i ) {
         out->power_i = astMalloc( sizeof( int ** )*(size_t) nin );
         if( astOK ) {
            for( i = 0; i < nin; i++ ) {
               out->power_i[ i ] = astMalloc( sizeof( int * )*(size_t) in->ncoeff_i[ i ] );
               if( astOK ) {
                  for( j = 0; j < in->ncoeff_i[ i ]; j++ ) {
                     out->power_i[ i ][ j ] = (int *) astStore( NULL, (void *) in->power_i[ i ][ j ],
                                                                sizeof( int )*(size_t) nout );
                  }
               }
            }
         }
      }
   }

/* If an error has occurred, free the output arrays. */
   if( !astOK ) {
      FreeArrays( out, 1, status );
      FreeArrays( out, 0, status );
   }

   return;

}

/* Destructor. */
/* ----------- */
static void Delete( AstObject *obj, int *status ) {
/*
*  Name:
*     Delete

*  Purpose:
*     Destructor for PolyMap objects.

*  Type:
*     Private function.

*  Synopsis:
*     void Delete( AstObject *obj, int *status )

*  Description:
*     This function implements the destructor for PolyMap objects.

*  Parameters:
*     obj
*        Pointer to the object to be deleted.
*     status
*        Pointer to the inherited status variable.

*  Returned Value:
*     void

*  Notes:
*     This function attempts to execute even if the global error status is
*     set.
*/

/* Local Variables: */
   AstPolyMap *this;            /* Pointer to PolyMap */

/* Obtain a pointer to the PolyMap structure. */
   this = (AstPolyMap *) obj;

/* Free the arrays. */
   FreeArrays( this, 1, status );
   FreeArrays( this, 0, status );

}

/* Dump function. */
/* -------------- */
static void Dump( AstObject *this_object, AstChannel *channel, int *status ) {
/*
*  Name:
*     Dump

*  Purpose:
*     Dump function for PolyMap objects.

*  Type:
*     Private function.

*  Synopsis:
*     void Dump( AstObject *this, AstChannel *channel, int *status )

*  Description:
*     This function implements the Dump function which writes out data
*     for the PolyMap class to an output Channel.

*  Parameters:
*     this
*        Pointer to the PolyMap whose data are being written.
*     channel
*        Pointer to the Channel to which the data are being written.
*     status
*        Pointer to the inherited status variable.
*/

#define KEY_LEN 50               /* Maximum length of a keyword */

/* Local Variables: */
   AstPolyMap *this;             /* Pointer to the PolyMap structure */
   char buff[ KEY_LEN + 1 ];     /* Buffer for keyword string */
   char comm[ 100 ];             /* Buffer for comment string */
   int i;                        /* Loop index */
   int iv;                       /* Vectorised keyword index */
   int j;                        /* Loop index */
   int k;                        /* Loop index */
   int nin;                      /* No. of input coords */
   int nout;                     /* No. of output coords */

/* Check the global error status. */
   if ( !astOK ) return;

/* Obtain a pointer to the PolyMap structure. */
   this = (AstPolyMap *) this_object;

/* Find the number of inputs and outputs of the uninverted Mapping. */
   nin = ( (AstMapping *) this )->nin;
   nout = ( (AstMapping *) this )->nout;

/* Write out values representing the instance variables for the
   PolyMap class.  */

/* First do the forward transformation arrays. Check they are used. */
   if( this->ncoeff_f ) {

/* Store the maximum power of each input axis value used by the forward
   transformation. */
      for( i = 0; i < nin; i++ ){
         (void) sprintf( buff, "MPF%d", i + 1 );
         (void) sprintf( comm, "Max. power of input %d in any forward polynomial", i + 1 );
          astWriteInt( channel, buff, 1, 1, (this->mxpow_f)[ i ], comm );
      }

/* Store the number of coefficients associated with each output of the forward
   transformation. */
      for( i = 0; i < nout; i++ ){
         (void) sprintf( buff, "NCF%d", i + 1 );
         (void) sprintf( comm, "No. of coeff.s for forward polynomial %d", i + 1 );
         astWriteInt( channel, buff, 1, 1, (this->ncoeff_f)[ i ], comm );
      }

/* Store the coefficient values used by the forward transformation. */
      iv = 1;
      for( i = 0; i < nout; i++ ){
         for( j = 0; j < this->ncoeff_f[ i ]; j++, iv++ ){
            if( (this->coeff_f)[ i ][ j ] != AST__BAD ) {
               (void) sprintf( buff, "CF%d", iv );
               (void) sprintf( comm, "Coeff %d of forward polynomial %d", j + 1, i + 1 );
               astWriteDouble( channel, buff, 1, 1, (this->coeff_f)[ i ][ j ], comm );
            }
         }
      }

/* Store the input axis powers associated with each coefficient of the forward
   transformation. */
      iv = 1;
      for( i = 0; i < nout; i++ ){
         for( j = 0; j < this->ncoeff_f[ i ]; j++ ){
            for( k = 0; k < nin; k++, iv++ ){
               if( (this->power_f)[ i ][ j ][ k ] > 0 ) {
                  (void) sprintf( buff, "PF%d", iv );
                  (void) sprintf( comm, "Power of i/p %d for coeff %d of fwd poly %d", k + 1, j + 1, i + 1 );
                  astWriteDouble( channel, buff, 1, 1, (this->power_f)[ i ][ j ][ k ], comm );
               }
            }
         }
      }
   }

/* Now do the inverse transformation arrays. Check they are used. */
   if( this->ncoeff_i ) {

/* Store the maximum power of each output axis value used by the inverse
   transformation. */
      for( i = 0; i < nout; i++ ){
         (void) sprintf( buff, "MPI%d", i + 1 );
         (void) sprintf( comm, "Max. power of output %d in any inverse polynomial", i + 1 );
          astWriteInt( channel, buff, 1, 1, (this->mxpow_i)[ i ], comm );
      }

/* Store the number of coefficients associated with each input of the inverse
   transformation. */
      for( i = 0; i < nin; i++ ){
         (void) sprintf( buff, "NCI%d", i + 1 );
         (void) sprintf( comm, "No. of coeff.s for inverse polynomial %d", i + 1 );
         astWriteInt( channel, buff, 1, 1, (this->ncoeff_i)[ i ], comm );
      }

/* Store the coefficient values used by the inverse transformation. */
      iv = 1;
      for( i = 0; i < nin; i++ ){
         for( j = 0; j < this->ncoeff_i[ i ]; j++, iv++ ){
            if( (this->coeff_i)[ i ][ j ] != AST__BAD ) {
               (void) sprintf( buff, "CI%d", iv );
               (void) sprintf( comm, "Coeff %d of inverse polynomial %d", j + 1, i + 1 );
               astWriteDouble( channel, buff, 1, 1, (this->coeff_i)[ i ][ j ], comm );
            }
         }
      }

/* Store the output axis powers associated with each coefficient of the inverse
   transformation. */
      iv = 1;
      for( i = 0; i < nin; i++ ){
         for( j = 0; j < this->ncoeff_i[ i ]; j++ ){
            for( k = 0; k < nout; k++, iv++ ){
               if( (this->power_i)[ i ][ j ][ k ] > 0 ) {
                  (void) sprintf( buff, "PI%d", iv );
                  (void) sprintf( comm, "Power of o/p %d for coeff %d of inv poly %d", k + 1, j + 1, i + 1 );
                  astWriteDouble( channel, buff, 1, 1, (this->power_i)[ i ][ j ][ k ], comm );
               }
            }
         }
      }
   }

/* Undefine macros local to this function. */
#undef KEY_LEN
}

/* Standard class functions. */
/* ========================= */
/* Implement the astIsAPolyMap and astCheckPolyMap functions using the macros
   defined for this purpose in the "object.h" header file. */
astMAKE_ISA(PolyMap,Mapping)
astMAKE_CHECK(PolyMap)

AstPolyMap *astPolyMap_( int nin, int nout, int ncoeff_f, const double coeff_f[],
                         int ncoeff_i, const double coeff_i[], const char *options, int *status, ...){
/*
*++
*  Name:
c     astPolyMap
f     AST_POLYMAP

*  Purpose:
*     Create a PolyMap.

*  Type:
*     Public function.

*  Synopsis:
c     #include "polymap.h"
c     AstPolyMap *astPolyMap( int nin, int nout, int ncoeff_f, const double coeff_f[],
c                             int ncoeff_i, const double coeff_i[],
c                             const char *options, ... )
f     RESULT = AST_POLYMAP( NIN, NOUT, NCOEFF_F, COEFF_F, NCOEFF_I, COEFF_I,
f                           OPTIONS, STATUS )

*  Class Membership:
*     PolyMap constructor.

*  Description:
*     This function creates a new PolyMap and optionally initialises
*     its attributes.
*
*     A PolyMap is a form of Mapping which performs a general polynomial
*     transformation.  Each output coordinate is a polynomial function of
*     all the input coordinates. The coefficients are specified separately
*     for each output coordinate. The forward and inverse transformations
*     are defined independantly by separate sets of coefficients.

*  Parameters:
c     nin
f     NIN = INTEGER (Given)
*        The number of input coordinates.
c     nout
f     NOUT = INTEGER (Given)
*        The number of output coordinates.
c     ncoeff_f
f     NCOEFF_F = INTEGER (Given)
*        The number of non-zero coefficients necessary to define the
*        forward transformation of the PolyMap. If zero is supplied, the
*        forward transformation will be undefined.
c     coeff_f
f     COEFF_F( * ) = DOUBLE PRECISION (Given)
*        An array containing
c        "ncoeff_f*( 2 + nin )" elements. Each group of "2 + nin"
f        "NCOEFF_F*( 2 + NIN )" elements. Each group of "2 + NIN"
*        adjacent elements describe a single coefficient of the forward
*        transformation. Within each such group, the first element is the
*        coefficient value; the next element is the integer index of the
*        PolyMap output which uses the coefficient within its defining
*        polynomial (the first output has index 1); the remaining elements
*        of the group give the integer powers to use with each input
*        coordinate value (powers must not be negative, and floating
*        point values are rounded to the nearest integer).
c        If "ncoeff_f" is zero, a NULL pointer may be supplied for "coeff_f".
*
*        For instance, if the PolyMap has 3 inputs and 2 outputs, each group
*        consisting of 5 elements, A groups such as "(1.2, 2.0, 1.0, 3.0, 0.0)"
*        describes a coefficient with value 1.2 which is used within the
*        definition of output 2. The output value is incremented by the
*        product of the coefficient value, the value of input coordinate
*        1 raised to the power 1, and the value of input coordinate 2 raised
*        to the power 3. Input coordinate 3 is not used since its power is
*        specified as zero. As another example, the group "(-1.0, 1.0,
*        0.0, 0.0, 0.0 )" describes adds a constant value -1.0 onto
*        output 1 (it is a constant value since the power for every input
*        axis is given as zero).
*
c        Each final output coordinate value is the sum of the "ncoeff_f" terms
c        described by the "ncoeff_f" groups within the supplied array.
f        Each final output coordinate value is the sum of the "NCOEFF_F" terms
f        described by the "NCOEFF_F" groups within the supplied array.
c     ncoeff_i
f     NCOEFF_I = INTEGER (Given)
*        The number of non-zero coefficients necessary to define the
*        inverse transformation of the PolyMap. If zero is supplied, the
*        inverse transformation will be undefined.
c     coeff_i
f     COEFF_I( * ) = DOUBLE PRECISION (Given)
*        An array containing
c        "ncoeff_i*( 2 + nout )" elements. Each group of "2 + nout"
f        "NCOEFF_I*( 2 + NOUT )" elements. Each group of "2 + NOUT"
*        adjacent elements describe a single coefficient of the inverse
c        transformation, using the same schame as "coeff_f",
f        transformation, using the same schame as "COEFF_F",
*        except that "inputs" and "outputs" are transposed.
c        If "ncoeff_i" is zero, a NULL pointer may be supplied for "coeff_i".
c     options
f     OPTIONS = CHARACTER * ( * ) (Given)
c        Pointer to a null-terminated string containing an optional
c        comma-separated list of attribute assignments to be used for
c        initialising the new PolyMap. The syntax used is identical to
c        that for the astSet function and may include "printf" format
c        specifiers identified by "%" symbols in the normal way.
f        A character string containing an optional comma-separated
f        list of attribute assignments to be used for initialising the
f        new PolyMap. The syntax used is identical to that for the
f        AST_SET routine.
c     ...
c        If the "options" string contains "%" format specifiers, then
c        an optional list of additional arguments may follow it in
c        order to supply values to be substituted for these
c        specifiers. The rules for supplying these are identical to
c        those for the astSet function (and for the C "printf"
c        function).
f     STATUS = INTEGER (Given and Returned)
f        The global status.

*  Returned Value:
c     astPolyMap()
f     AST_POLYMAP = INTEGER
*        A pointer to the new PolyMap.

*  Notes:
*     - A null Object pointer (AST__NULL) will be returned if this
c     function is invoked with the AST error status set, or if it
f     function is invoked with STATUS set to an error value, or if it
*     should fail for any reason.
*--
*/

/* Local Variables: */
   astDECLARE_GLOBALS          /* Pointer to thread-specific global data */
   AstPolyMap *new;            /* Pointer to new PolyMap */
   va_list args;               /* Variable argument list */

/* Check the global status. */
   if ( !astOK ) return NULL;

/* Get a pointer to the thread specific global data structure. */
   astGET_GLOBALS(NULL);

/* Initialise the PolyMap, allocating memory and initialising the
   virtual function table as well if necessary. */
   new = astInitPolyMap( NULL, sizeof( AstPolyMap ), !class_init,
                         &class_vtab, "PolyMap", nin, nout,
                         ncoeff_f, coeff_f, ncoeff_i, coeff_i );

/* If successful, note that the virtual function table has been
   initialised. */
   if ( astOK ) {
      class_init = 1;

/* Obtain the variable argument list and pass it along with the options string
   to the astVSet method to initialise the new PolyMap's attributes. */
      va_start( args, status );
      astVSet( new, options, NULL, args );
      va_end( args );

/* If an error occurred, clean up by deleting the new object. */
      if ( !astOK ) new = astDelete( new );
   }

/* Return a pointer to the new PolyMap. */
   return new;
}

AstPolyMap *astPolyMapId_( int nin, int nout, int ncoeff_f, const double coeff_f[],
                           int ncoeff_i, const double coeff_i[], const char *options, ... ){
/*
*  Name:
*     astPolyMapId_

*  Purpose:
*     Create a PolyMap.

*  Type:
*     Private function.

*  Synopsis:
*     #include "polymap.h"
*     AstPolyMap *astPolyMap( int nin, int nout, int ncoeff_f, const double coeff_f[],
*                             int ncoeff_i, const double coeff_i[],
*                             const char *options, ... )

*  Class Membership:
*     PolyMap constructor.

*  Description:
*     This function implements the external (public) interface to the
*     astPolyMap constructor function. It returns an ID value (instead
*     of a true C pointer) to external users, and must be provided
*     because astPolyMap_ has a variable argument list which cannot be
*     encapsulated in a macro (where this conversion would otherwise
*     occur).
*
*     The variable argument list also prevents this function from
*     invoking astPolyMap_ directly, so it must be a re-implementation
*     of it in all respects, except for the final conversion of the
*     result to an ID value.

*  Parameters:
*     As for astPolyMap_.

*  Returned Value:
*     The ID value associated with the new PolyMap.
*/

/* Local Variables: */
   astDECLARE_GLOBALS            /* Pointer to thread-specific global data */
   AstPolyMap *new;              /* Pointer to new PolyMap */
   va_list args;                 /* Variable argument list */
   int *status;                  /* Pointer to inherited status value */

/* Get a pointer to the inherited status value. */
   status = astGetStatusPtr;

/* Get a pointer to the thread specific global data structure. */
   astGET_GLOBALS(NULL);

/* Check the global status. */
   if ( !astOK ) return NULL;

/* Initialise the PolyMap, allocating memory and initialising the
   virtual function table as well if necessary. */
   new = astInitPolyMap( NULL, sizeof( AstPolyMap ), !class_init,
                         &class_vtab, "PolyMap", nin, nout,
                         ncoeff_f, coeff_f, ncoeff_i, coeff_i );

/* If successful, note that the virtual function table has been
   initialised. */
   if ( astOK ) {
      class_init = 1;

/* Obtain the variable argument list and pass it along with the options string
   to the astVSet method to initialise the new PolyMap's attributes. */
      va_start( args, options );
      astVSet( new, options, NULL, args );
      va_end( args );

/* If an error occurred, clean up by deleting the new object. */
      if ( !astOK ) new = astDelete( new );
   }

/* Return an ID value for the new PolyMap. */
   return astMakeId( new );
}

AstPolyMap *astInitPolyMap_( void *mem, size_t size, int init,
                             AstPolyMapVtab *vtab, const char *name,
                             int nin, int nout, int ncoeff_f, const double coeff_f[],
                             int ncoeff_i, const double coeff_i[], int *status ){
/*
*+
*  Name:
*     astInitPolyMap

*  Purpose:
*     Initialise a PolyMap.

*  Type:
*     Protected function.

*  Synopsis:
*     #include "polymap.h"
*     AstPolyMap *astInitPolyMap( void *mem, size_t size, int init,
*                                 AstPolyMapVtab *vtab, const char *name,
*                                 int nin, int nout, int ncoeff_f,
*                                 const double coeff_f[], int ncoeff_i,
*                                 const double coeff_i[] )

*  Class Membership:
*     PolyMap initialiser.

*  Description:
*     This function is provided for use by class implementations to initialise
*     a new PolyMap object. It allocates memory (if necessary) to accommodate
*     the PolyMap plus any additional data associated with the derived class.
*     It then initialises a PolyMap structure at the start of this memory. If
*     the "init" flag is set, it also initialises the contents of a virtual
*     function table for a PolyMap at the start of the memory passed via the
*     "vtab" parameter.

*  Parameters:
*     mem
*        A pointer to the memory in which the PolyMap is to be initialised.
*        This must be of sufficient size to accommodate the PolyMap data
*        (sizeof(PolyMap)) plus any data used by the derived class. If a value
*        of NULL is given, this function will allocate the memory itself using
*        the "size" parameter to determine its size.
*     size
*        The amount of memory used by the PolyMap (plus derived class data).
*        This will be used to allocate memory if a value of NULL is given for
*        the "mem" parameter. This value is also stored in the PolyMap
*        structure, so a valid value must be supplied even if not required for
*        allocating memory.
*     init
*        A logical flag indicating if the PolyMap's virtual function table is
*        to be initialised. If this value is non-zero, the virtual function
*        table will be initialised by this function.
*     vtab
*        Pointer to the start of the virtual function table to be associated
*        with the new PolyMap.
*     name
*        Pointer to a constant null-terminated character string which contains
*        the name of the class to which the new object belongs (it is this
*        pointer value that will subsequently be returned by the astGetClass
*        method).
*     nin
*        The number of input coordinate values per point. This is the
*        same as the number of columns in the matrix.
*     nout
*        The number of output coordinate values per point. This is the
*        same as the number of rows in the matrix.
*     ncoeff_f
*        The number of non-zero coefficients necessary to define the
*        forward transformation of the PolyMap. If zero is supplied, the
*        forward transformation will be undefined.
*     coeff_f
*        An array containing "ncoeff_f*( 2 + nin )" elements. Each group
*	 of "2 + nin" adjacent elements describe a single coefficient of
*	 the forward transformation. Within each such group, the first
*	 element is the coefficient value; the next element is the
*	 integer index of the PolyMap output which uses the coefficient
*	 within its defining polynomial (the first output has index 1);
*	 the remaining elements of the group give the integer powers to
*	 use with each input coordinate value (powers must not be
*	 negative)
*
*        For instance, if the PolyMap has 3 inputs and 2 outputs, each group
*        consisting of 5 elements, A groups such as "(1.2, 2.0, 1.0, 3.0, 0.0)"
*        describes a coefficient with value 1.2 which is used within the
*        definition of output 2. The output value is incremented by the
*        product of the coefficient value, the value of input coordinate
*        1 raised to the power 1, and the value of input coordinate 2 raised
*        to the power 3. Input coordinate 3 is not used since its power is
*        specified as zero. As another example, the group "(-1.0, 1.0,
*        0.0, 0.0, 0.0 )" describes adds a constant value -1.0 onto
*        output 1 (it is a constant value since the power for every input
*        axis is given as zero).
*
*        Each final output coordinate value is the sum of the "ncoeff_f" terms
*        described by the "ncoeff_f" groups within the supplied array.
*     ncoeff_i
*        The number of non-zero coefficients necessary to define the
*        inverse transformation of the PolyMap. If zero is supplied, the
*        inverse transformation will be undefined.
*     coeff_i
*        An array containing
*        "ncoeff_i*( 2 + nout )" elements. Each group of "2 + nout"
*        adjacent elements describe a single coefficient of the inverse
*        transformation, using the same schame as "coeff_f", except that
*        "inputs" and "outputs" are transposed.

*  Returned Value:
*     A pointer to the new PolyMap.

*  Notes:
*     -  A null pointer will be returned if this function is invoked with the
*     global error status set, or if it should fail for any reason.
*-
*/

/* Local Variables: */
   AstPolyMap *new;              /* Pointer to new PolyMap */

/* Check the global status. */
   if ( !astOK ) return NULL;

/* If necessary, initialise the virtual function table. */
   if ( init ) astInitPolyMapVtab( vtab, name );

/* Initialise a Mapping structure (the parent class) as the first component
   within the PolyMap structure, allocating memory if necessary. Specify that
   the Mapping should be defined in both the forward and inverse directions. */
   new = (AstPolyMap *) astInitMapping( mem, size, 0,
                                        (AstMappingVtab *) vtab, name,
                                        nin, nout, 1, 1 );
   if ( astOK ) {

/* Initialise the PolyMap data. */
/* ---------------------------- */

/* First initialise the pointers in case of errors. */
      new->ncoeff_f = NULL;
      new->power_f = NULL;
      new->coeff_f = NULL;
      new->mxpow_f = NULL;

      new->ncoeff_i = NULL;
      new->power_i = NULL;
      new->coeff_i = NULL;
      new->mxpow_i = NULL;

/* Store the forward transformation. */
      StoreArrays( new, 1, ncoeff_f, coeff_f, status );

/* Store the inverse transformation. */
      StoreArrays( new, 0, ncoeff_i, coeff_i, status );

/* If an error occurred, clean up by deleting the new PolyMap. */
      if ( !astOK ) new = astDelete( new );
   }

/* Return a pointer to the new PolyMap. */
   return new;
}

AstPolyMap *astLoadPolyMap_( void *mem, size_t size,
                             AstPolyMapVtab *vtab, const char *name,
                             AstChannel *channel, int *status ) {
/*
*+
*  Name:
*     astLoadPolyMap

*  Purpose:
*     Load a PolyMap.

*  Type:
*     Protected function.

*  Synopsis:
*     #include "polymap.h"
*     AstPolyMap *astLoadPolyMap( void *mem, size_t size,
*                                 AstPolyMapVtab *vtab, const char *name,
*                                 AstChannel *channel )

*  Class Membership:
*     PolyMap loader.

*  Description:
*     This function is provided to load a new PolyMap using data read
*     from a Channel. It first loads the data used by the parent class
*     (which allocates memory if necessary) and then initialises a
*     PolyMap structure in this memory, using data read from the input
*     Channel.
*
*     If the "init" flag is set, it also initialises the contents of a
*     virtual function table for a PolyMap at the start of the memory
*     passed via the "vtab" parameter.


*  Parameters:
*     mem
*        A pointer to the memory into which the PolyMap is to be
*        loaded.  This must be of sufficient size to accommodate the
*        PolyMap data (sizeof(PolyMap)) plus any data used by derived
*        classes. If a value of NULL is given, this function will
*        allocate the memory itself using the "size" parameter to
*        determine its size.
*     size
*        The amount of memory used by the PolyMap (plus derived class
*        data).  This will be used to allocate memory if a value of
*        NULL is given for the "mem" parameter. This value is also
*        stored in the PolyMap structure, so a valid value must be
*        supplied even if not required for allocating memory.
*
*        If the "vtab" parameter is NULL, the "size" value is ignored
*        and sizeof(AstPolyMap) is used instead.
*     vtab
*        Pointer to the start of the virtual function table to be
*        associated with the new PolyMap. If this is NULL, a pointer
*        to the (static) virtual function table for the PolyMap class
*        is used instead.
*     name
*        Pointer to a constant null-terminated character string which
*        contains the name of the class to which the new object
*        belongs (it is this pointer value that will subsequently be
*        returned by the astGetClass method).
*
*        If the "vtab" parameter is NULL, the "name" value is ignored
*        and a pointer to the string "PolyMap" is used instead.

*  Returned Value:
*     A pointer to the new PolyMap.

*  Notes:
*     - A null pointer will be returned if this function is invoked
*     with the global error status set, or if it should fail for any
*     reason.
*-
*/

#define KEY_LEN 50               /* Maximum length of a keyword */

   astDECLARE_GLOBALS            /* Pointer to thread-specific global data */
/* Local Variables: */
   AstPolyMap *new;              /* Pointer to the new PolyMap */
   char buff[ KEY_LEN + 1 ];     /* Buffer for keyword string */
   int i;                        /* Loop index */
   int iv;                       /* Vectorised keyword index */
   int j;                        /* Loop index */
   int k;                        /* Loop index */
   int nin;                      /* No. of input coords */
   int nout;                     /* No. of output coords */
   int undef;                    /* Is the transformation undefined? */

/* Get a pointer to the thread specific global data structure. */
   astGET_GLOBALS(channel);

/* Initialise. */
   new = NULL;

/* Check the global error status. */
   if ( !astOK ) return new;

/* If a NULL virtual function table has been supplied, then this is
   the first loader to be invoked for this PolyMap. In this case the
   PolyMap belongs to this class, so supply appropriate values to be
   passed to the parent class loader (and its parent, etc.). */
   if ( !vtab ) {
      size = sizeof( AstPolyMap );
      vtab = &class_vtab;
      name = "PolyMap";

/* If required, initialise the virtual function table for this class. */
      if ( !class_init ) {
         astInitPolyMapVtab( vtab, name );
         class_init = 1;
      }
   }

/* Invoke the parent class loader to load data for all the ancestral
   classes of the current one, returning a pointer to the resulting
   partly-built PolyMap. */
   new = astLoadMapping( mem, size, (AstMappingVtab *) vtab, name,
                         channel );

   if ( astOK ) {

/* Get the number of inputs and outputs for the uninverted Mapping. */
   nin = ( (AstMapping *) new )->nin;
   nout = ( (AstMapping *) new )->nout;

/* Read input data. */
/* ================ */
/* Request the input Channel to read all the input data appropriate to
   this class into the internal "values list". */
      astReadClassData( channel, "PolyMap" );

/* Allocate memory to hold the forward arrays. */
      new->ncoeff_f = astMalloc( sizeof( int )*(size_t) nout );
      new->mxpow_f = astMalloc( sizeof( int )*(size_t) nin );
      new->power_f = astMalloc( sizeof( int ** )*(size_t) nout );
      new->coeff_f = astMalloc( sizeof( double * )*(size_t) nout );
      if( astOK ) {

/* Assume the forward transformation is defined. */
         undef = 0;

/* Get the maximum power of each input axis value used by the forward
   transformation. Set a flag "undef" if no values relating to the
   forward transformation are found (this indicates that the forward
   transformation is not defined). */
         for( i = 0; i < nin && !undef; i++ ){
            (void) sprintf( buff, "mpf%d", i + 1 );
            (new->mxpow_f)[ i ] = astReadInt( channel, buff, INT_MAX );
            if( (new->mxpow_f)[ i ] == INT_MAX ) undef = 1;
         }

/* Get the number of coefficients associated with each output of the forward
   transformation. */
         for( i = 0; i < nout && !undef; i++ ){
            (void) sprintf( buff, "ncf%d", i + 1 );
            (new->ncoeff_f)[ i ] = astReadInt( channel, buff, INT_MAX );
            if( (new->ncoeff_f)[ i ] == INT_MAX ) undef = 1;
         }

/* Get the coefficient values used by the forward transformation. This
   uses new style vectorised key names if available. Otherwise it uses
   old style indexed names (which were superceded by vectorised names
   because they are shorter and so work better with FitsChans). */
         iv = 0;
         for( i = 0; i < nout && !undef; i++ ){

            (new->coeff_f)[ i ] = astMalloc( sizeof( double )*
                                           (size_t) new->ncoeff_f[ i ] );
            if( astOK ) {
               for( j = 0; j < new->ncoeff_f[ i ]; j++ ){
                  (void) sprintf( buff, "cf%d", ++iv );
                  (new->coeff_f)[ i ][ j ] = astReadDouble( channel, buff, AST__BAD );
                  if( (new->coeff_f)[ i ][ j ] == AST__BAD ) {
                     (void) sprintf( buff, "cf%d_%d", i + 1, j + 1 );
                     (new->coeff_f)[ i ][ j ] = astReadDouble( channel, buff, AST__BAD );
                  }
               }
            }
         }

/* Get the input axis powers associated with each coefficient of the forward
   transformation. */
         iv = 0;
         for( i = 0; i < nout && !undef; i++ ){
            (new->power_f)[ i ] = astMalloc( sizeof( int * )*
                                             (size_t) new->ncoeff_f[ i ] );
            if( astOK ) {
               for( j = 0; j < new->ncoeff_f[ i ]; j++ ){
                  (new->power_f)[ i ][ j ] = astMalloc( sizeof( int )* (size_t) nin );
                  if( astOK ) {
                     for( k = 0; k < nin; k++ ){
                        (void) sprintf( buff, "pf%d", ++iv );
                        (new->power_f)[ i ][ j ][ k ] = astReadInt( channel, buff, 0 );
                        if( (new->power_f)[ i ][ j ][ k ] == 0 ) {
                           (void) sprintf( buff, "pf%d_%d_%d", i + 1, j + 1, k + 1 );
                           (new->power_f)[ i ][ j ][ k ] = astReadInt( channel, buff, 0 );
                        }
                     }
                  }
               }
            }
         }

/* Free the arrays if the forward transformation is undefined. */
         if( undef ) {
            new->ncoeff_f = astFree( new->ncoeff_f );
            new->mxpow_f = astFree( new->mxpow_f );
            new->power_f = astFree( new->power_f );
            new->coeff_f = astFree( new->coeff_f );
         }
      }

/* Allocate memory to hold the inverse arrays. */
      new->ncoeff_i = astMalloc( sizeof( int )*(size_t) nin );
      new->mxpow_i = astMalloc( sizeof( int )*(size_t) nout );
      new->power_i = astMalloc( sizeof( int ** )*(size_t) nin );
      new->coeff_i = astMalloc( sizeof( double * )*(size_t) nin );
      if( astOK ) {

/* Assume the inverse transformation is defined. */
         undef = 0;

/* Get the maximum power of each output axis value used by the inverse
   transformation. Set a flag "undef" if no values relating to the
   inverse transformation are found (this indicates that the inverse
   transformation is not defined). */
         for( i = 0; i < nout && !undef; i++ ){
            (void) sprintf( buff, "mpi%d", i + 1 );
            (new->mxpow_i)[ i ] = astReadInt( channel, buff, INT_MAX );
            if( (new->mxpow_i)[ i ] == INT_MAX ) undef = 1;
         }

/* Get the number of coefficients associated with each input of the inverse
   transformation. */
         for( i = 0; i < nin && !undef; i++ ){
            (void) sprintf( buff, "nci%d", i + 1 );
            (new->ncoeff_i)[ i ] = astReadInt( channel, buff, INT_MAX );
            if( (new->ncoeff_i)[ i ] == INT_MAX ) undef = 1;
         }

/* Get the coefficient values used by the inverse transformation. */
         iv = 0;
         for( i = 0; i < nin && !undef; i++ ){

            (new->coeff_i)[ i ] = astMalloc( sizeof( double )*
                                           (size_t) new->ncoeff_i[ i ] );
            if( astOK ) {
               for( j = 0; j < new->ncoeff_i[ i ]; j++ ){
                  (void) sprintf( buff, "ci%d", ++iv );
                  (new->coeff_i)[ i ][ j ] = astReadDouble( channel, buff, AST__BAD );
                  if( (new->coeff_i)[ i ][ j ] == AST__BAD ) {
                     (void) sprintf( buff, "ci%d_%d", i + 1, j + 1 );
                     (new->coeff_i)[ i ][ j ] = astReadDouble( channel, buff, AST__BAD );
                  }
               }
            }
         }

/* Get the output axis powers associated with each coefficient of the inverse
   transformation. */
         iv = 0;
         for( i = 0; i < nin && !undef; i++ ){
            (new->power_i)[ i ] = astMalloc( sizeof( int * )*
                                             (size_t) new->ncoeff_i[ i ] );
            if( astOK ) {
               for( j = 0; j < new->ncoeff_i[ i ]; j++ ){
                  (new->power_i)[ i ][ j ] = astMalloc( sizeof( int )* (size_t) nout );
                  if( astOK ) {
                     for( k = 0; k < nout; k++ ){
                        (void) sprintf( buff, "pi%d", ++iv );
                        (new->power_i)[ i ][ j ][ k ] = astReadInt( channel, buff, 0 );
                        if( (new->power_i)[ i ][ j ][ k ] == 0 ) {
                           (void) sprintf( buff, "pi%d_%d_%d", i + 1, j + 1, k + 1 );
                           (new->power_i)[ i ][ j ][ k ] = astReadInt( channel, buff, 0 );
                        }
                     }
                  }
               }
            }
         }

/* Free the arrays if the inverse transformation is undefined. */
         if( undef ) {
            new->ncoeff_i = astFree( new->ncoeff_i );
            new->mxpow_i = astFree( new->mxpow_i );
            new->power_i = astFree( new->power_i );
            new->coeff_i = astFree( new->coeff_i );
         }
      }

/* If an error occurred, clean up by deleting the new PolyMap. */
      if ( !astOK ) new = astDelete( new );
   }

/* Return the new PolyMap pointer. */
   return new;

/* Undefine macros local to this function. */
#undef KEY_LEN
}

/* Virtual function interfaces. */
/* ============================ */
/* These provide the external interface to the virtual functions defined by
   this class. Each simply checks the global error status and then locates and
   executes the appropriate member function, using the function pointer stored
   in the object's virtual function table (this pointer is located using the
   astMEMBER macro defined in "object.h").

   Note that the member function may not be the one defined here, as it may
   have been over-ridden by a derived class. However, it should still have the
   same interface. */

AstPolyMap *astPolyTran_( AstPolyMap *this, int forward, double acc,
                          double *lbnd, double *ubnd, int *status ){
   if ( !astOK ) return NULL;
   return (**astMEMBER(this,PolyMap,PolyTran))( this, forward, acc, lbnd,
                                                ubnd, status );
}

















static double **SamplePoly( AstPolyMap *this, int ndim, int forward,
                            double **table, double *lbnd, double *ubnd,
                            int npoint, int *nsamp, int *status ){
/*
*  Name:
*     SamplePoly

*  Purpose:
*     Create a table of input and output positions for a 2D PolMap.

*  Type:
*     Private function.

*  Synopsis:
*     double **SamplePoly( AstPolyMap *this, int ndim, int forward,
*                          double **table, double *lbnd, double *ubnd,
*                          int npoint, int *nsamp, int *status )

*  Description:
*     This function creates a table containing samples of the requested
*     polynomial transformation at a grid of input points. This grid covers
*     the user-supplied region, using "npoint" points on each axis. If the
*     PolyMap is 1D, then it will be treated as a 2D polynomial in which the
*     second output is a unit transformation.

*  Parameters:
*     this
*        The PolyMap.
*     ndim
*        The value the Nin and Nout attributes (which must be equal).
*        This must be either 1 or 2. If the PolyMap is 1-dimensional,
*        a unit trasformation is used for the second dimension.
*     forward
*        If non-zero, then the forward PolyMap transformation is sampled.
*        Otherwise the inverse transformation is sampled.
*     table
*        Pointer to a previous table created by this function, which is
*        to be re-used, or NULL.
*     lbnd
*        An array holding the lower bounds of a rectangular region within
*        the PolyMap's input space (if "forward" is non-zero) or output
*        space (if "forward" is zero). The new polynomial will be
*        evaluated over this rectangle. If "ndim" is 1, only the first
*        element is accessed, and limits of [-1,+1] are used.
*     ubnd
*        An array holding the upper bounds of a rectangular region within
*        the PolyMap's input space (if "forward" is non-zero) or output
*        space (if "forward" is zero). The new polynomial will be
*        evaluated over this rectangle. If "ndim" is 1, only the first
*        element is accessed, and limits of [-1,+1] are used.
*     npoint
*        The number of points along each edge of the grid.
*     nsamp
*        Address of an int in which to return the total number of samples
*        in the returned table.
*     status
*        Pointer to the inherited status variable.

*  Returned Value:
*        Pointer to an array of 4 pointers. Each of these pointers points
*        to an array of "nsamp" doubles, being the sampled values for y1,
*        y2, x1 or x2 in that order. Here (x1,x2) are the input values
*        for the sampled transformation (these are spaced on the regular
*        grid specified by lbnd, ubnd and npoint), and (y1,y2) are the
*        output positions produced by the sampled transformation. The
*        returned pointer should be freed using astFreeDouble when no longer
*        needed.

*/

/* Local Variables: */
   AstMapping *map;
   AstPointSet *ps1;
   AstPointSet *ps2;
   AstUnitMap *um;
   double **result;
   double *p0;
   double *p1;
   double *ptr1[ 2 ];
   double *ptr2[ 2 ];
   double delta1;
   double delta0;
   double lbnd0;
   double lbnd1;
   double ubnd0;
   double ubnd1;
   double val0;
   double val1;
   int i;
   int j;

/* Initialise returned value */
   result = table;
   *nsamp = 0;

/* Check inherited status */
   if( !astOK ) return result;

/* Ensure we have a table of the correct size. */
   *nsamp = npoint*npoint;
   if( !result ) result = astCalloc( 4, sizeof( double * ) );
   if( result ) {
      for( i = 0; i < 4; i++ ) {
         result[ i ] = astRealloc( result[ i ] , (*nsamp)*sizeof( double ) );
      }
   }

/* Store the bounds to use. */
   lbnd0 = lbnd[ 0 ];
   ubnd0 = ubnd[ 0 ];
   if( ndim == 1 ) {
      lbnd1 = -1.0;
      ubnd1 = 1.0;
   } else {
      lbnd1 = lbnd[ 1 ];
      ubnd1 = ubnd[ 1 ];
   }

/* Work out the step sizes for the grid. */
   delta0 = ( ubnd0 - lbnd0 )/( npoint - 1 );
   delta1 = ( ubnd1 - lbnd1 )/( npoint - 1 );

/* Create a PointSet to hold the grid of input positions. Use columns 2
   and 3 of the table to hold the PointSet values. */
   ps1 = astPointSet( *nsamp, 2, " ", status );
   ptr1[ 0 ] = result[ 2 ];
   ptr1[ 1 ] = result[ 3 ];
   astSetPoints( ps1, ptr1 );

/* Create a PointSet to hold the grid of output positions. Use columns 0
   and 1 of the table to hold the PointSet values. */
   ps2 = astPointSet( *nsamp, 2, " ", status );
   ptr2[ 0 ] = result[ 0 ];
   ptr2[ 1 ] = result[ 1 ];
   astSetPoints( ps2, ptr2 );
   if( astOK ) {

/* Calculate the grid of input positions and store in the PointSet and
   therefore also in the returned table. */
      val0 = lbnd0;
      p0 = ptr1[ 0 ];
      p1 = ptr1[ 1 ];
      for( i = 0; i < npoint; i++ ) {
         val1 = lbnd1;
         for( j = 0; j < npoint; j++ ) {
             *(p0++) = val0;
             *(p1++) = val1;
             val1 += delta1;
         }
         val0 += delta0;
      }

/* If the PolyMap is 1D, add in a second dimension that uses a unit map. */
      if( ndim == 1 ) {
         um = astUnitMap( 1, " ", status );
         map = (AstMapping *) astCmpMap( this, um, 0, " ", status );
         um = astAnnul( um );
      } else {
         map = astClone( this );
      }

/* Transform the input grid to get the output grid. */
      (void) astTransform( map, ps1, forward, ps2 );

/* Free resources */
      map = astAnnul( map );
   }
   ps1 = astAnnul( ps1 );
   ps2 = astAnnul( ps2 );

/* If an error occurred, free the returned array. */
   if( !astOK ) result = astFreeDouble( result );

/* Return a pointer to the table. */
   return result;
}


