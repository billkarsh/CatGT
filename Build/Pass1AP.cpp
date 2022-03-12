
#include "Pass1AP.h"
#include "Util.h"
#include "Biquad.h"
#include "ShankMap.h"

#define MAX10BIT    512
#define GFIXOFF     128+32
#define GFIXBUFSMP  (GFIXOFF+192)




Pass1AP::~Pass1AP()
{
    if( shankMap ) delete shankMap;
    if( hp_gfix ) delete hp_gfix;
}


bool Pass1AP::go()
{
    int t0, g0 = GBL.gt_get_first( &t0 );

    doWrite = GBL.gt_nIndices() > 1
                    || GBL.apflt.isenabled() || GBL.tshift
                    || GBL.locout || GBL.gblcar || GBL.gfixdo;

    switch( openInputMeta( fim, meta.kvp, g0, t0, AP, ip, GBL.prb_miss_ok ) ) {
        case 0: break;
        case 1: return true;
        case 2: return false;
    }

    if( !GBL.makeOutputProbeFolder( g0, ip ) )
        return false;

    if( !o_open( g0 ) )
        return false;

    meta.read( AP, ip );

    if( !filtersAndScaling() )
        return false;

    initDigitalFields( 0.0001 );

    if( !openDigitalFiles( g0 ) )
        return false;

    gFOff.init( meta.srate, AP, ip );

    alloc();

    if( GBL.gfixdo )
        gfixbuf.resize( GFIXBUFSMP * meta.nC );

    fileLoop();

    if( shankMap )
        meta.kvp["~snsShankMap"] = shankMap->toString();

    meta.write( o_name, g0, t0, AP, ip );

    gfixEdits();

    return true;
}


void Pass1AP::neural( qint16 *data, int ntpts )
{
// Gbl detect

    if( GBL.gfixdo ) {
        gFixDetect(
            TLR, data, &gfixbuf[0], meta.smpOutSpan(),
            Tmul, ntpts, meta.nC, meta.nN, maxInt, -1 );
    }

// Loccar

    if( GBL.locout )
        sAveApplyLocal( data, &loccarBuf[0], ntpts, meta.nC, meta.nN );

// Gblcar

    if( GBL.gblcar )
        sAveApplyGlobal( data, ntpts, meta.nC, meta.nN, 1 );
}


bool Pass1AP::filtersAndScaling()
{
// -------
// Filters
// -------

    if( hp_gfix ) {
        delete hp_gfix;
        hp_gfix = 0;
    }

    if( GBL.gfixdo && !GBL.apflt.hasbiquadhp() )
        hp_gfix = new Biquad( bq_type_highpass, 300 / meta.srate );

// ----
// Imro
// ----

// 3A default

    Tmul    = 512.0 * 500.0 / (1000.0 * 0.6);
    maxInt  = MAX10BIT;

// Get actual

    IMROTbl *R = getProbe( meta.kvp );

    if( R ) {
        R->fromString( 0, meta.kvp["~imroTbl"].toString() );
        maxInt  = R->maxInt();
        Tmul    = maxInt * R->apGain( 0 )
                    / (1000 * meta.kvp["imAiRangeMax"].toDouble());
        R->muxTable( nADC, nGrp, muxTbl );
        delete R;
    }
    else {
        Log() << QString("Can't identify probe type in metadata '%1'.")
                    .arg( fim.fileName() );
        return false;
    }

// ---------------
// Channel mapping
// ---------------

    if( GBL.locout || GBL.gblcar || GBL.gfixdo ) {

        // ---------------------
        // Saved channel ID list
        // ---------------------

        QVector<uint>   chanIds;

        if( !getSavedChannels( chanIds, meta.kvp, fim ) )
            return false;

        // ---------------------------------------------
        // Graph (saved channel) to acq channel mappings
        // ---------------------------------------------

        const QStringList   sl = meta.kvp["acqApLfSy"].toString().split(
                                    QRegExp("^\\s+|\\s*,\\s*"),
                                    QString::SkipEmptyParts );
        int nAcqChan = sl[0].toInt() + sl[1].toInt() + sl[2].toInt();

        ig2ic.resize( meta.nC );
        ic2ig.fill( -1, qMax( nAcqChan, nADC * nGrp ) );

        for( int ig = 0; ig < meta.nC; ++ig ) {

            int &C = ig2ic[ig];

            C           = chanIds[ig];
            ic2ig[C]    = ig;
        }

        // --------
        // ShankMap
        // --------

        if( shankMap )
            delete shankMap;

        shankMap = new ShankMap;

        KVParams::const_iterator    it_kvp = meta.kvp.find( "~snsShankMap" );

        if( it_kvp != meta.kvp.end() )
            shankMap->fromString( it_kvp.value().toString() );
        else {
            Log() << QString("Missing ~snsShankMap tag '%1'.")
                        .arg( fim.fileName() );
            return false;
        }

        // -----------------
        // Excluded channels
        // -----------------

        QMap<int,QVector<uint>>::const_iterator  it_exc = GBL.mexc.find( ip );

        if( it_exc != GBL.mexc.end() ) {

            const QVector<uint> &C = it_exc.value();

            for( int ie = 0, ne = C.size(); ie < ne; ++ie ) {

                // IMPORTANT:
                // User chan label not necessarily within span of
                // true channels, hence, span of ic2ig[], so don't
                // use ic2ig for this lookup.

                int ig = ig2ic.indexOf( C[ie] );

                if( ig >= 0 )
                    shankMap->e[ig].u = 0;
            }
        }

        // ---
        // TSM
        // ---

        if( GBL.locout ) {
            sAveTable( meta.nN );
            loccarBuf.resize( meta.nC );
        }
    }

    return true;
}


void Pass1AP::gfixEdits()
{
    int g0 = GBL.gt_get_first( 0 );

    if( !TLR.size() ) {

        if( GBL.gfixdo ) {
            Log() << QString("Run %1_g%2 Gfix prb %3 edits/sec 0")
                        .arg( GBL.run ).arg( g0 ).arg( ip );
        }

        return;
    }

    QMap<qint64,LR>::iterator   it_fix  = TLR.begin(), end = TLR.end();
    qint64                      smpFLen = meta.smpOutSpan(),
                                nedit   = 0;
    int                         bufWid  = o_buf.size()/meta.nC - 1;

    for( ; it_fix != end; ++it_fix ) {

        qint16  *d;
        qint64  T0 = it_fix.key(),
                L  = T0 - it_fix.value().L,
                R  = T0 + it_fix.value().R;
        int     N;

        // consolidate close instances

        while( ++it_fix != end ) {

            qint64  TX = it_fix.key(),
                    LX = TX - it_fix.value().L,
                    RX = TX + it_fix.value().R;

            if( LX <= R ) {

                if( LX < L )
                    L = LX;

                if( RX > R )
                    R = RX;
            }
            else
                break;
        }

        --it_fix;

        L = qMin( L, T0 - 2 );
        R = qMax( R, T0 + 6 );

        L = qMax( L, qint64(0) );
        R = qMin( R, smpFLen - 1 ); // EOF limit

        d = &o_buf[0];
        N = qMin( R - L + 1, qint64(bufWid) );

        o_f.seek( meta.smpBytes * L );
        o_f.read( o_buf8(), meta.smpBytes * N );

        for( int it = 0; it < N; ++it, d += meta.nC )
            memset( d, 0, meta.nN*sizeof(qint16) );

        o_f.seek( meta.smpBytes * L );
        _write( meta.smpBytes * N );
        ++nedit;
    }

    Log() << QString("Run %1_g%2 Gfix prb %3 edits/sec %4")
                .arg( GBL.run ).arg( g0 ).arg( ip )
                .arg( nedit * meta.srate / smpFLen, 0, 'g', 3 );
}


// For each channel [0,nAP), calculate an 8-way
// neighborhood of indices into a timepoint's channels.
// - Annulus with {inner, outer} radii {GBL.locin, GBL.locout}.
// - The list is sorted for cache friendliness.
//
void Pass1AP::sAveTable( int nAP )
{
    TSM.clear();
    TSM.resize( nAP );

    QMap<ShankMapDesc,uint> ISM;
    shankMap->inverseMap( ISM );

    int rIn  = GBL.locin,
        rOut = GBL.locout;

    for( int ig = 0; ig < nAP; ++ig ) {

        const ShankMapDesc  &E = shankMap->e[ig];

        if( !E.u )
            continue;

        // ----------------------------------
        // Form map of excluded inner indices
        // ----------------------------------

        QMap<int,int>   inner;  // keys sorted, value is arbitrary

        int xL  = qMax( int(E.c)  - rIn, 0 ),
            xH  = qMin( uint(E.c) + rIn + 1, shankMap->nc ),
            yL  = qMax( int(E.r)  - rIn, 0 ),
            yH  = qMin( uint(E.r) + rIn + 1, shankMap->nr );

        for( int ix = xL; ix < xH; ++ix ) {

            for( int iy = yL; iy < yH; ++iy ) {

                QMap<ShankMapDesc,uint>::iterator   it;

                it = ISM.find( ShankMapDesc( E.s, ix, iy, 1 ) );

                if( it != ISM.end() )
                    inner[it.value()] = 1;
            }
        }

        // -------------------------
        // Fill with annulus members
        // -------------------------

        std::vector<int>    &V = TSM[ig];

        xL  = qMax( int(E.c)  - rOut, 0 );
        xH  = qMin( uint(E.c) + rOut + 1, shankMap->nc );
        yL  = qMax( int(E.r)  - rOut, 0 );
        yH  = qMin( uint(E.r) + rOut + 1, shankMap->nr );

        for( int ix = xL; ix < xH; ++ix ) {

            for( int iy = yL; iy < yH; ++iy ) {

                QMap<ShankMapDesc,uint>::iterator   it;

                it = ISM.find( ShankMapDesc( E.s, ix, iy, 1 ) );

                if( it != ISM.end() ) {

                    int i = it.value();

                    // Exclude inners

                    if( inner.find( i ) == inner.end() )
                        V.push_back( i );
                }
            }
        }

        qSort( V );
    }
}


void Pass1AP::sAveApplyLocal(
    qint16  *d,
    qint16  *tmp,
    int     ntpts,
    int     nC,
    int     nAP )
{
    nAP = ig2ic[nAP-1];    // highest acquired channel saved

    for( int it = 0; it < ntpts; ++it, d += nC ) {

        memcpy( tmp, d, nC*sizeof(qint16) );

        for( int ic = 0; ic <= nAP; ++ic ) {

            int ig = ic2ig[ic];

            if( ig >= 0 ) {

                const std::vector<int>  &V = TSM[ig];

                int nv = V.size();

                if( nv ) {

                    const int   *v  = &V[0];
                    int         sum = 0;

                    for( int iv = 0; iv < nv; ++iv )
                        sum += d[v[iv]];

                    tmp[ig] = d[ig] - sum/nv;
                }
            }
        }

        memcpy( d, tmp, nC*sizeof(qint16) );
    }
}


// Space averaging for all values.
//
#if 0
// ----------------
// Per-shank method
// ----------------
void Pass1AP::sAveApplyGlobal(
    qint16  *d,
    int     ntpts,
    int     nC,
    int     nAP,
    int     dwnSmp )
{
    if( nAP <= 0 )
        return;

    const ShankMapDesc  *E = &shankMap->e[0];

    int                 ns      = shankMap->ns,
                        dStep   = nC * dwnSmp;
    std::vector<int>    _A( ns ),
                        _N( ns );
    std::vector<float>  _S( ns );
    int                 *A  = &_A[0],
                        *N  = &_N[0];
    float               *S  = &_S[0];

    for( int it = 0; it < ntpts; it += dwnSmp, d += dStep ) {

        for( int is = 0; is < ns; ++is ) {
            S[is] = 0;
            N[is] = 0;
            A[is] = 0;
        }

        for( int ig = 0; ig < nAP; ++ig ) {

            const ShankMapDesc  *e = &E[ig];

            if( e->u ) {
                S[e->s] += d[ig];
                ++N[e->s];
            }
        }

        for( int is = 0; is < ns; ++is ) {

            if( N[is] > 1 )
                A[is] = S[is] / N[is];
        }

        for( int ig = 0; ig < nAP; ++ig )
            d[ig] -= A[E[ig].s];
    }
}
#else
// ------------------
// Whole-probe method
// ------------------
void Pass1AP::sAveApplyGlobal(
    qint16  *d,
    int     ntpts,
    int     nC,
    int     nAP,
    int     dwnSmp )
{
    if( nAP <= 0 )
        return;

    const ShankMapDesc  *E = &shankMap->e[0];

    int dStep = nC * dwnSmp;

    for( int it = 0; it < ntpts; it += dwnSmp, d += dStep ) {

        double  S = 0;
        int     A = 0,
                N = 0;

        for( int ig = 0; ig < nAP; ++ig ) {

            const ShankMapDesc  *e = &E[ig];

            if( e->u ) {
                S += d[ig];
                ++N;
            }
        }

        if( N > 1 )
            A = S / N;

        for( int ig = 0; ig < nAP; ++ig )
            d[ig] -= A;
    }
}
#endif


// Space averaging for all values.
//
// Stride method was used for 3A probes before mux tables.
//
#if 0
// ----------------
// Per-shank method
// ----------------
void Pass1AP::sAveApplyDmxStride(
    qint16  *d,
    int     ntpts,
    int     nC,
    int     nAP,
    int     stride,
    int     dwnSmp )
{
    if( nAP <= 0 )
        return;

    nAP = ig2ic[nAP-1];    // highest acquired channel saved

    const ShankMapDesc  *E = &shankMap->e[0];

    int                 ns      = shankMap->ns,
                        dStep   = nC * dwnSmp;
    std::vector<int>    _A( ns ),
                        _N( ns );
    std::vector<float>  _S( ns );
    int                 *A  = &_A[0],
                        *N  = &_N[0];
    float               *S  = &_S[0];

    for( int it = 0; it < ntpts; it += dwnSmp, d += dStep ) {

        for( int ic0 = 0; ic0 < stride; ++ic0 ) {

            for( int is = 0; is < ns; ++is ) {
                S[is] = 0;
                N[is] = 0;
                A[is] = 0;
            }

            for( int ic = ic0; ic <= nAP; ic += stride ) {

                int ig = ic2ig[ic];

                if( ig >= 0 ) {

                    const ShankMapDesc  *e = &E[ig];

                    if( e->u ) {
                        S[e->s] += d[ig];
                        ++N[e->s];
                    }
                }
            }

            for( int is = 0; is < ns; ++is ) {

                if( N[is] > 1 )
                    A[is] = S[is] / N[is];
            }

            for( int ic = ic0; ic <= nAP; ic += stride ) {

                int ig = ic2ig[ic];

                if( ig >= 0 )
                    d[ig] -= A[E[ig].s];
            }
        }
    }
}
#else
// ------------------
// Whole-probe method
// ------------------
void Pass1AP::sAveApplyDmxStride(
    qint16  *d,
    int     ntpts,
    int     nC,
    int     nAP,
    int     stride,
    int     dwnSmp )
{
    if( nAP <= 0 )
        return;

    nAP = ig2ic[nAP-1];    // highest acquired channel saved

    const ShankMapDesc  *E = &shankMap->e[0];

    int dStep = nC * dwnSmp;

    for( int it = 0; it < ntpts; it += dwnSmp, d += dStep ) {

        for( int ic0 = 0; ic0 < stride; ++ic0 ) {

            double  S = 0;
            int     A = 0,
                    N = 0;

            for( int ic = ic0; ic <= nAP; ic += stride ) {

                int ig = ic2ig[ic];

                if( ig >= 0 ) {

                    const ShankMapDesc  *e = &E[ig];

                    if( e->u ) {
                        S += d[ig];
                        ++N;
                    }
                }
            }

            if( N > 1 )
                A = S / N;

            for( int ic = ic0; ic <= nAP; ic += stride ) {

                int ig = ic2ig[ic];

                if( ig >= 0 )
                    d[ig] -= A;
            }
        }
    }
}
#endif


// Space averaging for all values.
//
#if 0
// ----------------
// Per-shank method
// ----------------
void Pass1AP::sAveApplyDmxTbl(
    qint16  *d,
    int     ntpts,
    int     nC,
    int     nAP,
    int     dwnSmp )
{
    if( nAP <= 0 )
        return;

    const ShankMapDesc  *E = &shankMap->e[0];

    int                 ns      = shankMap->ns,
                        dStep   = nC * dwnSmp;
    std::vector<int>    _A( ns ),
                        _N( ns );
    std::vector<float>  _S( ns );
    const int           *T  = &muxTbl[0];
    int                 *A  = &_A[0],
                        *N  = &_N[0];
    float               *S  = &_S[0];

    for( int it = 0; it < ntpts; it += dwnSmp, d += dStep ) {

        for( int irow = 0; irow < nGrp; ++irow ) {

            for( int is = 0; is < ns; ++is ) {
                S[is] = 0;
                N[is] = 0;
                A[is] = 0;
            }

            for( int icol = 0; icol < nADC; ++icol ) {

                int ig = ic2ig[T[nADC*irow + icol]];

                if( ig >= 0 ) {

                    const ShankMapDesc  *e = &E[ig];

                    if( e->u ) {
                        S[e->s] += d[ig];
                        ++N[e->s];
                    }
                }
            }

            for( int is = 0; is < ns; ++is ) {

                if( N[is] > 1 )
                    A[is] = S[is] / N[is];
            }

            for( int icol = 0; icol < nADC; ++icol ) {

                int ig = ic2ig[T[nADC*irow + icol]];

                if( ig >= 0 )
                    d[ig] -= A[E[ig].s];
            }
        }
    }
}
#else
// ------------------
// Whole-probe method
// ------------------
void Pass1AP::sAveApplyDmxTbl(
    qint16  *d,
    int     ntpts,
    int     nC,
    int     nAP,
    int     dwnSmp )
{
    if( nAP <= 0 )
        return;

    const ShankMapDesc  *E = &shankMap->e[0];

    int *T      = &muxTbl[0];
    int dStep   = nC * dwnSmp;

    for( int it = 0; it < ntpts; it += dwnSmp, d += dStep ) {

        for( int irow = 0; irow < nGrp; ++irow ) {

            double  S = 0;
            int     A = 0,
                    N = 0;

            for( int icol = 0; icol < nADC; ++icol ) {

                int ig = ic2ig[T[nADC*irow + icol]];

                if( ig >= 0 ) {

                    const ShankMapDesc  *e = &E[ig];

                    if( e->u ) {
                        S += d[ig];
                        ++N;
                    }
                }
            }

            if( N > 1 )
                A = S / N;

            for( int icol = 0; icol < nADC; ++icol ) {

                int ig = ic2ig[T[nADC*irow + icol]];

                if( ig >= 0 )
                    d[ig] -= A;
            }
        }
    }
}
#endif


class gfixIter
{
public:
    virtual ~gfixIter() {}
    virtual bool nextGroup() = 0;
    virtual bool nextMember() = 0;
    virtual int get_ic() = 0;
};

class gfixIterShifted : public gfixIter
{
private:
    int nAP, ic;
public:
    gfixIterShifted( int nAP )
    :   nAP(nAP), ic(-1)
    {
    }
    virtual ~gfixIterShifted()  {}
    virtual bool nextGroup()
    {
        return ic == -1;
    }
    virtual bool nextMember()
    {
        return ++ic < nAP;
    }
    virtual int get_ic()
    {
        return ic;
    }
};

class gfixIterStride : public gfixIter
{
private:
    int nAP, stride, ic0, ic;
public:
    gfixIterStride( int nAP, int stride )
    :   nAP(nAP), stride(stride), ic0(-1)
    {
    }
    virtual ~gfixIterStride()   {}
    virtual bool nextGroup()
    {
        ++ic0;
        ic = ic0 - stride;
        return ic0 < stride;
    }
    virtual bool nextMember()
    {
        return (ic += stride) < nAP;
    }
    virtual int get_ic()
    {
        return ic;
    }
};

class gfixIterTable : public gfixIter
{
private:
    const int   *T;
    int         nADC, nGrp, icol, irow;
public:
    gfixIterTable( const int *T, int nADC, int nGrp )
    :   T(T), nADC(nADC), nGrp(nGrp), irow(-1)
    {
    }
    virtual ~gfixIterTable()    {}
    virtual bool nextGroup()
    {
        ++irow;
        icol = -1;
        return irow < nGrp;
    }
    virtual bool nextMember()
    {
        return ++icol < nADC;
    }
    virtual int get_ic()
    {
        return T[nADC*irow + icol];
    }
};


// Report global artifact instances.
//
// Strategy: A timepoint is reported if 25% of the members of a
// demux channel group exceed Tamp amplitude and/or Tslp slope.
//
// The stride could be set to 24 for 3A probes before mux tables.
// With mux tables, set stride = -1.
//
void Pass1AP::gFixDetect(
    QMap<qint64,LR> &TLR,
    const qint16    *d,
    qint16          *B,
    qint64          t0,
    double          Tmul,
    int             ntpts,
    int             nC,
    int             nAP,
    int             maxInt,
    int             stride )
{
#define RES 2

    if( nAP <= 0 )
        return;

    if( stride > 0 )
        nAP = ig2ic[nAP-1]; // highest acquired channel saved

    const ShankMapDesc  *E      = &shankMap->e[0];
    const int           Tamp    = GBL.gfixamp * Tmul,
                        Tslp    = GBL.gfixslp * Tmul,
                        Tbas    = GBL.gfixbas * Tmul;
    int                 B0,
                        BN;

    // for ea timepoint
    for( int it = 0; it < ntpts; ++it, d += nC ) {

        if( t0 + it < BIQUAD_TRANS_WIDE )
            continue;

        gfixIter    *G;

        if( GBL.tshift )
            G = new gfixIterShifted( nAP );
        else if( stride > 0 )
            G = new gfixIterStride( nAP, stride );
        else
            G = new gfixIterTable( &muxTbl[0], nADC, nGrp );

        bool loaded = false;

        // for ea demux group
        while( G->nextGroup() ) {

            int nMbr = 0,
                nBad = 0,
                ipk  = 0,
                vpk  = 0;

            // test ea member
            while( G->nextMember() ) {

                int ig = ic2ig[G->get_ic()];

                if( ig >= 0 ) {

                    const ShankMapDesc  *e = &E[ig];

                    if( e->u ) {

                        int V = d[ig];

                        ++nMbr;

                        if( V <= -Tamp || V >= Tamp ) {

                            int slp = (it > 0 ? V - d[ig - nC] : 0);

                            if( slp < -Tslp || slp >= Tslp ) {

                                ++nBad;

                                if( V < 0 )
                                    V = -V;

                                if( V > vpk ) {
                                    ipk = ig;
                                    vpk = V;
                                }
                            }
                        }
                    }
                }
            }

            // report

//            if( nBad >= 0.25 * nMbr ) {
//                Log() << QString("%1  %2  %3")
//                            .arg( double(t0+it)/30000, 0, 'f', 5 )
//                            .arg( nMbr ).arg( nBad );
//                break;
//            }

            if( nBad >= 0.25 * nMbr ) {

                if( !loaded ) {

                    // it       = peak position in main buffer (d).
                    // i_it     = position in i_buf.
                    // lmarg    = max allowed LHS margin.
                    // ibuf0    = where to copy from i_buf.
                    // B0       = first sample in (B) buffer in d buf units.
                    // BN       = count of B samples.
                    // IT       = position in B.

                    int     i_it  = gfix0 + it,
                            lmarg = qMin( i_it, GFIXOFF ),
                            ibuf0 = i_it - lmarg;

                    B0 = it - lmarg;
                    BN = qMin( i_lim - ibuf0, GFIXBUFSMP );

                    memcpy( &B[0], &i_buf[ibuf0 * nC], BN * meta.smpBytes );
                    loaded = true;

                    if( hp_gfix ) {
                        hp_gfix->applyBlockwiseMem( B,
                                maxInt, BN, nC, 0, nAP );
                    }
                }

                // scan backward and forward for decay to baseline
                // using ipk = greatest deflection channel.

                qint16  *D;
                int     IT  = it - B0,
                        L   = IT,
                        R   = BN - 1 - IT;
                bool    neg = d[ipk] < 0;

                // scan backward for L

                D = &B[(IT-1)*nC + ipk];

                for( int jt = IT - 1; jt >= 0; --jt, D -= nC ) {

                    if( neg ) {
                        if( *D >= -Tbas ) {
                            L = IT - jt;
                            break;
                        }
                    }
                    else if( *D <= Tbas ) {
                        L = IT - jt;
                        break;
                    }
                }

                // scan forward for R: axis crossing

                D = &B[(IT+1)*nC + ipk];

                for( int jt = IT + 1; jt < BN; ++jt, D += nC ) {

                    if( neg ) {
                        if( *D > 0 ) {
                            R = jt - IT;
                            break;
                        }
                    }
                    else if( *D < 0 ) {
                        R = jt - IT;
                        break;
                    }
                }

                // scan forward for R: decay to baseline

                D = &B[(IT+R+1)*nC + ipk];

                for( int jt = IT + R + 1; jt < BN; ++jt, D += nC ) {

                    if( neg ) {
                        if( *D <= Tbas ) {
                            R = jt - IT;
                            break;
                        }
                    }
                    else if( *D >= -Tbas ) {
                        R = jt - IT;
                        break;
                    }
                }

                TLR[t0 + (it/RES)*RES + RES/2] = LR( L, R );
            }

        }   // end demux group

        delete G;

    }   // end timepoint
}


