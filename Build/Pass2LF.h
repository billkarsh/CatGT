#ifndef PASS2LF_H
#define PASS2LF_H

#include "Tool.h"

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

class Pass2LF
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
    Pass2LF( std::vector<BTYPE> &buf ) : buf(buf), closedIP(-9) {}
    virtual ~Pass2LF()                                          {}

    int first( int ip );
    bool next( int ie );
    void close();
};

#endif  // PASS2LF_H


