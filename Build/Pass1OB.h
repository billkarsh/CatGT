#ifndef PASS1OB_H
#define PASS1OB_H

#include "Tool.h"

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

class Pass1OB : public IOClient
{
private:
    Pass1IO     io;
    QFileInfo   fim;
    Meta        meta;
    int         ip,
                ex0,
                exLim;

public:
    Pass1OB( int ip ) : io(*this, fim, meta), ip(ip)    {}
    virtual ~Pass1OB()                                  {}

    bool go();

    virtual void digital( const qint16 *data, int ntpts );
    virtual void neural( qint16 *data, int ntpts )      {}

private:
    void initDigitalFields();
    bool openDigitalFiles( int g0 );
};

#endif  // PASS1OB_H


