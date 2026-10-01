#ifndef IMROTBL_T3012_H
#define IMROTBL_T3012_H

#include "IMROTbl_T3010base.h"

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

// Neuropixels NP3 alpha version B single shank metal cap
//
struct IMROTbl_T3012 : public IMROTbl_T3010base
{
    enum imLims_T3012 {
        imType3012Type  = 3012
    };

    IMROTbl_T3012( const QString &pn )
        :   IMROTbl_T3010base(pn, imType3012Type)   {}

    virtual int typeConst() const   {return imType3012Type;}
    virtual int probeTech() const   {return t_tech_np3_a1b;}
};

#endif  // IMROTBL_T3012_H


