
#include "Pass1.h"
#include "Util.h"
#include "Biquad.h"
#include "Subset.h"


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

#define IBUF( i_pos )   &i_buf[(i_pos)*meta.nC]
#define IBYT( i_pos )   ((i_pos)*meta.smpBytes)


/* ---------------------------------------------------------------- */
/* protected ------------------------------------------------------ */
/* ---------------------------------------------------------------- */

Pass1::~Pass1()
{
    for( int is = sv0; is < svLim; ++is )
        GBL.vS[is].close();

    if( hipass ) delete hipass;
    if( lopass ) delete lopass;
}


bool Pass1::splitShanks()
{
    flip_NXT =  meta.kvp["imDatPrb_type"].toInt() > 3000 &&
                meta.kvp["imDatApi"].toString() < "4.0.2";

    if( !doWrite )
        return true;

    if( js_in != AP )
        return true;

    for( int ik = 0, nk = GBL.vSK.size(); ik < nk; ++ik ) {

        SepShanks   &K = GBL.vSK[ik];

        if( K.ip > ip )
            return true;
        if( K.ip < ip )
            continue;

        return K.split( meta.kvp, fim );
    }

    return true;
}


bool Pass1::parseMaxZ( int &theZ )
{
    theZ = -1;

    if( !doWrite )
        return true;

    if( js_in < AP )
        return true;

    for( int iz = 0, nz = GBL.vMZ.size(); iz < nz; ++iz ) {

        MaxZ    &Z = GBL.vMZ[iz];

        if( Z.ip > ip )
            return true;
        if( Z.ip < ip )
            continue;

        theZ = iz;

        return Z.apply( meta.kvp, fim, js_in, js_out );
    }

    return true;
}


bool Pass1::o_open( int g0 )
{
    if( !doWrite )
        return true;

    if( js_in >= AP ) {

        sv0 = GBL.mySrange( svLim, js_in, ip );

        if( svLim > sv0 ) {

            for( int is = sv0; is < svLim; ++is ) {
                if( !GBL.vS[is].o_open( g0, js_out ) )
                    return false;
            }

            return true;
        }
    }

    return GBL.openOutputBinary( o_f, o_name, g0, js_out, ip, ip );
}


void Pass1::initDigitalFields( double rngMax )
{
    ex0 = GBL.myXrange( exLim, js_in, ip );

    for( int i = ex0; i < exLim; ++i ) {

        XTR *X = GBL.vX[i];

        X->autoWord( meta.nC );

        if( X->word < meta.nC )
            X->init( meta.srate, rngMax );
    }
}


bool Pass1::openDigitalFiles( int g0 )
{
    for( int i = ex0; i < exLim; ++i ) {

        XTR *X = GBL.vX[i];

        if( X->word >= meta.nC )
            continue;

        if( !X->openOutFiles( g0 ) )
            return false;
    }

    return true;
}


// Must be called after Meta::get().
//
void Pass1::alloc()
{
    i_buf.resize( SZIBUF * meta.nC );
    o_buf.resize( SZOBUF * meta.nC );

    if( js_in >= AP ) {

        if( js_out == AP && GBL.apflt.isbiquad() ) {
            if( GBL.apflt.Fhi )
                hipass = new Biquad( bq_type_highpass, GBL.apflt.Fhi / meta.srate );
            if( GBL.apflt.Flo )
                lopass = new Biquad( bq_type_lowpass, GBL.apflt.Flo / meta.srate );
        }
        else if( js_out == LF && GBL.lfflt.isbiquad() ) {
            if( GBL.lfflt.Fhi )
                hipass = new Biquad( bq_type_highpass, GBL.lfflt.Fhi / meta.srate );
            if( GBL.lfflt.Flo )
                lopass = new Biquad( bq_type_lowpass, GBL.lfflt.Flo / meta.srate );
        }

        fft.init( fim, meta, js_in, js_out );
    }
}


// Driving loop over files moving input to output.
//
void Pass1::fileLoop()
{
    P1EOF::EOFDAT   Dprev;
    GT_iterator     I;
    int             g, t;

    while( I.next( g, t ) ) {

        // -----------
        // Open binary
        // -----------

        switch( GBL.openInputBinary( i_f, i_fi, g, t, js_in, ip ) ) {
            case 0: break;
            case 1: continue;
            case 2: return;
        }

        // ------------------
        // File size, overlap
        // ------------------

        P1EOF::EOFDAT   Dthis = gP1EOF.getEOFDAT( g, t, js_in, ip );
        qint64          xferBytes;
        int             ret;

        ret     = inputSizeAndOverlap( xferBytes, Dprev, Dthis, g, t );
        Dprev   = Dthis;

        switch( ret ) {

            case 0: break;
            case 1: continue;
            case 2: return;
        }

        // ----------------------------
        // Process and copy binary data
        // ----------------------------

        while( meta.smpToBeWritten < meta.maxOutEOF && xferBytes ) {

            if( !load( xferBytes ) || !push() ) {
                i_f.close();
                return;
            }
        }

        i_f.close();
        meta.pass1_fileDone( g, t, js_out, ip );

        if( meta.smpToBeWritten >= meta.maxOutEOF )
            break;
    }

    flush();

    if( doWrite && vSeg.size() )
        lineFill();
}


// Rather than filling neural traces with zeros, we left-justify in o_buf,
// a parametrized line for each channel (c). The full line endpoints are
// Ya, Yb, separated by Xab points. For times i = [i0, iLim) we generate:
// Ya[c] + i * (Yb[c]-Ya[c])/Xab.
//
// The caller must ensure iLim-i0 < samples(o_buf).
//
// If zeroSY true, write a zero in SY for each i, else preserve SY.
//
void Pass1::line_fill_o_buf(
    const qint16    *Ya,
    const qint16    *Yb,
    qint64          Xab,
    int             i0,
    int             iLim,
    int             nC,
    int             nN,
    bool            zeroSY )
{
    qint16  *d  = &o_buf[0];
    int     nSY = nC - nN;

    for( int i = i0; i < iLim; ++i ) {

        double  dx = double(i) / Xab;

        for( int c = 0; c < nN; ++c )
            *d++ = Ya[c] + dx * (Yb[c] - Ya[c]);

        if( zeroSY ) {
            for( int sy = 0; sy < nSY; ++sy )
                *d++ = 0;
        }
        else
            d += nSY;
    }
}


void Pass1::digital( const qint16 *data, int ntpts )
{
    for( int i = ex0; i < exLim; ++i ) {

        XTR *X = GBL.vX[i];

        if( X->word < meta.nC )
            X->scan( data, meta.smpToBeWritten, ntpts, meta.nC );
    }
}


// Universal output to file for data and zeros.
// Callers send whole samples.
// This function doles out save subsets.
//
bool Pass1::_write( qint64 bytes )
{
    if( svLim > sv0 ) {

        qint64  smp = bytes / meta.smpBytes;

        for( int is = sv0; is < svLim; ++is ) {

            const Save  &S = GBL.vS[is];
            vec_i16     sub;

            Subset::subset( sub, o_buf, S.iKeep, meta.nC );
            bytes = smp * S.smpBytes;

            if( bytes != S.o_f->write( (char*)&sub[0], bytes ) )  {

                Log() << QString("Write failed (error %1); file '%2'.")
                            .arg( S.o_f->error() ).arg( S.o_name );
                return false;
            }
        }
    }
    else if( bytes != o_f.write( o_buf8(), bytes ) )  {

        Log() << QString("Write failed (error %1); file '%2'.")
                    .arg( o_f.error() ).arg( o_name );
        return false;
    }

    return true;
}


bool Pass1::zero( qint64 gapBytes, qint64 zfBytes )
{
    Log() <<
    QString("Gap before file '%1' out_start_smp=%2 inp_gap_smp=%3 out_zeros_smp=%4.")
    .arg( i_fi.fileName() )
    .arg( meta.smpWritten )
    .arg( gapBytes / meta.smpBytes )
    .arg( zfBytes / meta.smpBytes );

    if( zfBytes <= 0 )
        return true;

    if( js_in >= AP && GBL.linefil ) {
        vSeg.push_back(
        LineSeg( meta.smpWritten, zfBytes / meta.smpBytes ) );
    }

    qint64  o_bufBytes = o_buf.size() * sizeof(qint16);

    memset( o_buf8(), 0, o_bufBytes );

    do {

        qint64  cpyBytes = qMin( zfBytes, o_bufBytes );

        if( !_write( cpyBytes ) )
            return false;

        zfBytes         -= cpyBytes;
        meta.smpWritten += cpyBytes / meta.smpBytes;

    } while( zfBytes > 0 );

    return true;
}

/* ---------------------------------------------------------------- */
/* private -------------------------------------------------------- */
/* ---------------------------------------------------------------- */

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
// Overlap is tested based only upon the start and length of
// the two consecutive files in question. This is insensitive
// to any zero filling that may have been applied previously.
// Zero filling between these two is only performed AFTER
// overlap assessment.
//
// Note on EOF tracking:
// In general we (process and) write out everything we read in.
// smpToBeWritten is updated as we read. smpWritten is updated
// when we write. Generally we will read in multiple chunks and
// some margin, then pass that to output one chunk at a time.
// Therefore, smpWritten lags smpToBeWritten. smpToBeWritten and
// zero-filling (virtual input) are directly bounded by maxOutEOF.
//
// Return:
// 0 - ok.
// 1 - skip.
// 2 - fail or done.
//
int Pass1::inputSizeAndOverlap(
    qint64              &xferBytes,
    const P1EOF::EOFDAT &Dprev,
    const P1EOF::EOFDAT &Dthis,
    int                 g,
    int                 t )
{
    int t0, g0 = GBL.gt_get_first( &t0 );

    xferBytes = Dthis.bytes;

    if( g > g0 || t > t0 ) {

        // Not the first file, check overlap

        qint64  olapSmp     = Dprev.smp1st
                                + Dprev.bytes / Dprev.smpBytes
                                - Dthis.smp1st,
                olapBytes   = IBYT(olapSmp),
                fOffset;

        if( olapSmp > 0 ) {

            xferBytes -= olapBytes;

            if( xferBytes < meta.smpBytes ) {
                Log() <<
                    QString("Skipping tiny content"
                    " ([as bytes] olap: %1, rem: %2, perSamp: %3) file '%4'.")
                    .arg( olapBytes )
                    .arg( xferBytes )
                    .arg( meta.smpBytes )
                    .arg( i_fi.fileName() );
                i_f.close();
                return 1;
            }

            if( !i_f.seek( olapBytes ) ) {
                Log() << QString("Seek failed (offset: %1) for file '%2'.")
                            .arg( olapBytes )
                            .arg( i_fi.fileName() );
                i_f.close();
                return 2;
            }

            fOffset = meta.smpWritten + rem() - olapSmp;
        }
        else if( olapSmp < 0 ) {

            // Push any remainder data out.
            //
            // Note that this does not change the gap size used for
            // zeroFilling because that is based on smpToBeWritten.

            if( !flush() )
                return 2;

            if( !meta.pass1_zeroFill( *this, -olapBytes ) )
                return 2;

            fOffset = meta.smpWritten;
        }
        else
            fOffset = meta.smpWritten + rem();

        gFOff.addOffset( fOffset, js_out, ip );
    }
    else if( GBL.startsecs > 0 ) {

        // Offset startsecs into first file

        qint64  S = meta.smpBytes * qint64(GBL.startsecs * meta.srate);

        if( S >= xferBytes ) {
            Log() << QString("Startsecs(s) %1 >= span(s) %2 of file '%3'.")
                        .arg( GBL.startsecs )
                        .arg( xferBytes/meta.smpBytes/meta.srate )
                        .arg( i_fi.fileName() );
            i_f.close();
            return 2;
        }

        if( !i_f.seek( S ) ) {
            Log() << QString("Startsecs seek failed (offset: %1) for file '%2'.")
                        .arg( S )
                        .arg( i_fi.fileName() );
            i_f.close();
            return 2;
        }

        xferBytes -= S;
    }

    return 0;
}


// - Pad LHS as needed.
// - Slide remainder forward.
// - Load from file.
// - Biquad loaded data in place.
// - Call digital() on loaded data.
//
bool Pass1::load( qint64 &xferBytes )
{
// Pad and slide

    if( !i_nxt || i_nxt >= i_lim ) {
        // start fresh, now using value
        // extension instead of zeroing
        //
        // memset( &i_buf[0], 0, IBYT(SZMRG) );
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

// Flip if NXT stream

    if( flip_NXT ) {
        qint16 *data = IBUF(i_lim);
        for( int it = 0; it < ntpts; ++it, data += meta.nC ) {
            // flip neurals
            for( int i = 0; i < meta.nN; ++i )
                data[i] = -data[i];
        }
    }

// ExtendLHS

    if( !i_nxt )
        extendLHS();

// Biquad

    if( hipass )
        hipass->applyBlockwiseMem( IBUF(i_lim), maxInt, ntpts, meta.nC, 0, meta.nN );

    if( lopass )
        lopass->applyBlockwiseMem( IBUF(i_lim), maxInt, ntpts, meta.nC, 0, meta.nN );

// Digital

    digital( IBUF(i_lim), ntpts );

// Indices

    i_nxt  = SZMRG;
    i_lim += ntpts;

    xferBytes           -= bytes;
    meta.smpToBeWritten += ntpts;

    return true;
}


// Copy first loaded sample to
// each position in left margin.
//
void Pass1::extendLHS()
{
    qint16  *src = IBUF(SZMRG),
            *dst = &i_buf[0];
    int     nC   = meta.nC;

    for( int it = 0; it < SZMRG; ++it ) {

        for( int ic = 0; ic < nC; ++ic )
            *dst++ = src[ic];
    }
}


// Process and write neural middles.
//
bool Pass1::push()
{
    while( i_nxt + SZMID + SZMRG <= i_lim ) {

        fft.apply( &o_buf[0], IBUF(i_nxt - SZMRG),
                SZMID, SZFFT,
                meta.nC, meta.nN );

        gfix0 = i_nxt;

        neural( &o_buf[0], SZMID );

        i_nxt += SZMID;

        if( !write( IBYT(SZMID) ) )
            return false;
    }

    return true;
}


int Pass1::rem()
{
    if( !i_nxt || i_nxt >= i_lim )
        return 0;

    return i_lim - i_nxt;
}


// Process and write any neural remainder.
//
bool Pass1::flush()
{
    int ndst = rem(),
        src0 = i_nxt - SZMRG;

    if( ndst <= 0 )
        return true;

    fft.apply( &o_buf[0], IBUF(src0),
            ndst, i_lim - src0,
            meta.nC, meta.nN );

    gfix0 = i_nxt;

    neural( &o_buf[0], ndst );

    i_nxt = 0;
    i_lim = 0;

    return write( IBYT(ndst) );
}


bool Pass1::write( qint64 bytes )
{
    if( doWrite && !_write( bytes ) )
        return false;

    meta.smpWritten += bytes / meta.smpBytes;

    return true;
}


void Pass1::lineFill()
{
    vec_i16 Ya, Yb;

    if( svLim > sv0 ) {

        for( int is = sv0; is < svLim; ++is ) {
            const Save  &S = GBL.vS[is];
            lineFill1( S.o_f, Ya, Yb, S.smpBytes, S.nC, S.nN );
        }
    }
    else
        lineFill1( &o_f, Ya, Yb, meta.smpBytes, meta.nC, meta.nN );
}


void Pass1::lineFill1(
    QFile   *o_f,
    vec_i16 &Ya,
    vec_i16 &Yb,
    qint64  smpBytes,
    int     nC,
    int     nN )
{
    Ya.resize( nC );
    Yb.resize( nC );

    for( int is = 0, ns = vSeg.size(); is < ns; ++is ) {

        const LineSeg   &AB = vSeg[is];

        o_f->seek( smpBytes * (AB.t0 - 1) );
        o_f->read( (char*)&Ya[0], smpBytes );

        o_f->seek( smpBytes * (AB.t0 + AB.len) );
        o_f->read( (char*)&Yb[0], smpBytes );

        qint64  smpRem  = AB.len;
        int     i0      = 0;

        do {
            int smpThis = qMin( smpRem, qint64(SZOBUF) );

            line_fill_o_buf( &Ya[0], &Yb[0], 1 + AB.len,
                i0, i0 + smpThis, nC, nN, true );

            o_f->seek( smpBytes * (AB.t0 + i0) );
            o_f->write( o_buf8(), smpBytes * smpThis );

            i0      += smpThis;
            smpRem  -= smpThis;

        } while( smpRem > 0 );
    }
}


