#ifndef CGBL_H
#define CGBL_H

#include "IMROTbl.h"
#include "KVParams.h"

#include <QBitArray>
#include <QFileInfo>
#include <QSet>
#include <QVector>

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
    eXA,
    eXD,
    eXIA,
    eXID,
    eBFT,
    eBFV
} t_ex;

struct CniCfg
{
    enum niTypeId {
        niTypeMN    = 0,
        niTypeMA    = 1,
        niTypeXA    = 2,
        niTypeXD    = 3,
        niSumNeural = 0,
        niSumAnalog = 2,
        niSumAll    = 3,
        niNTypes    = 4
    };
};

struct JSIP {
    int js, ip;
    JSIP()
    :   js(0), ip(0)                            {}
    JSIP( t_js js, int ip, bool LF2AP = false )
    :   js(LF2AP && js==LF ? AP : js), ip(ip)   {}
    bool operator<( const JSIP &rhs ) const
    {
        if( js < rhs.js )
            return true;
        return ip < rhs.ip;
    }
};

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

struct XTR {
// Extractor base class
    double      srate;
    QFile       *f;     // time file
    QTextStream *ts;    // time file
    t_ex        ex;     // cmdline
    t_js        js;     // cmdline
    int         ip,     // cmdline
                word,   // cmdline
                usrord, // cmdline, -1=autosync
                nrun;   // working inarow count
    XTR() : f(0), ts(0), nrun(0)    {}
    virtual ~XTR()                  {}
    bool operator<( const XTR &rhs ) const
        {
            if( js < rhs.js )
                return true;
            else if( js == rhs.js ) {
                if( ip < rhs.ip )
                    return true;
                else if( ip == rhs.ip )
                    return usrord < rhs.usrord;
                else
                    return false;
            }
            else
                return false;
        }
    struct pointerCompare {
        bool operator()( const XTR *L, const XTR *R ) const {return *L < *R;}
    };
    void autoWord( int nC ) {if( word == -1 ) word = nC - 1;}
    void wordError( int nC );
    virtual QString sparam() const = 0;
    virtual QString suffix( const QString &stype ) const = 0;
    virtual void init( double rate, double rangeMax ) = 0;
    virtual bool openOutFiles( int g0 );
    virtual void scan( const qint16 *data, qint64 t0, int ntpts, int nC ) = 0;
    virtual void close() const;
protected:
    bool openOutTimesFile( int g0, t_ex ex );
    void remapped_ip( int k );
};

struct Pulse : public XTR {
// Pulse base class
    double  span,   // cmdline
            tol;    // cmdline, -1 if default
    qint64  edge1,  // marks working edge1
            edge2;  // marks working edge2
    int     spanlo, // tol-adjusted span
            spanhi, // tol-adjusted span
            seek;   // seek: 0=baseline, 1=edge1, 2=edge2, 3=Vtest+edge2
    Pulse() : tol(-1), seek(0)  {}
    virtual ~Pulse()            {}
    QString sSpan() const;
    void setTolerance( double rate );
};

struct A_Pulse : public Pulse {
// Analog Pulse class
    double  thresh, // cmdline
            thrsh2; // cmdline
    int     T,      // binary thresh
            V,      // binary thrsh2
            peak;   // working measured thrsh2
    A_Pulse() : thresh(0.0), thrsh2(0.0)    {}
    virtual ~A_Pulse()                      {}
    virtual QString sparam() const;
    virtual QString suffix( const QString &stype ) const;
    virtual void init( double rate, double rangeMax );
    void pos( const qint16 *data, qint64 t0, int ntpts, int nC );
    void inv( const qint16 *data, qint64 t0, int ntpts, int nC );
    virtual void scan( const qint16 *data, qint64 t0, int ntpts, int nC );
};

struct D_Pulse : public Pulse {
// Digital Pulse class
    int     bit;    // cmdline
    D_Pulse() : bit(-1) {}
    virtual ~D_Pulse()  {}
    virtual QString sparam() const;
    virtual QString suffix( const QString &stype ) const;
    virtual void init( double rate, double rangeMax );
    void pos( const qint16 *data, qint64 t0, int ntpts, int nC );
    void inv( const qint16 *data, qint64 t0, int ntpts, int nC );
    virtual void scan( const qint16 *data, qint64 t0, int ntpts, int nC );
};

struct BitField : public XTR {
// Digital bitfield extractor
    QFile       *fv;    // value file
    QTextStream *tsv;   // value file
    int         b0,     // cmdline
                nb,     // cmdline
                inarow; // cmdline
    uint        mask,
                vrun,   // value of working run
                vlast;  // last reported value
    BitField() : fv(0), tsv(0), vlast(-1)   {}
    virtual ~BitField()                     {}
    virtual QString sparam() const;
    virtual QString suffix( const QString &stype ) const;
    virtual void init( double rate, double rangeMax );
    virtual bool openOutFiles( int g0 );
    virtual void scan( const qint16 *data, qint64 t0, int ntpts, int nC );
    virtual void close() const;
};

struct Save {
// selective -save directive
    QVector<uint>   iKeep;      // indices relative to infile samples
    QFile           *o_f;
    QString         o_name,
                    sUsr_out,   // only for snsSaveChanSubset
                    sUsr;       // cmdline
    t_js            js;         // cmdline
    int             ip1,        // cmdline
                    ip2,        // cmdline
                    nC,         // iKeep.size
                    nN,         // neurals within iKeep
                    smpBytes;
    Save() : o_f(0), js(AP), ip1(0), ip2(0), nN(0)              {}
    Save( t_js js, int ip1, int ip2, const QString &s )
        :   o_f(0), sUsr(s), js(js), ip1(ip1), ip2(ip2), nN(0)  {}
    virtual ~Save()                                             {close();}
    bool operator<( const Save &rhs ) const
        {
            if( js < rhs.js )
                return true;
            else if( js == rhs.js ) {
                if( ip1 < rhs.ip1 )
                    return true;
                else if( ip1 == rhs.ip1 )
                    return ip2 < rhs.ip2;
                else
                    return false;
            }
            else
                return false;
        }
    static bool parse( const char *s );
    QString sparam() const;
    bool init( const KVParams &kvp, const QFileInfo &fim, int theZ );
    bool o_open( int g0, t_js js );
    void close();
};

struct SepShanks {
// -sepShanks directive
    QString     sUsr;
    int         ip,
                ipj[4];
    SepShanks()                             {}
    SepShanks( const QString &s ) : sUsr(s) {}
    virtual ~SepShanks()                    {}
    bool operator<( const SepShanks &rhs ) const
        {
            return (ip < rhs.ip);
        }
    bool parse( QSet<int> &seen );
    QString sparam() const;
    bool split( const KVParams &kvp, const QFileInfo &fim );
};

struct MaxZ {
// -maxZ directive
    double      z;
    QBitArray   bitsAP;
    QString     sUsr;
    int         ip,
                type;
    MaxZ() : ip(0)  {}
    MaxZ( const QString &s ) : sUsr(s)  {}
    virtual ~MaxZ()                     {}
    bool operator<( const MaxZ &rhs ) const
        {
            return (ip < rhs.ip);
        }
    bool parse( QSet<int> &seen );
    QString sparam() const;
    bool apply(
        const KVParams  &kvp,
        const QFileInfo &fim,
        int             js_in,
        int             js_out );
};

struct Elem {
// supercat element
    QMap<JSIP,double>   jsip2rate,  // ip1-indexed from fyi
                        jsip2head,  // ip1-indexed
                        jsip2tail;  // ip1-indexed
    QMap<JSIP,int>      jsip2nchn;  // ip1-indexed from fyi
    QMap<int,int>       mip2ip1;    // ip2-indexed from fyi
    QString             dir,
                        run;
    int                 g;
    bool                no_run_fld,
                        in_catgt_fld;
    Elem()
        :   g(0), no_run_fld(false), in_catgt_fld(false)        {}
    Elem(
        const QString   &dir,
        const QString   &run,
        int             g,
        bool            no_run_fld,
        bool            in_catgt_fld )
        :   dir(dir), run(run), g(g),
            no_run_fld(no_run_fld), in_catgt_fld(in_catgt_fld)  {}
    double& rate( t_js js, int ip ) {return jsip2rate[JSIP(js,ip)];}
    double& head( t_js js, int ip ) {return jsip2head[JSIP(js,ip,true)];}
    double& tail( t_js js, int ip ) {return jsip2tail[JSIP(js,ip,true)];}
    int&    nC( t_js js, int ip )   {return jsip2nchn[JSIP(js,ip)];}
    bool read_fyi();
    void unpack();
};

class CGBL
{
public:
    double              syncper,        // measured in supercat_trim
                        startsecs,
                        maxsecs,
                        locin_um,
                        locout_um,
                        gfixamp,        // amplitude
                        gfixslp,        // slope
                        gfixbas;        // baseline
    Filter              apflt,
                        lfflt;
    QString             sCmd,
                        run,
                        inpar,          // derived
                        dest,
                        aux_obase,      // derived
                        im_obase,       // derived
                        prb_obase;      // derived
    QMap<int,QVector<uint>> mexc;
    QMap<JSIP,double>   mjsiprate;      // pass-1 -> fyi
    QMap<JSIP,int>      mjsipnchn;      // pass-1 -> fyi
    QMap<int,int>       mip2ip1;        // pass-1 -> fyi
    QVector<GT3>        gtlist;
    QVector<uint>       vobx,
                        vprb;
    QVector<XTR*>       vX;
    QVector<Save>       vS;
    QVector<SepShanks>  vSK;
    QVector<MaxZ>       vMZ;
    QVector<Elem>       velem;
    QSet<int>           set_ip1;        // unique ip1 for pass-2
    KVParams            fyi;            // generated
    int                 ga,
                        gb,
                        ta,
                        tb,
                        zfilmax,
                        locin,
                        locout,
                        inarow;
    bool                no_run_fld,
                        prb_fld,
                        prb_miss_ok,
                        exported,
                        in_catgt_fld,   // derived
                        t_miss_ok,
                        ni,
                        ob,
                        ap,
                        lf,
                        prb_3A,
                        tshift,
                        linefil,
                        gblcar,
                        gbldmx,
                        gfixdo,
                        auto_sync,
                        force_ni_ob,
                        sc_trim,
                        sc_skipbin,
                        no_catgt_fld,
                        out_prb_fld;

public:
    CGBL()
        :   syncper(-1), startsecs(-1), maxsecs(0),
            locin_um(0), locout_um(0),
            gfixamp(0), gfixslp(0), gfixbas(0),
            ga(-1), gb(-1), ta(-2), tb(-2),
            zfilmax(-1), locin(0), locout(0), inarow(-1),
            no_run_fld(false), prb_fld(false), prb_miss_ok(false),
            exported(false), in_catgt_fld(false), t_miss_ok(false),
            ni(false), ob(false), ap(false), lf(false), prb_3A(false),
            tshift(true), linefil(true), gblcar(false), gbldmx(false),
            gfixdo(false), auto_sync(true), force_ni_ob(false),
            sc_trim(false), sc_skipbin(false), no_catgt_fld(false),
            out_prb_fld(false)  {}

    bool SetCmdLine( int argc, char* argv[] );

    int pass() const    {return (velem.size() ? 2 : 1);}

    bool gt_parse_list( const QString &s );
    void gt_set_tcat();
    bool gt_is_tcat() const;
    bool gt_ok_indices() const;
    QString gt_format_params() const;
    int gt_get_first( int *t ) const;
    int gt_nIndices() const;

    int myXrange( int &lim, t_js js, int ip ) const;
    int mySrange( int &lim, t_js js, int ip ) const;

    void fyi_ct_write();
    void fyi_sc_write();

    bool makeOutputProbeFolder( int g0, int ip1 );

    QString inFile( int g, int t, t_js js, int ip1, int ip2, t_ex ex, XTR *X = 0 );
    QString niOutFile( int g0, t_ex ex, XTR *X = 0 );
    QString obOutFile( int g0, int ip, t_ex ex, XTR *X = 0 );
    QString imOutFile( int g0, t_js js, int ip1, int ip2, t_ex ex, XTR *X = 0 );

    bool openOutputBinary(
        QFile       &fout,
        QString     &outBin,
        int         g0,
        t_js        js,
        int         ip1,
        int         ip2 );

    int openInputFile(
        QFile       &fin,
        QFileInfo   &fib,
        int         g,
        int         t,
        t_js        js,
        int         ip1,
        int         ip2,
        t_ex        ex,
        XTR         *X = 0 );

    int openInputBinary(
        QFile       &fin,
        QFileInfo   &fib,
        int         g,
        int         t,
        t_js        js,
        int         ip1,
        int         ip2 );

    int openInputMeta(
        QFileInfo   &fim,
        KVParams    &kvp,
        int         g,
        int         t,
        t_js        js,
        int         ip1,
        int         ip2,
        bool        canSkip );

    IMROTbl *getProbe( const KVParams &kvp );

    bool getSavedChannels(
        QVector<uint>   &snsFileChans,
        const KVParams  &kvp,
        const QFileInfo &fim );

private:
    bool parseChnexcl( const QString &s );
    bool parseSepShanks();
    bool parseMaxZ();
    bool parseElems( const QString &s );
    bool checkExtractors();
    bool checkSaves();
    QString formatChnexcl();
    QString formatElems();
    bool pass1FromCatGT();
    bool makeTaggedDest();
    void parseNiChanCounts(
        int             (&niCumTypCnt)[CniCfg::niNTypes],
        const KVParams  &kvp );
    bool addAutoExtractors();
    QString trim_adjust_slashes( const QString &dir );
    QString inPath( int g, t_js js, int ip1 );
    QString inPathUpTo_t( int g, t_js js, int ip1 );
    QString suffix( t_js js, int ip2, t_ex ex, XTR *X = 0 );
};

/* --------------------------------------------------------------- */
/* Globals ------------------------------------------------------- */
/* --------------------------------------------------------------- */

extern CGBL GBL;

#endif  // CGBL_H


