#ifndef PASS1LF_H
#define PASS1LF_H

#include "Tool.h"

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

class Pass1LF : public IOClient
{
private:
    Pass1IO     io;
    QFileInfo   fim;
    Meta        meta;
    int         ip;

public:
    Pass1LF( int ip ) : io(*this, fim, meta), ip(ip)    {}
    virtual ~Pass1LF()                                  {}

    bool go();

    virtual void digital( const qint16 *data, int ntpts );
    virtual void neural( qint16 *data, int ntpts );

private:
    void filtersAndScaling();
};

#endif  // PASS1LF_H


