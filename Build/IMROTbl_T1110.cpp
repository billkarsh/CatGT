
#include "IMROTbl_T1110.h"
#include "Util.h"

#ifdef HAVE_IMEC
#include "IMEC/NeuropixAPI.h"
using namespace Neuropixels;
#endif

#include <QFileInfo>
#include <QStringList>
#include <QRegExp>
#include <QTextStream>

/* ---------------------------------------------------------------- */
/* struct IMROHdr ------------------------------------------------- */
/* ---------------------------------------------------------------- */

// Pattern: "(type,colmode,refid,apgn,lfgn,apflt)"
//
QString IMROHdr_T1110::toString( int type ) const
{
    return QString("(%1,%2,%3,%4,%5,%6)")
            .arg( type ).arg( colmode ).arg( refid )
            .arg( apgn ).arg( lfgn ).arg( apflt );
}

/* ---------------------------------------------------------------- */
/* struct IMRODesc ------------------------------------------------ */
/* ---------------------------------------------------------------- */

// Pattern: "(grp bankA bankB)"
//
QString IMRODesc_T1110::toString( int grp ) const
{
    return QString("(%1 %2 %3)").arg( grp ).arg( bankA ).arg( bankB );
}


// Pattern: "grp bankA bankB"
//
// Note: The grp field is discarded.
//
IMRODesc_T1110 IMRODesc_T1110::fromString( const QString &s )
{
    const QStringList   sl = s.split(
                                QRegExp("\\s+"),
                                QString::SkipEmptyParts );

    return IMRODesc_T1110( sl.at( 1 ).toInt(), sl.at( 2 ).toInt() );
}

/* ---------------------------------------------------------------- */
/* struct IMROTbl ------------------------------------------------- */
/* ---------------------------------------------------------------- */

void IMROTbl_T1110::fillDefault()
{
    type = imType1110Type;

    e.clear();
    e.resize( imType1110Chan );
}


// Return true if two tables are same w.r.t connectivity.
//
bool IMROTbl_T1110::isConnectedSame( const IMROTbl *rhs ) const
{
    const IMROTbl_T1110 *RHS    = (const IMROTbl_T1110*)rhs;

    return ehdr.colmode == RHS->ehdr.colmode && e == RHS->e;
}


// Pattern: (type,colmode,refid,apgn,lfgn,apflt)(grp bankA bankB)()()...
//
QString IMROTbl_T1110::toString() const
{
    QString     s;
    QTextStream ts( &s, QIODevice::WriteOnly );

    ts << ehdr.toString( type );

    for( int ig = 0; ig < imType1110Groups; ++ig )
        ts << e[ig].toString( ig );

    return s;
}


// Pattern: (type,colmode,refid,apgn,lfgn,apflt)(grp bankA bankB)()()...
//
// Return true if file type compatible.
//
bool IMROTbl_T1110::fromString( QString *msg, const QString &s )
{
    QStringList sl = s.split(
                        QRegExp("^\\s*\\(|\\)\\s*\\(|\\)\\s*$"),
                        QString::SkipEmptyParts );
    int         n  = sl.size();

// Header

    QStringList hl = sl[0].split(
                        QRegExp("^\\s+|\\s*,\\s*"),
                        QString::SkipEmptyParts );

    if( hl.size() != 6 ) {
        type = -3;      // 3A type
        if( msg )
            *msg = "Wrong imro header size (should be 6)";
        return false;
    }

    type = hl[0].toInt();

    if( type != imType1110Type ) {
        if( msg ) {
            *msg = QString("Wrong imro type[%1] for probe type[%2]")
                    .arg( type ).arg( imType1110Type );
        }
        return false;
    }

    ehdr = IMROHdr_T1110(
            hl[1].toInt(), hl[2].toInt(), hl[3].toInt(),
            hl[4].toInt(), hl[5].toInt() );

// Entries

    e.clear();
    e.reserve( n - 1 );

    for( int i = 1; i < n; ++i ) {

        IMRODesc_T1110  E = IMRODesc_T1110::fromString( sl[i] );

        if( ehdr.colmode == 2 ) {
            if( E.bankA != E.bankB ) {
                if( msg )
                    *msg = "In dual col mode bankA must equal bankB.";
                return false;
            }
        }
        else {
            bool aColCrossed = (E.bankA/4) & 1,
                 bColCrossed = (E.bankB/4) & 1;

            if( aColCrossed == bColCrossed ) {
                if( msg ) {
                    *msg =  "In inner (or outer) col mode, one bank"
                            " must be col-crossed and the other not.";
                }
                return false;
            }
        }

        e.push_back( E );
    }

    if( e.size() != imType1110Groups ) {
        if( msg ) {
            *msg = QString("Wrong imro entry count [%1] (should be %2)")
                    .arg( e.size() ).arg( imType1110Groups );
        }
        return false;
    }

    return true;
}


bool IMROTbl_T1110::loadFile( QString &msg, const QString &path )
{
    QFile       f( path );
    QFileInfo   fi( path );

    if( !fi.exists() ) {

        msg = QString("Can't find '%1'").arg( fi.fileName() );
        return false;
    }
    else if( f.open( QIODevice::ReadOnly | QIODevice::Text ) ) {

        QString reason;

        if( fromString( &reason, f.readAll() ) ) {

            msg = QString("Loaded (type=%1) file '%2'")
                    .arg( type ).arg( fi.fileName() );
            return true;
        }
        else {
            msg = QString("Error: %1 in file '%2'")
                    .arg( reason ).arg( fi.fileName() );
            return false;
        }
    }
    else {
        msg = QString("Error opening '%1'").arg( fi.fileName() );
        return false;
    }
}


bool IMROTbl_T1110::saveFile( QString &msg, const QString &path ) const
{
    QFile       f( path );
    QFileInfo   fi( path );

    if( f.open( QIODevice::WriteOnly | QIODevice::Text ) ) {

        int n = f.write( STR2CHR( toString() ) );

        if( n > 0 ) {

            msg = QString("Saved (type=%1) file '%2'")
                    .arg( type )
                    .arg( fi.fileName() );
            return true;
        }
        else {
            msg = QString("Error writing '%1'").arg( fi.fileName() );
            return false;
        }
    }
    else {
        msg = QString("Error opening '%1'").arg( fi.fileName() );
        return false;
    }
}


// In dual mode bankA and bankB are the same.
// In outer mode upper cols are even and lower cols are odd.
// In inner mode upper cols are odd  and lower cols are even.
//
int IMROTbl_T1110::bank( int ch ) const
{
    const IMRODesc_T1110    &E = e[grpIdx( ch )];

    if( ehdr.colmode == 2 )         // dual
        return E.bankA;
    else {

        int col = this->col( ch, E.bankA );

        if( ehdr.colmode == 1 ) {  // outer

            if( col <= 3 ) {
                if( !(col & 1) )
                    return E.bankA;
            }
            else if( col & 1 )
                return E.bankA;
        }
        else {                      // inner

            if( col <= 3 ) {
                if( col & 1 )
                    return E.bankA;
            }
            else if( !(col & 1) )
                return E.bankA;
        }
    }

    return E.bankB;
}


int IMROTbl_T1110::elShankAndBank( int &bank, int ch ) const
{
    bank = this->bank( ch );
    return 0;
}


int IMROTbl_T1110::elShankColRow( int &col, int &row, int ch ) const
{
    int bank = this->bank( ch );

    col = this->col( ch, bank );
    row = this->row( ch, bank );

    return 0;
}


void IMROTbl_T1110::eaChansOrder( QVector<int> &v ) const
{
    QMap<int,int>   el2Ch;
    int             order   = 0,
                    _nAP    = nAP();

    v.resize( 2 * _nAP + 1 );

// Order the AP set

    for( int ic = 0; ic < _nAP; ++ic )
        el2Ch[chToEl( ic )] = ic;

    QMap<int,int>::iterator it;

    for( it = el2Ch.begin(); it != el2Ch.end(); ++it )
        v[it.value()] = order++;

// The LF set have same order but offset by nAP

    for( it = el2Ch.begin(); it != el2Ch.end(); ++it )
        v[it.value() + _nAP] = order++;

// SY is last

    v[order] = order;
}


// refid [0]    ext, shank=0, bank=0.
// refid [1]    tip, shank=0, bank=0.
//
int IMROTbl_T1110::refTypeAndFields( int &shank, int &bank, int /* ch */ ) const
{
    shank   = 0;
    bank    = 0;
    return ehdr.refid;
}


// tip -> base (probe orientation):
//
// Even bank direct std:          Odd bank group-crossed:
// G0 G4 G8  G12 G16 G20  (up)    G2 G6 G10 G14 G18 G22  (up)
// G2 G6 G10 G14 G18 G22  (up)    G0 G4 G8  G12 G16 G20  (up)
// G1 G5 G9  G13 G17 G21  (lw)    G3 G7 G11 G15 G19 G23  (lw)
// G3 G7 G11 G15 G19 G23  (lw)    G1 G5 G9  G13 G17 G21  (lw)
//
// Each bank has 24 groups arranged as 6 grprows X 4 grpcols.
// Each group  has 16 chans.
// Each grprow has 64 chans.
//
// The first  two (up) grpcols have only even chans.
// The second two (lw) grpcols have only odd  chans.
//
int IMROTbl_T1110::grpIdx( int ch ) const
{
    int grprow  = ch / 64,
        odd     = ch & 1;

// If ch is even, grpidx is either {4*grprow + 0, 4*grprow + 2}.
// If ch is odd,  grpidx is either {4*grprow + 1, 4*grprow + 3}.
// Which set member depends on ch%64 < 32 (if even); ch%64 < 33 (if odd).

    return 4*grprow + odd + 2*((ch - 64*grprow) >= (32 + odd));
}


int IMROTbl_T1110::col( int ch, int bank ) const
{
    int col_tbl[8]  = {0,2,1,3,  1,3,0,2},
        grpIdx      = this->grpIdx( ch ),
        grp_col     = col_tbl[4*(bank & 1) + (grpIdx % 4)],
        crossed     = (bank / 4) & 1,
        ingrp_col   = ((((ch % 64) % 32) / 2) & 1) ^ crossed;

    return 2*grp_col + ingrp_col;
}


int IMROTbl_T1110::row( int ch, int bank ) const
{
// Row within bank-0:

    int grpIdx      = this->grpIdx( ch ),
        grp_row     = grpIdx / 4,
        ingrp_row   = ((ch % 64) % 32) / 4,
        b0_row      = 8*grp_row + ingrp_row;

// Rows per bank = 384 / 8 = 48

    return 48*bank + b0_row;
}


// Our idea of electrode index runs faster across row, so,
// bottom row has electrodes {0..7}. That differs from
// convention in imec docs, but is easier to understand
// and the only purpose is sorting chans tip to base.
//
int IMROTbl_T1110::chToEl( int ch ) const
{
    int bank = this->bank( ch );

    return 8*row( ch, bank ) + col( ch, bank );
}


static int i2gn[IMROTbl_T1110::imType1110Gains]
            = {50,125,250,500,1000,1500,2000,3000};


int IMROTbl_T1110::idxToGain( int idx ) const
{
    return (idx >= 0 && idx < 8 ? i2gn[idx] : i2gn[3]);
}


int IMROTbl_T1110::gainToIdx( int gain ) const
{
    switch( gain ) {
        case 50:    return 0;
        case 125:   return 1;
        case 250:   return 2;
        case 500:   return 3;
        case 1000:  return 4;
        case 1500:  return 5;
        case 2000:  return 6;
        case 3000:  return 7;
        default:    return 3;
    }
}


void IMROTbl_T1110::locFltRadii( int &rin, int &rout, int iflt ) const
{
    switch( iflt ) {
        case 2:     rin = 8, rout = 32; break;
        default:    rin = 4, rout = 8;  break;
    }
}


void IMROTbl_T1110::muxTable( int &nADC, int &nGrp, std::vector<int> &T ) const
{
    nADC = 32;
    nGrp = 12;

    T.resize( imType1110Chan );

// Generate by pairs of columns

    int ch = 0;

    for( int icol = 0; icol < nADC; icol += 2 ) {

        for( int irow = 0; irow < nGrp; ++irow ) {
            T[nADC*irow + icol]     = ch++;
            T[nADC*irow + icol + 1] = ch++;
        }
    }
}


int IMROTbl_T1110::selectSites( int slot, int port, int dock ) const
{
#ifdef HAVE_IMEC

    NP_ErrorCode    err;

    if( err != SUCCESS )
        return err;

// ---------------
// Set column mode
// ---------------

// ------------------------------------
// Connect all according to table banks
// ------------------------------------

    for( int ig = 0; ig < imType1110Groups; ++ig ) {

        const IMRODesc_T1110    E = e[ig];

//        err = np_selectElectrodeMask( slot, port, dock, ic,
//                shank, electrodebanks_t(E.bankA) );

        if( err != SUCCESS )
            return err;

        if( ehdr.colmode < 2 ) {

//            err = np_selectElectrodeMask( slot, port, dock, ic,
//                    shank, electrodebanks_t(E.bankA) );

            if( err != SUCCESS )
                return err;
        }
    }
#endif

    return 0;
}


int IMROTbl_T1110::selectRefs( int slot, int port, int dock ) const
{
#ifdef HAVE_IMEC
// -----------------------------
// Connect all according to ehdr
// -----------------------------

    for( int ic = 0; ic < imType1110Chan; ++ic ) {

        NP_ErrorCode    err;

        err = np_setReference( slot, port, dock, ic,
                0, channelreference_t(ehdr.refid), 0 );

        if( err != SUCCESS )
            return err;
    }
#endif

    return 0;
}


int IMROTbl_T1110::selectGains( int slot, int port, int dock ) const
{
#ifdef HAVE_IMEC
// -------------------------
// Set all according to ehdr
// -------------------------

    for( int ic = 0; ic < imType1110Chan; ++ic ) {

        NP_ErrorCode    err;

        err = np_setGain( slot, port, dock, ic,
                gainToIdx( ehdr.apgn ),
                gainToIdx( ehdr.lfgn ) );

//---------------------------------------------------------
// Experiment to visualize LF scambling on shankviewer by
// setting every nth gain high and others low.
#if 0
        int apidx, lfidx;

        if( !(ic % 10) ) {
            apidx = R->gainToIdx( 3000 );
            lfidx = R->gainToIdx( 3000 );
        }
        else {
            apidx = R->gainToIdx( 50 );
            lfidx = R->gainToIdx( 50 );
        }

        err = np_setGain( P.slot, P.port, P.dock, ic,
                apidx,
                lfidx );
#endif
//---------------------------------------------------------

        if( err != SUCCESS )
            return err;
    }
#endif

    return 0;
}


int IMROTbl_T1110::selectAPFlts( int slot, int port, int dock ) const
{
#ifdef HAVE_IMEC
// -------------------------
// Set all according to ehdr
// -------------------------

    for( int ic = 0; ic < imType1110Chan; ++ic ) {

        NP_ErrorCode    err;

        err = np_setAPCornerFrequency( slot, port, dock, ic, !ehdr.apflt );

        if( err != SUCCESS )
            return err;
    }
#endif

    return 0;
}


