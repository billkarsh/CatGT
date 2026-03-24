
#include "IMROTbl_T3023.h"
#ifdef HAVE_API5
#include "IMEC/NeuropixAPI5.h"
#endif
#include "Util.h"   // IWYU pragma: keep

#include <QThread>


/* ---------------------------------------------------------------- */
/* struct IMROTbl ------------------------------------------------- */
/* ---------------------------------------------------------------- */

QString IMROTbl_T3023::selectSites5(
    NeuropixAPI::IProbe*    pr,
    const PAddr&            adr,
    bool                    write,
    bool                    check ) const
{
// ------------------------------------
// Connect all according to table banks
// ------------------------------------

    QString err;

#ifdef HAVE_API5

    for( int ic = 0, nC = nChan(); ic < nC; ++ic ) {

        int shank, bank;

        shank = elShankAndBank( bank, ic );

        try {
            pr->SelectElectrode(
                ic, NeuropixAPI::ElectrodeBank(bank), shank );
        }
        catch( NeuropixAPI::NeuropixException& e ) {
            err =
            QString("API5 SelectElectrode(%1) error %2 '%3'.")
            .arg( adr.tx_spd() )
            .arg( (int)e.GetErrorCode() ).arg( e.what() );
            return err;
        }
    }

    if( write ) {

        for( int itry = 1; itry <= 10; ++itry ) {

            try {
                if( !pr->WriteProbeConfiguration( check ) ) {
                    err =
                    QString("API5 WriteProbeConfiguration(%1) failed.")
                    .arg( adr.tx_spd() );
                    return err;
                }
                else if( itry > 1 ) {
                    Warning() <<
                    QString("API5 WriteProbeConfiguration(%1) took %2 tries.")
                    .arg( adr.tx_spd() ).arg( itry );
                }
            }
            catch( NeuropixAPI::NeuropixException& e ) {
                err =
                QString("API5 WriteProbeConfiguration(%1) error %2 '%3'.")
                .arg( adr.tx_spd() )
                .arg( (int)e.GetErrorCode() ).arg( e.what() );
                return err;
            }

            QThread::msleep( 100 );
        }
    }
#else
    Q_UNUSED( pr )
    Q_UNUSED( adr )
    Q_UNUSED( write )
    Q_UNUSED( check )
#endif

    return err;
}


QString IMROTbl_T3023::selectRefs5(
    NeuropixAPI::IProbe*    pr,
    const PAddr&            adr ) const
{
// -------------------------------
// Disconnect the 4 shank switches
// -------------------------------

    QString err;

#ifdef HAVE_API5

    if( nShank() == 4 ) {

        for( int ic = 0; ic < 4; ++ic ) {

            try {
                pr->SelectReference(
                    ic, NeuropixAPI::ChannelReference::None, ic );
            }
            catch( NeuropixAPI::NeuropixException& e ) {
                err =
                QString("API5 SelectReference(%1) error %2 '%3'.")
                .arg( adr.tx_spd() )
                .arg( (int)e.GetErrorCode() ).arg( e.what() );
                return err;
            }
        }
    }

// ---------------------------------------
// Connect all according to table ref data
// ---------------------------------------

    for( int ic = 0, nC = nChan(); ic < nC; ++ic ) {

        int type, shank, bank;

        type = refTypeAndFields( shank, bank, ic );

        try {
            pr->SelectReference(
                ic, NeuropixAPI::ChannelReference(type), shank );
        }
        catch( NeuropixAPI::NeuropixException& e ) {
            err =
            QString("API5 SelectReference(%1) error %2 '%3'.")
            .arg( adr.tx_spd() )
            .arg( (int)e.GetErrorCode() ).arg( e.what() );
            return err;
        }
    }
#else
    Q_UNUSED( pr )
    Q_UNUSED( adr )
#endif

    return err;
}


