#ifndef PASS1AP_H
#define PASS1AP_H

#include "Pass1.h"
#include "CAR.h"

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

class Pass1AP : public Pass1
{
private:
    double          Tmul;
    GeomMap         *geomMap;
    ShankMap        *shankMap;
    Biquad          *hp_gfix;
    CAR             car;
    vec_i16         gfixbuf;
    QMap<qint64,LR> TLR;        // gfix reports
    QVector<int>    ig2ic,      // saved to acquired AP
                    ic2ig;      // acq AP to saved or -1

public:
    Pass1AP( int ip )
        :   Pass1( AP, AP, ip ), geomMap(0), shankMap(0), hp_gfix(0)    {}
    virtual ~Pass1AP();

    bool go();

    virtual void neural( qint16 *data, int ntpts );

private:
    bool filtersAndScaling();
    void gfixEdits();
    void gfixZeros( qint64 L, int N );
    void gfixZero1(
        QFile   *o_f,
        qint64  L,
        int     N,
        int     smpBytes,
        int     nC,
        int     nAP );
    void gFixDetect(
        QMap<qint64,LR> &TLR,
        const qint16    *d,
        qint16          *B,
        qint64          t0,
        double          Tmul,
        int             ntpts,
        int             nC,
        int             nAP,
        int             maxInt,
        int             stride );
};

#endif  // PASS1AP_H


