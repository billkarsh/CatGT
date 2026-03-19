#ifndef IMROTBL_T3023_H
#define IMROTBL_T3023_H

#include "IMROTbl_T3020base.h"

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

// Neuropixels 3.0 alpha version B multishank silicon cap
//
struct IMROTbl_T3023 : public IMROTbl_T3020base
{
    enum imLims_T3023 {
        imType3023Type  = 3023
    };

    IMROTbl_T3023( const QString &pn )
        :   IMROTbl_T3020base(pn, imType3023Type)   {}

    virtual int typeConst() const       {return imType3023Type;}
    virtual int probeTech() const       {return t_tech_nxt_a1b;}
    virtual int apiFetchType() const    {return t_fetch_api5;}
};

#endif  // IMROTBL_T3023_H


