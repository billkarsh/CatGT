#ifndef PASS1AP_H
#define PASS1AP_H

#include "Pass1.h"

struct LR;
struct ShankMap;

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

class Pass1AP : public Pass1
{
private:
    double              Tmul;
    ShankMap            *shankMap;
    Biquad              *hp_gfix;
    std::vector<qint16> gfixbuf;
    QMap<qint64,LR>     TLR;                // gfix reports
    std::vector<qint16> loccarBuf;          // local CAR workspace
    QVector<int>        ig2ic,              // saved to acquired
                        ic2ig;              // acq to saved or -1
    std::vector<std::vector<int> >  TSM;
    std::vector<int>    muxTbl;
    int                 nADC,
                        nGrp;

public:
    Pass1AP( int ip ) : Pass1( AP, AP, ip ), shankMap(0), hp_gfix(0)    {}
    virtual ~Pass1AP();

    bool go();

    virtual void neural( qint16 *data, int ntpts );

private:
    bool filtersAndScaling();
    void gfixEdits();
    void gfixZeros( qint64 L, int N );
    void sAveTable( int nAP );
    void sAveApplyLocal(
        qint16  *d,
        qint16  *tmp,
        int     ntpts,
        int     nC,
        int     nAP );
    void sAveApplyGlobal(
        qint16  *d,
        int     ntpts,
        int     nC,
        int     nAP,
        int     dwnSmp );
    void sAveApplyDmxStride(
        qint16  *d,
        int     ntpts,
        int     nC,
        int     nAP,
        int     stride,
        int     dwnSmp );
    void sAveApplyDmxTbl(
        qint16  *d,
        int     ntpts,
        int     nC,
        int     nAP,
        int     dwnSmp );
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


