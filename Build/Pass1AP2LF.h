#ifndef Pass1AP2LF_H
#define Pass1AP2LF_H

#include "Pass1.h"

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

class Pass1AP2LF : public Pass1
{
private:
    int offset;

public:
    Pass1AP2LF( int ip ) : Pass1( AP, LF, ip ), offset(0)   {}
    virtual ~Pass1AP2LF()                                   {}

    bool go();

    virtual void digital( const qint16 *data, int ntpts )   {}
    virtual bool _write( qint64 bytes );
    virtual bool zero( qint64 gapBytes, qint64 zfBytes );

private:
    bool filtersAndScaling();
    void adjustMeta();
};

#endif  // Pass1AP2LF_H


