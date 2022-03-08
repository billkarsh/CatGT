#ifndef CGBL_H
#define CGBL_H

#include <QMap>
#include <QVector>

class QFile;
class QTextStream;

/* ---------------------------------------------------------------- */
/* Types ---------------------------------------------------------- */
/* ---------------------------------------------------------------- */

typedef enum {
    NI      = 0,
    OB,
    AP,
    LF
} t_js;

typedef enum {
    eBIN    = 0,
    eMETA,
    eSY,
    eXD,
    eXA,
    eiSY,
    eiXD,
    eiXA,
    eBFT,
    eBFV
} t_ex;

struct GT3 {
    int g, ta, tb;
};

struct GT_iterator {
    int n, icur;
    GT3 ecur;
    GT_iterator();
    bool next( int &g, int &t );
};

struct Filter {
    double  Fhi,
            Flo;
    QString type;
    int     order;
    Filter()            {clear();}
    void clear()        {Fhi = 0; Flo = 0; type.clear(); order = 0;}
    bool isenabled()    {return order != 0;}
    bool isbiquad()     {return type == "biquad";}
    bool hasbiquadhp()  {return isbiquad()  && Fhi != 0;}
    bool haslopass()    {return isenabled() && Flo != 0;}
    bool needsfft()     {return isenabled() && !isbiquad();}
    bool parse( const QString &s );
    QString format()    {return QString("%1,%2,%3,%4")
                                    .arg( type ).arg( order )
                                    .arg( Fhi ).arg( Flo );}
};

struct LR {
// artifact left, right widths
    int L, R;
    LR()                            {}
    LR( int L, int R ) : L(L), R(R) {}
};

struct XCT {
// extractor base class
    double      srate;
    QFile       *f;
    QTextStream *ts;
    int         word,   // cmdline
                nrun;   // working in a row count
    XCT() : f(0), ts(0), nrun(0)    {}
    virtual ~XCT()                  {}
    void autoWord( int nC ) {if( word == -1 ) word = nC - 1;}
    virtual QString suffix( const QString &stype ) = 0;
    bool openOutTimesFile( const QString &file );
    virtual void close();
};

struct TTL : public XCT {
// TTL base class
    double      span,   // cmdline
                tol;    // cmdline, -1 if default
    qint64      edge1,  // marks working edge1
                edge2;  // marks working edge2
    int         spanlo,
                spanhi,
                seek;   // seek: 0=baseline, 1=edge1, 2=edge2, 3=Vtest+edge2
    TTL() : tol(-1), seek(0)    {}
    virtual ~TTL()              {}
    QString sSpan();
    void setTolerance( double rate );
};

struct TTLA : public TTL {
// Analog TTL class
    double  thresh, // cmdline
            thrsh2; // cmdline
    int     T,      // binary thresh
            V,      // binary thrsh2
            peak;   // working measured thrsh2
    TTLA() : thresh(0.0), thrsh2(0.0)   {}
    virtual ~TTLA()                     {}
    QString sparam( const QString &stype );
    virtual QString suffix( const QString &stype );
    void XA( const qint16 *data, qint64 t0, int ntpts, int nC );
    void iXA( const qint16 *data, qint64 t0, int ntpts, int nC );
};

struct TTLD : public TTL {
// Digital TTL class
    int     ip,     // cmdline, SY only
            bit;    // cmdline
    TTLD() : ip(-1), bit(-1)    {}
    virtual ~TTLD()             {}
    QString sparam( const QString &stype );
    virtual QString suffix( const QString &stype );
    void XD( const qint16 *data, qint64 t0, int ntpts, int nC );
    void iXD( const qint16 *data, qint64 t0, int ntpts, int nC );
};

struct XBF : public XCT {
// Digital bitfield extractor
    QFile       *fv;    // value file
    QTextStream *tsv;
    int         b0,     // cmdline
                nb,     // cmdline
                inarow; // cmdline
    uint        mask,
                vrun,   // value of working run
                vlast;  // last reported value
    XBF() : fv(0), tsv(0), vlast(-1)    {}
    virtual ~XBF()                      {}
    void initMask( double rate );
    QString sparam();
    virtual QString suffix( const QString &stype );
    void BF( const qint16 *data, qint64 t0, int ntpts, int nC );
    bool openOutValsFile( const QString &file );
    virtual void close();
};

struct Elem {
    QMap<int,double>    iq2head,
                        iq2tail;
    QString             dir,
                        run;
    int                 g;
    bool                no_run_fld,
                        catgt_fld;
    Elem()
        :   g(0), no_run_fld(false), catgt_fld(false)       {}
    Elem(
        const QString   &dir,
        const QString   &run,
        int             g,
        bool            no_run_fld,
        bool            catgt_fld )
        :   dir(dir), run(run), g(g),
            no_run_fld(no_run_fld), catgt_fld(catgt_fld)    {}
    double& head( t_js js, int ip ) {return iq2head[1000*js + ip];}
    double& tail( t_js js, int ip ) {return iq2tail[1000*js + ip];}
    void unpack();
};

class CGBL
{
public:
    double          syncper,        // measured in supercat_trim
                    maxsecs,
                    gfixamp,        // amplitude
                    gfixslp,        // slope
                    gfixbas;        // baseline
    Filter          apflt,
                    lfflt;
    QString         sCmd,
                    run,
                    inpar,          // derived
                    opar,           // derived
                    aux_obase,      // derived
                    im_obase,       // derived
                    prb_obase;      // derived
    QMap<int,QVector<uint>> mexc;
    QVector<GT3>    gtlist;
//@OBX Sym GBL.vobx <-> GBL.vprb
    QVector<uint>   vprb,
                    vobx;
//@OBX Need (js,ip) universal extractors
    QVector<TTLD>   SY,
                    iSY,
                    XD,
                    iXD;
    QVector<TTLA>   XA,
                    iXA;
    QVector<XBF>    BF;
    QVector<Elem>   velem;
    int             ga,
                    gb,
                    ta,
                    tb,
                    zfilmax,
                    locin,
                    locout,
                    inarow;
    bool            no_run_fld,
                    prb_fld,
                    prb_miss_ok,
                    exported,
                    catgt_fld,      // derived
                    t_miss_ok,
                    ap,
                    lf,
                    ob,
//@OBX Sym GBL.ob <-> GBL.ni
                    ni,
                    prb_3A,
                    tshift,
                    gblcar,
                    gfixdo,
                    force_ni_ob,
                    sc_trim,
                    sc_skipbin,
                    out_prb_fld;

public:
    CGBL()
        :   syncper(-1), maxsecs(0),
            gfixamp(0), gfixslp(0), gfixbas(0),
            ga(-1), gb(-1), ta(-2), tb(-2),
            zfilmax(-1), locin(0), locout(0), inarow(-1),
            no_run_fld(false), prb_fld(false), prb_miss_ok(false),
            exported(false), catgt_fld(false), t_miss_ok(false),
            ap(false), lf(false), ob(false), ni(false), prb_3A(false),
            tshift(true), gblcar(false), gfixdo(false),
            force_ni_ob(false), sc_trim(false), sc_skipbin(false),
            out_prb_fld(false)  {}

    bool SetCmdLine( int argc, char* argv[] );

    bool gt_parse_list( const QString &s );
    void gt_set_tcat();
    bool gt_is_tcat() const;
    bool gt_ok_indices() const;
    QString gt_format_params() const;
    int gt_get_first( int *t ) const;
    int gt_nIndices() const;

    bool makeOutputProbeFolder( int g0, int ip );

    QString inFile( int g, int t, t_js js, int ip, t_ex ex, XCT *X = 0 );
    QString niOutFile( int g0, t_ex ex, XCT *X = 0 );
    QString obOutFile( int g0, int ip, t_ex ex, XCT *X = 0 );
    QString imOutFile( int g0, t_js js, int ip, t_ex ex, XCT *X = 0 );

private:
    bool parseChnexcl( const QString &s );
    bool parseElems( const QString &s );
    QString formatChnexcl();
    QString formatElems();
    bool pass1FromCatGT();
    bool makeTaggedDest();
    QString trim_adjust_slashes( const QString &dir );
    QString inPathUpTo_t( int g, t_js js, int ip );
    QString suffix( t_js js, int ip, t_ex ex, XCT *X = 0 );
};

/* --------------------------------------------------------------- */
/* Globals ------------------------------------------------------- */
/* --------------------------------------------------------------- */

extern CGBL GBL;

#endif  // CGBL_H


