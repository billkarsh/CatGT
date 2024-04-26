
#include "CGBL.h"
#include "Cmdline.h"
#include "Util.h"
#include "Subset.h"

#include <QDir>
#include <QSet>

#include <math.h>

#define DIGTOL  0.20


/* --------------------------------------------------------------- */
/* Globals ------------------------------------------------------- */
/* --------------------------------------------------------------- */

CGBL    GBL;

/* --------------------------------------------------------------- */
/* GT_iterator --------------------------------------------------- */
/* --------------------------------------------------------------- */

GT_iterator::GT_iterator()
{
    n    = GBL.gtlist.size();
    icur = -1;
}


bool GT_iterator::next( int &g, int &t )
{
    if( n ) {
        if( icur == -1 ) {
            icur = 0;
next_icur:
            ecur = GBL.gtlist[icur];
            g = ecur.g;
            t = ecur.ta;
        }
        else if( ++ecur.ta <= ecur.tb ) {
            g = ecur.g;
            t = ecur.ta;
        }
        else if( ++icur < n )
            goto next_icur;
        else
            return false;
    }
    else {
        if( icur == -1 ) {
            g = ecur.g  = GBL.ga;
            t = ecur.ta = GBL.ta;
            icur = 0;
        }
        else if( ++ecur.ta <= GBL.tb ) {
            g = ecur.g;
            t = ecur.ta;
        }
        else if( ++ecur.g <= GBL.gb ) {
            g = ecur.g;
            t = ecur.ta = GBL.ta;
        }
        else
            return false;
    }

    return true;
}

/* --------------------------------------------------------------- */
/* Filter -------------------------------------------------------- */
/* --------------------------------------------------------------- */

bool Filter::parse( const QString &s )
{
    const QStringList   sl = s.split(
                                QRegExp(","),
                                QString::SkipEmptyParts );
    if( sl.size() != 4 ) {
        Log() << "Filters need 4 arguments: -xxfilter=type,order,Fhi,Flo";
        return false;
    }

    type = sl.at(0);

    if( type != "biquad" && type != "butter" ) {
        Log() << "Only biquad and butter filters are supported.";
        return false;
    }

    order   = sl.at(1).toInt();
    Fhi     = sl.at(2).toDouble();
    Flo     = sl.at(3).toDouble();

    if( isbiquad() )
        order = 2;

    return true;
}

/* --------------------------------------------------------------- */
/* Extractors ---------------------------------------------------- */
/* --------------------------------------------------------------- */

bool XTR::openOutFiles( int g0 )
{
    return openOutTimesFile( g0, ex );
}


void XTR::close() const
{
    if( ts ) {
        ts->flush();
        delete ts;
    }

    if( f )
        delete f;
}


bool XTR::openOutTimesFile( int g0, t_ex ex )
{
    QString file,
            strm;

    switch( js ) {
        case NI:
            file = GBL.niOutFile( g0, ex, this );
            strm = "ni";
            break;
        case OB:
            file = GBL.obOutFile( g0, ip, ex, this );
            strm = QString("obx%1").arg( ip );
            break;
        case AP:
        case LF:
            file = GBL.imOutFile( g0, AP, ip, ex, this );
            strm = QString("imec%1").arg( ip );
            break;
    }

    f = new QFile( file );

    if( !f->open( QIODevice::WriteOnly | QIODevice::Text ) ) {
        Log() << QString("Error opening '%1'.").arg( file );
        return false;
    }

    f->resize( 0 );
    ts = new QTextStream( f );

// fyi entry

    if( usrord == -1 ) {
        GBL.fyi[QString("sync_%1").arg( strm )] = file;
        remapped_ip( -1 );
    }
    else {
        XTR *X0;
        // find first XTR for the stream (i)
        for( int i = 0, n = GBL.vX.size(); i < n; ++i ) {

            X0 = GBL.vX[i];

            if( X0->js == js && X0->ip == ip ) {

                // find this XTR (k)
                for( int k = i; k < n; ++k ) {

                    if( this == GBL.vX[k] ) {
                        k -= i + (X0->usrord == -1);
                        GBL.fyi[QString("times_%1_%2").arg( strm ).arg( k )] = file;
                        remapped_ip( k );
                        break;
                    }
                }
                break;
            }
        }
    }

    return true;
}


// Connect remapped ip to extractions...
//
// Extractions are run on input ip1.
// -save entries remap ip1 to ip2.
//
// openOutTimesFile():
// Original fyi entry already made for ip1:
// - (k=-1) auto-sync: "sync_imec(ip1)=file"
// - (k>=0) times:     "times_imec(ip1)_k=file"
//
// Here we make additional fyi entries:
// - (k=-1) "sync_imec(ip2)=sync_imec(ip1)"
// - (k>=0) "times_imec(ip2)_k=times_imec(ip1)_k"
//
void XTR::remapped_ip( int k )
{
    if( js != AP )
        return;

    QString lhs, rhs;

    if( k < 0 ) {
        lhs = QString("sync_imec%1");
        rhs = GBL.fyi[QString(lhs).arg( ip )].toString();
    }
    else {
        lhs = QString("times_imec%1_") + QString("%1").arg( k );
        rhs = GBL.fyi[QString(lhs).arg( ip )].toString();
    }

    for( int is = 0, ns = GBL.vS.size(); is < ns; ++is ) {

        const Save  &S = GBL.vS[is];

        if( S.js > AP )
            return;
        if( S.js < AP )
            continue;

        if( S.ip1 > ip )
            return;
        if( S.ip1 < ip )
            continue;

        if( S.ip2 == S.ip1 )
            continue;

        GBL.fyi[QString(lhs).arg( S.ip2 )] = rhs;
    }
}


QString Pulse::sSpan() const
{
    QString s = QString("%1").arg( span );
    s.replace( ".", "p" );
    return s;
}


void Pulse::setTolerance( double rate )
{
    srate = rate;

    if( tol >= 0 ) {
        spanlo = (span - tol) * 1e-3 * rate;
        spanhi = (span + tol) * 1e-3 * rate;
    }
    else {
        spanlo = span * (1.0 - DIGTOL) * 1e-3 * rate;
        spanhi = span * (1.0 + DIGTOL) * 1e-3 * rate;
    }
}


QString A_Pulse::sparam() const
{
    if( usrord == -1 )
        return QString();

    QString s = QString(" -%1=%2,%3,%4,%5,%6,%7")
                    .arg( ex == eXA ? "xa" : "xia" )
                    .arg( js ).arg( ip ).arg( word )
                    .arg( thresh ).arg( thrsh2 ).arg( span );

    if( tol >= 0 )
        s += QString(",%1").arg( tol );

    return s;
}


QString A_Pulse::suffix( const QString &stype ) const
{
    return QString(".%1_%2_%3.txt")
            .arg( stype ).arg( word ).arg( sSpan() );
}


void A_Pulse::init( double rate, double rangeMax )
{
    setTolerance( rate );

// assume unity gain

    T = SHRT_MAX * thresh / rangeMax;
    V = SHRT_MAX * thrsh2 / rangeMax;
}


void A_Pulse::pos( const qint16 *data, qint64 t0, int ntpts, int nC )
{
// -------------------
// Must start on a low
// -------------------

    const short *d      = &data[word - nC],
                *dlim   = &data[word + ntpts*nC];

    if( !seek ) {

seek_init:
        nrun = 0;

        while( (d += nC) < dlim ) {

            if( *d < T ) {
                seek = 1;
                goto seek_edge1;
            }
        }

        return;
    }

// --------------------
// Seek edge1 candidate
// --------------------

    if( seek == 1 ) {

seek_edge1:
        while( (d += nC) < dlim ) {

            if( *d >= T ) {

                if( !nrun++ )
                    edge1 = t0 + (d - &data[word]) / nC;

                if( nrun >= GBL.inarow ) {

                    nrun = 0;

                    if( V <= T ) {
                        seek = 2;
                        goto seek_edge2;
                    }

                    seek = 3;
                    peak = 0;
                    goto seek_edge2_pk;
                }

                // Check extended run length
                while( (d += nC) < dlim ) {

                    if( *d >= T ) {

                        if( ++nrun >= GBL.inarow ) {

                            nrun = 0;

                            if( V <= T ) {
                                seek = 2;
                                goto seek_edge2;
                            }

                            seek = 3;
                            peak = 0;
                            goto seek_edge2_pk;
                        }
                    }
                    else {
                        nrun = 0;
                        break;
                    }
                }
            }
        }

        return;
    }

// --------------------
// Seek edge2 candidate
// --------------------

    if( seek == 2 ) {

seek_edge2:
        if( span == 0 )
            goto report;

        while( (d += nC) < dlim ) {

            if( *d < T ) {

                if( !nrun++ )
                    edge2 = t0 + (d - &data[word]) / nC;

                if( nrun >= GBL.inarow )
                    goto report;

                // Check extended run length
                while( (d += nC) < dlim ) {

                    if( *d < T ) {

                        if( ++nrun >= GBL.inarow )
                            goto report;
                    }
                    else {
                        nrun = 0;
                        break;
                    }
                }
            }
        }

        return;
    }

// -----------------------------------
// Seek edge2 candidate + thrsh2 check
// -----------------------------------

    if( seek == 3 ) {

seek_edge2_pk:
        while( (d += nC) < dlim ) {

            if( *d > peak )
                peak = *d;

            if( *d < T ) {

                if( !nrun++ )
                    edge2 = t0 + (d - &data[word]) / nC;

                if( nrun >= GBL.inarow )
                    goto report;

                // Check extended run length
                while( (d += nC) < dlim ) {

                    if( *d < T ) {

                        if( ++nrun >= GBL.inarow )
                            goto report;
                    }
                    else {
                        nrun = 0;
                        break;
                    }
                }
            }
        }

        return;
    }

// -----------------
// Report edge1 time
// -----------------

report:
    if( seek == 3 && peak < V ) {
        seek = 0;
        goto seek_init;
    }

    seek = 0;

    if( !span ) {
write:
        *ts << QString("%1\n").arg( edge1 / srate, 0, 'f', 6 );
    }
    else {
        qint64  span = edge2 - edge1;

        if( span >= spanlo && span <= spanhi )
            goto write;
    }

    goto seek_init;
}


void A_Pulse::inv( const qint16 *data, qint64 t0, int ntpts, int nC )
{
// --------------------
// Must start on a high
// --------------------

    const short *d      = &data[word - nC],
                *dlim   = &data[word + ntpts*nC];

    if( !seek ) {

seek_init:
        nrun = 0;

        while( (d += nC) < dlim ) {

            if( *d >= T ) {
                seek = 1;
                goto seek_edge1;
            }
        }

        return;
    }

// --------------------
// Seek edge1 candidate
// --------------------

    if( seek == 1 ) {

seek_edge1:
        while( (d += nC) < dlim ) {

            if( *d < T ) {

                if( !nrun++ )
                    edge1 = t0 + (d - &data[word]) / nC;

                if( nrun >= GBL.inarow ) {

                    nrun = 0;

                    if( V >= T ) {
                        seek = 2;
                        goto seek_edge2;
                    }

                    seek = 3;
                    peak = 0;
                    goto seek_edge2_pk;
                }

                // Check extended run length
                while( (d += nC) < dlim ) {

                    if( *d < T ) {

                        if( ++nrun >= GBL.inarow ) {

                            nrun = 0;

                            if( V >= T ) {
                                seek = 2;
                                goto seek_edge2;
                            }

                            seek = 3;
                            peak = 0;
                            goto seek_edge2_pk;
                        }
                    }
                    else {
                        nrun = 0;
                        break;
                    }
                }
            }
        }

        return;
    }

// --------------------
// Seek edge2 candidate
// --------------------

    if( seek == 2 ) {

seek_edge2:
        if( span == 0 )
            goto report;

        while( (d += nC) < dlim ) {

            if( *d >= T ) {

                if( !nrun++ )
                    edge2 = t0 + (d - &data[word]) / nC;

                if( nrun >= GBL.inarow )
                    goto report;

                // Check extended run length
                while( (d += nC) < dlim ) {

                    if( *d >= T ) {

                        if( ++nrun >= GBL.inarow )
                            goto report;
                    }
                    else {
                        nrun = 0;
                        break;
                    }
                }
            }
        }

        return;
    }

// -----------------------------------
// Seek edge2 candidate + thrsh2 check
// -----------------------------------

    if( seek == 3 ) {

seek_edge2_pk:
        while( (d += nC) < dlim ) {

            if( *d < peak )
                peak = *d;

            if( *d >= T ) {

                if( !nrun++ )
                    edge2 = t0 + (d - &data[word]) / nC;

                if( nrun >= GBL.inarow )
                    goto report;

                // Check extended run length
                while( (d += nC) < dlim ) {

                    if( *d >= T ) {

                        if( ++nrun >= GBL.inarow )
                            goto report;
                    }
                    else {
                        nrun = 0;
                        break;
                    }
                }
            }
        }

        return;
    }

// -----------------
// Report edge1 time
// -----------------

report:
    if( seek == 3 && peak > V ) {
        seek = 0;
        goto seek_init;
    }

    seek = 0;

    if( !span ) {
write:
        *ts << QString("%1\n").arg( edge1 / srate, 0, 'f', 6 );
    }
    else {
        qint64  span = edge2 - edge1;

        if( span >= spanlo && span <= spanhi )
            goto write;
    }

    goto seek_init;
}


void A_Pulse::scan( const qint16 *data, qint64 t0, int ntpts, int nC )
{
    if( ex == eXA )
        pos( data, t0, ntpts, nC );
    else
        inv( data, t0, ntpts, nC );
}


QString D_Pulse::sparam() const
{
    if( usrord == -1 )
        return QString();

    QString s = QString(" -%1=%2,%3,%4,%5,%6")
                    .arg( ex == eXD ? "xd" : "xid" )
                    .arg( js ).arg( ip ).arg( word )
                    .arg( bit ).arg( span );

    if( tol >= 0 )
        s += QString(",%1").arg( tol );

    return s;
}


QString D_Pulse::suffix( const QString &stype ) const
{
    return QString(".%1_%2_%3_%4.txt")
            .arg( stype ).arg( word ).arg( bit ).arg( sSpan() );
}


void D_Pulse::init( double rate, double rangeMax )
{
    Q_UNUSED( rangeMax )

    setTolerance( rate );
}


void D_Pulse::pos( const qint16 *data, qint64 t0, int ntpts, int nC )
{
// -------------------
// Must start on a low
// -------------------

    const short *d      = &data[word - nC],
                *dlim   = &data[word + ntpts*nC];

    if( !seek ) {

seek_init:
        nrun = 0;

        while( (d += nC) < dlim ) {

            if( !((*d >> bit) & 1) ) {
                seek = 1;
                goto seek_edge1;
            }
        }

        return;
    }

// --------------------
// Seek edge1 candidate
// --------------------

    if( seek == 1 ) {

seek_edge1:
        while( (d += nC) < dlim ) {

            if( (*d >> bit) & 1 ) {

                if( !nrun++ )
                    edge1 = t0 + (d - &data[word]) / nC;

                if( nrun >= GBL.inarow ) {
                    seek = 2;
                    nrun = 0;
                    goto seek_edge2;
                }

                // Check extended run length
                while( (d += nC) < dlim ) {

                    if( (*d >> bit) & 1 ) {

                        if( ++nrun >= GBL.inarow ) {
                            seek = 2;
                            nrun = 0;
                            goto seek_edge2;
                        }
                    }
                    else {
                        nrun = 0;
                        break;
                    }
                }
            }
        }

        return;
    }

// --------------------
// Seek edge2 candidate
// --------------------

    if( seek == 2 ) {

seek_edge2:
        if( span == 0 )
            goto report;

        while( (d += nC) < dlim ) {

            if( !((*d >> bit) & 1) ) {

                if( !nrun++ )
                    edge2 = t0 + (d - &data[word]) / nC;

                if( nrun >= GBL.inarow )
                    goto report;

                // Check extended run length
                while( (d += nC) < dlim ) {

                    if( !((*d >> bit) & 1) ) {

                        if( ++nrun >= GBL.inarow )
                            goto report;
                    }
                    else {
                        nrun = 0;
                        break;
                    }
                }
            }
        }

        return;
    }

// -----------------
// Report edge1 time
// -----------------

report:
    seek = 0;

    if( !span ) {
write:
        *ts << QString("%1\n").arg( edge1 / srate, 0, 'f', 6 );
    }
    else {
        qint64  span = edge2 - edge1;

        if( span >= spanlo && span <= spanhi )
            goto write;
    }

    goto seek_init;
}


void D_Pulse::inv( const qint16 *data, qint64 t0, int ntpts, int nC )
{
// --------------------
// Must start on a high
// --------------------

    const short *d      = &data[word - nC],
                *dlim   = &data[word + ntpts*nC];

    if( !seek ) {

seek_init:
        nrun = 0;

        while( (d += nC) < dlim ) {

            if( (*d >> bit) & 1 ) {
                seek = 1;
                goto seek_edge1;
            }
        }

        return;
    }

// --------------------
// Seek edge1 candidate
// --------------------

    if( seek == 1 ) {

seek_edge1:
        while( (d += nC) < dlim ) {

            if( !((*d >> bit) & 1) ) {

                if( !nrun++ )
                    edge1 = t0 + (d - &data[word]) / nC;

                if( nrun >= GBL.inarow ) {
                    seek = 2;
                    nrun = 0;
                    goto seek_edge2;
                }

                // Check extended run length
                while( (d += nC) < dlim ) {

                    if( !((*d >> bit) & 1) ) {

                        if( ++nrun >= GBL.inarow ) {
                            seek = 2;
                            nrun = 0;
                            goto seek_edge2;
                        }
                    }
                    else {
                        nrun = 0;
                        break;
                    }
                }
            }
        }

        return;
    }

// --------------------
// Seek edge2 candidate
// --------------------

    if( seek == 2 ) {

seek_edge2:
        if( span == 0 )
            goto report;

        while( (d += nC) < dlim ) {

            if( (*d >> bit) & 1 ) {

                if( !nrun++ )
                    edge2 = t0 + (d - &data[word]) / nC;

                if( nrun >= GBL.inarow )
                    goto report;

                // Check extended run length
                while( (d += nC) < dlim ) {

                    if( (*d >> bit) & 1 ) {

                        if( ++nrun >= GBL.inarow )
                            goto report;
                    }
                    else {
                        nrun = 0;
                        break;
                    }
                }
            }
        }

        return;
    }

// -----------------
// Report edge1 time
// -----------------

report:
    seek = 0;

    if( !span ) {
write:
        *ts << QString("%1\n").arg( edge1 / srate, 0, 'f', 6 );
    }
    else {
        qint64  span = edge2 - edge1;

        if( span >= spanlo && span <= spanhi )
            goto write;
    }

    goto seek_init;
}


void D_Pulse::scan( const qint16 *data, qint64 t0, int ntpts, int nC )
{
    if( ex == eXD )
        pos( data, t0, ntpts, nC );
    else
        inv( data, t0, ntpts, nC );
}


QString BitField::sparam() const
{
    return QString(" -bf=%1,%2,%3,%4,%5,%6")
            .arg( js ).arg( ip ).arg( word )
            .arg( b0 ).arg( nb ).arg( inarow );
}


QString BitField::suffix( const QString &stype ) const
{
    return QString(".%1_%2_%3_%4.txt")
            .arg( stype ).arg( word ).arg( b0 ).arg( nb );
}


void BitField::init( double rate, double rangeMax )
{
    Q_UNUSED( rangeMax )

    srate = rate;

// mask = 2^nb - 1

    mask = 1;
    for( int i = 0; i < nb; ++i )
        mask *= 2;
    --mask;
}


bool BitField::openOutFiles( int g0 )
{
    if( !openOutTimesFile( g0, eBFT ) )
        return false;

    QString file;

    switch( js ) {
        case NI: file = GBL.niOutFile( g0, eBFV, this ); break;
        case OB: file = GBL.obOutFile( g0, ip, eBFV, this ); break;
        case AP:
        case LF: file = GBL.imOutFile( g0, AP, ip, eBFV, this ); break;
    }

    fv = new QFile( file );

    if( !fv->open( QIODevice::WriteOnly | QIODevice::Text ) ) {
        Log() << QString("Error opening '%1'.").arg( file );
        return false;
    }

    fv->resize( 0 );
    tsv = new QTextStream( fv );
    return true;
}


void BitField::scan( const qint16 *data, qint64 t0, int ntpts, int nC )
{
    const short *d = &data[word];

    for( int it = 0; it < ntpts; ++it, d += nC ) {

        uint v = (uint(*d) >> b0) & mask;

        if( v != vlast ) {

            if( inarow == 1 ) {
                // report now
                *ts  << QString("%1\n").arg( (t0 + it) / srate, 0, 'f', 6 );
                *tsv << QString("%1\n").arg( v );
                vlast = v;
            }
            else {
                // require inarow copies
                if( !nrun++ ) {
                    // new run
                    vrun = v;
                }
                else if( v != vrun ) {
                    // reset run count
                    nrun = 1;
                    vrun = v;
                }
                else if( nrun >= inarow ) {
                    // report start of run
                    *ts  << QString("%1\n").arg( (t0 + it + 1 - nrun) / srate, 0, 'f', 6 );
                    *tsv << QString("%1\n").arg( v );
                    vlast = v;
                    nrun  = 0;
                }
            }
        }
        else
            nrun = 0;
    }
}


void BitField::close() const
{
    if( tsv ) {
        tsv->flush();
        delete tsv;
    }

    if( fv )
        delete fv;

    XTR::close();
}

/* --------------------------------------------------------------- */
/* Save ---------------------------------------------------------- */
/* --------------------------------------------------------------- */

bool Save::parse( const char *s )
{
    int js, ip1, ip2, n;

    if( 3 > sscanf( s, "%d,%d,%d%n", &js, &ip1, &ip2, &n ) ) {
        Log() << "Save options need 4 arguments: -save=js,ip1,ip2,chan-list";
        return false;
    }

    if( ip2 < 0 ) {
        Log() << "Save option ip2 cannot be negative.";
        return false;
    }

// trim leading "   ,   "

    QString C( s + n );
    n = 0;
    for( int i = 0, sz = C.size(); i < sz; ++i ) {
        if( C[i] == ' ' || C[i] == ',' )
            ++n;
        else
            break;
    }
    if( n )
        C.remove( 0, n );

    Save    S( t_js(js), ip1, ip2, C );

    if( Subset::isAllChansStr( C ) )
        Log() << "Skipping directive to save all:" << S.sparam();
    else
        GBL.vS.push_back( S );

    return true;
}


QString Save::sparam() const
{
    return QString(" -save=%1,%2,%3,%4")
            .arg( js ).arg( ip1 ).arg( ip2 ).arg( sUsr );
}


bool Save::init( const KVParams &kvp, const QFileInfo &fim, int theZ )
{
// Reinit these for summing: In AP2LF mode the record is used twice
    iKeep.clear();
    nN = 0;

    QVector<uint>   snsFileChans, cUsr, cUsr2;

    if( !Subset::rngStr2Vec( cUsr, sUsr ) ) {
        Log() << QString("Bad channel-list format:%1").arg( sparam() );
        return false;
    }

    if( !GBL.getSavedChannels( snsFileChans, kvp, fim ) )
        return false;

    const QStringList   sl = kvp["acqApLfSy"].toString().split(
                                QRegExp("^\\s+|\\s*,\\s*"),
                                QString::SkipEmptyParts );
    int nAP = sl[0].toInt(),
        cSY = nAP + sl[1].toInt();

    if( js == AP )
        nAP = 0;    // offset channel indices for bitsAP test

    for( int ic = 0, nU = cUsr.size(); ic < nU; ++ic ) {

        int cU  = cUsr[ic],
            idx = snsFileChans.indexOf( cU );

        if( idx >= 0 && (theZ < 0 || GBL.vMZ[theZ].bitsAP.testBit( cU - nAP ))  ) {
            cUsr2.push_back( cU );
            iKeep.push_back( idx );
            nN += (cU < cSY);
        }
    }

    nC = iKeep.size();

    if( !nC ) {
        Log() << QString("Specified channels not in file:%1").arg( sparam() );
        return false;
    }

    sUsr_out    = Subset::vec2RngStr( cUsr2 );
    smpBytes    = nC*sizeof(qint16);
    return true;
}


bool Save::o_open( int g0, t_js js )
{
    o_f = new QFile;
    return GBL.openOutputBinary( *o_f, o_name, g0, js, ip2 );
}


void Save::close()
{
    if( o_f ) {
        delete o_f;
        o_f = 0;
    }
}

/* --------------------------------------------------------------- */
/* SepShanks ----------------------------------------------------- */
/* --------------------------------------------------------------- */

// Now, just check for bad params.
// Parse again later when metadata available.
//
bool SepShanks::parse( QSet<int> &seen )
{
    QStringList sl = sUsr.split(
                        QRegExp("^\\s+|\\s*,\\s*|\\s+$"),
                        QString::SkipEmptyParts );
    int         ns = sl.size();

    if( ns != 5 ) {
        Log() << QString("Error: -sepShanks=%1 has bad format.").arg( sUsr );
        return false;
    }

    ip = sl[0].toInt();

    for( int j = 0; j < 4; ++j )
        ipj[j] = sl[j+1].toInt();

    if( ip < 0 ) {
        Log() << QString("Error: -sepShanks=%1 has negative ip.").arg( sUsr );
        return false;
    }

    if( (ipj[0] == ip) + (ipj[1] == ip) + (ipj[2] == ip) + (ipj[3] == ip) > 1 ) {
        Log() << QString("Error: -sepShanks=%1 more than one ipj = ip.").arg( sUsr );
        return false;
    }

    if( (ipj[0] < 0) && (ipj[1] < 0) && (ipj[2] < 0) && (ipj[3] < 0) ) {
        Log() << QString("Error: -sepShanks=%1 all ipj negative.").arg( sUsr );
        return false;
    }

    if( (ipj[1] == ipj[0]) || (ipj[2] == ipj[0]) || (ipj[3] == ipj[0]) ||
        (ipj[2] == ipj[1]) || (ipj[3] == ipj[1]) || (ipj[3] == ipj[2]) ) {
        Log() << QString("Error: -sepShanks=%1 duplicate ipj.").arg( sUsr );
        return false;
    }

    if( seen.contains( ip ) ) {
        Log() << QString("Error: -sepShanks names probe %1 twice.").arg( ip );
        return false;
    }

    seen.insert( ip );

    return true;
}


QString SepShanks::sparam() const
{
    return QString(" -sepShanks=%1").arg( sUsr );
}


bool SepShanks::split( const KVParams &kvp, const QFileInfo &fim )
{
// already used?

    if( ip < 0 )
        return true;

// Prep imro

    IMROTbl *R = GBL.getProbe( kvp );
    if( !R ) {
        Log() << QString("Can't identify probe type in metadata '%1'.")
                    .arg( fim.fileName() );
        return false;
    }

    if( R->nShank() == 1 ) {
        ip = -1;
        delete R;
        return true;
    }

    R->fromString( 0, kvp["~imroTbl"].toString() );

    int nAP = R->nAP(),
        nSY = R->nSY();

// Prep saved chan list

    QVector<uint>   snsFileChans;
    if( !GBL.getSavedChannels( snsFileChans, kvp, fim ) ) {
        delete R;
        return false;
    }

// Prep shank lists

    QBitArray   K[4];

    for( int j = 0; j < 4; ++j )
        K[j].resize( nAP + nSY );

// Fill

    for( int ic = 0, nc = snsFileChans.size(); ic < nc; ++ic ) {

        int C = snsFileChans[ic];

        if( C < nAP )
            K[R->shnk( C )].setBit( C );
        else if( nSY == 1 ) {
            // each shank gets common SY
            for( int j = 0; j < 4; ++j )
                K[j].setBit( C );
        }
        else {
            // each shank gets own SY
            K[C - nAP].setBit( C );
        }
    }

// Split

    for( int j = 0; j < 4; ++j ) {

        if( ipj[j] < 0 )
            continue;

        QBitArray   B = K[j];
        B.truncate( nAP );
        if( !B.count( true ) )
            continue;

        QString arg = QString("2,%1,%2,%3")
                        .arg( ip ).arg( ipj[j] )
                        .arg( Subset::bits2RngStr( K[j] ) );

        Save::parse( STR2CHR( arg ) );
        qSort( GBL.vS );
    }

    ip = -1;
    delete R;
    return true;
}

/* --------------------------------------------------------------- */
/* MaxZ ---------------------------------------------------------- */
/* --------------------------------------------------------------- */

// Now, just check for bad params.
// Parse again later when metadata available.
//
bool MaxZ::parse( QSet<int> &seen )
{
    QStringList sl = sUsr.split(
                        QRegExp("^\\s+|\\s*,\\s*|\\s+$"),
                        QString::SkipEmptyParts );
    int         ns = sl.size();

    if( ns != 3 ) {
        Log() << QString("Error: -maxZ=%1 has bad format.").arg( sUsr );
        return false;
    }

    ip   = sl[0].toInt();
    type = sl[1].toInt();
    z    = sl[2].toDouble();

    if( type < 0 || type > 2 ) {
        Log() << QString("Error: -maxZ=%1 has bad type value.").arg( sUsr );
        return false;
    }

    if( seen.contains( ip ) ) {
        Log() << QString("Error: -maxZ names probe %1 twice.").arg( ip );
        return false;
    }

    seen.insert( ip );

    return true;
}


QString MaxZ::sparam() const
{
    return QString(" -maxZ=%1").arg( sUsr );
}


bool MaxZ::apply(
    const KVParams  &kvp,
    const QFileInfo &fim,
    int             js_in,
    int             js_out )
{
// Form bitsAP

    int nAP, nLF, nSY;

    IMROTbl *R = GBL.getProbe( kvp );
    if( !R ) {
        Log() << QString("Can't identify probe type in metadata '%1'.")
                    .arg( fim.fileName() );
        return false;
    }
    R->fromString( 0, kvp["~imroTbl"].toString() );

    nAP = R->nAP();
    nLF = R->nLF();
    nSY = R->nSY();

    bitsAP.resize( nAP );

    int maxRow; // inclusive
    switch( type ) {
        case 0:
            maxRow = int(z);
            break;
        case 1:
            maxRow =
            ceil( (z - R->zPitch()) / R->zPitch() );
            break;
        default:
            maxRow =
            ceil( (z - R->tipLength() - R->zPitch()) / R->zPitch() );
    }
    maxRow = qBound( 0, maxRow, R->nRow() - 1 );

    for( int ic = 0; ic < nAP; ++ic ) {
        int col, row;
        R->elShankColRow( col, row, ic );
        if( row <= maxRow )
            bitsAP.setBit( ic );
    }

    delete R;

    // Create/modify AP chnexcl list

    if( js_out == AP ) {

        QBitArray   bexc = ~bitsAP;

        if( bexc.count( true ) ) {

            if( GBL.mexc.contains( ip ) ) {

                Subset::rngStr2Vec( GBL.mexc[ip],
                    Subset::vec2RngStr( GBL.mexc[ip] )
                    + ","
                    + Subset::bits2RngStr( bexc ) );
            }
            else
                Subset::bits2Vec( GBL.mexc[ip], bexc );
        }
    }

    // Add Save record if doesn't already exist.
    // This is a simple save-all directive that
    // will be edited later.

    foreach( const Save &S, GBL.vS ) {
        if( S.js == js_in && S.ip1 == ip )
            return true;
    }

    QString sNU;
    if( js_in == AP )
        sNU = QString("0:%1").arg( nAP - 1 );
    else
        sNU = QString("%1:%2").arg( nAP ).arg( nAP + nLF - 1 );

    QString sSY = QString("%1").arg( nAP + nLF );
    if( nSY > 1 )
        sSY += QString(":%1").arg( nAP + nLF + nSY - 1 );

    QString arg = QString("%1,%2,%2,%3,%4")
                    .arg( js_in ).arg( ip )
                    .arg( sNU ).arg( sSY );

    Save::parse( STR2CHR( arg ) );
    qSort( GBL.vS );

    return true;
}

/* --------------------------------------------------------------- */
/* Elem ---------------------------------------------------------- */
/* --------------------------------------------------------------- */

void Elem::unpack()
{
    GBL.inpar           = dir;
    GBL.run             = run;
    GBL.ga              = g;
    GBL.gb              = g;
    GBL.no_run_fld      = no_run_fld;
    GBL.in_catgt_fld    = in_catgt_fld;
}

/* --------------------------------------------------------------- */
/* PrintUsage  --------------------------------------------------- */
/* --------------------------------------------------------------- */

static void PrintUsage()
{
    Log();
    Log() << "*** ERROR: MISSING CRITICAL PARAMETERS ***";
    Log() << "------------------------";
    Log() << "Purpose:";
    Log() << "+ Optionally join trials with given run_name and index ranges [ga,gb] [ta,tb]...";
    Log() << "+ ...Or run on any individual file.";
    Log() << "+ Optionally apply demultiplexing corrections.";
    Log() << "+ Optionally apply band-pass and global CAR filters.";
    Log() << "+ Optionally edit out saturation artifacts.";
    Log() << "+ Optionally extract tables of sync waveform edge times to drive TPrime.";
    Log() << "+ Optionally extract tables of other nonneural event times to be aligned with spikes.";
    Log() << "+ Optionally join the above outputs across different runs (supercat feature).\n";
    Log() << "Output:";
    Log() << "+ Results are placed next to source, named like this, with t-index = tcat:";
    Log() << "+    path/run_name_g5_tcat.imec1.ap.bin.";
    Log() << "+ Errors and run messages are appended to CatGT.log in the current working directory.\n";
    Log() << "Usage:";
    Log() << ">CatGT -dir=data_dir -run=run_name -g=ga,gb -t=ta,tb <which streams> [ options ]\n";
    Log() << "Which streams:";
    Log() << "-ni                      ;required to process ni stream";
    Log() << "-ob                      ;required to process ob streams";
    Log() << "-ap                      ;required to process ap streams";
    Log() << "-lf                      ;required to process lf streams";
    Log() << "-obx=0,3:5               ;if -ob process these OneBoxes";
    Log() << "-prb_3A                  ;if -ap or -lf process 3A-style probe files, e.g., run_name_g0_t0.imec.ap.bin";
    Log() << "-prb=0,3:5               ;if -ap or -lf AND !prb_3A process these probes\n";
    Log() << "Options:";
    Log() << "-no_run_fld              ;older data, or data files relocated without a run folder";
    Log() << "-prb_fld                 ;use folder-per-probe organization";
    Log() << "-prb_miss_ok             ;instead of stopping, silently skip missing probes";
    Log() << "-gtlist={gj,tja,tjb}     ;override {-g,-t} giving each listed g-index its own t-range";
    Log() << "-t=cat                   ;extract events from CatGT output files (instead of -t=ta,tb)";
    Log() << "-exported                ;apply FileViewer 'exported' tag to in/output filenames";
    Log() << "-t_miss_ok               ;instead of stopping, zero-fill if trial missing";
    Log() << "-zerofillmax=500         ;set a maximum zero-fill span (millisec)";
    Log() << "-startsecs=120.0         ;skip this initial span of each input stream (float seconds)";
    Log() << "-maxsecs=7.5             ;set a maximum output file length (float seconds)";
    Log() << "-apfilter=Typ,N,Fhi,Flo  ;apply ap band-pass filter of given {type, order, corners(float Hz)}";
    Log() << "-lffilter=Typ,N,Fhi,Flo  ;apply lf band-pass filter of given {type, order, corners(float Hz)}";
    Log() << "-no_tshift               ;DO NOT time-align channels to account for ADC multiplexing";
    Log() << "-loccar_um=40,140        ;apply ap local CAR annulus (exclude radius, include radius)";
    Log() << "-loccar=2,8              ;apply ap local CAR annulus (exclude radius, include radius)";
    Log() << "-gblcar                  ;apply ap global CAR filter over all channels";
    Log() << "-gbldmx                  ;apply ap global demuxed CAR filter over channel groups";
    Log() << "-gfix=0.40,0.10,0.02     ;rmv ap artifacts: ||amp(mV)||, ||slope(mV/sample)||, ||noise(mV)||";
    Log() << "-chnexcl={prb;chans}     ;this probe, exclude listed chans from ap loccar, gblcar, gfix";
    Log() << "-xa=0,0,2,3.0,4.5,25     ;extract pulse signal from analog chan (js,ip,word,thresh1(V),thresh2(V),millisec)";
    Log() << "-xd=2,0,384,6,500        ;extract pulse signal from digital chan (js,ip,word,bit,millisec)";
    Log() << "-xia=0,0,2,3.0,4.5,2     ;inverted version of xa";
    Log() << "-xid=2,0,384,6,50        ;inverted version of xd";
    Log() << "-bf=0,0,8,2,4,3          ;extract numeric bit-field from digital chan (js,ip,word,startbit,nbits,inarow)";
    Log() << "-inarow=5                ;extractor {xa,xd,xia,xid} antibounce stay high/low sample count";
    Log() << "-no_auto_sync            ;disable the automatic extraction of sync edges in all streams";
    Log() << "-save=2,0,5,20:60        ;save subset of probe chans (js,ip1,ip2,chan-list)";
    Log() << "-sepShanks=0,0,1,2,-1    ;save each shank in sep file (ip,ip0,ip1,ip2,ip3)";
    Log() << "-maxZ=0,0,100            ;probe inserted to given depth (ip,depth-type,depth-value)";
    Log() << "-pass1_force_ni_ob_bin   ;write pass one ni/ob binary tcat file even if not changed";
    Log() << "-supercat={dir,run_ga}   ;concatenate existing output files across runs (see ReadMe)";
    Log() << "-supercat_trim_edges     ;supercat after trimming each stream to matched sync edges";
    Log() << "-supercat_skip_ni_ob_bin ;do not supercat ni/ob binary files";
    Log() << "-dest=path               ;alternate path for output files (must exist)";
    Log() << "-no_catgt_fld            ;if using -dest, do not create catgt_run subfolder";
    Log() << "-out_prb_fld             ;if using -dest, create output subfolder per probe";
    Log() << "------------------------";
}

/* ---------------------------------------------------------------- */
/* CGBL ----------------------------------------------------------- */
/* ---------------------------------------------------------------- */

bool CGBL::SetCmdLine( int argc, char* argv[] )
{
// Parse args

    QString     ssupercat;
    const char  *sarg   = 0;
    int         usrord  = 0;

    for( int i = 1; i < argc; ++i ) {

        std::vector<double> vd;
        std::vector<int>    vi;

        if( GetArgStr( sarg, "-dir=", argv[i] ) )
            inpar = trim_adjust_slashes( sarg );
        else if( GetArgStr( sarg, "-run=", argv[i] ) )
            run = sarg;
        else if( GetArgStr( sarg, "-gtlist=", argv[i] ) ) {

            if( !gt_parse_list( sarg ) )
                return false;
        }
        else if( GetArgList( vi, "-g=", argv[i] ) ) {

            switch( vi.size() ) {
                case 2:
                    ga = vi[0];
                    gb = vi[1];
                    break;
                case 1:
                    gb = ga = vi[0];
                    break;
                default:
                    goto bad_param;
            }
        }
        else if( GetArgStr( sarg, "-t=", argv[i] ) ) {

            if( 0 == strcmp( sarg, "cat" ) )
                gt_set_tcat();
            else if( GetArgList( vi, "-t=", argv[i] ) ) {

                switch( vi.size() ) {
                    case 2:
                        ta = vi[0];
                        tb = vi[1];
                        break;
                    case 1:
                        tb = ta = vi[0];
                        break;
                    default:
                        goto bad_param;
                }
            }
            else
                goto bad_param;
        }
        else if( IsArg( "-no_run_fld", argv[i] ) )
            no_run_fld = true;
        else if( IsArg( "-prb_fld", argv[i] ) )
            prb_fld = true;
        else if( IsArg( "-prb_miss_ok", argv[i] ) )
            prb_miss_ok = true;
        else if( IsArg( "-exported", argv[i] ) )
            exported = true;
        else if( IsArg( "-t_miss_ok", argv[i] ) )
            t_miss_ok = true;
        else if( IsArg( "-ni", argv[i] ) )
            ni = true;
        else if( IsArg( "-ob", argv[i] ) )
            ob = true;
        else if( IsArg( "-ap", argv[i] ) )
            ap = true;
        else if( IsArg( "-lf", argv[i] ) )
            lf = true;
        else if( GetArgStr( sarg, "-obx=", argv[i] ) ) {

            if( !Subset::rngStr2Vec( vobx, sarg ) )
                goto bad_param;
        }
        else if( GetArgStr( sarg, "-prb=", argv[i] ) ) {

            if( !Subset::rngStr2Vec( vprb, sarg ) )
                goto bad_param;
        }
        else if( IsArg( "-prb_3A", argv[i] ) )
            prb_3A = true;
        else if( GetArg( &zfilmax, "-zerofillmax=%d", argv[i] ) )
            ;
        else if( GetArg( &startsecs, "-startsecs=%lf", argv[i] ) )
            ;
        else if( GetArg( &maxsecs, "-maxsecs=%lf", argv[i] ) )
            ;
        else if( GetArgStr( sarg, "-apfilter=", argv[i] ) ) {
            if( !apflt.parse( sarg ) )
                return false;
        }
        else if( GetArgStr( sarg, "-lffilter=", argv[i] ) ) {
            if( !lfflt.parse( sarg ) )
                return false;
        }
        else if( GetArgList( vd, "-loccar_um=", argv[i] ) && vd.size() == 2 ) {

            locin_um  = vd[0];
            locout_um = vd[1];

            if( locin_um < 10 || locin_um >= locout_um ) {
                Log() <<
                    QString("Bad loccar_um parameters: -loccar_um=%1,%2.")
                    .arg( locin_um ).arg( locout_um );
                return false;
            }
        }
        else if( GetArgList( vi, "-loccar=", argv[i] ) && vi.size() == 2 ) {

            locin  = vi[0];
            locout = vi[1];

            if( locin < 1 || locin >= locout ) {
                Log() <<
                    QString("Bad loccar parameters: -loccar=%1,%2.")
                    .arg( locin ).arg( locout );
                return false;
            }
        }
        else if( GetArg( &inarow, "-inarow=%d", argv[i] ) )
            ;
        else if( IsArg( "-no_tshift", argv[i] ) )
            tshift = false;
        else if( IsArg( "-gblcar", argv[i] ) )
            gblcar = true;
        else if( IsArg( "-gbldmx", argv[i] ) )
            gbldmx = true;
        else if( GetArgList( vd, "-gfix=", argv[i] ) && vd.size() == 3 ) {

            gfixamp = vd[0];
            gfixslp = vd[1];
            gfixbas = vd[2];
            gfixdo  = true;

            if( gfixamp < 0 )
                gfixamp = -gfixamp;

            if( gfixslp < 0 )
                gfixslp = -gfixslp;
        }
        else if( GetArgStr( sarg, "-chnexcl=", argv[i] ) ) {

            if( !parseChnexcl( sarg ) )
                return false;
        }
        else if( GetArgList( vd, "-xa=", argv[i] )
                && (vd.size() == 6 || vd.size() == 7) ) {

            A_Pulse *X = new A_Pulse;
            X->ex       = eXA;
            X->usrord   = usrord++;
            X->js       = t_js(vd[0]);
            X->ip       = vd[1];
            X->word     = vd[2];
            X->thresh   = vd[3];
            X->thrsh2   = vd[4];
            X->span     = vd[5];

            if( vd.size() == 7 )
                X->tol = vd[6];

            vX.push_back( X );
        }
        else if( GetArgList( vd, "-xia=", argv[i] )
                && (vd.size() == 6 || vd.size() == 7) ) {

            A_Pulse *X = new A_Pulse;
            X->ex       = eXIA;
            X->usrord   = usrord++;
            X->js       = t_js(vd[0]);
            X->ip       = vd[1];
            X->word     = vd[2];
            X->thresh   = vd[3];
            X->thrsh2   = vd[4];
            X->span     = vd[5];

            if( vd.size() == 7 )
                X->tol = vd[6];

            vX.push_back( X );
        }
        else if( GetArgList( vd, "-xd=", argv[i] )
                && (vd.size() == 5 || vd.size() == 6) ) {

            D_Pulse *X = new D_Pulse;
            X->ex       = eXD;
            X->usrord   = usrord++;
            X->js       = t_js(vd[0]);
            X->ip       = vd[1];
            X->word     = vd[2];
            X->bit      = vd[3];
            X->span     = vd[4];

            if( vd.size() == 6 )
                X->tol = vd[5];

            vX.push_back( X );
        }
        else if( GetArgList( vd, "-xid=", argv[i] )
                && (vd.size() == 5 || vd.size() == 6) ) {

            D_Pulse *X = new D_Pulse;
            X->ex       = eXID;
            X->usrord   = usrord++;
            X->js       = t_js(vd[0]);
            X->ip       = vd[1];
            X->word     = vd[2];
            X->bit      = vd[3];
            X->span     = vd[4];

            if( vd.size() == 6 )
                X->tol = vd[5];

            vX.push_back( X );
        }
        else if( GetArgList( vd, "-bf=", argv[i] )
                && (vd.size() == 6) ) {

            BitField    *X = new BitField;
            X->ex       = eBFT;
            X->js       = t_js(vd[0]);
            X->usrord   = usrord++;
            X->ip       = vd[1];
            X->word     = vd[2];
            X->b0       = vd[3];
            X->nb       = vd[4];
            X->inarow   = vd[5];

            vX.push_back( X );
        }
        else if( IsArg( "-no_auto_sync", argv[i] ) )
            auto_sync = false;
        else if( GetArgStr( sarg, "-save=", argv[i] ) ) {
            if( !Save::parse( sarg ) )
                return false;
        }
        else if( GetArgStr( sarg, "-sepShanks=", argv[i] ) )
            vSK.push_back( SepShanks( sarg ) );
        else if( GetArgStr( sarg, "-maxZ=", argv[i] ) )
            vMZ.push_back( MaxZ( sarg ) );
        else if( IsArg( "-pass1_force_ni_ob_bin", argv[i] ) )
            force_ni_ob = true;
        else if( GetArgStr( sarg, "-supercat=", argv[i] ) )
            ssupercat = sarg;
        else if( IsArg( "-supercat_trim_edges", argv[i] ) )
            sc_trim = true;
        else if( IsArg( "-supercat_skip_ni_ob_bin", argv[i] ) )
            sc_skipbin = true;
        else if( GetArgStr( sarg, "-dest=", argv[i] ) )
            dest = QDir(trim_adjust_slashes( sarg )).absolutePath();
        else if( IsArg( "-no_catgt_fld", argv[i] ) )
            no_catgt_fld = true;
        else if( IsArg( "-out_prb_fld", argv[i] ) )
            out_prb_fld = true;
        else {
bad_param:
            Log() <<
            QString("Unknown option or wrong param count for option '%1'.")
            .arg( argv[i] );
            return false;
        }
    }

// Check args

    if( !ssupercat.isEmpty() && !parseElems( ssupercat ) )
        return false;

    if( velem.size() ) {

        if( dest.isEmpty() ) {
            Log() << "Error: Supercat requires -dest option.";
            goto error;
        }

        if( sc_trim && !auto_sync ) {
            Log() << "Error: Supercat edge trimming requires auto sync extraction.";
            goto error;
        }

        startsecs       = 0;
        maxsecs         = 0;
        locin_um        = 0;
        locout_um       = 0;
        gfixamp         = 0;
        gfixslp         = 0;
        gfixbas         = 0;
        apflt.clear();
        lfflt.clear();
        mexc.clear();
        gtlist.clear();
        gt_set_tcat();
        zfilmax         = -1;
        locin           = 0;
        locout          = 0;
        inarow          = -1;
        t_miss_ok       = false;
        tshift          = false;
        gblcar          = false;
        gbldmx          = false;
        gfixdo          = false;
        no_catgt_fld    = false;
    }
    else {

        if( inpar.isEmpty() ) {
            Log() << "Error: Missing -dir.";
            goto error;
        }

        if( run.isEmpty() ) {
            Log() << "Error: Missing -run.";
            goto error;
        }

        if( !gt_ok_indices() )
            goto error;
    }

    if( !ni && !ob && !ap && !lf ) {
        Log() << "Error: Missing stream indicator {-ni, -ob, -ap, -lf}.";
        goto error;
    }

    if( ob && !vobx.size() ) {
        Log() << "Error: Missing OneBox specifier -obx=???.";
        goto error;
    }

    if( (ap || lf) && (!prb_3A && !vprb.size()) ) {

        Log() << "Error: Missing probe specifier {-prb_3A or -prb=???}.";
error:
        PrintUsage();
        return false;
    }

// One type of CAR

    int nSel = 0;
    if( GBL.locout_um > 0 || GBL.locout )
        ++nSel;
    if( GBL.gblcar )
        ++nSel;
    if( GBL.gbldmx )
        ++nSel;
    if( nSel > 1 ) {
        Log() << "Error: Select only one of {loccar, gblcar, gbldmx}.";
        goto error;
    }

// Check and sort extractors : js -> ip -> usrord

    if( !checkExtractors() )
        return false;

// Check and sort save options

    if( velem.isEmpty() ) {

        if( !checkSaves() || !parseSepShanks() || !parseMaxZ() )
            return false;
    }

// Echo

    QString sreq        = "",
            sgt         = "",
            sobxs       = "",
            sprbs       = "",
            szfil       = "",
            sstartsecs  = "",
            smaxsecs    = "",
            sapfilter   = "",
            slffilter   = "",
            stshift     = "",
            sloccar_um  = "",
            sloccar     = "",
            sgfix       = "",
            schnexc     = "",
            sXTR        = "",
            sinarow     = "",
            sSave       = "",
            sSepK       = "",
            sMaxZ       = "",
            ssuper      = "",
            sdest       = "";

    if( !velem.size() ) {
        sreq    = QString(" -dir=%1 -run=%2").arg( inpar ).arg( run );
        sgt     = gt_format_params();
    }
    else
        sgt = " -t=cat";

    if( vobx.size() )
        sobxs = " -obx=" + Subset::vec2RngStr( vobx );

    if( vprb.size() && !prb_3A )
        sprbs = " -prb=" + Subset::vec2RngStr( vprb );

    if( zfilmax >= 0 )
        szfil = QString(" -zerofillmax=%1").arg( zfilmax );

    if( startsecs > 0 )
        sstartsecs = QString(" -startsecs=%1").arg( startsecs );

    if( maxsecs > 0 )
        smaxsecs = QString(" -maxsecs=%1").arg( maxsecs );

    if( apflt.isenabled() > 0 )
        sapfilter = QString(" -apfilter=%1").arg( apflt.format() );

    if( lfflt.isenabled() > 0 )
        slffilter = QString(" -lffilter=%1").arg( lfflt.format() );

    if( !velem.size() && !tshift )
        stshift = QString(" -no_tshift");

    if( locout_um > 0 )
        sloccar_um = QString(" -loccar_um=%1,%2").arg( locin_um ).arg( locout_um );

    if( locout > 0 )
        sloccar = QString(" -loccar=%1,%2").arg( locin ).arg( locout );

    if( gfixdo )
        sgfix = QString(" -gfix=%1,%2,%3").arg( gfixamp ).arg( gfixslp ).arg( gfixbas );

    if( mexc.size() )
        schnexc = QString(" -chnexcl=%1").arg( formatChnexcl() );

    foreach( const XTR *X, vX )
        sXTR += X->sparam();

    if( inarow >= 0 ) {

        if( inarow < 1 )
            inarow = 1;

        sinarow = QString(" -inarow=%1").arg( inarow );
    }

    foreach( const Save &S, vS )
        sSave += S.sparam();

    foreach( const SepShanks &K, vSK )
        sSepK += K.sparam();

    foreach( const MaxZ &Z, vMZ )
        sMaxZ += Z.sparam();

    if( velem.size() )
        ssuper = QString(" -supercat=%1").arg( formatElems() );

    if( !dest.isEmpty() )
        sdest = QString(" -dest=%1").arg( dest );
    else {
        no_catgt_fld = false;
        out_prb_fld  = false;
    }

    sCmd =
        QString(
            "CatGT%1%2%3%4%5%6%7%8%9%10%11%12%13%14%15%16%17%18%19%20"
            "%21%22%23%24%25%26%27%28%29%30%31%32%33%34%35%36%37%38%39")
        .arg( sreq )
        .arg( sgt )
        .arg( no_run_fld ? " -no_run_fld" : "" )
        .arg( prb_fld ? " -prb_fld" : "" )
        .arg( prb_miss_ok ? " -prb_miss_ok" : "" )
        .arg( exported ? " -exported" : "" )
        .arg( t_miss_ok ? " -t_miss_ok" : "" )
        .arg( ni ? " -ni" : "" )
        .arg( ob ? " -ob" : "" )
        .arg( ap ? " -ap" : "" )
        .arg( lf ? " -lf" : "" )
        .arg( sobxs )
        .arg( prb_3A ? " -prb_3A" : "" )
        .arg( sprbs )
        .arg( szfil )
        .arg( sstartsecs )
        .arg( smaxsecs )
        .arg( sapfilter )
        .arg( slffilter )
        .arg( stshift )
        .arg( sloccar_um )
        .arg( sloccar )
        .arg( gblcar ? " -gblcar" : "" )
        .arg( gbldmx ? " -gbldmx" : "" )
        .arg( sgfix )
        .arg( schnexc )
        .arg( sXTR )
        .arg( sinarow )
        .arg( auto_sync ? "" : " -no_auto_sync" )
        .arg( sSave )
        .arg( sSepK )
        .arg( sMaxZ )
        .arg( force_ni_ob ? " -pass1_force_ni_ob_bin" : "" )
        .arg( ssuper )
        .arg( sc_trim ? " -supercat_trim_edges" : "" )
        .arg( sc_skipbin ? " -supercat_skip_ni_ob_bin" : "" )
        .arg( sdest )
        .arg( no_catgt_fld ? " -no_catgt_fld" : "" )
        .arg( out_prb_fld ? " -out_prb_fld" : "" );

    Log() << QString("Cmdline: %1").arg( sCmd );

// Probe count adjustment

    if( prb_3A )
        vprb.fill( 0, 1 );

// Set default inarow

    if( inarow < 0 )
        inarow = 5;

// Inpath adjustments

    if( velem.isEmpty() ) {
        // Pass-1 specific
        if( !pass1FromCatGT() )
            return false;
    }
    else {
        // Pass-2 specific
        velem[0].unpack();
    }

// Outpath adjustments

    if( !makeTaggedDest() )
        return false;

// Create auto_sync extractors

    return addAutoExtractors();
}


// Carve gtlist into {g,ta,tb}.
//
bool CGBL::gt_parse_list( const QString &s )
{
    if( !s.contains( "{" ) || !s.contains( "," ) ) {
        Log() << "Error: gtlist not formatted as elements {gj,tja,tjb}.";
        return false;
    }

// Split into elements

    QStringList el = s.split(
                        QRegExp("^\\s*\\{\\s*|\\s*\\}\\s*\\{\\s*|\\s*\\}\\s*$"),
                        QString::SkipEmptyParts );
    int         ne = el.size();

// Split each element

    for( int ie = 0; ie < ne; ++ie ) {

        QStringList sl = el[ie].split(
                            QRegExp("^\\s+|\\s*,\\s*|\\s+$"),
                            QString::SkipEmptyParts );
        int         ns = sl.size();

        if( ns != 3 ) {
            Log() <<
                QString("Error: gtlist element %1 has bad format.")
                .arg( ie );
            return false;
        }

        // parse indices

        GT3 E;
        E.g  = sl[0].toInt();
        E.ta = sl[1].toInt();
        E.tb = sl[2].toInt();

        if( E.g < 0 ) {
            Log() <<
                QString("Error: gtlist element %1 has illegal g.")
                .arg( ie );
            return false;
        }

        if( E.ta < 0 || E.tb < E.ta ) {
            Log() <<
                QString("Error: gtlist element %1 has illegal t-range.")
                .arg( ie );
            return false;
        }

        gtlist.push_back( E );
    }

    return true;
}


void CGBL::gt_set_tcat()
{
    ta = tb = -1;
}


bool CGBL::gt_is_tcat() const
{
    return ta == -1;
}


bool CGBL::gt_ok_indices() const
{
    if( gtlist.size() ) {

        if( ga != -1 || ta != -2 ) {
            Log() << "Error: Option -gtlist cannot be used with -g and -t options.";
            return false;
        }
    }
    else if( ga < 0 || gb < ga || ta < -1 || tb < ta ) {
        Log() << "Error: Bad g- or t-indices.";
        return false;
    }

    return true;
}


QString CGBL::gt_format_params() const
{
    QString s;
    int     n = gtlist.size();

    if( n ) {
        s = " -gtlist=";
        for( int i = 0; i < n; ++i ) {
            const GT3   &E = gtlist[i];
            s += QString("{%1,%2,%3}").arg( E.g ).arg( E.ta ).arg( E.tb );
        }
    }
    else {
        s = QString(" -g=%1").arg( ga );

        if( gb > ga )
            s += QString(",%1").arg( gb );

        if( ta == -1 )
            s += " -t=cat";
        else {
            s += QString(" -t=%1").arg( ta );
            if( tb > ta )
                s += QString(",%1").arg( tb );
        }
    }

    return s;
}


int CGBL::gt_get_first( int *t ) const
{
    if( gtlist.size() ) {

        const GT3   &E = gtlist[0];
        if( t )
            *t = E.ta;
        return E.g;
    }
    else {
        if( t )
            *t = ta;
        return ga;
    }
}


int CGBL::gt_nIndices() const
{
    int n = gtlist.size();

    if( n ) {
        int S = 0;
        for( int i = 0; i < n; ++i ) {
            const GT3   &E = gtlist[i];
            S += E.tb - E.ta + 1;
        }
        return S;
    }
    else
        return (gb - ga + 1) * (tb - ta + 1);
}


int CGBL::myXrange( int &lim, t_js js, int ip ) const
{
    lim = vX.size();

    for( int i = 0; i < lim; ++i ) {

        XTR *iX = vX[i];

        if( iX->js == js && iX->ip == ip ) {

            for( int j = i + 1; j < lim; ++j ) {

                XTR *jX = vX[j];

                if( jX->js != js || jX->ip != ip ) {
                    lim = j;
                    break;
                }
            }

            return i;
        }
    }

    return lim;
}


int CGBL::mySrange( int &lim, t_js js, int ip ) const
{
    lim = vS.size();

    for( int i = 0; i < lim; ++i ) {

        const Save  &iS = vS[i];

        if( iS.js == js && iS.ip1 == ip ) {

            for( int j = i + 1; j < lim; ++j ) {

                const Save  &jS = vS[j];

                if( jS.js != js || jS.ip1 != ip ) {
                    lim = j;
                    break;
                }
            }

            return i;
        }
    }

    return lim;
}


void CGBL::fyi_ct_write()
{
    QString srun = QString("%1_g%2").arg( run ).arg( gt_get_first( 0 ) ),
            dir;

    if( !dest.isEmpty() ) {
        if( no_catgt_fld ) {
            fyi["supercat_element"] = QString("{%1,%2}").arg( dest ).arg( srun );
            dir = dest;
        }
        else {
            fyi["supercat_element"] =
                QString("{%1,catgt_%2}").arg( dest ).arg( srun );
            dir = QString("%1/catgt_%2").arg( dest ).arg( srun );
        }
    }
    else {
        fyi["supercat_element"] = QString("{%1,%2}").arg( inpar ).arg( srun );
        dir = inpar;

        if( !no_run_fld )
            dir += QString("/%1").arg( srun );
    }

    fyi["outpath_top"] = dir;

    fyi.toMetaFile( QString("%1/%2_fyi.txt").arg( dir ).arg( srun ) );
}


void CGBL::fyi_sc_write()
{
    QString srun = QString("%1_g%2").arg( velem[0].run ).arg( velem[0].g ),
            dir  = QString("%1/supercat_%2").arg( dest ).arg( srun );

    fyi["outpath_top"] = dir;

    fyi.toMetaFile( QString("%1/%2_fyi.txt").arg( dir ).arg( srun ) );
}


bool CGBL::makeOutputProbeFolder( int g0, int ip )
{
    QString fyikey = QString("outpath_probe%1").arg( ip );

    if( !dest.isEmpty() ) {

        prb_obase = im_obase;

        if( out_prb_fld ) {

            // Create probe subfolder: dest/tag_run_g0/run_g0_imec0

            prb_obase += QString("/%1_g%2_imec%3").arg( run ).arg( g0 ).arg( ip );

            if( !QDir().exists( prb_obase ) && !QDir().mkdir( prb_obase ) ) {
                Log() << QString("Error creating dir '%1'.").arg( prb_obase );
                return false;
            }

            fyi[fyikey] = prb_obase;

            // Append run name up to _tcat

            prb_obase += QString("/%1_g%2_tcat").arg( run ).arg( g0 );
        }
    }
    else if( prb_fld )
        fyi[fyikey] = inPath( g0, AP, ip );

    return true;
}


QString CGBL::inFile( int g, int t, t_js js, int ip, t_ex ex, XTR *X )
{
    QString s = inPathUpTo_t( g, js, ip );

// t-index

    QString t_str;

    if( t == -1 )
        t_str = "cat";
    else
        t_str = QString("%1").arg( t );

    return s + t_str + suffix( js, ip, ex, X );
}


QString CGBL::niOutFile( int g0, t_ex ex, XTR *X )
{
    QString s;

    if( dest.isEmpty() )
        s = inPathUpTo_t( g0, NI, 0 ) + "cat";
    else
        s = aux_obase;

    return s + suffix( NI, 0, ex, X );
}


QString CGBL::obOutFile( int g0, int ip, t_ex ex, XTR *X )
{
    QString s;

    if( dest.isEmpty() )
        s = inPathUpTo_t( g0, OB, ip ) + "cat";
    else
        s = aux_obase;

    return s + suffix( OB, ip, ex, X );
}


QString CGBL::imOutFile( int g0, t_js js, int ip, t_ex ex, XTR *X )
{
    QString s;

    if( dest.isEmpty() )
        s = inPathUpTo_t( g0, js, ip ) + "cat";
    else
        s = prb_obase;

    return s + suffix( js, ip, ex, X );
}


bool CGBL::openOutputBinary(
    QFile       &fout,
    QString     &outBin,
    int         g0,
    t_js        js,
    int         ip )
{
    if( !velem.size() && gt_is_tcat() ) {
        Log() <<
        "Error: Secondary extraction pass (-t=cat) must not"
        " concatenate, tshift or filter.";
        return false;
    }

    switch( js ) {
        case NI: outBin = niOutFile( g0, eBIN ); break;
        case OB: outBin = obOutFile( g0, ip, eBIN ); break;
        case AP:
        case LF: outBin = imOutFile( g0, js, ip, eBIN ); break;
    }

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
int CGBL::openInputFile(
    QFile       &fin,
    QFileInfo   &fib,
    int         g,
    int         t,
    t_js        js,
    int         ip,
    t_ex        ex,
    XTR         *X )
{
    QString inBin = inFile( g, t, js, ip, ex, X );

    fib.setFile( inBin );

    if( !fib.exists() ) {
        Log() << QString("File not found '%1'.").arg( fib.filePath() );
        if( t_miss_ok )
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
int CGBL::openInputBinary(
    QFile       &fin,
    QFileInfo   &fib,
    int         g,
    int         t,
    t_js        js,
    int         ip )
{
    return openInputFile( fin, fib, g, t, js, ip, eBIN );
}


// Return:
// 0 - ok.
// 1 - skip.
// 2 - fail.
//
int CGBL::openInputMeta(
    QFileInfo   &fim,
    KVParams    &kvp,
    int         g,
    int         t,
    t_js        js,
    int         ip,
    bool        canSkip )
{
    QString inMeta = inFile( g, t, js, ip, eMETA );

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


// Return allocated probe class, or, 0.
//
IMROTbl *CGBL::getProbe( const KVParams &kvp )
{
    IMROTbl                     *R      = 0;
    KVParams::const_iterator    it_kvp  = kvp.find( "imDatPrb_pn" );
    QString                     pn;

    if( it_kvp != kvp.end() )
        pn = it_kvp.value().toString();
    else if( kvp.contains( "imProbeOpt" ) )
        pn = "Probe3A";

    if( !pn.isEmpty() )
        R = IMROTbl::alloc( pn );

    return R;
}


bool CGBL::getSavedChannels(
    QVector<uint>   &snsFileChans,
    const KVParams  &kvp,
    const QFileInfo &fim )
{
    QString chnstr = kvp["snsSaveChanSubset"].toString();

    if( Subset::isAllChansStr( chnstr ) )
        Subset::defaultVec( snsFileChans, kvp["nSavedChans"].toInt() );
    else if( !Subset::rngStr2Vec( snsFileChans, chnstr ) ) {
        Log() << QString("Bad snsSaveChanSubset tag '%1'.").arg( fim.fileName() );
        return false;
    }

    return true;
}

/* --------------------------------------------------------------- */
/* Private ------------------------------------------------------- */
/* --------------------------------------------------------------- */

// Carve chnexcl into {prb;chn}.
//
bool CGBL::parseChnexcl( const QString &s )
{
    QSet<int>   seen;

    if( !s.contains( "{" ) || !s.contains( ";" ) ) {
        Log() << "Error: chnexcl not formatted as elements {prb;channels}.";
        return false;
    }

// Split into elements

    QStringList el = s.split(
                        QRegExp("^\\s*\\{\\s*|\\s*\\}\\s*\\{\\s*|\\s*\\}\\s*$"),
                        QString::SkipEmptyParts );
    int         ne = el.size();

// Split each element

    for( int ie = 0; ie < ne; ++ie ) {

        QStringList sl = el[ie].split(
                            QRegExp("^\\s+|\\s*;\\s*|\\s+$"),
                            QString::SkipEmptyParts );
        int         ns = sl.size();

        if( ns != 2 ) {
            Log() <<
                QString("Error: chnexcl element %1 has bad format.")
                .arg( ie );
            return false;
        }

        // parse channels

        QVector<uint>   C;
        int             prb = sl[0].toInt();

        if( seen.contains( prb ) ) {
            Log() <<
                QString("Error: chnexcl names probe %1 twice.")
                .arg( prb );
            return false;
        }

        if( !Subset::rngStr2Vec( C, sl[1] ) ) {
            Log() <<
                QString("Error: chnexcl element %1 has bad channel list.")
                .arg( ie );
            return false;
        }

        seen.insert( prb );
        mexc[prb] = C;
    }

    return true;
}


// Now, just check for bad params.
// Parse again later when metadata available.
//
bool CGBL::parseSepShanks()
{
    QSet<int>   seen;

    for( int ik = 0, nk = vSK.size(); ik < nk; ++ik ) {

        if( !vSK[ik].parse( seen ) )
            return false;
    }

    qSort( vSK );
    return true;
}


// Now, just check for bad params.
// Parse again later when metadata available.
//
bool CGBL::parseMaxZ()
{
    QSet<int>   seen;

    for( int iz = 0, nz = vMZ.size(); iz < nz; ++iz ) {

        if( !vMZ[iz].parse( seen ) )
            return false;
    }

    qSort( vMZ );
    return true;
}


// Carve supercat into {dir,run,g}.
//
bool CGBL::parseElems( const QString &s )
{
// Split into elements

    QStringList el = s.split(
                        QRegExp("^\\s*\\{\\s*|\\s*\\}\\s*\\{\\s*|\\s*\\}\\s*$"),
                        QString::SkipEmptyParts );
    int         ne = el.size();

    if( ne < 2 ) {
        Log() << "Error: Supercat requires at least 2 elements.";
        return false;
    }

// Split each element

    QRegExp re("^(catgt_)?(.*)_g(\\d+)$");
    re.setCaseSensitivity( Qt::CaseInsensitive );

    for( int ie = 0; ie < ne; ++ie ) {

        QStringList sl = el[ie].split(
                            QRegExp("^\\s+|\\s*,\\s*|\\s+$"),
                            QString::SkipEmptyParts );
        int         ns = sl.size();

        if( ns != 2 ) {
            Log() <<
                QString("Error: Supercat element %1 has bad format.")
                .arg( ie );
            return false;
        }

        // Split run

        int i = sl[1].indexOf( re );

        if( i < 0 ) {
            Log() <<
                QString("Error: Supercat element %1 has bad run_ga part.")
                .arg( ie );
            return false;
        }

        bool    catgt_fld       = !re.cap(1).isEmpty(),
                e_no_run_fld    = (catgt_fld ? false : no_run_fld);

        velem.push_back( Elem(
            trim_adjust_slashes( sl[0] ), re.cap(2),
            re.cap(3).toInt(), e_no_run_fld, catgt_fld ) );
    }

    return true;
}


bool CGBL::checkExtractors()
{
    qSort( vX.begin(), vX.end(), XTR::pointerCompare() );

    bool ok = true;

    foreach( XTR *X, vX ) {

        if( X->js < 0 || X->js > AP ) {
            Log() <<
            QString("Error: Extractor js must be in range [0..2]:%1.")
            .arg( X->sparam() );
            ok = false;
        }

        switch( X->js ) {
            case NI:
                if( X->ip != 0 ) {
                    Log() <<
                    QString("Warning: Extractor ip should be zero for ni stream:%1.")
                    .arg( X->sparam() );
                }
                break;
            case OB:
                if( !vobx.contains( X->ip ) ) {
                    Log() <<
                    QString("Warning: Extractor ip not among -obx=list:%1.")
                    .arg( X->sparam() );
                }
                break;
            case AP:
                if( X->ex == eXA || X->ex == eXIA || X->ex == eBFT ) {
                    Log() <<
                    QString("Error: Illegal extractor type for AP stream:%1.")
                    .arg( X->sparam() );
                    ok = false;
                }
                if( !vprb.contains( X->ip ) ) {
                    Log() <<
                    QString("Warning: Extractor ip not among -prb=list:%1.")
                    .arg( X->sparam() );
                }
                break;
            default:;
        }

        if( X->ex == eBFT ) {

            BitField *B = reinterpret_cast<BitField*>(X);

            if( B->b0 < 0 || B->b0 > 15 || B->nb < 1 || B->nb > 16 - B->b0 ) {

                Log() <<
                QString("Error: Extractor bf startbit must be in range [0..15],"
                " nbits in range [1..16-startbit]:%1.")
                .arg( X->sparam() );
                ok = false;
            }

            if( B->inarow < 1 ) {
                B->inarow = 1;
                Log() << QString("Warning: Extractor bf inarow must be >= 1:%1.")
                .arg( X->sparam() );
            }
        }
    }

    return ok;
}


bool CGBL::checkSaves()
{
    qSort( vS );

    bool ok = true;

    foreach( const Save &S, vS ) {

        if( S.js < AP || S.js > LF ) {
            Log() <<
            QString("Error: Save js must be in range [2..3]:%1.")
            .arg( S.sparam() );
            ok = false;
        }

        if( (prb_3A && S.ip1 != 0) || !vprb.contains( S.ip1 ) ) {
            Log() <<
            QString("Warning: Save ip1 not among -prb=list:%1.")
            .arg( S.sparam() );
        }
    }

    return ok;
}


QString CGBL::formatChnexcl()
{
    QString s;

    QMap<int,QVector<uint>>::const_iterator it  = mexc.begin(),
                                            end = mexc.end();

    for( ; it != end; ++it ) {

        s += QString("{%1;%2}")
                .arg( it.key() )
                .arg( Subset::vec2RngStr( it.value() ) );
    }

    return s;
}


QString CGBL::formatElems()
{
    QString s;

    for( int ie = 0, ne = velem.size(); ie < ne; ++ie ) {

        const Elem &E = velem[ie];

        s += QString("{%1,%2%3_g%4}")
                .arg( E.dir )
                .arg( E.in_catgt_fld ? "catgt_" : "" )
                .arg( E.run ).arg( E.g );
    }

    return s;
}


// Check if pass-1 (non-supercat) input is coming from catgt outut.
// Adjust globals {catgt_fld, run, no_run_fld, g-range}.
//
// Return true if ok.
//
bool CGBL::pass1FromCatGT()
{
    if( run.startsWith( "catgt_", Qt::CaseInsensitive ) ) {

        run = run.remove( 0, 6 );

        if( !run.size() ) {
            Log() << QString("Error: empty run name '%1'.").arg( run );
            return false;
        }

        in_catgt_fld    = true;
        no_run_fld      = false;
    }

    if( in_catgt_fld || gt_is_tcat() )
        gb = ga;

    return true;
}


// Form reusable output path substrings.
//
// Output parts:
// There is no (g,t) dependence because output is
// always named for (ga,tcat). However, there may
// be probe (ip) dependence.
//
// Suffices:
// NI can be made now.
// IM has (ip) dependence.
//
// Return true if ok.
//
bool CGBL::makeTaggedDest()
{
    if( !dest.isEmpty() ) {

        int g0  = gt_get_first( 0 );

        im_obase = dest;

        // -----------------------------------------------------
        // Create run subfolder for all streams: dest/tag_run_g0
        // -----------------------------------------------------

        if( velem.isEmpty() ) {
            if( !no_catgt_fld )
                im_obase += QString("/catgt_%1_g%2").arg( run ).arg( g0 );
        }
        else
            im_obase += QString("/supercat_%1_g%2").arg( run ).arg( g0 );

        if( !QDir().exists( im_obase ) && !QDir().mkdir( im_obase ) ) {
            Log() << QString("Error creating dir '%1'.").arg( im_obase );
            return false;
        }

        // --------------------------------------------
        // NI/OB file base: dest/tag_run_g0/run_g0_tcat
        // --------------------------------------------

        aux_obase = QString("%1/%2_g%3_tcat")
                        .arg( im_obase ).arg( run ).arg( g0 );

        // ---------------
        // IM file base...
        // ---------------

        // If not probe folders, all streams use ni_obase style.
        // Else defer until each im stream to:
        //  + create _imec%1 subfolders
        //  + append run_g0_tcat base

        if( !out_prb_fld )
            im_obase = aux_obase;
    }

    return true;
}


// acqMnMaXaDw = acquired stream channel counts.
//
void CGBL::parseNiChanCounts(
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


bool CGBL::addAutoExtractors()
{
    if( !auto_sync )
        return true;

// NI

    if( ni ) {

        QFileInfo   fim;
        KVParams    kvp;
        int         t0, g0 = gt_get_first( &t0 );

        if( in_catgt_fld || velem.size() )
            t0 = -1;

        if( openInputMeta( fim, kvp, g0, t0, NI, 0, false ) )
            return false;

        QVector<uint>   snsFileChans;
        int             iword = kvp["syncNiChan"].toInt(),
                        bit;

        if( !getSavedChannels( snsFileChans, kvp, fim ) )
            return false;

        if( kvp["syncNiChanType"].toInt() == 1 ) {  // Analog

            A_Pulse *X = new A_Pulse;
            X->ex       = eXA;
            X->usrord   = -1;
            X->js       = NI;
            X->ip       = 0;
            X->word     = snsFileChans.indexOf( iword );
            X->thresh   = kvp["syncNiThresh"].toDouble();
            X->thrsh2   = 0;
            X->span     = 500;
            vX.push_back( X );
        }
        else {  // Digital

            int     niCumTypCnt[CniCfg::niNTypes];
            parseNiChanCounts( niCumTypCnt, kvp );

            bit     = iword % 16;
            iword   = niCumTypCnt[CniCfg::niSumAnalog] + iword / 16;
            iword   = snsFileChans.indexOf( iword );

            D_Pulse *X = new D_Pulse;
            X->ex       = eXD;
            X->usrord   = -1;
            X->js       = NI;
            X->ip       = 0;
            X->word     = iword;
            X->bit      = bit;
            X->span     = 500;
            vX.push_back( X );
        }
    }

// OB

    foreach( uint ip, vobx ) {
        D_Pulse *X = new D_Pulse;
        X->ex       = eXD;
        X->usrord   = -1;
        X->js       = OB;
        X->ip       = ip;
        X->word     = -1;
        X->bit      = 6;
        X->span     = 500;
        vX.push_back( X );
    }

// AP

    foreach( uint ip, vprb ) {
        D_Pulse *X = new D_Pulse;
        X->ex       = eXD;
        X->usrord   = -1;
        X->js       = AP;
        X->ip       = ip;
        X->word     = -1;
        X->bit      = 6;
        X->span     = 500;
        vX.push_back( X );
    }

// Re-sort

    qSort( vX.begin(), vX.end(), XTR::pointerCompare() );

    return true;
}


QString CGBL::trim_adjust_slashes( const QString &dir )
{
    QString s = dir.trimmed();

    s.replace( "\\", "/" );
    return s.remove( QRegExp("/+$") );
}


QString CGBL::inPath( int g, t_js js, int ip )
{
    QString srun = QString("%1_g%2").arg( run ).arg( g ),
            s;

// parent dir

    s = inpar;

// run subfolder?

    if( !no_run_fld ) {

        s += "/";

        if( in_catgt_fld )
            s += "catgt_";

        s += QString("%1").arg( srun );
    }

// probe subfolder?

    if( js >= AP && prb_fld )
        s += QString("/%1_imec%2").arg( srun ).arg( ip );

    return s;
}


QString CGBL::inPathUpTo_t( int g, t_js js, int ip )
{
    return inPath( g, js, ip ) + QString("/%1_g%2_t").arg( run ).arg( g );
}


QString CGBL::suffix( t_js js, int ip, t_ex ex, XTR *X )
{
    QString suf;

    if( exported )
        suf = ".exported";

    switch( js ) {
        case NI: suf += ".nidq"; break;
        case OB: suf += QString(".obx%1.obx").arg( ip ); break;
        case AP:
        case LF:
            suf += ".imec";
            if( !prb_3A )
                suf += QString("%1").arg( ip );
            suf += (js == AP ? ".ap" : ".lf");
    }

    switch( ex ) {
        case eBIN: suf += ".bin";  break;
        case eMETA: suf += ".meta"; break;
        case eXA: suf += X->suffix( "xa" ); break;
        case eXD: suf += X->suffix( "xd" ); break;
        case eXIA: suf += X->suffix( "xia" ); break;
        case eXID: suf += X->suffix( "xid" ); break;
        case eBFT: suf += X->suffix( "bft" ); break;
        case eBFV: suf += X->suffix( "bfv" ); break;
    }

    return suf;
}


