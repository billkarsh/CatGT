#ifndef PASS2_H
#define PASS2_H

#include "Tool.h"

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

typedef long double  BTYPE;

class Pass2
{
private:
    qint64                  samps;
    QString                 outBin;
    QFile                   fout;
    Meta                    meta;
    std::vector<BTYPE>      buf;
    static QMap<int,int>    ip1rep; // ip1 -> which ip2 is handling
    static QMutex           ip1Mtx;
    int                     ip1,
                            ip2,
                            ex0,
                            exLim;
    t_js                    js;
    bool                    miss_ok,
                            do_bin;

public:
    Pass2( t_js js, int ip2 );
    virtual ~Pass2()    {}

    int first();
    bool next( int ie );
    void close();

private:
    void set_ip1rep();
    bool is_ip1rep();
    void initDigitalFields();
    bool openDigitalFiles( int g0 );
    bool copyDigitalFiles( int ie );

    qint64 checkCounts( int ie );

    bool copyFile( QFile &fout, int ie, t_ex ex, XTR *X = 0 );
    bool copyFilesBF( int ie, BitField *B );
};

#endif  // PASS2_H


