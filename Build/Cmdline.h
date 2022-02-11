#ifndef CMDLINE_H
#define CMDLINE_H

#include <vector>

/* --------------------------------------------------------------- */
/* Functions ----------------------------------------------------- */
/* --------------------------------------------------------------- */

bool IsArg( const char *pat, const char *argv );
bool GetArg( void *v, const char *pat, const char *argv );
bool GetArgStr( const char* &s, const char *pat, char *argv );
bool GetArgList( std::vector<int> &v, const char *pat, char *argv );
bool GetArgList( std::vector<double> &v, const char *pat, char *argv );

#endif  // CMDLINE_H


