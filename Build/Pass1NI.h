#ifndef PASS1NI_H
#define PASS1NI_H

#include "Tool.h"

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

class Pass1NI : public IOClient
{
private:
    Pass1IO io;
    Meta    meta;

public:
    Pass1NI() : io(*this, meta) {}
    virtual ~Pass1NI()          {}

    bool go();

    virtual void digital( const qint16 *data, int ntpts );
    virtual void neural( qint16 *data, int ntpts );

private:
    void initDigitalFields();
    bool openDigitalFiles( int g0 );
};

#endif  // PASS1NI_H


