#ifndef Pass1AP2LF_H
#define Pass1AP2LF_H

#include "Tool.h"

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

struct Dwn1IO : public Pass1IO {
    int     offset;
    Dwn1IO( IOClient &client, Meta &meta )
    :   Pass1IO(client, meta), offset(0)    {}
    virtual qint64 _write( qint64 bytes );
    virtual bool zero( qint64 gapBytes, qint64 zfBytes );
};

class Pass1AP2LF : public IOClient
{
private:
    Dwn1IO  io;
    Meta    meta;
    int     ip;

public:
    Pass1AP2LF( int ip ) : io(*this, meta), ip(ip)  {}
    virtual ~Pass1AP2LF()                           {}

    bool go();

    virtual void digital( const qint16 *data, int ntpts );
    virtual void neural( qint16 *data, int ntpts );

private:
    void filtersAndScaling();
    void adjustMeta();
};

#endif  // Pass1AP2LF_H


