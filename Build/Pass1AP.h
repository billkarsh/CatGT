#ifndef PASS1AP_H
#define PASS1AP_H

#include "Pass1.h"

struct LR;
struct GeomMap;
struct ShankMap;

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

struct SUElem {
// The common parts of GeomMap & ShankMap.
// This device allows unification of code whichever map is loaded.
    qint16 s, u;
    SUElem( qint16 s, qint16 u ) : s(s), u(u)   {}
};

typedef std::vector<SUElem> SUList;

struct MedCAR {
private:
    std::vector<int>    idx;
    vec_i16             arrange;
    vec_i16::iterator   ibeg, imid, iend;
    int                 nC,
                        nU;
public:
    MedCAR()            {}
    virtual ~MedCAR()   {}
    void init( const SUList &SU, int nC, int nAP );
    void apply( qint16 *d, int ntpts );
};

class Pass1AP : public Pass1
{
private:
    double              Tmul;
    GeomMap             *geomMap;
    ShankMap            *shankMap;
    Biquad              *hp_gfix;
    MedCAR              medCAR;
    vec_i16             gfixbuf;
    QMap<qint64,LR>     TLR;                // gfix reports
    vec_i16             loccarBuf;          // local CAR workspace
    QVector<int>        ig2ic,              // saved to acquired AP
                        ic2ig;              // acq AP to saved or -1
    std::vector<std::vector<int> >  TSM;
    SUList              SU;
    std::vector<int>    muxTbl;
    int                 nADC,
                        nGrp;

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
    void sAveTable_geomMap( int nAP );
    void sAveTable_shankMap( int nAP );
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


