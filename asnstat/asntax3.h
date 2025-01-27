/***********************************************************************
*
**
*        Automatic header module from ASNTOOL
*
************************************************************************/

#ifndef _ASNTOOL_
#include <asn.h>
#endif

static char * asnfilename = "asntax3.h13";
static AsnValxNode avnx[5] = {
    {20,"none" ,0,0.0,&avnx[1] } ,
    {20,"info" ,1,0.0,&avnx[2] } ,
    {20,"warn" ,2,0.0,&avnx[3] } ,
    {20,"error" ,3,0.0,&avnx[4] } ,
    {20,"fatal" ,4,0.0,NULL } };

static AsnType atx[51] = {
  {401, "Org-ref" ,1,0,0,0,0,0,1,0,NULL,NULL,NULL,0,&atx[1]} ,
  {402, "Taxon3-request" ,1,0,0,0,0,0,0,0,NULL,&atx[15],&atx[2],0,&atx[4]} ,
  {0, "request" ,128,0,0,0,0,0,0,0,NULL,&atx[13],&atx[3],0,NULL} ,
  {0, NULL,1,-1,0,0,0,0,0,0,NULL,&atx[4],NULL,0,NULL} ,
  {403, "T3Request" ,1,0,0,0,0,0,0,0,NULL,&atx[14],&atx[5],0,&atx[11]} ,
  {0, "taxid" ,128,0,0,0,0,0,0,0,NULL,&atx[6],NULL,0,&atx[7]} ,
  {302, "INTEGER" ,0,2,0,0,0,0,0,0,NULL,NULL,NULL,0,NULL} ,
  {0, "name" ,128,1,0,0,0,0,0,0,NULL,&atx[8],NULL,0,&atx[9]} ,
  {323, "VisibleString" ,0,26,0,0,0,0,0,0,NULL,NULL,NULL,0,NULL} ,
  {0, "org" ,128,2,0,0,0,0,0,0,NULL,&atx[0],NULL,0,&atx[10]} ,
  {0, "join" ,128,3,0,0,0,0,0,0,NULL,&atx[11],NULL,0,NULL} ,
  {404, "SequenceOfInt" ,1,0,0,0,0,0,0,0,NULL,&atx[13],&atx[12],0,&atx[16]} ,
  {0, NULL,1,-1,0,0,0,0,0,0,NULL,&atx[6],NULL,0,NULL} ,
  {312, "SEQUENCE OF" ,0,16,0,0,0,0,0,0,NULL,NULL,NULL,0,NULL} ,
  {315, "CHOICE" ,0,-1,0,0,0,0,0,0,NULL,NULL,NULL,0,NULL} ,
  {311, "SEQUENCE" ,0,16,0,0,0,0,0,0,NULL,NULL,NULL,0,NULL} ,
  {405, "Taxon3-reply" ,1,0,0,0,0,0,0,0,NULL,&atx[15],&atx[17],0,&atx[19]} ,
  {0, "reply" ,128,0,0,0,0,0,0,0,NULL,&atx[13],&atx[18],0,NULL} ,
  {0, NULL,1,-1,0,0,0,0,0,0,NULL,&atx[19],NULL,0,NULL} ,
  {406, "T3Reply" ,1,0,0,0,0,0,0,0,NULL,&atx[14],&atx[20],0,&atx[21]} ,
  {0, "error" ,128,0,0,0,0,0,0,0,NULL,&atx[21],NULL,0,&atx[28]} ,
  {407, "T3Error" ,1,0,0,0,0,0,0,0,NULL,&atx[15],&atx[22],0,&atx[29]} ,
  {0, "level" ,128,0,0,0,0,0,0,0,NULL,&atx[23],&avnx[0],0,&atx[24]} ,
  {310, "ENUMERATED" ,0,10,0,0,0,0,0,0,NULL,NULL,NULL,0,NULL} ,
  {0, "message" ,128,1,0,0,0,0,0,0,NULL,&atx[8],NULL,0,&atx[25]} ,
  {0, "taxid" ,128,2,0,1,0,0,0,0,NULL,&atx[6],NULL,0,&atx[26]} ,
  {0, "name" ,128,3,0,1,0,0,0,0,NULL,&atx[8],NULL,0,&atx[27]} ,
  {0, "org" ,128,4,0,1,0,0,0,0,NULL,&atx[0],NULL,0,NULL} ,
  {0, "data" ,128,1,0,0,0,0,0,0,NULL,&atx[29],NULL,0,NULL} ,
  {408, "T3Data" ,1,0,0,0,0,0,0,0,NULL,&atx[15],&atx[30],0,&atx[35]} ,
  {0, "org" ,128,0,0,0,0,0,0,0,NULL,&atx[0],NULL,0,&atx[31]} ,
  {0, "blast-name-lineage" ,128,1,0,1,0,0,0,0,NULL,&atx[13],&atx[32],0,&atx[33]} ,
  {0, NULL,1,-1,0,0,0,0,0,0,NULL,&atx[8],NULL,0,NULL} ,
  {0, "status" ,128,2,0,1,0,0,0,0,NULL,&atx[13],&atx[34],0,&atx[42]} ,
  {0, NULL,1,-1,0,0,0,0,0,0,NULL,&atx[35],NULL,0,NULL} ,
  {409, "T3StatusFlags" ,1,0,0,0,0,0,0,0,NULL,&atx[15],&atx[36],0,&atx[43]} ,
  {0, "property" ,128,0,0,0,0,0,0,0,NULL,&atx[8],NULL,0,&atx[37]} ,
  {0, "value" ,128,1,0,0,0,0,0,0,NULL,&atx[14],&atx[38],0,NULL} ,
  {0, "bool" ,128,0,0,0,0,0,0,0,NULL,&atx[39],NULL,0,&atx[40]} ,
  {301, "BOOLEAN" ,0,1,0,0,0,0,0,0,NULL,NULL,NULL,0,NULL} ,
  {0, "int" ,128,1,0,0,0,0,0,0,NULL,&atx[6],NULL,0,&atx[41]} ,
  {0, "str" ,128,2,0,0,0,0,0,0,NULL,&atx[8],NULL,0,NULL} ,
  {0, "refresh" ,128,3,0,1,0,0,0,0,NULL,&atx[43],NULL,0,NULL} ,
  {410, "T3RefreshFlags" ,1,0,0,0,0,0,0,0,NULL,&atx[15],&atx[44],0,NULL} ,
  {0, "taxid-changed" ,128,0,0,0,0,0,0,0,NULL,&atx[39],NULL,0,&atx[45]} ,
  {0, "taxname-changed" ,128,1,0,0,0,0,0,0,NULL,&atx[39],NULL,0,&atx[46]} ,
  {0, "division-changed" ,128,2,0,0,0,0,0,0,NULL,&atx[39],NULL,0,&atx[47]} ,
  {0, "lineage-changed" ,128,3,0,0,0,0,0,0,NULL,&atx[39],NULL,0,&atx[48]} ,
  {0, "gc-changed" ,128,4,0,0,0,0,0,0,NULL,&atx[39],NULL,0,&atx[49]} ,
  {0, "mgc-changed" ,128,5,0,0,0,0,0,0,NULL,&atx[39],NULL,0,&atx[50]} ,
  {0, "orgmod-changed" ,128,6,0,0,0,0,0,0,NULL,&atx[39],NULL,0,NULL} };

static AsnModule ampx[1] = {
  { "NCBI-Taxon3" , "asntax3.h13",&atx[0],NULL,NULL,0,0} };

static AsnValxNodePtr avn = avnx;
static AsnTypePtr at = atx;
static AsnModulePtr amp = ampx;



/**************************************************
*
*    Defines for Module NCBI-Taxon3
*
**************************************************/

#define TAXON3_REQUEST &at[1]
#define TAXON3_REQUEST_request &at[2]
#define TAXON3_REQUEST_request_E &at[3]

#define T3REQUEST &at[4]
#define T3REQUEST_taxid &at[5]
#define T3REQUEST_name &at[7]
#define T3REQUEST_org &at[9]
#define T3REQUEST_join &at[10]

#define SEQUENCEOFINT &at[11]
#define SEQUENCEOFINT_E &at[12]

#define TAXON3_REPLY &at[16]
#define TAXON3_REPLY_reply &at[17]
#define TAXON3_REPLY_reply_E &at[18]

#define T3REPLY &at[19]
#define T3REPLY_error &at[20]
#define T3REPLY_data &at[28]

#define T3ERROR &at[21]
#define T3ERROR_level &at[22]
#define T3ERROR_message &at[24]
#define T3ERROR_taxid &at[25]
#define T3ERROR_name &at[26]
#define T3ERROR_org &at[27]

#define T3DATA &at[29]
#define T3DATA_org &at[30]
#define T3DATA_blast_name_lineage &at[31]
#define T3DATA_blast_name_lineage_E &at[32]
#define T3DATA_status &at[33]
#define T3DATA_status_E &at[34]
#define T3DATA_refresh &at[42]

#define T3STATUSFLAGS &at[35]
#define T3STATUSFLAGS_property &at[36]
#define T3STATUSFLAGS_value &at[37]
#define T3STATUSFLAGS_value_bool &at[38]
#define T3STATUSFLAGS_value_int &at[40]
#define T3STATUSFLAGS_value_str &at[41]

#define T3REFRESHFLAGS &at[43]
#define T3REFRESHFLAGS_taxid_changed &at[44]
#define T3REFRESHFLAGS_taxname_changed &at[45]
#define T3REFRESHFLAGS_division_changed &at[46]
#define T3REFRESHFLAGS_lineage_changed &at[47]
#define T3REFRESHFLAGS_gc_changed &at[48]
#define T3REFRESHFLAGS_mgc_changed &at[49]
#define T3REFRESHFLAGS_orgmod_changed &at[50]
