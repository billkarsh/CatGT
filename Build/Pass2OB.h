#ifndef PASS2OB_H
#define PASS2OB_H

#include "Tool.h"

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

class Pass2OB
{
private:
    qint64              samps;
    QString             outBin;
    QFile               fout;
    Meta                meta;
    std::vector<BTYPE>  &buf;
    int                 ip,
                        closedIP,
                        ex0,
                        exLim;

public:
    Pass2OB( std::vector<BTYPE> &buf ) : buf(buf), closedIP(-9) {}
    virtual ~Pass2OB()                                          {}

    bool first( int ip );
    bool next( int ie );
    void close();

private:
    void initDigitalFields();
    bool openDigitalFiles( int g0 );
    bool copyDigitalFiles( int ie );
};

#endif  // PASS2OB_H


