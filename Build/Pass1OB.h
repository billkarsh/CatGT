#ifndef PASS1OB_H
#define PASS1OB_H

#include "Pass1.h"

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

class Pass1OB : public Pass1
{
public:
    Pass1OB( int ip ) : Pass1( OB, OB, ip ) {}
    virtual ~Pass1OB()                      {}

    bool go();
};

#endif  // PASS1OB_H


