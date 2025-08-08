#ifndef PASS2_H
#define PASS2_H

#include "Tool.h"

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

class Pass2
{
private:
    qint64              samps;
    QString             outBin;
    QFile               fout;
    Meta                meta;
    std::vector<BTYPE>  &buf;
    QSet<int>           ip1_seen;
    int                 ip1,
                        ip2,
                        closedIP,
                        ex0,
                        exLim;
    t_js                js;
    bool                miss_ok,
                        do_bin;

public:
    Pass2( std::vector<BTYPE> &buf, t_js js );
    virtual ~Pass2()    {}

    int first( int ip2 );
    bool next( int ie );
    void close();

private:
    void initDigitalFields();
    bool openDigitalFiles( int g0 );
    bool copyDigitalFiles( int ie );

    qint64 checkCounts( int ie );

    bool copyFile( QFile &fout, int ie, t_ex ex, XTR *X = 0 );
    bool copyFilesBF( int ie, BitField *B );
};

#endif  // PASS2_H


