#ifndef PASS1NI_H
#define PASS1NI_H

#include "Pass1.h"

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

class Pass1NI : public Pass1
{
public:
    Pass1NI() : Pass1( NI, NI, 0 )  {}
    virtual ~Pass1NI()              {}

    bool go();
};

#endif  // PASS1NI_H


