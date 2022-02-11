#ifndef PASS2AP_H
#define PASS2AP_H

#include "Tool.h"

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

class Pass2AP
{
private:
    qint64              samps;
    QString             outBin;
    QFile               fout;
    Meta                meta;
    std::vector<BTYPE>  &buf;
    int                 ip,
                        closedIP;

public:
    Pass2AP( std::vector<BTYPE> &buf ) : buf(buf), closedIP(-9) {}
    virtual ~Pass2AP()                                          {}

    int first( int ip );
    bool next( int ie );
    void close();

private:
    void initDigitalFields();
    bool openDigitalFiles( int g0 );
    bool copyDigitalFiles( int ie );
};

#endif  // PASS2AP_H


