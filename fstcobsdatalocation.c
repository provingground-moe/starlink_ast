/*
*+
*  Name:
*     fstcobsdatalocation.c

*  Purpose:
*     Define a FORTRAN 77 interface to the AST StcObsDataLocation class.

*  Type of Module:
*     C source file.

*  Description:
*     This file defines FORTRAN 77-callable C functions which provide
*     a public FORTRAN 77 interface to the StcObsDataLocation class.

*  Routines Defined:
*     AST_ISASTCOBSDATALOCATION
*     AST_STCOBSDATALOCATION

*  Copyright:
*     <COPYRIGHT_STATEMENT>

*  Authors:
*     DSB: David S. Berry (Starlink)

*  History:
*     22-NOV-2004 (DSB):
*        Original version.
*/

/* Define the astFORTRAN77 macro which prevents error messages from
   AST C functions from reporting the file and line number where the
   error occurred (since these would refer to this file, they would
   not be useful). */
#define astFORTRAN77

/* Header files. */
/* ============= */
#include "f77.h"                 /* FORTRAN <-> C interface macros (SUN/209) */
#include "c2f77.h"               /* F77 <-> C support functions/macros */
#include "error.h"               /* Error reporting facilities */
#include "memory.h"              /* Memory handling facilities */
#include "stcobsdatalocation.h" /* C interface to the StcObsDataLocation class */


F77_LOGICAL_FUNCTION(ast_isastcobsdatalocation)( INTEGER(THIS), INTEGER(STATUS) ) {
   GENPTR_INTEGER(THIS)
   F77_LOGICAL_TYPE(RESULT);

   astAt( "AST_ISASTCOBSDATALOCATION", NULL, 0 );
   astWatchSTATUS(
      RESULT = astIsAStcObsDataLocation( astI2P( *THIS ) ) ? F77_TRUE : F77_FALSE;
   )
   return RESULT;
}

F77_INTEGER_FUNCTION(ast_stcobsdatalocation)( INTEGER(REG),
                               INTEGER(NCOORDS),
                               INTEGER_ARRAY(COORDS),
                               CHARACTER(OPTIONS),
                               INTEGER(STATUS)
                               TRAIL(OPTIONS) ) {
   GENPTR_INTEGER(REG)
   GENPTR_INTEGER(NCOORDS)
   GENPTR_CHARACTER(OPTIONS)
   GENPTR_INTEGER_ARRAY(COORDS)
   AstKeyMap **coords;
   F77_INTEGER_TYPE(RESULT);
   char *options;
   int i;

   astAt( "AST_STCOBSDATALOCATION", NULL, 0 );
   astWatchSTATUS(
      options = astString( OPTIONS, OPTIONS_length );

/* Change ',' to '\n' (see AST_SET in fobject.c for why). */
      if ( astOK ) {
         for ( i = 0; options[ i ]; i++ ) {
            if ( options[ i ] == ',' ) options[ i ] = '\n';
         }
      }

/* Convert supplied integers to pointers. */
      coords = astMalloc( sizeof( AstKeyMap * )*(size_t)( *NCOORDS ));
      if( astOK ) {
         for( i = 0; i < *NCOORDS; i++ ) {
            coords[ i ] = (AstKeyMap *) astMakePointer( astI2P( COORDS[ i ] ));
         }
      }

      RESULT = astP2I( astStcObsDataLocation( astI2P( *REG ), *NCOORDS, 
                                              coords, "%s", options ) );
      astFree( coords );
      astFree( options );
   )
   return RESULT;
}