/*   vsmutil.h
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*            National Center for Biotechnology Information (NCBI)
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government do not place any restriction on its use or reproduction.
*  We would, however, appreciate having the NCBI and the author cited in
*  any work or product based on this material
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
* ===========================================================================
*
* File Name:  vsmutil.h
*
* Author:  Jonathan Kans
*
* Version Creation Date:   3/3/95
*
* $Revision: 6.18 $
*
* File Description: 
*
* Modifications:  
* --------------------------------------------------------------------------
*
* ==========================================================================
*/

#ifndef _VSMUTIL_
#define _VSMUTIL_

#include <vibrant.h>


#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************
*
*   The validate window can be populated with errors sent by ErrPostEx by
*       calling ErrSetHandler (ValidErrHook).
*
*****************************************************************************/

typedef void (LIBCALLBACK *ErrNotifyProc) PROTO((
              ErrSev sev, int errcode, int subcode,
              Uint2 entityID, Uint4 itemID, Uint2 itemtype,
              Boolean select, Boolean dblClick, Boolean shftKey));

typedef void (LIBCALLBACK *SequesterProc) PROTO((Uint2 entityID, ValNodePtr bsp_list));
                                                 
extern WindoW CreateValidateWindow (ErrNotifyProc notify, CharPtr title,
                                    FonT font, ErrSev sev, Int2 verbose);
extern WindoW CreateValidateWindowEx (ErrNotifyProc notify, CharPtr title,
                                      FonT font, ErrSev sev, Int2 verbose,
                                      BaseFormPtr bfp, FormActnFunc revalProc,
                                      Boolean okaytosetviewtarget);
extern WindoW CreateValidateWindowExEx (ErrNotifyProc notify, CharPtr title,
                                      FonT font, ErrSev sev, Int2 verbose,
                                      BaseFormPtr bfp, FormActnFunc revalProc, FormActnFunc continueProc,
                                      Boolean okaytosetviewtarget);
extern WindoW CreateValidateWindowExExEx (ErrNotifyProc notify, CharPtr title,
                                      FonT font, ErrSev sev, Int2 verbose,
                                      BaseFormPtr bfp, 
                                      FormActnFunc revalProc, 
                                      FormActnFunc revalNoTaxProc, 
                                      FormActnFunc continueProc,
                                      SequesterProc sequesterProc,
                                      SequesterProc segregateProc,
                                      Boolean okaytosetviewtarget);
extern Boolean IsTaxValidationRequested (WindoW w);
extern void ShowValidateWindow (void);
extern void ShowValidateDoc (void);
extern void HideValidateDoc (void);
extern void RepopulateValidateFilter (void);
extern void ClearValidateWindow (void);
extern void FreeValidateWindow (void);
extern void AppendValidMessage (CharPtr text1, CharPtr text2, CharPtr text3,
                                ErrSev sev, int errcode, int subcode,
                                Uint2 entityID, Uint4 itemID, Uint2 itemtype,
                                CharPtr message, CharPtr expanded, ValNodePtr context);
extern int LIBCALLBACK ValidErrHook PROTO((const ErrDesc *err));
extern Boolean ShouldSetSuppressContext (void);
extern Boolean ShouldSetJustShowAccession (void);

#ifndef WIN_MAC
extern void CreateStdValidatorFormMenus (WindoW w);
#endif

extern void LIBCALLBACK ValidErrCallback (
  ErrSev severity,
  int errcode,
  int subcode,
  Uint2 entityID,
  Uint2 itemtype,
  Uint4 itemID,
  CharPtr accession,
  CharPtr featureID,
  CharPtr message,
  CharPtr objtype,
  CharPtr label,
  CharPtr context,
  CharPtr location,
  CharPtr product,
  Pointer userdata
);

/*****************************************************************************
*
*   The search window can send hits to the replace window by passing
*       AppendReplaceMessage as the gather parameter.
*
*****************************************************************************/

typedef void (LIBCALLBACK *SearchGatherProc) PROTO((
              CharPtr searchFor, CharPtr foundIn, CharPtr label,
              Uint2 entityID, Uint4 itemID, Uint2 itemtype, Uint2 subtype));

extern void CreateSearchWindow (SearchGatherProc gather, CharPtr title, Uint2 entityID);
extern void ShowSearchWindow (void);
extern void FreeSearchWindow (void);

/*****************************************************************************
*
*   The replace window can be sent hits from the search gather function.
*
*****************************************************************************/

typedef void (LIBCALLBACK *ReplaceNotifyProc) PROTO((
              Uint2 entityID, Uint4 itemID, Uint2 itemtype,
              Uint2 subtype, Boolean select, Boolean dblClick));

extern void CreateReplaceWindow (ReplaceNotifyProc notify, CharPtr title,
                                 FonT font, Boolean verbose);
extern void ShowReplaceWindow (void);
extern void FreeReplaceWindow (void);
extern void LIBCALLBACK AppendReplaceMessage (CharPtr searchFor, CharPtr foundIn, CharPtr label,
                                              Uint2 entityID, Uint4 itemID,
                                              Uint2 itemtype, Uint2 subtype);
extern void LIBCALLBACK StdReplaceNotify (Uint2 entityID, Uint4 itemID, Uint2 itemtype,
                                          Uint2 subtype, Boolean select, Boolean dblClick);


extern Boolean WriteBadSpecificHostTable (ValNodePtr bad_biop_list, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* ndef _VSMUTIL_ */
