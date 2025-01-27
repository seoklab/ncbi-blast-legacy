/*  seqmgr.c
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE                          
*               National Center for Biotechnology Information
*                                                                          
*  This software/database is a "United States Government Work" under the   
*  terms of the United States Copyright Act.  It was written as part of    
*  the author's official duties as a United States Government employee and 
*  thus cannot be copyrighted.  This software/database is freely available 
*  to the public for use. The National Library of Medicine and the U.S.    
*  Government have not placed any restriction on its use or reproduction.  
*                                                                          
*  Although all reasonable efforts have been taken to ensure the accuracy  
*  and reliability of the software and data, the NLM and the U.S.          
*  Government do not and cannot warrant the performance or results that    
*  may be obtained by using this software or data. The NLM and the U.S.    
*  Government disclaim all warranties, express or implied, including       
*  warranties of performance, merchantability or fitness for any particular
*  purpose.                                                                
*                                                                          
*  Please cite the author in any work or product based on this material.   
*
* ===========================================================================
*
* File Name:  seqmgr.c
*
* Author:  James Ostell
*   
* Version Creation Date: 9/94
*
* $Revision: 6.321 $
*
* File Description:  Manager for Bioseqs and BioseqSets
*
* Modifications:  
* --------------------------------------------------------------------------
* Date       Name        Description of modification
* -------  ----------  -----------------------------------------------------
*
*
*
* ==========================================================================
*/

/** for ErrPostEx() ****/

static char *this_module = "ncbiapi";
#define THIS_MODULE this_module
static char *this_file = __FILE__;
#define THIS_FILE this_file

/**********************/

#include <explore.h>       /* new public functions prototyped here */
#include <seqmgr.h>        /* the interface */
#include <sequtil.h>       /* CLEAN THIS UP LATER? */
#include <gather.h>
#include <subutil.h>
#include <ncbithr.h>
#include <objfdef.h>
#include <sqnutils.h>
#include <seqport.h>
#include <edutil.h>
#include <alignmgr2.h>

/*****************************************************************************
*
*   Bioseq Management
*
*****************************************************************************/

static BioseqPtr LIBCALLBACK BSFetchFunc PROTO((SeqIdPtr sid, Uint1 ld_type));
static BioseqPtr NEAR BioseqFindFunc PROTO((SeqIdPtr sid, Boolean reload_from_cache, Boolean force_it, Boolean use_bioseq_cache));
static Boolean NEAR SeqMgrGenericSelect PROTO((SeqLocPtr region, Int2 type,
                                             Uint1Ptr rgb));
static BioseqPtr NEAR BioseqReloadFunc PROTO((SeqIdPtr sid, ObjMgrDataPtr omdp));

static Boolean NEAR SeqMgrProcessNonIndexedBioseq PROTO((Boolean force_it));
static Boolean NEAR SeqMgrAddIndexElement PROTO((SeqMgrPtr smp, BioseqPtr bsp, CharPtr buf,
                                                  Boolean sort_now));
static void NEAR RevStringUpper PROTO((CharPtr str));
static BSFetchTop NEAR SeqMgrGetFetchTop (void);


/*****************************************************************************
*
*   Return the current SeqMgr
*       SeqMgrGet is obsolete
*       SeqMgrReadLock, ReadUnlock, WriteLock, WriteUnlock are thread safe
*
*****************************************************************************/
static TNlmMutex smp_mutex = NULL;
static SeqMgrPtr global_smp = NULL;
static TNlmRWlock smp_RWlock = NULL;
static TNlmRWlock sgi_RWlock = NULL;

/*****************************************************************************
*
*   Return the current SeqMgr
*       Initialize if not done already
*       This function will become obsolete
*
*****************************************************************************/
NLM_EXTERN SeqMgrPtr LIBCALL SeqMgrGet (void)
{
    Int4 ret;
    SeqMgrPtr smp;

    if (global_smp != NULL)
        return global_smp;

    ret = NlmMutexLockEx(&smp_mutex);  /* protect this section */
    if (ret)  /* error */
    {
        ErrPostEx(SEV_FATAL,0,0,"SeqMgrGet failed [%ld]", (long)ret);
        return NULL;
    }

    if (global_smp == NULL)  /* check again after mutex */
    {
                                 /*** have to initialize it **/
        smp = (SeqMgrPtr) MemNew (sizeof(SeqMgr));
        smp->bsfetch = BSFetchFunc;  /* BioseqFetch default */
        smp->fetch_on_lock = TRUE;     /* fetch when locking */
        smp_RWlock = NlmRWinit();  /* initialize RW lock */
        sgi_RWlock = NlmRWinit();  /* initialize RW lock */
        global_smp = smp;       /* do this last for mutex safety */
    }

    NlmMutexUnlock(smp_mutex);

    return global_smp;
}

/*****************************************************************************
*
*   SeqMgrReadLock()
*       Initialize if not done already
*       A thread can have only one read or write lock at a time
*       Many threads can have read locks
*       Only one thread can have a write lock
*       No other threads may have read locks if a write lock is granted
*       If another thread holds a write lock, this call blocks until write
*          is unlocked.
*
*****************************************************************************/
NLM_EXTERN SeqMgrPtr LIBCALL SeqMgrReadLock (void)
{
    SeqMgrPtr smp;
    Int4 ret;

    smp = SeqMgrGet();  /* ensure initialization */

    ret = NlmRWrdlock(smp_RWlock);
    if (ret != 0)
    {
        ErrPostEx(SEV_ERROR,0,0,"SeqMgrReadLock: RWrdlock error [%ld]",
            (long)ret);
        return NULL;
    }
    return smp;
}

/*****************************************************************************
*
*   SeqMgrWriteLock
*       Initialize if not done already
*       A thread can have only one read or write lock at a time
*       Many threads can have read locks
*       Only one thread can have a write lock
*       No other threads may have read locks if a write lock is granted
*       If another thread holds a read or write lock, this call blocks until write
*          is unlocked.
*
*****************************************************************************/
NLM_EXTERN SeqMgrPtr LIBCALL SeqMgrWriteLock (void)
{
    SeqMgrPtr smp;
    Int4 ret;

    smp = SeqMgrGet();  /* ensure initialization */

    ret = NlmRWwrlock(smp_RWlock);
    if (ret != 0)
    {
        ErrPostEx(SEV_ERROR,0,0,"SeqMgrWriteLock: RWwrlock error [%ld]",
            (long)ret);
        return NULL;
    }
    smp->is_write_locked = TRUE;
    return smp;
}


/*****************************************************************************
*
*  SeqMgrUnlock()
*
*****************************************************************************/
NLM_EXTERN Boolean LIBCALL SeqMgrUnlock (void)
{
    SeqMgrPtr smp;
    Int4 ret;

    smp = SeqMgrGet();  /* ensure initialization */

    ret = NlmRWunlock(smp_RWlock);
    if (ret != 0)
    {
        ErrPostEx(SEV_ERROR,0,0,"SeqMgrUnlock: RWunlock error [%ld]",
            (long)ret);
        return FALSE;
    }
    smp->is_write_locked = FALSE;  /* can't be write locked */
    return TRUE;
}

/****************************************************************************
*
*  RevStringUpper(str)
*    Up cases and reverses string
*      to get different parts early for SeqId StringCmps
*
*****************************************************************************/
static void NEAR RevStringUpper (CharPtr str)
{
    CharPtr nd;
    Char tmp;

        if (str == NULL)
            return;
    nd = str;
    while (*nd != '\0')
        nd++;
    nd--;

    while (nd > str)
    {
        tmp = TO_UPPER(*nd);
        *nd = TO_UPPER(*str);
        *str = tmp;
        nd--; str++;
    }

    if (nd == str)
        *nd = TO_UPPER(*nd);
    return;
}

NLM_EXTERN Boolean MakeReversedSeqIdString (SeqIdPtr sid, CharPtr buf, size_t len)

{
  Uint1         oldchoice;
  CharPtr       tmp;
  TextSeqIdPtr  tsip;

  if (sid == NULL || buf == NULL || len < 1) return FALSE;
  oldchoice = 0;
  switch (sid->choice) {
    case SEQID_GI:
      sprintf (buf, "%ld", (long)(sid->data.intvalue));
      break;
    case SEQID_EMBL:
    case SEQID_DDBJ:
      oldchoice = sid->choice;
      sid->choice = SEQID_GENBANK;
    case SEQID_GENBANK:
    case SEQID_PIR:
    case SEQID_OTHER:
    case SEQID_SWISSPROT:
    case SEQID_PRF:
    case SEQID_TPG:
    case SEQID_TPE:
    case SEQID_TPD:
    case SEQID_GPIPE:
    case SEQID_NAMED_ANNOT_TRACK:
      tsip = (TextSeqIdPtr) (sid->data.ptrvalue);
      if (tsip->accession != NULL) {
        tmp = tsip->name;
        tsip->name = NULL;
        SeqIdWrite (sid, buf, PRINTID_FASTA_SHORT, len);
        tsip->name = tmp;
      } else {
        SeqIdWrite (sid, buf, PRINTID_FASTA_SHORT, len);
      }
      if (oldchoice)
        sid->choice = oldchoice;
      break;
    default:
      SeqIdWrite (sid, buf, PRINTID_FASTA_SHORT, len);
      break;
  }
  RevStringUpper (buf);
  return TRUE;
}

/*****************************************************************************
*
*   SeqEntrySetScope(sep)
*       scopes global seqentry searches to sep
*       setting sep=NULL, opens scope to all seqentries in memory
*       returns the current scope
*
*****************************************************************************/
NLM_EXTERN SeqEntryPtr LIBCALL SeqEntrySetScope(SeqEntryPtr sep)
{
    SeqEntryPtr curr = NULL;
    SeqMgrPtr smp;
    Int2 i, j;
    SMScopePtr smsp;
    TNlmThread thr;
    Boolean found;

    smp = SeqMgrWriteLock();
    if (smp == NULL) goto ret;
    thr = NlmThreadSelf();
    found = FALSE;
    for (i = 0, smsp = smp->scope; i < smp->num_scope; i++, smsp++)
    {
        if (NlmThreadCompare(thr, smsp->thr))
        {
            curr = smsp->SEscope;
            smsp->SEscope = sep;
            if (sep == NULL)  /* removing one? */
            {
                smp->num_scope--;
                j = smp->num_scope - i;  /* number to move */
                if (j)  /* not last one */
                    MemCopy(smsp, (smsp+1), (size_t)(j * sizeof(SMScope)));
            }
            goto ret;    /* all done */
        }
    }

                  /* thread not on list */
    if (sep == NULL)
        goto ret;       /* nothing to do */

    i = smp->num_scope;
    j = smp->total_scope;
    if (j == i)  /* need more room */
    {
        j += 20;   /* new size */
        smsp = smp->scope;
        smp->scope = MemNew((size_t)(j * sizeof(SMScope)));
        MemCopy(smp->scope, smsp, (size_t)(i * sizeof(SMScope)));
        smp->total_scope = j;
        MemFree(smsp);
    }

    smp->scope[i].thr = thr;
    smp->scope[i].SEscope = sep;
    smp->num_scope++;

ret: SeqMgrUnlock();
    return curr;
}

/*****************************************************************************
*
*   SeqEntryGetScope(sep)
*       returns the current scope or NULL if none set
*
*****************************************************************************/
NLM_EXTERN SeqEntryPtr LIBCALL SeqEntryGetScope(void)
{
    SeqMgrPtr smp;
    SeqEntryPtr scope = NULL;
    Int2 i;
    SMScopePtr smsp;
    TNlmThread thr;

    smp = SeqMgrReadLock();
    if (smp == NULL) return FALSE;
    thr = NlmThreadSelf();
    for (i = 0, smsp = smp->scope; i < smp->num_scope; i++, smsp++)
    {
        if (NlmThreadCompare(thr, smsp->thr))
        {
            scope = smsp->SEscope;
            break;
        }
    }
    SeqMgrUnlock();
    return scope;
}

/*****************************************************************************
*
*   BioseqFind(SeqIdPtr)
*       Just checks in object loaded memory
*       Will also restore a Bioseq that has been cached out
*
*****************************************************************************/
NLM_EXTERN BioseqPtr LIBCALL BioseqFind (SeqIdPtr sid)
{
    return BioseqFindFunc(sid, TRUE, TRUE, TRUE);
}

/*****************************************************************************
*
*   BioseqFindCore(sid)
*       Finds a Bioseq in memory based on SeqId when only "core" elements needed
*       Will NOT restore a Bioseq that has been cached out by SeqMgr
*       This function is for use ONLY by functions that only need the parts
*         of the Bioseq left when cached out. This includes the SeqId chain,
*         and non-pointer components of the Bioseq.
*
*****************************************************************************/
NLM_EXTERN BioseqPtr LIBCALL BioseqFindCore (SeqIdPtr sip)
{
    return BioseqFindFunc(sip, FALSE, TRUE, TRUE);
}

/*****************************************************************************
*
*   BioseqFindSpecial(sid)
*       Finds a Bioseq in memory based on SeqId when only "core" elements needed
*       Will NOT restore a Bioseq that has been cached out by SeqMgr
*       This function does not use the bioseq_cache mechanism, and is for
*         the validator to check for IdOnMultipleBioseqs.
*
*****************************************************************************/
NLM_EXTERN BioseqPtr LIBCALL BioseqFindSpecial (SeqIdPtr sip)
{
    return BioseqFindFunc(sip, FALSE, TRUE, FALSE);
}

/*****************************************************************************
*
*   BioseqFindEntity(sid, itemIDptr)
*       Finds a Bioseq in memory based on SeqId
*       Will NOT restore a Bioseq that has been cached out by SeqMgr
*       returns EntityID if found, otherwise 0
*       itemIDptr is set to the value for itemID in ObjMgr functions
*       itemtype is OBJ_BIOSEQ of course
*
*****************************************************************************/
NLM_EXTERN Uint2 LIBCALL BioseqFindEntity (SeqIdPtr sip, Uint4Ptr itemIDptr)
{
    BioseqPtr bsp;
    Uint2 entityID = 0;

    *itemIDptr = 0;
    bsp = BioseqFindCore(sip);
    if (bsp == NULL) return entityID;  /* not found */
    entityID = ObjMgrGetEntityIDForPointer((Pointer)bsp);
    if (! entityID)
        return entityID;

    *itemIDptr = GatherItemIDByData(entityID, OBJ_BIOSEQ, (Pointer)bsp);
    return entityID;
}

/********************************************************************************
*
*   BioseqReload (omdp, lockit)
*     reloads the cached SeqEntry at top of omdp
*     if (lockit) locks the record
*
*********************************************************************************/

NLM_EXTERN ObjMgrDataPtr LIBCALL BioseqReload(ObjMgrDataPtr omdp, Boolean lockit)
{
    BioseqPtr bsp = NULL;
    ObjMgrDataPtr retval = NULL;
    Int4 j;
    ObjMgrPtr omp;

    if (omdp == NULL) return retval;
    if (! ((omdp->datatype == OBJ_BIOSEQ) || (omdp->datatype == OBJ_BIOSEQSET)))
        return retval;
    if (omdp->parentptr != NULL)
    {
        omp = ObjMgrReadLock();
        omdp = ObjMgrFindTop(omp, omdp);
        ObjMgrUnlock();
        if (omdp == NULL)
            return retval;
    }

    if (omdp->tempload == TL_CACHED)   /* only need to reload if cached */
    {
        bsp = BioseqReloadFunc (NULL, omdp);
        if (bsp == NULL)
            return retval;
        omp = ObjMgrReadLock();
        j = ObjMgrLookup(omp, (Pointer)bsp);
        if (j < 0) {

                    Char tmpbuff[256];

                    SeqIdWrite(bsp->id, tmpbuff, 
                               PRINTID_FASTA_LONG, sizeof(tmpbuff));

                    ErrPostEx(SEV_WARNING, 0, __LINE__, 
                              "ObjMgrLookup() returned negative value "
                              "id = %s, totobj = %d, currobj = %d, "
                              "HighestEntityID = %d", tmpbuff, omp->totobj,
                              omp->currobj, omp->HighestEntityID);
                    
                    ObjMgrUnlock();
                    return retval;
                }

        omdp = ObjMgrFindTop(omp, omp->datalist[j]);
        ObjMgrUnlock();
    }
     
    if (lockit)
    {
        ObjMgrLock(omdp->datatype, omdp->dataptr, TRUE);
    }

    return omdp;
}

static BSFetchTop NEAR SeqMgrGetFetchTop (void)
{
    SeqMgrPtr smp;
    BSFetchTop bsftp=NULL;

    smp = SeqMgrReadLock();
    if (smp == NULL) return bsftp;
    bsftp = smp->bsfetch;
    SeqMgrUnlock();
    return bsftp;
}
    
static BioseqPtr NEAR BioseqReloadFunc (SeqIdPtr sid, ObjMgrDataPtr omdp)
{
    Int4 j;
    ObjMgrDataPtr oldomdp;
    OMUserDataPtr omudp, next;
    ObjMgrProcPtr ompp;
    OMProcControl ompc;
    BioseqPtr bsp= NULL;
    Int2 ret;
    ObjMgrPtr omp;
    BSFetchTop bsftp=NULL;

    ompp = NULL;
    omp = ObjMgrReadLock();
    for (omudp = omdp->userdata; omudp != NULL; omudp = omudp->next)
    {
        if (omudp->proctype == OMPROC_FETCH)  /* caching function */
        {
            ompp = ObjMgrProcFind(omp, omudp->procid, NULL, 0);
            if (ompp != NULL)
                break;
        }
    }
    ObjMgrUnlock();

    if (ompp != NULL && ompp->outputtype != OBJ_BIOSEQ)
        return bsp;

    oldomdp = omdp;
    omdp = NULL;
    bsftp = SeqMgrGetFetchTop();
    if (bsftp != NULL)
    {
        if (ompp != NULL)    /* fetch proc left a signal */
        {                                 /* rerun fetch */
            MemSet((Pointer)(&ompc), 0, sizeof(OMProcControl));
            ompc.input_data = sid;
            ompc.input_entityID = oldomdp->EntityID;
            ompc.proc = ompp;
            ret = (* (ompp->func))((Pointer)&ompc);
            switch (ret)
            {
                case OM_MSG_RET_ERROR:
                    ErrShow();
                    break;
                case OM_MSG_RET_DEL:
                    break;
                case OM_MSG_RET_OK:
                    break;
                case OM_MSG_RET_DONE:
                    omp = ObjMgrWriteLock();
                    ObjMgrSetTempLoad (omp, ompc.output_data);
                    ObjMgrUnlock();
                    bsp = (BioseqPtr)(ompc.output_data);
                    break;
                default:
                    break;
            }
        }
        
        if (bsp == NULL)  /* nope, try regular fetch */
        {
            bsp = (*(bsftp))(sid, BSFETCH_TEMP);
        }

        if (bsp != NULL)
        {
            omp = ObjMgrReadLock();
            j = ObjMgrLookup(omp, (Pointer)bsp);
            if (j < 0) {
                            
                            Char tmpbuff[256];
                            
                            SeqIdWrite(bsp->id, tmpbuff, 
                                       PRINTID_FASTA_LONG, sizeof(tmpbuff));
                            
                            ErrPostEx(SEV_WARNING, 0, __LINE__, 
                                      "ObjMgrLookup() returned negative value "
                                      "id = %s, totobj = %d, currobj = %d, "
                                      "HighestEntityID = %d", tmpbuff, 
                                      omp->totobj,
                                      omp->currobj, omp->HighestEntityID);
                            ObjMgrUnlock();
                            return bsp;
                        }
            omdp = ObjMgrFindTop(omp, omp->datalist[j]);
            ObjMgrUnlock();
            ObjMgrDeleteIndexOnEntityID (omp, oldomdp->EntityID);
            omdp->EntityID = oldomdp->EntityID;
            oldomdp->EntityID = 0;
            ObjMgrAddIndexOnEntityID (omp, omdp->EntityID, omdp);

            omudp = omdp->userdata;
            while (omudp != NULL)
            {
                next = omudp->next;
                if (omudp->freefunc != NULL)
                                 (*(omudp->freefunc))(omudp->userdata.ptrvalue);
                MemFree(omudp);
                omudp = next;
            }
            omdp->userdata = oldomdp->userdata;
            oldomdp->userdata = NULL;

            if (oldomdp->choice != NULL)
                SeqEntryFree(oldomdp->choice);
            else
            {
                switch(oldomdp->datatype)
                {
                    case OBJ_BIOSEQ:
                        BioseqFree((BioseqPtr)(oldomdp->dataptr));
                        break;
                    case OBJ_BIOSEQSET:
                        BioseqSetFree((BioseqSetPtr)(oldomdp->dataptr));
                        break;
                    default:
                        ErrPostEx(SEV_ERROR,0,0,"BioseqReloadFunc: delete unknown type [%d]",
                            (int)(oldomdp->datatype));
                        break;
                }
            }
        }
    }
    return bsp;
}
/** static func used internally **/

/*******************************************
*
*  WARNING: if you change BIOSEQ_CACHE_NUM, you have to change the
*   number of NULL in the initialization of the 2 static pointer arrays
*   below
*
*******************************************/
/* nb: this cache is cleared in SeqMgrDeleteFromBioseqIndex() */
#define BIOSEQ_CACHE_NUM 3
static SeqEntryPtr se_cache[BIOSEQ_CACHE_NUM] = {
    NULL, NULL, NULL};   /* for a few platforms */
static ObjMgrDataPtr omdp_cache[BIOSEQ_CACHE_NUM] = {
    NULL, NULL, NULL};   /* for a few platforms */
static TNlmMutex smp_cache_mutex = NULL;

static BioseqPtr NEAR BioseqFindFunc (SeqIdPtr sid, Boolean reload_from_cache, Boolean force_it, Boolean use_bioseq_cache)
{
    Int4 i, j, num, imin, imax, retval;
    SeqIdIndexElementPtr PNTR sipp;
    CharPtr tmp;
    Char buf[80];
    Boolean do_return;
    SeqMgrPtr smp;
    ObjMgrPtr omp;
    ObjMgrDataPtr omdp = NULL;
    BioseqPtr bsp = NULL, tbsp;
    SeqEntryPtr scope;

    if (sid == NULL)
        return NULL;

    SeqMgrReadLock();    /* make sure no other thread is writing */
    retval = NlmMutexLockEx(&smp_cache_mutex);  /* protect this section */
    SeqMgrUnlock();
    if (retval)  /* error */
    {
        ErrPostEx(SEV_FATAL,0,0,"BioseqFindFunc cache mutex failed [%ld]", (long)retval);
        return NULL;
    }

    do_return = FALSE;
    scope = SeqEntryGetScope();       /* first check the cache */
    for (i = 0; i < BIOSEQ_CACHE_NUM && use_bioseq_cache; i++)
    {
        if (omdp_cache[i] == NULL)
            break;
        omdp = omdp_cache[i];
        if (omdp->datatype == OBJ_BIOSEQ)
        {
            if ((scope == NULL) || (scope == se_cache[i]))
            {
                bsp = (BioseqPtr)(omdp->dataptr);

                if (BioseqMatch(bsp, sid))
                {
                    for (j = i; j > 0; j--)  /* shift to top of cache */
                    {
                        omdp_cache[j] = omdp_cache[j-1];
                        se_cache[j] = se_cache[j-1];
                    }
                    omdp_cache[0] = omdp;
                    se_cache[0] = scope;

                    if (! reload_from_cache)
                    {
                        do_return = TRUE;
                        goto done_cache;
                    }

                    omp = ObjMgrReadLock();
                    omdp = ObjMgrFindTop(omp, omdp);
                    ObjMgrUnlock();
                    if (omdp == NULL || omdp->tempload != TL_CACHED)
                    {
                        do_return = TRUE;
                        goto done_cache;
                    }

                    bsp = BioseqReloadFunc(sid, omdp);

                    if (bsp == NULL)
                    {
                        
                        ErrPostEx(SEV_ERROR,0,0,"BioseqFindFunc: couldn't uncache");
                    }
                    do_return = TRUE;
                    goto done_cache;
                }
            }
        }
    }
done_cache:
    NlmMutexUnlock(smp_cache_mutex);
    if (do_return)  /* all done */
    {
        return bsp;
    }

    bsp = NULL; /* resetting it */

    SeqMgrProcessNonIndexedBioseq(force_it);    /* make sure all are indexed */

        /* stringify as in SeqMgrAdd */

    MakeReversedSeqIdString (sid, buf, 79); /* common function to make id, call RevStringUpper */


    imin = 0;
    smp = SeqMgrReadLock();
    imax = smp->BioseqIndexCnt - 1;
    sipp = smp->BioseqIndex;

    num = -1;

    while (imax >= imin)
    {
        i = (imax + imin)/2;
        tmp = sipp[i]->str;
        if ((j = StringCmp(tmp, buf)) > 0)
            imax = i - 1;
        else if (j < 0)
            imin = i + 1;
        else
        {
            num = i;
            break;
        }
    }

    if (num < 0)  /* couldn't find it */
    {
        /*
        Message(MSG_ERROR, "[1] Couldn't find [%s]", buf);
        */
        bsp = NULL;
        goto ret;
    }


    if (scope != NULL)    /* check in scope */
    {
        tbsp = (BioseqPtr)(sipp[num]->omdp->dataptr);
        if (ObjMgrIsChild(scope->data.ptrvalue, tbsp))
        {
            bsp = tbsp;
            omdp = sipp[num]->omdp;
        }
        else
        {                  /* not in scope, could be duplicate SeqId */
            i = num-1;
            while ((i >= 0) && (bsp == NULL) && (! StringCmp(sipp[i]->str, buf)))  /* back up */
            {
               tbsp = (BioseqPtr)(sipp[i]->omdp->dataptr);
               if (ObjMgrIsChild(scope->data.ptrvalue, tbsp))
               {
                   bsp = tbsp;
                    omdp = sipp[i]->omdp;
               }
               i--;
            }
            i = num + 1;
            imax = smp->BioseqIndexCnt - 1;
            while ((bsp == NULL) && (i <= imax) && (! StringCmp(sipp[i]->str, buf)))
            {
               tbsp = (BioseqPtr)(sipp[i]->omdp->dataptr);
               if (ObjMgrIsChild(scope->data.ptrvalue, tbsp))
               {
                   bsp = tbsp;
                    omdp = sipp[i]->omdp;
               }
               i++;
            }
        }
    }
    else  /* no scope set */
    {
        omdp = sipp[num]->omdp;
        bsp = (BioseqPtr)(omdp->dataptr);
    }


    if (bsp == NULL)   /* not found */
    {
        /*
        Message(MSG_ERROR, "[2] Couldn't find [%s]", buf);
        */
        goto ret;
    }

    retval = NlmMutexLockEx(&smp_cache_mutex);  /* protect this section */
    if (retval)  /* error */
    {
        ErrPostEx(SEV_FATAL,0,0,"BioseqFindFunc2 cache mutex failed [%ld]", (long)retval);
        bsp = NULL;
        goto ret;
    }

    for (j = (BIOSEQ_CACHE_NUM - 1); j > 0; j--)  /* shift to top of cache */
    {
        omdp_cache[j] = omdp_cache[j-1];
        se_cache[j] = se_cache[j-1];
    }
    omdp_cache[0] = omdp;
    se_cache[0] = scope;

    NlmMutexUnlock(smp_cache_mutex);

    if (! reload_from_cache)
        goto ret;

    omp = ObjMgrReadLock();
    omdp = ObjMgrFindTop(omp, omdp);
    ObjMgrUnlock();
    if (omdp == NULL)
    {
        bsp = NULL;
        goto ret;
    }
        if (omdp->tempload == TL_CACHED)
        {
                SeqMgrUnlock();
                bsp = BioseqReloadFunc(sid, omdp);
                goto ret2;
        }
ret:
        SeqMgrUnlock();
ret2:
        return bsp;
}

/*****************************************************************************
*
*   ClearBioseqFindCache()
*       frees internal omdp and se caches which can thwart detection of colliding IDs
*
*****************************************************************************/
NLM_EXTERN void ClearBioseqFindCache (void)

{
  Int4       i;
  SeqMgrPtr  smp;

  smp = SeqMgrWriteLock ();

  for (i = 0; i < BIOSEQ_CACHE_NUM; i++) {
    omdp_cache [i] = NULL;
    se_cache [i] = NULL;
  }

  SeqMgrUnlock ();
}

/*****************************************************************************
*
*   SeqMgrFreeCache()
*       frees all cached SeqEntrys
*       returns FALSE if any errors occurred
*
*****************************************************************************/
NLM_EXTERN Boolean LIBCALL SeqMgrFreeCache(void)
{
    return ObjMgrFreeCache(OBJ_SEQENTRY);
}

/*****************************************************************************
*
*   BioseqLockById(SeqIdPtr)
*       Finds the Bioseq and locks it
*       Makes sure appropriate BioseqContent is present
*
*****************************************************************************/
static BioseqPtr LIBCALL BioseqLockByIdEx (SeqIdPtr sid, Boolean force_it)
{
    BioseqPtr bsp = NULL;
    SeqMgrPtr smp;
    SeqEntryPtr oldscope = NULL;
    BSFetchTop bsftp;
    Boolean fetch_on_lock;
    DbtagPtr dbt;

    if (sid == NULL) return bsp;

    /* special case for DeltaSeqsToSeqLoc fake IDs - ignore */
    if (sid->choice == SEQID_GENERAL) {
        dbt = (DbtagPtr) sid->data.ptrvalue;
        if (dbt != NULL && StringCmp (dbt->db, "SeqLit") == 0) {
            return NULL;
        }
    }

    bsp = BioseqFindFunc(sid, TRUE, force_it, TRUE);
    if (bsp == NULL)
    {
        smp = SeqMgrReadLock();
        if (smp == NULL) return bsp;
        fetch_on_lock = smp->fetch_on_lock;
        bsftp = smp->bsfetch;
        SeqMgrUnlock();

        if (fetch_on_lock)
        {
            oldscope = SeqEntrySetScope (NULL);
            if (oldscope != NULL) {
                bsp = BioseqFindFunc(sid, TRUE, force_it, TRUE);
                SeqEntrySetScope (oldscope);
            }
            if (bsp == NULL && bsftp != NULL)
                bsp = (*(bsftp))(sid, BSFETCH_TEMP);
        }
    }

    if (bsp == NULL) return NULL;

    ObjMgrLock(OBJ_BIOSEQ, (Pointer)bsp, TRUE);
    return bsp;
}

NLM_EXTERN BioseqPtr LIBCALL BioseqLockById (SeqIdPtr sid)
{
    return BioseqLockByIdEx (sid, TRUE);
}

/*****************************************************************************
*
*   BioseqUnlockById(SeqIdPtr sip)
*       Frees a Bioseq to be dumped from memory if necessary
*
*****************************************************************************/
NLM_EXTERN Boolean LIBCALL BioseqUnlockById (SeqIdPtr sip)
{
    BioseqPtr bsp;

    if (sip == NULL) return FALSE;

    bsp = BioseqFindFunc(sip, FALSE, TRUE, TRUE);
    if (bsp == NULL)
        return FALSE;

    ObjMgrLock(OBJ_BIOSEQ, (Pointer)bsp, FALSE);
    return TRUE;
}

/*****************************************************************************
*
*   BioseqLock(BioseqPtr)
*       Locks a Bioseq
*       Any cached data is returned to memory
*
*****************************************************************************/
NLM_EXTERN BioseqPtr LIBCALL BioseqLock (BioseqPtr bsp)
{
    if (bsp == NULL) return NULL;

    ObjMgrLock(OBJ_BIOSEQ, (Pointer)bsp, TRUE);

    return bsp;
}

/*****************************************************************************
*
*   BioseqUnlock(BioseqPtr)
*       Frees a Bioseq to be dumped from memory if necessary
*
*****************************************************************************/
NLM_EXTERN Boolean LIBCALL BioseqUnlock (BioseqPtr bsp)
{
    if (bsp == NULL) return FALSE;

    if (ObjMgrLock(OBJ_BIOSEQ, (Pointer)bsp, FALSE) >= 0)
        return TRUE;
    else
        return FALSE;
}

/*****************************************************************************
*
*   BioseqFetch(SeqIdPtr, flag)
*       loads bioseq into memory if possible
*       first trys LocalLoad
*       they trys EntrezLoad
*
*****************************************************************************/
static BioseqPtr LIBCALLBACK BSFetchFunc (SeqIdPtr sid, Uint1 ld_type)
{
    BioseqPtr bsp = NULL;
    ObjMgrProcPtr ompp;
    OMProcControl ompc;
    Int2 ret;
    ObjMgrPtr omp;

    ompp = NULL;
    while ((ompp = ObjMgrProcFindNext(NULL, OMPROC_FETCH, OBJ_SEQID, OBJ_BIOSEQ, ompp)) != NULL)
    {
        MemSet((Pointer)(&ompc), 0, sizeof(OMProcControl));
        ompc.input_data = sid;
        ompc.proc = ompp;
        ret = (* (ompp->func))((Pointer)&ompc);
        switch (ret)
        {
            case OM_MSG_RET_ERROR:
                ErrShow();
                break;
            case OM_MSG_RET_DEL:
                break;
            case OM_MSG_RET_OK:
                break;
            case OM_MSG_RET_DONE:
                if (ld_type == BSFETCH_TEMP)
                {
                    omp = ObjMgrWriteLock();
                    ObjMgrSetTempLoad (omp, ompc.output_data);
                    ObjMgrUnlock();
                }
                bsp = (BioseqPtr)(ompc.output_data);
                break;
            default:
                break;
        }
        if (bsp != NULL)  /* got one */
            break;
    }

    return bsp;
}


NLM_EXTERN BioseqPtr LIBCALL BioseqFetch (SeqIdPtr sid, Uint1 ld_type)
{
    BSFetchTop bsftp;
    BioseqPtr bsp;

    bsp = BioseqFindFunc(sid, TRUE, TRUE, TRUE);
    if (bsp != NULL) return bsp;
    
    bsftp = SeqMgrGetFetchTop();
    if (bsftp == NULL) return NULL;

    return (*(bsftp))(sid, ld_type);
}

/*****************************************************************************
*
*   GetSeqIdForGI(Int4)
*     returns the SeqId for a GI
*     returns NULL if can't find it
*     The returned SeqId is allocated. Caller must free it.
*
*****************************************************************************/
typedef struct seqidblock {
  Int4       uid;
  time_t     touch;
  SeqIdPtr   sip;
  CharPtr    revstr;
} SeqIdBlock, PNTR SeqIdBlockPtr;

static ValNodePtr seqidgicache = NULL;
static ValNodePtr PNTR seqidgiarray = NULL;
static ValNodePtr PNTR giseqidarray = NULL;
static Int2 seqidcount = 0;
static Boolean seqidgiindexed = FALSE;

/* record first in small linear list so as not to sort main list after every addition */
static ValNodePtr seqidgilatest = NULL;
static Int2 seqidunidxcount = 0;

/*
static TNlmRWlock sid_RWlock = NULL;
*/

NLM_EXTERN void RecordInSeqIdGiCache (Int4 gi, SeqIdPtr sip)

{
    Char buf [80];
    ValNodePtr vnp;
    SeqIdBlockPtr sibp;
    Int4 retval;

    /* if (sip == NULL) return; okay to cache NULL because we protect against SeqIdDup */

    retval = NlmRWwrlock(sgi_RWlock);
    if (retval != 0)
    {
        ErrPostEx(SEV_ERROR,0,0,"RecSeqIdGi: RWwrlock error [%ld]",
            (long)retval);
        return;
    }

    
    vnp = ValNodeNew (NULL);
    if (vnp == NULL) goto ret;
    sibp = (SeqIdBlockPtr) MemNew (sizeof (SeqIdBlock));
    if (sibp == NULL) {
        MemFree (vnp);
        goto ret;
    }

    sibp->uid = gi;
    if (sip != NULL) {
        sibp->sip = SeqIdDup (sip);
        sibp->touch = GetSecs ();
        if (MakeReversedSeqIdString (sibp->sip, buf, sizeof (buf) - 1)) {
          sibp->revstr = StringSave (buf);
        }
    }
    vnp->data.ptrvalue = (Pointer) sibp;

    /* insert at head of unindexed list. */

    vnp->next = seqidgilatest;
    seqidgilatest = vnp;
    seqidunidxcount++;

    if (seqidunidxcount > 50 && seqidgilatest != NULL && seqidgicache != NULL) {

      /* if over threshhold, insert unindexed list at head of main list (must
         already exist so as to allow bulk lookup recording prior to use) */

      vnp = seqidgilatest;
      while (vnp->next != NULL) {
        vnp = vnp->next;
      }

      vnp->next = seqidgicache;
      seqidgicache = seqidgilatest;

      /* clear unindexed list pointer and reset count */

      seqidgilatest = NULL;
      seqidunidxcount = 0;

      /* null out sorted access arrays, will sort, unique, and index at next use */

      seqidgiarray = MemFree (seqidgiarray);
      giseqidarray = MemFree (giseqidarray);
      seqidgiindexed = FALSE;
    }

ret:
    retval = NlmRWunlock(sgi_RWlock);
    if (retval != 0)
    {
        ErrPostEx(SEV_ERROR,0,0,"RecSeqIdGiUnlock: RWunlock error [%ld]",
            (long)retval);
    }
}

NLM_EXTERN void FreeSeqIdGiCache (void)

{
  Int4           ret;
  SeqIdBlockPtr  sibp;
  ValNodePtr     vnp;

  ret = NlmRWwrlock(sgi_RWlock);
  if (ret != 0) {
    ErrPostEx(SEV_ERROR,0,0,"FreeSeqIdGiCache: RWwrlock error [%ld]", (long) ret);
    return;
  }

  seqidgiindexed = FALSE;
  seqidcount = 0;
  seqidgiarray = MemFree (seqidgiarray);
  giseqidarray = MemFree (giseqidarray);

  for (vnp = seqidgicache; vnp != NULL; vnp = vnp->next) {
    sibp = (SeqIdBlockPtr) vnp->data.ptrvalue;
    if (sibp == NULL) continue;
    sibp->sip = SeqIdFree (sibp->sip);
    sibp->revstr = MemFree (sibp->revstr);
  }
  seqidgicache = ValNodeFreeData (seqidgicache);

  /* also free unindexed list of most recent additions */

  for (vnp = seqidgilatest; vnp != NULL; vnp = vnp->next) {
    sibp = (SeqIdBlockPtr) vnp->data.ptrvalue;
    if (sibp == NULL) continue;
    sibp->sip = SeqIdFree (sibp->sip);
    sibp->revstr = MemFree (sibp->revstr);
  }
  seqidgilatest = ValNodeFreeData (seqidgilatest);
  seqidunidxcount = 0;

  ret = NlmRWunlock(sgi_RWlock);
  if (ret != 0) {
    ErrPostEx(SEV_ERROR,0,0,"FreeSeqIdGiCache: RWwrlock error [%ld]", (long) ret);
    return;
  }
}

/* trim list by sorting older nodes to end of list if list grew too large */

static int LIBCALLBACK SortSeqIdGiCacheTime (VoidPtr ptr1, VoidPtr ptr2)

{
  SeqIdBlockPtr  sibp1;
  SeqIdBlockPtr  sibp2;
  ValNodePtr     vnp1;
  ValNodePtr     vnp2;

  if (ptr1 == NULL || ptr2 == NULL) return 0;
  vnp1 = *((ValNodePtr PNTR) ptr1);
  vnp2 = *((ValNodePtr PNTR) ptr2);
  if (vnp1 == NULL || vnp2 == NULL) return 0;
  sibp1 = (SeqIdBlockPtr) vnp1->data.ptrvalue;
  sibp2 = (SeqIdBlockPtr) vnp2->data.ptrvalue;
  if (sibp1 == NULL || sibp2 == NULL) return 0;
  if (sibp1->touch > sibp2->touch) {
     return -1;
  } else if (sibp1->touch < sibp2->touch) {
    return 1;
  }
  return 0;
}

/* sort valnode list by gi */

static int LIBCALLBACK SortSeqIdGiByUid (VoidPtr ptr1, VoidPtr ptr2)

{
  SeqIdBlockPtr  sibp1;
  SeqIdBlockPtr  sibp2;
  ValNodePtr     vnp1;
  ValNodePtr     vnp2;

  if (ptr1 == NULL || ptr2 == NULL) return 0;
  vnp1 = *((ValNodePtr PNTR) ptr1);
  vnp2 = *((ValNodePtr PNTR) ptr2);
  if (vnp1 == NULL || vnp2 == NULL) return 0;
  sibp1 = (SeqIdBlockPtr) vnp1->data.ptrvalue;
  sibp2 = (SeqIdBlockPtr) vnp2->data.ptrvalue;
  if (sibp1 == NULL || sibp2 == NULL) return 0;
  if (sibp1->uid < sibp2->uid) {
     return -1;
  } else if (sibp1->uid > sibp2->uid) {
    return 1;
  }
  return 0;
}

static ValNodePtr UniqueSeqIdGiByUid (ValNodePtr list)

{
  SeqIdBlockPtr  curr, last;
  ValNodePtr     next;
  Pointer PNTR   prev;
  ValNodePtr     vnp;

  if (list == NULL) return NULL;
  last = (SeqIdBlockPtr) list->data.ptrvalue;
  vnp = list->next;
  prev = (Pointer PNTR) &(list->next);
  while (vnp != NULL) {
    next = vnp->next;
    curr = (SeqIdBlockPtr) vnp->data.ptrvalue;
    if (last != NULL && curr != NULL && last->uid == curr->uid) {
      vnp->next = NULL;
      *prev = next;
      ValNodeFreeData (vnp);
    } else {
      last = (SeqIdBlockPtr) vnp->data.ptrvalue;
      prev = (Pointer PNTR) &(vnp->next);
    }
    vnp = next;
  }

  return list;
}

/* sort valnode array by reversed seqid string */

static int LIBCALLBACK SortSeqIdGiByString (VoidPtr ptr1, VoidPtr ptr2)

{
  SeqIdBlockPtr  sibp1;
  SeqIdBlockPtr  sibp2;
  CharPtr        str1;
  CharPtr        str2;
  ValNodePtr     vnp1;
  ValNodePtr     vnp2;

  if (ptr1 == NULL || ptr2 == NULL) return 0;
  vnp1 = *((ValNodePtr PNTR) ptr1);
  vnp2 = *((ValNodePtr PNTR) ptr2);
  if (vnp1 == NULL || vnp2 == NULL) return 0;
  sibp1 = (SeqIdBlockPtr) vnp1->data.ptrvalue;
  sibp2 = (SeqIdBlockPtr) vnp2->data.ptrvalue;
  if (sibp1 == NULL || sibp2 == NULL) return 0;
  str1 = sibp1->revstr;
  str2 = sibp2->revstr;
  if (str1 == NULL || str2 == NULL) return 0;
  return StringICmp (str1, str2);
}

static Boolean UpdateSeqIdGiArrays (void)

{
  Int2           i;
  Int4           ret;
  SeqIdBlockPtr  sibp;
  ValNodePtr     tmp, vnp;

  if (seqidgicache == NULL && seqidgilatest == NULL) return FALSE;

  if (! seqidgiindexed) {
    ret = NlmRWwrlock (sgi_RWlock);
    if (ret != 0) {
      ErrPostEx (SEV_ERROR, 0, 0, "SeqIdGi: RWwrlock error [%ld]", (long) ret);
      return FALSE;
    }

    if (seqidunidxcount > 50 && seqidgilatest != NULL) {

      /* if over threshhold, insert unindexed list at head of main list */

      vnp = seqidgilatest;
      while (vnp->next != NULL) {
        vnp = vnp->next;
      }

      vnp->next = seqidgicache;
      seqidgicache = seqidgilatest;

      /* clear unindexed list pointer and reset count */

      seqidgilatest = NULL;
      seqidunidxcount = 0;

      /* null out sorted access arrays, will sort, unique, and index at next use */

      seqidgiarray = MemFree (seqidgiarray);
      giseqidarray = MemFree (giseqidarray);
      seqidgiindexed = FALSE;
    }

    if (! seqidgiindexed) {

      /* if list is too large, sort by touch time, cut least recently used ids */

      seqidcount = (Int2) ValNodeLen (seqidgicache);
      if (seqidcount > 32000) {

        seqidgicache = ValNodeSort (seqidgicache, SortSeqIdGiCacheTime);
        for (vnp = seqidgicache; vnp != NULL && seqidcount > 24000; vnp = vnp->next) {
          seqidcount--;
        }
        if (vnp != NULL) {
          for (tmp = vnp->next; tmp != NULL; tmp = tmp->next) {
            sibp = (SeqIdBlockPtr) tmp->data.ptrvalue;
            if (sibp == NULL) continue;
            sibp->sip = SeqIdFree (sibp->sip);
            sibp->revstr = MemFree (sibp->revstr);
          }
          vnp->next = ValNodeFreeData (vnp->next);
        }
      }

      /* sort list by gi */

      seqidgicache = ValNodeSort (seqidgicache, SortSeqIdGiByUid);
      seqidgicache = UniqueSeqIdGiByUid (seqidgicache);
      seqidcount = (Int2) ValNodeLen (seqidgicache);

      /* copy sorted list into both arrays */

      if (seqidcount > 0) {
        seqidgiarray = MemNew (sizeof (ValNodePtr) * (size_t) (seqidcount + 1));
        giseqidarray = MemNew (sizeof (ValNodePtr) * (size_t) (seqidcount + 1));
        if (seqidgiarray != NULL && giseqidarray != NULL) {
          for (vnp = seqidgicache, i = 0; vnp != NULL; vnp = vnp->next, i++) {
            seqidgiarray [i] = vnp;
            giseqidarray [i] = vnp;
          }

          /* now resort one array by seqid string */

          StableMergeSort (giseqidarray, (size_t) seqidcount, sizeof (ValNodePtr), SortSeqIdGiByString);
        }
      }

      /* finally, set indexed flag */

      seqidgiindexed = TRUE;
    }

    ret = NlmRWunlock (sgi_RWlock);
    if (ret != 0) {
      ErrPostEx (SEV_ERROR, 0, 0, "SeqIdGi: RWunlock error [%ld]", (long) ret);
      return FALSE;
    }
  }

  return TRUE;
}

NLM_EXTERN Boolean FetchFromSeqIdGiCache (Int4 gi, SeqIdPtr PNTR sipp)

{
    ValNodePtr vnp;
    SeqIdBlockPtr sibp = NULL;
    Int2 left, right, mid;
    Int4 compare, ret;
    Boolean done = FALSE;
    

    if (sipp != NULL) {
      *sipp = NULL;
    }
    if (seqidgicache == NULL && seqidgilatest == NULL) return done;

    if (! UpdateSeqIdGiArrays ()) {
        return done;
    }

    ret = NlmRWrdlock(sgi_RWlock);
    if (ret != 0)
    {
        ErrPostEx(SEV_ERROR,0,0,"SeqIdGi: RWrdlock error [%ld]",
            (long)ret);
        return done;
    }

    if (seqidgiarray != NULL) {
        left = 1;
        right = seqidcount;
        while (left <= right) {
            mid = (left + right) / 2;
            compare = 0;
            vnp = seqidgiarray [mid - 1];
            if (vnp != NULL) {
                sibp = (SeqIdBlockPtr) vnp->data.ptrvalue;
                if (sibp != NULL) {
                    compare = gi - sibp->uid;
                }
            }
            if (compare <= 0) {
                right = mid - 1;
            }
            if (compare >= 0) {
                left = mid + 1;
            }
        }
        if (left > right + 1 && sibp != NULL) {
            if (sibp->sip != NULL) {
                if (sipp != NULL) {
                    *sipp = SeqIdDup (sibp->sip);
                }
                sibp->touch = GetSecs ();
            }
            done = TRUE;
        }
    }

    if (! done) {
      for (vnp = seqidgilatest; vnp != NULL; vnp = vnp->next) {
        sibp = (SeqIdBlockPtr) vnp->data.ptrvalue;
        if (sibp == NULL) continue;
        if (sibp->uid == gi) {
          if (sibp->sip != NULL) {
            if (sipp != NULL) {
              *sipp = SeqIdDup (sibp->sip);
            }
            sibp->touch = GetSecs ();
            done = TRUE;
            break;
          }
        }
      }
    }

    ret = NlmRWunlock(sgi_RWlock);
    if (ret != 0)
    {
        ErrPostEx(SEV_ERROR,0,0,"SeqIdGi: RWunlock error [%ld]",
            (long)ret);
    }

    return done;
}

NLM_EXTERN SeqIdPtr LIBCALL GetSeqIdForGI (Int4 gi)
{
    BioseqPtr bsp = NULL;
    ObjMgrProcPtr ompp;
    OMProcControl ompc;
    Int2 ret;
    SeqIdPtr sip, sip2=NULL, otherh=NULL, otherl = NULL, otherp = NULL, gb=NULL;
    ValNode vn;
    SeqEntryPtr oldscope = NULL;


    if (gi <= 0)
        return sip2;

    vn.choice = SEQID_GI;
    vn.data.intvalue = gi;
    vn.next = NULL;

    oldscope = SeqEntrySetScope (NULL);
    bsp = BioseqFindCore(&vn);
    SeqEntrySetScope (oldscope);

    if (bsp != NULL)
    {
        for (sip = bsp->id; sip != NULL; sip = sip->next)
        {
            switch (sip->choice) 
            {
                case SEQID_LOCAL:           /* object id */
                case SEQID_GIBBSQ:         
                case SEQID_GIBBMT:
                case SEQID_PATENT:
                case SEQID_GENERAL:
                    otherl = sip;
                    break;
                case SEQID_GI:
                    break;
                case SEQID_GENBANK:
                case SEQID_EMBL:
                case SEQID_PIR:
                case SEQID_SWISSPROT:
                case SEQID_DDBJ:
                case SEQID_PRF:
                case SEQID_PDB:
                case SEQID_OTHER:
                case SEQID_TPG:
                case SEQID_TPE:
                case SEQID_TPD:
                    gb = sip;
                    break;
                case SEQID_GPIPE:
                    otherp = sip;
                    break;
                default:
                    if (otherh == NULL)
                        otherh = sip;
                    break;
            }
        }
    }


    if (gb != NULL)
        sip2 = gb;
    else if (otherp != NULL)
        sip2 = otherp;
    else if (otherh != NULL)
        sip2 = otherh;
    else if (otherl != NULL)
        sip2 = otherl;

    if (sip2 != NULL)
        return SeqIdDup(sip2);

    if (FetchFromSeqIdGiCache (gi, &sip2)) {
        return sip2;
    }

    ompp = NULL;
    while ((ompp = ObjMgrProcFindNext(NULL, OMPROC_FETCH, OBJ_SEQID, OBJ_SEQID, ompp)) != NULL)
    {
        if ((ompp->subinputtype == SEQID_GI) && (ompp->suboutputtype == 0))
        {
            MemSet((Pointer)(&ompc), 0, sizeof(OMProcControl));
            ompc.input_data = &vn;
            ompc.proc = ompp;
            ret = (* (ompp->func))((Pointer)&ompc);
            switch (ret)
            {
                case OM_MSG_RET_ERROR:
                    ErrShow();
                    break;
                case OM_MSG_RET_DEL:
                    break;
                case OM_MSG_RET_OK:
                    break;
                case OM_MSG_RET_DONE:
                    sip2 = (SeqIdPtr)(ompc.output_data);
                    if (sip2 != NULL) {
                         RecordInSeqIdGiCache (gi, sip2);
                        return sip2;
                    }
                    break;
                default:
                    break;
            }
        }
    }

    RecordInSeqIdGiCache (gi, sip2);
    return sip2;
}

/*****************************************************************************
*
*   GetGIForSeqId(SeqIdPtr)
*     returns the GI for a SeqId
*     returns 0 if can't find it
*
*****************************************************************************/
NLM_EXTERN Boolean FetchFromGiSeqIdCache (SeqIdPtr sip, Int4Ptr gip)

{
    Char buf [80];
    ValNodePtr vnp;
    SeqIdBlockPtr sibp = NULL;
    Int2 left, right, mid;
    Int4 compare, ret;
    Boolean done = FALSE;
    

    if (gip != NULL) {
      *gip = 0;
    }
    if (seqidgicache == NULL && seqidgilatest == NULL) return done;
    if (sip == NULL) return done;
    if (! MakeReversedSeqIdString (sip, buf, sizeof (buf) - 1)) return done;

    if (! UpdateSeqIdGiArrays ()) {
        return done;
    }

    ret = NlmRWrdlock(sgi_RWlock);
    if (ret != 0)
    {
        ErrPostEx(SEV_ERROR,0,0,"SeqIdGi: RWrdlock error [%ld]",
            (long)ret);
        return done;
    }

    if (giseqidarray != NULL) {
        left = 1;
        right = seqidcount;
        while (left <= right) {
            mid = (left + right) / 2;
            compare = 0;
            vnp = giseqidarray [mid - 1];
            if (vnp != NULL) {
                sibp = (SeqIdBlockPtr) vnp->data.ptrvalue;
                if (sibp != NULL) {
                    compare = StringCmp (buf, sibp->revstr);
                }
            }
            if (compare <= 0) {
                right = mid - 1;
            }
            if (compare >= 0) {
                left = mid + 1;
            }
        }
        if (left > right + 1 && sibp != NULL) {
            if (gip != NULL) {
                *gip = sibp->uid;
            }
            sibp->touch = GetSecs ();
            done = TRUE;
        }
    }

    if (! done) {
      for (vnp = seqidgilatest; vnp != NULL; vnp = vnp->next) {
        sibp = (SeqIdBlockPtr) vnp->data.ptrvalue;
        if (sibp == NULL) continue;
        if (StringCmp (buf, sibp->revstr) == 0) {
          if (gip != NULL) {
            *gip = sibp->uid;
          }
          sibp->touch = GetSecs ();
          done = TRUE;
          break;
        }
      }
    }

    ret = NlmRWunlock(sgi_RWlock);
    if (ret != 0)
    {
        ErrPostEx(SEV_ERROR,0,0,"SeqIdGi: RWunlock error [%ld]",
            (long)ret);
    }

    return done;
}

NLM_EXTERN Int4 LIBCALL GetGIForSeqId (SeqIdPtr sid)
{
    BioseqPtr bsp = NULL;
    ObjMgrProcPtr ompp;
    OMProcControl ompc;
    Int2 ret;
    SeqIdPtr sip;
    Int4 gi = 0;
    SeqEntryPtr oldscope = NULL;


    if (sid == NULL)
        return gi;

    if (sid->choice == SEQID_GI)
        return sid->data.intvalue;

    oldscope = SeqEntrySetScope (NULL);
    bsp = BioseqFindCore(sid);
    SeqEntrySetScope (oldscope);

    if (bsp != NULL)
    {
        for (sip = bsp->id; sip != NULL; sip = sip->next)
        {
            if (sip->choice == SEQID_GI)
                return sip->data.intvalue;
        }
    }

    if (FetchFromGiSeqIdCache (sid, &gi)) {
        return gi;
    }

    ompp = NULL;
    while ((ompp = ObjMgrProcFindNext(NULL, OMPROC_FETCH, OBJ_SEQID, OBJ_SEQID, ompp)) != NULL)
    {
        if ((ompp->subinputtype == 0) && (ompp->suboutputtype == SEQID_GI))
        {
            MemSet((Pointer)(&ompc), 0, sizeof(OMProcControl));
            ompc.input_data = sid;
            ompc.proc = ompp;
            ret = (* (ompp->func))((Pointer)&ompc);
            switch (ret)
            {
                case OM_MSG_RET_ERROR:
                    ErrShow();
                    break;
                case OM_MSG_RET_DEL:
                    break;
                case OM_MSG_RET_OK:
                    break;
                case OM_MSG_RET_DONE:
                    sip = (SeqIdPtr)(ompc.output_data);
                    if (sip != NULL)
                    {
                        if (sip->choice == SEQID_GI)
                        {
                            gi = sip->data.intvalue;
                            SeqIdFree(sip);
                            RecordInSeqIdGiCache (gi, sid);
                            return gi;
                        }
                        SeqIdFree(sip);
                    }
                    break;
                default:
                    break;
            }
        }
    }

    return gi;
}


/*****************************************************************************
*
*   SeqEntryFind(sip)
*       returns top level seqentry for sip
*
*****************************************************************************/
NLM_EXTERN SeqEntryPtr LIBCALL SeqEntryFind (SeqIdPtr sid)
{
    BioseqPtr bsp;
    ObjMgrDataPtr omdp;
    ObjMgrDataPtr PNTR omdpp;
    SeqEntryPtr result=NULL;
    SeqSubmitPtr ssp;
    Int4 i;
    ObjMgrPtr omp;

    bsp = BioseqFind(sid);
    if (bsp == NULL) return result;

    omp = ObjMgrReadLock();
    omdpp = omp->datalist;

    i = ObjMgrLookup(omp, (Pointer)bsp);
    if (i < 0) {
            Char tmpbuff[256];
            
            SeqIdWrite(bsp->id, tmpbuff, 
                       PRINTID_FASTA_LONG, sizeof(tmpbuff));
            
            ErrPostEx(SEV_WARNING, 0, __LINE__, 
                      "ObjMgrLookup() returned negative value "
                      "id = %s, totobj = %d, currobj = %d, "
                      "HighestEntityID = %d", tmpbuff, 
                      omp->totobj,
                      omp->currobj, omp->HighestEntityID);
            ObjMgrUnlock();
            return result;
        }

    omdp = omdpp[i];
    while (omdp->parentptr != NULL)
    {
        i = ObjMgrLookup(omp, (omdp->parentptr));
        if (i < 0) {
                    Char tmpbuff[256];
                    
                    SeqIdWrite(bsp->id, tmpbuff, 
                               PRINTID_FASTA_LONG, sizeof(tmpbuff));
                    
                    ErrPostEx(SEV_WARNING, 0, __LINE__, 
                              "ObjMgrLookup() returned negative value "
                              "id = %s, totobj = %d, currobj = %d, "
                              "HighestEntityID = %d", tmpbuff, 
                              omp->totobj,
                              omp->currobj, omp->HighestEntityID);
                    ObjMgrUnlock();
                    return result;
                }
        omdp = omdpp[i];
    }

    if (omdp->datatype == OBJ_SEQSUB) {
        ssp = (SeqSubmitPtr) omdp->dataptr;
        if (ssp != NULL && ssp->datatype == 1) {
            result = (SeqEntryPtr) ssp->data;
        }
    } else {
        result = omdp->choice;
    }
    ObjMgrUnlock();
    return result;
}

/*****************************************************************************
*
*   BioseqContextPtr BioseqContextNew (bsp)
*
*****************************************************************************/
NLM_EXTERN BioseqContextPtr LIBCALL BioseqContextNew (BioseqPtr bsp)
{
    ObjMgrDataPtr omdp;
    ObjMgrDataPtr PNTR omdpp;
    Int4    i;
    Int2    ctr=0;
    SeqEntryPtr seps[BIOSEQCONTEXTMAX];
    BioseqContextPtr bcp;
    ObjMgrPtr omp;

    if (bsp == NULL)
        return NULL;


    bcp = MemNew(sizeof(BioseqContext));
    bcp->bsp = bsp;
    bcp->se.choice = 1;   /* bioseq */
    bcp->se.data.ptrvalue = bsp;

    omp = ObjMgrReadLock();
    if (omp == NULL) return BioseqContextFree(bcp);
    omdpp = omp->datalist;

    i = ObjMgrLookup(omp, (Pointer)bsp);
    if (i < 0) {
            Char tmpbuff[256];
            
            SeqIdWrite(bsp->id, tmpbuff, 
                       PRINTID_FASTA_LONG, sizeof(tmpbuff));
            
            ErrPostEx(SEV_WARNING, 0, __LINE__, 
                      "ObjMgrLookup() returned negative value "
                      "id = %s, totobj = %d, currobj = %d, "
                      "HighestEntityID = %d", tmpbuff, 
                      omp->totobj,
                      omp->currobj, omp->HighestEntityID);
            ObjMgrUnlock();
            return NULL;
        }
    omdp = omdpp[i];

    if (omdp->choice != NULL)
    {
        seps[ctr] = omdp->choice;
        ctr++;

        while (omdp->parentptr != NULL)
        {
            i = ObjMgrLookup(omp, (omdp->parentptr));
            if (i < 0) {
                            Char tmpbuff[256];
                            
                            SeqIdWrite(bsp->id, tmpbuff, 
                                       PRINTID_FASTA_LONG, sizeof(tmpbuff));
                            
                            ErrPostEx(SEV_WARNING, 0, __LINE__, 
                                      "ObjMgrLookup() returned negative value "
                                      "id = %s, totobj = %d, currobj = %d, "
                                      "HighestEntityID = %d", tmpbuff, 
                                      omp->totobj,
                                      omp->currobj, omp->HighestEntityID);
                            ObjMgrUnlock();
                            return NULL;
                        }
            omdp = omdpp[i];
            if (omdp->choice != NULL)
            {
                if (ctr == BIOSEQCONTEXTMAX)
                    ErrPostEx(SEV_ERROR, 0,0, "BioseqContextNew: more than %d levels",(int)ctr);
                else
                {
                    seps[ctr] = omdp->choice;
                    ctr++;
                }
            }
        }

        bcp->count = ctr;
        for (i = 0; i < bcp->count; i++)
        {
            ctr--;
            bcp->context[i] = seps[ctr];
        }
    }

    if (omdp->tempload == TL_CACHED)
    {
        ErrPostEx(SEV_ERROR,0,0,"BioseqContextNew: bsp is TL_CACHED");
        bcp = BioseqContextFree(bcp);
    }

    ObjMgrUnlock();

    return bcp;
}

/*****************************************************************************
*
*   BioseqContextFree(bcp)
*
*****************************************************************************/
NLM_EXTERN BioseqContextPtr LIBCALL BioseqContextFree(BioseqContextPtr bcp)
{
    return MemFree(bcp);
}

/*****************************************************************************
*
*   BioseqContextGetSeqDescr(bcp, type, curr, SeqEntryPtr PNTR sep)
*       returns pointer to the next SeqDescr of this type
*       type gives type of Seq-descr
*       if (type == 0)
*          get them all
*       curr is NULL or previous node of this type found
*       moves up from bsp
*        if (sep != NULL) sep set to SeqEntryPtr containing the SeqDescr.
*
*****************************************************************************/
NLM_EXTERN ValNodePtr LIBCALL BioseqContextGetSeqDescr (BioseqContextPtr bcp, Int2 type, ValNodePtr curr, SeqEntryPtr PNTR the_sep)    /* the last one you used */
{
    Int2 i;
    ValNodePtr tmp = NULL;
    Boolean found = FALSE;
    BioseqPtr bsp;
    BioseqSetPtr bssp;

    if (bcp == NULL) return NULL;
    
    if (the_sep != NULL)
        *the_sep = NULL;
        
    if (bcp->count == 0)   /* just a Bioseq */
    {
        tmp = BioseqGetSeqDescr(bcp->bsp, type, curr);
        if (the_sep != NULL) *the_sep = bcp->context[1];
        return tmp;
    }

    i = bcp->count - 1;
    if (curr != NULL)   /* find where we are */
    {
        while ((i >= 0) && (! found))
        {
            if (IS_Bioseq(bcp->context[i]))
            {
                bsp = (BioseqPtr)((bcp->context[i])->data.ptrvalue);
                tmp = bsp->descr;
            }
            else
            {
                bssp = (BioseqSetPtr)((bcp->context[i])->data.ptrvalue);
                tmp = bssp->descr;
            }
            while ((tmp != curr) && (tmp != NULL))
                tmp = tmp->next;
            if (tmp == curr)
            {
                found = TRUE;
                tmp = tmp->next;
            }
            else
                i--;
        }
        if (! found)   /* can't find it! */
            return NULL;
    }
    else              /* get first one */
    {
        tmp = bcp->bsp->descr;
    }
        
    while (i >= 0)
    {
        while (tmp != NULL)
        {
            if ((! type) || ((Int2)(tmp->choice) == type))
            {
                if (the_sep != NULL) *the_sep = bcp->context[i];
                return tmp;
            }
            tmp = tmp->next;
        }
        i--;
        if (i >= 0)
        {
            if (IS_Bioseq(bcp->context[i]))
            {
                bsp = (BioseqPtr)((bcp->context[i])->data.ptrvalue);
                tmp = bsp->descr;
            }
            else
            {
                bssp = (BioseqSetPtr)((bcp->context[i])->data.ptrvalue);
                tmp = bssp->descr;
            }
        }
    }
    return NULL;
}

/*****************************************************************************
*
*   BioseqContextGetSeqFeat(bcp, type, curr, sapp)
*       returns pointer to the next Seq-feat of this type
*       type gives type of Seq-descr
*       if (type == 0)
*          get them all
*       curr is NULL or previous node of this type found
*       moves up from bsp
*       if (sapp != NULL) is filled with SeqAnnotPtr containing the SeqFeat
*       in:
*           0 = sfp->location only
*           1 = sfp->product only
*           2 = either of above
*
*****************************************************************************/
NLM_EXTERN SeqFeatPtr LIBCALL BioseqContextGetSeqFeat (BioseqContextPtr bcp, Int2 type,
    SeqFeatPtr curr, SeqAnnotPtr PNTR sapp, Int2 in)    /* the last one you used */
{
    SeqEntryPtr sep;

    if (bcp == NULL) return NULL;
    
    if (sapp != NULL)
        *sapp = NULL;

    if (bcp->count == 0)    /* just a BioseqSeq */
        sep = &(bcp->se);
    else
        sep = bcp->context[0];

    return SeqEntryGetSeqFeat (sep, type, curr, sapp, in, bcp->bsp);
}

typedef struct smgetseqfeat {
    Boolean hit;
    SeqFeatPtr last,
        this;
    SeqAnnotPtr sap;
    SeqLocPtr slp1, slp2;
    Int2 in, type;
} SMGetSeqFeat, PNTR GetSeqFeatPtr;

NLM_EXTERN void GetSeqFeatCallback (SeqEntryPtr sep, Pointer data, Int4 index, Int2 indent);

/*****************************************************************************
*
*   SeqEntryGetSeqFeat(sep, type, curr, sapp)
*       returns pointer to the next Seq-feat of this type
*       type gives type of SeqFeat
*       if (type == 0)
*          get them all
*       curr is NULL or previous node of this type found
*       moves up from bsp
*       if (sapp != NULL) is filled with SeqAnnotPtr containing the SeqFeat
*       if (bsp != NULL) then for that Bioseq match on location by "in"
*       in:
*           0 = sfp->location only
*           1 = sfp->product only
*           2 = either of above
*
*****************************************************************************/
NLM_EXTERN SeqFeatPtr LIBCALL SeqEntryGetSeqFeat (SeqEntryPtr sep, Int2 type,
    SeqFeatPtr curr, SeqAnnotPtr PNTR sapp, Int2 in, BioseqPtr bsp)    /* the last one you used */
{
    SMGetSeqFeat gsf;
    ValNode vn1, vn2;
    
    if (sep == NULL) return NULL;
    
    if (sapp != NULL)
        *sapp = NULL;

    if (curr == NULL)
        gsf.hit = TRUE;
    else
        gsf.hit = FALSE;
    gsf.last = curr;
    gsf.this = NULL;
    gsf.sap = NULL;
    gsf.type = type;
    gsf.in = in;
    if (bsp != NULL)   /* matching by Bioseq */
    {
        if ((bsp->repr == Seq_repr_seg) || (bsp->repr == Seq_repr_ref))
        {
            vn2.choice = SEQLOC_MIX;
            vn2.data.ptrvalue = bsp->seq_ext;
            gsf.slp2 = (SeqLocPtr)(&vn2);
        }
        else
            gsf.slp2 = NULL;

        vn1.choice = SEQLOC_WHOLE;
        vn1.data.ptrvalue = (Pointer) SeqIdFindBest (bsp->id, 0);
        gsf.slp1 = (SeqLocPtr)(&vn1);
    }
    else
        gsf.slp1 = NULL;

    SeqEntryExplore (sep, (Pointer)(&gsf), GetSeqFeatCallback);

    if (sapp != NULL)
        *sapp = gsf.sap;

    return gsf.this;
}

NLM_EXTERN void GetSeqFeatCallback (SeqEntryPtr sep, Pointer data, Int4 index, Int2 indent)
{
    GetSeqFeatPtr gsfp;
    BioseqPtr bsp;
    BioseqSetPtr bssp;
    SeqAnnotPtr sap;
    SeqFeatPtr sfp, last;
    Boolean hit, gotit = FALSE;
    Uint1 type;
    SeqLocPtr slp1, slp2 = NULL, slp;
    Int2 i, in = 0, retval;

    gsfp = (GetSeqFeatPtr)data;
    if (gsfp->this != NULL)   /* got it */
        return;

    last = gsfp->last;
    hit = gsfp->hit;
    type = (Uint1)(gsfp->type);
    if (gsfp->slp1 != NULL)   /* matching by Bioseq */
    {
        slp1 = gsfp->slp1;
        slp2 = gsfp->slp2;
        in = gsfp->in;
    }
    else
        slp1 = NULL;

    if (IS_Bioseq(sep))
    {
        bsp = (BioseqPtr)(sep->data.ptrvalue);
        sap = bsp->annot;
    }
    else
    {
        bssp = (BioseqSetPtr)(sep->data.ptrvalue);
        sap = bssp->annot;
    }

    while (sap != NULL)
    {
        if (sap->type == 1)  /* feature table */
        {
            for (sfp = (SeqFeatPtr)(sap->data); sfp != NULL; sfp = sfp->next)
            {
                if (! hit)       /* still looking */
                {
                    if (sfp == last)
                    {
                        hit = TRUE;
                        gsfp->hit = TRUE;
                    }
                }
                else
                {
                    if ((! type) || (type == sfp->data.choice))
                    {
                        if (slp1 != NULL)   /* look for feats on a bioseq */
                        {
                            for (i = 0; i < 2; i++)
                            {
                                if ((i == 0) && (in != 1))
                                    slp = sfp->location;
                                else if ((i==1) && (in != 0))
                                    slp = sfp->product;
                                else
                                    slp = NULL;
                                if (slp != NULL)
                                {
                                    retval = SeqLocCompare(slp, slp1);
                                    if (retval)
                                    {
                                        gotit = TRUE;
                                        break;
                                    }

                                    if (slp2 != NULL)
                                    {
                                        retval = SeqLocCompare(slp, slp2);
                                        if (retval)
                                        {
                                            gotit = TRUE;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                        else
                            gotit = TRUE;
                        if (gotit)
                        {
                            gsfp->this = sfp;
                            gsfp->sap = sap;
                            return;
                        }
                    }
                }
            }
        }

        sap = sap->next;
    }

    return;
}

/*****************************************************************************
*
*   BioseqContextGetTitle(bcp)
*     returns first title for Bioseq in this context
*
*****************************************************************************/
NLM_EXTERN CharPtr LIBCALL BioseqContextGetTitle(BioseqContextPtr bcp)
{
    CharPtr title = NULL;
    ValNodePtr vnp;

    vnp = BioseqContextGetSeqDescr(bcp, Seq_descr_title, NULL, NULL);
    if (vnp != NULL)
        title = (CharPtr)vnp->data.ptrvalue;
    return title;
}

/*****************************************************************************
*
*   SeqMgr Functions
*
*****************************************************************************/

/*****************************************************************************
*
*   SeqMgrSeqEntry(type, data, sep)
*       Adds the SeqEntryPtr pointing directly to this Bioseq or BioseqSet
*
*****************************************************************************/
NLM_EXTERN Boolean LIBCALL SeqMgrSeqEntry (Uint1 type, Pointer data, SeqEntryPtr sep)
{
    return ObjMgrSetChoice (OBJ_SEQENTRY, sep, data);
}

/*****************************************************************************
*
*   SeqMgrGetSeqEntryForData(data)
*       returns SeqEntryPtr for a BioseqPtr or BioseqSetPtr
*       sep must have been put in SeqMgr using SeqMgrSeqEntry
*       the Bioseq/BioseqSets it is a part of must also be in SeqMgr
*       returns NULL on failure.
*
*****************************************************************************/
NLM_EXTERN SeqEntryPtr LIBCALL SeqMgrGetSeqEntryForData (Pointer data)
{
    return ObjMgrGetChoiceForData(data);
}

/*****************************************************************************
*
*   SeqMgrGetEntityIDForSeqEntry(sep)
*       returns the EntityID for a SeqEntryPtr
*       sep must have been put in SeqMgr using SeqMgrSeqEntry
*       the Bioseq/BioseqSets it is a part of must also be in SeqMgr
*       This function will move up to the top of the SeqEntry tree it may be
*          in. If top level EntityID is 0, one is assigned at this point.
*       If an element is moved under a different hierarchy, its EntityID will
*          change.
*       returns 0 on failure.
*
*****************************************************************************/
NLM_EXTERN Int2 LIBCALL SeqMgrGetEntityIDForSeqEntry (SeqEntryPtr sep)
{
    return ObjMgrGetEntityIDForChoice (sep);
}

/*****************************************************************************
*
*   SeqMgrGetSeqEntryForEntityID (id)
*
*****************************************************************************/
NLM_EXTERN SeqEntryPtr LIBCALL SeqMgrGetSeqEntryForEntityID (Int2 id)
{
    return ObjMgrGetChoiceForEntityID (id);
}

/*****************************************************************************
*
*   SeqMgrSetBSFetchTop (fetch, data)
*       sets the BSFetchTop routine to "fetch"
*       returns previous value
*       set to NULL to turn off all fetching for that type
*
*       Current value can be called directly as BioseqFetch();
*       Default is
*           1) looks in memory
*           2) looks locally if LocalBSFetch is set
*            3) looks remotely if RemoteBSFetch is set
*
*****************************************************************************/
NLM_EXTERN BSFetchTop LIBCALL SeqMgrSetBSFetchTop (BSFetchTop fetch, Pointer data)
{
    SeqMgrPtr smp;
    BSFetchTop tmp = NULL;

    smp = SeqMgrWriteLock();
    if (smp == NULL) return tmp;
    
    tmp = smp->bsfetch;
    smp->bsfetch = fetch;
    smp->TopData = data;
    SeqMgrUnlock();
    return tmp;
}

/*****************************************************************************
*
*   SeqMgrSetFetchOnLock(value)
*       if value = TRUE, manager will try to fetch the bioseq if not in
*          memory, when BioseqLock is called
*       if FALSE, BioseqLock will only look in memory
*       returns previous value of fetch_on_lock
*       default is to fetch_on_lock
*
*****************************************************************************/
NLM_EXTERN Boolean LIBCALL SeqMgrSetFetchOnLock (Boolean value)
{
    Boolean tmp=FALSE;
    SeqMgrPtr smp;

    smp = SeqMgrWriteLock();
    if (smp == NULL) return tmp;

    tmp = smp->fetch_on_lock;
    smp->fetch_on_lock = value;
    SeqMgrUnlock();
    return tmp;
}

/*****************************************************************************
*
*   SeqMgrLinkSeqEntry(sep, parenttype, parentptr)
*      connects all component seq-entries by traversing the linked list
*        all calling SeqMgrConnect and SeqMgrSeqEntry appropriately
*        if parenttype != 0, then assumes seqentry is contained in parentptr
*           and should be connected to it
*
*****************************************************************************/
NLM_EXTERN Boolean LIBCALL SeqMgrLinkSeqEntry (SeqEntryPtr sep, Uint2 parenttype, Pointer parentptr)
{
    SeqEntryPtr sep2;
    BioseqSetPtr bssp;
    Uint2 the_type;
    
    if (sep == NULL)
        return FALSE;

    if (IS_Bioseq(sep))
        the_type = OBJ_BIOSEQ;
    else
        the_type = OBJ_BIOSEQSET;

    SeqMgrSeqEntry((Uint1)the_type, sep->data.ptrvalue, sep);

    /**** if (parenttype != 0) ****/
    ObjMgrConnect(the_type, sep->data.ptrvalue, parenttype, parentptr);

    if (! IS_Bioseq(sep))
    {
        bssp = (BioseqSetPtr)(sep->data.ptrvalue);
        for (sep2 = bssp->seq_set; sep2 != NULL; sep2 = sep2->next)
        {
            SeqMgrLinkSeqEntry (sep2, the_type, sep->data.ptrvalue);
        }
    }
    return TRUE;
}
/*****************************************************************************
*
*   Selection Functions for data objects based on SeqLoc
*      See also general selection in objmgr.h
*
*****************************************************************************/

/*****************************************************************************
*
*   SeqMgrSelect(region)
*      region is a SeqLocPtr
*          It can only apply to one Bioseq
*          selected area will be extreme left and right ends
*          fuzziness is ignored
*      if something else selected, deselects it first, then selects requested
*        item
*      to select without deselecting something else, use SeqMgrAlsoSelect()
*      returns TRUE if item is now currently selected, FALSE if not
*      "region" is always copied. Caller is responsible for managment of
*         SeqLoc that is passed in.
*
*****************************************************************************/
NLM_EXTERN Boolean LIBCALL SeqMgrSelect (SeqLocPtr region)
{
    return SeqMgrGenericSelect(region, 1, NULL);
}

NLM_EXTERN Boolean LIBCALL SeqMgrAlsoSelect (SeqLocPtr region)
{
    return SeqMgrGenericSelect(region, 2, NULL);
}

/*****************************************************************************
*
*   SeqMgrDeSelect(region)
*       if this item was selected, then deselects and returns TRUE
*       else returns FALSE
*
*****************************************************************************/
NLM_EXTERN Boolean LIBCALL SeqMgrDeSelect (SeqLocPtr region)
{
    return SeqMgrGenericSelect(region, 3, NULL);
}

/*****************************************************************************
*
*   SeqMgrSetColor(region, rgb)
*
*****************************************************************************/
NLM_EXTERN Boolean LIBCALL SeqMgrSetColor (SeqLocPtr region, Uint1Ptr rgb)
{
    if (rgb == NULL) return FALSE;
        return SeqMgrGenericSelect(region, 4, rgb);
}

static Boolean NEAR SeqMgrGenericSelect (SeqLocPtr region, Int2 type,
                                           Uint1Ptr rgb)
{
    SeqInt si;
    ValNode vn;
    SeqIdPtr sip;
    Uint2 entityID;
    Uint4 itemID;

    if (region == NULL) return FALSE;

    sip = SeqLocId(region);
    if (sip == NULL) return FALSE;

    entityID = BioseqFindEntity(sip, &itemID);
    if (entityID == 0) return FALSE;

    MemSet((Pointer)(&si), 0, sizeof(SeqInt));
    MemSet((Pointer)(&vn), 0, sizeof(ValNode));

    si.id = sip;
    si.from = SeqLocStart(region);
    si.to = SeqLocStop(region);
    si.strand = SeqLocStrand(region);

    if ((si.from < 0) || (si.to < 0) || (si.from > si.to)) return FALSE;

    vn.choice = SEQLOC_INT;
    vn.data.ptrvalue = &si;

    switch (type)
    {
        case 1:
            return ObjMgrSelect(entityID, itemID, OBJ_BIOSEQ, OM_REGION_SEQLOC, &vn);
        case 2:
            return ObjMgrAlsoSelect(entityID, itemID, OBJ_BIOSEQ, OM_REGION_SEQLOC, &vn);
        case 3:
            return ObjMgrDeSelect(entityID, itemID, OBJ_BIOSEQ, OM_REGION_SEQLOC, &vn);
        case 4:
            return ObjMgrSetColor(entityID, itemID, OBJ_BIOSEQ,
                                 OM_REGION_SEQLOC, &vn, rgb);
        default:
            break;
    }

    return FALSE;
}

/*****************************************************************************
*
*   SpreadGapsInDeltaSeq(BioseqPtr bsp)
*      bsp must be a delta seq
*      function counts deltas with known lengths ( = known_len)
*               counts deltas which are gaps of unknown length ( = unk_count)
*                  these can delta of length 0, delta with fuzz = lim (unk),
*                    or SEQLOC_NULL
*               converts all unknown gaps to delta with fuzz = lim(unk)
*               sets length of all unknown gaps to
*                  (bsp->length - known_len)/unk_count
*                  any reminder spread over first few gaps
*
*****************************************************************************/
NLM_EXTERN Boolean LIBCALL SpreadGapsInDeltaSeq (BioseqPtr bsp)
{
    Boolean retval = FALSE;
    Int4 known_len = 0,
         total_gap, gap_len,
         unk_count = 0,
         remainder;
    DeltaSeqPtr dsp;
    SeqLocPtr slocp;
    SeqLitPtr slp;
    IntFuzzPtr ifp;

    if (bsp == NULL) return retval;
    if ((bsp->repr != Seq_repr_delta) || (bsp->seq_ext == NULL))
        return retval;

    retval = TRUE;  /* can function */

    for (dsp = (DeltaSeqPtr)(bsp->seq_ext); dsp != NULL; dsp = dsp->next)
    {
        switch (dsp->choice)
        {
            case 1:      /* SeqLocPtr */
                slocp = (SeqLocPtr)(dsp->data.ptrvalue);
                if (slocp == NULL) break;
                if (slocp->choice == SEQLOC_NULL)  /* convert it */
                {
                    SeqLocFree(slocp);
                    slp = SeqLitNew();
                    dsp->choice = 2;
                    dsp->data.ptrvalue = slp;
                    ifp = IntFuzzNew();
                    slp->fuzz = ifp;
                    ifp->choice = 4;   /* lim - type unk */
                    unk_count++;
                }
                else                               /* count length */
                    known_len += SeqLocLen(slocp);
                break;
            case 2:   /* SeqLitPtr */
                slp = (SeqLitPtr)(dsp->data.ptrvalue);
                if (slp == NULL) break;
                if (slp->seq_data != NULL)         /* not a gap */
                {
                    known_len += slp->length;
                    break;
                }
                ifp = slp->fuzz;
                if (slp->length == 0)  /* unknown length */
                {
                    unk_count++;
                    if (ifp != NULL)
                    {
                        if (ifp->choice != 4)  /* not lim */
                            ifp = IntFuzzFree(ifp);
                        else if (ifp->a != 0)  /* not unk */
                            ifp = IntFuzzFree(ifp);
                    }
                    if (ifp == NULL)
                    {
                        ifp = IntFuzzNew();
                        ifp->choice = 4; /* lim - unk */
                        slp->fuzz = ifp;
                    }
                }
                else                      /* gap length was set */
                {
                    if (ifp == NULL)  /* no fuzz - count length */
                        known_len += slp->length;
                    else              /* might be a guess */
                    {
                        if ((ifp->choice == 4) && (ifp->a == 0)) /* lim - unk */
                            unk_count++;
                        else
                            known_len += slp->length;
                    }
                }
                break;
            default:
                break;
        }

    }

    if (unk_count == 0)   /* no unknown gaps */
        return retval;

    total_gap = bsp->length - known_len;
    if (total_gap < 0)
        total_gap = 0;
    gap_len = total_gap / unk_count;
    remainder = total_gap - (gap_len * unk_count);

    for (dsp = (DeltaSeqPtr)(bsp->seq_ext); dsp != NULL; dsp = dsp->next)
    {
        switch (dsp->choice)
        {
            case 1:      /* SeqLocPtr */
                break;
            case 2:   /* SeqLitPtr */
                slp = (SeqLitPtr)(dsp->data.ptrvalue);
                if (slp == NULL) break;
                if (slp->seq_data != NULL) break;
                ifp = slp->fuzz;
                if (ifp == NULL) break;
                if ((ifp->choice != 4) || (ifp->a != 0))
                    break;
                slp->length = gap_len;
                if (remainder)
                {
                    slp->length++;
                    remainder--;
                }
                break;
            default:
                break;
        }
    }

    return retval;
}

/*****************************************************************************
*
*   CountGapsInDeltaSeq(BioseqPtr bsp, &num_segs, &num_gaps, &known_residues, &num_gaps_faked)
*      bsp must be a delta seq
*      function counts deltas and returns a profile
*          num_segs = total number of segments
*          num_gaps = total number of segments representing gaps
*          known_residues = number of real residues in the sequence (not gaps)
*          num_gaps_faked = number of gaps where real length is not known, but where
*                           a length was guessed by spreading the total gap length
*                           out over all gaps evenly.
*
*      NOTE: any of these pointers except bsp can be NULL
*
*      returns TRUE if values in argument were set.
*
*****************************************************************************/
static Boolean NextLitLength (DeltaSeqPtr next, Int4Ptr lenp)

{
  SeqLitPtr  slp;

  if (lenp == NULL) return FALSE;
  *lenp = 0;
  if (next == NULL || next->choice != 2) return FALSE;
  slp = (SeqLitPtr) next->data.ptrvalue;
  if (slp == NULL || slp->seq_data == NULL) return FALSE;
  *lenp = slp->length;
  return TRUE;
}

NLM_EXTERN Boolean LIBCALL CountGapsInDeltaSeq (BioseqPtr bsp, Int4Ptr num_segs, Int4Ptr num_gaps, Int4Ptr known_residues, Int4Ptr num_gaps_faked, CharPtr buf, Int4 buflen)
{
    Boolean retval = FALSE;
    Int4 residues = 0,
        segs = 0,
        gaps = 0,
        len = 0,
        fake_gaps = 0,
        from = 0, 
        tlen = 0,
        nxtlen;
    DeltaSeqPtr dsp, next;
    SeqLocPtr slocp;
    SeqLitPtr slp;
    IntFuzzPtr ifp;
    Boolean unk;
    static Char tmp[128];
    Int2 diff, blen;

    if (bsp == NULL) return retval;
    if ((bsp->repr != Seq_repr_delta) || (bsp->seq_ext == NULL))
        return retval;

    retval = TRUE;  /* can function */


    for (dsp = (DeltaSeqPtr)(bsp->seq_ext); dsp != NULL; dsp = next)
    {
        next = dsp->next;
        segs++;
        from = len + 1;
        switch (dsp->choice)
        {
            case 1:      /* SeqLocPtr */
                slocp = (SeqLocPtr)(dsp->data.ptrvalue);
                if (slocp == NULL) break;
                if (slocp->choice == SEQLOC_NULL)  /* gap */
                {
                    gaps++;
                    sprintf(tmp, "* %ld %ld gap of unknown length~", (long) from, (long) len);
                    blen = (Int2) MIN ((Int4) buflen, (Int4) sizeof (tmp));
                    diff = LabelCopy(buf, tmp, blen);
                    buflen -= diff;
                    buf += diff;
                }
                else {                              /* count length */
                    residues += SeqLocLen(slocp);
                    if (buf != NULL) {
                        tlen =  SeqLocLen(slocp);
                        len  += tlen;
                        sprintf(tmp, "* %8ld %8ld: contig of %ld bp in length~", (long) from, (long) len, (long) tlen);
                        blen = (Int2) MIN ((Int4) buflen, (Int4) sizeof (tmp));
                        diff = LabelCopy(buf, tmp, blen);
                        buflen -= diff;
                        buf += diff;
                    }
                }
                break;
            case 2:   /* SeqLitPtr */
                slp = (SeqLitPtr)(dsp->data.ptrvalue);
                if (slp == NULL) break;
                tlen =  slp->length;
                len  += tlen;
                if (slp->seq_data != NULL)
                {
                    residues += slp->length;
                    while (NextLitLength (next, &nxtlen)) {
                        tlen += nxtlen;
                        len  += nxtlen;
                        residues += nxtlen;
                        next = next->next;
                    }
                    if (buf) {
                        sprintf(tmp, "* %8ld %8ld: contig of %ld bp in length~", (long) from, (long) len, (long) tlen);
                        blen = (Int2) MIN ((Int4) buflen, (Int4) sizeof (tmp));
                        diff = LabelCopy(buf, tmp, blen);
                        buflen -= diff;
                        buf += diff;
                    }
                }
                else
                {
                    unk = FALSE;
                    gaps++;
                    ifp = slp->fuzz;
                    if (ifp != NULL)
                    {
                        if ((ifp->choice == 4) && (ifp->a == 0)) {
                            unk = TRUE;
                            fake_gaps++;
                            if (buf) {
                                if (from > len) {
                                sprintf(tmp, "*                    gap of unknown length~");
                                } else {
                                sprintf(tmp, "* %8ld %8ld: gap of unknown length~", (long) from, (long) len);
                                }
                                blen = (Int2) MIN ((Int4) buflen, (Int4) sizeof (tmp));
                                diff = LabelCopy(buf, tmp, blen);
                                buflen -= diff;
                                buf += diff;
                            }
                        }
                    }
                    if (!unk && buf) {
                        sprintf(tmp, "* %8ld %8ld: gap of %ld bp~", (long) from, (long) len, (long) tlen);
                        blen = (Int2) MIN ((Int4) buflen, (Int4) sizeof (tmp));
                        diff = LabelCopy(buf, tmp, blen);
                        buflen -= diff;
                        buf += diff;
                    }
                }
                break;
            default:
                break;
        }
    }

    if (num_segs != NULL)
        *num_segs = segs;
    if (num_gaps != NULL)
        *num_gaps = gaps;
    if (known_residues != NULL)
        *known_residues = residues;
    if (num_gaps_faked != NULL)
        *num_gaps_faked = fake_gaps;

    return retval;
}


/*****************************************************************************
*
*   SeqMgrAdd(type, data)
*       adds a Bioseq or BioseqSet to the sequence manager
*
*****************************************************************************/
NLM_EXTERN Boolean LIBCALL SeqMgrAdd (Uint2 type, Pointer data)
{
    Boolean retval;
    
    SeqMgrWriteLock(); 
    retval = ObjMgrAdd(type, data);
    if (type != OBJ_BIOSEQ) {
        SeqMgrUnlock(); 
        return retval;
    }
    retval &= SeqMgrAddToBioseqIndex((BioseqPtr)data);
    
    SeqMgrUnlock(); 
    
    return retval;
}


/*****************************************************************************
*
*   SeqMgrDelete(type, data)
*       deletes a Bioseq or BioseqSet from the sequence manager
*
*****************************************************************************/
NLM_EXTERN Boolean LIBCALL SeqMgrDelete (Uint2 type, Pointer data)
{
    Boolean retval = FALSE;

    SeqMgrWriteLock();
    if (type == OBJ_BIOSEQ)  /* remove id indexes */
        SeqMgrDeleteFromBioseqIndex((BioseqPtr)data);

    retval = ObjMgrDelete(type, data);
    SeqMgrUnlock();
    return retval;
}



static Boolean NEAR SeqMgrAddIndexElement(SeqMgrPtr smp, BioseqPtr bsp, CharPtr buf, Boolean sort_now)
{
    SeqIdIndexElementPtr sip, PNTR sipp;
    SeqIdIndexBlockPtr sibp, prev;
    Int4 imin, imax, i, j;
    CharPtr tmp, newstr;
    ObjMgrDataPtr omdp;
    ObjMgrPtr omp;

    omp = ObjMgrReadLock();
    omdp = ObjMgrFindByData(omp, (Pointer)bsp);  /* caching protection */
    ObjMgrUnlock();
    if (omdp == NULL)
    {
        return FALSE;
    }

    sipp = smp->BioseqIndex;
    if (smp->BioseqIndexCnt >= smp->BioseqIndexNum)  /* expand space */
    {
       prev = NULL;
       for (sibp = smp->BioseqIndexData; sibp != NULL; sibp = sibp->next)
           prev = sibp;
       sibp = MemNew(sizeof(SeqIdIndexBlock));
       if (prev != NULL)
           prev->next = sibp;
       else
           smp->BioseqIndexData = sibp;

       smp->BioseqIndex = MemNew((smp->BioseqIndexNum + 100) * 
                     sizeof(SeqIdIndexElementPtr));
       MemCopy(smp->BioseqIndex, sipp, (smp->BioseqIndexNum * 
                        sizeof(SeqIdIndexElementPtr)));
       MemFree(sipp);
       smp->BioseqIndexNum += 100;
       sipp = smp->BioseqIndex;
       for (i = 0, j = smp->BioseqIndexCnt; i < 100; i++, j++)
           sipp[j] = &(sibp->sid[i]);
    }

    i = smp->BioseqIndexCnt;   /* empties are at the end */
    sip = sipp[i];
    sip->omdp = omdp;       /* fill in the values */
    sip->str = StringSave(buf);
    newstr = sip->str;
    RevStringUpper(newstr);  /* try to avoid case check */

    if (! sort_now)
    {
        smp->BioseqIndexCnt++;     /* got one more */
        return TRUE;
    }

    imin = 0;                   /* find where it goes */
    imax = i-1;
    if (imax >= 0)
        tmp = sipp[imax]->str;
    if ((i) && (StringCmp(newstr, sipp[imax]->str) < 0))
    {
        i = (imax + imin) / 2;
        while (imax > imin)
        {
            tmp = sipp[i]->str;
            if ((j = StringCmp(newstr, tmp)) < 0)
                imax = i - 1;
            else if (j > 0)
                imin = i + 1;
            else
                break;
            i = (imax + imin)/2;
        }

        if (StringCmp(newstr, sipp[i]->str) > 0) /* check for off by 1 */
        {
            i++;
        }


        imax = smp->BioseqIndexCnt - 1;     /* open the array */
        while (imax >= i)
        {
            sipp[imax+1] = sipp[imax];
            imax--;
        }
    }

    sipp[i] = sip;    /* put in the pointer in order */
    smp->BioseqIndexCnt++;     /* got one more */
    return TRUE;
}

/*****************************************************************************
*
*   SeqMgrHoldIndexing(Boolean hold)
*       stops sequence indexing to allow bulk loading if hold = TRUE
*       starts it when hold = FALSE;
*       uses a counter so you must call it the same number of times
*        with TRUE as with FALSE
*       when the counter decrements to 0, it will index what it has.
*
*****************************************************************************/
NLM_EXTERN void LIBCALL SeqMgrHoldIndexing (Boolean hold)
{
    SeqMgrPtr smp;

    smp = SeqMgrWriteLock();
    if (hold)
        smp->hold_indexing++;
    else
        smp->hold_indexing--;
    SeqMgrUnlock();

    if (! smp->hold_indexing)
        SeqMgrProcessNonIndexedBioseq(FALSE);

    return;
}

int LIBCALLBACK SeqIdIndexElementCmp (VoidPtr a, VoidPtr b);

int LIBCALLBACK SeqIdIndexElementCmp (VoidPtr a, VoidPtr b)
{
    return (int)(StringCmp((*(SeqIdIndexElementPtr PNTR)a)->str,
                   (*(SeqIdIndexElementPtr PNTR)b)->str));
}

/*****************************************************************************
*
*   SeqMgrProcessNonIndexedBioseq(Boolean force_it)
*       Indexes a BioseqPtr by SeqId(s)
*       If ! force_it, respects the smp->don't index flag
*
*****************************************************************************/
static Boolean NEAR SeqMgrProcessNonIndexedBioseq(Boolean force_it)
{
    BioseqPtr PNTR bspp, bsp;
    Int4 i, total, k, old_BioseqIndexCnt;
    SeqIdPtr sip;
    Char buf[80];
    /*
    CharPtr tmp;
    */
    Uint1 oldchoice;
    Boolean indexed;
    TextSeqIdPtr tsip;
    SeqMgrPtr smp;
    Int2 version;
    Boolean sort_now = TRUE;
    TextSeqId tsi;
    SeqId si;

    smp = SeqMgrReadLock();
    if ((! smp->NonIndexedBioseqCnt) ||           /* nothing to index */
        ((! force_it) && (smp->hold_indexing)))   /* holding off on indexing */
    {
        SeqMgrUnlock();
        return TRUE;
    }
    SeqMgrUnlock();

    smp = SeqMgrWriteLock();
        if ((! smp->NonIndexedBioseqCnt) ||           /* nothing to index */
            ((! force_it) && (smp->hold_indexing)))   /* holding off on indexing */
    {
        SeqMgrUnlock();
        return TRUE;
    }

    total = smp->NonIndexedBioseqCnt;
    old_BioseqIndexCnt=smp->BioseqIndexCnt; /*** remember this one to do smart sort ****/

    if (total > 100)   /* heap sort is faster */
        sort_now = FALSE;

    bspp = smp->NonIndexedBioseq;
    for (i = 0; i < total; i++)
    {
        indexed = FALSE;
        bsp = bspp[i];
        if (bsp != NULL)
        {
            if (bsp->id != NULL)
            {
                indexed = TRUE;
                version = 0;
                for (sip = bsp->id; sip != NULL; sip = sip->next)
                {
                    oldchoice = 0;
                    switch (sip->choice)
                    {
                    case SEQID_GI:
                        sprintf(buf, "%ld", (long)(sip->data.intvalue));
                        SeqMgrAddIndexElement(smp, bsp, buf, sort_now);
                        break;
                    case SEQID_EMBL:
                    case SEQID_DDBJ:
                        oldchoice = sip->choice;
                        /*
                        sip->choice = SEQID_GENBANK;
                        */
                    case SEQID_GENBANK:
                    case SEQID_OTHER:
                    case SEQID_TPG:
                    case SEQID_TPE:
                    case SEQID_TPD:
                    case SEQID_GPIPE:
                        tsip = (TextSeqIdPtr)(sip->data.ptrvalue);
                        if (((tsip->version > 0) && (tsip->release == NULL))
                            && SHOWVERSION)
                          {
                            version = tsip->version;
                          }
                    case SEQID_PIR:
                    case SEQID_SWISSPROT:
                    case SEQID_PRF:
                        tsip = (TextSeqIdPtr)(sip->data.ptrvalue);
                        /*
                        if (tsip->name != NULL)
                        {
                            tmp = tsip->accession;
                            tsip->accession = NULL;
                            SeqIdWrite(sip, buf, PRINTID_FASTA_SHORT, 79);
                            SeqMgrAddIndexElement(smp, bsp, buf,sort_now);
                            tsip->accession = tmp;
                        }
                        */
                        /*
                        tmp = tsip->name;
                        tsip->name = NULL;
                        SeqIdWrite(sip, buf, PRINTID_FASTA_SHORT, 79);
                        SeqMgrAddIndexElement(smp, bsp, buf, sort_now);
                        */

                        MemSet ((Pointer) &tsi, 0, sizeof (TextSeqId));
                        tsi.name = tsip->name;
                        tsi.accession = tsip->accession;
                        tsi.release = tsip->release;
                        tsi.version = tsip->version;
                        MemSet ((Pointer) &si, 0, sizeof (SeqId));
                        si.choice = sip->choice;
                        if (oldchoice != 0) {
                          si.choice = SEQID_GENBANK;
                        }
                        si.data.ptrvalue = (Pointer) &tsi;

                        if (tsi.name != NULL) {
                          tsi.accession = NULL;
                          SeqIdWrite(&si, buf, PRINTID_FASTA_SHORT, 79);
                          SeqMgrAddIndexElement(smp, bsp, buf, sort_now);
                          tsi.accession = tsip->accession;
                        }
                        tsi.name = NULL;
                        SeqIdWrite(&si, buf, PRINTID_FASTA_SHORT, 79);
                        SeqMgrAddIndexElement(smp, bsp, buf, sort_now);
                        if (version) {
                          tsi.version = 0;
                          SeqIdWrite(&si, buf, PRINTID_FASTA_SHORT, 79);
                          SeqMgrAddIndexElement(smp, bsp, buf, sort_now);
                          /*
                          tsip->version = 0;
                          SeqIdWrite(sip, buf, PRINTID_FASTA_SHORT, 79);
                          SeqMgrAddIndexElement(smp, bsp, buf, sort_now);
                          tsip->version = version;
                          */
                        }
                        /*
                        tsip->name = tmp;
                        */
                        /*
                        if (oldchoice)
                            sip->choice = oldchoice;
                        */
                        break;
                    default:
                          SeqIdWrite(sip, buf, PRINTID_FASTA_SHORT, 79);
                        SeqMgrAddIndexElement(smp, bsp, buf, sort_now);
                        break;
                    }
                }
            }
        }
        if (indexed)
            bspp[i] = NULL;
    }

    /* faster single pass removal of NULLs */
    for (i = 0, k = 0; i < total; i++) {
        bsp = bspp [i];
        if (bsp != NULL) {
            bspp [k] = bsp;
            k++;
        }
    }
    total = k;

    /*
    for (i = 0; i < total; i++)
    {
        if (bspp[i] == NULL)
        {
           total--;
           for (k = i; k < total; k++)
               bspp[k] = bspp[k+1];
           i--;
        }
    }
    */

    smp->NonIndexedBioseqCnt = total;

    if (! sort_now)   /* sort at the end */
    {
        if(   old_BioseqIndexCnt > 1000 /**** sorted part of the array is large ***/
                   && (old_BioseqIndexCnt*1.1 > smp->BioseqIndexCnt ) ){ /*** unsorted part of the array is < 10% ***/
            SeqIdIndexElementPtr PNTR    bsindex_buf;
            SeqIdIndexElementPtr        stack_buf[1024];
            int    i_o, i_n, i_w;
            int    unsorted_size= smp->BioseqIndexCnt - old_BioseqIndexCnt;

#if 1
            /****  sort unsorted part ****/
            StableMergeSort((VoidPtr) (smp->BioseqIndex+old_BioseqIndexCnt), (size_t) unsorted_size,
                                  sizeof(SeqIdIndexElementPtr), SeqIdIndexElementCmp);
            /**** move new part to an array ****/
            if(unsorted_size > 1024){
                bsindex_buf=Nlm_Malloc(sizeof(*bsindex_buf)*unsorted_size); 
            } else {
                bsindex_buf=stack_buf;
            }
            MemMove((VoidPtr)bsindex_buf,(VoidPtr)(smp->BioseqIndex+old_BioseqIndexCnt),
                sizeof(*bsindex_buf)*unsorted_size);

            /**** merge both arrays from the end ****/
            i_n=unsorted_size-1;      /**** new part index ****/
            i_o=old_BioseqIndexCnt-1; /**** old part index ***/
            i_w=smp->BioseqIndexCnt-1;/**** whole array index ***/
            i=0;
            while(i_n >= 0) {
                if(   i_o < 0
                                   || SeqIdIndexElementCmp((VoidPtr)(bsindex_buf+i_n),
                               (VoidPtr)(smp->BioseqIndex+i_o)) >= 0){
                    /**** move new element ***/
                    smp->BioseqIndex[i_w]=bsindex_buf[i_n];
                    i_w--;i_n--;
                } else {
                    /**** move old element ***/
                    smp->BioseqIndex[i_w]=smp->BioseqIndex[i_o];
                                        i_w--;i_o--;
                }
                i++;
            }
            /*** cleanup *****/
            if(unsorted_size > 1024){
                MemFree(bsindex_buf);
            }
#else   
            StableMergeSort((VoidPtr) (smp->BioseqIndex), (size_t)(smp->BioseqIndexCnt),
                                  sizeof(SeqIdIndexElementPtr), SeqIdIndexElementCmp);
#endif
        } else { /** Heap Sort should be faster ***/
            StableMergeSort((VoidPtr) (smp->BioseqIndex), (size_t)(smp->BioseqIndexCnt),
                  sizeof(SeqIdIndexElementPtr), SeqIdIndexElementCmp);
        }
    }

    SeqMgrUnlock();

    return TRUE;
}



/*****************************************************************************
*
*   SeqMgrAddToBioseqIndex(bsp)
*       Indexes a BioseqPtr by SeqId(s)
*
*****************************************************************************/
NLM_EXTERN Boolean LIBCALL SeqMgrAddToBioseqIndex (BioseqPtr bsp)
{
    SeqMgrPtr smp;
    BioseqPtr PNTR bspp;

    if (bsp == NULL)
        return FALSE;

    smp = SeqMgrWriteLock();

    /* if this bsp was the last one added, no need to add it again */
    if (smp->NonIndexedBioseqCnt > 0 && smp->NonIndexedBioseq [smp->NonIndexedBioseqCnt - 1] == bsp) {
        SeqMgrUnlock();
        return TRUE;
    }

                              /* increase array as needed */
    if (smp->NonIndexedBioseqCnt >= smp->NonIndexedBioseqNum)
    {
        bspp = smp->NonIndexedBioseq;
        smp->NonIndexedBioseq = MemNew((smp->NonIndexedBioseqNum + 10) * sizeof (BioseqPtr));
        if (smp->NonIndexedBioseq == NULL) {
          Message (MSG_POSTERR, "Unable to allocate memory for bioseq index");
          smp->NonIndexedBioseq = bspp;
          return FALSE;
        }
        MemCopy(smp->NonIndexedBioseq, bspp, (smp->NonIndexedBioseqNum * sizeof(BioseqPtr)));
        MemFree(bspp);
        smp->NonIndexedBioseqNum += 10;
    }

    smp->NonIndexedBioseq[smp->NonIndexedBioseqCnt] = bsp;
    smp->NonIndexedBioseqCnt++;

    SeqMgrUnlock();

    SeqMgrProcessNonIndexedBioseq(FALSE);

    return TRUE;
}


/*****************************************************************************
*
*   SeqMgrDeleteDeleteFromBioseqIndex(bsp)
*       Removes index on BioseqPtr SeqIds
*
*****************************************************************************/
NLM_EXTERN Boolean LIBCALL SeqMgrDeleteFromBioseqIndex (BioseqPtr bsp)
{
    SeqMgrPtr smp;
    SeqIdIndexElementPtr PNTR sipp, sip;
    Int4 i, j, num;
    BioseqPtr PNTR bspp;
    ObjMgrDataPtr omdp;
    ObjMgrPtr omp;

    smp = SeqMgrWriteLock();

    /* bail if in bulk deletion of large record */
    if (bsp != NULL) {
        omdp = SeqMgrGetOmdpForBioseq (bsp);
        if (omdp != NULL && omdp->bulkIndexFree) {
            SeqMgrUnlock();
            return FALSE;
        }
    }
                                /* check if not indexed yet */
    if (smp->NonIndexedBioseqCnt > 0)
    {
        num = smp->NonIndexedBioseqCnt;
        bspp = smp->NonIndexedBioseq;
        for (i = 0; i < num; i++)
        {
            if (bspp[i] == bsp)
            {
                num--;
                for (j = i; j < num; j++)
                     bspp[j] = bspp[j+1];
                i--;
            }
        }
        smp->NonIndexedBioseqCnt = num;
    }

    num = smp->BioseqIndexCnt;
    sipp = smp->BioseqIndex;

        /*    omp = ObjMgrReadLock();  */
        
        omp = ObjMgrGet();
    omdp = ObjMgrFindByData(omp, (Pointer)bsp);
        
    /* ObjMgrUnlock(); */

    for (i = 0; i < BIOSEQ_CACHE_NUM; i++)   /* remove from BioseqFind cache */
    {
        if (omdp_cache[i] == omdp)
        {
            omdp_cache[i] = NULL;
            se_cache[i] = NULL;
        }
    }

    for (i = 0; i < num; i++)
    {
       if (sipp[i]->omdp == omdp)
       {
           sipp[i]->omdp = NULL;
           sipp[i]->str = MemFree(sipp[i]->str);
           sip = sipp[i];
           for (j = i; j < (num-1); j++)
               sipp[j] = sipp[j+1];
           sipp[j] = sip;
           num--; i--;
       }
    }

    smp->BioseqIndexCnt = num;

    SeqMgrUnlock();

    return TRUE;
}

/*****************************************************************************
*
*   SeqMgrDeleteIndexesInRecord (sep)
*       Bulk removal of SeqId index on entire entity prior to its deletion
*
*****************************************************************************/
static void MarkSeqForBulkDeletion (
  BioseqPtr bsp,
  Pointer userdata
)

{
  ObjMgrDataPtr  omdp;

  if (bsp == NULL) return;
  omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp == NULL || omdp->being_freed) return;
  omdp->bulkIndexFree = TRUE;
}

static void MarkSetForBulkDeletion (
  BioseqSetPtr bssp,
  Pointer userdata
)

{
  ObjMgrDataPtr  omdp;
  ObjMgrPtr      omp;

  if (bssp == NULL) return;
  omp = ObjMgrWriteLock ();
  omdp = ObjMgrFindByData (omp, bssp); 
  if (omdp != NULL && !omdp->being_freed) {
    omdp->bulkIndexFree = TRUE;
  }
  ObjMgrUnlock ();
}

NLM_EXTERN Boolean LIBCALL SeqMgrDeleteIndexesInRecord (SeqEntryPtr sep)

{
  BioseqPtr                  bsp;
  BioseqPtr PNTR             bspp;
  Int4                       i, j, k, num;
  ObjMgrDataPtr              omdp;
  SeqIdIndexElementPtr PNTR  sipp;
  SeqMgrPtr                  smp;
  SeqIdIndexElementPtr PNTR  tmp;

  if (sep == NULL) return FALSE;

  smp = SeqMgrWriteLock ();

  VisitBioseqsInSep (sep, NULL, MarkSeqForBulkDeletion);
  VisitSetsInSep (sep, NULL, MarkSetForBulkDeletion);

  /* check if not indexed yet */

  if (smp->NonIndexedBioseqCnt > 0) {

    num = smp->NonIndexedBioseqCnt;
    bspp = smp->NonIndexedBioseq;

    for (i = 0; i < num; i++) {
      bsp = bspp [i];
      if (bsp != NULL) {
        omdp = SeqMgrGetOmdpForBioseq (bsp);
        if (omdp != NULL && omdp->bulkIndexFree) {
          num--;
          for (j = i; j < num; j++) {
             bspp [j] = bspp [j + 1];
          }
          i--;
        }
      }
    }

    smp->NonIndexedBioseqCnt = num;
  }

  /* remove from BioseqFind cache */

  for (i = 0; i < BIOSEQ_CACHE_NUM; i++) {
    omdp = omdp_cache [i];
    if (omdp != NULL && omdp->bulkIndexFree) {
      omdp_cache [i] = NULL;
      se_cache [i] = NULL;
    }
  }

  /* bulk free of indexes from sipp list */

  sipp = smp->BioseqIndex;
  if (sipp == NULL) {
    SeqMgrUnlock ();
    return FALSE;
  }

  num = smp->BioseqIndexCnt;
  tmp = (SeqIdIndexElementPtr PNTR) MemNew (sizeof (SeqIdIndexElementPtr) * (size_t) (num + 1));
  if (tmp != NULL) {

    /* null out dying indexes, compress list, move empties to end */

    for (i = 0, j = 0, k = 0; i < num; i++) {
      omdp = sipp [i]->omdp;
      if (omdp != NULL && omdp->bulkIndexFree) {
        sipp [i]->omdp = NULL;
        sipp [i]->str = MemFree (sipp [i]->str);
        tmp [k] = sipp [i];
        k++;
      } else {
        sipp [j] = sipp [i];
        j++;
      }
    }
    /* update count of remaining indexes */

    smp->BioseqIndexCnt = j;
    MemMove (sipp + j, tmp, sizeof (SeqIdIndexElementPtr) * (size_t) k);
  }
  MemFree (tmp);

  SeqMgrUnlock ();

  return TRUE;
}

/*****************************************************************************
*
*   SeqMgrClearBioseqIndex()
*       Clears entire SeqId index for all entities
*
*****************************************************************************/
NLM_EXTERN void SeqMgrClearBioseqIndex (void)

{
  BioseqPtr PNTR             bspp;
  Int4                       i, num;
  SeqIdIndexBlockPtr         sibp, next;
  SeqIdIndexElementPtr       sip;
  SeqIdIndexElementPtr PNTR  sipp;
  SeqMgrPtr                  smp;

  smp = SeqMgrWriteLock ();

  num = smp->NonIndexedBioseqCnt;
  bspp = smp->NonIndexedBioseq;
  if (bspp != NULL) {
    for (i = 0; i < num; i++) {
      bspp [i] = NULL;
    }
  }
  smp->NonIndexedBioseqCnt = 0;
  smp->NonIndexedBioseqNum = 0;
  smp->NonIndexedBioseq = MemFree (smp->NonIndexedBioseq);

  num = smp->BioseqIndexCnt;
  sipp = smp->BioseqIndex;
  if (sipp != NULL) {
    for (i = 0; i < num; i++) {
      sip = sipp [i];
      if (sip != NULL) {
        sip->omdp = NULL;
        sip->str = MemFree (sip->str);
      }
      sipp [i] = NULL;
    }
  }
  smp->BioseqIndexCnt = 0;
  smp->BioseqIndexNum = 0;
  for (sibp = smp->BioseqIndexData; sibp != NULL; sibp = next) {
    next = sibp->next;
    MemFree (sibp);
  }
  smp->BioseqIndexData = NULL;

  SeqMgrUnlock ();
}

/*****************************************************************************
*
*   SeqMgrReplaceInBioseqIndex(bsp)
*       Replaces index on BioseqPtr SeqIds
*
*****************************************************************************/
NLM_EXTERN Boolean LIBCALL SeqMgrReplaceInBioseqIndex (BioseqPtr bsp)
{
    SeqMgrDeleteFromBioseqIndex(bsp);
    return SeqMgrAddToBioseqIndex(bsp);
}

/*****************************************************************************
*
*   GetUniGeneIDForSeqId(SeqIdPtr)
*     returns the UniGene ID for a SeqId
*     returns 0 if can't find it, or not a legal unigene id
*     This only applies to genomes division of entrez
*
*****************************************************************************/

/*****************************************************************
*
*    IT IS a KLUDGE!! Add 1,000,000 to the unigene id
*
*****************************************************************/
#define KLUDGE_UNIGENE 1000000    /*the kludge offset val add to unigene sequence*/
#define KLUDGE_FlyBase 2000000    /*the kludge offset for FlyBase*/
#define KLUDGE_JACKSON 3000000  /*the kludge offset for the Mouse data*/
#define KLUDGE_JRGP    4000000  /*the kludge offset for the rice data*/
#define KLUDGE_CESC    5000000  /*the kludge offset for the C. elegans data*/
#define KLUDGE_BSNR    6000000  /*the kludge offset for the B. subtilis data*/
#define KLUDGE_HUMGEN  7000000  /*the kludge offset for the Human genomic data*/
#define KLUDGE_YGG     8000000  /*the kludge offset for the yeast data*/
#define KLUDGE_NCBICG  9000000  /*the kludge offset for small genomes*/
#define KLUDGE_MAIZE  10000000  /*the kludge offset for corn*/

NLM_EXTERN Int4 LIBCALL GetUniGeneIDForSeqId (SeqIdPtr sip)
{
    DbtagPtr db_tag;
    ObjectIdPtr oip;

    if (sip == NULL)
        return 0;


    if(sip->choice != SEQID_GENERAL)
        return 0;

    db_tag = sip->data.ptrvalue;
    if(db_tag == NULL || db_tag->db == NULL)
        return 0;

    oip = db_tag->tag;
    if(oip == NULL || oip->id == 0)
        return 0;

    if(StringCmp(db_tag->db, "UNIGENE") == 0)
        return (KLUDGE_UNIGENE+ oip->id);
    if(StringCmp(db_tag->db, "UniGene") == 0)
        return (KLUDGE_UNIGENE+ oip->id);
    if(StringCmp(db_tag->db, "FlyBase") == 0)
        return (KLUDGE_FlyBase+ oip->id);
    if(StringCmp(db_tag->db, "JACKSON") == 0)
        return (KLUDGE_JACKSON+ oip->id);
    if(StringCmp(db_tag->db, "JRGP") == 0)
        return (KLUDGE_JRGP + oip->id);
    if(StringCmp(db_tag->db, "CESC") == 0)
        return (KLUDGE_CESC + oip->id);
    if(StringCmp(db_tag->db, "BSNR") == 0)
        return (KLUDGE_BSNR + oip->id);
        if(StringCmp(db_tag->db, "HUMGEN") == 0)
                return (KLUDGE_HUMGEN + oip->id);
        if(StringCmp(db_tag->db, "YGG") == 0)
                return (KLUDGE_YGG + oip->id);
        if(StringCmp(db_tag->db, "NCBICG") == 0)
                return (KLUDGE_NCBICG + oip->id);
        if(StringCmp(db_tag->db, "MAIZE") == 0)
                return (KLUDGE_MAIZE + oip->id);
    return 0;

}


/*****************************************************************************
*
*   BioseqExtra extensions to preindex for rapid retrieval
*
*****************************************************************************/

/*
*  remaining to be done are mapping tables for rapid coordinate conversion
*  between genome record and parts, genomic DNA and mRNA, and mRNA and protein
*/

static ObjMgrDataPtr SeqMgrGetOmdpForPointer (Pointer ptr)

{
  ObjMgrDataPtr  omdp;
  ObjMgrPtr      omp;

  if (ptr == NULL) return NULL;
  omp = ObjMgrWriteLock ();
  omdp = ObjMgrFindByData (omp, ptr);
  ObjMgrUnlock ();
  return omdp;
}

NLM_EXTERN ObjMgrDataPtr SeqMgrGetOmdpForBioseq (BioseqPtr bsp)

{
  ObjMgrDataPtr  omdp = NULL;
  ObjMgrPtr      omp;

  if (bsp == NULL) return NULL;
  omp = ObjMgrWriteLock ();
  omdp = (ObjMgrDataPtr) bsp->omdp;
  if (omdp == NULL) {
    omdp = ObjMgrFindByData (omp, bsp);
    bsp->omdp = (Pointer) omdp;
  }
  ObjMgrUnlock ();
  return omdp;
}

NLM_EXTERN Pointer SeqMgrGetExtraDataForOmdp (ObjMgrDataPtr omdp)

{
  Pointer    extradata;
  ObjMgrPtr  omp;

  if (omdp == NULL) return NULL;
  omp = ObjMgrWriteLock ();
  extradata = (Pointer) omdp->extradata;
  ObjMgrUnlock ();
  return extradata;
}

static SeqEntryPtr SeqMgrGetTopSeqEntryForEntity (Uint2 entityID)

{
  ObjMgrDataPtr  omdp;
  SeqSubmitPtr   ssp;

  omdp = ObjMgrGetData (entityID);
  if (omdp == NULL) return FALSE;
  switch (omdp->datatype) {
    case OBJ_SEQSUB :
      ssp = (SeqSubmitPtr) omdp->dataptr;
      if (ssp != NULL && ssp->datatype == 1) {
        return (SeqEntryPtr) ssp->data;
      }
      break;
    case OBJ_BIOSEQ :
    case OBJ_BIOSEQSET :
      return (SeqEntryPtr) omdp->choice;
    default :
      break;
  }
  return NULL;
}


static Boolean SeqMgrClearBioseqExtraData (ObjMgrDataPtr omdp)

{
  BioseqExtraPtr  bspextra;
  SMFeatBlockPtr  currf;
  SMSeqIdxPtr     currp;
  Int2            i;
  SMFeatItemPtr   itemf;
  Int4            j;
  SMFeatBlockPtr  nextf;
  SMSeqIdxPtr     nextp;
  SMFidItemPtr    sfip;

  if (omdp == NULL) return FALSE;
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return FALSE;

  /* free sorted arrays of pointers into data blocks */

  bspextra->descrsByID = MemFree (bspextra->descrsByID);
  bspextra->descrsBySdp = MemFree (bspextra->descrsBySdp);
  bspextra->descrsByIndex = MemFree (bspextra->descrsByIndex);

  bspextra->annotDescByID = MemFree (bspextra->annotDescByID);

  bspextra->alignsByID = MemFree (bspextra->alignsByID);

  bspextra->featsByID = MemFree (bspextra->featsByID);
  bspextra->featsBySfp = MemFree (bspextra->featsBySfp);
  bspextra->featsByPos = MemFree (bspextra->featsByPos);
  bspextra->featsByRev = MemFree (bspextra->featsByRev);
  bspextra->featsByLabel = MemFree (bspextra->featsByLabel);

  bspextra->genesByPos = MemFree (bspextra->genesByPos);
  bspextra->mRNAsByPos = MemFree (bspextra->mRNAsByPos);
  bspextra->CDSsByPos = MemFree (bspextra->CDSsByPos);
  bspextra->pubsByPos = MemFree (bspextra->pubsByPos);
  bspextra->orgsByPos = MemFree (bspextra->orgsByPos);
  bspextra->operonsByPos = MemFree (bspextra->operonsByPos);

  bspextra->genesByLocusTag = MemFree (bspextra->genesByLocusTag);

  /* free list of descriptor information */

  bspextra->desclisthead = ValNodeFreeData (bspextra->desclisthead);

  /* free arrays to speed mapping from parts to segmented bioseq */

  bspextra->partsByLoc = MemFree (bspextra->partsByLoc);
  bspextra->partsBySeqId = MemFree (bspextra->partsBySeqId);

  /* free data blocks of feature information */

  currf = bspextra->featlisthead;
  while (currf != NULL) {
    nextf = currf->next;

    if (currf->data != NULL) {

      /* free allocated label strings within block items */

      for (i = 0; i < currf->index; i++) {
        itemf = &(currf->data [i]);
        MemFree (itemf->label);
        MemFree (itemf->ivals);
      }

      /* free array of SMFeatItems */

      MemFree (currf->data);
    }

    MemFree (currf);
    currf = nextf;
  }

  /* free data blocks of parts to segment mapping information */

  currp = bspextra->segparthead;
  while (currp != NULL) {
    nextp = currp->next;
    SeqLocFree (currp->slp);
    MemFree (currp->seqIdOfPart);
    MemFree (currp);
    currp = nextp;
  }

  /* free list of seqfeatptrs whose product points to the bioseq */

  bspextra->prodlisthead = ValNodeFree (bspextra->prodlisthead);

  if (bspextra->featsByFeatID != NULL) {
    for (j = 0; j < bspextra->numfids; j++) {
      sfip = bspextra->featsByFeatID [j];
      if (sfip == NULL) continue;
      MemFree (sfip->fid);
      MemFree (sfip);
    }
    bspextra->featsByFeatID = MemFree (bspextra->featsByFeatID);
  }

  /* clean interval list once implemented */

  bspextra->featlisthead = NULL;
  bspextra->featlisttail = NULL;
  bspextra->segparthead = NULL;

  bspextra->numaligns = 0;
  bspextra->numfeats = 0;
  bspextra->numgenes = 0;
  bspextra->nummRNAs = 0;
  bspextra->numCDSs = 0;
  bspextra->numpubs = 0;
  bspextra->numorgs = 0;
  bspextra->numoperons = 0;
  bspextra->numfids = 0;
  bspextra->numsegs = 0;

  bspextra->min = INT4_MAX;
  bspextra->processed = UINT1_MAX;
  bspextra->blocksize = 50;

  bspextra->protFeat = NULL;
  bspextra->cdsOrRnaFeat = NULL;

  /* free genome - parts mapping arrays when they are added */

  return TRUE;
}

static Boolean DoSeqMgrFreeBioseqExtraData (ObjMgrDataPtr omdp)

{
  if (omdp == NULL) return FALSE;
  if (omdp->datatype != OBJ_BIOSEQ && omdp->datatype != OBJ_BIOSEQSET) return FALSE;
  if (omdp->extradata != NULL) {
    SeqMgrClearBioseqExtraData (omdp);
    omdp->extradata = MemFree (omdp->extradata);
    omdp->reapextra = NULL;
    omdp->reloadextra = NULL;
    omdp->freeextra = NULL;
  }
  return TRUE;
}

/* object manager callbacks to reap, reload, and free extra bioseq data */

NLM_EXTERN Pointer LIBCALLBACK SeqMgrReapBioseqExtraFunc (Pointer data)

{
  BioseqExtraPtr  bspextra;
  SMFeatBlockPtr  curr;
  Int2            i;
  SMFeatItemPtr   item;
  ObjMgrDataPtr   omdp;
  SMDescItemPtr   sdip;
  ValNodePtr      vnp;

  omdp = (ObjMgrDataPtr) data;
  if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return NULL;
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return NULL;

  /* loop through data blocks of feature information */

  curr = bspextra->featlisthead;
  while (curr != NULL) {

    /* NULL out pointers to cached out feature and annot */

    if (curr->data != NULL) {
      for (i = 0; i < curr->index; i++) {
        item = &(curr->data [i]);
        item->sfp = NULL;
        item->sap = NULL;
      }
    }

    curr = curr->next;
  }

  /* these were originally only recorded if omdp->tempload == TL_NOT_TEMP */

  bspextra->protFeat = NULL;
  bspextra->cdsOrRnaFeat = NULL;

  /* NULL out pointers to cached out descriptors */

  for (vnp = bspextra->desclisthead; vnp != NULL; vnp = vnp->next) {
    sdip = (SMDescItemPtr) vnp->data.ptrvalue;
    if (sdip != NULL) {
      sdip->sdp = NULL;
      sdip->sep = NULL;
    }
  }

  return NULL;
}

/* !!! SeqMgrReloadBioseqExtraFunc is not yet implemented !!! */

NLM_EXTERN Pointer LIBCALLBACK SeqMgrReloadBioseqExtraFunc (Pointer data)

{
  return NULL;
}

NLM_EXTERN Pointer LIBCALLBACK SeqMgrFreeBioseqExtraFunc (Pointer data)

{
  DoSeqMgrFreeBioseqExtraData ((ObjMgrDataPtr) data);
  return NULL;
}

/*****************************************************************************
*
*   SeqMgrClearFeatureIndexes clears every bioseq in an entity
*
*****************************************************************************/

static void SeqMgrClearIndexesProc (SeqEntryPtr sep, Pointer mydata, Int4 index, Int2 indent)

{
  BioseqPtr      bsp;
  BioseqSetPtr   bssp;
  ObjMgrDataPtr  omdp = NULL;
  BoolPtr        rsult;
  SeqAlignPtr    sal;
  SeqAnnotPtr    sap = NULL;

  if (sep == NULL || (! IS_Bioseq (sep))) return;
  if (IS_Bioseq (sep)) {
    bsp = (BioseqPtr) sep->data.ptrvalue;
    if (bsp == NULL) return;
    omdp = SeqMgrGetOmdpForBioseq (bsp);
    sap = bsp->annot;
  } else if (IS_Bioseq_set (sep)) {
    bssp = (BioseqSetPtr) sep->data.ptrvalue;
    if (bssp == NULL) return;
    omdp = SeqMgrGetOmdpForPointer (bssp);
    sap = bssp->annot;
  } else return;
  while (sap != NULL) {
    if (sap->type == 2) {
      sal = (SeqAlignPtr) sap->data;
      while (sal != NULL) {
        /* ! clean up seq-align indexes ! */
        sal = sal->next;
      }
    }
    sap = sap->next;
  }
  if (omdp != NULL && DoSeqMgrFreeBioseqExtraData (omdp)) {
    rsult = (BoolPtr) mydata;
    *rsult = TRUE;
  }
}

NLM_EXTERN Boolean LIBCALL SeqMgrClearFeatureIndexes (Uint2 entityID, Pointer ptr)

{
  ObjMgrDataPtr  omdp;
  Boolean        rsult = FALSE;
  SeqEntryPtr    sep;

  if (entityID == 0) {
    entityID = ObjMgrGetEntityIDForPointer (ptr);
  }
  if (entityID == 0) return FALSE;
  sep = SeqMgrGetTopSeqEntryForEntity (entityID);
  if (sep == NULL) return FALSE;
  SeqEntryExplore (sep, (Pointer) (&rsult), SeqMgrClearIndexesProc);

  /* clear out object manager time of indexing flag and master feature itemID list */

  omdp = ObjMgrGetData (entityID);
  if (omdp != NULL) {
    omdp->indexed = 0;
    SeqMgrClearBioseqExtraData (omdp);
    omdp->extradata = MemFree (omdp->extradata);
    omdp->reapextra = NULL;
    omdp->reloadextra = NULL;
    omdp->freeextra = NULL;
  }
  return rsult;
}

/*****************************************************************************
*
*   IsNonGappedLiteral(BioseqPtr bsp)
*      Returns TRUE if bsp is a delta seq is composed only of Seq-lits with
*      actual sequence data.  These are now made to allow optimal compression
*      of otherwise raw sequences with runs of ambiguous bases.
*
*****************************************************************************/

NLM_EXTERN Boolean IsNonGappedLiteral (BioseqPtr bsp)

{
  DeltaSeqPtr  dsp;
  SeqLitPtr    slitp;

  if (bsp == NULL || bsp->repr != Seq_repr_delta) return FALSE;
  if (bsp->seq_ext_type != 4 || bsp->seq_ext == NULL) return FALSE;

  for (dsp = (DeltaSeqPtr) bsp->seq_ext; dsp != NULL; dsp = dsp->next) {
    if (dsp->choice != 2) return FALSE; /* not Seq-lit */
    slitp = (SeqLitPtr) dsp->data.ptrvalue;
    if (slitp == NULL) return FALSE;
    if (slitp->seq_data == NULL || slitp->length == 0) return FALSE; /* gap */
  }

  return TRUE;
}

/*****************************************************************************
*
*   FindAppropriateBioseq finds the segmented bioseq if location is join on parts
*
*****************************************************************************/

static BioseqPtr FindAppropriateBioseq (SeqLocPtr loc, BioseqPtr tryfirst, BoolPtr is_small_genome_set)

{
  BioseqPtr       bsp = NULL;
  BioseqExtraPtr  bspextra;
  BioseqSetPtr    bssp;
  ObjMgrDataPtr   omdp;
  BioseqPtr       part;
  SeqEntryPtr     sep;
  SeqIdPtr        sip;
  SeqLocPtr       slp;

  if (is_small_genome_set != NULL) {
    *is_small_genome_set = FALSE;
  }
  if (loc == NULL) return NULL;
  sip = SeqLocId (loc);
  if (sip != NULL) {
    if (tryfirst != NULL && SeqIdIn (sip, tryfirst->id)) {
      bsp = tryfirst;
    } else {
      bsp = BioseqFindCore (sip);
    }

    /* first see if this is raw local part of segmented bioseq */

    if (bsp != NULL && (bsp->repr == Seq_repr_raw || IsNonGappedLiteral (bsp))) {
      omdp = SeqMgrGetOmdpForBioseq (bsp);
      if (omdp != NULL && omdp->datatype == OBJ_BIOSEQ) {
        bspextra = (BioseqExtraPtr) omdp->extradata;
        if (bspextra != NULL) {
          if (bspextra->parentBioseq != NULL) {
            bsp = bspextra->parentBioseq;
          }
        }
      }
    }
    return bsp;
  }

  /* otherwise assume location is on multiple parts of a segmented set (deprecated) or is in a small genome set */

  slp = SeqLocFindNext (loc, NULL);
  if (slp == NULL) return NULL;
  sip = SeqLocId (slp);
  if (sip == NULL) return NULL;
  part = BioseqFindCore (sip);
  if (part == NULL) return NULL;
  omdp = SeqMgrGetOmdpForBioseq (part);
  while (omdp != NULL) {
    if (omdp->datatype == OBJ_BIOSEQSET) {
      bssp = (BioseqSetPtr) omdp->dataptr;
      if (bssp != NULL) {
        if (bssp->_class == BioseqseqSet_class_segset) {
          for (sep = bssp->seq_set; sep != NULL; sep = sep->next) {
            if (IS_Bioseq (sep)) {
              bsp = (BioseqPtr) sep->data.ptrvalue;
              if (bsp != NULL) {
                return bsp;
              }
            }
          }
        } else if (bssp->_class == BioseqseqSet_class_small_genome_set) {
          if (is_small_genome_set != NULL) {
            *is_small_genome_set = TRUE;
          }
          return part;
        }
      }
    }
    omdp = SeqMgrGetOmdpForPointer (omdp->parentptr);
  }
  return NULL;
}

/*****************************************************************************
*
*   FindFirstLocalBioseq is called as a last resort if FindAppropriateBioseq
*     fails, and it scans the feature location to find the first local bioseq
*     referenced by a feature interval
*
*****************************************************************************/

static BioseqPtr FindFirstLocalBioseq (SeqLocPtr loc)

{
  BioseqPtr  bsp;
  SeqIdPtr   sip;
  SeqLocPtr  slp = NULL;

  if (loc == NULL) return NULL;

  while ((slp = SeqLocFindNext (loc, slp)) != NULL) {
    sip = SeqLocId (slp);
    if (sip != NULL) {
      bsp = BioseqFindCore (sip);
      if (bsp != NULL) return bsp;
    }
  }

  return NULL;
}

/*****************************************************************************
*
*   BioseqFindFromSeqLoc finds the segmented bioseq if location is join on parts,
*     and does so even if some of the intervals are far accessions.
*
*****************************************************************************/

NLM_EXTERN BioseqPtr BioseqFindFromSeqLoc (SeqLocPtr loc)

{
  BioseqPtr  bsp = NULL;

  if (loc == NULL) return NULL;
  bsp = FindAppropriateBioseq (loc, NULL, NULL);
  if (bsp == NULL) {
    bsp = FindFirstLocalBioseq (loc);
  }
  return bsp;
}

/*****************************************************************************
*
*   SeqMgrGetParentOfPart returns the segmented bioseq parent of a part bioseq,
*     and fills in the context structure.
*
*****************************************************************************/

NLM_EXTERN BioseqPtr LIBCALL SeqMgrGetParentOfPart (BioseqPtr bsp,
                                                    SeqMgrSegmentContext PNTR context)

{
  BioseqExtraPtr    bspextra;
  Char              buf [80];
  Int2              compare;
  Uint2             entityID;
  Int4              i;
  Int4              numsegs;
  ObjMgrDataPtr     omdp;
  BioseqPtr         parent;
  SMSeqIdxPtr PNTR  partsByLoc;
  SMSeqIdxPtr PNTR  partsBySeqId;
  SMSeqIdxPtr       segpartptr;
  SeqIdPtr          sip;
  SeqLocPtr         slp;
  Int4              L, R, mid;

  if (context != NULL) {
    MemSet ((Pointer) context, 0, sizeof (SeqMgrSegmentContext));
  }
  if (bsp == NULL) return NULL;
  omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp == NULL) return NULL;
  if (omdp->datatype != OBJ_BIOSEQ) return NULL;
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return NULL;
  entityID = bsp->idx.entityID;
  if (entityID < 1) {
    entityID = ObjMgrGetEntityIDForPointer (omdp->dataptr);
  }

  parent = bspextra->parentBioseq;
  if (parent == NULL) return NULL;

  /* now need parts list from extra data on parent */

  omdp = SeqMgrGetOmdpForBioseq (parent);
  if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return parent;
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return parent;

  partsBySeqId = bspextra->partsBySeqId;
  numsegs = bspextra->numsegs;
  if (partsBySeqId == NULL || numsegs < 1) return parent;

  sip = bsp->id;
  if (sip == NULL) return parent;

  /* binary search into array on segmented bioseq sorted by part seqID (reversed) string */

  while (sip != NULL) {
    if (MakeReversedSeqIdString (sip, buf, sizeof (buf) - 1)) {
      L = 0;
      R = numsegs - 1;
      while (L < R) {
        mid = (L + R) / 2;
        segpartptr = partsBySeqId [mid];
        compare = StringCmp (segpartptr->seqIdOfPart, buf);
        if (compare < 0) {
          L = mid + 1;
        } else {
          R = mid;
        }
      }
      segpartptr = partsBySeqId [R];
      if (segpartptr != NULL && StringCmp (segpartptr->seqIdOfPart, buf) == 0) {
        if (context != NULL) {
          slp = segpartptr->slp;
          context->entityID = entityID;
          context->itemID = segpartptr->itemID;
          context->slp = slp;
          context->parent = segpartptr->parentBioseq;
          context->cumOffset = segpartptr->cumOffset;
          context->from = segpartptr->from;
          context->to = segpartptr->to;
          context->strand = segpartptr->strand;
          context->userdata = NULL;
          context->omdp = (Pointer) omdp;
          context->index = 0;

          /* now find entry in partsByLoc list to set proper index */

          partsByLoc = bspextra->partsByLoc;
          if (partsByLoc != NULL) {
            i = 0;
            while (i < numsegs) {
              if (segpartptr == partsByLoc [i]) {
                context->index = i + 1;
              }
              i++;
            }
          }
        }
        return parent;
      }
    }
    sip = sip->next;
  }

  return parent;
}

/*****************************************************************************
*
*   SeqMgrGetBioseqContext fills in the context structure for any bioseq.
*
*****************************************************************************/

NLM_EXTERN Boolean LIBCALL SeqMgrGetBioseqContext (BioseqPtr bsp,
                                                   SeqMgrBioseqContext PNTR context)

{
  BioseqExtraPtr  bspextra;
  Uint2           entityID;
  ObjMgrDataPtr   omdp;
  SeqEntryPtr     sep;

  if (context != NULL) {
    MemSet ((Pointer) context, 0, sizeof (SeqMgrBioseqContext));
  }
  if (bsp == NULL || context == NULL) return FALSE;

  entityID = bsp->idx.entityID;
  if (entityID < 1) {
    entityID = ObjMgrGetEntityIDForPointer (bsp);
  }
  if (entityID == 0) return FALSE;

  sep = SeqMgrGetTopSeqEntryForEntity (entityID);
  if (sep == NULL) return FALSE;

  context->entityID = entityID;
  context->index = 0;
  context->userdata = NULL;

  omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp != NULL && omdp->datatype == OBJ_BIOSEQ) {
    bspextra = (BioseqExtraPtr) omdp->extradata;
    if (bspextra != NULL) {
      context->itemID = bspextra->bspItemID;
      context->bsp = bsp;
      context->sep = bsp->seqentry;
      if (bsp->idx.parenttype == OBJ_BIOSEQSET) {
        context->bssp = (BioseqSetPtr) bsp->idx.parentptr;
      }
      context->numsegs = bspextra->numsegs;
      context->omdp = omdp;
      context->index = bspextra->bspIndex;
    }
  }

  return (Boolean) (context->index != 0);
}

/*****************************************************************************
*
*   GetOffsetInNearBioseq is called to get the intervals on near bioseqs
*
*****************************************************************************/

static Int4 GetOffsetInNearBioseq (SeqLocPtr loc, BioseqPtr in, Uint1 which_end)

{
  BioseqPtr  bsp;
  SeqLocPtr  first = NULL, last = NULL, slp = NULL;
  SeqIdPtr   sip;
  Uint1      strand;
  Int4       val;

  if (loc == NULL) return -1;

  /* first attempt should work if no far bioseqs */

  val = GetOffsetInBioseq (loc, in, which_end);
  if (val != -1) return val;

  /* now go through sublocs and find extremes that are not on far bioseqs */

  while ((slp = SeqLocFindNext (loc, slp)) != NULL) {
    sip = SeqLocId (slp);
    if (sip != NULL) {
      bsp = BioseqFind (sip);
      if (bsp != NULL) {
        last = slp;
        if (first == NULL) {
          first = slp;
        }
      }
    }
  }
  if (first == NULL) return -1;
  strand = SeqLocStrand (first);

  switch (which_end) {
    case SEQLOC_LEFT_END:
      if (strand == Seq_strand_minus) {
        return GetOffsetInBioseq (last, in, which_end);
      } else {
        return GetOffsetInBioseq (first, in, which_end);
      }
      break;
    case SEQLOC_RIGHT_END:
      if (strand == Seq_strand_minus) {
        return GetOffsetInBioseq (first, in, which_end);
      } else {
        return GetOffsetInBioseq (last, in, which_end);
      }
      break;
    case SEQLOC_START:
      return GetOffsetInBioseq (first, in, which_end);
      break;
    case SEQLOC_STOP:
      return GetOffsetInBioseq (last, in, which_end);
      break;
    default :
      break;
  }

  return -1;
}


static void GetLeftAndRightOffsetsInNearBioseq (
  SeqLocPtr loc,
  BioseqPtr in,
  Int4Ptr left,
  Int4Ptr right,
  Boolean small_genome_set,
  Boolean bad_order,
  Boolean mixed_strand
)

{
  BioseqPtr  bsp;
  Int4       cur_left = -1, cur_right = -1;
  SeqLocPtr  first = NULL, last = NULL, slp = NULL;
  SeqIdPtr   sip;
  Uint1      strand;
  Int4       val_left = -1, val_right = -1;
  Boolean    left_flip = FALSE, right_flip = FALSE;

  if (left != NULL) {
    *left = -1;
  }
  if (right != NULL) {
    *right = -1;
  }
  if (loc == NULL) return;

  /* first attempt should work if no far bioseqs */
  sip = SeqLocId (loc);
  if (in != NULL && SeqIdIn (sip, in->id)) {
    bsp = in;
  } else {
    bsp = BioseqFind (sip);
  }
  if (bsp != NULL) {
    GetLeftAndRightOffsetsInBioseq (loc, in, &val_left, &val_right, bsp->topology == TOPOLOGY_CIRCULAR, &left_flip, &right_flip);
    if (val_left != -1 && val_right != -1) {
      if (left != NULL) {
        *left = val_left;
      }
      if (right != NULL) {
        *right = val_right;
      }
      return;
    }
  }

  /* now go through sublocs and find extremes that are not on far bioseqs */

  while ((slp = SeqLocFindNext (loc, slp)) != NULL) {
    sip = SeqLocId (slp);
    if (sip != NULL) {
      bsp = BioseqFind (sip);
      if (bsp != NULL && ((! small_genome_set) || bsp == in)) {
        last = slp;
        if (first == NULL) {
          first = slp;
        }
      }
    }
  }
  if (first == NULL) return;
  strand = SeqLocStrand (first);

  if (strand == Seq_strand_minus) {
    val_left = GetOffsetInBioseq (last, in, SEQLOC_LEFT_END);
    val_right = GetOffsetInBioseq (first, in, SEQLOC_RIGHT_END);
  } else {
    val_left = GetOffsetInBioseq (first, in, SEQLOC_LEFT_END);
    val_right = GetOffsetInBioseq (last, in, SEQLOC_RIGHT_END);
  }

  if (left != NULL) {
    *left = val_left;
  }
  if (right != NULL) {
    *right = val_right;
  }
}


/*
static Int4 GetOffsetInFirstLocalBioseq (SeqLocPtr loc, BioseqPtr in, Uint1 which_end)

{
  SeqLocPtr  slp = NULL;
  Int4       val;

  if (loc == NULL) return -1;

  while ((slp = SeqLocFindNext (loc, slp)) != NULL) {
    val = GetOffsetInBioseq (slp, in, which_end);
    if (val != -1) return val;
  }

  return -1;
}
*/

/*****************************************************************************
*
*   SeqMgrFindSMFeatItemPtr and SeqMgrFindSMFeatItemByID return SMFeatItemPtr
*     to access internal fields
*   SeqMgrGetDesiredDescriptor and SeqMgrGetDesiredFeature take an itemID,
*     position index, or SeqDescPtr or SeqFeatPtr, return the SeqDescPtr or
*     SeqFeatPtr, and fill in the context structure
*
*****************************************************************************/

NLM_EXTERN SMFeatItemPtr LIBCALL SeqMgrFindSMFeatItemPtr (SeqFeatPtr sfp)

{
  SMFeatItemPtr PNTR  array;
  BioseqPtr           bsp;
  BioseqExtraPtr      bspextra;
  SMFeatBlockPtr      curr;
  Int2                i;
  SMFeatItemPtr       item;
  Int4                L;
  Int4                mid;
  ObjMgrDataPtr       omdp;
  Int4                R;

  if (sfp == NULL) return NULL;
  bsp = FindAppropriateBioseq (sfp->location, NULL, NULL);
  if (bsp == NULL) {
    bsp = FindFirstLocalBioseq (sfp->location);
  }
  omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return NULL;
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return NULL;

  /* first try array sorted by SeqFeatPtr value */

  array = bspextra->featsBySfp;
  if (array != NULL && bspextra->numfeats > 0) {
    L = 0;
    R = bspextra->numfeats - 1;
    while (L < R) {
      mid = (L + R) / 2;
      item = array [mid];
      if (item != NULL && item->sfp < sfp) {
        L = mid + 1;
      } else {
        R = mid;
      }
    }

    item = array [R];
    if (item->sfp == sfp) return item;
  }

  /* now look in feature indices for cached feature information */

  curr = bspextra->featlisthead;
  while (curr != NULL) {

    if (curr->data != NULL) {
      for (i = 0; i < curr->index; i++) {
        item = &(curr->data [i]);
        if (item->sfp == sfp && (! item->ignore)) return item;
      }
    }

    curr = curr->next;
  }

  return NULL;
}

NLM_EXTERN SMFeatItemPtr LIBCALL SeqMgrFindSMFeatItemByID (Uint2 entityID, BioseqPtr bsp, Uint4 itemID)

{
  SMFeatItemPtr PNTR  array;
  BioseqExtraPtr      bspextra;
  SMFeatBlockPtr      curr;
  Int2                i;
  SMFeatItemPtr       item;
  Int4                L;
  Int4                mid;
  ObjMgrDataPtr       omdp;
  Int4                R;

  if (entityID > 0) {
    omdp = ObjMgrGetData (entityID);
    if (omdp == NULL) return NULL;
  } else {
    if (bsp == NULL) return NULL;
    omdp = SeqMgrGetOmdpForBioseq (bsp);
    if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return NULL;
  }
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return NULL;

  /* first try array sorted by itemID value */

  array = bspextra->featsByID;
  if (array != NULL && bspextra->numfeats > 0) {
    L = 0;
    R = bspextra->numfeats - 1;
    while (L < R) {
      mid = (L + R) / 2;
      item = array [mid];
      if (item != NULL && item->itemID < itemID) {
        L = mid + 1;
      } else {
        R = mid;
      }
    }

    item = array [R];
    if (item->itemID == itemID) return item;
  }

  /* now look in feature indices for cached feature information */

  curr = bspextra->featlisthead;
  while (curr != NULL) {

    if (curr->data != NULL) {
      for (i = 0; i < curr->index; i++) {
        item = &(curr->data [i]);
        if (item->itemID == itemID && (! item->ignore)) return item;
      }
    }

    curr = curr->next;
  }

  return NULL;
}

static Int4 ItemIDfromAnnotDesc (AnnotDescPtr adp)

{
  ObjValNodePtr  ovp;

  if (adp == NULL || adp->extended == 0) return 0;
  ovp = (ObjValNodePtr) adp;
  return ovp->idx.itemID;
}

NLM_EXTERN AnnotDescPtr LIBCALL SeqMgrFindAnnotDescByID (Uint2 entityID, Uint4 itemID)

{
  AnnotDescPtr PNTR  array;
  BioseqExtraPtr     bspextra;
  AnnotDescPtr       item;
  Int4               L;
  Int4               mid;
  ObjMgrDataPtr      omdp;
  Int4               R;

  if (entityID < 1) return NULL;
  omdp = ObjMgrGetData (entityID);
  if (omdp == NULL) return NULL;
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return NULL;

  array = bspextra->annotDescByID;
  if (array != NULL && bspextra->numannotdesc > 0) {
    L = 0;
    R = bspextra->numannotdesc - 1;
    while (L < R) {
      mid = (L + R) / 2;
      item = array [mid];
      if (item != NULL && ItemIDfromAnnotDesc (item) < itemID) {
        L = mid + 1;
      } else {
        R = mid;
      }
    }

    item = array [R];
    if (ItemIDfromAnnotDesc (item) == itemID) return item;
  }

  return NULL;
}

NLM_EXTERN SeqAlignPtr LIBCALL SeqMgrFindSeqAlignByID (Uint2 entityID, Uint4 itemID)

{
  BioseqExtraPtr  bspextra;
  ObjMgrDataPtr   omdp;

  if (entityID < 1) return NULL;
  omdp = ObjMgrGetData (entityID);
  if (omdp == NULL) return NULL;
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return NULL;

  if (bspextra->alignsByID != NULL && bspextra->numaligns > 0 &&
      itemID > 0 && itemID <= (Uint4) bspextra->numaligns) {
    return bspextra->alignsByID [itemID];
  }

  return NULL;
}

NLM_EXTERN SeqFeatPtr LIBCALL SeqMgrGetDesiredFeature (Uint2 entityID, BioseqPtr bsp,
                                                       Uint4 itemID, Uint4 index, SeqFeatPtr sfp,
                                                       SeqMgrFeatContext PNTR context)

{
  SMFeatItemPtr PNTR  array;
  BioseqExtraPtr      bspextra;
  SeqFeatPtr          curr;
  SMFeatItemPtr       item = NULL;
  ObjMgrDataPtr       omdp;

  if (context != NULL) {
    MemSet ((Pointer) context, 0, sizeof (SeqMgrFeatContext));
  }
  if (entityID > 0) {
    omdp = ObjMgrGetData (entityID);
    if (omdp == NULL) return NULL;
  } else {
    if (bsp == NULL) return NULL;
    omdp = SeqMgrGetOmdpForBioseq (bsp);
    if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return NULL;
  }
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return NULL;

  if (itemID > 0) {
    item = SeqMgrFindSMFeatItemByID (entityID, bsp, itemID);
  } else if (index > 0) {
    array = bspextra->featsByPos;
    if (array != NULL && bspextra->numfeats > 0 && index <= (Uint4) bspextra->numfeats) {
      item = array [index - 1];
    }
  } else if (sfp != NULL) {
    item = SeqMgrFindSMFeatItemPtr (sfp);
  }
  if (item == NULL) return NULL;

  entityID = ObjMgrGetEntityIDForPointer (omdp->dataptr);

  curr = item->sfp;
  if (curr != NULL && context != NULL && (! item->ignore)) {
    context->entityID = entityID;
    context->itemID = item->itemID;
    context->sfp = curr;
    context->sap = item->sap;
    context->bsp = item->bsp;
    context->label = item->label;
    context->left = item->left;
    context->right = item->right;
    context->dnaStop = item->dnaStop;
    context->partialL = item->partialL;
    context->partialR = item->partialR;
    context->farloc = item->farloc;
    context->bad_order = item->bad_order;
    context->mixed_strand = item->mixed_strand;
    context->ts_image = item->ts_image;
    context->strand = item->strand;
    if (curr != NULL) {
      context->seqfeattype = curr->data.choice;
    } else {
      context->seqfeattype = FindFeatFromFeatDefType (item->subtype);
    }
    context->featdeftype = item->subtype;
    context->numivals = item->numivals;
    context->ivals = item->ivals;
    context->userdata = NULL;
    context->omdp = (Pointer) omdp;
    context->index = item->index + 1;
  }
  return curr;
}

/*
static ValNodePtr DesiredDescriptorPerBioseq (SeqEntryPtr sep, BioseqPtr bsp,
                                              Uint2 itemID, Uint2 index, ValNodePtr sdp,
                                              SeqMgrDescContext PNTR context)

{
  BioseqSetPtr  bssp;
  ValNodePtr    curr = NULL;
  SeqEntryPtr   tmp;

  if (sep != NULL) {
    if (IS_Bioseq (sep)) {
      bsp = (BioseqPtr) sep->data.ptrvalue;
      if (bsp == NULL) return NULL;
    } else if (IS_Bioseq_set (sep)) {
      bssp = (BioseqSetPtr) sep->data.ptrvalue;
      if (bssp == NULL) return NULL;
      for (tmp = bssp->seq_set; tmp != NULL; tmp = tmp->next) {
        curr = DesiredDescriptorPerBioseq (tmp, NULL, itemID, index, sdp, context);
        if (curr != NULL) return curr;
      }
      return NULL;
    }
  }

  if (bsp == NULL) return NULL;

  while ((curr = SeqMgrGetNextDescriptor (bsp, curr, 0, context)) != NULL) {
    if (itemID > 0 && itemID == context->itemID) return curr;
    if (index > 0 && index == context->index) return curr;
    if (sdp != NULL && sdp == curr) return curr;
  }

  return NULL;
}

NLM_EXTERN ValNodePtr LIBCALL SeqMgrGetDesiredDescriptor (Uint2 entityID, BioseqPtr bsp,
                                                          Uint2 itemID, Uint2 index, ValNodePtr sdp,
                                                          SeqMgrDescContext PNTR context)

{
  SeqMgrDescContext  dfaultcontext;
  SeqEntryPtr        sep;

  if (context == NULL) {
    context = &dfaultcontext;
  }
  if (context != NULL) {
    MemSet ((Pointer) context, 0, sizeof (SeqMgrDescContext));
  }

  if (entityID > 0) {
    sep = SeqMgrGetTopSeqEntryForEntity (entityID);
    if (sep == NULL) return NULL;
    return DesiredDescriptorPerBioseq (sep, NULL, itemID, index, sdp, context);
  } else if (bsp != NULL) {
    return DesiredDescriptorPerBioseq (NULL, bsp, itemID, index, sdp, context);
  }

  return NULL;
}
*/

static SMDescItemPtr SeqMgrFindSMDescItemByID (BioseqExtraPtr bspextra, Uint4 itemID)

{
  SMDescItemPtr PNTR  array;
  SMDescItemPtr       item;
  Int4                L;
  Int4                mid;
  Int4                R;

  if (bspextra == NULL) return NULL;

  array = bspextra->descrsByID;
  if (array != NULL && bspextra->numdescs > 0) {
    L = 0;
    R = bspextra->numdescs - 1;
    while (L < R) {
      mid = (L + R) / 2;
      item = array [mid];
      if (item != NULL && item->itemID < itemID) {
        L = mid + 1;
      } else {
        R = mid;
      }
    }

    item = array [R];
    if (item->itemID == itemID) return item;
  }

  return NULL;
}

static SMDescItemPtr SeqMgrFindSMDescItemBySdp (BioseqExtraPtr bspextra, SeqDescrPtr sdp)

{
  SMDescItemPtr PNTR  array;
  SMDescItemPtr       item;
  Int4                L;
  Int4                mid;
  Int4                R;

  if (bspextra == NULL) return NULL;

  array = bspextra->descrsBySdp;
  if (array != NULL && bspextra->numdescs > 0) {
    L = 0;
    R = bspextra->numdescs - 1;
    while (L < R) {
      mid = (L + R) / 2;
      item = array [mid];
      if (item != NULL && item->sdp < sdp) {
        L = mid + 1;
      } else {
        R = mid;
      }
    }

    item = array [R];
    if (item->sdp == sdp) return item;
  }

  return NULL;
}

static SMDescItemPtr SeqMgrFindSMDescItemByIndex (BioseqExtraPtr bspextra, Uint4 index)

{
  SMDescItemPtr PNTR  array;
  SMDescItemPtr       item;
  Int4                L;
  Int4                mid;
  Int4                R;

  if (bspextra == NULL) return NULL;

  array = bspextra->descrsByIndex;
  if (array != NULL && bspextra->numdescs > 0) {
    L = 0;
    R = bspextra->numdescs - 1;
    while (L < R) {
      mid = (L + R) / 2;
      item = array [mid];
      if (item != NULL && item->index < index) {
        L = mid + 1;
      } else {
        R = mid;
      }
    }

    item = array [R];
    if (item->index == index) return item;
  }

  return NULL;
}

NLM_EXTERN ValNodePtr LIBCALL SeqMgrGetDesiredDescriptor (Uint2 entityID, BioseqPtr bsp,
                                                          Uint4 itemID, Uint4 index, ValNodePtr sdp,
                                                          SeqMgrDescContext PNTR context)

{
  BioseqExtraPtr     bspextra;
  SeqMgrDescContext  dfaultcontext;
  ObjMgrDataPtr      omdp = NULL;
  SMDescItemPtr      sdip = NULL;
  SeqEntryPtr        sep;

  if (context == NULL) {
    context = &dfaultcontext;
  }
  if (context != NULL) {
    MemSet ((Pointer) context, 0, sizeof (SeqMgrDescContext));
  }

  if (entityID > 0) {
    sep = SeqMgrGetTopSeqEntryForEntity (entityID);
    if (sep != NULL) {
      omdp = SeqMgrGetOmdpForPointer (sep->data.ptrvalue);
    }
  } else if (bsp != NULL) {
    omdp = SeqMgrGetOmdpForBioseq (bsp);
    entityID = bsp->idx.entityID;
    if (entityID < 1) {
      entityID = ObjMgrGetEntityIDForPointer (bsp);
    }
  }

  if (omdp == NULL) return NULL;
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return NULL;

  if (itemID > 0) {
    sdip = SeqMgrFindSMDescItemByID (bspextra, itemID);
  } else if (index > 0) {
    sdip = SeqMgrFindSMDescItemByIndex (bspextra, index);
  } else if (sdp != NULL) {
    sdip = SeqMgrFindSMDescItemBySdp (bspextra, sdp);
  }

  if (sdip == NULL) return NULL;

  context->entityID = entityID;
  context->itemID = sdip->itemID;
  context->sdp = sdip->sdp;
  context->sep = sdip->sep;
  context->level = sdip->level;
  context->seqdesctype = sdip->seqdesctype;
  context->userdata = NULL;
  context->omdp = omdp;
  context->index = sdip->index;

  return sdip->sdp;
}

NLM_EXTERN AnnotDescPtr LIBCALL SeqMgrGetDesiredAnnotDesc (
  Uint2 entityID,
  BioseqPtr bsp,
  Uint4 itemID,
  SeqMgrAndContext PNTR context
)

{
  AnnotDescPtr      adp = NULL;
  BioseqExtraPtr    bspextra;
  SeqMgrAndContext  dfaultcontext;
  ObjMgrDataPtr     omdp = NULL;
  SeqEntryPtr       sep;

  if (context == NULL) {
    context = &dfaultcontext;
  }
  if (context != NULL) {
    MemSet ((Pointer) context, 0, sizeof (SeqMgrAndContext));
  }

  if (entityID > 0) {
    sep = SeqMgrGetTopSeqEntryForEntity (entityID);
    if (sep != NULL) {
      omdp = SeqMgrGetOmdpForPointer (sep->data.ptrvalue);
    }
  } else if (bsp != NULL) {
    omdp = SeqMgrGetOmdpForBioseq (bsp);
    entityID = bsp->idx.entityID;
    if (entityID < 1) {
      entityID = ObjMgrGetEntityIDForPointer (bsp);
    }
  }

  if (omdp == NULL) return NULL;
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return NULL;

  if (itemID > 0) {
    adp = SeqMgrFindAnnotDescByID (entityID, itemID);
  }

  if (adp == NULL) return NULL;

  context->entityID = entityID;
  context->itemID = itemID;
  context->adp = adp;
  context->annotdesctype = adp->choice;
  context->userdata = NULL;
  context->omdp = omdp;
  context->index = itemID;

  return adp;
}

/*****************************************************************************
*
*   RecordFeaturesInBioseqs callback explores bioseqs, bioseq sets, and features,
*     keeping a running total of the descriptor item counts, and records specific
*     information about features on each bioseq
*
*****************************************************************************/

typedef struct extraindex {
  SeqEntryPtr     topsep;
  BioseqPtr       lastbsp;
  SeqAnnotPtr     lastsap;
  BioseqSetPtr    lastbssp;
  ValNodePtr      alignhead;
  ValNodePtr      lastalign;
  ValNodePtr      adphead;
  ValNodePtr      lastadp;
  SMSeqIdxPtr     segpartail;
  Int4            cumulative;
  Uint4           bspcount;
  Uint4           aligncount;
  Uint4           descrcount;
  Uint4           featcount;
  Uint4           adpcount;
  Int4            seqlitid;
  Boolean         flip;
} ExtraIndex, PNTR ExtraIndexPtr;

static void SetDescriptorCounts (ValNodePtr sdp, ExtraIndexPtr exindx, Pointer thisitem, Uint2 thistype)

{
  Uint4          count = 0;
  ObjMgrDataPtr  omdp;

  /* count bioseq or bioseq set descriptors, to calculate omdp.lastDescrItemID */

  if (sdp == NULL || exindx == NULL) return;
  if (thistype == OBJ_BIOSEQ) {
    omdp = SeqMgrGetOmdpForBioseq ((BioseqPtr) thisitem);
  } else {
    omdp = SeqMgrGetOmdpForPointer (thisitem);
  }
  if (omdp == NULL) return;

  omdp->lastDescrItemID = exindx->descrcount;
  while (sdp != NULL) {
    count++;
    sdp = sdp->next;
  }
  exindx->descrcount += count;
}

static void CreateBioseqExtraBlock (ObjMgrDataPtr omdp, BioseqPtr bsp)

{
  BioseqExtraPtr  bspextra;

  if (omdp == NULL || omdp->extradata != NULL) return;

  bspextra = (BioseqExtraPtr) MemNew (sizeof (BioseqExtra));
  omdp->extradata = (Pointer) bspextra;
  if (bspextra == NULL) return;

  omdp->reapextra = SeqMgrReapBioseqExtraFunc;
  omdp->reloadextra = SeqMgrReloadBioseqExtraFunc;
  omdp->freeextra = SeqMgrFreeBioseqExtraFunc;

  bspextra->bsp = bsp;
  bspextra->omdp = omdp;
  bspextra->min = INT4_MAX;
  bspextra->processed = UINT1_MAX;
}

static Boolean CountAlignmentsProc (GatherObjectPtr gop)

{
  ExtraIndexPtr  exindx;

  if (gop == NULL || gop->itemtype != OBJ_SEQALIGN) return TRUE;
  exindx = (ExtraIndexPtr) gop->userdata;
  if (exindx == NULL) return TRUE;
  (exindx->aligncount)++;
  return TRUE;
}

static Boolean CollectAlignsProc (GatherObjectPtr gop)

{
  SeqAlignPtr PNTR  alignsByID;

  if (gop == NULL || gop->itemtype != OBJ_SEQALIGN) return TRUE;
  alignsByID = (SeqAlignPtr PNTR) gop->userdata;
  if (alignsByID == NULL) return TRUE;
  alignsByID [gop->itemID] = (SeqAlignPtr) gop->dataptr;
  return TRUE;
}

NLM_EXTERN void LIBCALL SeqMgrIndexAlignments (Uint2 entityID)

{
  SeqAlignPtr PNTR  alignsByID;
  BioseqExtraPtr    bspextra;
  ExtraIndex        exind;
  Boolean           objMgrFilter [OBJ_MAX];
  ObjMgrDataPtr     omdp;

  if (entityID == 0) return;

  /* count alignments */

  exind.topsep = NULL;
  exind.lastbsp = NULL;
  exind.lastsap = NULL;
  exind.lastbssp = NULL;
  exind.alignhead = NULL;
  exind.lastalign = NULL;
  exind.adphead = NULL;
  exind.lastadp = NULL;
  exind.segpartail = NULL;
  exind.bspcount = 0;
  exind.aligncount = 0;
  exind.descrcount = 0;
  exind.featcount = 0;
  exind.adpcount = 0;
  exind.seqlitid = 0;

  MemSet ((Pointer) objMgrFilter, 0, sizeof (objMgrFilter));
  objMgrFilter [OBJ_SEQALIGN] = TRUE;
  GatherObjectsInEntity (entityID, 0, NULL, CountAlignmentsProc, (Pointer) &exind, objMgrFilter);

  omdp = ObjMgrGetData (entityID);
  if (omdp != NULL) {

    CreateBioseqExtraBlock (omdp, NULL);
    bspextra = (BioseqExtraPtr) omdp->extradata;
    if (bspextra != NULL) {

      /* get rid of previous lookup array */

      bspextra->alignsByID = MemFree (bspextra->alignsByID);
      bspextra->numaligns = 0;

      /* alignment ID to SeqAlignPtr index always goes on top of entity */

      if (exind.aligncount > 0) {
        alignsByID = (SeqAlignPtr PNTR) MemNew (sizeof (SeqAlignPtr) * (exind.aligncount + 2));
        if (alignsByID != NULL) {

          /* copy SeqAlignPtr for each itemID */

          GatherObjectsInEntity (entityID, 0, NULL, CollectAlignsProc, (Pointer) alignsByID, objMgrFilter);

          bspextra->alignsByID = alignsByID;
          bspextra->numaligns = exind.aligncount;
        }
      }
    }
  }
}

static SeqIdPtr SeqIdWithinBioseq (BioseqPtr bsp, SeqLocPtr slp)

{
  SeqIdPtr  a;
  SeqIdPtr  b;

  if (bsp == NULL || slp == NULL) return NULL;
  a = SeqLocId (slp);
  if (a == NULL) return NULL;
  for (b = bsp->id; b != NULL; b = b->next) {
    if (SeqIdComp (a, b) == SIC_YES) return b;
  }
  return NULL;
}

/*
static void FindGPS (BioseqSetPtr bssp, Pointer userdata)

{
  BoolPtr  is_gpsP;

  if (bssp == NULL || bssp->_class != BioseqseqSet_class_gen_prod_set) return;
  is_gpsP = (BoolPtr) userdata;
  *is_gpsP = TRUE;
}
*/

static void ProcessFeatureProducts (SeqFeatPtr sfp, Uint4 itemID, GatherObjectPtr gop)

{
  BioseqPtr         bsp;
  BioseqExtraPtr    bspextra;
  BioseqSetPtr      bssp;
  Char              buf [81];
  CharPtr           ctmp;
  Int4              diff;
  GatherContext     gc;
  GatherContextPtr  gcp;
  Boolean           is_gps;
  CharPtr           loclbl;
  Int4              min;
  ObjMgrDataPtr     omdp;
  Uint1             processed;
  CharPtr           prodlbl;
  ProtRefPtr        prp;
  SeqFeatPtr        prt;
  CharPtr           ptmp;
  SeqAnnotPtr       sap;
  SeqIdPtr          sip;
  SeqLocPtr         slp;
  ValNode           vn;

  if (sfp == NULL || sfp->product == NULL) return;
  if (sfp->data.choice != SEQFEAT_CDREGION &&
      sfp->data.choice != SEQFEAT_RNA &&
      sfp->data.choice != SEQFEAT_PROT) return;

  sip = SeqLocId (sfp->product);
  if (sip == NULL) return;
  bsp = BioseqFind (sip);
  if (bsp == NULL) return;
  omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return;

  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) {
    CreateBioseqExtraBlock (omdp, bsp);
    bspextra = (BioseqExtraPtr) omdp->extradata;
  }
  if (bspextra == NULL) return;

  /* cds or rna reference stored in product bioseq's omdp.cdsOrRnaFeat */

  if (bspextra->cdsOrRnaFeat != NULL && bspextra->cdsOrRnaFeat != sfp) {
    FeatDefLabel (sfp, buf, sizeof (buf) - 1, OM_LABEL_CONTENT);
    ctmp = SeqLocPrint (sfp->location);
    loclbl = ctmp;
    if (loclbl == NULL) {
      loclbl = "?";
    }
    ptmp = SeqLocPrint (sfp->product);
    prodlbl = ptmp;
    if (prodlbl == NULL) {
      prodlbl = "?";
    }
    /*
    {
      GatherContext     gc;
      GatherContextPtr  gcp;
      Boolean           is_gps = FALSE;
      SeqEntryPtr       sep;
      MemSet ((Pointer) &gc, 0, sizeof (GatherContext));
      gcp = &gc;
      gc.entityID = gop->entityID;
      gc.itemID = gop->itemID;
      gc.thistype = gop->itemtype;
      sep = GetTopSeqEntryForEntityID (gop->entityID);
      VisitSetsInSep (sep, (Pointer) &is_gps, FindGPS);
      if (! is_gps) {
        ErrPostItem (SEV_WARNING, 0, 0,
                     "SeqMgr indexing cds or rna progenitor already set - Feature: %s - Location [%s] - Product [%s]",
                     buf, loclbl, prodlbl);
      }
    }
    */
    is_gps = FALSE;
    if (bsp->idx.parenttype == OBJ_BIOSEQSET) {
      bssp = (BioseqSetPtr) bsp->idx.parentptr;
      while (bssp != NULL) {
        if (bssp->_class == BioseqseqSet_class_gen_prod_set) {
          is_gps = TRUE;
        }
        if (bssp->idx.parenttype == OBJ_BIOSEQSET) {
          bssp = (BioseqSetPtr) bssp->idx.parentptr;
        } else {
          bssp = NULL;
        }
      }
    }
    if (! is_gps) {
      MemSet ((Pointer) &gc, 0, sizeof (GatherContext));
      gcp = &gc;
      gc.entityID = gop->entityID;
      gc.itemID = gop->itemID;
      gc.thistype = gop->itemtype;
      ErrPostItem (SEV_WARNING, 0, 0,
                   "SeqMgr indexing cds or rna progenitor already set - Feature: %s - Location [%s] - Product [%s]",
                   buf, loclbl, prodlbl);
    }
    MemFree (ctmp);
    MemFree (ptmp);
  }

  /* if (omdp->tempload == TL_NOT_TEMP) { */
    bspextra->cdsOrRnaFeat = sfp;
  /* } */

  /* add to prodlisthead list for gather by get_feats_product */

  ValNodeAddPointer (&(bspextra->prodlisthead), 0, (Pointer) sfp);

  if (sfp->data.choice == SEQFEAT_RNA || sfp->data.choice == SEQFEAT_PROT) return;

  /* if protFeat exists it was set by exhaustive gather on protein bioseq */

  if (bspextra->protFeat != NULL) return;

  /* calculate largest protein feature on cds's product bioseq */

  min = INT4_MAX;
  processed = UINT1_MAX;
  vn.choice = SEQLOC_WHOLE;
  vn.data.ptrvalue = (Pointer) bsp->id;
  vn.next = NULL;
  slp = (Pointer) (&vn);

  sap = bsp->annot;
  while (sap != NULL) {
    if (sap->type == 1) {
      prt = (SeqFeatPtr) sap->data;
      while (prt != NULL) {
        if (prt->data.choice == SEQFEAT_PROT) {
          prp = (ProtRefPtr) prt->data.value.ptrvalue;

          /* get SeqId in bioseq that matches SeqId used for location */

          vn.data.ptrvalue = SeqIdWithinBioseq (bsp, prt->location);

          diff = SeqLocAinB (prt->location, slp);
          if (diff >= 0 && prp != NULL) {
            if (diff < min) {
              min = diff;
              processed = prp->processed;
              /* if (omdp->tempload == TL_NOT_TEMP) { */
                bspextra->protFeat = prt;
              /* } */
            } else if (diff == min) {
              /* unprocessed 0 preferred over preprotein 1 preferred over mat peptide 2 */
              if ( /* prp != NULL && prp->processed == 0 */ prp->processed < processed ) {
                min = diff;
                processed = prp->processed;
                bspextra->protFeat = prt;
              }
            }
          }
        }
        prt = prt->next;
      }
    }
    sap = sap->next;
  }
}


static Boolean SimpleIvalsCalculation (SeqLocPtr slp, BioseqPtr bsp, Boolean flip, SMFeatItemPtr item)
{
  SeqIntPtr sint;

  if (!flip && slp != NULL && bsp != NULL && item != NULL && slp->choice == SEQLOC_INT
      && (sint = (SeqIntPtr) slp->data.ptrvalue) != NULL
      && SeqIdIn (sint->id, bsp->id)) {
    item->strand = sint->strand;
    item->numivals = 1;
    item->ivals = MemNew (sizeof (Int4) * 2);
    if (item->strand == Seq_strand_minus) {
      item->ivals[0] = sint->to;
      item->ivals[1] = sint->from;
    } else {
      item->ivals[0] = sint->from;
      item->ivals[1] = sint->to;
    }
    return TRUE;
  } else {
    return FALSE;
  }
}

static void RecordOneFeature (BioseqExtraPtr bspextra, ObjMgrDataPtr omdp,
                              BioseqPtr bsp, ExtraIndexPtr exindx, SeqFeatPtr sfp,
                              Int4 left, Int4 right, Uint4 itemID, Uint2 subtype,
                              Boolean farloc, Boolean bad_order, Boolean mixed_strand,
                              Boolean ignore, Boolean ts_image)

{
  Char            buf [129];
  SMFeatBlockPtr  curr;
  Int4            from;
  Int2            i;
  SMFeatItemPtr   item;
  Int4Ptr         ivals;
  SeqLocPtr       loc;
  SMFeatBlockPtr  next;
  Int2            numivals = 0;
  CharPtr         ptr;
  SeqIdPtr        sip;
  SeqLocPtr       slp = NULL;
  Uint1           strand;
  Int4            swap;
  Int4            to;

  if (bspextra == NULL || omdp == NULL || bsp == NULL || exindx == NULL || sfp == NULL) return;

  if (bspextra->featlisttail != NULL) {

    /* just in case blocksize should was not set for some reason */

    if (bspextra->blocksize < 1) {
      bspextra->blocksize = 5;
    }

    curr = bspextra->featlisttail;
    if (curr->index >= bspextra->blocksize) {

      /* allocate next chunk in linked list of blocks */

      next = (SMFeatBlockPtr) MemNew (sizeof (SMFeatBlock));
      curr->next = next;

      if (next != NULL) {
        bspextra->featlisttail = next;
        curr = next;
      }
    }

    if (curr->index < bspextra->blocksize) {

      /* allocate data block if not yet done for this chunk */

      if (curr->data == NULL) {
        curr->data = (SMFeatItemPtr) MemNew (sizeof (SMFeatItem) * (size_t) (bspextra->blocksize));
      }

      /* now record desired information about current feature */

      if (curr->data != NULL) {
        item = &(curr->data [curr->index]);
        /* if (omdp->tempload == TL_NOT_TEMP) { */
          item->sfp = sfp;
          item->sap = exindx->lastsap;
          item->bsp = bsp;
        /* } */
        FeatDefLabel (sfp, buf, sizeof (buf) - 1, OM_LABEL_CONTENT);
        ptr = buf;
        if (sfp->data.choice == SEQFEAT_RNA) {
          ptr = StringStr (buf, "RNA-");
          if (ptr != NULL) {
            ptr += 4;
          } else {
            ptr = buf;
          }
        }
        item->label = StringSaveNoNull (ptr);
        item->left = left;
        item->right = right;
        if (exindx->flip) {
          item->left = bsp->length - right;
          item->right = bsp->length - left;
        }
        item->dnaStop = -1;
        CheckSeqLocForPartial (sfp->location, &(item->partialL), &(item->partialR));
        item->farloc = farloc;
        item->bad_order = bad_order;
        item->mixed_strand = mixed_strand;
        item->ts_image = ts_image;
        /*
        item->strand = SeqLocStrand (sfp->location);
        if (exindx->flip) {
          item->strand = StrandCmp (item->strand);
        }
        */
        if (subtype == 0) {
          subtype = FindFeatDefType (sfp);
        }
        item->subtype = subtype;
        item->itemID = itemID;
        item->ignore = ignore;
        item->overlap = -1;

        /* record start/stop pairs of intervals on target bioseq */

        /*
        single_interval = (Boolean) (item->subtype == FEATDEF_GENE ||
                                     item->subtype == FEATDEF_PUB);
        */

        if (SimpleIvalsCalculation (sfp->location, bsp, exindx->flip, item)) {
          /* don't need to do complex merging to calculate intervals */
        } else {
          loc = SeqLocMergeExEx (bsp, sfp->location, NULL, FALSE, FALSE, FALSE, FALSE, TRUE, TRUE);

          if (exindx->flip) {
            sip = SeqIdFindBest (bsp->id, 0);
            slp = SeqLocCopyRegion (sip, loc, bsp, 0, bsp->length - 1, Seq_strand_minus, FALSE);
            SeqLocFree (loc);
            loc = slp;
          }
          /* record strand relative to segmented parent */
          item->strand = SeqLocStrand (loc);
          if (exindx->flip) {
            item->strand = StrandCmp (item->strand);
          }
          strand = item->strand;

          slp = NULL;
          while ((slp = SeqLocFindNext (loc, slp)) != NULL) {
            numivals++;
          }
          if (numivals > 0) {
            ivals = MemNew (sizeof (Int4) * (numivals * 2));
            item->ivals = ivals;
            item->numivals = numivals;
            slp = NULL;
            i = 0;
            while ((slp = SeqLocFindNext (loc, slp)) != NULL) {
              from = SeqLocStart (slp);
              to = SeqLocStop (slp);
              if (strand == Seq_strand_minus) {
                swap = from;
                from = to;
                to = swap;
              }
              ivals [i] = from;
              i++;
              ivals [i] = to;
              i++;
            }
          }
          SeqLocFree (loc);
        }
      }

      /* increment count on current block */

      (curr->index)++;

      /* count all features, per bioseq and per entity */

      (bspextra->numfeats)++;
      (exindx->featcount)++;

    }
  }
}


static void CheckForTransSplice (
  SeqFeatPtr sfp,
  BoolPtr bad_orderP,
  BoolPtr mixed_strandP,
  Boolean circular
)

{
  Boolean    mixed_strand = FALSE, ordered = TRUE;
  SeqIdPtr   id1, id2;
  SeqLocPtr  prev, tmp;
  SeqIntPtr  sip1, sip2, prevsip;
  Uint1      strand1, strand2;

  if (sfp == NULL || sfp->location == NULL) return;

  tmp = NULL;
  prev = NULL;
  sip1 = NULL;
  id1 = NULL;
  prevsip = NULL;
  strand1 = Seq_strand_other;

  while ((tmp = SeqLocFindNext (sfp->location, tmp)) != NULL) {

    /* just check seqloc_interval */

    if (tmp->choice == SEQLOC_INT) {
      sip1 = prevsip;
      sip2 = (SeqIntPtr) (tmp->data.ptrvalue);
      strand2 = sip2->strand;
      id2 = sip2->id;
      if ((sip1 != NULL) && (ordered) && (! circular)) {
        if (SeqIdForSameBioseq (sip1->id, sip2->id)) {
          if (strand2 == Seq_strand_minus) {
            if (sip1->to < sip2->to) {
              ordered = FALSE;
            }
          } else {
            if (sip1->to > sip2->to) {
              ordered = FALSE;
            }
          }
        }
      }
      prevsip = sip2;
      if ((strand1 != Seq_strand_other) && (strand2 != Seq_strand_other)) {
        if (SeqIdForSameBioseq (id1, id2)) {
          if (strand1 != strand2) {
            if (strand1 == Seq_strand_plus && strand2 == Seq_strand_unknown) {
              /* unmarked_strand = TRUE; */
            } else if (strand1 == Seq_strand_unknown && strand2 == Seq_strand_plus) {
              /* unmarked_strand = TRUE; */
            } else {
              mixed_strand = TRUE;
            }
          }
        }
      }

      strand1 = strand2;
      id1 = id2;
    }
  }

  /* Publication intervals ordering does not matter */

  if (sfp->idx.subtype == FEATDEF_PUB) {
    ordered = TRUE;
  }

  /* ignore ordering of heterogen bonds */

  if (sfp->data.choice == SEQFEAT_HET) {
    ordered = TRUE;
  }

  /* misc_recomb intervals SHOULD be in reverse order */

  if (sfp->idx.subtype == FEATDEF_misc_recomb) {
    ordered = TRUE;
  }

    /* primer_bind intervals MAY be in on opposite strands */

  if (sfp->idx.subtype == FEATDEF_primer_bind) {
    mixed_strand = FALSE;
    ordered = TRUE;
  }

  if (! ordered) {
    *bad_orderP = TRUE;
  }
  if (mixed_strand) {
    *mixed_strandP = TRUE;
  }
}


static Boolean RecordFeatureOnBioseq (
  GatherObjectPtr gop,
  BioseqPtr bsp,
  SeqFeatPtr sfp,
  ExtraIndexPtr exindx,
  Boolean usingLocalBsp,
  Boolean special_case,
  Boolean small_gen_set,
  Boolean ts_image
)

{
  Boolean         bad_order;
  BioseqExtraPtr  bspextra;
  Char            buf [81];
  Int2            count;
  CharPtr         ctmp;
  Int4            diff;
  Int4            left;
  CharPtr         loclbl;
  Boolean         mixed_strand;
  ObjMgrDataPtr   omdp;
  ProtRefPtr      prp;
  Int4            right;
  SeqAnnotPtr     sap;
  SeqLocPtr       slp;
  Int4            swap;
  SeqFeatPtr      tmp;
  ValNode         vn;

  if (gop == NULL || bsp == NULL || sfp == NULL || exindx == NULL) return FALSE;

  omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp == NULL) return TRUE;

  /* now prepare for adding feature to index */

  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) {
    CreateBioseqExtraBlock (omdp, bsp);
    bspextra = (BioseqExtraPtr) omdp->extradata;
  }
  if (bspextra == NULL) return TRUE;

  /* get extreme left and right extents of feature location on near bioseqs */
  /* merge here to get correct extremes even in case of trans-splicing */

  /* but this broke the handling of genes spanning the origin, so cannot do */
  /*
  slp = SeqLocMergeEx (bsp, sfp->location, NULL, TRUE, TRUE, FALSE, FALSE);
  */
  if (special_case) {
    slp = sfp->product;
  } else {
    slp = sfp->location;
  }

  bad_order = FALSE;
  mixed_strand = FALSE;
  CheckForTransSplice (sfp, &bad_order, &mixed_strand, /* (Boolean) (bsp->topology == TOPOLOGY_CIRCULAR) */ FALSE);

#if 1
  GetLeftAndRightOffsetsInNearBioseq (slp, bsp, &left, &right, small_gen_set, bad_order, mixed_strand);
#else
  left = GetOffsetInNearBioseq (slp, bsp, SEQLOC_LEFT_END);
  right = GetOffsetInNearBioseq (slp, bsp, SEQLOC_RIGHT_END);
#endif
  /*
  SeqLocFree (slp);
  */
  if (left == -1 || right == -1) {
    GatherContext     gc;
    GatherContextPtr  gcp;
    Char              lastbspid [41];
    SeqIdPtr          sip;
    MemSet ((Pointer) &gc, 0, sizeof (GatherContext));
    gcp = &gc;
    gc.entityID = gop->entityID;
    gc.itemID = gop->itemID;
    gc.thistype = gop->itemtype;
    lastbspid [0] = '\0';
    if (exindx->lastbsp != NULL) {
      sip = SeqIdFindBest (exindx->lastbsp->id, 0);
      if (sip != NULL) {
        SeqIdWrite (sip, lastbspid, PRINTID_FASTA_LONG, sizeof (lastbspid));
      }
    }
    FeatDefLabel (sfp, buf, sizeof (buf) - 1, OM_LABEL_CONTENT);
    ctmp = SeqLocPrint (sfp->location);
    loclbl = ctmp;
    if (loclbl == NULL) {
      loclbl = "?";
    }
    ErrPostItem (SEV_WARNING, 0, 0,
                 "SeqMgr indexing feature mapping problem - Feature: %s - Location [%s] - Record [%s]",
                 buf, loclbl, lastbspid);
    MemFree (ctmp);
    return TRUE;
  }

  /* if indexing protein bioseq, store largest protein feature */

  if (sfp->data.choice == SEQFEAT_PROT) {
    prp = (ProtRefPtr) sfp->data.value.ptrvalue;

    vn.choice = SEQLOC_WHOLE;
    vn.data.ptrvalue = (Pointer) bsp->id;
    vn.next = NULL;
    slp = (Pointer) &vn;

    /* get SeqId in bioseq that matches SeqId used for location */

    vn.data.ptrvalue = (Pointer) SeqIdWithinBioseq (bsp, sfp->location);

    diff = SeqLocAinB (sfp->location, slp);
    if (diff >= 0 && prp != NULL) {
      if (diff < bspextra->min) {
        bspextra->min = diff;
        bspextra->processed = prp->processed;
        /* if (omdp->tempload == TL_NOT_TEMP) { */
          bspextra->protFeat = sfp;
        /* } */
      } else if (diff == bspextra->min) {
        /* unprocessed 0 preferred over preprotein 1 preferred over mat peptide 2 */
        if ( /* prp != NULL && prp->processed == 0 */ prp->processed < bspextra->processed ) {
          bspextra->min = diff;
          bspextra->processed = prp->processed;
          bspextra->protFeat = sfp;
        }
      }
    }
  }

  /* add feature item to linked list of blocks */

  if (bspextra->featlisthead == NULL) {
    bspextra->featlisthead = (SMFeatBlockPtr) MemNew (sizeof (SMFeatBlock));

    /* for first feature indexed on this bioseq, quickly see if few or many
       additional features, since most features on a bioseq are packaged in
       the same list, and most proteins only have one bioseq */

    for (tmp = sfp, count = 0;
         tmp != NULL && count < 50;
         tmp = tmp->next, count++) continue;

    /* extend count if above features were packaged on a bioseq set (presumably CDS or mRNA) */

    if (exindx->lastbssp != NULL) {
      for (sap = bsp->annot; sap != NULL; sap = sap->next) {
        if (sap->type == 1) {

          for (tmp = (SeqFeatPtr) sap->data;
               tmp != NULL && count < 50;
               tmp = tmp->next, count++) continue;

        }
      }
    }

    bspextra->blocksize = count;
  }
  if (bspextra->featlisttail == NULL) {
    bspextra->featlisttail = bspextra->featlisthead;
  }

  if (bspextra->featlisttail != NULL) {

    /* if feature spans origin, record with left < 0 */

    if (left > right && bsp->topology == TOPOLOGY_CIRCULAR) {
      left -= bsp->length;
    }

    /* some trans-spliced locations can confound GetOffsetInNearBioseq, so normalize here */

    if (left > right) {
      swap = left;
      left = right;
      right = swap;
    }

    RecordOneFeature (bspextra, omdp, bsp, exindx, sfp, left,
                      right, gop->itemID, gop->subtype, usingLocalBsp,
                      bad_order, mixed_strand, special_case, ts_image);

    /* record gene, publication, and biosource features twice if spanning the origin */

    if (left < 0 && bsp->topology == TOPOLOGY_CIRCULAR) {
      if (sfp->data.choice == SEQFEAT_GENE ||
          sfp->data.choice == SEQFEAT_PUB ||
          sfp->data.choice == SEQFEAT_BIOSRC ||
          sfp->idx.subtype == FEATDEF_operon) {

        RecordOneFeature (bspextra, omdp, bsp, exindx, sfp, left + bsp->length,
                          right + bsp->length, gop->itemID, gop->subtype, usingLocalBsp,
                          bad_order, mixed_strand, TRUE, ts_image);

      }
    }
  }

  return TRUE;
}

typedef struct adpbspdata {
  AnnotDescPtr  adp;
  BioseqPtr     bsp;
} AdpBspData, PNTR AdpBspPtr;

/* callback for recording features and descriptor, prot, and cdsOrRna information */

static Boolean RecordFeaturesInBioseqs (GatherObjectPtr gop)

{
  AdpBspPtr       abp;
  AnnotDescPtr    adp = NULL;
  BioseqPtr       bsp = NULL;
  BioseqExtraPtr  bspextra;
  BioseqSetPtr    bssp = NULL;
  Char            buf [81];
  CharPtr         ctmp;
  ExtraIndexPtr   exindx;
  ValNodePtr      head = NULL;
  BioseqPtr       lbsp;
  CharPtr         loclbl;
  ObjMgrDataPtr   omdp;
  SeqAnnotPtr     sap = NULL;
  ValNodePtr      sdp = NULL;
  SeqFeatPtr      sfp = NULL;
  SeqAlignPtr     sal = NULL;
  SeqIdPtr        sip;
  SeqLocPtr       slp;
  Boolean         small_gen_set = FALSE;
  Boolean         special_case = FALSE;
  ValNodePtr      tail = NULL;
  Boolean         usingLocalBsp = FALSE;
  ValNodePtr      vnp;

  switch (gop->itemtype) {
    case OBJ_BIOSEQ :
      bsp = (BioseqPtr) gop->dataptr;
      if (bsp == NULL) return TRUE;
      sdp = bsp->descr;
      break;
    case OBJ_BIOSEQSET :
      bssp = (BioseqSetPtr) gop->dataptr;
      if (bssp == NULL) return TRUE;
      sdp = bssp->descr;
      break;
    case OBJ_SEQANNOT :
      sap = (SeqAnnotPtr) gop->dataptr;
      break;
    case OBJ_ANNOTDESC :
      adp = (AnnotDescPtr) gop->dataptr;
      break;
    case OBJ_SEQFEAT :
      sfp = (SeqFeatPtr) gop->dataptr;
      break;
    case OBJ_SEQALIGN :
      sal = (SeqAlignPtr) gop->dataptr;
      break;
    default :
      return TRUE;
  }

  exindx = (ExtraIndexPtr) gop->userdata;
  if (exindx == NULL) return FALSE;

  /* save bspItemID to support bioseq explore functions */

  if (bsp != NULL) {

    (exindx->bspcount)++;

    /* save last BioseqPtr to check first for appropriate bioseq */

    exindx->lastbsp = bsp;

    /* blocksize for new block based only on features packaged on bioseq */

    exindx->lastbssp = NULL;

    omdp = SeqMgrGetOmdpForBioseq (bsp);
    if (omdp != NULL) {
      bspextra = (BioseqExtraPtr) omdp->extradata;
      if (bspextra == NULL) {
        CreateBioseqExtraBlock (omdp, bsp);
        bspextra = (BioseqExtraPtr) omdp->extradata;
      }
      if (bspextra != NULL) {
        bspextra->bspItemID = gop->itemID;
        bspextra->bspIndex = exindx->bspcount;
      }
    }
  }

  /* save last BioseqSetPtr to calculate blocksize from bioseq set and bioseq features,
     features on bioseq set presumably being CDS or mRNA and applying only to nucleotides */

  if (bssp != NULL) {
    exindx->lastbssp = bssp;
  }

  /* count bioseq or bioseq set descriptors, to calculate lastDescrItemID */

  if (sdp != NULL) {
    SetDescriptorCounts (sdp, exindx, gop->dataptr, gop->itemtype);
    return TRUE;
  }

  /* save SeqAnnotPtr containing next features to be gathered */

  if (sap != NULL) {
    exindx->lastsap = sap;
    return TRUE;
  }

  /* record SeqAlignPtr in val node list - expects all itemIDs to be presented */

  if (sal != NULL) {
    vnp = ValNodeAddPointer (&(exindx->lastalign), 0, (Pointer) sal);
    if (exindx->alignhead == NULL) {
      exindx->alignhead = exindx->lastalign;
    }
    exindx->lastalign = vnp;
    (exindx->aligncount)++;
    return TRUE;
  }

  /* record AnnotDescPtr and relevant BioseqPtr in val node list */

  if (adp != NULL) {
    abp = (AdpBspPtr) MemNew (sizeof (AdpBspData));
    if (abp != NULL) {
      abp->adp = adp;
      sap = exindx->lastsap;
      if (sap != NULL && sap->type == 1) {
        bsp = NULL;
        sfp = (SeqFeatPtr) sap->data;
        while (sfp != NULL && bsp == NULL) {
          slp = sfp->location;
          if (slp != NULL) {
            bsp = BioseqFindFromSeqLoc (slp);
          }
          sfp = sfp->next;
        }
        abp->bsp = bsp;
      }
      vnp = ValNodeAddPointer (&(exindx->lastadp), 0, (Pointer) abp);
      if (exindx->adphead == NULL) {
        exindx->adphead = exindx->lastadp;
      }
      exindx->lastadp = vnp;
      (exindx->adpcount)++;
    }
    return TRUE;
  }

  /* otherwise index features on every bioseq in entity */

  if (sfp == NULL) return TRUE;

  /* cds or rna reference stored in product bioseq's omdp.cdsOrRnaFeat,
     best protein feature in omdp.protFeat (do before adding CDS) */

  if (sfp->product != NULL) {
    ProcessFeatureProducts (sfp, gop->itemID, gop);
  }

  bsp = FindAppropriateBioseq (sfp->location, exindx->lastbsp, &small_gen_set);

  /* failure here can be due to SeqLoc that references far accession */

  if (bsp == NULL) {

    /* if far accession, find first local bioseq on any location interval */

    bsp = FindFirstLocalBioseq (sfp->location);

    /* report whether far accession was able to be handled */

    FeatDefLabel (sfp, buf, sizeof (buf) - 1, OM_LABEL_CONTENT);
    ctmp = SeqLocPrint (sfp->location);
    loclbl = ctmp;
    if (loclbl == NULL) {
      loclbl = "?";
    }

    if (bsp == NULL) {
      {
        GatherContext     gc;
        GatherContextPtr  gcp;
        Char              lastbspid [41];
        SeqIdPtr          sip;
        MemSet ((Pointer) &gc, 0, sizeof (GatherContext));
        gcp = &gc;
        gc.entityID = gop->entityID;
        gc.itemID = gop->itemID;
        gc.thistype = gop->itemtype;
        lastbspid [0] = '\0';
        if (exindx->lastbsp != NULL) {
          sip = SeqIdFindBest (exindx->lastbsp->id, 0);
          if (sip != NULL) {
            SeqIdWrite (sip, lastbspid, PRINTID_FASTA_LONG, sizeof (lastbspid));
          }
        }
        ErrPostItem (SEV_WARNING, 0, 0,
                     "SeqMgr indexing feature location problem - Feature: %s - Location [%s] - Record [%s]",
                     buf, loclbl, lastbspid);
      }
    } else {
      /*
      ErrPostItem (SEV_INFO, 0, 0,
                   "SeqMgr indexing detected and handled far accession - Feature: %s - Location [%s]",
                   buf, loclbl);
      */
    }
    MemFree (ctmp);

    if (bsp == NULL && sfp->product != NULL &&
        sfp->data.choice == SEQFEAT_CDREGION &&
        IS_Bioseq (exindx->topsep)) {
      bsp = (BioseqPtr) exindx->topsep->data.ptrvalue;
      if (bsp == NULL || (! ISA_aa (bsp->mol))) return TRUE;
      special_case = TRUE;
      bsp = FindAppropriateBioseq (sfp->product, exindx->lastbsp, &small_gen_set);
      if (bsp == NULL) return TRUE;
    } else {
      if (bsp == NULL) return TRUE;
      usingLocalBsp = TRUE;
    }
  }

  /* assume subsequent features will be on this bioseq */

  exindx->lastbsp = bsp;

  RecordFeatureOnBioseq (gop, bsp, sfp, exindx, usingLocalBsp, special_case, small_gen_set, FALSE);

  /* for small genome set, index mixed-chromosome features on other chromosomes as misc_features for visibility */

  if (sfp->data.choice != SEQFEAT_GENE) return TRUE;

  if (small_gen_set) {
    slp = SeqLocFindNext (sfp->location, NULL);
    while (slp != NULL) {
      sip = SeqLocId (slp);
      if (sip != NULL) {
        lbsp = BioseqFindCore (sip);
        if (lbsp != NULL) {
          if (lbsp != bsp) {
            ValNodeAddPointerEx (&head, &tail, 0, (Pointer) lbsp);
          }
        }
      }
      slp = SeqLocFindNext (sfp->location, slp);
    }
    if (head != NULL) {
      head = ValNodeSort (head, SortByPtrvalue);
      head = UniquePtrValNode (head);

      for (vnp = head; vnp != NULL; vnp = vnp->next) {
        bsp = (BioseqPtr) vnp->data.ptrvalue;
        if (bsp == NULL) continue;

        /*
        !!! need to add flag so that these features are only fetched by flatfile generator
        and with a distinct flag so that they show up as something like misc_feature instead
        of CDS !!!
        */

        exindx->lastbsp = bsp;
        RecordFeatureOnBioseq (gop, bsp, sfp, exindx, usingLocalBsp, special_case, small_gen_set, TRUE);
      }

      ValNodeFree (head);
    }
  }

  return TRUE;
}

/*****************************************************************************
*
*   RecordSegmentsInBioseqs callback explores bioseq segments
*
*****************************************************************************/

static Boolean RecordSegmentsInBioseqs (GatherObjectPtr gop)

{
  BioseqPtr       bsp = NULL;
  BioseqExtraPtr  bspextra;
  Char            buf [80];
  Dbtag           db;
  DeltaSeqPtr     dsp;
  ExtraIndexPtr   exindx;
  Int4            from;
  Boolean         isSeg = FALSE;
  ObjectId        oi;
  ObjMgrDataPtr   omdp;
  SMSeqIdxPtr     segpartptr;
  SeqId           si;
  SeqIdPtr        sid;
  SeqInt          sint;
  SeqIntPtr       sipp;
  SeqLoc          sl;
  SeqLitPtr       slitp;
  SeqLocPtr       slp = NULL;
  Uint1           strand;
  Int4            to;

  exindx = (ExtraIndexPtr) gop->userdata;
  if (exindx == NULL) return FALSE;

  switch (gop->itemtype) {
    case OBJ_BIOSEQ :
      bsp = (BioseqPtr) gop->dataptr;
      if (bsp == NULL) return TRUE;
      break;
    case OBJ_BIOSEQ_SEG :
      isSeg = TRUE;
      slp = (SeqLocPtr) gop->dataptr;
      if (slp == NULL) return TRUE;
      break;
    case OBJ_BIOSEQ_DELTA :
      dsp = (DeltaSeqPtr) gop->dataptr;
      if (dsp == NULL) return TRUE;
      if (dsp->choice == 1) {
        slp = (SeqLocPtr) dsp->data.ptrvalue;
      } else if (dsp->choice == 2) {
        slitp = (SeqLitPtr) dsp->data.ptrvalue;
        if (slitp != NULL) {
          /* fake seqloc, same as in DeltaSeqsToSeqLocs */
          MemSet ((Pointer) &sl, 0, sizeof (SeqLoc));
          MemSet ((Pointer) &sint, 0, sizeof (SeqInt));
          MemSet ((Pointer) &si, 0, sizeof (SeqId));
          MemSet ((Pointer) &db, 0, sizeof (Dbtag));
          MemSet ((Pointer) &oi, 0, sizeof (ObjectId));
          sl.choice = SEQLOC_INT;
          sl.data.ptrvalue = (Pointer) &sint;
          sint.from = 0;
          sint.to = slitp->length - 1;
          si.choice = SEQID_GENERAL;
          si.data.ptrvalue = (Pointer) &db;
          db.db = "SeqLit";
          db.tag = &oi;
          (exindx->seqlitid)++;
          oi.id = exindx->seqlitid;
          sint.id = &si;
          slp = &sl;
        }
      }
      break;
    default :
      return TRUE;
  }

  if (bsp != NULL) {
    if (bsp->repr == Seq_repr_seg) {
      exindx->lastbsp = bsp;
    } else if (bsp->repr == Seq_repr_delta) {
      exindx->lastbsp = bsp;
    } else {
      exindx->lastbsp = NULL;
    }
    exindx->cumulative = 0;
    return TRUE;
  }

  if (slp == NULL) return TRUE;

  bsp = exindx->lastbsp;
  if (bsp == NULL) return TRUE;

  omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp == NULL) return TRUE;

  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) {
    CreateBioseqExtraBlock (omdp, bsp);
    bspextra = (BioseqExtraPtr) omdp->extradata;
  }
  if (bspextra == NULL) return TRUE;

  if (slp->choice == SEQLOC_INT && slp->data.ptrvalue != NULL) {
    sipp = (SeqIntPtr) (slp->data.ptrvalue);
    from = sipp->from;
    to = sipp->to;
    strand = sipp->strand;
  } else {
    from = 0;
    to = SeqLocLen (slp) - 1;
    strand = SeqLocStrand (slp);
  }

  if (to - from + 1 < 1) return TRUE;

  /* create and fill in SMSeqIdx element */

  segpartptr = MemNew (sizeof (SMSeqIdx));
  if (segpartptr != NULL) {
    sid = SeqLocId (slp);
    if (MakeReversedSeqIdString (sid, buf, sizeof (buf) - 1)) {
      segpartptr->slp = AsnIoMemCopy (slp,
                                      (AsnReadFunc) SeqLocAsnRead,
                                      (AsnWriteFunc) SeqLocAsnWrite);
      segpartptr->seqIdOfPart = StringSave (buf);
      if (isSeg) {

        /* only annotate parentBioseq for segmented, not delta bioseq */

        segpartptr->parentBioseq = bsp;
      } else {
        segpartptr->parentBioseq = NULL;
      }
      segpartptr->cumOffset = exindx->cumulative;
      segpartptr->from = from;
      segpartptr->to = to;
      segpartptr->strand = strand;
      segpartptr->itemID = gop->itemID;
    }
  }

  exindx->cumulative += (to - from + 1);

  /* link into segparthead list of parts IDs */

  if (bspextra->segparthead == NULL) {
    bspextra->segparthead = segpartptr;
    exindx->segpartail = segpartptr;
  } else if (exindx->segpartail != NULL) {
    exindx->segpartail->next = segpartptr;
    exindx->segpartail = segpartptr;
  }

  return TRUE;
}

/*****************************************************************************
*
*   SortFeatItemListByID callback sorts array into feature item table by itemID
*   SortFeatItemListBySfp sorts by feature pointer
*   SortFeatItemListByPos sorts by feature position
*   SortFeatItemListByRev sorts by reverse feature position
*
*****************************************************************************/

static int LIBCALLBACK SortFeatItemListByID (VoidPtr vp1, VoidPtr vp2)

{
  SMFeatItemPtr PNTR  spp1 = vp1;
  SMFeatItemPtr PNTR  spp2 = vp2;
  SMFeatItemPtr       sp1;
  SMFeatItemPtr       sp2;

  if (spp1 == NULL || spp2 == NULL) return 0;
  sp1 = *((SMFeatItemPtr PNTR) spp1);
  sp2 = *((SMFeatItemPtr PNTR) spp2);
  if (sp1 == NULL || sp2 == NULL) return 0;

  /* sort by feature itemID */

  if (sp1->itemID > sp2->itemID) {
    return 1;
  } else if (sp1->itemID < sp2->itemID) {
    return -1;

  /* for duplicated genes, etc., that cross origin, put ignored item last for binary search */

  } else if (sp1->ignore) {
    return 1;
  } else if (sp2->ignore) {
    return -1;
  }

  return 0;
}

static int LIBCALLBACK SortFeatItemListBySfp (VoidPtr vp1, VoidPtr vp2)

{
  SMFeatItemPtr PNTR  spp1 = vp1;
  SMFeatItemPtr PNTR  spp2 = vp2;
  SMFeatItemPtr       sp1;
  SMFeatItemPtr       sp2;

  if (spp1 == NULL || spp2 == NULL) return 0;
  sp1 = *((SMFeatItemPtr PNTR) spp1);
  sp2 = *((SMFeatItemPtr PNTR) spp2);
  if (sp1 == NULL || sp2 == NULL) return 0;

  /* sort by SeqFeatPtr value */

  if (sp1->sfp > sp2->sfp) {
    return 1;
  } else if (sp1->sfp < sp2->sfp) {
    return -1;

  /* for duplicated genes, etc., that cross origin, put ignored item last for binary search */

  } else if (sp1->ignore) {
    return 1;
  } else if (sp2->ignore) {
    return -1;
  }

  return 0;
}

static int LIBCALLBACK SortFeatItemListByLabel (VoidPtr vp1, VoidPtr vp2)

{
  int                 compare;
  SMFeatItemPtr PNTR  spp1 = vp1;
  SMFeatItemPtr PNTR  spp2 = vp2;
  SMFeatItemPtr       sp1;
  SMFeatItemPtr       sp2;

  if (spp1 == NULL || spp2 == NULL) return 0;
  sp1 = *((SMFeatItemPtr PNTR) spp1);
  sp2 = *((SMFeatItemPtr PNTR) spp2);
  if (sp1 == NULL || sp2 == NULL) return 0;

  /* sort by label value */

  compare = StringICmp (sp1->label, sp2->label);
  if (compare > 0) {
    return 1;
  } else if (compare < 0) {
    return -1;
  }

  /* If they're case-insensitive the same, but case-sensitive different,
     then fall back to sort by case-sensitive 
     (e.g. AJ344068.1 has genes korA and KorA ) */
  compare = StringCmp (sp1->label, sp2->label);
  if( compare > 0 ) {
    return 1;
  } else if( compare < 0 ) {
    return -1;
  }

  /* for duplicated genes, etc., that cross origin, put ignored item last for binary search */

  if (sp1->ignore) {
    return 1;
  } else if (sp2->ignore) {
    return -1;
  }

  return 0;
}

static int LIBCALLBACK SortFeatItemListByLocusTag (VoidPtr vp1, VoidPtr vp2)

{
  int                 compare;
  GeneRefPtr          grp1;
  GeneRefPtr          grp2;
  SeqFeatPtr          sfp1;
  SeqFeatPtr          sfp2;
  SMFeatItemPtr PNTR  spp1 = vp1;
  SMFeatItemPtr PNTR  spp2 = vp2;
  SMFeatItemPtr       sp1;
  SMFeatItemPtr       sp2;

  if (spp1 == NULL || spp2 == NULL) return 0;
  sp1 = *((SMFeatItemPtr PNTR) spp1);
  sp2 = *((SMFeatItemPtr PNTR) spp2);
  if (sp1 == NULL || sp2 == NULL) return 0;

  sfp1 = sp1->sfp;
  sfp2 = sp2->sfp;
  if (sfp1 == NULL || sfp2 == NULL) return 0;

  if (sfp1->data.choice != SEQFEAT_GENE || sfp2->data.choice != SEQFEAT_GENE) return 0;
  grp1 = (GeneRefPtr) sfp1->data.value.ptrvalue;
  grp2 = (GeneRefPtr) sfp2->data.value.ptrvalue;
  if (grp1 == NULL || grp2 == NULL) return 0;

  /* sort by locus_tag */

  compare = StringICmp (grp1->locus_tag, grp2->locus_tag);
  if (compare > 0) {
    return 1;
  } else if (compare < 0) {
    return -1;
  }

  /* sort by locus if locus_tag is identical */

  compare = StringICmp (grp1->locus, grp2->locus);
  if (compare > 0) {
    return 1;
  } else if (compare < 0) {
    return -1;
  }

  /* for duplicated genes that cross origin, put ignored item last for binary search */

  if (sp1->ignore) {
    return 1;
  } else if (sp2->ignore) {
    return -1;
  }

  return 0;
}

static int LIBCALLBACK SortFeatItemListByPos (VoidPtr vp1, VoidPtr vp2)

{
  Int2                compare;
  CdRegionPtr         crp1;
  CdRegionPtr         crp2;
  Int2                i;
  Int2                j;
  Int2                numivals;
  SeqAnnotPtr         sap1;
  SeqAnnotPtr         sap2;
  SMFeatItemPtr PNTR  spp1 = vp1;
  SMFeatItemPtr PNTR  spp2 = vp2;
  SMFeatItemPtr       sp1;
  SMFeatItemPtr       sp2;
  SeqFeatPtr          sfp1;
  SeqFeatPtr          sfp2;
  Uint1               subtype1;
  Uint1               subtype2;

  if (spp1 == NULL || spp2 == NULL) return 0;
  sp1 = *((SMFeatItemPtr PNTR) spp1);
  sp2 = *((SMFeatItemPtr PNTR) spp2);
  if (sp1 == NULL || sp2 == NULL) return 0;

  /* feature with smallest left extreme is first */

  if (sp1->left > sp2->left) {
    return 1;
  } else if (sp1->left < sp2->left) {
    return -1;

  /* reversing order so that longest feature is first */

  } else if (sp1->right > sp2->right) {
    return -1; /* was 1 */
  } else if (sp1->right < sp2->right) {
    return 1; /* was -1 */
  }

  /* given identical extremes, put operon features first */

  if (sp1->subtype == FEATDEF_operon && sp2->subtype != FEATDEF_operon) {
    return -1;
  } else if (sp2->subtype == FEATDEF_operon && sp1->subtype != FEATDEF_operon) {
    return 1;
  }

  /* then gene features */

  if (sp1->subtype == FEATDEF_GENE && sp2->subtype != FEATDEF_GENE) {
    return -1;
  } else if (sp2->subtype == FEATDEF_GENE && sp1->subtype != FEATDEF_GENE) {
    return 1;
  }

  /* then rna features */

  subtype1 = FindFeatFromFeatDefType (sp1->subtype);
  subtype2 = FindFeatFromFeatDefType (sp2->subtype);

  if (subtype1 == SEQFEAT_RNA && subtype2 != SEQFEAT_RNA) {
    return -1;
  } else if (subtype2 == SEQFEAT_RNA && subtype1 != SEQFEAT_RNA) {
    return 1;
  }

  /* then cds features */

  if (sp1->subtype == FEATDEF_CDS && sp2->subtype != FEATDEF_CDS) {
    return -1;
  } else if (sp2->subtype == FEATDEF_CDS && sp1->subtype != FEATDEF_CDS) {
    return 1;
  }

  /* next compare internal intervals */

  numivals = MIN (sp1->numivals, sp2->numivals);
  if (numivals > 0 && sp1->ivals != NULL && sp2->ivals != NULL) {
    for (i = 0, j = 0; i < numivals; i++) {

      /* check biological start position */

      if (sp1->ivals [j] > sp2->ivals [j]) {
        return 1;
      } else if (sp1->ivals [j] < sp2->ivals [j]) {
        return -1;
      }
      j++;

      /* check biological stop position */

      if (sp1->ivals [j] > sp2->ivals [j]) {
        return -1; /* was 1 */
      } else if (sp1->ivals [j] < sp2->ivals [j]) {
        return 1; /* was -1 */
      }
      j++;
    }
  }

  /* one with fewer intervals goes first */

  if (sp1->numivals > sp2->numivals) {
    return 1;
  } else if (sp1->numivals < sp2->numivals) {
    return -1;
  }

  /* next compare other feature subtypes */

  if (sp1->subtype < sp2->subtype) {
    return -1;
  } else if (sp1->subtype > sp2->subtype) {
    return 1;
  }

  /* if identical gap ranges, use itemID to put flatfile-generated gap feature last */

  if (sp1->subtype == FEATDEF_gap && sp2->subtype == FEATDEF_gap) {
    if (sp1->itemID > sp2->itemID) {
      return 1;
    } else if (sp1->itemID < sp2->itemID) {
      return -1;
    }
  }

  /* if identical cds ranges, compare codon_start */

  if (sp1->subtype == FEATDEF_CDS && sp2->subtype == FEATDEF_CDS) {
    sfp1 = sp1->sfp;
    sfp2 = sp2->sfp;
    if (sfp1 != NULL && sfp2 != NULL) {
      crp1 = (CdRegionPtr) sfp1->data.value.ptrvalue;
      crp2 = (CdRegionPtr) sfp2->data.value.ptrvalue;
      if (crp1 != NULL && crp2 != NULL) {
        if (crp1->frame > 1 || crp2->frame > 1) {
          if (crp1->frame < crp2->frame) {
            return -1;
          } else if (crp1->frame < crp2->frame) {
            return 1;
          }
        }
      }
    }
  }

  /* then compare feature label */

  compare = StringCmp (sp1->label, sp2->label);
  if (compare > 0) {
    return 1;
  } else if (compare < 0) {
    return -1;
  }

  /* compare parent seq-annot by itemID (was sap pointer value) */

  sap1 = sp1->sap;
  sap2 = sp2->sap;
  if (sap1 != NULL && sap2 != NULL) {
    if (sap1->idx.itemID > sap2->idx.itemID) {
      return 1;
    } else if (sap1->idx.itemID < sap2->idx.itemID) {
      return -1;
    }
  }

  /* last comparison to make it absolutely deterministic */

  if (sp1->itemID > sp2->itemID) {
    return 1;
  } else if (sp1->itemID < sp2->itemID) {
    return -1;
  }

  return 0;
}

static int LIBCALLBACK SortFeatItemListByRev (VoidPtr vp1, VoidPtr vp2)

{
  Int2                compare;
  CdRegionPtr         crp1;
  CdRegionPtr         crp2;
  Int2                i;
  Int2                j;
  Int2                k;
  Int2                numivals;
  SeqAnnotPtr         sap1;
  SeqAnnotPtr         sap2;
  SMFeatItemPtr PNTR  spp1 = vp1;
  SMFeatItemPtr PNTR  spp2 = vp2;
  SMFeatItemPtr       sp1;
  SMFeatItemPtr       sp2;
  SeqFeatPtr          sfp1;
  SeqFeatPtr          sfp2;
  Uint1               subtype1;
  Uint1               subtype2;

  if (spp1 == NULL || spp2 == NULL) return 0;
  sp1 = *((SMFeatItemPtr PNTR) spp1);
  sp2 = *((SMFeatItemPtr PNTR) spp2);
  if (sp1 == NULL || sp2 == NULL) return 0;

  /* feature with largest right extreme is first */

  if (sp1->right < sp2->right) {
    return 1;
  } else if (sp1->right > sp2->right) {
    return -1;

  /* reversing order so that longest feature is first */

  } else if (sp1->left < sp2->left) {
    return -1;
  } else if (sp1->left > sp2->left) {
    return 1;
  }

  /* given identical extremes, put operon features first */

  if (sp1->subtype == FEATDEF_operon && sp2->subtype != FEATDEF_operon) {
    return -1;
  } else if (sp2->subtype == FEATDEF_operon && sp1->subtype != FEATDEF_operon) {
    return 1;
  }

  /* then gene features */

  if (sp1->subtype == FEATDEF_GENE && sp2->subtype != FEATDEF_GENE) {
    return -1;
  } else if (sp2->subtype == FEATDEF_GENE && sp1->subtype != FEATDEF_GENE) {
    return 1;
  }

  /* then rna features */

  subtype1 = FindFeatFromFeatDefType (sp1->subtype);
  subtype2 = FindFeatFromFeatDefType (sp2->subtype);

  if (subtype1 == SEQFEAT_RNA && subtype2 != SEQFEAT_RNA) {
    return -1;
  } else if (subtype2 == SEQFEAT_RNA && subtype1 != SEQFEAT_RNA) {
    return 1;
  }

  /* then cds features */

  if (sp1->subtype == FEATDEF_CDS && sp2->subtype != FEATDEF_CDS) {
    return -1;
  } else if (sp2->subtype == FEATDEF_CDS && sp1->subtype != FEATDEF_CDS) {
    return 1;
  }

  /* next compare internal intervals */

  numivals = MIN (sp1->numivals, sp2->numivals);
  if (numivals > 0 && sp1->ivals != NULL && sp2->ivals != NULL) {
    for (i = 0, j = sp1->numivals * 2, k = sp2->numivals * 2; i < numivals; i++) {

      /* check biological stop position */

      k--;
      j--;
      if (sp1->ivals [j] < sp2->ivals [k]) {
        return 1;
      } else if (sp1->ivals [j] > sp2->ivals [k]) {
        return -1;
      }

      /* check biological start position */

      k--;
      j--;
      if (sp1->ivals [j] < sp2->ivals [k]) {
        return -1;
      } else if (sp1->ivals [j] > sp2->ivals [k]) {
        return 1;
      }
    }
  }

  /* one with fewer intervals goes first */

  if (sp1->numivals > sp2->numivals) {
    return 1;
  } else if (sp1->numivals < sp2->numivals) {
    return -1;
  }

  /* next compare other feature subtypes */

  if (sp1->subtype < sp2->subtype) {
    return -1;
  } else if (sp1->subtype > sp2->subtype) {
    return 1;
  }

  /* if identical gap ranges, use itemID to put flatfile-generated gap feature last */

  if (sp1->subtype == FEATDEF_gap && sp2->subtype == FEATDEF_gap) {
    if (sp1->itemID > sp2->itemID) {
      return 1;
    } else if (sp1->itemID < sp2->itemID) {
      return -1;
    }
  }

  /* if identical cds ranges, compare codon_start */

  if (sp1->subtype == FEATDEF_CDS && sp2->subtype == FEATDEF_CDS) {
    sfp1 = sp1->sfp;
    sfp2 = sp2->sfp;
    if (sfp1 != NULL && sfp2 != NULL) {
      crp1 = (CdRegionPtr) sfp1->data.value.ptrvalue;
      crp2 = (CdRegionPtr) sfp2->data.value.ptrvalue;
      if (crp1 != NULL && crp2 != NULL) {
        if (crp1->frame > 1 || crp2->frame > 1) {
          if (crp1->frame < crp2->frame) {
            return -1;
          } else if (crp1->frame < crp2->frame) {
            return 1;
          }
        }
      }
    }
  }

  /* then compare feature label */

  compare = StringCmp (sp1->label, sp2->label);
  if (compare > 0) {
    return 1;
  } else if (compare < 0) {
    return -1;
  }

  /* compare parent seq-annot by itemID (was sap pointer value) */

  sap1 = sp1->sap;
  sap2 = sp2->sap;
  if (sap1 != NULL && sap2 != NULL) {
    if (sap1->idx.itemID > sap2->idx.itemID) {
      return 1;
    } else if (sap1->idx.itemID < sap2->idx.itemID) {
      return -1;
    }
  }

  /* last comparison to make it absolutely deterministic */

  if (sp1->itemID > sp2->itemID) {
    return 1;
  } else if (sp1->itemID < sp2->itemID) {
    return -1;
  }

  return 0;
}

static int LIBCALLBACK SortFidListByFeatID (VoidPtr vp1, VoidPtr vp2)

{
  int                compare;
  SMFidItemPtr PNTR  spp1 = vp1;
  SMFidItemPtr PNTR  spp2 = vp2;
  SMFidItemPtr       sp1;
  SMFidItemPtr       sp2;

  if (spp1 == NULL || spp2 == NULL) return 0;
  sp1 = *((SMFidItemPtr PNTR) spp1);
  sp2 = *((SMFidItemPtr PNTR) spp2);
  if (sp1 == NULL || sp2 == NULL) return 0;

  /* sort by feature itemID label value */

  compare = StringICmp (sp1->fid, sp2->fid);
  if (compare > 0) {
    return 1;
  } else if (compare < 0) {
    return -1;
  }

  return 0;
}

/*****************************************************************************
*
*   IndexSegmentedParts callback builds index to speed up mapping
*     of parts to segmented bioseqs
*
*****************************************************************************/

static int LIBCALLBACK SortSeqIdxArray (VoidPtr ptr1, VoidPtr ptr2)

{
  Int2              compare;
  SMSeqIdxPtr PNTR  partp1 = ptr1;
  SMSeqIdxPtr PNTR  partp2 = ptr2;
  SMSeqIdxPtr       part1, part2;

  if (partp1 == NULL || partp2 == NULL) return 0;
  part1 = *((SMSeqIdxPtr PNTR) partp1);
  part2 = *((SMSeqIdxPtr PNTR) partp2);
  if (part1 == NULL || part2 == NULL) return 0;
  compare = StringCmp (part1->seqIdOfPart, part2->seqIdOfPart);
  if (compare > 0) {
    return 1;
  } else if (compare < 0) {
    return -1;
  }
  if (part1->cumOffset > part2->cumOffset) {
    return 1;
  } else if (part1->cumOffset < part2->cumOffset) {
    return -1;
  }
  return 0;
}

static Boolean WithinPartsSet (BioseqPtr bsp)

{
  BioseqSetPtr  bssp;

  if (bsp == NULL) return FALSE;

  if (bsp->idx.parenttype == OBJ_BIOSEQSET && bsp->idx.parentptr != NULL) {
    bssp = (BioseqSetPtr) bsp->idx.parentptr;
    while (bssp != NULL) {
      if (bssp->_class == BioseqseqSet_class_parts) return TRUE;
      if (bssp->idx.parenttype != OBJ_BIOSEQSET) return FALSE;
      bssp = bssp->idx.parentptr;
    }
  }

  return FALSE;
}

static void IndexSegmentedParts (SeqEntryPtr sep, BioseqPtr PNTR lastsegbsp)

{
  BioseqPtr         bsp;
  BioseqExtraPtr    bspextra;
  BioseqSetPtr      bssp;
  Int4              i;
  Int4              numsegs = 0;
  ObjMgrDataPtr     omdp;
  SMSeqIdxPtr PNTR  partsByLoc;
  SMSeqIdxPtr PNTR  partsBySeqId;
  SMSeqIdxPtr       segpartptr;

  if (sep == NULL) return;
  if (IS_Bioseq_set (sep)) {
    bssp = (BioseqSetPtr) sep->data.ptrvalue;
    if (bssp == NULL) return;
    for (sep = bssp->seq_set; sep != NULL; sep = sep->next) {
      IndexSegmentedParts (sep, lastsegbsp);
    }
    if (bssp->_class == BioseqseqSet_class_segset && lastsegbsp != NULL) {
      *lastsegbsp = NULL;
    }
    return;
  }

  if (! IS_Bioseq (sep)) return;
  bsp = (BioseqPtr) sep->data.ptrvalue;
  if (bsp == NULL) return;

  /* check for raw part packaged with segmented bioseq */

  if ((bsp->repr == Seq_repr_raw || IsNonGappedLiteral (bsp)) &&
      lastsegbsp != NULL && *lastsegbsp != NULL && WithinPartsSet (bsp)) {
    omdp = SeqMgrGetOmdpForBioseq (bsp);
    if (omdp == NULL) return;

    bspextra = (BioseqExtraPtr) omdp->extradata;
    if (bspextra == NULL) {
      CreateBioseqExtraBlock (omdp, bsp);
      bspextra = (BioseqExtraPtr) omdp->extradata;
    }
    if (bspextra == NULL) return;

    /* now record segmented parent of raw part if all are packaged together */

    bspextra->parentBioseq = *lastsegbsp;
    return;
  }

  if (bsp->repr != Seq_repr_seg && bsp->repr != Seq_repr_delta) return;

  omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp == NULL) return;

  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) {
    CreateBioseqExtraBlock (omdp, bsp);
    bspextra = (BioseqExtraPtr) omdp->extradata;
  }
  if (bspextra == NULL) return;

  if (lastsegbsp != NULL && bsp->repr == Seq_repr_seg) {
    *lastsegbsp = bsp;
  }

  for (segpartptr = bspextra->segparthead;
       segpartptr != NULL;
       segpartptr = segpartptr->next) {
    numsegs++;
  }

  bspextra->numsegs = numsegs;
  segpartptr = bspextra->segparthead;
  if (numsegs < 1 || segpartptr == NULL) return;

  partsByLoc = (SMSeqIdxPtr PNTR) MemNew (sizeof (SMSeqIdxPtr) * (numsegs + 1));
  bspextra->partsByLoc = partsByLoc;

  if (partsByLoc != NULL) {
    i = 0;
    while (i < numsegs && segpartptr != NULL) {
      partsByLoc [i] = segpartptr;
      segpartptr = segpartptr->next;
      i++;
    }

    partsBySeqId = (SMSeqIdxPtr PNTR) MemNew (sizeof (SMSeqIdxPtr) * (numsegs + 1));
    bspextra->partsBySeqId = partsBySeqId;

    if (partsBySeqId != NULL) {
      for (i = 0; i < numsegs; i++) {
        partsBySeqId [i] = partsByLoc [i];
      }

      /* sort array by SeqId for binary search */

      StableMergeSort ((Pointer) partsBySeqId, numsegs, sizeof (SMSeqIdxPtr), SortSeqIdxArray);
    }

  }
}

/*****************************************************************************
*
*   IndexRecordedFeatures callback builds sorted arrays of features and genes
*
*****************************************************************************/

static void IndexRecordedFeatures (SeqEntryPtr sep, Boolean dorevfeats, Uint4 baseItemID)

{
  BioseqPtr           bsp;
  BioseqExtraPtr      bspextra;
  BioseqSetPtr        bssp;
  SeqFeatPtr          cds;
  SMFeatBlockPtr      curr;
  SeqLocPtr           dnaloc;
  SMFeatItemPtr PNTR  featsByID;
  SMFeatItemPtr PNTR  featsBySfp;
  SMFeatItemPtr PNTR  featsByPos;
  SMFeatItemPtr PNTR  featsByRev;
  SMFeatItemPtr PNTR  featsByLabel;
  SMFeatItemPtr PNTR  genesByLocusTag;
  SMFeatItemPtr PNTR  genesByPos;
  Int4                i;
  Int4                j;
  SMFeatItemPtr       item;
  SMFeatItemPtr       last;
  BioseqPtr           nuc;
  Int4                numfeats;
  Int4                numgenes;
  ObjMgrDataPtr       omdp;
  Int4                pt;
  SeqLocPtr           segloc;
  SeqFeatPtr          sfp;
  SeqLocPtr           slp;
  Int4                stop;

  if (sep == NULL) return;
  if (IS_Bioseq_set (sep)) {
    bssp = (BioseqSetPtr) sep->data.ptrvalue;
    if (bssp == NULL) return;
    for (sep = bssp->seq_set; sep != NULL; sep = sep->next) {
      IndexRecordedFeatures (sep, dorevfeats, baseItemID);
    }
    return;
  }

  if (! IS_Bioseq (sep)) return;
  bsp = (BioseqPtr) sep->data.ptrvalue;
  if (bsp == NULL) return;

  omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp == NULL) return;
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return;

  numfeats = bspextra->numfeats;

  curr = bspextra->featlisthead;

  if (numfeats > 0 && curr != NULL) {

    /* build array of pointers into feature items */

    featsByID = (SMFeatItemPtr PNTR) MemNew (sizeof (SMFeatItemPtr) * (numfeats + 1));
    bspextra->featsByID = featsByID;

    if (featsByID != NULL) {
      i = 0;
      j = 0;
      while (i < numfeats && curr != NULL) {
        if (j >= curr->index || j >= bspextra->blocksize) {
          j = 0;
          curr = curr->next;
        }
        if (curr != NULL && j < curr->index && curr->data != NULL) {
          featsByID [i] = &(curr->data [j]);
          i++;
          j++;
        }
      }
      if (i < numfeats) {
        ErrPostEx (SEV_WARNING, 0, 0, "SeqMgr indexing feature table build problem");
      }

      featsBySfp = (SMFeatItemPtr PNTR) MemNew (sizeof (SMFeatItemPtr) * (numfeats + 1));
      bspextra->featsBySfp = featsBySfp;

      if (featsBySfp != NULL) {
        for (i = 0; i < numfeats; i++) {
          featsBySfp [i] = featsByID [i];
        }

        /* sort all features by SeqFeatPtr value */

        StableMergeSort ((VoidPtr) featsBySfp, (size_t) numfeats, sizeof (SMFeatItemPtr), SortFeatItemListBySfp);
      }

      featsByPos = (SMFeatItemPtr PNTR) MemNew (sizeof (SMFeatItemPtr) * (numfeats + 1));
      bspextra->featsByPos = featsByPos;

      if (featsByPos != NULL) {
        for (i = 0; i < numfeats; i++) {
          featsByPos [i] = featsByID [i];
        }

        /* sort all features by feature location on bioseq */

        StableMergeSort ((VoidPtr) featsByPos, (size_t) numfeats, sizeof (SMFeatItemPtr), SortFeatItemListByPos);

        for (i = 0; i < numfeats; i++) {
          item = featsByPos [i];
          if (item != NULL) {
            item->index = i;
          }
        }

        /* gap feature in record overrides flatfile-generated feature */

        if (baseItemID > 0) {
          last = featsByPos [0];
          for (i = 1; i < numfeats; i++) {
            item = featsByPos [i];
            if (item != NULL && last != NULL) {
              if (last->subtype == FEATDEF_gap && item->subtype == FEATDEF_gap) {
                if (last->left == item->left && last->right == item->right) {
                  if (item->itemID >= baseItemID) {
                    item->ignore = TRUE;
                  }
                }
              }
            }
            last = item;
          }
        }

        /* build arrays of sorted gene, mRNA, CDS, publication, and biosource features for lookup by overlap */

        bspextra->genesByPos = SeqMgrBuildFeatureIndex (bsp, &(bspextra->numgenes), 0, FEATDEF_GENE);
        bspextra->mRNAsByPos = SeqMgrBuildFeatureIndex (bsp, &(bspextra->nummRNAs), 0, FEATDEF_mRNA);
        bspextra->CDSsByPos = SeqMgrBuildFeatureIndex (bsp, &(bspextra->numCDSs), 0, FEATDEF_CDS);
        bspextra->pubsByPos = SeqMgrBuildFeatureIndex (bsp, &(bspextra->numpubs), 0, FEATDEF_PUB);
        bspextra->orgsByPos = SeqMgrBuildFeatureIndex (bsp, &(bspextra->numorgs), 0, FEATDEF_BIOSRC);
        bspextra->operonsByPos = SeqMgrBuildFeatureIndex (bsp, &(bspextra->numoperons), 0, FEATDEF_operon);
      }

      if (dorevfeats) {
        featsByRev = (SMFeatItemPtr PNTR) MemNew (sizeof (SMFeatItemPtr) * (numfeats + 1));
        bspextra->featsByRev = featsByRev;

        if (featsByRev != NULL) {
          for (i = 0; i < numfeats; i++) {
            featsByRev [i] = featsByID [i];
          }

          /* optionally sort all features by feature reverse location on bioseq */

          StableMergeSort ((VoidPtr) featsByRev, (size_t) numfeats, sizeof (SMFeatItemPtr), SortFeatItemListByRev);
        }
      }

      featsByLabel = (SMFeatItemPtr PNTR) MemNew (sizeof (SMFeatItemPtr) * (numfeats + 1));
      bspextra->featsByLabel = featsByLabel;

      if (featsByLabel != NULL) {
        for (i = 0; i < numfeats; i++) {
          featsByLabel [i] = featsByID [i];
        }

        /* sort all features by label value */

        StableMergeSort ((VoidPtr) featsByLabel, (size_t) numfeats, sizeof (SMFeatItemPtr), SortFeatItemListByLabel);
      }

      genesByPos = bspextra->genesByPos;
      numgenes = bspextra->numgenes;
      if (genesByPos != NULL && numgenes > 0) {

        genesByLocusTag = (SMFeatItemPtr PNTR) MemNew (sizeof (SMFeatItemPtr) * (numgenes + 1));
        bspextra->genesByLocusTag = genesByLocusTag;

        if (genesByLocusTag != NULL) {
          for (i = 0; i < numgenes; i++) {
            genesByLocusTag [i] = genesByPos [i];
          }

          /* sort by locus_tag value */

          StableMergeSort ((VoidPtr) genesByLocusTag, (size_t) numgenes, sizeof (SMFeatItemPtr), SortFeatItemListByLocusTag);
        }
      }
    }
  }

  if (numfeats < 1 || (! ISA_aa (bsp->mol))) return;
  cds = SeqMgrGetCDSgivenProduct (bsp, NULL);
  if (cds == NULL) return;
  nuc = BioseqFindFromSeqLoc (cds->location);
  if (nuc == NULL) return;

  featsByPos = bspextra->featsByPos;
  if (featsByPos != NULL) {
    for (i = 0; i < numfeats; i++) {
      item = featsByPos [i];
      if (item != NULL) {
        sfp = item->sfp;
        if (sfp != NULL) {

          /* map to dna (on parts if segmented) */

          dnaloc = aaFeatLoc_to_dnaFeatLoc (cds, sfp->location);
          if (dnaloc != NULL) {

            /* map to segmented bioseq coordinates if necessary */

            segloc = SeqLocMergeExEx (nuc, dnaloc, NULL, FALSE, TRUE, FALSE, FALSE, TRUE, TRUE);

            SeqLocFree (dnaloc);
            if (segloc != NULL) {

              slp = NULL;
              stop = -1;

              /* now find where last point maps on nucleotide for flatfile */

              while ((slp = SeqLocFindNext (segloc, slp)) != NULL) {
                pt = SeqLocStop (slp);
                if (pt != -1) {
                  stop = pt;
                }
              }
              item->dnaStop = stop;

              SeqLocFree (segloc);
            }
          }
        }
      }
    }
  }
}

/*****************************************************************************
*
*   IndexFeaturesOnEntity makes feature pointers across all Bioseqs in entity
*
*****************************************************************************/

static void IndexFeaturesOnEntity (SeqEntryPtr sep, SMFeatItemPtr PNTR featsByID, Int4Ptr countP)

{
  BioseqPtr       bsp;
  BioseqExtraPtr  bspextra;
  BioseqSetPtr    bssp;
  Int4            count;
  Int4            i;
  Int4            numfeats;
  ObjMgrDataPtr   omdp;

  if (sep == NULL || featsByID == NULL || countP == NULL) return;
  if (IS_Bioseq_set (sep)) {
    bssp = (BioseqSetPtr) sep->data.ptrvalue;
    if (bssp == NULL) return;
    for (sep = bssp->seq_set; sep != NULL; sep = sep->next) {
      IndexFeaturesOnEntity (sep, featsByID, countP);
    }
    return;
  }

  if (! IS_Bioseq (sep)) return;
  bsp = (BioseqPtr) sep->data.ptrvalue;
  if (bsp == NULL) return;

  omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp == NULL) return;
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return;

  numfeats = bspextra->numfeats;
  if (bspextra->featsByID != NULL || numfeats > 0) {
    count = *countP;

    for (i = 0; i < numfeats; i++, count++) {
      featsByID [count] = bspextra->featsByID [i];
    }

    *countP = count;
  }
}

/*****************************************************************************
*
*   SortDescItemListByID callback sorts by descriptor itemID
*   SortDescItemListBySdp sorts by descriptor pointer
*   SortDescItemListByIndex sorts by descriptor index
*
*****************************************************************************/

static int LIBCALLBACK SortDescItemListByID (VoidPtr vp1, VoidPtr vp2)

{
  SMDescItemPtr PNTR  spp1 = vp1;
  SMDescItemPtr PNTR  spp2 = vp2;
  SMDescItemPtr       sp1;
  SMDescItemPtr       sp2;

  if (spp1 == NULL || spp2 == NULL) return 0;
  sp1 = *((SMDescItemPtr PNTR) spp1);
  sp2 = *((SMDescItemPtr PNTR) spp2);
  if (sp1 == NULL || sp2 == NULL) return 0;

  /* sort by descriptor itemID */

  if (sp1->itemID > sp2->itemID) {
    return 1;
  } else if (sp1->itemID < sp2->itemID) {
    return -1;
  }

  return 0;
}

static int LIBCALLBACK SortDescItemListBySdp (VoidPtr vp1, VoidPtr vp2)

{
  SMDescItemPtr PNTR  spp1 = vp1;
  SMDescItemPtr PNTR  spp2 = vp2;
  SMDescItemPtr       sp1;
  SMDescItemPtr       sp2;

  if (spp1 == NULL || spp2 == NULL) return 0;
  sp1 = *((SMDescItemPtr PNTR) spp1);
  sp2 = *((SMDescItemPtr PNTR) spp2);
  if (sp1 == NULL || sp2 == NULL) return 0;

  /* sort by SeqDescrPtr value */

  if (sp1->sdp > sp2->sdp) {
    return 1;
  } else if (sp1->sdp < sp2->sdp) {
    return -1;
  }

  return 0;
}

static int LIBCALLBACK SortDescItemListByIndex (VoidPtr vp1, VoidPtr vp2)

{
  SMDescItemPtr PNTR  spp1 = vp1;
  SMDescItemPtr PNTR  spp2 = vp2;
  SMDescItemPtr       sp1;
  SMDescItemPtr       sp2;

  if (spp1 == NULL || spp2 == NULL) return 0;
  sp1 = *((SMDescItemPtr PNTR) spp1);
  sp2 = *((SMDescItemPtr PNTR) spp2);
  if (sp1 == NULL || sp2 == NULL) return 0;

  /* sort by descriptor index */

  if (sp1->index > sp2->index) {
    return 1;
  } else if (sp1->index < sp2->index) {
    return -1;
  }

  return 0;
}

/*****************************************************************************
*
*   RecordDescriptorsInBioseqs callback records list of relevant descriptors
*
*****************************************************************************/

static void RecordDescriptorsInBioseqs (BioseqPtr bsp, Pointer userdata)

{
  BioseqExtraPtr      bspextra;
  SeqMgrDescContext   context;
  ValNodePtr          head = NULL;
  ValNodePtr          last = NULL;
  Int4                numdescs = 0;
  ObjMgrDataPtr       omdp;
  SMDescItemPtr       sdip;
  SeqDescrPtr         sdp;
  ValNodePtr          vnp;

  if (bsp == NULL) return;

  omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp == NULL) return;
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return;

  sdp = SeqMgrGetNextDescriptor (bsp, NULL, 0, &context);
  while (sdp != NULL) {

    numdescs++;
    sdip = (SMDescItemPtr) MemNew (sizeof (SMDescItem));
    if (sdip != NULL) {
      vnp = ValNodeNew (last);
      if (head == NULL) {
        head = vnp;
      }
      last = vnp;
      if (vnp != NULL) {
        vnp->data.ptrvalue = (Pointer) sdip;
      }
      sdip->sdp = sdp;
      sdip->sep = context.sep;
      sdip->itemID = context.itemID;
      sdip->index = context.index;
      sdip->level = context.level;
      sdip->seqdesctype = context.seqdesctype;
    }

    sdp = SeqMgrGetNextDescriptor (bsp, sdp, 0, &context);
  }

  bspextra->desclisthead = head;
  bspextra->numdescs = numdescs;
}

/*****************************************************************************
*
*   RecordDescriptorsOnTopSet callback records list of all descriptors
*
*****************************************************************************/

typedef struct descindex {
  ValNodePtr  deschead;
  ValNodePtr  lastdesc;
  Int4        numdescs;
} DescIndex, PNTR DescIndexPtr;

static void RecordAllDescsCallback (SeqEntryPtr sep, Pointer mydata, Int4 index, Int2 indent)

{
  BioseqPtr      bsp;
  BioseqSetPtr   bssp;
  DescIndexPtr   dxp;
  ObjValNodePtr  ovp;
  SMDescItemPtr  sdip;
  SeqDescrPtr    sdp = NULL;
  ValNodePtr     vnp;

  if (sep == NULL || mydata == NULL) return;
  dxp = (DescIndexPtr) mydata;

  if (IS_Bioseq (sep)) {
    bsp = (BioseqPtr) sep->data.ptrvalue;
    if (bsp == NULL) return;
    sdp = bsp->descr;
  } else if (IS_Bioseq_set (sep)) {
    bssp = (BioseqSetPtr) sep->data.ptrvalue;
    if (bssp == NULL) return;
    sdp = bssp->descr;
  } else return;

  while (sdp != NULL) {
    (dxp->numdescs)++;
    sdip = (SMDescItemPtr) MemNew (sizeof (SMDescItem));
    if (sdip != NULL) {
      vnp = ValNodeNew (dxp->lastdesc);
      if (dxp->deschead == NULL) {
        dxp->deschead = vnp;
      }
      dxp->lastdesc = vnp;
      if (vnp != NULL) {
        vnp->data.ptrvalue = (Pointer) sdip;
      }
      sdip->sdp = sdp;
      sdip->sep = sep;
      if (sdp->extended != 0) {
        ovp = (ObjValNodePtr) sdp;
        sdip->itemID = ovp->idx.itemID;
      }
      sdip->index = 0;
      sdip->level = indent;
      sdip->seqdesctype = sdp->choice;
    }
    sdp = sdp->next;
  }
}

static void RecordDescriptorsOnTopSet (SeqEntryPtr sep)

{
  BioseqExtraPtr  bspextra;
  BioseqSetPtr    bssp;
  DescIndex       dx;
  ObjMgrDataPtr   omdp;

  if (sep == NULL) return;
  if (! IS_Bioseq_set (sep)) return;

  bssp = (BioseqSetPtr) sep->data.ptrvalue;
  if (bssp == NULL) return;

  omdp = SeqMgrGetOmdpForPointer (bssp);
  if (omdp == NULL) return;
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) {
    CreateBioseqExtraBlock (omdp, NULL);
    bspextra = (BioseqExtraPtr) omdp->extradata;
  }
  if (bspextra == NULL) return;

  dx.deschead = NULL;
  dx.lastdesc = NULL;
  dx.numdescs = 0;

  SeqEntryExplore (sep, (Pointer) &dx, RecordAllDescsCallback);

  bspextra->desclisthead = dx.deschead;
  bspextra->numdescs = dx.numdescs;
}

/*****************************************************************************
*
*   IndexRecordedDescriptors callback builds sorted arrays of descriptors
*
*****************************************************************************/

static void IndexRecordedDescriptors (SeqEntryPtr sep, Pointer mydata, Int4 index, Int2 indent)

{
  BioseqPtr           bsp;
  BioseqExtraPtr      bspextra;
  BioseqSetPtr        bssp;
  SMDescItemPtr PNTR  descrsByID;
  SMDescItemPtr PNTR  descrsBySdp;
  SMDescItemPtr PNTR  descrsByIndex;
  ValNodePtr          head;
  Int4                i;
  Int4                numdescs;
  ObjMgrDataPtr       omdp = NULL;
  SMDescItemPtr       sdip;
  ValNodePtr          vnp;

  if (sep == NULL) return;
  if (IS_Bioseq (sep)) {
    bsp = (BioseqPtr) sep->data.ptrvalue;
    if (bsp == NULL) return;
    omdp = SeqMgrGetOmdpForBioseq (bsp);
  } else if (IS_Bioseq_set (sep)) {
    bssp = (BioseqSetPtr) sep->data.ptrvalue;
    if (bssp == NULL) return;
    omdp = SeqMgrGetOmdpForPointer (bssp);
  } else return;

  if (omdp == NULL) return;
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return;

  head = bspextra->desclisthead;
  numdescs = bspextra->numdescs;

  if (head != NULL && numdescs > 0) {

    /* build array of pointers into descriptor items */

    descrsByID = (SMDescItemPtr PNTR) MemNew (sizeof (SMDescItemPtr) * (numdescs + 1));
    bspextra->descrsByID = descrsByID;

    descrsBySdp = (SMDescItemPtr PNTR) MemNew (sizeof (SMDescItemPtr) * (numdescs + 1));
    bspextra->descrsBySdp = descrsBySdp;

    descrsByIndex = (SMDescItemPtr PNTR) MemNew (sizeof (SMDescItemPtr) * (numdescs + 1));
    bspextra->descrsByIndex = descrsByIndex;

    if (descrsByID != NULL && descrsBySdp != NULL && descrsByIndex != NULL) {
      for (i = 0, vnp = head; i < numdescs && vnp != NULL; i++, vnp = vnp->next) {
        sdip = (SMDescItemPtr) vnp->data.ptrvalue;
        if (sdip != NULL) {
          descrsByID [i] = sdip;
          descrsBySdp [i] = sdip;
          descrsByIndex [i] = sdip;
        }
      }

      /* sort all descriptors by itemID, SeqDescrPtr value, or index */

      StableMergeSort ((VoidPtr) descrsByID, (size_t) numdescs, sizeof (SMDescItemPtr), SortDescItemListByID);
      StableMergeSort ((VoidPtr) descrsBySdp, (size_t) numdescs, sizeof (SMDescItemPtr), SortDescItemListBySdp);
      StableMergeSort ((VoidPtr) descrsByIndex, (size_t) numdescs, sizeof (SMDescItemPtr), SortDescItemListByIndex);
    }
  }
}

/*****************************************************************************
*
*   DoSegmentedProtein needed because SeqIdWithinBioseq may fail for segmented proteins
*
*****************************************************************************/

static void DoSegmentedProtein (BioseqPtr bsp, Pointer userdata)

{
  BioseqExtraPtr     bspextra;
  SeqMgrFeatContext  context;
  ObjMgrDataPtr      omdp;
  BioseqPtr          parent = NULL;
  SeqFeatPtr         sfp;

  if (! ISA_aa (bsp->mol)) return;

   if (bsp->repr != Seq_repr_seg) {
    parent = SeqMgrGetParentOfPart (bsp, NULL);
    if (parent == NULL) return;
  }

 omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp == NULL) return;

  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return;

  /* if it already has a best protein feature, return */

  if (bspextra->protFeat != NULL) return;

  /* part of parent inherits best protein from parent */

  if (bsp->repr != Seq_repr_seg && parent != NULL) {
    sfp = SeqMgrGetBestProteinFeature (parent, NULL);
    bspextra->protFeat = sfp;
    return;
  }

  /* now check for full-length proteins on segmented parent */

  sfp = SeqMgrGetNextFeatureByLabel (bsp, NULL, SEQFEAT_PROT, 0, &context);
  while (sfp != NULL) {
    if (context.left == 0 && context.right == bsp->length - 1) {
      bspextra->protFeat = sfp;
    }

    sfp = SeqMgrGetNextFeatureByLabel (bsp, sfp, SEQFEAT_PROT, 0, &context);
  }
}

/*****************************************************************************
*
*   IndexAnnotDescsOnBioseqs
*
*****************************************************************************/

static int LIBCALLBACK SortAbpVnpByBsp (VoidPtr ptr1, VoidPtr ptr2)

{
  AdpBspPtr      abp1, abp2;
  AnnotDescPtr   adp1, adp2;
  BioseqPtr      bsp1, bsp2;
  ObjValNodePtr  ovp1, ovp2;
  ValNodePtr     vnp1, vnp2;

  if (ptr1 == NULL || ptr2 == NULL) return 0;
  vnp1 = *((ValNodePtr PNTR) ptr1);
  vnp2 = *((ValNodePtr PNTR) ptr2);
  if (vnp1 == NULL || vnp2 == NULL) return 0;
  abp1 = (AdpBspPtr) vnp1->data.ptrvalue;
  abp2 = (AdpBspPtr) vnp2->data.ptrvalue;
  if (abp1 == NULL || abp2 == NULL) return 0;
  bsp1 = (BioseqPtr) abp1->bsp;
  bsp2 = (BioseqPtr) abp2->bsp;
  if (bsp1 > bsp2) {
    return 1;
  } else if (bsp1 < bsp2) {
    return -1;
  }
  adp1 = (AnnotDescPtr) abp1->adp;
  adp2 = (AnnotDescPtr) abp2->adp;
  if (adp1 == NULL || adp2 == NULL) return 0;
  if (adp1->extended != 0 && adp2->extended != 0) {
    ovp1 = (ObjValNodePtr) adp1;
    ovp2 = (ObjValNodePtr) adp2;
    if (ovp1->idx.itemID > ovp2->idx.itemID) {
      return 1;
    } else if (ovp1->idx.itemID < ovp2->idx.itemID) {
      return -1;
    }
  }
  return 0;
}

static BioseqPtr GetBspFromVnpAbpBsp (
  ValNodePtr vnp
)

{
  AdpBspPtr  abp;

  if (vnp == NULL) return NULL;
  abp = (AdpBspPtr) vnp->data.ptrvalue;
  if (abp == NULL) return NULL;
  return abp->bsp;
}

static void IndexAnnotDescsOnBioseqs (
  ValNodePtr adphead
)

{
  AdpBspPtr          abp;
  Int4               adpcount, count;
  AnnotDescPtr PNTR  annotDescByID;
  BioseqPtr          bsp;
  BioseqExtraPtr     bspextra;
  ValNodePtr         nxt, top, vnp;
  ObjMgrDataPtr      omdp;

  if (adphead == NULL) return;
  top = adphead;

  while (top != NULL) {
    bsp = GetBspFromVnpAbpBsp (top);
    adpcount = 1;
    nxt = top->next;
    while (nxt != NULL && GetBspFromVnpAbpBsp (nxt) == bsp) {
      adpcount++;
      nxt = nxt->next;
    }

    if (bsp != NULL) {
      omdp = SeqMgrGetOmdpForBioseq (bsp);
      if (omdp != NULL && omdp->datatype == OBJ_BIOSEQ) {
        CreateBioseqExtraBlock (omdp, NULL);
        bspextra = (BioseqExtraPtr) omdp->extradata;
        if (bspextra != NULL) {

          annotDescByID = (AnnotDescPtr PNTR) MemNew (sizeof (AnnotDescPtr) * (adpcount + 1));
          if (annotDescByID != NULL) {

            for (vnp = top, count = 0; vnp != NULL && count < adpcount; vnp = vnp->next, count++) {
              abp = (AdpBspPtr) vnp->data.ptrvalue;
              if (abp == NULL) continue;
              annotDescByID [count] = abp->adp;
            }

            bspextra->annotDescByID = annotDescByID;
            bspextra->numannotdesc = adpcount;
          }
        }
      }
    }

    top = nxt;
  }
}

static void IndexFeatIDsOnEntity (
  BioseqExtraPtr bspextra
)

{
  Char                buf [32];
  SMFidItemPtr PNTR   featsByFeatID;
  SMFeatItemPtr PNTR  featsByID;
  ValNodePtr          head = NULL, last = NULL;
  SMFeatItemPtr       item;
  Int4                j;
  Int4                len;
  ObjectIdPtr         oip;
  SMFidItemPtr        sfip;
  SeqFeatPtr          sfp;
  ValNodePtr          vnp;

  if (bspextra == NULL || bspextra->numfeats < 1 || bspextra->featsByID == NULL) return;

  featsByID = bspextra->featsByID;
  for (j = 0; j < bspextra->numfeats; j++) {
    item = featsByID [j];
    if (item == NULL) continue;
    if (item->ignore) continue;
    sfp = item->sfp;
    if (sfp == NULL) continue;
    if (sfp->id.choice != 3) continue;
    oip = (ObjectIdPtr) sfp->id.value.ptrvalue;
    if (oip == NULL) continue;
    sfip = (SMFidItemPtr) MemNew (sizeof (SMFidItem));
    if (sfip == NULL) continue;
    if (StringDoesHaveText (oip->str)) {
      sfip->fid = StringSave (oip->str);
    } else {
      sprintf (buf, "%ld", (long) oip->id);
      sfip->fid = StringSave (buf);
    }
    sfip->feat = item;
    vnp = ValNodeAddPointer (&last, 0, (Pointer) sfip);
    if (head == NULL) {
      head = vnp;
    }
    last = vnp;
  }

  len = ValNodeLen (head);
  if (len < 1) return;
  featsByFeatID = (SMFidItemPtr PNTR) MemNew (sizeof (SMFidItemPtr) * (len + 1));
  if (featsByFeatID != NULL) {
    for (vnp = head, j = 0; vnp != NULL; vnp = vnp->next, j++) {
      sfip = (SMFidItemPtr) vnp->data.ptrvalue;
      if (sfip == NULL) continue;
      featsByFeatID [j] = sfip;
    }

    /* sort all features on entity-wide list by itemID */

    StableMergeSort ((VoidPtr) featsByFeatID, (size_t) len, sizeof (SMFidItemPtr), SortFidListByFeatID);

    bspextra->featsByFeatID = featsByFeatID;
    bspextra->numfids = len;
  }
  ValNodeFree (head);
}

/*****************************************************************************
*
*   SeqMgrReindexBioseqExtraData refreshes internal indices for rapid retrieval
*
*****************************************************************************/

static Uint2 LIBCALL s_DoSeqMgrIndexFeatures (
  Uint2 entityID,
  Pointer ptr,
  Boolean flip,
  Boolean dorevfeats,
  ValNodePtr extra
)

{
  AdpBspPtr           abp;
  AnnotDescPtr PNTR   annotDescByID;
  Uint4               baseItemID = 0;
  BioseqPtr           bsp;
  BioseqExtraPtr      bspextra;
  Int4                count;
  ExtraIndex          exind;
  SMFeatItemPtr PNTR  featsByID;
  BioseqPtr           lastsegbsp = NULL;
  Boolean             objMgrFilter [OBJ_MAX];
  SeqEntryPtr         oldscope;
  ObjMgrDataPtr       omdp;
  ValNodePtr          publist;
  SeqAnnotPtr         sap;
  SeqEntryPtr         sep;
  SeqFeatPtr          sfp;
  ValNodePtr          vnp;

  if (entityID == 0) {
    entityID = ObjMgrGetEntityIDForPointer (ptr);
  }
  if (entityID == 0) return 0;

  /* reset any existing index data on all bioseqs in entity */

  SeqMgrClearFeatureIndexes (entityID, NULL);

  /* want to scope to bioseqs within the entity, to allow for colliding IDs */

  sep = SeqMgrGetTopSeqEntryForEntity (entityID);

  /* make top SeqEntry if only Bioseq or BioseqSet was read */

  if (sep == NULL) {
    omdp = ObjMgrGetData (entityID);
    if (omdp != NULL) {
      if (omdp->datatype == OBJ_BIOSEQ || omdp->datatype == OBJ_BIOSEQSET) {
        sep = SeqEntryNew ();
        if (sep != NULL) {
          if (omdp->datatype == OBJ_BIOSEQ) {
            sep->choice = 1;
            sep->data.ptrvalue = omdp->dataptr;
            SeqMgrSeqEntry (SM_BIOSEQ, omdp->dataptr, sep);
          } else {
            sep->choice = 2;
            sep->data.ptrvalue = omdp->dataptr;
            SeqMgrSeqEntry (SM_BIOSEQSET, omdp->dataptr, sep);
          }
        }
        sep = GetTopSeqEntryForEntityID (entityID);
      }
    }
  }

  if (sep == NULL) return 0;

  /* clean up many old-style ASN.1 problems without changing structure */

  BasicSeqEntryCleanup (sep);

  /* do the same cleanup to remotely fetched feature tables */

  for (vnp = extra; vnp != NULL; vnp = vnp->next) {
    bsp = (BioseqPtr) vnp->data.ptrvalue;
    if (bsp == NULL) continue;
    for (sap = bsp->annot; sap != NULL; sap = sap->next) {
      if (sap->type != 1) continue;
      for (sfp = (SeqFeatPtr) sap->data; sfp != NULL; sfp = sfp->next) {
        publist = NULL;
        CleanUpSeqFeat (sfp, FALSE, FALSE, TRUE, TRUE, &publist);
        ValNodeFreeData (publist);
      }
    }
  }

  /* set gather/objmgr fields now present in several objects */

  AssignIDsInEntityEx (entityID, 0, NULL, extra);

  /* get first feature itemID in remote feature tables (including generated gaps) */

  for (vnp = extra; vnp != NULL && baseItemID == 0; vnp = vnp->next) {
    bsp = (BioseqPtr) vnp->data.ptrvalue;
    if (bsp == NULL) continue;
    for (sap = bsp->annot; sap != NULL && baseItemID == 0; sap = sap->next) {
      if (sap->type != 1) continue;
      for (sfp = (SeqFeatPtr) sap->data; sfp != NULL && baseItemID == 0; sfp = sfp->next) {
        baseItemID = sfp->idx.itemID;
      }
    }
  }

  /* set scope for FindAppropriateBioseq, FindFirstLocalBioseq */

  oldscope = SeqEntrySetScope (sep);

  /* gather all segmented locations */

  exind.topsep = sep;
  exind.lastbsp = NULL;
  exind.lastsap = NULL;
  exind.lastbssp = NULL;
  exind.alignhead = NULL;
  exind.lastalign = NULL;
  exind.adphead = NULL;
  exind.lastadp = NULL;
  exind.segpartail = NULL;
  exind.bspcount = 0;
  exind.aligncount = 0;
  exind.descrcount = 0;
  exind.featcount = 0;
  exind.adpcount = 0;
  exind.seqlitid = 0;
  exind.flip = flip;

  MemSet ((Pointer) objMgrFilter, 0, sizeof (objMgrFilter));
  objMgrFilter [OBJ_BIOSEQ] = TRUE;
  /* objMgrFilter [OBJ_BIOSEQSET] = TRUE not needed */
  objMgrFilter [OBJ_BIOSEQ_SEG] = TRUE;
  objMgrFilter [OBJ_BIOSEQ_DELTA] = TRUE;
  GatherObjectsInEntity (entityID, 0, NULL, RecordSegmentsInBioseqs, (Pointer) &exind, objMgrFilter);

  /* build indexes to speed mapping of parts to segmented bioseq */

  lastsegbsp = NULL;

  IndexSegmentedParts (sep, &lastsegbsp);

  /* now gather to get descriptor itemID counts on each bioseq or bioseq set,
     and record features on the bioseq indicated by the feature location */

  exind.topsep = sep;
  exind.lastbsp = NULL;
  exind.lastsap = NULL;
  exind.lastbssp = NULL;
  exind.alignhead = NULL;
  exind.lastalign = NULL;
  exind.adphead = NULL;
  exind.lastadp = NULL;
  exind.segpartail = NULL;
  exind.bspcount = 0;
  exind.aligncount = 0;
  exind.descrcount = 0;
  exind.featcount = 0;
  exind.adpcount = 0;
  exind.seqlitid = 0;
  exind.flip = flip;

  MemSet ((Pointer) objMgrFilter, 0, sizeof (objMgrFilter));
  objMgrFilter [OBJ_BIOSEQ] = TRUE;
  objMgrFilter [OBJ_BIOSEQSET] = TRUE;
  objMgrFilter [OBJ_SEQANNOT] = TRUE;
  objMgrFilter [OBJ_ANNOTDESC] = TRUE;
  objMgrFilter [OBJ_SEQFEAT] = TRUE;
  objMgrFilter [OBJ_SEQALIGN] = TRUE;
  GatherObjectsInEntityEx (entityID, 0, NULL, RecordFeaturesInBioseqs, (Pointer) &exind, objMgrFilter, extra);

  /* finish building array of sorted features on each indexed bioseq */

  IndexRecordedFeatures (sep, dorevfeats, baseItemID);

  /* set best protein feature for segmented protein bioseqs and their parts */

  VisitBioseqsInSep (sep, NULL, DoSegmentedProtein);

  /* resetset scope used to limit FindAppropriateBioseq, FindFirstLocalBioseq */

  SeqEntrySetScope (oldscope);

  /* stamp top of entity with time of indexing */

  omdp = ObjMgrGetData (entityID);
  if (omdp != NULL) {
    omdp->indexed = GetSecs ();

    /* alignment ID to SeqAlignPtr index always goes on top of entity */

    SeqMgrIndexAlignments (entityID);

    /* master indexes if top of entity is not a Bioseq */

    if (omdp->datatype != OBJ_BIOSEQ) {

      CreateBioseqExtraBlock (omdp, NULL);
      bspextra = (BioseqExtraPtr) omdp->extradata;
      if (bspextra != NULL) {

        /* make master index of features by itemID at top of entity */

        if (exind.featcount > 0) {
          featsByID = (SMFeatItemPtr PNTR) MemNew (sizeof (SMFeatItemPtr) * (exind.featcount + 1));
          if (featsByID != NULL) {
            count = 0;
            IndexFeaturesOnEntity (sep, featsByID, &count);

            /* sort all features on entity-wide list by itemID */

            StableMergeSort ((VoidPtr) featsByID, (size_t) count, sizeof (SMFeatItemPtr), SortFeatItemListByID);

            bspextra->featsByID = featsByID;
            bspextra->numfeats = count;
          }
        }

       /* make master index of annot descs by itemID at top of entity */

        if (exind.adpcount > 0) {
          annotDescByID = (AnnotDescPtr PNTR) MemNew (sizeof (AnnotDescPtr) * (exind.adpcount + 1));
          if (annotDescByID != NULL) {
            for (vnp = exind.adphead, count = 0; vnp != NULL && count < (Int4) exind.adpcount; vnp = vnp->next, count++) {
              abp = (AdpBspPtr) vnp->data.ptrvalue;
              if (abp == NULL) continue;
              annotDescByID [count] = abp->adp;
            }

            bspextra->annotDescByID = annotDescByID;
            bspextra->numannotdesc = exind.adpcount;
          }
        }
      }
    }

    /* add feature ID indexto top of entity */

    CreateBioseqExtraBlock (omdp, NULL);
    bspextra = (BioseqExtraPtr) omdp->extradata;
    if (bspextra != NULL) {
      IndexFeatIDsOnEntity (bspextra);
    }
  }

  /* finish indexing list of descriptors on each indexed bioseq */

  VisitBioseqsInSep (sep, NULL, RecordDescriptorsInBioseqs);

  /* index annot descs on each target bioseq */

  if (exind.adphead != NULL) {
    exind.adphead = ValNodeSort (exind.adphead, SortAbpVnpByBsp);
    IndexAnnotDescsOnBioseqs (exind.adphead);
  }

  if (IS_Bioseq_set (sep)) {
    RecordDescriptorsOnTopSet (sep);
  }

  SeqEntryExplore (sep, NULL, IndexRecordedDescriptors);

  /* free chain of SeqAlignPtr now that index is built */

  ValNodeFree (exind.alignhead);

  /* free chain of AdpBspPtr (AnnotDescPtr and BioseqPtr) now that index is built */

  ValNodeFreeData (exind.adphead);

  return entityID;
}

static TNlmMutex  smp_feat_index_mutex = NULL;

NLM_EXTERN Uint2 LIBCALL SeqMgrIndexFeaturesExEx (
  Uint2 entityID,
  Pointer ptr,
  Boolean flip,
  Boolean dorevfeats,
  ValNodePtr extra
)

{
  Uint2  eID;
  Int4   ret;

  ret = NlmMutexLockEx (&smp_feat_index_mutex);
  if (ret) {
    ErrPostEx (SEV_FATAL, 0, 0, "SeqMgrIndexFeatures mutex failed [%ld]", (long) ret);
    return 0;
  }

  eID = s_DoSeqMgrIndexFeatures (entityID, ptr, flip, dorevfeats, extra);

  NlmMutexUnlock (smp_feat_index_mutex);

  return eID;
}

NLM_EXTERN Uint2 LIBCALL SeqMgrIndexFeaturesEx (
  Uint2 entityID,
  Pointer ptr,
  Boolean flip,
  Boolean dorevfeats
)

{
  return SeqMgrIndexFeaturesExEx (entityID, ptr, flip, dorevfeats, NULL);
}

NLM_EXTERN Uint2 LIBCALL SeqMgrIndexFeatures (
  Uint2 entityID,
  Pointer ptr
)

{
  return SeqMgrIndexFeaturesExEx (entityID, ptr, FALSE, FALSE, NULL);
}

/*****************************************************************************
*
*   SeqMgrIsBioseqIndexed checks for presence of time of indexing stamp
*
*****************************************************************************/

NLM_EXTERN time_t LIBCALL SeqMgrFeaturesAreIndexed (Uint2 entityID)

{
  ObjMgrDataPtr  omdp;

  if (entityID == 0) return 0;
  omdp = ObjMgrGetData (entityID);
  if (omdp == NULL) return 0;
  return omdp->indexed;
}

/*****************************************************************************
*
*   SeqMgrGetBestProteinFeature and SeqMgrGetCDSgivenProduct take a protein
*     bioseq to get the best protein feature or encoding CDS
*   SeqMgrGetRNAgivenProduct takes an mRNA (cDNA) bioseq and gets encoding mRNA
*     feature on the genomic bioseq
*
*****************************************************************************/

NLM_EXTERN ProtRefPtr LIBCALL SeqMgrGetProtXref (SeqFeatPtr sfp)

{
  ProtRefPtr      prp = NULL;
  SeqFeatXrefPtr  xref;

  if (sfp == NULL) return NULL;
  xref = sfp->xref;
  while (xref != NULL && xref->data.choice != SEQFEAT_PROT) {
    xref = xref->next;
  }
  if (xref != NULL) {
    prp = (ProtRefPtr) xref->data.value.ptrvalue;
  }
  return prp;
}

static void SetContextForFeature (SeqFeatPtr sfp, SeqMgrFeatContext PNTR context, ObjMgrDataPtr omdp)

{
  SMFeatItemPtr  best;
  SeqFeatPtr     bst;

  if (sfp == NULL || context == NULL || omdp == NULL) return;
  best = SeqMgrFindSMFeatItemPtr (sfp);
  if (best == NULL) return;
  bst = best->sfp;
  if (bst != NULL && bst->idx.entityID > 0) {
    context->entityID = bst->idx.entityID;
  } else {
    context->entityID = ObjMgrGetEntityIDForPointer (omdp->dataptr);
  }
  context->itemID = best->itemID;
  context->sfp = bst;
  context->sap = best->sap;
  context->bsp = best->bsp;
  context->label = best->label;
  context->left = best->left;
  context->right = best->right;
  context->dnaStop = best->dnaStop;
  context->partialL = best->partialL;
  context->partialR = best->partialR;
  context->farloc = best->farloc;
  context->bad_order = best->bad_order;
  context->mixed_strand = best->mixed_strand;
  context->ts_image = best->ts_image;
  context->strand = best->strand;
  if (bst != NULL) {
    context->seqfeattype = bst->data.choice;
  } else {
    context->seqfeattype = FindFeatFromFeatDefType (best->subtype);
  }
  context->featdeftype = best->subtype;
  context->numivals = best->numivals;
  context->ivals = best->ivals;
  context->userdata = NULL;
  context->omdp = (Pointer) omdp;
  context->index = best->index + 1;
}

NLM_EXTERN SeqFeatPtr LIBCALL SeqMgrGetBestProteinFeature (BioseqPtr bsp,
                                                           SeqMgrFeatContext PNTR context)

{
  BioseqExtraPtr  bspextra;
  ObjMgrDataPtr   omdp;

  if (context != NULL) {
    MemSet ((Pointer) context, 0, sizeof (SeqMgrFeatContext));
  }
  omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return NULL;
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return NULL;
  SetContextForFeature (bspextra->protFeat, context, omdp);
  return bspextra->protFeat;
}

NLM_EXTERN SeqFeatPtr LIBCALL SeqMgrGetCDSgivenProduct (BioseqPtr bsp,
                                                        SeqMgrFeatContext PNTR context)

{
  BioseqExtraPtr  bspextra;
  ObjMgrDataPtr   omdp;
  SeqFeatPtr      sfp;

  if (context != NULL) {
    MemSet ((Pointer) context, 0, sizeof (SeqMgrFeatContext));
  }
  omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return NULL;
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return NULL;
  sfp = bspextra->cdsOrRnaFeat;
  if (sfp == NULL || sfp->data.choice != SEQFEAT_CDREGION) return NULL;
  SetContextForFeature (sfp, context, omdp);
  return sfp;
}

NLM_EXTERN SeqFeatPtr LIBCALL SeqMgrGetRNAgivenProduct (BioseqPtr bsp,
                                                        SeqMgrFeatContext PNTR context)

{
  BioseqExtraPtr  bspextra;
  ObjMgrDataPtr   omdp;
  SeqFeatPtr      sfp;

  if (context != NULL) {
    MemSet ((Pointer) context, 0, sizeof (SeqMgrFeatContext));
  }
  omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return NULL;
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return NULL;
  sfp = bspextra->cdsOrRnaFeat;
  if (sfp == NULL || sfp->data.choice != SEQFEAT_RNA) return NULL;
  SetContextForFeature (sfp, context, omdp);
  return sfp;
}

NLM_EXTERN SeqFeatPtr LIBCALL SeqMgrGetPROTgivenProduct (BioseqPtr bsp,
                                                         SeqMgrFeatContext PNTR context)

{
  BioseqExtraPtr  bspextra;
  ObjMgrDataPtr   omdp;
  SeqFeatPtr      sfp;

  if (context != NULL) {
    MemSet ((Pointer) context, 0, sizeof (SeqMgrFeatContext));
  }
  omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return NULL;
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return NULL;
  sfp = bspextra->cdsOrRnaFeat;
  if (sfp == NULL || sfp->data.choice != SEQFEAT_PROT) return NULL;
  SetContextForFeature (sfp, context, omdp);
  return sfp;
}

NLM_EXTERN ValNodePtr LIBCALL SeqMgrGetSfpProductList (BioseqPtr bsp)

{
  BioseqExtraPtr  bspextra;
  ObjMgrDataPtr   omdp;

  omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return NULL;
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return NULL;
  return bspextra->prodlisthead;
}

/*****************************************************************************
*
*   SeqMgrGetGeneXref, SeqMgrGeneIsSuppressed, SeqMgrGetProtXref,
*     SeqMgrGetOverlappingGene, and SeqMgrGetOverlappingPub
*
*****************************************************************************/

static Boolean HasNoText (CharPtr str)

{
  Char  ch;

  if (str != NULL) {
    ch = *str;
    while (ch != '\0') {
      if (ch > ' ') {
        return FALSE;
      }
      str++;
      ch = *str;
    }
  }
  return TRUE;
}

NLM_EXTERN GeneRefPtr LIBCALL SeqMgrGetGeneXref (SeqFeatPtr sfp)

{
  GeneRefPtr      grp = NULL;
  SeqFeatXrefPtr  xref;

  if (sfp == NULL) return NULL;
  xref = sfp->xref;
  while (xref != NULL && xref->data.choice != SEQFEAT_GENE) {
    xref = xref->next;
  }
  if (xref != NULL) {
    grp = (GeneRefPtr) xref->data.value.ptrvalue;
  }
  return grp;
}

NLM_EXTERN GeneRefPtr LIBCALL SeqMgrGetGeneXrefEx (SeqFeatPtr sfp, ObjectIdPtr PNTR oipP)

{
  GeneRefPtr      grp = NULL;
  ObjectIdPtr     oip;
  SeqFeatXrefPtr  xref;

  if (oipP != NULL) {
    *oipP = NULL;
  }
  if (sfp == NULL) return NULL;
  xref = sfp->xref;
  while (xref != NULL && xref->data.choice != SEQFEAT_GENE) {
    xref = xref->next;
  }
  if (xref != NULL) {
    grp = (GeneRefPtr) xref->data.value.ptrvalue;
    if (xref->id.choice == 3) {
      oip = (ObjectIdPtr) xref->id.value.ptrvalue;
      if (oip != NULL && oipP != NULL) {
        *oipP = oip;
      }
    }
  }
  return grp;
}

NLM_EXTERN Boolean LIBCALL SeqMgrGeneIsSuppressed (GeneRefPtr grp)

{
  if (grp == NULL) return FALSE;
  if (grp != NULL && HasNoText (grp->locus) && HasNoText (grp->allele) &&
      HasNoText (grp->desc) && HasNoText (grp->maploc) &&
      HasNoText (grp->locus_tag) && grp->db == NULL &&
      grp->syn == NULL) return TRUE;
  return FALSE;
}

static Boolean CheckInternalExonBoundaries (Int2 numivalsA, Int4Ptr ivalsA, Int2 numivalsB, Int4Ptr ivalsB)

{
  Int2  i;
  Int2  j;

  if (numivalsA > numivalsB) return FALSE;
  if (ivalsA == NULL || ivalsB == NULL) return TRUE;

  /* scan first exon-intron boundary against candidate start positions */

  for (i = 0; i <= numivalsB - numivalsA; i++) {
    if (ivalsA [1] == ivalsB [2 * i + 1]) break;
  }
  if (i > numivalsB - numivalsA) return FALSE;

  /* scan subsequent exon-intron and intron-exon boundaries */

  for (j = 2; j <= 2 * numivalsA - 2; j++) {
    if (ivalsA [j] != ivalsB [2 * i + j]) return FALSE;
  }

  return TRUE;
}

static Boolean StrandsMatch (Uint1 featstrand, Uint1 locstrand)

{
  if (featstrand == locstrand) return TRUE;
  if (locstrand == Seq_strand_unknown && featstrand != Seq_strand_minus) return TRUE;
  if (featstrand == Seq_strand_unknown && locstrand != Seq_strand_minus) return TRUE;
  if (featstrand == Seq_strand_both && locstrand != Seq_strand_minus) return TRUE;
  if (locstrand == Seq_strand_both) return TRUE;
  return FALSE;
}

static Int4 TestForOverlap (SMFeatItemPtr feat, SeqLocPtr slp,
                            Int4 left, Int4 right, Int2 overlapType,
                            Int2 numivals, Int4Ptr ivals)

{
  SeqLocPtr   a, b;
  Int4        diff;
  SeqFeatPtr  sfp;

  if (overlapType == SIMPLE_OVERLAP) {

    /* location must merely be overlapped by gene, etc., or either one inside the other */

    if (feat->right >= left && feat->left <= right) {
      diff = ABS (left - feat->left) + ABS (feat->right - right);
      return diff;
    }

    /*
    if ((feat->left <= left && feat->right > left) ||
        (feat->left < right && feat->right >= right)) {
      diff = ABS (left - feat->left) + ABS (feat->right - right);
      return diff;
    }
    */

  } else if (overlapType == CONTAINED_WITHIN) {

    /* requires location to be completely contained within gene, etc. */

    if (feat->left <= left && feat->right >= right) {
      diff = (left - feat->left) + (feat->right - right);
      return diff;
    }

  } else if (overlapType == LOCATION_SUBSET || overlapType == CHECK_INTERVALS) {

    /* requires individual intervals to be completely contained within gene, etc. */
    sfp = feat->sfp;
    if (sfp != NULL) {
      diff = SeqLocAinB (slp, sfp->location);
      if (diff >= 0) {
        if (overlapType == LOCATION_SUBSET || numivals == 1 ||
            CheckInternalExonBoundaries (numivals, ivals, feat->numivals, feat->ivals)) {
          return diff;
        }
      }
    }

  } else if (overlapType == INTERVAL_OVERLAP || overlapType == COMMON_INTERVAL) {

    /* requires overlap between at least one pair of intervals (INTERVAL_OVERLAP) */
    /* or one complete shared interval (COMMON_INTERVAL) */

    if (feat->right >= left && feat->left <= right) {
      sfp = feat->sfp;
      if (sfp != NULL) {
        a = SeqLocFindNext (slp, NULL);
        while (a != NULL) {
          b = SeqLocFindNext (sfp->location, NULL);
          while (b != NULL) {
            if ((overlapType == INTERVAL_OVERLAP
                && SeqLocCompare (a, b) != SLC_NO_MATCH) 
              || (overlapType == COMMON_INTERVAL
                && SeqLocCompare (a, b) == SLC_A_EQ_B))
            {
              diff = ABS (left - feat->left) + ABS (feat->right - right);
              return diff;
            }
            b = SeqLocFindNext (sfp->location, b);
          }
          a = SeqLocFindNext (slp, a);
        }
      }
    }
  }
  else if (overlapType == RANGE_MATCH)
  {
      /* left and right ends must match exactly */
      if (feat->right == right && feat->left == left)
      {
        return 0;
      }
  }

  return -1;
}

static void SeqMgrBestOverlapSetContext (
  SMFeatItemPtr best,
  ObjMgrDataPtr omdp,
  Pointer userdata,
  SeqMgrFeatContext PNTR context
)

{
  SeqFeatPtr  bst;

  if (best != NULL && omdp != NULL && context != NULL) {
    bst = best->sfp;
    if (bst != NULL && bst->idx.entityID > 0) {
      context->entityID = bst->idx.entityID;
    } else {
      context->entityID = ObjMgrGetEntityIDForPointer (omdp->dataptr);
    }
    context->itemID = best->itemID;
    context->sfp = best->sfp;
    context->sap = best->sap;
    context->bsp = best->bsp;
    context->label = best->label;
    context->left = best->left;
    context->right = best->right;
    context->dnaStop = best->dnaStop;
    context->partialL = best->partialL;
    context->partialR = best->partialR;
    context->farloc = best->farloc;
    context->bad_order = best->bad_order;
    context->mixed_strand = best->mixed_strand;
    context->ts_image = best->ts_image;
    context->strand = best->strand;
    if (bst != NULL) {
      context->seqfeattype = bst->data.choice;
    } else {
      context->seqfeattype = FindFeatFromFeatDefType (best->subtype);
    }
    context->featdeftype = best->subtype;
    context->numivals = best->numivals;
    context->ivals = best->ivals;
    context->userdata = userdata;
    context->omdp = (Pointer) omdp;
    context->index = best->index + 1;
  }
}

static Boolean TransSplicedStrandsMatch (Uint1 locstrand, SeqLocPtr slp, SMFeatItemPtr feat)

{
  Uint1       featstrand;
  SeqLocPtr   loc;
  SeqFeatPtr  sfp;

  if (slp == NULL || feat == NULL) return FALSE;
  sfp = feat->sfp;
  if (sfp == NULL) return FALSE;

  if (! sfp->excpt) return FALSE;
  if (StringISearch (sfp->except_text, "trans-splicing") == NULL) return FALSE;

  loc = SeqLocFindNext (sfp->location, NULL);
  while (loc != NULL) {
    if (SeqLocAinB (slp, loc) >= 0) {
      featstrand = SeqLocStrand (loc);
      if (StrandsMatch (featstrand, locstrand)) return TRUE;
    }
    loc = SeqLocFindNext (sfp->location, loc);
  }

  return FALSE;
}

static SeqFeatPtr SeqMgrGetBestOverlappingFeat (
  SeqLocPtr slp,
  Uint2 subtype,
  SMFeatItemPtr PNTR array,
  Int4 num,
  Int4Ptr pos,
  Int2 overlapType,
  SeqMgrFeatContext PNTR context,
  Int2Ptr count,
  Pointer userdata,
  SeqMgrFeatExploreProc userfunc,
  Boolean special
)

{
  SMFeatItemPtr   best = NULL;
  BioseqPtr       bsp;
  BioseqExtraPtr  bspextra;
  Int4            diff;
  Uint2           entityID;
  SMFeatItemPtr   feat;
  Int4            from;
  Boolean         goOn = TRUE;
  Int4            hier = -1;
  Int2            i;
  Uint4           index = 0;
  Int4Ptr         ivals = NULL;
  Int4            L;
  Int4            left;
  SeqLocPtr       loc;
  Int4            max = INT4_MAX;
  Boolean         may_be_trans_spliced;
  Int4            mid;
  Int2            numivals = 0;
  SeqEntryPtr     oldscope;
  ObjMgrDataPtr   omdp;
  SMFeatItemPtr   prev;
  Int4            R;
  Int4            right;
  SeqEntryPtr     sep;
  Uint1           strand;
  Int4            swap;
  SeqLocPtr       tmp;
  Int4            to;

  if (context != NULL) {
    MemSet ((Pointer) context, 0, sizeof (SeqMgrFeatContext));
  }
  if (pos != NULL) {
    *pos = 0;
  }
  if (count != NULL) {
    *count = 0;
  }
  if (slp == NULL) return NULL;

  bsp = FindAppropriateBioseq (slp, NULL, NULL);
  if (bsp == NULL) {
    bsp = FindFirstLocalBioseq (slp);
  }
  if (bsp == NULL) return NULL;
  omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return NULL;

  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return NULL;

  switch (subtype) {
    case FEATDEF_GENE :
      array = bspextra->genesByPos;
      num = bspextra->numgenes;
      break;
    case FEATDEF_CDS :
      array = bspextra->CDSsByPos;
      num = bspextra->numCDSs;
      break;
    case FEATDEF_mRNA :
      array = bspextra->mRNAsByPos;
      num = bspextra->nummRNAs;
      break;
    case FEATDEF_PUB :
      array = bspextra->pubsByPos;
      num = bspextra->numpubs;
      break;
    case FEATDEF_BIOSRC :
      array = bspextra->orgsByPos;
      num = bspextra->numorgs;
      break;
      case FEATDEF_operon :
      array = bspextra->operonsByPos;
      num = bspextra->numoperons;
    default :
      break;
  }

  if (array == NULL || num < 1) return NULL;

  entityID = bsp->idx.entityID;
  if (entityID < 1) {
    entityID = ObjMgrGetEntityIDForPointer (bsp);
  }
  sep = SeqMgrGetTopSeqEntryForEntity (entityID);
  oldscope = SeqEntrySetScope (sep);

  left = GetOffsetInNearBioseq (slp, bsp, SEQLOC_LEFT_END);
  right = GetOffsetInNearBioseq (slp, bsp, SEQLOC_RIGHT_END);

  SeqEntrySetScope (oldscope);

  if (left == -1 || right == -1) return NULL;

  /* if feature spans origin, normalize with left < 0 */

  if (left > right && bsp->topology == TOPOLOGY_CIRCULAR) {
    left -= bsp->length;
  }

  /* some trans-spliced locations can confound GetOffsetInNearBioseq, so normalize here */

  if (left > right) {
    swap = left;
    left = right;
    right = swap;
  }

  /* binary search to leftmost candidate within the xxxByPos array */

  L = 0;
  R = num - 1;
  while (L < R) {
    mid = (L + R) / 2;
    feat = array [mid];
    if (feat != NULL && feat->right < left) {
      L = mid + 1;
    } else {
      R = mid;
    }
  }

  feat = array [R];

  if (feat != NULL && feat->left > left && R > 0) {

    /* if hit is already past location, location was in between local hits */

    prev = array [R - 1];
    if (prev != NULL && prev->overlap != -1) {

      /* backup R by one to check appropriate overlap hierarchy */

      R--;
      feat = array [R];
    }
  }

  if (feat != NULL) {
    hier = feat->overlap;
  }

  loc = SeqLocMergeExEx (bsp, slp, NULL, FALSE, /* TRUE */ FALSE, FALSE, FALSE, TRUE, TRUE);
  strand = SeqLocStrand (loc);
  if (overlapType == CHECK_INTERVALS) {
    tmp = NULL;
    while ((tmp = SeqLocFindNext (loc, tmp)) != NULL) {
      numivals++;
    }
    if (numivals > 0) {
      ivals = MemNew (sizeof (Int4) * (numivals * 2));
      if (ivals != NULL) {
        tmp = NULL;
        i = 0;
        while ((tmp = SeqLocFindNext (loc, tmp)) != NULL) {
          from = SeqLocStart (tmp);
          to = SeqLocStop (tmp);
          if (strand == Seq_strand_minus) {
            swap = from;
            from = to;
            to = swap;
          }
          ivals [i] = from;
          i++;
          ivals [i] = to;
          i++;
        }
      }
    }
  }
  SeqLocFree (loc);

  /* linear scan to smallest covering gene, publication, biosource, etc. */

  while (R < num && feat != NULL && feat->left <= right) {

    if ((! feat->ignore) || userfunc == NULL) {

      /* requires feature to be contained within gene, etc. */

      may_be_trans_spliced = (Boolean) (special && (feat->bad_order || feat->mixed_strand));
      if (may_be_trans_spliced) {
        diff = TestForOverlap (feat, slp, left, right, LOCATION_SUBSET, numivals, ivals);
      } else {
        diff = TestForOverlap (feat, slp, left, right, overlapType, numivals, ivals);
      }
      if (diff >= 0) {

        if (StrandsMatch (feat->strand, strand) || (may_be_trans_spliced && TransSplicedStrandsMatch (strand, slp, feat))) {

          if (userfunc != NULL && context != NULL && goOn) {
            SeqMgrBestOverlapSetContext (feat, omdp, userdata, context);
            if (! userfunc (feat->sfp, context)) {
              goOn = FALSE;
            }
            if (count != NULL) {
              (*count)++;
            }
          }

          /* diff = (left - feat->left) + (feat->right - right); */
          /* Don't need to check ties because in this loop we always hit the leftmost first */
          if ( diff < max )
          {
            best = feat;
            index = R;
            max = diff;
          }
        }
      }
    }
    R++;
    feat = array [R];
  }

  /* also will go up gene overlap hierarchy pointers from original R hit */

  while (hier != -1) {

    feat = array [hier];
    if (feat != NULL && ((! feat->ignore) || userfunc == NULL)) {

      may_be_trans_spliced = (Boolean) (special && (feat->bad_order || feat->mixed_strand));
      if (may_be_trans_spliced) {
        diff = TestForOverlap (feat, slp, left, right, LOCATION_SUBSET, numivals, ivals);
      } else {
        diff = TestForOverlap (feat, slp, left, right, overlapType, numivals, ivals);
      }
      if (diff >= 0) {

        if (StrandsMatch (feat->strand, strand) || (may_be_trans_spliced && TransSplicedStrandsMatch (strand, slp, feat))) {

          if (userfunc != NULL && context != NULL && goOn) {
            SeqMgrBestOverlapSetContext (feat, omdp, userdata, context);
            if (! userfunc (feat->sfp, context)) {
              goOn = FALSE;
            }
            if (count != NULL) {
              (*count)++;
            }
          }

          /* diff = (left - feat->left) + (feat->right - right); */
          /* For ties, first wins */
          if (diff < max || ( diff == max && hier < index )) {
            best = feat;
            index = hier;
            max = diff;
          }
        }
      }
      hier = feat->overlap;
    } else {
      hier = -1;
    }
  }

  if (ivals != NULL) {
    ivals = MemFree (ivals);
  }

  if (best != NULL) {
    if (pos != NULL) {
      *pos = index + 1;
    }
    if (context != NULL) {
      SeqMgrBestOverlapSetContext (best, omdp, userdata, context);
    }
    return best->sfp;
  }

  return NULL;
}

NLM_EXTERN Int4 TestFeatOverlap (SeqFeatPtr sfpA, SeqFeatPtr sfpB, Int2 overlapType)

{
  Int4           diff;
  SMFeatItemPtr  sfipA, sfipB;

  if (sfpA == NULL || sfpB == NULL) return -1;
  sfipA = SeqMgrFindSMFeatItemPtr (sfpA);
  sfipB = SeqMgrFindSMFeatItemPtr (sfpB);
  if (sfipA == NULL || sfipB == NULL) return -1;

  diff = TestForOverlap (sfipB, sfpA->location, sfipA->left, sfipA->right,
                         overlapType, sfipA->numivals, sfipA->ivals);
  if (diff < 0) return -1;

  if (StrandsMatch (sfipB->strand, sfipA->strand)) {
    return diff;
  }

  return -1;
}

NLM_EXTERN SeqFeatPtr LIBCALL SeqMgrGetOverlappingGene (SeqLocPtr slp, SeqMgrFeatContext PNTR context)

{
  return SeqMgrGetBestOverlappingFeat (slp, FEATDEF_GENE, NULL, 0, NULL, CONTAINED_WITHIN, context, NULL, NULL, NULL, TRUE);
}

NLM_EXTERN SeqFeatPtr LIBCALL SeqMgrGetOverlappingmRNA (SeqLocPtr slp, SeqMgrFeatContext PNTR context)

{
  return SeqMgrGetBestOverlappingFeat (slp, FEATDEF_mRNA, NULL, 0, NULL, CONTAINED_WITHIN, context, NULL, NULL, NULL, FALSE);
}

NLM_EXTERN SeqFeatPtr LIBCALL SeqMgrGetLocationSupersetmRNA (SeqLocPtr slp, SeqMgrFeatContext PNTR context)

{
  return SeqMgrGetBestOverlappingFeat (slp, FEATDEF_mRNA, NULL, 0, NULL, LOCATION_SUBSET, context, NULL, NULL, NULL, FALSE);
}

NLM_EXTERN SeqFeatPtr LIBCALL SeqMgrGetOverlappingCDS (SeqLocPtr slp, SeqMgrFeatContext PNTR context)

{
  return SeqMgrGetBestOverlappingFeat (slp, FEATDEF_CDS, NULL, 0, NULL, CONTAINED_WITHIN, context, NULL, NULL, NULL, FALSE);
}

NLM_EXTERN SeqFeatPtr LIBCALL SeqMgrGetOverlappingPub (SeqLocPtr slp, SeqMgrFeatContext PNTR context)

{
  return SeqMgrGetBestOverlappingFeat (slp, FEATDEF_PUB, NULL, 0, NULL, CONTAINED_WITHIN, context, NULL, NULL, NULL, FALSE);
}

NLM_EXTERN SeqFeatPtr LIBCALL SeqMgrGetOverlappingSource (SeqLocPtr slp, SeqMgrFeatContext PNTR context)

{
  return SeqMgrGetBestOverlappingFeat (slp, FEATDEF_BIOSRC, NULL, 0, NULL, CONTAINED_WITHIN, context, NULL, NULL, NULL, FALSE);
}

NLM_EXTERN SeqFeatPtr LIBCALL SeqMgrGetOverlappingOperon (SeqLocPtr slp, SeqMgrFeatContext PNTR context)

{
  return SeqMgrGetBestOverlappingFeat (slp, FEATDEF_operon, NULL, 0, NULL, CONTAINED_WITHIN, context, NULL, NULL, NULL, FALSE);
}

/*****************************************************************************
*
*   SeqMgrGetFeatureByLabel returns a feature with the desired label
*   If desired, place a SeqMgrFeatContext data structure on the stack, and pass
*     in &context as the last parameter
*
*****************************************************************************/

static CharPtr GetLabelOrLocusTag (SMFeatItemPtr feat, Boolean byLocusTag)

{
  GeneRefPtr  grp;
  SeqFeatPtr  sfp;

  if (feat == NULL) return NULL;
  if (byLocusTag) {
    sfp = feat->sfp;
    if (sfp == NULL || sfp->data.choice != SEQFEAT_GENE) return NULL;
    grp = (GeneRefPtr) sfp->data.value.ptrvalue;
    if (grp == NULL) return NULL;
    return grp->locus_tag;
  }
  return feat->label;
}

static SeqFeatPtr LIBCALL SeqMgrGetFeatureByLabelEx (BioseqPtr bsp, CharPtr label,
                                                     Uint1 seqFeatChoice, Uint1 featDefChoice,
                                                     Boolean byLocusTag, SeqMgrFeatContext PNTR context)

{
  SMFeatItemPtr PNTR  array;
  BioseqExtraPtr      bspextra;
  Uint2               entityID;
  SMFeatItemPtr       feat;
  Int4                L;
  Int4                mid;
  Int4                num;
  ObjMgrDataPtr       omdp;
  Int4                R;
  Uint1               seqfeattype;
  SeqFeatPtr          sfp;

  if (context != NULL) {
    MemSet ((Pointer) context, 0, sizeof (SeqMgrFeatContext));
  }

  if (bsp == NULL || StringHasNoText (label)) return NULL;

  omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return NULL;

  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return NULL;
  if (byLocusTag) {
    array = bspextra->genesByLocusTag;
    num = bspextra->numgenes;
  } else {
    array = bspextra->featsByLabel;
    num = bspextra->numfeats;
  }
  if (array == NULL || num < 1) return NULL;

  entityID = bsp->idx.entityID;
  if (entityID < 1) {
    entityID = ObjMgrGetEntityIDForPointer (omdp->dataptr);
  }

  /* binary search to leftmost candidate within the featsByLabel array */

  L = 0;
  R = num - 1;
  while (L < R) {
    mid = (L + R) / 2;
    feat = array [mid];
    if (feat != NULL && StringICmp (GetLabelOrLocusTag (feat, byLocusTag), label) < 0) {
      L = mid + 1;
    } else {
      R = mid;
    }
  }

  feat = array [R];

  /* linear scan to find desired label on desired feature type */

  while (R < num && feat != NULL && StringICmp (GetLabelOrLocusTag (feat, byLocusTag), label) == 0) {
    sfp = feat->sfp;
    if (sfp != NULL) {
      seqfeattype = sfp->data.choice;
      if ((seqFeatChoice == 0 || seqfeattype == seqFeatChoice) &&
          (featDefChoice == 0 || feat->subtype == featDefChoice) &&
          (! feat->ignore)) {
        if (context != NULL) {
          context->entityID = entityID;
          context->itemID = feat->itemID;
          context->sfp = sfp;
          context->sap = feat->sap;
          context->bsp = feat->bsp;
          context->label = GetLabelOrLocusTag (feat, byLocusTag);
          context->left = feat->left;
          context->right = feat->right;
          context->dnaStop = feat->dnaStop;
          context->partialL = feat->partialL;
          context->partialR = feat->partialR;
          context->farloc = feat->farloc;
          context->bad_order = feat->bad_order;
          context->mixed_strand = feat->mixed_strand;
          context->ts_image = feat->ts_image;
          context->strand = feat->strand;
          context->seqfeattype = seqfeattype;
          context->featdeftype = feat->subtype;
          context->numivals = feat->numivals;
          context->ivals = feat->ivals;
          context->userdata = NULL;
          context->omdp = (Pointer) omdp;
          context->index = R + 1;
        }
        return sfp;
      }
    }

    R++;
    feat = array [R];
  }

  return NULL;
}

NLM_EXTERN SeqFeatPtr LIBCALL SeqMgrGetFeatureByLabel (BioseqPtr bsp, CharPtr label,
                                                       Uint1 seqFeatChoice, Uint1 featDefChoice,
                                                       SeqMgrFeatContext PNTR context)

{
  return SeqMgrGetFeatureByLabelEx (bsp, label, seqFeatChoice, featDefChoice, FALSE, context);
}

NLM_EXTERN SeqFeatPtr LIBCALL SeqMgrGetGeneByLocusTag (BioseqPtr bsp, CharPtr locusTag,
                                                       SeqMgrFeatContext PNTR context)

{
  return SeqMgrGetFeatureByLabelEx (bsp, locusTag, SEQFEAT_GENE, 0, TRUE, context);
}

NLM_EXTERN SeqFeatPtr LIBCALL SeqMgrGetFeatureByFeatID (
  Uint2 entityID,
  BioseqPtr bsp,
  CharPtr featid,
  SeqFeatXrefPtr xref,
  SeqMgrFeatContext PNTR context
)

{
  SMFidItemPtr PNTR  array;
  BioseqExtraPtr     bspextra;
  Char               buf [32];
  SMFeatItemPtr      feat;
  SMFidItemPtr       item;
  Int4               L;
  Int4               mid;
  Int4               num;
  ObjectIdPtr        oip;
  ObjMgrDataPtr      omdp;
  Int4               R;
  SeqFeatPtr         sfp;

  if (context != NULL) {
    MemSet ((Pointer) context, 0, sizeof (SeqMgrFeatContext));
  }

  if (entityID > 0) {
    omdp = ObjMgrGetData (entityID);
    if (omdp == NULL) return NULL;
  } else {
    if (bsp == NULL) return NULL;
    omdp = SeqMgrGetOmdpForBioseq (bsp);
    if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return NULL;
  }
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return NULL;

  /* first try array sorted by itemID value */

  array = bspextra->featsByFeatID;
  num = bspextra->numfids;
  if (array == NULL || num < 1) return NULL;

  if (StringHasNoText (featid) && xref != NULL && xref->id.choice == 3) {
    oip = (ObjectIdPtr) xref->id.value.ptrvalue;
    if (oip != NULL) {
      if (StringDoesHaveText (oip->str)) {
        featid = oip->str;
      } else {
        sprintf (buf, "%ld", (long) oip->id);
        featid = buf;
      }
    }
  }
  if (StringHasNoText (featid)) return NULL;

  L = 0;
  R = num - 1;
  while (L < R) {
    mid = (L + R) / 2;
    item = array [mid];
    if (item != NULL && StringICmp (item->fid, featid) < 0) {
      L = mid + 1;
    } else {
      R = mid;
    }
  }

  item = array [R];
  if (StringICmp (item->fid, featid) == 0) {
    feat = item->feat;
    if (feat == NULL) return NULL;
    sfp = feat->sfp;
    if (sfp != NULL) {
      if (! feat->ignore) {
        if (context != NULL) {
          context->entityID = entityID;
          context->itemID = feat->itemID;
          context->sfp = sfp;
          context->sap = feat->sap;
          context->bsp = feat->bsp;
          context->label = feat->label;
          context->left = feat->left;
          context->right = feat->right;
          context->dnaStop = feat->dnaStop;
          context->partialL = feat->partialL;
          context->partialR = feat->partialR;
          context->farloc = feat->farloc;
          context->bad_order = feat->bad_order;
          context->mixed_strand = feat->mixed_strand;
          context->ts_image = feat->ts_image;
          context->strand = feat->strand;
          context->seqfeattype = sfp->data.choice;;
          context->featdeftype = feat->subtype;
          context->numivals = feat->numivals;
          context->ivals = feat->ivals;
          context->userdata = NULL;
          context->omdp = (Pointer) omdp;
          context->index = R + 1;
        }
        return sfp;
      }
    }
  }

  return NULL;
}

/*****************************************************************************
*
*   SeqMgrBuildFeatureIndex builds a sorted array index for any feature type
*     (including gene, mRNA, CDS, publication, and biosource built-in arrays)
*   SeqMgrGetOverlappingFeature uses the array, or a feature subtype (chocies
*     are FEATDEF_GENE, FEATDEF_CDS, FEATDEF_mRNA, FEATDEF_PUB, or FEATDEF_BIOSRC)
*     to find feature overlap, requiring either that the location be completely
*     contained within the feature intervals, contained within the feature extreme
*     range, or merely that it be overlapped by the feature, and returns the position
*     in the index
*   SeqMgrGetFeatureInIndex gets an arbitrary feature indexed by the array
*
*****************************************************************************/

NLM_EXTERN VoidPtr LIBCALL SeqMgrBuildFeatureIndex (BioseqPtr bsp, Int4Ptr num,
                                                    Uint1 seqFeatChoice, Uint1 featDefChoice)

{
  SMFeatItemPtr PNTR  array;
  BioseqExtraPtr      bspextra;
  SMFeatItemPtr PNTR  featsByPos;
  Int4                i;
  Int4                j;
  Int4                k;
  SMFeatItemPtr       item;
  Int4                numfeats;
  Int4                numitems;
  SMFeatItemPtr       nxtitem;
  ObjMgrDataPtr       omdp;
  Boolean             overlaps;
  Uint1               seqfeattype;

  if (num != NULL) {
    *num = 0;
  }
  if (bsp == NULL) return NULL;
  omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return NULL;

  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return NULL;

  featsByPos = bspextra->featsByPos;
  numfeats = bspextra->numfeats;
  if (featsByPos == NULL || numfeats < 1) return NULL;

  for (i = 0, numitems = 0; i < numfeats; i++) {
    item = featsByPos [i];
    seqfeattype = FindFeatFromFeatDefType (item->subtype);
    if ((seqFeatChoice == 0 || seqfeattype == seqFeatChoice) &&
        (featDefChoice == 0 || item->subtype == featDefChoice)) {
      numitems++;
    }
  }
  if (numitems < 1) return NULL;

  array = (SMFeatItemPtr PNTR) MemNew (sizeof (SMFeatItemPtr) * (numitems + 1));
  if (array == NULL) return NULL;

  i = 0;
  j = 0;
  while (i < numfeats && j < numitems) {
    item = featsByPos [i];
    seqfeattype = FindFeatFromFeatDefType (item->subtype);
    if ((seqFeatChoice == 0 || seqfeattype == seqFeatChoice) &&
        (featDefChoice == 0 || item->subtype == featDefChoice)) {
      array [j] = item;
      j++;
    }
    i++;
  }

  if (num != NULL) {
    *num = numitems;
  }

  for (j = 0; j < numitems - 1; j++) { 
      item = array [j];
      for (k = j + 1, overlaps = TRUE; k < numitems && overlaps; k++) {
          nxtitem = array [k];
          if ((item->left <= nxtitem->left && item->right > nxtitem->left) ||
              (item->left < nxtitem->right && item->right >= nxtitem->right)) {
 
              /* after binary search, also go up the hierarchy chain to avoid traps */
 
              nxtitem->overlap = j;
          } else {
              overlaps = FALSE;
          }
      }
  }

  return (VoidPtr) array;
}

NLM_EXTERN SeqFeatPtr LIBCALL SeqMgrGetOverlappingFeature (SeqLocPtr slp, Uint2 subtype,
                                                           VoidPtr featarray, Int4 numfeats,
                                                           Int4Ptr position, Int2 overlapType,
                                                           SeqMgrFeatContext PNTR context)

{
  return SeqMgrGetBestOverlappingFeat (slp, subtype, (SMFeatItemPtr PNTR) featarray,
                                       numfeats, position, overlapType, context, NULL, NULL, NULL, FALSE);
}

NLM_EXTERN SeqFeatPtr LIBCALL SeqMgrGetOverlappingFeatureEx (SeqLocPtr slp, Uint2 subtype,
                                                           VoidPtr featarray, Int4 numfeats,
                                                           Int4Ptr position, Int2 overlapType,
                                                           SeqMgrFeatContext PNTR context,
                                                           Boolean special)

{
  return SeqMgrGetBestOverlappingFeat (slp, subtype, (SMFeatItemPtr PNTR) featarray,
                                       numfeats, position, overlapType, context, NULL, NULL, NULL, special);
}

NLM_EXTERN Int2 LIBCALL SeqMgrGetAllOverlappingFeatures (SeqLocPtr slp, Uint2 subtype,
                                                         VoidPtr featarray,
                                                         Int4 numfeats,
                                                         Int2 overlapType,
                                                         Pointer userdata,
                                                         SeqMgrFeatExploreProc userfunc)

{
  SeqMgrFeatContext  context;
  Int2               count;

  SeqMgrGetBestOverlappingFeat (slp, subtype, (SMFeatItemPtr PNTR) featarray,
                                numfeats, NULL, overlapType, &context, &count,
                                userdata, userfunc, FALSE);

  return count;
}

NLM_EXTERN SeqFeatPtr LIBCALL SeqMgrGetFeatureInIndex (BioseqPtr bsp, VoidPtr featarray,
                                                       Int4 numfeats, Uint4 index,
                                                       SeqMgrFeatContext PNTR context)

{
  SMFeatItemPtr PNTR  array;
  SeqFeatPtr          curr;
  Uint2               entityID;
  SMFeatItemPtr       item = NULL;
  ObjMgrDataPtr       omdp;

  if (context != NULL) {
    MemSet ((Pointer) context, 0, sizeof (SeqMgrFeatContext));
  }
  if (bsp == NULL || featarray == NULL || numfeats < 1) return NULL;
  if (index < 1 || index > (Uint4) numfeats) return NULL;
  omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return NULL;

  array = (SMFeatItemPtr PNTR) featarray;
  item = array [index - 1];
  if (item == NULL) return NULL;

  entityID = bsp->idx.entityID;
  if (entityID < 1) {
    entityID = ObjMgrGetEntityIDForPointer (omdp->dataptr);
  }

  curr = item->sfp;
  if (curr != NULL && context != NULL && (! item->ignore)) {
    context->entityID = entityID;
    context->itemID = item->itemID;
    context->sfp = curr;
    context->sap = item->sap;
    context->bsp = item->bsp;
    context->label = item->label;
    context->left = item->left;
    context->right = item->right;
    context->dnaStop = item->dnaStop;
    context->partialL = item->partialL;
    context->partialR = item->partialR;
    context->farloc = item->farloc;
    context->bad_order = item->bad_order;
    context->mixed_strand = item->mixed_strand;
    context->ts_image = item->ts_image;
    context->strand = item->strand;
    if (curr != NULL) {
      context->seqfeattype = curr->data.choice;
    } else {
      context->seqfeattype = FindFeatFromFeatDefType (item->subtype);
    }
    context->featdeftype = item->subtype;
    context->numivals = item->numivals;
    context->ivals = item->ivals;
    context->userdata = NULL;
    context->omdp = (Pointer) omdp;
    context->index = item->index + 1;
  }
  return curr;
}

/*****************************************************************************
*
*   SeqMgrGetNextDescriptor and SeqMgrGetNextFeature
*
*****************************************************************************/

NLM_EXTERN ValNodePtr LIBCALL SeqMgrGetNextDescriptor (BioseqPtr bsp, ValNodePtr curr,
                                                       Uint1 seqDescChoice,
                                                       SeqMgrDescContext PNTR context)

{
  BioseqSetPtr   bssp;
  Uint2          entityID;
  ObjMgrDataPtr  omdp;
  SeqEntryPtr    sep;
  ValNode        vn;

  if (context == NULL) return NULL;

  /* if curr is NULL, initialize context fields (in user's stack) */

  if (curr == NULL) {
    if (bsp == NULL) return NULL;
    omdp = SeqMgrGetOmdpForBioseq (bsp);
    if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return NULL;

    context->omdp = (Pointer) omdp;
    context->itemID = omdp->lastDescrItemID;
    context->index = 0;
    context->level = 0;

    /* start curr just before beginning of bioseq descriptor list */

    curr = &vn;
    vn.choice = 0;
    vn.data.ptrvalue = 0;
    vn.next = bsp->descr;
  }

  omdp = (ObjMgrDataPtr) context->omdp;
  if (omdp == NULL) return NULL;

  if (bsp != NULL && bsp->idx.entityID > 0) {
    entityID = bsp->idx.entityID;
  } else {
    entityID = ObjMgrGetEntityIDForPointer (omdp->dataptr);
  }

  if (bsp != NULL && bsp->seqentry != NULL) {
    sep = bsp->seqentry;
  } else {  
    sep = ObjMgrGetChoiceForData (omdp->dataptr);
  }

  /* now look for next appropriate descriptor after curr in current chain */

  while (curr != NULL) {
    curr = curr->next;
    if (curr != NULL) {
      (context->itemID)++;
      (context->index)++;
      if (seqDescChoice == 0 || curr->choice == seqDescChoice) {
        context->entityID = entityID;
        context->sdp = curr;
        context->sep = sep;
        context->seqdesctype = curr->choice;
        context->userdata = NULL;
        context->omdp = (Pointer) omdp;
        return curr;
      }
    }
  }

  /* now go up omdp chain looking for next descriptor */

  while (curr == NULL) {
    omdp = SeqMgrGetOmdpForPointer (omdp->parentptr);
    if (omdp == NULL) return NULL;

    /* update current omdp in context */

    context->omdp = (Pointer) omdp;
    context->itemID = omdp->lastDescrItemID;

    switch (omdp->datatype) {
      case OBJ_BIOSEQ :
        bsp = (BioseqPtr) omdp->dataptr;
        curr = bsp->descr;
        break;
      case OBJ_BIOSEQSET :
        bssp = (BioseqSetPtr) omdp->dataptr;
        curr = bssp->descr;
        break;
      default :
        break;
    }

    if (omdp->datatype == OBJ_BIOSEQ && bsp != NULL && bsp->seqentry != NULL) {
      sep = bsp->seqentry;
    } else if (omdp->datatype == OBJ_BIOSEQSET && bssp != NULL && bssp->seqentry != NULL) {
      sep = bssp->seqentry;
    } else {
      sep = ObjMgrGetChoiceForData (omdp->dataptr);
    }
    
    (context->level)++;

    /* now look for first appropriate descriptor in current chain */

    while (curr != NULL) {
      (context->itemID)++;
      (context->index)++;
      if (seqDescChoice == 0 || curr->choice == seqDescChoice) {
        context->entityID = entityID;
        context->sdp = curr;
        context->sep = sep;
        context->seqdesctype = curr->choice;
        context->userdata = NULL;
        context->omdp = (Pointer) omdp;
        return curr;
      }
      curr = curr->next;
    }
  }

  return curr;
}

static SeqFeatPtr LIBCALL SeqMgrGetNextFeatureEx (BioseqPtr bsp, SeqFeatPtr curr,
                                                  Uint1 seqFeatChoice, Uint1 featDefChoice,
                                                  SeqMgrFeatContext PNTR context,
                                                  Boolean byLabel, Boolean byLocusTag)

{
  SMFeatItemPtr PNTR  array = NULL;
  BioseqExtraPtr      bspextra;
  Uint2               entityID;
  Uint4               i;
  SMFeatItemPtr       item;
  Int4                num = 0;
  ObjMgrDataPtr       omdp;
  Uint1               seqfeattype;

  if (context == NULL) return NULL;

  /* if curr is NULL, initialize context fields (in user's stack) */


  if (curr == NULL) {
    if (bsp == NULL) return NULL;
    omdp = SeqMgrGetOmdpForBioseq (bsp);
    if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return NULL;

    context->omdp = (Pointer) omdp;
    context->index = 0;
  }

  omdp = (ObjMgrDataPtr) context->omdp;
  if (omdp == NULL) return NULL;
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return NULL;
  if (byLocusTag) {
    array = bspextra->genesByLocusTag;
    num = bspextra->numgenes;
  } else if (byLabel) {
    array = bspextra->featsByLabel;
    num = bspextra->numfeats;
  } else {
    array = bspextra->featsByPos;
    num = bspextra->numfeats;
  }
  if (array == NULL || num < 1) return NULL;

  if (bsp != NULL && bsp->idx.entityID > 0) {
    entityID = bsp->idx.entityID;
  } else {
    entityID = ObjMgrGetEntityIDForPointer (omdp->dataptr);
  }

  i = context->index;

  /* now look for next appropriate feature */

  while (i < (Uint4) num) {
    item = array [i];
    if (item != NULL) {
      curr = item->sfp;
      i++;
      if (curr != NULL) {
        seqfeattype = curr->data.choice;
        if ((seqFeatChoice == 0 || seqfeattype == seqFeatChoice) &&
            (featDefChoice == 0 || item->subtype == featDefChoice) &&
            (! item->ignore)) {
          context->entityID = entityID;
          context->itemID = item->itemID;
          context->sfp = curr;
          context->sap = item->sap;
          context->bsp = item->bsp;
          context->label = item->label;
          context->left = item->left;
          context->right = item->right;
          context->dnaStop = item->dnaStop;
          context->partialL = item->partialL;
          context->partialR = item->partialR;
          context->farloc = item->farloc;
          context->bad_order = item->bad_order;
          context->mixed_strand = item->mixed_strand;
          context->ts_image = item->ts_image;
          context->strand = item->strand;
          context->seqfeattype = seqfeattype;
          context->featdeftype = item->subtype;
          context->numivals = item->numivals;
          context->ivals = item->ivals;
          context->userdata = NULL;
          context->omdp = (Pointer) omdp;
          if (byLocusTag) {
            context->index = i;
          } else if (byLabel) {
            context->index = i;
          } else {
            context->index = item->index + 1;
          }
          return curr;
        }
      }
    }
  }

  return NULL;
}

NLM_EXTERN SeqFeatPtr LIBCALL SeqMgrGetNextFeature (BioseqPtr bsp, SeqFeatPtr curr,
                                                    Uint1 seqFeatChoice, Uint1 featDefChoice,
                                                    SeqMgrFeatContext PNTR context)

{
  return SeqMgrGetNextFeatureEx (bsp, curr, seqFeatChoice, featDefChoice, context, FALSE, FALSE);
}

NLM_EXTERN SeqFeatPtr LIBCALL SeqMgrGetNextFeatureByLabel (BioseqPtr bsp, SeqFeatPtr curr,
                                                           Uint1 seqFeatChoice, Uint1 featDefChoice,
                                                           SeqMgrFeatContext PNTR context)

{
  return SeqMgrGetNextFeatureEx (bsp, curr, seqFeatChoice, featDefChoice, context, TRUE, FALSE);
}

NLM_EXTERN SeqFeatPtr LIBCALL SeqMgrGetNextGeneByLocusTag (BioseqPtr bsp, SeqFeatPtr curr,
                                                           SeqMgrFeatContext PNTR context
)

{
  return SeqMgrGetNextFeatureEx (bsp, curr, SEQFEAT_GENE, 0, context, FALSE, TRUE);
}

NLM_EXTERN AnnotDescPtr LIBCALL SeqMgrGetNextAnnotDesc (
  BioseqPtr bsp,
  AnnotDescPtr curr,
  Uint1 annotDescChoice,
  SeqMgrAndContext PNTR context
)

{
  Uint1              annotdesctype;
  AnnotDescPtr PNTR  array = NULL;
  BioseqExtraPtr     bspextra;
  Uint2              entityID;
  Uint4              i;
  AnnotDescPtr       item;
  Int4               num = 0;
  ObjMgrDataPtr      omdp;
  ObjValNodePtr      ovp;

  if (context == NULL) return NULL;

  /* if curr is NULL, initialize context fields (in user's stack) */


  if (curr == NULL) {
    if (bsp == NULL) return NULL;
    /*
    entityID = ObjMgrGetEntityIDForPointer (bsp);
    if (entityID < 1) return NULL;
    omdp = ObjMgrGetData (entityID);
    if (omdp == NULL) return NULL;
    */
    omdp = SeqMgrGetOmdpForBioseq (bsp);
    if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return NULL;

    context->omdp = (Pointer) omdp;
    context->index = 0;
  }

  omdp = (ObjMgrDataPtr) context->omdp;
  if (omdp == NULL) return NULL;

  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return NULL;

  array = bspextra->annotDescByID;
  num = bspextra->numannotdesc;
  if (array == NULL || num < 1) return NULL;

  if (bsp != NULL && bsp->idx.entityID > 0) {
    entityID = bsp->idx.entityID;
  } else {
    entityID = ObjMgrGetEntityIDForPointer (omdp->dataptr);
  }

  i = context->index;

  /* now look for next appropriate annotdesc */

  while (i < (Uint4) num) {
    item = array [i];
    if (item != NULL && item->extended != 0) {
      ovp = (ObjValNodePtr) item;
      i++;
      annotdesctype = item->choice;
      if (annotDescChoice == 0 || annotdesctype == annotDescChoice) {
        context->entityID = entityID;
        context->itemID = ovp->idx.itemID;
        context->adp = item;
        context->annotdesctype = annotdesctype;
        context->userdata = NULL;
        context->omdp = (Pointer) omdp;
        context->index = i;
        return item;
      }
    }
  }

  return NULL;
}

/*****************************************************************************
*
*   SeqMgrExploreBioseqs, SeqMgrExploreSegments, SeqMgrExploreDescriptors,
*     SeqMgrExploreFeatures, SeqMgrVisitDescriptors, and SeqMgrVisitFeatures
*
*****************************************************************************/

static Boolean JustExamineBioseqs (SeqEntryPtr sep, BioseqSetPtr bssp,
                                   SeqMgrBioseqContextPtr context,
                                   SeqMgrBioseqExploreProc userfunc,
                                   Boolean nucs, Boolean prots, Boolean parts,
                                   Int4Ptr count)

{
  BioseqPtr       bsp;
  BioseqExtraPtr  bspextra;
  ObjMgrDataPtr   omdp;

  if (sep == NULL || context == NULL || userfunc == NULL) return FALSE;

  if (IS_Bioseq (sep)) {
    bsp = (BioseqPtr) sep->data.ptrvalue;
    if (bsp == NULL) return TRUE;

    /* check for desired molecule type */

    if (ISA_na (bsp->mol) && (! nucs)) return TRUE;
    if (ISA_aa (bsp->mol) && (! prots)) return TRUE;

    omdp = SeqMgrGetOmdpForBioseq (bsp);
    if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return TRUE;
    bspextra = (BioseqExtraPtr) omdp->extradata;
    if (bspextra == NULL) return TRUE;

    context->itemID = bspextra->bspItemID;
    context->bsp = bsp;
    context->sep = sep;
    context->bssp = bssp;
    context->numsegs = bspextra->numsegs;
    context->omdp = omdp;
    (context->index)++;

    if (count != NULL) {
      (*count)++;
    }
    /* continue until user function returns FALSE, then exit all recursions */

    if (! userfunc (bsp, context)) return FALSE;
    return TRUE;
  }

  if (IS_Bioseq_set (sep)) {
    bssp = (BioseqSetPtr) sep->data.ptrvalue;
    if (bssp == NULL) return TRUE;

    /* check to see if parts should be explored */

    if (bssp->_class == BioseqseqSet_class_parts) {
      if (! parts) return TRUE;

      /* within the parts set we want to see individual component bioseqs */

      nucs = TRUE;
      prots = TRUE;
    }

    /* recursively explore bioseq set until user function returns FALSE */

    for (sep = bssp->seq_set; sep != NULL; sep = sep->next) {
      if (! JustExamineBioseqs (sep, bssp, context, userfunc, nucs, prots, parts, count)) return FALSE;
    }
  }

  return TRUE;
}

NLM_EXTERN Int4 LIBCALL SeqMgrExploreBioseqs (Uint2 entityID, Pointer ptr, Pointer userdata,
                                              SeqMgrBioseqExploreProc userfunc,
                                              Boolean nucs, Boolean prots, Boolean parts)

{
  SeqMgrBioseqContext  context;
  Int4                 count = 0;
  SeqEntryPtr          sep;

  if (entityID == 0) {
    entityID = ObjMgrGetEntityIDForPointer (ptr);
  }
  if (entityID == 0) return 0;
  sep = SeqMgrGetTopSeqEntryForEntity (entityID);
  if (sep == NULL) return 0;
  if (userfunc == NULL) return 0;

  context.entityID = entityID;
  context.index = 0;
  context.userdata = userdata;

  /* recursive call to explore SeqEntry and pass appropriate bioseqs to user */

  JustExamineBioseqs (sep, NULL, &context, userfunc, nucs, prots, parts, &count);

  return count;
}

NLM_EXTERN Int4 LIBCALL SeqMgrExploreSegments (BioseqPtr bsp, Pointer userdata,
                                               SeqMgrSegmentExploreProc userfunc)

{
  BioseqExtraPtr        bspextra;
  SeqMgrSegmentContext  context;
  Int4                  count = 0;
  Uint2                 entityID;
  Uint4                 i;
  ObjMgrDataPtr         omdp;
  SMSeqIdxPtr PNTR      partsByLoc;
  SMSeqIdxPtr           segpartptr;
  SeqLocPtr             slp;

  if (bsp == NULL) return 0;
  if (bsp->repr != Seq_repr_seg && bsp->repr != Seq_repr_delta) return 0;
  omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return 0;
  if (userfunc == NULL) return 0;
  entityID = bsp->idx.entityID;
  if (entityID < 1) {
    entityID = ObjMgrGetEntityIDForPointer (omdp->dataptr);
  }

  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return 0;
  partsByLoc = bspextra->partsByLoc;
  if (partsByLoc == NULL || bspextra->numsegs < 1) return 0;

  for (i = 0; i < (Uint4) bspextra->numsegs; i++) {
    segpartptr = partsByLoc [i];
    if (segpartptr != NULL) {
      slp = segpartptr->slp;
      context.entityID = entityID;
      context.itemID = segpartptr->itemID;
      context.slp = slp;
      context.parent = segpartptr->parentBioseq;
      context.cumOffset = segpartptr->cumOffset;
      context.from = segpartptr->from;
      context.to = segpartptr->to;
      context.strand = segpartptr->strand;
      context.userdata = userdata;
      context.omdp = (Pointer) omdp;
      context.index = i + 1;

      count++;

      if (! userfunc (slp, &context)) return count;
    }
  }

  return count;
}

NLM_EXTERN Int4 LIBCALL SeqMgrExploreDescriptors (BioseqPtr bsp, Pointer userdata,
                                                  SeqMgrDescExploreProc userfunc,
                                                  BoolPtr seqDescFilter)

{
  BioseqSetPtr       bssp;
  SeqMgrDescContext  context;
  Int4               count = 0;
  Uint2              entityID;
  Uint4              itemID;
  ObjMgrDataPtr      omdp;
  ValNodePtr         sdp;
  SeqEntryPtr        sep;

  if (bsp == NULL) return 0;
  omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return 0;
  if (userfunc == NULL) return 0;
  entityID = bsp->idx.entityID;
  if (entityID < 1) {
    entityID = ObjMgrGetEntityIDForPointer (omdp->dataptr);
  }

  context.index = 0;
  context.level = 0;
  while (omdp != NULL) {
    itemID = omdp->lastDescrItemID;
    sdp = NULL;
    switch (omdp->datatype) {
      case OBJ_BIOSEQ :
        bsp = (BioseqPtr) omdp->dataptr;
        sdp = bsp->descr;
        break;
      case OBJ_BIOSEQSET :
        bssp = (BioseqSetPtr) omdp->dataptr;
        sdp = bssp->descr;
        break;
      default :
        break;
    }

    sep = ObjMgrGetChoiceForData (omdp->dataptr);

    /* call for every appropriate descriptor in current chain */

    while (sdp != NULL) {
      itemID++;
      if (seqDescFilter == NULL || seqDescFilter [sdp->choice]) {
        context.entityID = entityID;
        context.itemID = itemID;
        context.sdp = sdp;
        context.sep = sep;
        context.seqdesctype = sdp->choice;
        context.userdata = userdata;
        context.omdp = (Pointer) omdp;
        (context.index)++;

        count++;

        if (! userfunc (sdp, &context)) return count;
      }
      sdp = sdp->next;
    }

    /* now go up omdp chain looking for next descriptor */

    omdp = SeqMgrGetOmdpForPointer (omdp->parentptr);
    (context.level)++;
  }
  return count;
}

static Int4 LIBCALL SeqMgrExploreFeaturesInt (BioseqPtr bsp, Pointer userdata,
                                              SeqMgrFeatExploreProc userfunc,
                                              SeqLocPtr locationFilter,
                                              BoolPtr seqFeatFilter,
                                              BoolPtr featDefFilter,
                                              Boolean doreverse)

{
  BioseqExtraPtr      bspextra;
  SeqMgrFeatContext   context;
  Int4                count = 0;
  Uint2               entityID;
  SMFeatItemPtr PNTR  featsByID;
  SMFeatItemPtr PNTR  featsByPos;
  SMFeatItemPtr PNTR  featsByRev;
  Uint4               i;
  SMFeatItemPtr       item;
  Int4                left = INT4_MIN;
  ObjMgrDataPtr       omdp;
  Int4                right = INT4_MAX;
  Uint1               seqfeattype;
  SeqFeatPtr          sfp;
  Uint4               start = 0;
  Int4                tmp;

  if (bsp == NULL) return 0;
  omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return 0;
  if (userfunc == NULL) return 0;
  entityID = bsp->idx.entityID;
  if (entityID < 1) {
    entityID = ObjMgrGetEntityIDForPointer (omdp->dataptr);
  }

  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return 0;

  if (doreverse) {
    if (bspextra->featsByRev == NULL) {

      /* index by reverse position if not already done */

      featsByRev = (SMFeatItemPtr PNTR) MemNew (sizeof (SMFeatItemPtr) * (bspextra->numfeats + 1));
      bspextra->featsByRev = featsByRev;

      if (featsByRev != NULL) {
        featsByID = bspextra->featsByID;
        for (i = 0; i < (Uint4) bspextra->numfeats; i++) {
          featsByRev [i] = featsByID [i];
        }

        /* sort all features by feature reverse location on bioseq */

        StableMergeSort ((VoidPtr) featsByRev, (size_t) bspextra->numfeats, sizeof (SMFeatItemPtr), SortFeatItemListByRev);
      }
    }

    featsByPos = bspextra->featsByRev;
  } else {
    featsByPos = bspextra->featsByPos;
  }
  if (featsByPos == NULL || bspextra->numfeats < 1) return 0;

  if (locationFilter != NULL) {
    left = GetOffsetInBioseq (locationFilter, bsp, SEQLOC_LEFT_END);
    if (left == -1) left = INT4_MIN;
    right = GetOffsetInBioseq (locationFilter, bsp, SEQLOC_RIGHT_END);
    if (right == -1) right = INT4_MAX;

    /* if far segmented or delta, and location (from explore
       segments) is minus strand, will need to swap */

    if (left > right) {
      tmp = left;
      left = right;
      right = tmp;
    }

    /*
    binary search to leftmost candidate would need featsByPos array
    variant sorted primarily by rightmost position, so comment this
    out for now, resurrect and add new array only if it turns out to
    be necessary when we support entrez fetch subrecord by location
    */

    /*
    L = 0;
    R = bspextra->numfeats - 1;
    while (L < R) {
      mid = (L + R) / 2;
      item = featsByPos [mid];
      if (item != NULL && item->right < left) {
        L = mid + 1;
      } else {
        R = mid;
      }
    }

    start = R;
    */
  }

  /* call for every appropriate feature in sorted list */

  for (i = start; i < (Uint4) bspextra->numfeats; i++) {
    item = featsByPos [i];
    if (item != NULL) {

      /* can exit once past rightmost limit */

      if (locationFilter != NULL && (! doreverse) && item->left > right) return count;
      if (locationFilter != NULL && (doreverse) && item->right < left) return count;

      sfp = item->sfp;
      if (sfp != NULL) {
        seqfeattype = sfp->data.choice;
      } else {
        seqfeattype = FindFeatFromFeatDefType (item->subtype);
      }
      if ((seqFeatFilter == NULL || seqFeatFilter [seqfeattype]) &&
          (featDefFilter == NULL || featDefFilter [item->subtype]) &&
          (locationFilter == NULL || (item->right >= left && item->left <= right)) &&
          (! item->ignore)) {
        context.entityID = entityID;
        context.itemID = item->itemID;
        context.sfp = sfp;
        context.sap = item->sap;
        context.bsp = item->bsp;
        context.label = item->label;
        context.left = item->left;
        context.right = item->right;
        context.dnaStop = item->dnaStop;
        context.partialL = item->partialL;
        context.partialR = item->partialR;
        context.farloc = item->farloc;
        context.bad_order = item->bad_order;
        context.mixed_strand = item->mixed_strand;
        context.strand = item->strand;
        context.seqfeattype = seqfeattype;
        context.featdeftype = item->subtype;
        context.numivals = item->numivals;
        context.ivals = item->ivals;
        context.userdata = userdata;
        context.omdp = (Pointer) omdp;
        context.index = item->index + 1;

        count++;

        if (! userfunc (sfp, &context)) return count;
      }
    }
  }
  return count;
}

NLM_EXTERN Int4 LIBCALL SeqMgrExploreFeatures (BioseqPtr bsp, Pointer userdata,
                                               SeqMgrFeatExploreProc userfunc,
                                               SeqLocPtr locationFilter,
                                               BoolPtr seqFeatFilter,
                                               BoolPtr featDefFilter)

{
  return SeqMgrExploreFeaturesInt (bsp, userdata, userfunc, locationFilter, seqFeatFilter, featDefFilter, FALSE);
}

NLM_EXTERN Int4 LIBCALL SeqMgrExploreFeaturesRev (BioseqPtr bsp, Pointer userdata,
                                                  SeqMgrFeatExploreProc userfunc,
                                                  SeqLocPtr locationFilter,
                                                  BoolPtr seqFeatFilter,
                                                  BoolPtr featDefFilter)

{
  return SeqMgrExploreFeaturesInt (bsp, userdata, userfunc, locationFilter, seqFeatFilter, featDefFilter, TRUE);
}

static Int2 VisitDescriptorsPerSeqEntry (Uint2 entityID, SeqEntryPtr sep,
                                         Pointer userdata, SeqMgrDescExploreProc userfunc,
                                         BoolPtr seqDescFilter)

{
  BioseqPtr          bsp;
  BioseqSetPtr       bssp = NULL;
  Uint2              count = 0;
  SeqMgrDescContext  context;
  Uint4              itemID;
  ObjMgrDataPtr      omdp = NULL;
  ValNodePtr         sdp = NULL;
  SeqEntryPtr        tmp;

  if (sep != NULL) {
    if (IS_Bioseq (sep)) {
      bsp = (BioseqPtr) sep->data.ptrvalue;
      if (bsp == NULL) return 0;
      omdp = SeqMgrGetOmdpForBioseq (bsp);
      sdp = bsp->descr;
    } else if (IS_Bioseq_set (sep)) {
      bssp = (BioseqSetPtr) sep->data.ptrvalue;
      if (bssp == NULL) return 0;
      omdp = SeqMgrGetOmdpForPointer (bssp);
      sdp = bssp->descr;
    }
  }
  if (omdp == NULL) return 0;
  itemID = omdp->lastDescrItemID;

  context.index = 0;
  context.level = 0;

  while (sdp != NULL) {
    itemID++;
    if (seqDescFilter == NULL || seqDescFilter [sdp->choice]) {
      context.entityID = entityID;
      context.itemID = itemID;
      context.sdp = sdp;
      context.sep = sep;
      context.seqdesctype = sdp->choice;
      context.userdata = userdata;
      context.omdp = (Pointer) omdp;

      count++;

      if (! userfunc (sdp, &context)) return count;
    }
    sdp = sdp->next;
  }

  if (bssp != NULL) {
    for (tmp = bssp->seq_set; tmp != NULL; tmp = tmp->next) {
      count += VisitDescriptorsPerSeqEntry (entityID, tmp, userdata, userfunc, seqDescFilter);
    }
  }

  return count;
}

NLM_EXTERN Int2 LIBCALL SeqMgrVisitDescriptors (Uint2 entityID, Pointer userdata,
                                                SeqMgrDescExploreProc userfunc,
                                                BoolPtr seqDescFilter)

{
  SeqEntryPtr  sep;

  if (entityID < 1 || userfunc == NULL) return 0;
  sep = SeqMgrGetTopSeqEntryForEntity (entityID);
  if (sep == NULL) return 0;

  return VisitDescriptorsPerSeqEntry (entityID, sep, userdata, userfunc, seqDescFilter);
}

NLM_EXTERN Int2 LIBCALL SeqMgrVisitFeatures (Uint2 entityID, Pointer userdata,
                                             SeqMgrFeatExploreProc userfunc,
                                             BoolPtr seqFeatFilter, BoolPtr featDefFilter)

{
  BioseqExtraPtr      bspextra;
  SeqMgrFeatContext   context;
  Int2                count = 0;
  SMFeatItemPtr PNTR  featsByID;
  Uint2               i;
  SMFeatItemPtr       item;
  ObjMgrDataPtr       omdp;
  Uint1               seqfeattype;
  SeqFeatPtr          sfp;

  omdp = ObjMgrGetData (entityID);
  if (omdp == NULL) return 0;
  if (userfunc == NULL) return 0;

  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return 0;
  featsByID = bspextra->featsByID;
  if (featsByID == NULL || bspextra->numfeats < 1) return 0;

  /* call for every appropriate feature in itemID order */

  for (i = 0; i < bspextra->numfeats; i++) {
    item = featsByID [i];
    if (item != NULL) {

      sfp = item->sfp;
      if (sfp != NULL) {
        seqfeattype = sfp->data.choice;
      } else {
        seqfeattype = FindFeatFromFeatDefType (item->subtype);
      }
      if ((seqFeatFilter == NULL || seqFeatFilter [seqfeattype]) &&
          (featDefFilter == NULL || featDefFilter [item->subtype]) &&
          (! item->ignore)) {
        context.entityID = entityID;
        context.itemID = item->itemID;
        context.sfp = sfp;
        context.sap = item->sap;
        context.bsp = item->bsp;
        context.label = item->label;
        context.left = item->left;
        context.right = item->right;
        context.dnaStop = item->dnaStop;
        context.partialL = item->partialL;
        context.partialR = item->partialR;
        context.farloc = item->farloc;
        context.bad_order = item->bad_order;
        context.mixed_strand = item->mixed_strand;
        context.strand = item->strand;
        context.seqfeattype = seqfeattype;
        context.featdeftype = item->subtype;
        context.numivals = item->numivals;
        context.ivals = item->ivals;
        context.userdata = userdata;
        context.omdp = (Pointer) omdp;
        context.index = 0;

        count++;

        if (! userfunc (sfp, &context)) return count;
      }
    }
  }
  return count;
}

/*****************************************************************************
*
*   SeqMgrMapPartToSegmentedBioseq can speed up sequtil's CheckPointInBioseq
*     for indexed part bioseq to segmented bioseq mapping
*
*****************************************************************************/

static SMSeqIdxPtr BinarySearchPartToSegmentMap (BioseqPtr in, Int4 pos, BioseqPtr bsp, SeqIdPtr sip, Boolean relaxed, Int4 from, Int4 to)

{
  BioseqExtraPtr    bspextra;
  Char              buf [80];
  Int2              compare;
  ObjMgrDataPtr     omdp;
  SMSeqIdxPtr PNTR  partsBySeqId;
  SMSeqIdxPtr       segpartptr;
  CharPtr           seqIdOfPart;
  Int4              L, R, mid;

  if (in == NULL) return NULL;
  omdp = SeqMgrGetOmdpForBioseq (in);
  if (omdp == NULL) return NULL;
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return NULL;

  partsBySeqId = bspextra->partsBySeqId;
  if (partsBySeqId == NULL || bspextra->numsegs < 1) return NULL;

  if (bsp != NULL) {
    sip = bsp->id;
  }
  if (sip == NULL) return NULL;

  /* binary search into array on segmented bioseq sorted by part seqID (reversed) string */

  while (sip != NULL) {
    if (MakeReversedSeqIdString (sip, buf, sizeof (buf) - 1)) {
      L = 0;
      R = bspextra->numsegs - 1;
      while (L < R) {
        mid = (L + R) / 2;
        segpartptr = partsBySeqId [mid];
        compare = StringCmp (segpartptr->seqIdOfPart, buf);
        if (compare < 0) {
          L = mid + 1;
        } else {
          R = mid;
        }
      }

      /* loop through all components with same seqID, get appropriate segment */

      segpartptr = partsBySeqId [R];
      seqIdOfPart = segpartptr->seqIdOfPart;
      while (R < bspextra->numsegs && StringCmp (seqIdOfPart, buf) == 0) {
        if (relaxed) {

          /* for genome mapping of portion not included in contig */

          if ((from >= segpartptr->from && from <= segpartptr->to) ||
              (to >= segpartptr->from && to <= segpartptr->to) ||
              (from < segpartptr->from && to > segpartptr->to) ||
              (to < segpartptr->from && from > segpartptr->to)) {

            return segpartptr;
          }

        } else if (pos >= segpartptr->from && pos <= segpartptr->to) {

          /* otherwise only map portion included in contig */

          return segpartptr;
        }

        R++;
        if (R < bspextra->numsegs) {
          segpartptr = partsBySeqId [R];
          seqIdOfPart = segpartptr->seqIdOfPart;
        } else {
          seqIdOfPart = NULL;
        }
      }
    }
    sip = sip->next;
  }

  return NULL;
}

NLM_EXTERN SMSeqIdxPtr GenomePartToSegmentMap (BioseqPtr in, BioseqPtr bsp, Int4 from, Int4 to)

{
  return BinarySearchPartToSegmentMap (in, 0, bsp, NULL, TRUE, from, to);
}

NLM_EXTERN Int4 LIBCALL SeqMgrMapPartToSegmentedBioseq (BioseqPtr in, Int4 pos, BioseqPtr bsp, SeqIdPtr sip, BoolPtr flip_strand)

{
  BioseqExtraPtr  bspextra;
  SMSeqIdxPtr     currp;
  SMSeqIdxPtr     nextp;
  ObjMgrDataPtr   omdp;
  SMSeqIdxPtr     segpartptr;

  if (in == NULL) return -1;
  if (flip_strand != NULL) {
    *flip_strand = FALSE;
  }

  /* first check to see if part has been loaded and single map up block installed */

  if (bsp != NULL) {
    omdp = SeqMgrGetOmdpForBioseq (bsp);
    if (omdp != NULL) {
      bspextra = (BioseqExtraPtr) omdp->extradata;
      if (bspextra != NULL) {

        /* no need for partsByLoc or partsBySeqId arrays, just use segparthead linked list */

        for (segpartptr = bspextra->segparthead; segpartptr != NULL; segpartptr = segpartptr->next) {
          if (segpartptr->parentBioseq == in) {
            if (pos >= segpartptr->from && pos <= segpartptr->to) {

              /* success, immediate return with mapped up value */

              if (segpartptr->strand == Seq_strand_minus) {
                if (flip_strand != NULL) {
                  *flip_strand = FALSE;
                }
                return segpartptr->cumOffset + (segpartptr->to - pos);
              } else {
                return segpartptr->cumOffset + (pos - segpartptr->from);
              }
            }
          }
        }
      }
    }
  }

  /* otherwise do binary search on segmented bioseq mapping data */

  segpartptr = BinarySearchPartToSegmentMap (in, pos, bsp, sip, FALSE, 0, 0);
  if (segpartptr == NULL) return -1;

  if (pos >= segpartptr->from && pos <= segpartptr->to) {

    /* install map up block on part, if it has been loaded, to speed up next search */

    if (bsp != NULL) {
      omdp = SeqMgrGetOmdpForBioseq (bsp);
      if (omdp != NULL) {
        bspextra = (BioseqExtraPtr) omdp->extradata;
        if (bspextra == NULL) {
          CreateBioseqExtraBlock (omdp, bsp);
          bspextra = (BioseqExtraPtr) omdp->extradata;
        }
        if (bspextra != NULL) {

          /* clean up any old map up info on part */

          for (currp = bspextra->segparthead; currp != NULL; currp = nextp) {
            nextp = currp->next;
            SeqLocFree (currp->slp);
            MemFree (currp->seqIdOfPart);
            MemFree (currp);
          }
          bspextra->segparthead = NULL;
          bspextra->numsegs = 0;
          bspextra->partsByLoc = MemFree (bspextra->partsByLoc);
          bspextra->partsBySeqId = MemFree (bspextra->partsBySeqId);

          /* allocate single map up block */

          currp = MemNew (sizeof (SMSeqIdx));
          if (currp != NULL) {
            currp->slp = AsnIoMemCopy (segpartptr->slp,
                                       (AsnReadFunc) SeqLocAsnRead,
                                       (AsnWriteFunc) SeqLocAsnWrite);
            currp->seqIdOfPart = StringSave (segpartptr->seqIdOfPart);
            currp->parentBioseq = segpartptr->parentBioseq;
            currp->cumOffset = segpartptr->cumOffset;
            currp->from = segpartptr->from;
            currp->to = segpartptr->to;
            currp->strand = segpartptr->strand;
          }

          /* add new map up block to part */

          bspextra->segparthead = currp;
        }
      }
    }

    /* now return offset result */

    if (segpartptr->strand == Seq_strand_minus) {
      if (flip_strand != NULL) {
        *flip_strand = TRUE;
      }
      return segpartptr->cumOffset + (segpartptr->to - pos);
    } else {
      return segpartptr->cumOffset + (pos - segpartptr->from);
    }
  }
  return -1;
}

/*****************************************************************************
*
*   TrimLocInSegment takes a location on an indexed far segmented part and trims
*     trims it to the region referred to by the parent segmented or delta bioseq.
*
*     Only implemented for seqloc_int components, not seqloc_point
*
*****************************************************************************/

NLM_EXTERN SeqLocPtr TrimLocInSegment (
  BioseqPtr master,
  SeqLocPtr location,
  BoolPtr p5ptr,
  BoolPtr p3ptr
)

{
  BioseqPtr         bsp;
  BioseqExtraPtr    bspextra;
  Char              buf [80];
  Int2              compare;
  ObjMgrDataPtr     omdp;
  Boolean           partial5;
  Boolean           partial3;
  SMSeqIdxPtr PNTR  partsBySeqId;
  SeqLocPtr         rsult = NULL;
  SMSeqIdxPtr       segpartptr;
  CharPtr           seqIdOfPart;
  SeqIdPtr          sip;
  SeqIntPtr         sint;
  SeqLocPtr         slp;
  Uint1             strand;
  Int4              L, R, mid;
  Int4              start, stop, swap;

  if (master == NULL || location == NULL) return NULL;

  omdp = SeqMgrGetOmdpForBioseq (master);
  if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return NULL;
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return NULL;

  partsBySeqId = bspextra->partsBySeqId;
  if (partsBySeqId == NULL || bspextra->numsegs < 1) return NULL;

  partial5 = FALSE;
  partial3 = FALSE;

  if (p5ptr != NULL) {
    partial5 = *p5ptr;
  }
  if (p3ptr != NULL) {
    partial3 = *p3ptr;
  }

  for (slp = SeqLocFindNext (location, NULL);
       slp != NULL;
       slp = SeqLocFindNext (location, slp)) {
    if (slp->choice != SEQLOC_INT) continue;
    sint = (SeqIntPtr) slp->data.ptrvalue;
    if (sint == NULL) continue;
    strand = sint->strand;

    bsp = BioseqFind (sint->id);
    if (bsp == NULL) continue;

    for (sip = bsp->id; sip != NULL; sip = sip->next) {
      if (! MakeReversedSeqIdString (sip, buf, sizeof (buf) - 1)) continue;

      L = 0;
      R = bspextra->numsegs - 1;
      while (L < R) {
        mid = (L + R) / 2;
        segpartptr = partsBySeqId [mid];
        compare = StringCmp (segpartptr->seqIdOfPart, buf);
        if (compare < 0) {
          L = mid + 1;
        } else {
          R = mid;
        }
      }

      segpartptr = partsBySeqId [R];
      seqIdOfPart = segpartptr->seqIdOfPart;

      while (R < bspextra->numsegs && StringCmp (seqIdOfPart, buf) == 0) {

        start = sint->from;
        stop = sint->to;

        if ((sint->from <= segpartptr->from && sint->to > segpartptr->from) ||
            (sint->from < segpartptr->to && sint->to >= segpartptr->to)) {

          if (sint->from < segpartptr->from) {
            start = segpartptr->from;
            if (strand == Seq_strand_minus || strand == Seq_strand_both_rev) {
              partial3 = TRUE;
            } else {
              partial5 = TRUE;
            }
          }
          if (sint->to > segpartptr->to) {
            stop = segpartptr->to;
            if (strand == Seq_strand_minus || strand == Seq_strand_both_rev) {
              partial5 = TRUE;
            } else {
              partial3 = TRUE;
            }
          }

          if (strand == Seq_strand_minus || strand == Seq_strand_both_rev) {
            swap = start;
            start = stop;
            stop = swap;
          }

          rsult = AddIntervalToLocation (rsult, sint->id, start, stop, FALSE, FALSE);
        }

        R++;
        if (R < bspextra->numsegs) {
          segpartptr = partsBySeqId [R];
          seqIdOfPart = segpartptr->seqIdOfPart;
        } else {
          seqIdOfPart = NULL;
        }
      }
    }
  }

  if (p5ptr != NULL) {
    *p5ptr = partial5;
  }
  if (p3ptr != NULL) {
    *p3ptr = partial3;
  }

  return rsult;
}

/***************************/

static ValNodePtr  smp_requested_uid_list = NULL;
static TNlmMutex   smp_requested_uid_mutex = NULL;

static ValNodePtr  smp_locked_bsp_list = NULL;
static TNlmMutex   smp_locked_bsp_mutex = NULL;

static void AddBspToList (
  BioseqPtr bsp
)

{
  Int4        ret;
  ValNodePtr  vnp;

  if (bsp == NULL) return;

  ret = NlmMutexLockEx (&smp_locked_bsp_mutex);
  if (ret) {
    ErrPostEx (SEV_FATAL, 0, 0, "AddBspToList mutex failed [%ld]", (long) ret);
    return;
  }

  vnp = ValNodeAddPointer (NULL, 0, (Pointer) bsp);
  if (vnp != NULL) {
    vnp->next = smp_locked_bsp_list;
    smp_locked_bsp_list = vnp;
  }

  NlmMutexUnlock (smp_locked_bsp_mutex);
}

static Int4 RemoveUidFromQueue (
  void
)

{
  Int4        ret, uid = 0;
  ValNodePtr  vnp;

  ret = NlmMutexLockEx (&smp_requested_uid_mutex);
  if (ret) {
    ErrPostEx (SEV_FATAL, 0, 0, "RemoveUidFromQueue mutex failed [%ld]", (long) ret);
    return 0;
  }

  /* extract next requested uid from queue */

  if (smp_requested_uid_list != NULL) {
    vnp = smp_requested_uid_list;
    smp_requested_uid_list = vnp->next;
    vnp->next = NULL;
    uid = (Int4) vnp->data.intvalue;
    ValNodeFree (vnp);
  }

  NlmMutexUnlock (smp_requested_uid_mutex);

  return uid;
}

static VoidPtr DoAsyncLookup (
  VoidPtr arg
)

{
  BioseqPtr  bsp;
  Int4       uid;
  ValNode    vn;

  MemSet ((Pointer) &vn, 0, sizeof (ValNode));

  uid = RemoveUidFromQueue ();
  while (uid > 0) {

    vn.choice = SEQID_GI;
    vn.data.intvalue = uid;
    vn.next = NULL;

    if (BioseqFindFunc (&vn, FALSE, FALSE, TRUE) == NULL) {
      bsp = BioseqLockByIdEx (&vn, FALSE);
      if (bsp != NULL) {
        AddBspToList (bsp);
      }
    }

    uid = RemoveUidFromQueue ();
  }

  return NULL;
}

#define NUM_ASYNC_LOOKUP_THREADS 5

static ValNodePtr LookupAndExtractBspListMT (
  ValNodePtr PNTR uidlistP
)

{
  Int2        i;
  Int4        ret;
  VoidPtr     status;
  ValNodePtr  sublist = NULL;
  TNlmThread  thds [NUM_ASYNC_LOOKUP_THREADS];

  if (uidlistP == NULL || *uidlistP == NULL) return NULL;

  ret = NlmMutexLockEx (&smp_requested_uid_mutex);
  if (ret) {
    ErrPostEx (SEV_FATAL, 0, 0, "add uid mutex failed [%ld]", (long) ret);
    return NULL;
  }

  smp_requested_uid_list = *uidlistP;
  *uidlistP = NULL;

  NlmMutexUnlock (smp_requested_uid_mutex);

  /* spawn several threads for individual lock requests */

  for (i = 0; i < NUM_ASYNC_LOOKUP_THREADS; i++) {
    thds [i] = NlmThreadCreate (DoAsyncLookup, NULL);
  }

  /* wait for all fetching threads to complete */

  for (i = 0; i < NUM_ASYNC_LOOKUP_THREADS; i++) {
    NlmThreadJoin (thds [i], &status);
  }

  ret = NlmMutexLockEx (&smp_locked_bsp_mutex);
  if (ret) {
    ErrPostEx (SEV_FATAL, 0, 0, "get bsp mutex failed [%ld]", (long) ret);
    return NULL;
  }

  sublist = smp_locked_bsp_list;
  smp_locked_bsp_list = NULL;

  NlmMutexUnlock (smp_locked_bsp_mutex);

  return sublist;
}

static ValNodePtr LookupAndExtractBspListST (
  ValNodePtr PNTR uidlistP,
  Boolean reindexIfBig
)

{
  BioseqPtr    bsp;
  Uint2        entityID;
  SeqEntryPtr  sep;
  SeqId        si;
  ValNodePtr   sublist = NULL, vnp, vnx;
  Int4         uid;

  if (uidlistP == NULL || *uidlistP == NULL) return NULL;

  MemSet ((Pointer) &si, 0, sizeof (SeqId));

  /* record fetching loop */

  for (vnp = *uidlistP; vnp != NULL; vnp = vnp->next) {
    uid = (Int4) vnp->data.intvalue;
    if (uid < 1) continue;
    si.choice = SEQID_GI;
    si.data.intvalue = uid;

    if (BioseqFindFunc (&si, FALSE, TRUE, TRUE) != NULL) continue;
    bsp = BioseqLockByIdEx (&si, FALSE);
    if (bsp == NULL) continue;

    if (reindexIfBig) {
      entityID = ObjMgrGetEntityIDForPointer (bsp);
      sep = GetTopSeqEntryForEntityID (entityID);
      if (sep != NULL && VisitBioseqsInSep (sep, NULL, NULL) > 2) {
        SeqMgrHoldIndexing (FALSE);
        ObjMgrClearHold ();
        ObjMgrSetHold ();
        SeqMgrHoldIndexing (TRUE);
      }
   }

    vnx = ValNodeAddPointer (NULL, 0, (Pointer) bsp);
    if (vnx == NULL) continue;
    vnx->next = sublist;
    sublist = vnx;
  }

  /* clean up input uidlist */

  *uidlistP = ValNodeFree (*uidlistP);

  return sublist;
}

static ValNodePtr LookupAndExtractBspList (
  ValNodePtr PNTR uidlistP,
  Boolean usethreads,
  Boolean reindexIfBig
)

{
  SeqEntryPtr  oldsep;
  SeqId        si;
  ValNodePtr   sublist = NULL, vnp;
  Int4         uid;

  if (uidlistP == NULL || *uidlistP == NULL) return NULL;

  MemSet ((Pointer) &si, 0, sizeof (SeqId));

  /* exclude any records already loaded anywhere in memory */

  oldsep = SeqEntrySetScope (NULL);
  for (vnp = *uidlistP; vnp != NULL; vnp = vnp->next) {
    uid = (Int4) vnp->data.intvalue;
    if (uid < 1) continue;
    si.choice = SEQID_GI;
    si.data.intvalue = uid;

    if (BioseqFindFunc (&si, FALSE, FALSE, TRUE) == NULL) continue;
    vnp->data.intvalue = 0;
  }
  SeqEntrySetScope (oldsep);

  /* now do actual fetching */

  if (usethreads) {
    sublist = LookupAndExtractBspListMT (uidlistP);
  } else {
    sublist = LookupAndExtractBspListST (uidlistP, reindexIfBig);
  }

  return sublist;
}

static void SortUniqueCleanseUidList (
  ValNodePtr PNTR uidlistP,
  ValNodePtr PNTR bsplistP
)

{
  BioseqPtr        bsp;
  Int4             j, len, L, R, mid, uid;
  SeqIdPtr         sip;
  ValNodePtr PNTR  uids;
  ValNodePtr       vnp, vnx;

  if (uidlistP == NULL || *uidlistP == NULL) return;

  /* sort and unique uids to download */

  *uidlistP = ValNodeSort (*uidlistP, SortByIntvalue);
  *uidlistP = UniqueIntValNode (*uidlistP);

  if (bsplistP == NULL || *bsplistP == NULL) return;

  /* zero out any uids already fetched in earlier loop */

  len = ValNodeLen (*uidlistP);
  if (len == 0) return;
  uids = (ValNodePtr PNTR) MemNew (sizeof (ValNodePtr) * (len + 1));
  if (uids == NULL) return;

  for (vnp = *uidlistP, j = 0; vnp != NULL; vnp = vnp->next, j++) {
    uids [j] = vnp;
  }

  for (vnp = *bsplistP; vnp != NULL; vnp = vnp->next) {
    bsp = (BioseqPtr) vnp->data.ptrvalue;
    if (bsp == NULL) continue;
    uid = 0;
    for (sip = bsp->id; sip != NULL && uid == 0; sip = sip->next) {
      if (sip->choice != SEQID_GI) continue;
      uid = (Int4) sip->data.intvalue;
    }
    if (uid < 1) continue;

    L = 0;
    R = len - 1;

    while (L < R) {
      mid = (L + R) / 2;
      vnx = uids [mid];
      if (vnx != NULL && vnx->data.intvalue < uid) {
        L = mid + 1;
      } else {
        R = mid;
      }
    }

    vnx = uids [R];
    if (vnx != NULL && vnx->data.intvalue == uid) {
      /* mark uid that is already loaded */
      vnx->choice = 1;
    }
  }

  for (vnp = *uidlistP; vnp != NULL; vnp = vnp->next) {
    if (vnp->choice == 1) {
      /* clear out marked uids */
      vnp->data.intvalue = 0;
    }
  }

  MemFree (uids);
}

typedef struct iddata {
  ValNodePtr  uidlist;
  ValNodePtr  siplist;
} IdLists, PNTR IdListsPtr;

static void CollectAllSegments (SeqLocPtr slp, Pointer userdata)

{
  BioseqPtr     bsp;
  IdListsPtr    ilp;
  SeqLocPtr     loc;
  SeqIdPtr      sip;
  TextSeqIdPtr  tsip;
  Int4          uid = 0;
  ValNodePtr    vnp;

  if (slp == NULL || userdata == NULL) return;
  ilp = (IdListsPtr) userdata;

  sip = SeqLocId (slp);
  if (sip == NULL) {
    loc = SeqLocFindNext (slp, NULL);
    if (loc != NULL) {
      sip = SeqLocId (loc);
    }
  }
  if (sip == NULL) return;
  if (sip->choice == SEQID_GI) {
    uid = (Int4) sip->data.intvalue;
  } else {
    switch (sip->choice) {
      case SEQID_GENBANK :
      case SEQID_EMBL :
      case SEQID_DDBJ :
      case SEQID_OTHER :
      case SEQID_TPG:
      case SEQID_TPE:
      case SEQID_TPD:

        /* if not gi number, first see if local accession */

        bsp = BioseqFindCore (sip);
        if (bsp != NULL) return;

        tsip = (TextSeqIdPtr) sip->data.ptrvalue;
        if (tsip != NULL) {
          if (tsip->version > 0) {
            uid = GetGIForSeqId (sip);
          }
        }
        break;
      default :
        break;
    }
    if (uid < 1) {
      vnp = ValNodeAddPointer (NULL, 0, (Pointer) sip);
      if (vnp == NULL) return;

      /* if not resolvable to gi number, link in head of sip list */

      vnp->next = ilp->siplist;
      ilp->siplist = vnp;

      return;
    }
  }
  if (uid < 1) return;

  vnp = ValNodeAddInt (NULL, 0, uid);
  if (vnp == NULL) return;

  /* link in head of uid list */

  vnp->next = ilp->uidlist;
  ilp->uidlist = vnp;
}

static void CollectAllBioseqs (BioseqPtr bsp, Pointer userdata)

{
  DeltaSeqPtr  dsp;
  SeqLocPtr    slp = NULL;
  ValNode      vn;

  if (bsp == NULL || userdata == NULL) return;

  if (bsp->repr == Seq_repr_seg) {
    vn.choice = SEQLOC_MIX;
    vn.extended = 0;
    vn.data.ptrvalue = bsp->seq_ext;
    vn.next = NULL;
    while ((slp = SeqLocFindNext (&vn, slp)) != NULL) {
      if (slp != NULL && slp->choice != SEQLOC_NULL) {
        CollectAllSegments (slp, userdata);
      }
    }
  } else if (bsp->repr == Seq_repr_delta) {
    for (dsp = (DeltaSeqPtr) (bsp->seq_ext); dsp != NULL; dsp = dsp->next) {
      if (dsp->choice == 1) {
        slp = (SeqLocPtr) dsp->data.ptrvalue;
        if (slp != NULL && slp->choice != SEQLOC_NULL) {
          CollectAllSegments (slp, userdata);
        }
      }
    }
  }
}

static void CollectAllLocations (SeqFeatPtr sfp, Pointer userdata)

{
  SeqLocPtr  slp = NULL;

  if (sfp == NULL || userdata == NULL || sfp->location == NULL) return;

  while ((slp = SeqLocFindNext (sfp->location, slp)) != NULL) {
    if (slp != NULL && slp->choice != SEQLOC_NULL) {
      CollectAllSegments (slp, userdata);
    }
  }
}

static void CollectAllProducts (SeqFeatPtr sfp, Pointer userdata)

{
  SeqLocPtr  slp = NULL;

  if (sfp == NULL || userdata == NULL || sfp->product == NULL) return;

  while ((slp = SeqLocFindNext (sfp->product, slp)) != NULL) {
    if (slp != NULL && slp->choice != SEQLOC_NULL) {
      CollectAllSegments (slp, userdata);
    }
  }
}

static void CollectAllSublocs (SeqLocPtr loc, Pointer userdata)

{
  SeqLocPtr  slp = NULL;

  if (loc == NULL || userdata == NULL) return;

  while ((slp = SeqLocFindNext (loc, slp)) != NULL) {
    if (slp != NULL && slp->choice != SEQLOC_NULL) {
      CollectAllSegments (slp, userdata);
    }
  }
}

static void FetchFromUidList (
  ValNodePtr PNTR uidlistP,
  ValNodePtr PNTR bsplistP,
  Boolean usethreads,
  Boolean reindexIfBig
)

{
  BioseqPtr   bsp;
  ValNodePtr  sublist, uidlist, vnp;

  if (uidlistP == NULL || bsplistP == NULL) return;

  SortUniqueCleanseUidList (uidlistP, bsplistP);
  sublist = LookupAndExtractBspList (uidlistP, usethreads, reindexIfBig);

  while (sublist != NULL) {

    uidlist = NULL;

    /* recursively queue delta or segmented component uids */

    for (vnp = sublist; vnp != NULL; vnp = vnp->next) {

      bsp = (BioseqPtr) vnp->data.ptrvalue;
      if (bsp == NULL) continue;
      if (bsp->repr != Seq_repr_seg && bsp->repr != Seq_repr_delta) continue;

      CollectAllBioseqs (bsp, (Pointer) &uidlist);
    }

    ValNodeLink (bsplistP, sublist);
    sublist = NULL;

    SortUniqueCleanseUidList (&uidlist, bsplistP);
    sublist = LookupAndExtractBspList (&uidlist, usethreads, reindexIfBig);
  }
}

static void FetchFromSipList (
  ValNodePtr PNTR siplistP,
  ValNodePtr PNTR bsplistP
)

{
  BioseqPtr   bsp;
  SeqIdPtr    sip;
  ValNodePtr  vnp;
  ValNodePtr  vnx;

  if (siplistP == NULL || bsplistP == NULL) return;

  for (vnp = *siplistP; vnp != NULL; vnp = vnp->next) {
    sip = (SeqIdPtr) vnp->data.ptrvalue;
    if (sip == NULL) continue;
    if (BioseqFindCore (sip) != NULL) continue;
    bsp = BioseqLockById (sip);
    if (bsp == NULL) continue;
    vnx = ValNodeAddPointer (NULL, 0, (Pointer) bsp);
    if (vnx == NULL) continue;
    vnx->next = *bsplistP;
    *bsplistP = vnx;
  }
}

static void LookForNonGiSegments (
  SeqLocPtr slp,
  SeqIdPtr sip,
  Pointer userdata
)

{
  BoolPtr    nonGi;
  SeqLocPtr  loc;

  if (slp == NULL && sip == NULL) return;
  if (userdata == NULL) return;
  nonGi = (BoolPtr) userdata;

  if (sip == NULL) {
    sip = SeqLocId (slp);
    if (sip == NULL) {
      loc = SeqLocFindNext (slp, NULL);
      if (loc != NULL) {
        sip = SeqLocId (loc);
      }
    }
  }
  if (sip == NULL) return;

  if (sip->choice != SEQID_GI) {
    *nonGi = TRUE;
  }
}

static void LookForNonGiBioseqs (
  BioseqPtr bsp,
  Pointer userdata
)

{
  DeltaSeqPtr  dsp;
  SeqLocPtr    slp = NULL;
  ValNode      vn;

  if (bsp == NULL) return;

  if (bsp->repr == Seq_repr_seg) {
    vn.choice = SEQLOC_MIX;
    vn.extended = 0;
    vn.data.ptrvalue = bsp->seq_ext;
    vn.next = NULL;
    while ((slp = SeqLocFindNext (&vn, slp)) != NULL) {
      if (slp != NULL && slp->choice != SEQLOC_NULL) {
        LookForNonGiSegments (slp, NULL, userdata);
      }
    }
  } else if (bsp->repr == Seq_repr_delta) {
    for (dsp = (DeltaSeqPtr) (bsp->seq_ext); dsp != NULL; dsp = dsp->next) {
      if (dsp->choice == 1) {
        slp = (SeqLocPtr) dsp->data.ptrvalue;
        if (slp != NULL && slp->choice != SEQLOC_NULL) {
          LookForNonGiSegments (slp, NULL, userdata);
        }
      }
    }
  }
}

static void LookForNonGiLocations (SeqFeatPtr sfp, Pointer userdata)

{
  SeqLocPtr  slp = NULL;

  if (sfp == NULL || userdata == NULL || sfp->location == NULL) return;

  while ((slp = SeqLocFindNext (sfp->location, slp)) != NULL) {
    if (slp != NULL && slp->choice != SEQLOC_NULL) {
      LookForNonGiSegments (slp, NULL, userdata);
    }
  }
}

static void LookForNonGiProducts (SeqFeatPtr sfp, Pointer userdata)

{
  SeqLocPtr  slp = NULL;

  if (sfp == NULL || userdata == NULL || sfp->product == NULL) return;

  while ((slp = SeqLocFindNext (sfp->product, slp)) != NULL) {
    if (slp != NULL && slp->choice != SEQLOC_NULL) {
      LookForNonGiSegments (slp, NULL, userdata);
    }
  }
}

static void LookForNonGiSublocs (SeqLocPtr loc, Pointer userdata)

{
  SeqLocPtr  slp = NULL;

  if (loc == NULL || userdata == NULL) return;

  while ((slp = SeqLocFindNext (loc, slp)) != NULL) {
    if (slp != NULL && slp->choice != SEQLOC_NULL) {
      LookForNonGiSegments (slp, NULL, userdata);
    }
  }
}

NLM_EXTERN ValNodePtr AdvcLockFarComponents (
  SeqEntryPtr sep,
  Boolean components,
  Boolean locations,
  Boolean products,
  SeqLocPtr loc,
  Boolean usethreads
)

{
  ValNodePtr   bsplist = NULL;
  IdLists      ils;
  Boolean      nonGi;
  SeqEntryPtr  oldsep;

  if (sep == NULL) return NULL;
  oldsep = SeqEntrySetScope (sep);

  /* if non-GI components/locations/products, lookup in bulk first */

  if (components) {
    nonGi = FALSE;
    VisitBioseqsInSep (sep, (Pointer) &nonGi, LookForNonGiBioseqs);
    if (nonGi) {
      LookupFarSeqIDs (sep, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE);
    }
  }

  if (locations) {
    nonGi = FALSE;
    VisitFeaturesInSep (sep, (Pointer) &nonGi, LookForNonGiLocations);
    if (nonGi) {
      LookupFarSeqIDs (sep, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE);
    }
  }

  if (products) {
    nonGi = FALSE;
    VisitFeaturesInSep (sep, (Pointer) &nonGi, LookForNonGiProducts);
    if (nonGi) {
      LookupFarSeqIDs (sep, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE);
    }
  }

  if (loc != NULL) {
    nonGi = FALSE;
    LookForNonGiSublocs (loc, (Pointer) &nonGi);
    if (nonGi) {
      LookupFarSeqIDs (sep, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE);
    }
  }

  /* now collect list of GI numbers, lock into memory */

  ils.siplist = NULL;

  if (components) {
    ObjMgrSetHold ();
    SeqMgrHoldIndexing (TRUE);
    ils.uidlist = NULL;
    VisitBioseqsInSep (sep, (Pointer) &ils, CollectAllBioseqs);
    FetchFromUidList (&ils.uidlist, &bsplist, usethreads, FALSE);
    SeqMgrHoldIndexing (FALSE);
    ObjMgrClearHold ();
  }

  if (locations) {
    ObjMgrSetHold ();
    SeqMgrHoldIndexing (TRUE);
    ils.uidlist = NULL;
    VisitFeaturesInSep (sep, (Pointer) &ils, CollectAllLocations);
    FetchFromUidList (&ils.uidlist, &bsplist, usethreads, TRUE);
    SeqMgrHoldIndexing (FALSE);
    ObjMgrClearHold ();
  }

  if (products) {
    ObjMgrSetHold ();
    SeqMgrHoldIndexing (TRUE);
    ils.uidlist = NULL;
    VisitFeaturesInSep (sep, (Pointer) &ils, CollectAllProducts);
    FetchFromUidList (&ils.uidlist, &bsplist, usethreads, TRUE);
    SeqMgrHoldIndexing (FALSE);
    ObjMgrClearHold ();
  }

  if (loc != NULL) {
    ObjMgrSetHold ();
    SeqMgrHoldIndexing (TRUE);
    ils.uidlist = NULL;
    CollectAllSublocs (loc, (Pointer) &ils);
    FetchFromUidList (&ils.uidlist, &bsplist, usethreads, TRUE);
    SeqMgrHoldIndexing (FALSE);
    ObjMgrClearHold ();
  }

  /* process list of non-GI sips, lock into memory */

  if (ils.siplist != NULL) {
    FetchFromSipList (&ils.siplist, &bsplist);

    ValNodeFree (ils.siplist);
  }

  SeqEntrySetScope (oldsep);
  return bsplist;
}

/***************************/

NLM_EXTERN ValNodePtr LockFarComponentsEx (SeqEntryPtr sep, Boolean components, Boolean locations, Boolean products, SeqLocPtr loc)

{
#ifdef OS_UNIX
  CharPtr      str;
#endif

  if (sep == NULL) return NULL;

#ifdef OS_UNIX
  str = getenv ("ADV_LOCK_FAR_COMPONENTS");
  if (str != NULL) {
    if (StringICmp (str, "Multi") == 0) {
      return AdvcLockFarComponents (sep, components, locations, products, loc, TRUE);
    }
  }
#endif

  return AdvcLockFarComponents (sep, components, locations, products, loc, FALSE);
}

NLM_EXTERN ValNodePtr LockFarComponents (SeqEntryPtr sep)

{
  return LockFarComponentsEx (sep, TRUE, FALSE, FALSE, NULL);
}

NLM_EXTERN ValNodePtr UnlockFarComponents (ValNodePtr bsplist)

{
  BioseqPtr   bsp;
  ValNodePtr  vnp;

  if (bsplist == NULL) return NULL;

  ObjMgrSetHold ();

  for (vnp = bsplist; vnp != NULL; vnp = vnp->next) {
    bsp = (BioseqPtr) vnp->data.ptrvalue;
    if (bsp != NULL) {
      BioseqUnlock (bsp);
    }
  }

  ObjMgrClearHold ();

  return ValNodeFree (bsplist);
}

NLM_EXTERN ValNodePtr LockFarAlignmentBioseqs (SeqAlignPtr salp)
{
  ValNodePtr    bsplist = NULL;
  SeqAlignPtr   tmp_salp;
  Int4          alnRows, seq_num, index_num;
  SeqIdPtr      tmp_sip;
  BioseqPtr     bsp;
  ObjMgrDataPtr omdp;
  ObjMgrPtr     omp;
  
  omp = ObjMgrWriteLock();
  if (omp == NULL) return NULL;
  
  for (tmp_salp = salp; tmp_salp != NULL; tmp_salp = tmp_salp->next) {
    alnRows = AlnMgr2GetNumRows(tmp_salp);  /* size of the alignment */
    for (seq_num = 1; seq_num < alnRows + 1; seq_num++) {
      tmp_sip = AlnMgr2GetNthSeqIdPtr(tmp_salp, seq_num);
      bsp = BioseqLockById(tmp_sip);
      if (bsp == NULL) continue;
      index_num = ObjMgrLookup(omp, (Pointer)bsp);
      if (index_num < 0) {
        ValNodeAddPointer (&bsplist, 0, bsp);
      } else {
        omdp = ObjMgrFindTop (omp, omp->datalist[index_num]);
        if (omdp != NULL && omdp->tempload == TL_NOT_TEMP) {
          BioseqUnlock (bsp);
        } else {
          ValNodeAddPointer (&bsplist, 0, bsp);
        }
      }
    }
  }
  ObjMgrUnlock();
  return bsplist;
}

/*****************************************************************************
*
*   SeqMgrSetPreCache
*       registers the GiToSeqID precache function
*   LookupFarSeqIDs
*       calls any registered function to preload the cache
*
*****************************************************************************/

NLM_EXTERN void LIBCALL SeqMgrSetPreCache (SIDPreCacheFunc func)

{
  SeqMgrPtr  smp;

  smp = SeqMgrWriteLock ();
  if (smp == NULL) return;
  smp->seq_id_precache_func = func;
  SeqMgrUnlock ();
}

NLM_EXTERN Int4 LookupFarSeqIDs (
  SeqEntryPtr sep,
  Boolean components,
  Boolean locations,
  Boolean products,
  Boolean alignments,
  Boolean history,
  Boolean inference,
  Boolean others
)

{
  SIDPreCacheFunc  func;
  SeqMgrPtr        smp;

  smp = SeqMgrReadLock ();
  if (smp == NULL) return 0;
  func = smp->seq_id_precache_func;
  SeqMgrUnlock ();
  if (func == NULL) return 0;
  return (*func) (sep, components, locations, products, alignments, history, inference, others);
}

/*****************************************************************************
*
*   SeqMgrSetSeqIdSetFunc
*       registers the GiToSeqIdSet lookup function
*   GetSeqIdSetForGI
*       calls any registered function to lookup the set of SeqIds
*
*****************************************************************************/

NLM_EXTERN void LIBCALL SeqMgrSetSeqIdSetFunc (SeqIdSetLookupFunc func)

{
  SeqMgrPtr  smp;

  smp = SeqMgrWriteLock ();
  if (smp == NULL) return;
  smp->seq_id_set_lookup_func = func;
  SeqMgrUnlock ();
}

NLM_EXTERN SeqIdPtr LIBCALL GetSeqIdSetForGI (Int4 gi)

{
  SeqIdSetLookupFunc  func;
  SeqMgrPtr           smp;

  smp = SeqMgrReadLock ();
  if (smp == NULL) return 0;
  func = smp->seq_id_set_lookup_func;
  SeqMgrUnlock ();
  if (func == NULL) return 0;
  return (*func) (gi);
}

/*****************************************************************************
*
*   SeqMgrSetLenFunc
*       registers the GiToSeqLen lookup function
*   SeqMgrSetAccnVerFunc
*       registers the GiToAccnVer lookup function
*
*****************************************************************************/

NLM_EXTERN void LIBCALL SeqMgrSetLenFunc (SeqLenLookupFunc func)

{
  SeqMgrPtr  smp;

  smp = SeqMgrWriteLock ();
  if (smp == NULL) return;
  smp->seq_len_lookup_func = func;
  SeqMgrUnlock ();
}

NLM_EXTERN void LIBCALL SeqMgrSetAccnVerFunc (AccnVerLookupFunc func)

{
  SeqMgrPtr  smp;

  smp = SeqMgrWriteLock ();
  if (smp == NULL) return;
  smp->accn_ver_lookup_func = func;
  SeqMgrUnlock ();
}

/*******************************************************************
*
*   SeqEntryAsnOut()
*
*       dumps parts of SeqEntry from a memory object
*
*******************************************************************/

typedef struct ext_pack_data {
   SeqEntryPtr  sep [5];
   Uint4        minSapItemID;
   Uint4        maxSapItemID;
   ValNodePtr   descChain;
   ValNodePtr   featChain;
   ValNodePtr   lastVnp;
} ExtPackData, PNTR ExtPackPtr;

static void GetSapBounds (SeqAnnotPtr sap, Pointer userdata)

{
   ExtPackPtr  epp;

   epp = (ExtPackPtr) userdata;
   epp->minSapItemID = MIN (epp->minSapItemID, sap->idx.itemID);
   epp->maxSapItemID = MAX (epp->maxSapItemID, sap->idx.itemID);
}

NLM_EXTERN Boolean SeqEntryAsnOut (SeqEntryPtr sep, SeqIdPtr sip,
                                    Int2 retcode, AsnIoPtr aipout)

{
   BioseqPtr          bsp;
   BioseqSetPtr       bssp;
   SeqMgrFeatContext  context;
   Uint2              entityID;
   ExtPackData        epd;
   SeqEntryPtr        oldscope;
   BioseqSetPtr       parent;
   SeqAnnotPtr        sap;
   SeqDescrPtr        sdp;
   SeqFeatPtr         sfp;
   SeqEntryPtr        top;
   ValNodePtr         vnp;
   AsnOptionPtr       aopp_feat = NULL, aopp_desc = NULL;
   DataVal            dv;

   if (sep == NULL || sip == NULL || aipout == NULL) return FALSE;

   if (retcode > 4) {
     retcode = 0;
   }
   if (retcode < 0) {
     retcode = 0;
   }

   entityID = ObjMgrGetEntityIDForChoice (sep);
   if (entityID < 1) return FALSE;
   top = GetTopSeqEntryForEntityID (entityID);
   if (top == NULL) return FALSE;

   /* indexing sets idx fields, will find features outside of desired 
SeqEntry */

   if (SeqMgrFeaturesAreIndexed (entityID) == 0) {
     SeqMgrIndexFeatures (entityID, NULL);
   }

   /* find Bioseq within entity given SeqId */

   oldscope = SeqEntrySetScope (top);
   bsp = BioseqFind (sip);
   SeqEntrySetScope (oldscope);
   if (bsp == NULL) return FALSE;

   MemSet ((Pointer) &epd, 0, sizeof (ExtPackData));

   /* get parent hierarchy */

   epd.sep [0] = top;
   epd.sep [1] = bsp->seqentry;

   if (bsp->idx.parenttype == OBJ_BIOSEQSET) {
     parent = (BioseqSetPtr) bsp->idx.parentptr;
     while (parent != NULL) {
       switch (parent->_class) {
         case BioseqseqSet_class_nuc_prot :
           epd.sep [3] = parent->seqentry;
           break;
         case BioseqseqSet_class_segset :
           epd.sep [2] = parent->seqentry;
           break;
         case BioseqseqSet_class_pub_set :
           epd.sep [4] = parent->seqentry;
           break;
         default :
           break;
       }
       if (parent->idx.parenttype == OBJ_BIOSEQSET) {
         parent = (BioseqSetPtr) parent->idx.parentptr;
       } else {
         parent = NULL;
       }
     }
   }

   /* get desired SeqEntry given retcode parameter */

   sep = NULL;
   while (retcode >= 0 && sep == NULL) {
     sep = epd.sep [retcode];
     retcode --;
   }
   if (sep == NULL) return FALSE;

   /* get immediate parent of SeqEntry to be returned */

   parent = NULL;
   if (IS_Bioseq (sep)) {
     bsp = (BioseqPtr) sep->data.ptrvalue;
     if (bsp == NULL) return FALSE;
     if (bsp->idx.parenttype == OBJ_BIOSEQSET) {
       parent = (BioseqSetPtr) bsp->idx.parentptr;
     }
   } else if (IS_Bioseq_set (sep)) {
     bssp = (BioseqSetPtr) sep->data.ptrvalue;
     if (bssp == NULL) return FALSE;
     if (bssp->idx.parenttype == OBJ_BIOSEQSET) {
       parent = (BioseqSetPtr) bssp->idx.parentptr;
     }
   }

   /* find itemID range of SeqAnnots within current SeqEntry */

   epd.minSapItemID = UINT4_MAX;
   epd.maxSapItemID = 0;
   VisitAnnotsInSep (sep, (Pointer) &epd, GetSapBounds);

   /* go up parent hierarchy, pointing to applicable descriptors */

   epd.lastVnp = NULL;
   while (parent != NULL) {
     for (sdp = parent->descr; sdp != NULL; sdp = sdp->next) {
       vnp = ValNodeAddPointer (&(epd.lastVnp), 0, (Pointer) sdp);
       if (epd.descChain == NULL) {
         epd.descChain = epd.lastVnp;
       }
       epd.lastVnp = vnp;
     }
     if (parent->idx.parenttype == OBJ_BIOSEQSET) {
       parent = (BioseqSetPtr) parent->idx.parentptr;
     } else {
       parent = NULL;
     }
   }

   /* find features indexed on Bioseq that are packaged outside 
current SeqEntry */

   epd.lastVnp = NULL;
   sfp = SeqMgrGetNextFeature (bsp, NULL, 0, 0, &context);
   while (sfp != NULL) {
     sap = context.sap;
     if (sap != NULL) {
       if (sap->idx.itemID < epd.minSapItemID || sap->idx.itemID > 
epd.maxSapItemID) {
         vnp = ValNodeAddPointer (&(epd.lastVnp), 0, (Pointer) sfp);
         if (epd.featChain == NULL) {
           epd.featChain = epd.lastVnp;
         }
         epd.lastVnp = vnp;
       }
     }
     sfp = SeqMgrGetNextFeature (bsp, sfp, 0, 0, &context);
   }

   /* also need to get features whose products point to the Bioseq */

   sfp = NULL;
   if (ISA_na (bsp->mol)) {
     sfp = SeqMgrGetRNAgivenProduct (bsp, &context);
   } else if (ISA_aa (bsp->mol)) {
     sfp = SeqMgrGetCDSgivenProduct (bsp, &context);
   }
   if (sfp != NULL) {
     sap = context.sap;
     if (sap != NULL) {
       if (sap->idx.itemID < epd.minSapItemID || sap->idx.itemID > 
epd.maxSapItemID) {
         vnp = ValNodeAddPointer (&(epd.lastVnp), 0, (Pointer) sfp);
         if (epd.featChain == NULL) {
           epd.featChain = epd.lastVnp;
         }
         epd.lastVnp = vnp;
       }
     }
   }

   /* now write sep, adding descriptors from descChain and features 
from featChain */

  MemSet(&dv, 0, sizeof(DataVal));  /* zero it out */
  if (epd.descChain)   /* have extra descriptors */
  {
    dv.ptrvalue = (Pointer)(epd.descChain);
    aopp_desc = AsnIoOptionNew(aipout, OP_NCBIOBJSEQ, CHECK_EXTRA_DESC, dv, NULL);
  } 

  if (epd.featChain)   /* have extra features */
  {
    dv.ptrvalue = (Pointer)(epd.featChain);
    aopp_feat = AsnIoOptionNew(aipout, OP_NCBIOBJSEQ, CHECK_EXTRA_FEAT, dv, NULL);
  }

  SeqEntryAsnWrite(sep, aipout, NULL);

   /* clean up valnode chains */

   ValNodeFree (epd.descChain);
   ValNodeFree (epd.featChain);

   return TRUE;
}

/*
static void SeqMgrReport (void)

{
  BioseqPtr                  bsp;
  BioseqPtr PNTR             bspp;
  Int4                       i, num;
  ObjMgrDataPtr              omdp;
  ObjMgrPtr                  omp;
  SeqIdIndexElementPtr PNTR  sipp;
  SeqMgrPtr                  smp;
  Char                       str [41];

  omp = ObjMgrGet ();
  if (omp != NULL) {
    printf ("Currobj %d, totobj %d\n", (int) omp->currobj, (int) omp->totobj);
    fflush (stdout);
  }
  smp = SeqMgrGet ();
  if (smp != NULL) {
    num = smp->BioseqIndexCnt;
    sipp = smp->BioseqIndex;
    printf ("BioseqIndexCnt %ld\n", (long) num);
    fflush (stdout);
    if (sipp == NULL) {
      printf ("sipp is NULL\n");
      fflush (stdout);
    } else {
      for (i = 0; i < num; i++) {
        omdp = sipp [i]->omdp;
        if (omdp != NULL && omdp->bulkIndexFree) {
          printf ("omdp %ld bulkIndexFree flag set\n", (long) i);
          fflush (stdout);
        }
        StringNCpy_0 (str, sipp [i]->str, sizeof (str));
        RevStringUpper (str);
        printf (" %3ld - %s\n", (long) i, str);
        fflush (stdout);
      }
      printf ("-\n");
      fflush (stdout);
      for (i = smp->BioseqIndexCnt; i < smp->BioseqIndexNum; i++) {
        StringNCpy_0 (str, sipp [i]->str, sizeof (str));
        RevStringUpper (str);
        if (! StringHasNoText (str)) {
          printf (" %3ld - %s\n", (long) i, str);
          fflush (stdout);
        }
      }
      printf ("-\n");
      fflush (stdout);
    }
    num = smp->NonIndexedBioseqCnt;
    bspp = smp->NonIndexedBioseq;
    printf ("NonIndexedBioseqCnt %ld\n", (long) num);
    fflush (stdout);
    if (bspp == NULL) {
      printf ("bspp is NULL\n");
      fflush (stdout);
      return;
    }
    for (i = 0; i < num; i++) {
      bsp = bspp [i];
      if (bsp != NULL) {
        SeqIdWrite (bsp->id, str, PRINTID_FASTA_LONG, sizeof (str) - 1);
        printf (" %3ld - %s\n", (long) i, str);
        fflush (stdout);
      } else {
        printf (" %3ld - (null)\n", (long) i);
        fflush (stdout);
      }
    }
  }
  printf ("\n");
  fflush (stdout);
}
*/

typedef int (*FeatureFindCompare) PROTO ((SMFeatItemPtr, CharPtr));

static int FeatureFindCompareLabel (SMFeatItemPtr feat, CharPtr label) 
{
  if (feat == NULL) return -1;
  return StringICmp (feat->label, label);
}

static int FeatureFindCompareLocusTag (SMFeatItemPtr feat, CharPtr label)
{
  GeneRefPtr grp;
  
  if (feat == NULL || feat->sfp == NULL || feat->subtype != FEATDEF_GENE) {
    return -1;
  }
  grp = (GeneRefPtr) feat->sfp->data.value.ptrvalue;
  return StringICmp (grp->locus_tag, label);
}

static Int4 FindArrayPosForFirst 
(SMFeatItemPtr PNTR array, 
 FeatureFindCompare compare_func,
 Int4               num, 
 CharPtr            label,
 Uint1              seqFeatChoice,
 Uint1              featDefChoice)
{
  Int4                L, R;
  Int4                mid;
  SMFeatItemPtr       feat;
  
  if (array == NULL || compare_func == NULL) return -1;
  /* use binary search to find first one */
  L = 0;
  R = num - 1;
  while (L < R) {
    mid = (L + R) / 2;
    feat = array [mid];
    if (feat != NULL && compare_func (feat, label) < 0) {
      L = mid + 1;
    } else {
      R = mid;
    }
  }
  if (R > num) {
    return -1;
  }
  return R;
}

static SeqFeatPtr FindNthFeatureUseMultipleArrays 
(SMFeatItemPtr PNTR PNTR arrays,
 Int4Ptr                 array_sizes,      
 FeatureFindCompare PNTR compare_funcs,
 Int4                    num_arrays,
 CharPtr                 label,
 Uint2                   entityID, 
 BioseqPtr               bsp,
 Uint1                   seqFeatChoice,
 Uint1                   featDefChoice,
 Int4                    n,
 Int4 PNTR               last_found,
 SeqMgrFeatContext PNTR  context)
{
  Int4Ptr firsts;
  Boolean found, already_found;
  SMFeatItemPtr       feat;
  Int4                index = 0, k, leftmost, i2;
  SMFeatItemPtr PNTR  found_list;
  SeqFeatPtr          sfp = NULL;
  ObjMgrDataPtr       omdp;
  
  if (arrays == NULL || array_sizes == NULL || compare_funcs == NULL || num_arrays < 1) return NULL;
  
  omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return NULL;  
  
  found_list = (SMFeatItemPtr PNTR) MemNew (sizeof (SMFeatItemPtr) * n);
  
  /* set up pointers to first match in each array */
  firsts = (Int4Ptr) MemNew (num_arrays * sizeof (Int4));  
  for (k = 0; k < num_arrays; k++) {  
    firsts[k] = FindArrayPosForFirst (arrays[k], 
                                      compare_funcs[k], 
                                      array_sizes[k], label, 
                                      seqFeatChoice, featDefChoice);
    found = FALSE;
    while (!found
           && firsts[k] >= 0 && firsts[k] < array_sizes[k]
           && compare_funcs[k] (arrays[k][firsts[k]], label) == 0) {
      feat = arrays[k][firsts[k]];
      if (feat->sfp != NULL
          && (seqFeatChoice == 0 || feat->sfp->data.choice == seqFeatChoice) 
          && (featDefChoice == 0 || feat->subtype == featDefChoice) 
          && (! feat->ignore)) {
        found = TRUE;
      } else {
        firsts[k]++;
      }
    }
    if (!found) {
      firsts[k] = -1;
    }
  }
  leftmost = 0;
  while (index < n && leftmost != -1) {
    /* find leftmost match first and increment */
    leftmost = -1;
    for (k = 0; k < num_arrays; k++) {
      if (firsts[k] > -1) {        
        if (leftmost == -1 || SortFeatItemListByPos (arrays[k] + firsts[k], arrays[leftmost] + firsts[leftmost]) < 0) {
          leftmost = k;
        }
      }
    }
    if (leftmost > -1) {
      already_found = FALSE;
      for (i2 = 0; i2 < index && !already_found; i2++) {
        if (found_list[i2]->sfp == arrays[leftmost][firsts[leftmost]]->sfp) {
          already_found = TRUE;
        }
      }
      if (!already_found) {
        feat = arrays[leftmost][firsts[leftmost]];
        found_list[index] = feat;  
        sfp = feat->sfp;
        if (context != NULL) {
          context->entityID = entityID;
          context->itemID = feat->itemID;
          context->sfp = feat->sfp;
          context->sap = feat->sap;
          context->bsp = feat->bsp;
          context->label = feat->label;
          context->left = feat->left;
          context->right = feat->right;
          context->dnaStop = feat->dnaStop;
          context->partialL = feat->partialL;
          context->partialR = feat->partialR;
          context->farloc = feat->farloc;
          context->strand = feat->strand;
          context->seqfeattype = sfp->data.choice;
          context->featdeftype = feat->subtype;
          context->numivals = feat->numivals;
          context->ivals = feat->ivals;
          context->userdata = NULL;
          context->omdp = (Pointer) omdp;
          context->index = firsts[leftmost] + 1;
        }      
        index++;
        if (last_found != NULL) {
          *last_found = index;
        }
      }
      /* increment to next in leftmost array */
      firsts[leftmost]++;
      found = FALSE;
      while (!found
             && firsts[leftmost] >= 0 && firsts[leftmost] < array_sizes[leftmost]
             && compare_funcs[leftmost] (arrays[leftmost][firsts[leftmost]], label) == 0) {
        feat = arrays[leftmost][firsts[leftmost]];
        if (feat->sfp != NULL
            && (seqFeatChoice == 0 || feat->sfp->data.choice == seqFeatChoice) 
            && (featDefChoice == 0 || feat->subtype == featDefChoice) 
            && (! feat->ignore)) {
          found = TRUE;
        } else {
          firsts[leftmost]++;
        }
      }
      if (!found) {
        firsts[leftmost] = -1;
      }
    }
  }
  found_list = MemFree (found_list);
  if (index == n) {
    return sfp;
  } else {
    return NULL;
  }
}
 
NLM_EXTERN SeqFeatPtr FindNthGeneOnBspByLabelOrLocusTag 
(BioseqPtr              bsp,
 CharPtr                label,
 Int4                   n,
 Int4 PNTR              last_found,
 SeqMgrFeatContext PNTR context)
{
  ObjMgrDataPtr       omdp;
  BioseqExtraPtr      bspextra;
  Uint2               entityID;
  SMFeatItemPtr PNTR  arrays[2];
  Int4                array_sizes[2];
  FeatureFindCompare  compare_funcs[2]; 
  SeqFeatPtr          sfp = NULL; 
  Int4                num;
  
  if (bsp == NULL || StringHasNoText (label)) return NULL;

  omdp = SeqMgrGetOmdpForBioseq (bsp);
  if (omdp == NULL || omdp->datatype != OBJ_BIOSEQ) return NULL;

  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return NULL;
  num = bspextra->numfeats;

  if (num < 1 || bspextra->featsByLabel == NULL || bspextra->genesByLocusTag == NULL) return NULL;
  

  if (n < 0 || n > bspextra->numfeats) return NULL;

  entityID = ObjMgrGetEntityIDForPointer (omdp->dataptr);
  
  arrays[0] = bspextra->featsByLabel;
  array_sizes[0] = bspextra->numfeats;
  compare_funcs[0] = FeatureFindCompareLabel;
  arrays[1] = bspextra->genesByLocusTag;
  array_sizes[1] = bspextra->numgenes;
  compare_funcs[1] = FeatureFindCompareLocusTag;
  
  sfp = FindNthFeatureUseMultipleArrays (arrays, array_sizes, compare_funcs, 2, label, entityID,
                                         bsp, SEQFEAT_GENE, FEATDEF_GENE, n + 1, last_found,
                                         context);
                                                                                  
  return sfp;
}


static Boolean SeqMgrClearBioseqExtraDataDescriptors (ObjMgrDataPtr omdp)
{
  BioseqExtraPtr  bspextra;

  if (omdp == NULL) return FALSE;
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL) return FALSE;

  /* free sorted arrays of pointers into data blocks */

  bspextra->descrsByID = MemFree (bspextra->descrsByID);
  bspextra->descrsBySdp = MemFree (bspextra->descrsBySdp);
  bspextra->descrsByIndex = MemFree (bspextra->descrsByIndex);

  /* free list of descriptor information */

  bspextra->desclisthead = ValNodeFreeData (bspextra->desclisthead);

  return TRUE;
}


static void SeqMgrClearDescriptorIndexesProc (SeqEntryPtr sep, Pointer mydata, Int4 index, Int2 indent)

{
  BioseqPtr      bsp;
  BioseqSetPtr   bssp;
  ObjMgrDataPtr  omdp = NULL;
  BoolPtr        rsult;

  if (sep == NULL || (! IS_Bioseq (sep))) return;
  if (IS_Bioseq (sep)) {
    bsp = (BioseqPtr) sep->data.ptrvalue;
    if (bsp == NULL) return;
    omdp = SeqMgrGetOmdpForBioseq (bsp);
  } else if (IS_Bioseq_set (sep)) {
    bssp = (BioseqSetPtr) sep->data.ptrvalue;
    if (bssp == NULL) return;
    omdp = SeqMgrGetOmdpForPointer (bssp);
  } else return;
  if (omdp != NULL && SeqMgrClearBioseqExtraDataDescriptors (omdp)) {
    rsult = (BoolPtr) mydata;
    *rsult = TRUE;
  }
}


/* NOTE - this function does NOT do basic seqentry cleanup;
 * it assumes that cleanup has been done already, probably
 * on just the descriptor that was changed.
 */
NLM_EXTERN void SeqMgrRedoDescriptorIndexes (Uint2 entityID, Pointer ptr)

{
  Boolean        rsult = FALSE;
  SeqEntryPtr    sep;

  if (entityID == 0) {
    entityID = ObjMgrGetEntityIDForPointer (ptr);
  }
  if (entityID == 0) return;
  sep = SeqMgrGetTopSeqEntryForEntity (entityID);
  if (sep == NULL) return;
  SeqEntryExplore (sep, (Pointer) (&rsult), SeqMgrClearDescriptorIndexesProc);

  /* finish indexing list of descriptors on each indexed bioseq */

  VisitBioseqsInSep (sep, NULL, RecordDescriptorsInBioseqs);

  if (IS_Bioseq_set (sep)) {
    RecordDescriptorsOnTopSet (sep);
  }

  SeqEntryExplore (sep, NULL, IndexRecordedDescriptors);
}


static void SeqMgrRedoFeatByLabel (ObjMgrDataPtr omdp)
{
  BioseqExtraPtr      bspextra;
  SeqFeatPtr          sfp;
  Int4                i;
  Char                buf [129];
  CharPtr             ptr;

  if (omdp == NULL) return;
  bspextra = (BioseqExtraPtr) omdp->extradata;
  if (bspextra == NULL || bspextra->featsByLabel == NULL) return;

  for (i = 0; i < bspextra->numfeats; i++) {
    sfp = bspextra->featsByLabel[i]->sfp;

    FeatDefLabel (sfp, buf, sizeof (buf) - 1, OM_LABEL_CONTENT);
    ptr = buf;
    if (sfp->data.choice == SEQFEAT_RNA) {
      ptr = StringStr (buf, "RNA-");
      if (ptr != NULL) {
        ptr += 4;
      } else {
        ptr = buf;
      }
    }
    bspextra->featsByLabel[i]->label = MemFree (bspextra->featsByLabel[i]->label);
    bspextra->featsByLabel[i]->label = StringSaveNoNull (ptr);
  }

  StableMergeSort ((VoidPtr) bspextra->featsByLabel, (size_t) bspextra->numfeats, sizeof (SMFeatItemPtr), SortFeatItemListByLabel);
}

static void SeqMgrRedoFeatByLabelProc (SeqEntryPtr sep, Pointer mydata, Int4 index, Int2 indent)

{
  BioseqPtr      bsp;
  BioseqSetPtr   bssp;
  ObjMgrDataPtr  omdp = NULL;

  if (sep == NULL || (! IS_Bioseq (sep))) return;
  if (IS_Bioseq (sep)) {
    bsp = (BioseqPtr) sep->data.ptrvalue;
    if (bsp == NULL) return;
    omdp = SeqMgrGetOmdpForBioseq (bsp);
  } else if (IS_Bioseq_set (sep)) {
    bssp = (BioseqSetPtr) sep->data.ptrvalue;
    if (bssp == NULL) return;
    omdp = SeqMgrGetOmdpForPointer (bssp);
  } else return;
  SeqMgrRedoFeatByLabel (omdp);
}


NLM_EXTERN void SeqMgrRedoFeatByLabelIndexes (Uint2 entityID, Pointer ptr)
{
  Int4    ret;
  SeqEntryPtr sep;

  ret = NlmMutexLockEx (&smp_feat_index_mutex);
  if (ret) {
    ErrPostEx (SEV_FATAL, 0, 0, "SeqMgrIndexFeatures mutex failed [%ld]", (long) ret);
    return;
  }
  if (entityID == 0) {
    entityID = ObjMgrGetEntityIDForPointer (ptr);
  }
  if (entityID != 0) {
    sep = GetTopSeqEntryForEntityID (entityID);
    SeqEntryExplore (sep, NULL, SeqMgrRedoFeatByLabelProc);
  }

  NlmMutexUnlock (smp_feat_index_mutex);
}

