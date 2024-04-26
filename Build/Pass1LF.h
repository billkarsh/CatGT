#ifndef PASS1LF_H
#define PASS1LF_H

#include "Pass1.h"

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

class Pass1LF : public Pass1
{
public:
    Pass1LF( int ip ) : Pass1( LF, LF, ip ) {}
    virtual ~Pass1LF()                      {}

    bool go();

    virtual void digital( const qint16 *data, int ntpts )   {}

private:
    bool filtersAndScaling();
};

#endif  // PASS1LF_H


