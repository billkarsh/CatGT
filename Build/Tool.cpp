
#include "Tool.h"
#include "CGBL.h"
#include "Util.h"
#include "Pass1NI.h"
#include "Pass1AP.h"
#include "Pass1AP2LF.h"
#include "Pass1LF.h"
#include "Pass2NI.h"
#include "Pass2AP.h"
#include "Pass2LF.h"
#include "Biquad.h"
#include "SGLTypes.h"
#include "Subset.h"

#include    <math.h>




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


void FFT::init( const Meta &meta, int ap_in, int ap_out )
{
    if( GBL.tshift ) {

        // Mux table

        IMROTbl *R = getProbe( meta.kvp );

        if( R ) {

            std::vector<int>    muxTbl;
            int                 nADC,
                                nGrp,
                                nN;

            R->muxTable( nADC, nGrp, muxTbl );
                delT = 2 * M_PI / SZFFT;

                if( !ap_in )
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
        }
    }

    if( !ap_out && GBL.apflt.needsfft() ) {

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
    else if( ap_out && GBL.lfflt.needsfft() ) {

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
// of size SZMRG, composed of previous data (and leading zeros
// as needed).
//
// - ndst is the true sample count to be copied out to dst and
// is assumed to be <= SZMID.
//
// - nsrc = SZMRG + ndst + (as many additional true samples as
// are available to fill RHS margin <= SZMRG). This function will
// zero-pad on the right out to SZFFT.
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

        if( nsrc < SZFFT )
            memset( &real[nsrc], 0, (SZFFT - nsrc) * sizeof(double) );

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
/* Pass1IO -------------------------------------------------------- */
/* ---------------------------------------------------------------- */

#define IBUF( i_pos )   &i_buf[(i_pos)*meta.nC]
#define IBYT( i_pos )   ((i_pos)*meta.smpBytes)


Pass1IO::~Pass1IO()
{
    if( hipass ) delete hipass;
    if( lopass ) delete lopass;
}


bool Pass1IO::o_open( int g0, int ip, int ap_in, int ap_out )
{
    this->ip     = ip;
    this->ap_in  = ap_in;
    this->ap_out = ap_out;

    if( !doWrite )
        return true;

    return openOutputBinary( o_f, o_name, g0, ip, ap_out );
}


// Must be called after Meta::get().
//
void Pass1IO::alloc( bool initfft )
{
    i_buf.resize( SZIBUF * meta.nC );
    o_buf.resize( SZOBUF * meta.nC );

    if( ip >= 0 ) {

        if( !ap_out && GBL.apflt.isbiquad() ) {
            if( GBL.apflt.Fhi )
                hipass = new Biquad( bq_type_highpass, GBL.apflt.Fhi / meta.srate );
            if( GBL.apflt.Flo )
                lopass = new Biquad( bq_type_lowpass, GBL.apflt.Flo / meta.srate );
        }
        else if( ap_out && GBL.lfflt.isbiquad() ) {
            if( GBL.lfflt.Fhi )
                hipass = new Biquad( bq_type_highpass, GBL.lfflt.Fhi / meta.srate );
            if( GBL.lfflt.Flo )
                lopass = new Biquad( bq_type_lowpass, GBL.lfflt.Flo / meta.srate );
        }

        if( initfft )
            fft.init( meta, ap_in, ap_out );
    }
}


// Driving loop over files moving input to output.
//
void Pass1IO::run()
{
    GT_iterator I;
    int         g, t;

    while( I.next( g, t ) ) {

        // -----------
        // Open binary
        // -----------

        switch( openInputBinary( i_f, i_fi, g, t, ip, ap_in ) ) {
            case 0: break;
            case 1: continue;
            case 2: return;
        }

        // ------------------
        // File size, overlap
        // ------------------

        qint64  xferBytes;

        switch( inputSizeAndOverlap( xferBytes, g, t ) ) {

            case 0: break;
            case 1: continue;
            case 2: return;
        }

        // ----------------------------
        // Process and copy binary data
        // ----------------------------

        while( xferBytes ) {

            if( !load( xferBytes ) || !push() ) {
                i_f.close();
                return;
            }
        }

        i_f.close();
        meta.pass1_fileDone( g, t, ip, ap_out );

        if( meta.smpInpEOF >= meta.maxOutEOF ) {
            flush();
            return;
        }
    }

    flush();
}


// The next binary file was just opened...now...
// A lot going on here:
//
// xferBytes:
// How many bytes of the next binary to process AFTER this
// function completes. This size is not bounded by maxOutEOF.
//
// If the next file overlaps the previous one, we seek into
// the binary and reduce xferBytes. Processing of chunks can
// resume with xferBytes now added to any previous remainder.
//
// If there is a gap to the next file we check for remainder
// input data, and if present, process and write that. Then
// we optionally zero fill the output and update EOF trackers.
// Note EOF tracking is updated even if (doWrite) is false.
//
// Note on EOF tracking:
// In general we should (process and) write out everything we
// read in. smpInpEOF should be updated as we read. smpOutEOF
// should be updated when we write. Generally we will read in
// multiple chunks and some margin, then pass that to output
// one chunk at a time. Therefore, smpOutEOF will usually lag
// smpInpEOF. It is smpInpEOF that is bounded by maxOutEOF.
// Likewise, zero-filling (virtual input) should be bounded
// by maxOutEOF.
//
// Return:
// 0 - ok.
// 1 - skip.
// 2 - fail or done.
//
int Pass1IO::inputSizeAndOverlap( qint64 &xferBytes, int g, int t )
{
    int t0, g0 = GBL.gt_get_first( &t0 );

    if( g > g0 || t > t0 ) {

        // Not the first file, so get its meta data...

        QFileInfo   fim;
        KVParams    kvp;
        int         ret = openInputMeta( fim, kvp, g, t, ip, ap_in, GBL.t_miss_ok );

        if( ret )
            return ret;

        xferBytes = kvp["fileSizeBytes"].toLongLong();

        qint64  olap = IBYT(meta.smpInpEOF - kvp["firstSample"].toLongLong());

        if( olap > 0 ) {

            xferBytes -= olap;

            if( xferBytes < meta.smpBytes ) {
                Log() <<
                    QString("Skipping tiny content"
                    " (olap: %1, rem: %2, bps: %3) file '%4'.")
                    .arg( olap )
                    .arg( xferBytes )
                    .arg( meta.smpBytes )
                    .arg( i_fi.fileName() );
                i_f.close();
                return 1;
            }

            if( !i_f.seek( olap ) ) {
                Log() << QString("Seek failed (offset: %1) for file '%2'.")
                            .arg( olap )
                            .arg( i_fi.fileName() );
                i_f.close();
                return 2;
            }
        }
        else if( olap < 0 ) {

            // Push any remainder data out.
            //
            // Note that this does not change the gap size used
            // for zeroFill because that is based on smpInpEOF.

            if( !flush() )
                return 2;

            if( !meta.pass1_zeroFill( *this, -olap ) )
                return 2;
        }
    }
    else    // Already have meta data for first file
        xferBytes = meta.kvp["fileSizeBytes"].toLongLong();

    return 0;
}


// - Zero-pad LHS as needed.
// - Slide remainder forward.
// - Load from file.
// - Biquad loaded data in place.
// - Call client.digital on loaded data.
//
bool Pass1IO::load( qint64 &xferBytes )
{
// Zero-pad and slide

    if( !i_nxt || i_nxt >= i_lim ) {
        // start fresh
        memset( &i_buf[0], 0, IBYT(SZMRG) );
        i_lim = SZMRG;
    }
    else {

        i_lim -= i_nxt - SZMRG;

        if( i_nxt > SZMRG )
            memcpy( &i_buf[0], IBUF(i_nxt - SZMRG), IBYT(i_lim) );
    }

// Load

    qint64  bytes;
    int     ntpts;

    bytes = meta.pass1_sizeRead( ntpts, xferBytes, IBYT(SZIBUF - i_lim) );

    if( bytes != i_f.read( (char*)IBUF(i_lim), bytes ) ) {
        Log() << QString("Read failed for file '%1'.").arg( i_fi.fileName() );
        return false;
    }

// Biquad

    if( hipass )
        hipass->applyBlockwiseMem( IBUF(i_lim), maxInt, ntpts, meta.nC, 0, meta.nN );

    if( lopass )
        lopass->applyBlockwiseMem( IBUF(i_lim), maxInt, ntpts, meta.nC, 0, meta.nN );

// Digital

    client.digital( IBUF(i_lim), ntpts );

// Indices

    i_nxt  = SZMRG;
    i_lim += ntpts;

    xferBytes       -= bytes;
    meta.smpInpEOF  += ntpts;

    return true;
}


// Process and write neural middles.
//
bool Pass1IO::push()
{
    while( i_nxt + SZMID + SZMRG <= i_lim ) {

        fft.apply( &o_buf[0], IBUF(i_nxt - SZMRG),
                SZMID, SZFFT,
                meta.nC, meta.nN );

        gfix0 = i_nxt;

        client.neural( &o_buf[0], SZMID );

        i_nxt += SZMID;

        if( !write( IBYT(SZMID) ) )
            return false;
    }

    return true;
}


// Process and write any neural remainder.
//
bool Pass1IO::flush()
{
    if( !i_nxt || i_nxt >= i_lim )
        return true;

    int ndst = i_lim - i_nxt,
        src0 = i_nxt - SZMRG;

    fft.apply( &o_buf[0], IBUF(src0),
            ndst, i_lim - src0,
            meta.nC, meta.nN );

    gfix0 = i_nxt;

    client.neural( &o_buf[0], ndst );

    i_nxt = 0;
    i_lim = 0;

    return write( IBYT(ndst) );
}


bool Pass1IO::write( qint64 bytes )
{
    if( doWrite && bytes != _write( bytes ) ) {
        Log() << QString("Write failed (error %1); input file '%2'.")
                    .arg( o_f.error() )
                    .arg( i_fi.fileName() );
        return false;
    }

    meta.smpOutEOF += bytes / meta.smpBytes;

    return true;
}


qint64 Pass1IO::_write( qint64 bytes )
{
    return o_f.write( o_buf8(), bytes );
}


bool Pass1IO::zero( qint64 gapBytes, qint64 zfBytes )
{
    Log() <<
    QString("Gap before file '%1' out_start_smp=%2 inp_gap_smp=%3 out_zeros_smp=%4")
    .arg( i_fi.fileName() )
    .arg( meta.smpOutSpan() )
    .arg( gapBytes / meta.smpBytes )
    .arg( zfBytes / meta.smpBytes );

    if( zfBytes <= 0 )
        return true;

    qint64  o_bufBytes = o_buf.size() * sizeof(qint16);

    memset( o_buf8(), 0, o_bufBytes );

    do {

        qint64  cpyBytes = qMin( zfBytes, o_bufBytes );

        if( cpyBytes != _write( cpyBytes ) ) {
            Log() << QString("Zero fill failed (error %1); input file '%2'.")
                        .arg( o_f.error() )
                        .arg( i_fi.fileName() );
            return false;
        }

        zfBytes        -= cpyBytes;
        meta.smpOutEOF += cpyBytes / meta.smpBytes;

    } while( zfBytes > 0 );

    return true;
}

/* ---------------------------------------------------------------- */
/* Meta ----------------------------------------------------------- */
/* ---------------------------------------------------------------- */

void Meta::read( int ip, int ap )
{
    if( ip >= 0 ) {

        QStringList sl = kvp["snsApLfSy"].toString().split(
                            QRegExp("^\\s+|\\s*,\\s*"),
                            QString::SkipEmptyParts );

        srate   = kvp["imSampRate"].toDouble();
        nN      = sl[ap].toInt();
    }
    else {
        srate   = kvp["niSampRate"].toDouble();
        nN      = 0;
    }

    smp1st      = kvp["firstSample"].toLongLong();
    smpInpEOF   = smp1st;
    smpOutEOF   = smp1st;
    maxOutEOF   = (GBL.maxsecs > 0 ? smp1st + GBL.maxsecs * srate : UNSET64);
    nC          = kvp["nSavedChans"].toInt();
    smpBytes    = nC*sizeof(qint16);
    gLast       = GBL.gt_get_first( &tLast );
    nFiles      = 0;
}


void Meta::write( const QString &outBin, int g0, int t0, int ip, int ap )
{
    QDateTime   tCreate( QDateTime::currentDateTime() );
    qint64      smpFLen = smpOutSpan();
    int         ne      = GBL.velem.size();

    if( !ne && !smpFLen ) {
        // Pass1:
        // If no bin written, set size that would have been written.
        // Note doWrite (hence EOF tracking) always true if trials...
        // so only need to calculate EOF for single file case.
        smpFLen = kvp["fileSizeBytes"].toLongLong() / (sizeof(qint16) * nC);
        smpFLen = qMin( smpFLen, maxOutEOF - smp1st );
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

    if( ip >= 0 )
        kvp.toMetaFile( GBL.imOutFile( g0, ip, 1, ap ) );
    else
        kvp.toMetaFile( GBL.niOutFile( g0, 1 ) );
}


// Return number of bytes to read from file bounded
// both by buffer available bytes and user maxOutEOF.
//
qint64 Meta::pass1_sizeRead( int &ntpts, qint64 xferBytes, qint64 bufBytes )
{
    qint64  bytes = qMin( xferBytes, bufBytes );

    ntpts = bytes / smpBytes;

    if( smpInpEOF + ntpts >= maxOutEOF ) {

        ntpts = maxOutEOF - smpInpEOF;
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
bool Meta::pass1_zeroFill( Pass1IO &io, qint64 gapBytes )
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

    if( smpInpEOF + ntpts > maxOutEOF ) {

        ntpts   = maxOutEOF - smpInpEOF;
        zfBytes = ntpts * smpBytes;
    }

// update smpInpEOF

    smpInpEOF += zfBytes / smpBytes;

// write and update smpOutEOF

    bool    ok = true;

    if( io.doWrite )
        ok = io.zero( gapBytes, zfBytes );
    else
        smpOutEOF += zfBytes / smpBytes;

    return ok && smpOutEOF < maxOutEOF;
}


void Meta::pass1_fileDone( int g, int t, int ip, int ap )
{
// Counting

    ++nFiles;

    if( g > gLast )
        gLast = g;

    if( t > tLast )
        tLast = t;

// Status message

    QString lbl;

    if( ip >= 0 )
        lbl = QString("%1 prb %2").arg( ap ? "lf" : "ap" ).arg( ip );
    else
        lbl = "ni stream";

    if( t == -1 )
        Log() << QString("Done %1: tcat").arg( lbl );
    else if( nFiles > 1 && nFiles % 20 == 0 ) {

        Log() << QString("Done %1: %2 of %3")
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
/* Functions ------------------------------------------------------ */
/* ---------------------------------------------------------------- */

// Return:
// 0 - treat as 1.0 lf stream.
// 1 - convert: 2.0 probe + lflopass filter.
// 2 - skip: 2.0 probe without filter.
//
static int lfCase( int ip )
{
// seek 1.0 lf meta

    int         t0, g0 = GBL.gt_get_first( &t0 );
    QString     inMeta = GBL.inFile( g0, t0, ip, 1, 1 );
    QFileInfo   fim( inMeta );

    if( fim.exists() )
        return 0;

// seek ap meta

    inMeta = GBL.inFile( g0, t0, ip, 1, 0 );
    fim.setFile( inMeta );

    if( !fim.exists() )
        return 0;

// check lf chan count

    KVParams    kvp;

    if( !kvp.fromMetaFile( inMeta ) )
        return 0;

    IMROTbl *R = getProbe( kvp );

    if( !R )
        return 0;

    int nLF = R->nLF();
    delete R;

    if( nLF )
        return 0;

// it's a 2.0... seek lf filter

    if( GBL.lfflt.haslopass() ) {
        Log() << QString("Creating lf stream for 2.0 probe %1.").arg( ip );
        return 1;
    }

    Log() <<
    QString("Can't create lf stream for 2.0 probe %1 without lf low pass filter.").arg( ip );

    return 2;
}


void pass1entrypoint()
{
    if( GBL.ni ) {
        Pass1NI P;
        if( !P.go() )
            return;
    }

    foreach( uint ip, GBL.vprb ) {

        if( GBL.ap ) {
            Pass1AP P( ip );
            if( !P.go() )
                return;
        }

        if( GBL.lf ) {
            switch( lfCase( ip ) ) {
                case 0: {
                    Pass1LF     P( ip );
                    if( !P.go() )
                        return;
                }
                break;
                case 1: {
                    Pass1AP2LF  P( ip );
                    if( !P.go() )
                        return;
                }
                break;
                default:
                    ;
            }
        }
    }
}


// acqMnMaXaDw = acquired stream channel counts.
//
static void _supercat_parseNiChanCounts(
    int             (&niCumTypCnt)[CniCfg::niNTypes],
    const KVParams  &kvp )
{
    const QStringList   sl = kvp["acqMnMaXaDw"].toString().split(
                                QRegExp("^\\s+|\\s*,\\s*"),
                                QString::SkipEmptyParts );

// --------------------------------
// First count each type separately
// --------------------------------

    niCumTypCnt[CniCfg::niTypeMN] = sl[0].toInt();
    niCumTypCnt[CniCfg::niTypeMA] = sl[1].toInt();
    niCumTypCnt[CniCfg::niTypeXA] = sl[2].toInt();
    niCumTypCnt[CniCfg::niTypeXD] = sl[3].toInt();

// ---------
// Integrate
// ---------

    for( int i = 1; i < CniCfg::niNTypes; ++i )
        niCumTypCnt[i] += niCumTypCnt[i - 1];
}


// Decrement tail edge if beyond span of lf file.
//
static bool _supercat_checkLF( double &tlast, double apsrate, int ip )
{
    QFileInfo   fim;
    KVParams    kvp;
    qint64      lfsamp;

    switch( openInputMeta( fim, kvp, GBL.ga, -1, ip, 1, GBL.prb_miss_ok ) ) {
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


// Get first edge (head) and last edge (tail) for stream (ip).
//
static bool _supercat_streamSelectEdges( int ie, int ip )
{
    QFileInfo   fib,
                fim;
    QFile       fin;
    KVParams    kvp;
    TTL         *T = 0;
    int         nC,
                is = 0; // not found
    bool        canSkip = (ip >= 0 ? GBL.prb_miss_ok : false);

// ---------------------
// Get common meta items
// ---------------------

    switch( openInputMeta( fim, kvp, GBL.ga, -1, ip, 0, canSkip ) ) {
        case 0: break;
        case 1: return true;
        case 2: return false;
    }

    nC = kvp["nSavedChans"].toInt();

// ------------------------------------------
// Find the TTL and the (is) code for this ip
// ------------------------------------------

    if( ip >= 0 ) { // Probe

        for( int i = 0, n = GBL.SY.size(); i < n; ++i ) {

            TTLD    &D = GBL.SY[i];

            if( D.ip == ip && D.word < nC ) {

                D.autoWord( nC );
                T  = &D;
                is = 2; // SY
                break;
            }
        }

        if( !is ) {
            Log() << QString("Can't find SY spec for probe %1 sync.")
                        .arg( ip );
            return false;
        }
    }
    else {  // NI stream

        QVector<uint>   chanIds;
        int             iword = kvp["syncNiChan"].toInt(),
                        bit;

        if( !getSavedChannels( chanIds, kvp, fim ) )
            return false;

        if( kvp["syncNiChanType"].toInt() == 1 ) {  // Analog

            iword = chanIds.indexOf( iword );

            for( int i = 0, n = GBL.XA.size(); i < n; ++i ) {

                T = &GBL.XA[i];

                if( T->word == iword ) {
                    is = 4; // XA
                    break;
                }
            }

            if( !is ) {
                Log() << QString("Can't find XA spec for ni sync (word %1).")
                            .arg( iword );
                return false;
            }
        }
        else {  // Digital

            int     niCumTypCnt[CniCfg::niNTypes];
            _supercat_parseNiChanCounts( niCumTypCnt, kvp );

            bit     = iword % 16;
            iword   = niCumTypCnt[CniCfg::niSumAnalog] + iword / 16;
            iword   = chanIds.indexOf( iword );

            for( int i = 0, n = GBL.XD.size(); i < n; ++i ) {

                TTLD    &D = GBL.XD[i];

                if( D.word == iword && D.bit == bit ) {
                    T  = &D;
                    is = 3; // XD
                    break;
                }
            }

            if( !is ) {
                Log() << QString("Can't find XD spec for ni sync (word %1 bit %2).")
                            .arg( iword ).arg( bit );
                return false;
            }
        }
    }

// --------------
// Open edge file
// --------------

    if( openInputFile( fin, fib, GBL.ga, -1, ip, is, 0, T ) )
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
            GBL.velem[ie].ip2head[ip]   = (ie ? t : 0);
            tlast                       = t;
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

// --------
// Check lf
// --------

    if( ip >= 0 && GBL.lf
        && !_supercat_checkLF( tlast, kvp["imSampRate"].toDouble(), ip ) ) {

        return false;
    }

// -----
// Store
// -----

    GBL.velem[ie].ip2tail[ip] = tlast;

    return true;
}


static bool _supercat_runSelectEdges( int ie )
{
// ----------------
// Get stream edges
// ----------------

    GBL.velem[ie].unpack();

    if( GBL.ni && !_supercat_streamSelectEdges( ie, -1 ) )
        return false;

    foreach( uint ip, GBL.vprb ) {

        if( (GBL.ap || GBL.lf) && !_supercat_streamSelectEdges( ie, ip ) )
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
    QMap<int,double>::iterator  it, end = GBL.velem[ie].ip2tail.end();
    for( it = GBL.velem[ie].ip2tail.begin(); it != end; ++it ) {
        if( it.value() < shortest )
            shortest = it.value();
    }

    QList<int>  keys = GBL.velem[ie].ip2tail.keys();
    foreach( int ip, keys ) {

        double  tail = GBL.velem[ie].ip2tail[ip];

        while( tail - shortest > GBL.syncper/2 )
            tail -= GBL.syncper;

        GBL.velem[ie].ip2tail[ip] = tail;
    }

    return true;
}


static bool _supercatNI( Pass2NI *NI )
{
    GBL.velem[0].unpack();

    if( !NI->first() )
        return false;

    for( int ie = 1, ne = GBL.velem.size(); ie < ne; ++ie ) {

        GBL.velem[ie].unpack();

        if( !NI->next( ie ) )
            return false;
    }

    NI->close();
    return true;
}


static bool _supercatAP( Pass2AP *AP, int ip )
{
    GBL.velem[0].unpack();

    switch( AP->first( ip ) ) {
        case 0: break;
        case 1: return true;
        case 2: return false;
    }

    for( int ie = 1, ne = GBL.velem.size(); ie < ne; ++ie ) {

        GBL.velem[ie].unpack();

        if( !AP->next( ie ) )
            return false;
    }

    AP->close();
    return true;
}


static bool _supercatLF( Pass2LF *LF, int ip )
{
    GBL.velem[0].unpack();

    switch( LF->first( ip ) ) {
        case 0: break;
        case 1: return true;
        case 2: return false;
    }

    for( int ie = 1, ne = GBL.velem.size(); ie < ne; ++ie ) {

        GBL.velem[ie].unpack();

        if( !LF->next( ie ) )
            return false;
    }

    LF->close();
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

    Pass2NI *NI = (GBL.ni ? new Pass2NI( buf ) : 0);
    Pass2AP *AP = (GBL.ap ? new Pass2AP( buf ) : 0);
    Pass2LF *LF = (GBL.lf ? new Pass2LF( buf ) : 0);

    if( GBL.ni ) {
        if( !_supercatNI( NI ) )
            goto done;
    }

    foreach( uint ip, GBL.vprb ) {

        if( GBL.ap ) {
            if( !_supercatAP( AP, ip ) )
                goto done;
        }

        if( GBL.lf ) {
            if( !_supercatLF( LF, ip ) )
                goto done;
        }
    }

done:
    if( NI ) {
        NI->close();
        delete NI;
    }

    if( AP ) {
        AP->close();
        delete AP;
    }

    if( LF ) {
        LF->close();
        delete LF;
    }
}


// Return allocated probe class, or, 0.
//
IMROTbl *getProbe( const KVParams &kvp )
{
    IMROTbl                     *R      = 0;
    KVParams::const_iterator    it_kvp  = kvp.find( "imDatPrb_type" );
    int                         prbType = -999;

    if( it_kvp != kvp.end() )
        prbType = it_kvp.value().toInt();
    else if( kvp.contains( "imProbeOpt" ) )
        prbType = -3;

    if( prbType != -999 )
        R = IMROTbl::alloc( prbType );

    return R;
}


bool getSavedChannels(
    QVector<uint>   &chanIds,
    const KVParams  &kvp,
    const QFileInfo &fim )
{
    QString chnstr = kvp["snsSaveChanSubset"].toString();

    if( Subset::isAllChansStr( chnstr ) )
        Subset::defaultVec( chanIds, kvp["nSavedChans"].toInt() );
    else if( !Subset::rngStr2Vec( chanIds, chnstr ) ) {
        Log() << QString("Bad snsSaveChanSubset tag '%1'.").arg( fim.fileName() );
        return false;
    }

    return true;
}


bool openOutputBinary( QFile &fout, QString &outBin, int g0, int ip, int ap )
{
    if( !GBL.velem.size() && GBL.gt_is_tcat() ) {
        Log() << "Error: Secondary extraction pass (-t=cat) must not concatenate or filter";
        return false;
    }

    if( ip >= 0 )
        outBin = GBL.imOutFile( g0, ip, 0, ap );
    else
        outBin = GBL.niOutFile( g0, 0 );

    fout.setFileName( outBin );

    if( !fout.open( QIODevice::ReadWrite ) ) {
        Log() << QString("Error opening '%1'.").arg( outBin );
        return false;
    }

    fout.resize( 0 );

    return true;
}


// Return:
// 0 - ok.
// 1 - skip.
// 2 - fail.
//
int openInputFile(
    QFile       &fin,
    QFileInfo   &fib,
    int         g,
    int         t,
    int         ip,
    int         is,
    int         ap,
    XCT         *X )
{
    QString inBin = GBL.inFile( g, t, ip, is, ap, X );

    fib.setFile( inBin );

    if( !fib.exists() ) {
        Log() << QString("File not found '%1'.").arg( fib.filePath() );
        if( GBL.t_miss_ok )
            return 1;
        return 2;
    }

    fin.setFileName( inBin );

    QIODevice::OpenMode mode = QIODevice::ReadOnly;

    if( X )
        mode |= QIODevice::Text;

    if( !fin.open( mode ) ) {
        Log() << QString("Error opening file '%1'.").arg( fib.filePath() );
        return 2;
    }

    return 0;
}


// Return:
// 0 - ok.
// 1 - skip.
// 2 - fail.
//
int openInputBinary(
    QFile       &fin,
    QFileInfo   &fib,
    int         g,
    int         t,
    int         ip,
    int         ap )
{
    return openInputFile( fin, fib, g, t, ip, 0, ap );
}


// Return:
// 0 - ok.
// 1 - skip.
// 2 - fail.
//
int openInputMeta(
    QFileInfo   &fim,
    KVParams    &kvp,
    int         g,
    int         t,
    int         ip,
    int         ap,
    bool        canSkip )
{
    QString inMeta = GBL.inFile( g, t, ip, 1, ap );

    fim.setFile( inMeta );

    if( !fim.exists() ) {
        Log() << QString("Meta file not found '%1'.").arg( fim.filePath() );
        if( canSkip )
            return 1;
        return 2;
    }

    if( !kvp.fromMetaFile( inMeta ) ) {
        Log() << QString("Meta file is corrupt '%1'.").arg( fim.fileName() );
        return 2;
    }

    return 0;
}


// - Check channel count matches first run.
// - Return sample count, or zero if error.
//
qint64 p2_checkCounts( const Meta &meta, int ie, int ip, int ap )
{
    QFileInfo   fim;
    KVParams    kvpn;
    int         chans;

    if( openInputMeta( fim, kvpn, GBL.ga, -1, ip, ap, false ) )
        return 0;

    chans = kvpn["nSavedChans"].toInt();

    if( chans != meta.nC ) {
        Log() <<
        QString("Error at supercat run '%1': Channel count [%2]"
        " does not match run-zero count [%3].")
        .arg( ie ).arg( chans ).arg( meta.nC );
        return 0;
    }

    if( GBL.sc_trim ) {
        Elem    &E = GBL.velem[ie];
        return  qint64(E.ip2tail[ip] * meta.srate + 1)
                - qint64(E.ip2head[ip] * meta.srate)
                - (ie ? 1 : 0);
    }

    return kvpn["fileSizeBytes"].toLongLong() / (sizeof(qint16) * meta.nC);
}


// Trimming version.
//
static bool p2_copyBinary(
    QFile               &fout,
    QFile               &fin,
    const QFileInfo     &fib,
    Meta                &meta,
    std::vector<BTYPE>  &buf,
    int                 ie,
    int                 ip )
{
    qint64  head     = GBL.velem[ie].ip2head[ip] * meta.srate,
            tail     = GBL.velem[ie].ip2tail[ip] * meta.srate + 1,
            bufBytes = sizeof(BTYPE) * buf.size(),
            finBytes = fin.size();

    if( finBytes % meta.smpBytes ) {

        finBytes = (finBytes / meta.smpBytes) * meta.smpBytes;

        Log() <<
        QString("Warning: Untrimmed binary file not a multiple of sample size '%1'.")
        .arg( fib.filePath() );
    }

    if( ie ) {
        ++head;
        fin.seek( head * meta.smpBytes );
    }

    finBytes = qMin( finBytes, (tail - head) * meta.smpBytes );

    while( finBytes > 0 ) {

        int cpySamps,
            cpyBytes = qMin( bufBytes, finBytes );

        cpySamps = cpyBytes / meta.smpBytes;
        cpyBytes = cpySamps * meta.smpBytes;

        if( cpyBytes != fin.read( (char*)&buf[0], cpyBytes ) ) {
            Log() << QString("Read failed for file '%1'.").arg( fib.filePath() );
            return false;
        }

        if( cpyBytes != fout.write( (char*)&buf[0], cpyBytes ) ) {
            Log() << QString("Write failed (error %1) for file '%2'.")
                        .arg( fout.error() )
                        .arg( fib.filePath() );
            return false;
        }

        finBytes       -= cpyBytes;
        meta.smpOutEOF += cpySamps;
    }

    return true;
}


static bool p2_copyBinary(
    QFile               &fout,
    QFile               &fin,
    const QFileInfo     &fib,
    Meta                &meta,
    std::vector<BTYPE>  &buf )
{
    qint64  bufBytes = sizeof(BTYPE) * buf.size(),
            finBytes = fin.size();

    if( finBytes % meta.smpBytes ) {
        Log() << QString("Binary file not a multiple of sample size '%1'.").arg( fib.filePath() );
        return false;
    }

    while( finBytes > 0 ) {

        int cpySamps,
            cpyBytes = qMin( bufBytes, finBytes );

        cpySamps = cpyBytes / meta.smpBytes;
        cpyBytes = cpySamps * meta.smpBytes;

        if( cpyBytes != fin.read( (char*)&buf[0], cpyBytes ) ) {
            Log() << QString("Read failed for file '%1'.").arg( fib.filePath() );
            return false;
        }

        if( cpyBytes != fout.write( (char*)&buf[0], cpyBytes ) ) {
            Log() << QString("Write failed (error %1) for file '%2'.")
                        .arg( fout.error() )
                        .arg( fib.filePath() );
            return false;
        }

        finBytes       -= cpyBytes;
        meta.smpOutEOF += cpySamps;
    }

    return true;
}


// Trimming version.
//
static void p2_copyOffsetTimes( QFile &fin, double secs, int ie, int ip, XCT *X )
{
    double  t,
            head = GBL.velem[ie].ip2head[ip],
            tail = GBL.velem[ie].ip2tail[ip];
    QString line;
    bool    ok;

    if( ie )
        secs -= head;

    while( !(line = fin.readLine()).isEmpty() ) {

        t = line.toDouble( &ok );

        if( ok ) {
            if( t > head && t <= tail )
                *X->ts << QString("%1\n").arg( t + secs, 0, 'f', 6 );
        }
        else
            *X->ts << line;
    }
}


static void p2_copyOffsetTimes( QFile &fin, double secs, XCT *X )
{
    double  t;
    QString line;
    bool    ok;

    while( !(line = fin.readLine()).isEmpty() ) {

        t = line.toDouble( &ok );

        if( ok )
            *X->ts << QString("%1\n").arg( t + secs, 0, 'f', 6 );
        else
            *X->ts << line;
    }
}


bool p2_openAndCopyFile(
    QFile               &fout,
    Meta                &meta,
    std::vector<BTYPE>  &buf,
    qint64              samps,
    int                 ie,
    int                 ip,
    int                 is,
    int                 ap,
    XCT                 *X )
{
    QFile       fin;
    QFileInfo   fib;

    if( openInputFile( fin, fib, GBL.ga, -1, ip, is, ap, X ) )
        return false;

    if( !X ) {

        if( GBL.sc_trim ) {

            if( !p2_copyBinary( fout, fin, fib, meta, buf, ie, ip ) )
                return false;
        }
        else if( !p2_copyBinary( fout, fin, fib, meta, buf ) )
            return false;
    }
    else if( GBL.sc_trim )
        p2_copyOffsetTimes( fin, samps / meta.srate, ie, ip, X );
    else
        p2_copyOffsetTimes( fin, samps / meta.srate, X );

    return true;
}


bool p2_openAndCopyBFFiles( Meta &meta, qint64 samps, int ie, XBF &B )
{
    QFile       ftin, fvin;
    QFileInfo   ftib, fvib;

    if( openInputFile( ftin, ftib, GBL.ga, -1, -1, 8, 0, &B ) )
        return false;

    if( openInputFile( fvin, fvib, GBL.ga, -1, -1, 9, 0, &B ) )
        return false;

    if( GBL.sc_trim ) { // tandem trim and copy

        double  t,
                secs = samps / meta.srate,
                head = GBL.velem[ie].ip2head[-1],
                tail = GBL.velem[ie].ip2tail[-1];
        QString LT, LV;
        bool    ok;

        if( ie )
            secs -= head;

        while( !(LT = ftin.readLine()).isEmpty() ) {

            LV = fvin.readLine();
            t  = LT.toDouble( &ok );

            if( ok ) {
                if( t > head && t <= tail ) {
                    *B.ts  << QString("%1\n").arg( t + secs, 0, 'f', 6 );
                    *B.tsv << LV;
                }
            }
            else {
                *B.ts  << LT;
                *B.tsv << LV;
            }
        }
    }
    else {

        // offet times
        p2_copyOffsetTimes( ftin, samps / meta.srate, &B );

        // append unmodified values
        QString line;
        while( !(line = fvin.readLine()).isEmpty() )
            *B.tsv << line;
    }

    return true;
}


