#ifndef PASS2NI_H
#define PASS2NI_H

#include "Tool.h"

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

class Pass2NI
{
private:
    qint64              samps;
    QString             outBin;
    QFile               fout;
    Meta                meta;
    std::vector<BTYPE>  &buf;
    bool                closed;

public:
    Pass2NI( std::vector<BTYPE> &buf ) : buf(buf), closed(false)    {}
    virtual ~Pass2NI()                                              {}

    bool first();
    bool next( int ie );
    void close();

private:
    void initDigitalFields();
    bool openDigitalFiles( int g0 );
    bool copyDigitalFiles( int ie );
};

#endif  // PASS2NI_H


