
#include "Util.h"
#include "Pass1AP.h"
#include "Pass1AP2LF.h"
#include "Pass1LF.h"
#include "Pass1NI.h"
#include "Pass1OB.h"
#include "Pass2.h"
#include "ChanMap.h"
#include "GeomMap.h"
#include "ShankMap.h"
#include "Subset.h"

#include <math.h>




/* ---------------------------------------------------------------- */
/* FFT ------------------------------------------------------------ */
/* ---------------------------------------------------------------- */

// With FFT-based filtering one must use a long enough FFT to adequately
// sample low frequencies. A one second span is good down to 1 Hz (really,
// 0.5 Hz not too bad), so handles LFP band data without much distortion.
// 32768 is a little longer than one second.
//
// Freq domain filtering will cause some ringing in low freq components
// so we process the data in overlapped 32768 sample chunks, throwing
// away about 500 points from each chunk end (except at the file ends).
//
// - FFT is used for {tshift, filters}.
// - FFT chunks are a power of 2 in size.
// - FFT ops make edge artifacts so we trim margins and keep middles.
// - FFT chunks are, therefore, overlapped.
// - The middles are resulting OBUFs.
// - The input buffer holds NMID output middles to reduce reload frequency.
// - The input buffer holds a premargin for FFT overlap and gfix look-back.
// - The input buffer holds a postmargin for FFT overlap and gfix look-ahead.

#define SZFFT   32768
#define SZMRG   500
#define SZMID   (SZFFT - 2*SZMRG)
#define SZOBUF  SZFFT
#define NMID    1
#define SZIBUF  (NMID*SZMID + 2*SZMRG)


FFT::~FFT()
{
    if( cplx ) {
        fftw_destroy_plan( pfwd );
        fftw_destroy_plan( pbwd );
        fftw_free( cplx );
    }

    if( real )
        fftw_free( real );
}


void FFT::init(
    const QFileInfo &fim,
    const Meta      &meta,
    t_js            js_in,
    t_js            js_out )
{
    if( GBL.tshift ) {

        // Mux table

        IMROTbl *R = GBL.getProbe( meta.kvp );

        if( R ) {

            std::vector<int>    muxTbl;
            int                 nADC,
                                nGrp,
                                nN;

            R->muxTable( nADC, nGrp, muxTbl );
                delT = 2 * M_PI / SZFFT;

                if( js_in == AP )
                    delT /= (nGrp + (R->nLF() > 0));
                else
                    delT /= nGrp;

                nN = R->nAP();
            delete R;

            // Each channel's mux group (delT multiplier), or,
            //     <= 0 if nothing to do.
            // Row-0 is the ref group, nothing to do.
            // Type 1200 fills N.A. muxTbl entries with 128.

            ic2grp.fill( -1, nADC * nGrp );
            const int   *T = &muxTbl[0];
            int         *G = &ic2grp[0];

            for( int irow = 1; irow < nGrp; ++irow ) {

                for( int icol = 0; icol < nADC; ++icol ) {

                    int ic = T[nADC*irow + icol];

                    if( ic < nN )
                        G[ic] = irow;
                }
            }

            tshift = true;
        }
        else {
            delT = 0;
            ic2grp.fill( -1, meta.nC );

            Log() << QString(
            "Skipping TShift: Can't identify probe type in metadata '%1'.")
            .arg( fim.fileName() );
        }
    }

    if( js_out == AP && GBL.apflt.needsfft() ) {

        if( GBL.apflt.type == "butter" ) {

            // Glp(f) = 1/sqrt(1 + (f/fc)^2R)
            // Ghp(f) = 1/sqrt(1 + (fc/f)^2R)
            //
            // f  = {1,...,N/2} * (sample_rate/N).
            // fc = {Fhi,Flo} (Hz).

            flt.resize( SZFFT/2 + 1 );

            flt[0] = (GBL.apflt.Fhi ? 0 : 1);

            double  f = meta.srate / SZFFT;
            int     x = 2 * GBL.apflt.order;

            for( int i = 1; i <= SZFFT/2; ++i ) {

                double  G = 1;

                if( GBL.apflt.Fhi )
                    G /= sqrt( 1 + pow( GBL.apflt.Fhi / (i * f), x ) );

                if( GBL.apflt.Flo )
                    G /= sqrt( 1 + pow( (i * f) / GBL.apflt.Flo, x ) );

                flt[i] = G;
            }
        }

        filter = true;
    }
    else if( js_out == LF && GBL.lfflt.needsfft() ) {

        if( GBL.lfflt.type == "butter" ) {

            // Glp(f) = 1/sqrt(1 + (f/fc)^2R)
            // Ghp(f) = 1/sqrt(1 + (fc/f)^2R)
            //
            // f  = {1,...,N/2} * (sample_rate/N).
            // fc = {Fhi,Flo} (Hz).

            flt.resize( SZFFT/2 + 1 );

            flt[0] = (GBL.lfflt.Fhi ? 0 : 1);

            double  f = meta.srate / SZFFT;
            int     x = 2 * GBL.lfflt.order;

            for( int i = 1; i <= SZFFT/2; ++i ) {

                double  G = 1;

                if( GBL.lfflt.Fhi )
                    G /= sqrt( 1 + pow( GBL.lfflt.Fhi / (i * f), x ) );

                if( GBL.lfflt.Flo )
                    G /= sqrt( 1 + pow( (i * f) / GBL.lfflt.Flo, x ) );

                flt[i] = G;
            }
        }

        filter = true;
    }

// FFT workspace

    if( tshift || filter ) {

        if( !ic2grp.size() )
            ic2grp.fill( -1, meta.nC );

        real = fftw_alloc_real( SZFFT );
        cplx = fftw_alloc_complex( SZFFT/2 + 1 );
        pfwd = fftw_plan_dft_r2c_1d( SZFFT, real, cplx, FFTW_ESTIMATE );
        pbwd = fftw_plan_dft_c2r_1d( SZFFT, cplx, real, FFTW_ESTIMATE );
    }
}


// - On entry, caller will have prepared src with a LHS margin
// of size SZMRG, composed of previous data (and leading padding
// as needed).
//
// - ndst is the true sample count to be copied out to dst and
// is assumed to be <= SZMID.
//
// - nsrc = SZMRG + ndst + (as many additional true samples as
// are available to fill RHS margin <= SZMRG). This function will
// pad on the right out to SZFFT.
//
// Apply dst = FFT(src), or, dst = src if FFT not initialized.
//
void FFT::apply(
    qint16          *dst,
    const qint16    *src,
    int             ndst,
    int             nsrc,
    int             nC,
    int             nN )
{
    int src0 = SZMRG * nC;

    if( !(tshift || filter) ) {
        // move all mids
        memcpy( dst, &src[src0], ndst * nC * sizeof(qint16) );
        return;
    }

// For each channel, process or move it...

    double  norm = 1.0 / SZFFT;

    for( int ic = 0; ic < nC; ++ic ) {

        qint16          *d = &dst[ic];
        const qint16    *s;

        if( ic >= nN || (ic2grp[ic] <= 0 && !filter) ) {
            // move mid
            s = &src[src0 + ic];
            for( int i = 0; i < ndst; ++i, d += nC, s += nC )
                *d = *s;
            continue;
        }

        // in, including LHS margin

        s = &src[ic];
        for( int i = 0; i < nsrc; ++i, s += nC )
            real[i] = *s;

        // RHS pad

        if( nsrc < SZFFT ) {
            // memset( &real[nsrc], 0, (SZFFT - nsrc) * sizeof(double) );
            extendRHS( nsrc );
        }

        // process

        fftw_execute( pfwd );

        if( tshift )
            timeShiftChannel( ic2grp[ic] );

        if( filter ) {

            double  *A = &cplx[0][0];

            for( int j = 0; j <= SZFFT/2; ++j, A += 2 ) {
                A[0] *= flt[j];
                A[1] *= flt[j];
            }
        }

        fftw_execute( pbwd );

        // out, excluding LHS margin

        double  *r = &real[SZMRG];
        for( int i = 0; i < ndst; ++i, d += nC, ++r )
            *d = norm * *r;
    }
}


// Copy last loaded sample to each
// remaining position in FFT space.
//
void FFT::extendRHS( int nsrc )
{
    double  src = (nsrc > 0 ? real[nsrc-1] : 0.0);
    double* dst = &real[nsrc];

    for( int i = 0, n = SZFFT - nsrc; i < n; ++i )
        *dst++ = src;
}


// Freqs: j = {0, 1, ..., N/2} * (sample_rate/N).
// The tshift amount for 0th channel in a group is zero.
// The tshift amount for ith channel in a group is igrp/(nGrp*sample_rate),
//     where nGrp*sample_rate is the multiplexing sample rate.
// DC-component 0 does not need multiplier.
// Multiplier M(j)
//     = exp(-i*2pi*(j*sample_rate/N)*t_shift)
//     = exp(-i*2pi*j*igrp/(nGrp*N))
//     = exp(-i*igrp*delT*j)
//     = M(1)^j.
//
void FFT::timeShiftChannel( int igrp )
{
    double  arg = igrp * delT,
            c1  = cos( arg ),
            s1  = -sin( arg ),
            cx  = 1,
            sx  = 0;
    double  *A  = &cplx[1][0];

    for( int j = 1; j <= SZFFT/2; ++j, A += 2 ) {

        double  c = cx*c1 - sx*s1,
                s = cx*s1 + sx*c1,
                a = A[0],
                b = A[1];

        cx      = c;
        sx      = s;
        A[0]    = a*c - b*s;
        A[1]    = a*s + b*c;
    }
}

/* ---------------------------------------------------------------- */
/* Meta ----------------------------------------------------------- */
/* ---------------------------------------------------------------- */

void Meta::read( t_js js )
{
    switch( js ) {
        case NI:
            srate   = kvp["niSampRate"].toDouble();
            nN      = 0;
            break;
        case OB:
            srate   = kvp["obSampRate"].toDouble();
            nN      = 0;
            break;
        case AP:
        case LF:
            QStringList sl = kvp["snsApLfSy"].toString().split(
                                QRegExp("^\\s+|\\s*,\\s*"),
                                QString::SkipEmptyParts );
            srate   = kvp["imSampRate"].toDouble();
            nN      = sl[js-AP].toInt();
            break;
    }

    smp1st          = kvp["firstSample"].toLongLong()
                        + qint64(GBL.startsecs * srate);
    smpToBeWritten  = 0;
    smpWritten      = 0;
    maxOutEOF       = (GBL.maxsecs > 0 ? qint64(GBL.maxsecs * srate) : UNSET64);
    nC              = kvp["nSavedChans"].toInt();
    smpBytes        = nC*sizeof(qint16);
    gLast           = GBL.gt_get_first( &tLast );
    nFiles          = 0;
}


void Meta::write( const QString &outBin, int g0, int t0, t_js js, int ip )
{
    QDateTime   tCreate( QDateTime::currentDateTime() );
    qint64      smpFLen = smpWritten;
    int         ne      = GBL.velem.size();

    if( !ne && !smpFLen ) {
        // Pass1:
        // If no bin written, set size that would have been written.
        // Note doWrite (hence EOF tracking) always true if trials...
        // so only need to calculate EOF for single file case.
        smpFLen = kvp["fileSizeBytes"].toLongLong() / (sizeof(qint16) * nC);
        smpFLen = qMin( smpFLen, maxOutEOF );
    }

    kvp["fileName"]                 = outBin;
    kvp["fileCreateTime_original"]  = kvp["fileCreateTime"];
    kvp["fileCreateTime"]           = dateTime2Str( tCreate, Qt::ISODate );
    kvp["fileSHA1"]                 = 0;
    kvp["fileSizeBytes"]            = smpBytes * smpFLen;
    kvp["fileTimeSecs"]             = smpFLen / srate;

    if( ne ) {
        delPass1Tags();
        kvp["catNRuns"]     = QString("%1").arg( nFiles );
        kvp["catGTCmdline"] = QString("<%1>").arg( GBL.sCmd );
    }
    else {
        kvp["catNFiles"]    = QString("%1").arg( nFiles );
        kvp["catGVals"]     = QString("%1,%2").arg( g0 ).arg( gLast );
        kvp["catTVals"]     = QString("%1,%2").arg( t0 ).arg( tLast );
        cmdlineEntry();
    }

    switch( js ) {
        case NI: kvp.toMetaFile( GBL.niOutFile( g0, eMETA ) ); break;
        case OB: kvp.toMetaFile( GBL.obOutFile( g0, ip, eMETA ) ); break;
        case AP:
        case LF: kvp.toMetaFile( GBL.imOutFile( g0, js, ip, eMETA ) ); break;
    }
}


void Meta::writeSave( int sv0, int svLim, int g0, int t0, t_js js_out )
{
    ChanMapIM                   *chanMap    = 0;
    GeomMap                     *geomMap    = 0;
    ShankMap                    *shankMap   = 0;
    KVParams::const_iterator    it_kvp;
    int                         nimec = 0;

    for( int is = sv0; is < svLim; ++is ) {
        const Save &S = GBL.vS[is];
        if( S.ip2 > nimec )
            nimec = S.ip2;
    }
    ++nimec;

    it_kvp = kvp.find( "~snsChanMap" );
    if( it_kvp != kvp.end() ) {
        chanMap = new ChanMapIM;
        chanMap->fromString( it_kvp.value().toString() );
    }

    it_kvp = kvp.find( "~snsGeomMap" );
    if( it_kvp != kvp.end() ) {
        geomMap = new GeomMap;
        geomMap->fromString( it_kvp.value().toString() );
    }

    it_kvp = kvp.find( "~snsShankMap" );
    if( it_kvp != kvp.end() ) {
        shankMap = new ShankMap;
        shankMap->fromString( it_kvp.value().toString() );
    }

    for( int is = sv0; is < svLim; ++is ) {

        const Save &S = GBL.vS[is];

        smpBytes = S.smpBytes;

        kvp["nSavedChans"]          = S.nC;
        kvp["snsSaveChanSubset"]    = S.sUsr_out;

        QString fmt = (S.js == AP ? "%1,0,%2" : "0,%1,%2");
        kvp["snsApLfSy"] = QString(fmt)
                            .arg( S.nN )
                            .arg( S.iKeep.size() - S.nN );

        if( kvp["typeImEnabled"].toInt() < nimec )
            kvp["typeImEnabled"] = nimec;

        QBitArray   bits;
        Subset::vec2Bits( bits, S.iKeep );

        if( chanMap )
            kvp["~snsChanMap"] = chanMap->toString( bits );

        if( geomMap )
            kvp["~snsGeomMap"] = geomMap->toString( bits, 0 );

        if( shankMap )
            kvp["~snsShankMap"] = shankMap->toString( bits, 0 );

        write( S.o_name, g0, t0, js_out, S.ip2 );
    }

    if( chanMap )
        delete chanMap;

    if( geomMap )
        delete geomMap;

    if( shankMap )
        delete shankMap;
}


// Return number of bytes to read from file bounded
// both by buffer available bytes and user maxOutEOF.
//
qint64 Meta::pass1_sizeRead( int &ntpts, qint64 xferBytes, qint64 bufBytes )
{
    qint64  bytes = qMin( xferBytes, bufBytes );

    ntpts = bytes / smpBytes;

    if( smpToBeWritten + ntpts >= maxOutEOF ) {

        ntpts = maxOutEOF - smpToBeWritten;
        bytes = ntpts * smpBytes;
    }

    return bytes;
}


// Output zeros bounded both by zfilmax and maxOutEOF.
//
// A false result terminates Pass1IO::run()...
// If failed file IO return false.
// If OK, but maxOutEOF reached, return false.
// Else return true.
//
bool Meta::pass1_zeroFill( Pass1 &H, qint64 gapBytes )
{
    qint64  zfBytes = gapBytes;

// zfilmax

    if( GBL.zfilmax >= 0 ) {
        // Rounded to whole samples
        zfBytes = qMin(
                    smpBytes * qint64(0.001 * GBL.zfilmax * srate),
                    gapBytes );
    }

// maxOutEOF

    int ntpts = zfBytes / smpBytes;

    if( smpToBeWritten + ntpts > maxOutEOF ) {

        ntpts   = maxOutEOF - smpToBeWritten;
        zfBytes = ntpts * smpBytes;
    }

// update smpToBeWritten

    smpToBeWritten += zfBytes / smpBytes;

// write and update smpWritten

    bool    ok = true;

    if( H.doWrite )
        ok = H.zero( gapBytes, zfBytes );
    else
        smpWritten += zfBytes / smpBytes;

    return ok && smpWritten < maxOutEOF;
}


void Meta::pass1_fileDone( int g, int t, t_js js, int ip )
{
// Counting

    ++nFiles;

    if( g > gLast )
        gLast = g;

    if( t > tLast )
        tLast = t;

// Status message

    QString lbl;

    switch( js ) {
        case NI: lbl = "nidq"; break;
        case OB: lbl = QString("obx%1").arg( ip ); break;
        case AP: lbl = QString("imap%1").arg( ip ); break;
        case LF: lbl = QString("imlf%1").arg( ip ); break;
    }

    if( t == -1 )
        Log() << QString("Done %1: tcat.").arg( lbl );
    else if( nFiles > 1 && nFiles % 20 == 0 ) {

        Log() << QString("Done %1: %2 of %3.")
                    .arg( lbl ).arg( nFiles ).arg( GBL.gt_nIndices() );
    }
}


void Meta::cmdlineEntry()
{
// Next key available to use

    QString                     s;
    KVParams::const_iterator    end = kvp.end();

    for( int i = 0; i < 100; ++i ) {

        s = QString("catGTCmdline%1").arg( i );

        if( kvp.find( s ) == end )
            goto insert;
    }

insert:
    kvp[s] = QString("<%1>").arg( GBL.sCmd );
}


void Meta::delPass1Tags()
{
    for( int i = 0; i < 100; ++i )
        kvp.remove(  QString("catGTCmdline%1").arg( i ) );

    kvp.remove( "catNFiles" );
    kvp.remove( "catGVals" );
    kvp.remove( "catTVals" );
}

/* ---------------------------------------------------------------- */
/* FOffsets ------------------------------------------------------- */
/* ---------------------------------------------------------------- */

FOffsets    gFOff;  // global file offsets


void FOffsets::init( double rate, t_js js, int ip )
{
    QString s = stream( js, ip );

    mrate0[s] = rate;
    moff[s].push_back( GBL.startsecs > 0 ? -qint64(GBL.startsecs * rate) : 0 );
}


void FOffsets::addOffset( qint64 off, t_js js, int ip )
{
    moff[stream( js, ip )].push_back( off );
}


void FOffsets::addRate( double rate, t_js js, int ip )
{
    mrate[stream( js, ip )].push_back( rate );
}


void FOffsets::dwnSmp( int ip )
{
    QString s = stream( LF, ip );

    if( moff.find( s ) != moff.end() ) {

        QVector<qint64> &V = moff[s];

        for( int i = 1, n = V.size(); i < n; ++i )
            V[i] /= 12;
    }
}


void FOffsets::ct_write()
{
    if( GBL.in_catgt_fld || !gFOff.mrate0.size() )
        return;

    QString dir,
            srun = QString("%1_g%2")
                    .arg( GBL.run ).arg( GBL.gt_get_first( 0 ) );

    if( !GBL.dest.isEmpty() ) {
        dir = GBL.dest + "/";
        if( !GBL.no_catgt_fld )
            dir += QString("catgt_%1/").arg( srun );
    }
    else {
        dir = GBL.inpar + "/";
        if( !GBL.no_run_fld )
            dir += QString("%1/").arg( srun );
    }

    writeEntries( dir + QString("%1_ct_offsets.txt").arg( srun ) );
}


void FOffsets::sc_write()
{
    if( GBL.velem.size() < 2 || !gFOff.mrate0.size() )
        return;

    QString srun = QString("%1_g%2")
                    .arg( GBL.velem[0].run ).arg( GBL.velem[0].g );

    writeEntries( QString("%1/supercat_%2/%2_sc_offsets.txt")
                    .arg( GBL.dest ).arg( srun ) );
}


QString FOffsets::stream( t_js js, int ip )
{
    switch( js ) {
        case NI: return "nidq";
        case OB: return QString("obx%1").arg( ip );
        case AP: return QString("imap%1").arg( ip );
        case LF: return QString("imlf%1").arg( ip );
    }
}


void FOffsets::writeEntries( QString file )
{
    QFile   f( file );
    f.open( QIODevice::WriteOnly | QIODevice::Text );
    QTextStream ts( &f );

    // Offsets
    {
        QMap<QString,QVector<qint64>>::const_iterator it, end = moff.end();

        for( it = moff.begin(); it != end; ++it ) {

            const QVector<qint64>   &V = it.value();

            ts << "smp_" << it.key() << ":";
            foreach( qint64 d, V )
                ts << "\t" << d;
            ts << "\n";
        }

        for( it = moff.begin(); it != end; ++it ) {

            double                  rate    = mrate0[it.key()];
            const QVector<qint64>   &V      = it.value();

            ts << "sec_" << it.key() << ":";
            foreach( qint64 d, V )
                ts << "\t" << QString("%1").arg( d / rate, 0, 'f', 6 );
            ts << "\n";
        }
    }

    // Rates
    {
        QMap<QString,QVector<double>>::const_iterator it, end = mrate.end();

        for( it = mrate.begin(); it != end; ++it ) {

            const QVector<double>   &V = it.value();

            ts << "srate_" << it.key() << ":";
            foreach( double d, V )
                ts << "\t" << QString("%1").arg( d, 0, 'f', 6 );
            ts << "\n";
        }
    }

    ts.flush();
    f.close();
}


/* ---------------------------------------------------------------- */
/* P1LFCase ------------------------------------------------------- */
/* ---------------------------------------------------------------- */

P1LFCase    gP1LFCase; // global lf case types


void P1LFCase::init()
{
    if( !GBL.lf )
        return;

    int t0, g0 = GBL.gt_get_first( &t0 );

    foreach( uint ip, GBL.vprb )
        ip2case[ip] = lfCaseCalc( g0, t0, ip );
}


// Return:
// 0 - treat as 1.0 lf stream.
// 1 - convert: full-band + lflopass filter.
// 2 - skip: problem.
//
int P1LFCase::lfCaseCalc( int g0, int t0, int ip )
{
// seek 1.0 lf meta

    QString     inMeta = GBL.inFile( g0, t0, LF, ip, eMETA );
    QFileInfo   fim( inMeta );

    if( fim.exists() )
        return 0;

// seek ap meta

    inMeta = GBL.inFile( g0, t0, AP, ip, eMETA );
    fim.setFile( inMeta );

    if( !fim.exists() )
        return 0;

// Check full-band criteria:
// - Either no LF channels, or,
// - At least one AP filter OFF.

    KVParams    kvp;

    if( !kvp.fromMetaFile( inMeta ) )
        return 0;

    bool fullband = false;

    IMROTbl *R = GBL.getProbe( kvp );

        if( !R ) {
            Log() <<
            QString("LF convert error: probe %1: Unknown probe type.")
            .arg( ip );
            return 2;
        }

        if( !R->nLF() )
            fullband = true;
        else {
            R->fromString( 0, kvp["~imroTbl"].toString() );
            fullband = R->anyChanFullBand();
        }
    delete R;

    if( !fullband ) {
        Log() <<
        QString("LF convert error: probe %1: No LF files, AP not full-band.")
        .arg( ip );
        return 2;
    }

// it's full-band... seek lf filter

    if( GBL.lfflt.haslopass() ) {
        Log() << QString("Creating lf stream for probe %1.").arg( ip );
        return 1;
    }

    Log() <<
    QString("LF convert error: probe %1: No lf low pass filter.")
    .arg( ip );

    return 2;
}

/* ---------------------------------------------------------------- */
/* P1EOF ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

P1EOF   gP1EOF; // global file length


bool P1EOF::GTJSIP::operator<( const GTJSIP &rhs ) const
{
    if( g < rhs.g )
        return true;

    if( g > rhs.g )
        return false;

    if( t < rhs.t )
        return true;

    if( t > rhs.t )
        return false;

    if( js < rhs.js )
        return true;

    if( js > rhs.js )
        return false;

    return ip < rhs.ip;
}


bool P1EOF::init()
{
// Get all EOF metadata

    GT_iterator I;
    int         g, t;

    while( I.next( g, t ) ) {

        if( GBL.ni ) {
            if( !getMeta( g, t, NI, 0, GBL.t_miss_ok ) )
                return false;
        }

        foreach( uint ip, GBL.vobx ) {
            if( !getMeta( g, t, OB, ip, GBL.t_miss_ok ) )
                return false;
        }

        foreach( uint ip, GBL.vprb ) {

            bool    miss_ok = GBL.t_miss_ok || GBL.prb_miss_ok;

            if( GBL.ap ) {
                if( !getMeta( g, t, AP, ip, miss_ok ) )
                    return false;
            }

            if( GBL.lf ) {
                switch( gP1LFCase.getCase( ip ) ) {
                    case 0:
                        if( !getMeta( g, t, LF, ip, miss_ok ) )
                            return false;
                    break;
                    case 1:
                        if( !getMeta( g, t, AP, ip, miss_ok ) )
                            return false;
                    break;
                    default:
                        ;
                }
            }
        }
    }

// Trim each {g,t} set to shortest

    QMap<GTJSIP,EOFDAT>::iterator
        it      = id2dat.begin(),
        end     = id2dat.end(),
        start   = it,
        last    = it + 1,
        best    = it;

    for( ; ; ++it ) {

        if( it == end ||
            it.key().g != start.key().g ||
            it.key().t != start.key().t ) {

            for( QMap<GTJSIP,EOFDAT>::iterator j = start; j != last; ++j ) {

                // Guard against making file too long
                // due to an inaccurate revised rate.

                if( j != best ) {
                    j.value().bytes =
                        qMin( j.value().bytes,
                            j.value().smpBytes *
                            llround(best.value().span *
                            j.value().srate) );
                }
            }

            if( it == end )
                break;

            start = it;
            last  = it + 1;
            best  = it;
        }
        else {
            if( it.value().span < best.value().span )
                 best = it;

            last = it + 1;
        }
    }

    return true;
}


P1EOF::EOFDAT P1EOF::getEOFDAT( int g, int t, t_js js, int ip ) const
{
    return id2dat[GTJSIP( g, t, js, ip )];
}


bool P1EOF::getMeta( int g, int t, t_js js, int ip, bool t_miss_ok )
{
    QFileInfo   fim;
    KVParams    kvp;
    int         ret = GBL.openInputMeta( fim, kvp, g, t, js, ip, t_miss_ok );

    switch( ret ) {
        case 0: break;
        case 1: return true;
        case 2: return false;
    }

    EOFDAT  D;

    switch( js ) {
        case NI:
            D.srate = kvp["niSampRate"].toDouble();
            break;
        case OB:
            D.srate = kvp["obSampRate"].toDouble();
            break;
        case AP:
        case LF:
            D.srate = kvp["imSampRate"].toDouble();
            break;
    }

    D.bytes     = kvp["fileSizeBytes"].toLongLong();
    D.smp1st    = kvp["firstSample"].toLongLong();
    D.smpBytes  = sizeof(qint16) * kvp["nSavedChans"].toInt();
    D.span      = D.bytes / D.smpBytes / D.srate;

    id2dat[GTJSIP( g, t, js, ip )] = D;
    return true;
}

/* ---------------------------------------------------------------- */
/* Functions ------------------------------------------------------ */
/* ---------------------------------------------------------------- */

void pass1entrypoint()
{
    gP1LFCase.init();

    if( !gP1EOF.init() )
        goto done;

    if( GBL.ni ) {
        Pass1NI P;
        if( !P.go() )
            goto done;
    }

    foreach( uint ip, GBL.vobx ) {

        Pass1OB P( ip );
        if( !P.go() )
            goto done;
    }

    foreach( uint ip, GBL.vprb ) {

        if( GBL.ap ) {
            Pass1AP P( ip );
            if( !P.go() )
                goto done;
        }

        if( GBL.lf ) {
            switch( gP1LFCase.getCase( ip ) ) {
                case 0: {
                    Pass1LF     P( ip );
                    if( !P.go() )
                        goto done;
                }
                break;
                case 1: {
                    Pass1AP2LF  P( ip );
                    if( !P.go() )
                        goto done;
                }
                break;
                default:
                    ;
            }
        }
    }

done:
    gFOff.ct_write();
    GBL.fyi_ct_write();
}


// Decrement tail edge if beyond span of lf file.
//
static bool _supercat_checkLF( double &tlast, double apsrate, int ip )
{
    QFileInfo   fim;
    KVParams    kvp;
    qint64      lfsamp;

    switch( GBL.openInputMeta( fim, kvp, GBL.ga, -1, LF, ip, GBL.prb_miss_ok ) ) {
        case 0: break;
        case 1: return true;
        case 2: return false;
    }

    lfsamp = kvp["fileSizeBytes"].toLongLong()
                / (sizeof(qint16) * kvp["nSavedChans"].toInt());

// AP edge beyond AP-span of lf?

    if( tlast * apsrate >= 12 * lfsamp )
        tlast -= GBL.syncper;

    return true;
}


// Get first edge (head) and last edge (tail) for stream.
//
// Note that this operation requires that sync edges files
// were extracted during pass 1, which can not happen for
// LF stream. Therefore, this js parameter should be only
// {NI, OB, AP}.
//
static bool _supercat_streamSelectEdges( int ie, t_js js, int ip )
{
// ---------------------
// Get common meta items
// ---------------------

    QFileInfo   fim;
    KVParams    kvp;
    bool        miss_ok = (js == AP ? GBL.prb_miss_ok : false);

    switch( GBL.openInputMeta( fim, kvp, GBL.ga, -1, js, ip, miss_ok ) ) {
        case 0: break;
        case 1: return true;
        case 2: return false;
    }

// -------------------
// Find sync extractor
// -------------------

    XTR *X  = 0;
    int exLim,
        ex0 = GBL.myXrange( exLim, js, ip ),
        nC  = kvp["nSavedChans"].toInt();

    for( int i = ex0; i < exLim; ++i ) {

        XTR *iX = GBL.vX[i];

        if( iX->usrord == -1 ) {
            iX->autoWord( nC );
            X = iX;
            break;
        }
    }

// --------------
// Open edge file
// --------------

    QFileInfo   fib;
    QFile       fin;

    if( GBL.openInputFile( fin, fib, GBL.ga, -1, js, ip, X->ex, X ) )
        return false;

// ---------
// Get edges
// ---------

    double  tlast = 0;
    QString line;
    bool    ok;

// first

    while( !(line = fin.readLine()).isEmpty() ) {

        double  t = line.toDouble( &ok );

        if( ok ) {
            GBL.velem[ie].head( js, ip )    = (ie ? t : 0);
            tlast                           = t;
            break;
        }
    }

// syncper

    if( GBL.syncper <= 0 ) {

        while( !(line = fin.readLine()).isEmpty() ) {

            double  t = line.toDouble( &ok );

            if( ok ) {
                GBL.syncper = t - tlast;
                tlast       = t;
                break;
            }
        }
    }

// last

    while( !(line = fin.readLine()).isEmpty() ) {

        double  t = line.toDouble( &ok );

        if( ok )
            tlast = t;
    }

// -------------------
// At least two edges?
// -------------------

    if( tlast == 0 || GBL.velem[ie].iq2head.isEmpty() ||
        tlast - GBL.velem[ie].head( js, ip ) < 2.0 ) {

        Log() <<
        QString("-supercat_trim_edges found fewer than 2 edges in file '%1'.")
        .arg( fib.filePath() );
        return false;
    }

// --------
// Check lf
// --------

    if( js == AP && GBL.lf
        && !_supercat_checkLF( tlast, kvp["imSampRate"].toDouble(), ip ) ) {

        return false;
    }

// -----
// Store
// -----

    GBL.velem[ie].tail( js, ip ) = tlast;

    return true;
}


static bool _supercat_runSelectEdges( int ie )
{
// ----------------
// Get stream edges
// ----------------

    GBL.velem[ie].unpack();

    if( GBL.ni && !_supercat_streamSelectEdges( ie, NI, 0 ) )
        return false;

    foreach( uint ip, GBL.vobx ) {
        if( !_supercat_streamSelectEdges( ie, OB, ip ) )
            return false;
    }

    foreach( uint ip, GBL.vprb ) {
        if( !_supercat_streamSelectEdges( ie, AP, ip ) )
            return false;
    }

// ------------------------------------
// Adjust tail so exists in all streams
// ------------------------------------

// Nominally, all streams stop togther and the tail edges
// are close, where close means within syncper/2. However,
// Some tails may be short because of lf downsampling.
// Some tails may be long because a worker thread got a late stop signal.
// The common tail is the earliest. The others are adjusted by subtracting
// whole periods until "close."

    double  shortest = 1e99;
    QMap<int,double>::iterator  it, end = GBL.velem[ie].iq2tail.end();
    for( it = GBL.velem[ie].iq2tail.begin(); it != end; ++it ) {
        if( it.value() < shortest )
            shortest = it.value();
    }

    QList<int>  keys = GBL.velem[ie].iq2tail.keys();
    foreach( int iq, keys ) {

        double  tail = GBL.velem[ie].iq2tail[iq];

        while( tail - shortest > GBL.syncper/2 )
            tail -= GBL.syncper;

        GBL.velem[ie].iq2tail[iq] = tail;
    }

    return true;
}


static bool _supercat( Pass2 *H, int ip )
{
    GBL.velem[0].unpack();

    switch( H->first( ip ) ) {
        case 0: break;
        case 1: return true;
        case 2: return false;
    }

    for( int ie = 1, ne = GBL.velem.size(); ie < ne; ++ie ) {

        GBL.velem[ie].unpack();

        if( !H->next( ie ) )
            return false;
    }

    H->close();
    return true;
}


void supercatentrypoint()
{
// For each run and its streams, select head and tail sync edges

    if( GBL.sc_trim ) {
        for( int ie = 0, ne = GBL.velem.size(); ie < ne; ++ie ) {
            if( !_supercat_runSelectEdges( ie ) )
                return;
        }
    }

// supercat each stream

    std::vector<BTYPE>  buf( 32*1024*1024 / sizeof(BTYPE) );

    Pass2 *hNI = (GBL.ni ? new Pass2( buf, NI ) : 0);
    Pass2 *hOB = (GBL.ob ? new Pass2( buf, OB ) : 0);
    Pass2 *hAP = (GBL.ap ? new Pass2( buf, AP ) : 0);
    Pass2 *hLF = (GBL.lf ? new Pass2( buf, LF ) : 0);

    if( GBL.ni && !_supercat( hNI, 0 ) )
        goto done;

    foreach( uint ip, GBL.vobx ) {
        if( !_supercat( hOB, ip ) )
            goto done;
    }

    foreach( uint ip, GBL.vprb ) {
        if( GBL.ap && !_supercat( hAP, ip ) )
            goto done;
        if( GBL.lf && !_supercat( hLF, ip ) )
            goto done;
    }

done:
    gFOff.sc_write();
    GBL.fyi_sc_write();

    if( hLF ) {
        hLF->close();
        delete hLF;
    }

    if( hAP ) {
        hAP->close();
        delete hAP;
    }

    if( hOB ) {
        hOB->close();
        delete hOB;
    }

    if( hNI ) {
        hNI->close();
        delete hNI;
    }
}


