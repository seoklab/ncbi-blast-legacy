/*   cdrgn.c
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
* File Name:  cdrgn.c
*
* Author:  Jonathan Kans
*
* Version Creation Date:   1/22/95
*
* $Revision: 6.131 $
*
* File Description: 
*
* Modifications:  
* --------------------------------------------------------------------------
* Date     Name        Description of modification
* -------  ----------  -----------------------------------------------------
*
*
* ==========================================================================
*/

#include <cdrgn.h>
#include <biosrc.h>
#include <seqport.h>
#include <gather.h>
#include <tofasta.h>
#include <document.h>
#include <suggslp.h>
#include <salutil.h>
#include <salign.h>
#include <edutil.h>
#include <bspview.h>
#include <toasn3.h>
#include <subutil.h>
#include <explore.h>
#include <pmfapi.h>
#include <seqpanel.h>
#include <sqnutils.h>
#define NLM_GENERATED_CODE_PROTO
#include <objmacro.h>
#include <macroapi.h>

#define CODING_REGION_PAGE    0
#define GENE_PAGE             0
#define PROT_PAGE             0
#define RNA_PAGE              0
#define COMMON_PAGE           1
#define LOCATION_PAGE         2

#define NUM_PAGES  8

typedef struct cdrgnform {
  FEATURE_FORM_BLOCK
  SeqEntryPtr   sep;
  GrouP         pages [NUM_PAGES];
  DialoG        foldertabs;
  Int2          currentPage;
  ButtoN        conceptTransA;
  GrouP         protTextGrp;
  TexT          protNameText;
  TexT          protDescText;
  GrouP         protPromptGrp;
  DialoG        ecProcs;
  Uint2         protEntityID;
  Uint4         protItemID;
  Uint2         protItemtype;
  Boolean       protFound;
  PrompT        protlen;
  DialoG        protseq;
  GrouP         protSeqIdGrp;
  TexT          protSeqIdTxt;
  ButtoN        forceNear;
  ButtoN        launchBtn;
  ButtoN        edProtBtn;
  ButtoN        edProtBtn2;
  ButtoN        edSeqBtn;
  BioseqPtr     usethisbioseq;
  Boolean       autoUpdatePartials;
  Boolean       autoRetranslate;
  Boolean       autoUpdatemRNASpans;
  Boolean       locvisited;
  ButtoN        convertToMiscFeat;
  Boolean       saveAsMiscFeat;
  ButtoN        makemRNAMatch;
} CdRgnForm, PNTR CdRgnFormPtr;

typedef struct cdrgnpage {
  DIALOG_MESSAGE_BLOCK
  ButtoN             orf;
  PopuP              frame;
  ButtoN             conflict;
  PopuP              geneticCode;
  DialoG             cdBrk;
  Int2               count;
  EnumFieldAssoc     PNTR alist;
  EnumFieldAssocPtr  alists [2];
  GrouP              cdRgnGrp [4];
  GrouP              commonGrp;
  Int2               currentPage;
  CdRgnFormPtr       cfp;
} CdRgnPage, PNTR CdRgnPagePtr;

typedef struct rawbioseqpage {
  DIALOG_MESSAGE_BLOCK
  ByteStorePtr  bs;
  Uint1         mol;
  Uint1         repr;
  Uint1         seq_data_type;
  DoC           doc;
  CdRgnFormPtr  cfp;
} RawBioseqPage, PNTR RawBioseqPagePtr;

/* rna moved here to be available in product page */

typedef struct rnaform {
  FEATURE_FORM_BLOCK
  SeqEntryPtr   sep;
  GrouP         pages [NUM_PAGES];
  DialoG        foldertabs;
  Int2          currentPage;
  Boolean       empty_feat;
} RnaForm, PNTR RnaFormPtr;

typedef struct rnapage {
  DIALOG_MESSAGE_BLOCK
  PopuP         type;
  ButtoN        pseudo;
  GrouP         nameGrp;
  PrompT        rrnaPrompt;
  PrompT        ornaPrompt;
  TexT          name;
  GrouP         trnaGrp;
  PopuP         AAitem;
  DialoG        codons;
  DialoG        anticodon;
  GrouP         trnaPages [3];
  RnaFormPtr    rfp;

  /* for ncRNA page */
  GrouP         ncrnaGrp;
  DialoG        ncrnaClass;
  TexT          ncrnaProduct;

  /* for tmRNA page */
  GrouP         tmrnaGrp;
  TexT          tmrnaProduct;
  DialoG        tmrnaTagPeptide;
  
  /* for switching between types */
  Int4          prev_rna_type;
} RnaPage, PNTR RnaPagePtr;


static ParData brParFmt = {FALSE, FALSE, FALSE, FALSE, FALSE, 0, 0};
static ColData brColFmt = {0, 0, 80, 0, NULL, 'l', FALSE, FALSE, FALSE, FALSE, TRUE};

static Boolean RawFastaFileFunc (BioseqPtr bsp, Int2 key, CharPtr buf, Uint4 buflen, Pointer data)

{
  FILE  *fp;

  fp = (FILE *) data;
  switch (key) {
    case FASTA_SEQLINE :
      fprintf (fp, "%s\n", buf);
      break;
    default :
      break;
  }
  return TRUE;
}

static void BioseqPtrToRawBioseqPage (DialoG d, Pointer data)

{
  BioseqPtr         bsp;
  Char              buf [256];
  FILE              *fp;
  MyFsa             mfa;
  ErrSev            oldErrSev;
  Char              path [PATH_MAX];
  RecT              r;
  RawBioseqPagePtr  rbpp;

  rbpp = (RawBioseqPagePtr) GetObjectExtra (d);
  bsp = (BioseqPtr) data;
  if (rbpp != NULL) {
    rbpp->bs = BSFree (rbpp->bs);
    Reset (rbpp->doc);
    if (bsp != NULL) {
      if ((bsp->repr == Seq_repr_raw || bsp->repr == Seq_repr_const) &&
          bsp->seq_data_type != Seq_code_gap) {
        rbpp->bs = BSDup ((ByteStorePtr) bsp->seq_data);
        rbpp->seq_data_type = bsp->seq_data_type;
        rbpp->mol = bsp->mol;
        rbpp->repr = bsp->repr;
        TmpNam (path);
        fp = FileOpen (path, "w");
        if (fp != NULL) {
          MemSet ((Pointer) (&mfa), 0, sizeof (MyFsa));
          mfa.buf = buf;
          mfa.buflen = 255;
          mfa.seqlen = 50;
          mfa.mydata = (Pointer) fp;
          mfa.myfunc = RawFastaFileFunc;
          mfa.bad_asn1 = FALSE;
          mfa.order = 0;
          mfa.accession = NULL;
          mfa.organism = NULL;
          mfa.tech = 0;
          mfa.do_virtual = FALSE;
          mfa.formatdb = TRUE;

          if (ISA_na(bsp->mol))
            mfa.code = Seq_code_iupacna;
          else
            mfa.code = Seq_code_ncbieaa;

          oldErrSev = ErrSetMessageLevel (SEV_MAX);
          /*
          BioseqToFastaX (bsp, &mfa, ISA_na (bsp->mol));
          */
          BioseqFastaStream (bsp, fp, STREAM_EXPAND_GAPS | STREAM_CORRECT_INVAL, 50, 0, 0, FALSE);
          ErrSetMessageLevel (oldErrSev);
        }
        FileClose (fp);
        ObjectRect (rbpp->doc, &r);
        InsetRect (&r, 4, 4);
        brColFmt.pixWidth = r.right - r.left;
        DisplayFancy (rbpp->doc, path, &brParFmt, &brColFmt, programFont, 4);
        FileRemove (path);
      }
    }
    Update ();
  }
}

static Pointer RawBioseqPageToBioseqPtr (DialoG d)

{
  BioseqPtr         bsp;
  Char              ch;
  CdRgnFormPtr      cfp;
  Int4              cnt;
  Int4              cntr;
  Int2              i;
  Char              ids [32];
  Int4              nxt;
  Char              str [102];
  RawBioseqPagePtr  rbpp;

  bsp = NULL;
  rbpp = (RawBioseqPagePtr) GetObjectExtra (d);
  if (rbpp != NULL) {
    bsp = BioseqNew ();
    if (bsp != NULL) {
      cfp = rbpp->cfp;
      ids [0] = '\0';
      if (cfp != NULL) {
        if (Visible (cfp->protSeqIdGrp)) {
          GetTitle (cfp->protSeqIdTxt, ids, sizeof (ids) - 1);
        }
      }
      if (StringHasNoText (ids)) {
        if (ISA_na (rbpp->mol)) {
          bsp->id = MakeSeqID ("dna_seq");
        } else if (ISA_aa (rbpp->mol)) {
          bsp->id = MakeSeqID ("prt_seq");
        } else {
          bsp->id = MakeSeqID ("oth_seq");
        }
      } else {
        bsp->id = MakeSeqID (ids);
      }
      bsp->mol = rbpp->mol;
      bsp->repr = rbpp->repr;
      /*
      bsp->seq_data_type = ISA_na (bsp->mol) ? Seq_code_iupacna : Seq_code_ncbieaa;
      */
      bsp->seq_data_type = rbpp->seq_data_type;
      bsp->length = BSLen (rbpp->bs);
      if (bsp->length == 0) {
        bsp = BioseqFree (bsp);
        return NULL;
      }
      bsp->seq_data = (SeqDataPtr) BSNew (bsp->length);
      if (bsp->seq_data != NULL) {
        BSSeek (rbpp->bs, 0, SEEK_SET);
        cntr = BSLen (rbpp->bs);
        cnt = MIN (cntr, 100L);
        while (cnt > 0) {
          cntr -= cnt;
          nxt = MIN (cntr, 100L);
          BSRead (rbpp->bs, str, cnt * sizeof (Char));
          for (i = 0; i < cnt; i++) {
            ch = str [i];
            str [i] = TO_UPPER (ch);
          }
          if (nxt == 0 && cnt > 0 && bsp->seq_data_type == Seq_code_ncbieaa) {
            if (str [cnt - 1] == '*') {
              cnt--;
              if (bsp->length > 0) {
                (bsp->length)--;
              }
            }
          }
          BSWrite ((ByteStorePtr) bsp->seq_data, str, cnt * sizeof (Char));
          cnt = nxt;
        }
      }
      /*
      if (bsp->seq_data_type == Seq_code_ncbieaa) {
        BioseqRawConvert (bsp, Seq_code_iupacaa);
      }
      */
      SeqMgrAddToBioseqIndex (bsp);
    }
  }
  return (Pointer) bsp;
}

static void CleanupRawBioseqPage (GraphiC g, VoidPtr data)

{
  RawBioseqPagePtr  rbpp;

  rbpp = (RawBioseqPagePtr) data;
  if (rbpp != NULL) {
    BSFree (rbpp->bs);
  }
  MemFree (data);
}

static DialoG CreateBioseqRawDialog (GrouP h, CharPtr title, Uint1 mol, CdRgnFormPtr cfp)

{
  Int2              charwid;
  RawBioseqPagePtr  rbpp;
  GrouP             m;
  GrouP             p;
  GrouP             s;

  p = HiddenGroup (h, 1, 0, NULL);
  SetGroupSpacing (p, 10, 10);

  rbpp = (RawBioseqPagePtr) MemNew (sizeof (RawBioseqPage));
  if (rbpp != NULL) {

    SetObjectExtra (p, rbpp, CleanupRawBioseqPage);
    rbpp->dialog = (DialoG) p;
    rbpp->todialog = BioseqPtrToRawBioseqPage;
    rbpp->fromdialog = RawBioseqPageToBioseqPtr;
    rbpp->testdialog = NULL;

    if (title != NULL && title [0] != '\0') {
      s = NormalGroup (p, 0, -2, title, systemFont, NULL);
    } else {
      s = HiddenGroup (p, 0, -2, NULL);
    }
    m = HiddenGroup (s, -1, 0, NULL);
    SetGroupSpacing (m, 10, 10);

    rbpp->cfp = cfp;
    rbpp->mol = mol;
    SelectFont (programFont);
    charwid = CharWidth ('0');
    SelectFont (systemFont);
    rbpp->doc = DocumentPanel (m, charwid * 50 + 2, stdLineHeight * 6);
  }

  return (DialoG) p;
}

static void ConvertToAACodeBreak (CdRgnFormPtr cfp, CdRegionPtr crp, CodeBreakPtr cb)

{
  SeqFeatPtr  sfp;
  SeqLocPtr   slp;

  if (cfp == NULL || crp == NULL || cb == NULL) return;
  sfp = SeqFeatNew ();
  if (sfp == NULL) return;
  sfp->data.choice = SEQFEAT_CDREGION;
  sfp->data.value.ptrvalue = crp;
  sfp->location = DialogToPointer (cfp->location);
  sfp->product = DialogToPointer (cfp->product);
  while (cb != NULL) {
    slp = dnaLoc_to_aaLoc (sfp, cb->loc, TRUE, NULL, TRUE);
    cb->loc = SeqLocFree (cb->loc);
    cb->loc = slp;
    cb = cb->next;
  }
  SeqLocFree (sfp->product);
  SeqLocFree (sfp->location);
  MemFree (sfp);
}

static void ConvertToNTCodeBreak (CdRgnFormPtr cfp, CdRegionPtr crp, CodeBreakPtr cb)

{
  SeqFeatPtr  sfp;
  SeqLocPtr   slp;

  if (cfp == NULL || crp == NULL || cb == NULL) return;
  sfp = SeqFeatNew ();
  if (sfp == NULL) return;
  sfp->data.choice = SEQFEAT_CDREGION;
  sfp->data.value.ptrvalue = crp;
  sfp->location = DialogToPointer (cfp->location);
  sfp->product = DialogToPointer (cfp->product);
  while (cb != NULL) {
    slp = aaLoc_to_dnaLoc (sfp, cb->loc);
    cb->loc = SeqLocFree (cb->loc);
    cb->loc = slp;
    cb = cb->next;
  }
  SeqLocFree (sfp->product);
  SeqLocFree (sfp->location);
  MemFree (sfp);
}

static void CdRegionPtrToCdRgnPage (DialoG d, Pointer data)

{
  CodeBreakPtr    cbhead;
  CodeBreakPtr    cbp;
  CdRgnPagePtr    cpp;
  CdRegionPtr     crp;
  CodeBreakPtr    curr;
  GeneticCodePtr  gcp;
  Int4            id;
  CodeBreakPtr    last;
  Char            name [256];
  ValNodePtr      tmp;
  SeqFeatPtr      mrna;
  SeqLocPtr       slp;
  SeqMgrFeatContext fcontext;

  cpp = (CdRgnPagePtr) GetObjectExtra (d);
  crp = (CdRegionPtr) data;
  if (cpp != NULL) {
    if (crp != NULL) {
      SetStatus (cpp->orf, crp->orf);
      if (crp->orf) {
        SafeShow (cpp->orf);
      }
      SetStatus (cpp->conflict, crp->conflict);
      if (crp->conflict) {
        SafeShow (cpp->conflict);
      }
      SetValue (cpp->frame, crp->frame + 1);
      id = 0;
      name [0] = '\0';
      gcp = crp->genetic_code;
      if (gcp != NULL) {
        tmp = (ValNodePtr) gcp->data.ptrvalue;
        for (tmp = (ValNodePtr) gcp->data.ptrvalue; tmp != NULL; tmp = tmp->next) {
          switch (tmp->choice) {
            case 1 :
              if (name [0] == '\0') {
                StringNCpy_0 (name, (CharPtr) tmp->data.ptrvalue, sizeof (name));
              }
              break;
            case 2 :
              id = tmp->data.intvalue;
              break;
            default :
              break;
          }
        }
        if (id == 0) {
          gcp = GeneticCodeFind (id, name);
          if (gcp != NULL) {
            for (tmp = (ValNodePtr) gcp->data.ptrvalue; tmp != NULL; tmp = tmp->next) {
              switch (tmp->choice) {
                case 2 :
                  id = tmp->data.intvalue;
                  break;
                default :
                  break;
              }
            }
          }
        }
      }
      if (id > 0 && id < NUM_GENETIC_CODES) {
        SetValue (cpp->geneticCode, gcIdToIndex [(Int2) id]);
      } else {
        SetValue (cpp->geneticCode, 1);
      }
      cbhead = NULL;
      last = NULL;
      curr = crp->code_break;
      while (curr != NULL) {
        cbp = AsnIoMemCopy ((Pointer) curr,
                            (AsnReadFunc) CodeBreakAsnRead,
                            (AsnWriteFunc) CodeBreakAsnWrite);
        if (cbhead == NULL) {
          cbhead = cbp;
        }
        if (last != NULL) {
          last->next = cbp;
        }
        last = cbp;
        curr = curr->next;
      }
      ConvertToAACodeBreak (cpp->cfp, crp, cbhead);
      PointerToDialog (cpp->cdBrk, cbhead);
      CodeBreakFree (cbhead);

    } else {
      SafeSetStatus (cpp->orf, FALSE);
      SafeSetStatus (cpp->conflict, FALSE);
      SafeSetValue (cpp->frame, 1);
      SafeSetValue (cpp->geneticCode, 1);
    }
    /* look for overlapping mRNA */
    slp = DialogToPointer (cpp->cfp->location);
    mrna = SeqMgrGetOverlappingmRNA (slp, &fcontext);
    slp = SeqLocFree (slp);
    if (mrna == NULL) {
      Hide (cpp->cfp->makemRNAMatch);
    } else {
      Show (cpp->cfp->makemRNAMatch);
    }

  }
}

static SeqIdPtr  globalid;

static CodeBreakPtr RemoveNullLocationCodeBreaks (CodeBreakPtr cbp)
{
  CodeBreakPtr this_cbp, prev_cbp, next_cbp;
  
  prev_cbp = NULL;
  for (this_cbp = cbp; this_cbp != NULL; this_cbp = next_cbp)
  {
      next_cbp = this_cbp->next;
      if (this_cbp->loc == NULL)
      {
        Message (MSG_ERROR, "A code break was removed because the location is now invalid");
        if (prev_cbp == NULL)
        {
            cbp = this_cbp->next;
        }
        else
        {
            prev_cbp->next = this_cbp->next;
        }
        this_cbp->next = NULL;
        CodeBreakFree (this_cbp);
      }
  }
  return cbp;
}

static Pointer CdRgnPageToCdRegionPtr (DialoG d)

{
  BioseqPtr     bsp = NULL;
  CodeBreakPtr  cbp;
  CdRgnFormPtr  cfp;
  ValNodePtr    code;
  CdRgnPagePtr  cpp;
  CdRegionPtr   crp;
  ImpFeatPtr    ifp;
  SeqIdPtr      sip;
  SeqLocPtr     slp;
  Int2          val;
  ValNodePtr    vnp;

  crp = NULL;
  cpp = (CdRgnPagePtr) GetObjectExtra (d);
  if (cpp != NULL) {
    cfp = cpp->cfp;
    if (cfp != NULL && cfp->saveAsMiscFeat) {
      ifp = ImpFeatNew ();
      if (ifp != NULL) {
        ifp->key = StringSave ("misc_feature");
      }
      return (Pointer) ifp;
    }
    crp = CdRegionNew ();
    if (crp != NULL) {
      crp->orf = GetStatus (cpp->orf);
      crp->conflict = GetStatus (cpp->conflict);
      crp->frame = (Uint1) GetValue (cpp->frame) - 1;
      crp->gaps = 0;
      crp->mismatch = 0;
      crp->stops = 0;
      val = gcIndexToId [GetValue (cpp->geneticCode)];
      if (val == 0) {
        val = 1;  /* default to standard genetic code if not set */
      }
      if (val > 0 && val < NUM_GENETIC_CODES) {
        code = ValNodeNew (NULL);
        if (code != NULL) {
          code->choice = 254;
          vnp = ValNodeNew (NULL);
          code->data.ptrvalue = vnp;
          if (vnp != NULL) {
            vnp->choice = 2;
            vnp->data.intvalue = (Int4) val;
          }
        }
        crp->genetic_code = code;
        globalid = NULL;
        cfp = cpp->cfp;
        if (cfp != NULL) {
          slp = DialogToPointer (cfp->product);
          if (slp != NULL) {
            sip = SeqLocId (slp);
            if (sip != NULL) {
              bsp = BioseqFind (sip);
              if (bsp != NULL) {
                globalid = SeqIdFindBest (bsp->id, 0);
              }
            }
            SeqLocFree (slp);
          }
        }
        if (globalid != NULL) {
          cbp = DialogToPointer (cpp->cdBrk);
          ConvertToNTCodeBreak (cpp->cfp, crp, cbp);
          /* remove any code breaks that have fallen off the end of the protein */
          cbp = RemoveNullLocationCodeBreaks (cbp);
          crp->code_break = cbp;
        }
      }
    }
  }
  return (Pointer) crp;
}

static void PopulateGeneticCodePopup (PopuP gc)

{
  Int2  i;

  if (gc != NULL) {
    PopupItem (gc, " ");
    for (i = 1; i <= numGeneticCodes; i++) {
      PopupItem (gc, gcNames [i]);
    }
  }
}

static void CleanupCdRgnPage (GraphiC g, VoidPtr data)

{
  CdRgnPagePtr  cpp;
  Int2          j;

  cpp = (CdRgnPagePtr) data;
  if (cpp != NULL) {
    if (cpp->alist != NULL) {
      for (j = 0; j < cpp->count; j++) {
        MemFree (cpp->alist [j].name);
      }
    }
    MemFree (cpp->alist);
  }
  MemFree (data);
}

static void SetCdRgnImportExportItems (CdRgnFormPtr cfp)

{
  CdRgnPagePtr  cpp;
  IteM          exportItm;
  IteM          importItm;

  if (cfp != NULL) {
    importItm = FindFormMenuItem ((BaseFormPtr) cfp, VIB_MSG_IMPORT);
    exportItm = FindFormMenuItem ((BaseFormPtr) cfp, VIB_MSG_EXPORT);
    switch (cfp->currentPage) {
      case CODING_REGION_PAGE :
        cpp = (CdRgnPagePtr) GetObjectExtra (cfp->data);
        if (cpp != NULL) {
          if (cpp->currentPage == 0) {
            SafeSetTitle (importItm, "Import Protein FASTA...");
            SafeSetTitle (exportItm, "Export Protein FASTA...");
            SafeEnable (importItm);
            SafeEnable (exportItm);
          } else {
            SafeSetTitle (importItm, "Import...");
            SafeSetTitle (exportItm, "Export...");
            SafeDisable (importItm);
            SafeDisable (exportItm);
          }
        } else {
          SafeSetTitle (importItm, "Import...");
          SafeSetTitle (exportItm, "Export...");
          SafeDisable (importItm);
          SafeDisable (exportItm);
        }
        break;
      case COMMON_PAGE :
        SafeSetTitle (importItm, "Import...");
        SafeSetTitle (exportItm, "Export...");
        SafeDisable (importItm);
        SafeDisable (exportItm);
        break;
      case LOCATION_PAGE :
        SafeSetTitle (importItm, "Import SeqLoc...");
        SafeSetTitle (exportItm, "Export SeqLoc...");
        SafeEnable (importItm);
        SafeEnable (exportItm);
        break;
      default :
        break;
    }
  }
}

static CharPtr cdRgnTabs [] = {
  "Product", "Protein", "Exceptions", "Misc", NULL
};

static void ChangeCdRgnSubPage (VoidPtr data, Int2 newval, Int2 oldval)

{
  CdRgnPagePtr  cpp;

  cpp = (CdRgnPagePtr) data;
  if (cpp != NULL) {
    cpp->currentPage = newval;
    if (oldval >= 0 && oldval <= 3) {
      SafeHide (cpp->cdRgnGrp [oldval]);
    }
    /*
    if (newval == 3) {
      SafeHide (cpp->commonGrp);
    }
    */
    if (newval >= 0 && newval <= 3) {
      SafeShow (cpp->cdRgnGrp [newval]);
    }
    /*
    if (newval >= 0 && newval < 3) {
      SafeShow (cpp->commonGrp);
    }
    */
    SetCdRgnImportExportItems (cpp->cfp);
    Update ();
  }
}

static Boolean ProtViewLaunchFunc (GatherContextPtr gcp)

{
  BioseqPtr        bsp;
  Int2             handled;
  SeqViewProcsPtr  svpp;

  if (gcp == NULL) return TRUE;
  bsp = (BioseqPtr) gcp->userdata;
  if (bsp == NULL) return TRUE;
  if (gcp->thistype == OBJ_BIOSEQ) {
    if (bsp == (BioseqPtr) gcp->thisitem) {
      WatchCursor ();
      svpp = (SeqViewProcsPtr) GetAppProperty ("SeqDisplayForm");
      if (svpp != NULL) {
        svpp->forceSeparateViewer = TRUE;
      }
      handled = GatherProcLaunch (OMPROC_VIEW, FALSE, gcp->entityID, gcp->itemID,
                                  OBJ_BIOSEQ, 0, 0, OBJ_BIOSEQ, 0);
      ArrowCursor ();
      if (handled != OM_MSG_RET_DONE || handled == OM_MSG_RET_NOPROC) {
        Message (MSG_ERROR, "Unable to launch viewer on protein product.");
      }
      return FALSE;
    }
  }
  return TRUE;
}

static void LaunchProteinViewer (ButtoN b)

{
  BioseqPtr     bsp;
  CdRgnFormPtr  cfp;
  GatherScope   gs;
  SeqIdPtr      sip;
  SeqLocPtr     slp;

  cfp = (CdRgnFormPtr) GetObjectExtra (b);
  if (cfp != NULL) {
    slp = DialogToPointer (cfp->product);
    if (slp != NULL) {
      sip = SeqLocId (slp);
      if (sip != NULL) {
        bsp = BioseqFind (sip);
        if (bsp != NULL) {
          MemSet ((Pointer) (&gs), 0, sizeof (GatherScope));
          gs.seglevels = 1;
          gs.get_feats_location = TRUE;
          MemSet((Pointer)(gs.ignore), (int)(TRUE), (size_t)(OBJ_MAX * sizeof(Boolean)));
          gs.ignore[OBJ_BIOSEQ] = FALSE;
          gs.ignore[OBJ_BIOSEQ_SEG] = FALSE;
          GatherEntity (cfp->input_entityID, (Pointer) bsp, ProtViewLaunchFunc, &gs);
        }
      }
      SeqLocFree (slp);
    }
  }
}

static Boolean EdProtSeqLaunchFunc (GatherContextPtr gcp)

{
  BioseqPtr  bsp;
  Int2       handled;

  if (gcp == NULL) return TRUE;
  bsp = (BioseqPtr) gcp->userdata;
  if (bsp == NULL) return TRUE;
  if (gcp->thistype == OBJ_BIOSEQ) {
    if (bsp == (BioseqPtr) gcp->thisitem) {
      WatchCursor ();
      handled = GatherProcLaunch (OMPROC_EDIT, FALSE, gcp->entityID, gcp->itemID,
                                  OBJ_BIOSEQ, OBJ_BIOSEQ, Seq_repr_raw, OBJ_BIOSEQ, Seq_repr_raw);
      ArrowCursor ();
      if (handled != OM_MSG_RET_DONE || handled == OM_MSG_RET_NOPROC) {
        Message (MSG_ERROR, "Unable to launch editor on protein product.");
      }
      return FALSE;
    }
  }
  return TRUE;
}

static void LaunchProtSeqEd (ButtoN b)

{
  BioseqPtr     bsp;
  CdRgnFormPtr  cfp;
  GatherScope   gs;
  SeqIdPtr      sip;
  SeqLocPtr     slp;

  cfp = (CdRgnFormPtr) GetObjectExtra (b);
  if (cfp != NULL) {
    slp = DialogToPointer (cfp->product);
    if (slp != NULL) {
      sip = SeqLocId (slp);
      if (sip != NULL) {
        bsp = BioseqFind (sip);
        if (bsp != NULL) {
Message (MSG_OK, "Please close and reopen the coding region window after editing the sequence");
          MemSet ((Pointer) (&gs), 0, sizeof (GatherScope));
          gs.seglevels = 1;
          gs.get_feats_location = TRUE;
          MemSet((Pointer)(gs.ignore), (int)(TRUE), (size_t)(OBJ_MAX * sizeof(Boolean)));
          gs.ignore[OBJ_BIOSEQ] = FALSE;
          gs.ignore[OBJ_BIOSEQ_SEG] = FALSE;
          GatherEntity (cfp->input_entityID, (Pointer) bsp, EdProtSeqLaunchFunc, &gs);
        }
      }
      SeqLocFree (slp);
    }
  }
}

static void LaunchProtFeatEd (ButtoN b)

{
  CdRgnFormPtr  cfp;
  Int2          handled;

  cfp = (CdRgnFormPtr) GetObjectExtra (b);
  if (cfp != NULL && cfp->protFound) {
    WatchCursor ();
    SafeHide (cfp->protTextGrp);
    SafeShow (cfp->protPromptGrp);
    Update ();
    handled = GatherProcLaunch (OMPROC_EDIT, FALSE, cfp->protEntityID, cfp->protItemID,
                                cfp->protItemtype, 0, 0, cfp->protItemtype, 0);
    ArrowCursor ();
    Update ();
    if (handled != OM_MSG_RET_DONE || handled == OM_MSG_RET_NOPROC) {
      Message (MSG_ERROR, "Unable to launch editor on protein feature.");
    }
  }
}

static void ChangeCdRgnPage (VoidPtr data, Int2 newval, Int2 oldval)

{
  CdRgnFormPtr  cfp;

  cfp = (CdRgnFormPtr) data;
  if (cfp != NULL) {
    cfp->currentPage = newval;
    SafeHide (cfp->pages [oldval]);
    SafeShow (cfp->pages [newval]);
    switch (newval) {
      case CODING_REGION_PAGE :
        SendMessageToDialog (cfp->data, VIB_MSG_ENTER);
        break;
      case COMMON_PAGE :
        break;
      case LOCATION_PAGE :
        SendMessageToDialog (cfp->location, VIB_MSG_ENTER);
        cfp->locvisited = TRUE;
        break;
      default :
        break;
    }
    SetCdRgnImportExportItems (cfp);
    Update ();
  }
}

typedef struct protgatherlist {
  ObjMgrPtr  omp;
  SeqLocPtr  slp;
  Int4       min;
  CharPtr    protName;
  CharPtr    protDesc;
  Uint2      entityID;
  Uint4      itemID;
  Uint2      itemtype;
  Boolean    protFound;
} ProtGatherList, PNTR ProtGatherPtr;

static Boolean ProtMatchFunc (GatherContextPtr gcp)

{
  Int4           diff;
  ObjMgrTypePtr  omtp;
  ProtGatherPtr  pgp;
  ProtRefPtr     prp;
  SeqFeatPtr     sfp;
  Char           thislabel [41];
  ValNodePtr     vnp;

  if (gcp == NULL) return TRUE;

  pgp = (ProtGatherPtr) gcp->userdata;
  if (pgp == NULL) return TRUE;

  thislabel [0] = '\0';

  if (gcp->thistype == OBJ_SEQFEAT) {
    sfp = (SeqFeatPtr) gcp->thisitem;
    if (sfp != NULL && sfp->data.choice == SEQFEAT_PROT && sfp->data.value.ptrvalue != NULL) {
      omtp = ObjMgrTypeFind (pgp->omp, gcp->thistype, NULL, NULL);
      if (omtp == NULL) {
        return TRUE;
      }
      if (omtp->labelfunc != NULL) {
        (*(omtp->labelfunc)) (gcp->thisitem, thislabel, 40, OM_LABEL_CONTENT);
      }
      if (thislabel [0] != '\0') {
        diff = SeqLocAinB (sfp->location, pgp->slp);
        if (diff >= 0) {
          if (diff < pgp->min) {
            pgp->min = diff;
            prp = (ProtRefPtr) sfp->data.value.ptrvalue;
            pgp->protName = MemFree (pgp->protName);
            pgp->protDesc = MemFree (pgp->protDesc);
            vnp = prp->name;
            if (vnp != NULL) {
              pgp->protName = StringSave ((CharPtr) vnp->data.ptrvalue);
            }
            pgp->protDesc = StringSave (prp->desc);
            pgp->entityID = gcp->entityID;
            pgp->itemID = gcp->itemID;
            pgp->itemtype = gcp->thistype;
            pgp->protFound = TRUE;
          }
        }
      }
    }
  }

  return TRUE;
}

static void SetBestProteinFeature (CdRgnFormPtr cfp, SeqLocPtr product)

{
  GatherScope     gs;
  ProtGatherList  pgl;

  if (cfp == NULL) return;
  pgl.protName = NULL;
  pgl.protDesc = NULL;
  pgl.entityID = 0;
  pgl.itemID = 0;
  pgl.itemtype = 0;
  pgl.protFound = FALSE;
  if (product != NULL) {
    pgl.omp = ObjMgrGet ();
    pgl.slp = product;
    pgl.min = INT4_MAX;
    MemSet ((Pointer) (&gs), 0, sizeof (GatherScope));
    gs.seglevels = 1;
    gs.get_feats_location = TRUE;
    MemSet((Pointer)(gs.ignore), (int)(TRUE), (size_t)(OBJ_MAX * sizeof(Boolean)));
    gs.ignore[OBJ_BIOSEQ] = FALSE;
    gs.ignore[OBJ_BIOSEQ_SEG] = FALSE;
    gs.ignore[OBJ_SEQFEAT] = FALSE;
    gs.ignore[OBJ_SEQANNOT] = FALSE;
    GatherEntity (cfp->input_entityID, (Pointer) &pgl, ProtMatchFunc, &gs);
  }
  SafeSetTitle (cfp->protNameText, pgl.protName);
  SafeSetTitle (cfp->protDescText, pgl.protDesc);
  cfp->protEntityID = pgl.entityID;
  cfp->protItemID = pgl.itemID;
  cfp->protItemtype = pgl.itemtype;
  cfp->protFound = pgl.protFound;
  MemFree (pgl.protName);
  MemFree (pgl.protDesc);
}

static void SetProteinLengthDisplay (PrompT p, Int4 length)

{
  Char  len [32];

  if (p == NULL) return;
  if (length > 0) {
    sprintf (len, "%ld", (long) length);
    SetTitle (p, len);
  } else {
    SetTitle (p, "");
  }
}

static void CdRegionPtrToForm (ForM f, Pointer data)

{
  BioseqPtr       bsp;
  CdRgnFormPtr    cfp;
  Char            id [41];
  MolInfoPtr      mip;
  SeqEntryPtr     oldsep;
  ProtRefPtr      prp;
  ValNodePtr      sdp;
  SeqEntryPtr     sep;
  SeqFeatPtr      sfp;
  SeqIdPtr        sip;
  ValNodePtr      vnp;
  SeqFeatXrefPtr  xref;

  cfp = (CdRgnFormPtr) GetObjectExtra (f);
  if (cfp != NULL) {
    sep = GetTopSeqEntryForEntityID (cfp->input_entityID);
    oldsep = SeqEntrySetScope (sep);
    cfp->locvisited = TRUE;  /* even passing NULL will clear this protection */
    sfp = (SeqFeatPtr) data;
    if (sfp != NULL) {
      PointerToDialog (cfp->location, sfp->location);
      PointerToDialog (cfp->product, sfp->product);
      PointerToDialog (cfp->data, sfp->data.value.ptrvalue);
      SeqFeatPtrToCommon ((FeatureFormPtr) cfp, sfp);
      SetBestProteinFeature (cfp, sfp->product);
      if (! cfp->protFound) {
        xref = sfp->xref;
        while (xref != NULL && xref->data.choice != SEQFEAT_PROT) {
          xref = xref->next;
        }
        if (xref != NULL) {
          prp = (ProtRefPtr) xref->data.value.ptrvalue;
          if (prp != NULL) {
            vnp = prp->name;
            if (vnp != NULL) {
              SafeSetTitle (cfp->protNameText, (CharPtr) vnp->data.ptrvalue);
            }
            SafeSetTitle (cfp->protDescText, prp->desc);
          }
        }
      }
      if (sfp->product != NULL) {
        SafeHide (cfp->protSeqIdGrp);
        sip = SeqLocId (sfp->product);
        if (sip != NULL) {
          bsp = BioseqFind (sip);
          if (bsp != NULL) {
            PointerToDialog (cfp->protseq, bsp);
            SetProteinLengthDisplay (cfp->protlen, bsp->length);
            SafeEnable (cfp->launchBtn);
            SafeEnable (cfp->edProtBtn);
            SafeEnable (cfp->edProtBtn2);
            SafeEnable (cfp->edSeqBtn);
            SafeSetStatus (cfp->conceptTransA, FALSE);
            sep = SeqMgrGetSeqEntryForData (bsp);
            if (sep == NULL) return;
            sdp = SeqEntryGetSeqDescr (sep, Seq_descr_molinfo, NULL);
            if (sdp == NULL) return;
            mip = (MolInfoPtr) sdp->data.ptrvalue;
            if (mip == NULL) return;
            SafeSetStatus (cfp->conceptTransA, (Boolean) (mip->tech == 13));
            SeqEntrySetScope (oldsep);
            return;
          } else {
            SeqIdWrite (sip, id, PRINTID_FASTA_LONG, sizeof (id));
            SafeSetTitle (cfp->protSeqIdTxt, id);
            SafeShow (cfp->protSeqIdGrp);
            SafeDisable (cfp->launchBtn);
            SafeDisable (cfp->edProtBtn);
            SafeDisable (cfp->edProtBtn2);
            SafeDisable (cfp->edSeqBtn);
            SafeSetStatus (cfp->conceptTransA, FALSE);
            SeqEntrySetScope (oldsep);
            return;
          }
        }
      }
    }
    SafeSetTitle (cfp->protSeqIdTxt, "");
    SafeShow (cfp->protSeqIdGrp);
    SafeDisable (cfp->launchBtn);
    SafeDisable (cfp->edProtBtn);
    SafeDisable (cfp->edProtBtn2);
    SafeDisable (cfp->edSeqBtn);
    SafeSetStatus (cfp->conceptTransA, FALSE);
    SeqEntrySetScope (oldsep);
  }
}

typedef struct productpage {
  DIALOG_MESSAGE_BLOCK
  Int2          count;
  SeqEntryPtr   PNTR bsptr;
  Handle        product;
  Boolean       usePopupForProduct;
  Int2          oldval;
  Boolean       nucProducts;
  CdRgnFormPtr  cfp;
  RnaFormPtr    rfp;
  BaseFormPtr   bfp;
} ProductPage, PNTR ProductPagePtr;

static void FillInProducts (SeqEntryPtr sep, Pointer mydata,
                            Int4 index, Int2 indent)

{
  ProductPagePtr  ppp;

  if (sep != NULL && mydata != NULL) {
    ppp = (ProductPagePtr) mydata;
    ppp->bsptr [index] = sep;
  }
}

static void PopulateProductControl (Handle pr, ProductPagePtr ppp)

{
  BioseqPtr    bsp;
  Int2         i;
  size_t       len;
  Boolean      okay;
  SeqEntryPtr  sep;
  Char         str [128];
  /*
  Char         ch;
  CharPtr      ttl;
  */

  if (pr != NULL && ppp != NULL && ppp->bsptr != NULL) {
    if (ppp->usePopupForProduct) {
      PopupItem (pr, "     ");
    } else {
      ListItem (pr, "     ");
    }
    for (i = 0; i < ppp->count; i++) {
      sep = ppp->bsptr [i];
      if (sep != NULL && sep->choice == 1 && sep->data.ptrvalue != NULL) {
        bsp = (BioseqPtr) sep->data.ptrvalue;
        okay = FALSE;
        if (ppp->nucProducts) {
          if (ISA_na (bsp->mol)) {
            okay = TRUE;
          }
        } else {
          if (ISA_aa (bsp->mol)) {
            okay = TRUE;
          }
        }
        if (okay) {
          SeqIdWrite (bsp->id, str, PRINTID_REPORT, sizeof (str));
          /*
          StringCat (str, " ");
          len = StringLen (str);
          ttl = BioseqGetTitle (bsp);
          StringNCat (str, ttl, sizeof (str) - len - 2);
          ttl = str;
          ch = *ttl;
          while (ch != '\0') {
            if (ch == ';') {
              *ttl = ',';
            }
            ttl++;
            ch = *ttl;
          }
          */
          len = StringLen (str);
          if (len > 60) {
            StringCpy (str + 60, "...");
          }
          if (ppp->usePopupForProduct) {
            PopupItem (pr, str);
          } else {
            ListItem (pr, str);
          }
        }
      }
    }
    SetValue (pr, 1);
    ppp->oldval = 1;
  }
}

static void SeqLocPtrToProduct (DialoG d, Pointer data)

{
  BaseFormPtr     bfp;
  BioseqPtr       bsp;
  Int2            found;
  Int2            i;
  Boolean         okay;
  SeqEntryPtr     oldsep;
  ProductPagePtr  ppp;
  Int2            prots;
  SeqEntryPtr     sep;
  SeqIdPtr        sip;
  SeqLocPtr       slp;
  BioseqPtr       theBsp;

  ppp = (ProductPagePtr) GetObjectExtra (d);
  slp = (SeqLocPtr) data;
  if (ppp != NULL) {
    found = 0;
    if (slp != NULL) {
      sip = SeqLocId (slp);
      if (sip != NULL) {
        bfp = ppp->bfp;
        if (bfp != NULL) {
          sep = GetTopSeqEntryForEntityID (bfp->input_entityID);
          oldsep = SeqEntrySetScope (sep);
        }
        theBsp = BioseqFind (sip);
        if (bfp != NULL) {
          SeqEntrySetScope (oldsep);
        }
        if (theBsp != NULL) {
          prots = 0;
          for (i = 0; i < ppp->count; i++) {
            sep = ppp->bsptr [i];
            if (sep != NULL && sep->choice == 1 && sep->data.ptrvalue != NULL) {
              bsp = (BioseqPtr) sep->data.ptrvalue;
              okay = FALSE;
              if (ppp->nucProducts) {
                if (ISA_na (bsp->mol)) {
                  okay = TRUE;
                }
              } else {
                if (ISA_aa (bsp->mol)) {
                  okay = TRUE;
                }
              }
              if (okay) {
                prots++;
                if (bsp == theBsp) {
                  found = prots;
                }
              }
            }
          }
        }
      }
    }
    SetValue (ppp->product, found + 1);
    ppp->oldval = found + 1;
    if (! ppp->usePopupForProduct) {
      SetOffset (ppp->product, 0, found);
    }
  }
}

static Pointer ProductToSeqLocPtr (DialoG d)

{
  BioseqPtr       bsp;
  CdRgnFormPtr    cfp = NULL;
  Int2            i;
  Char            id [128];
  Boolean         okay;
  ProductPagePtr  ppp;
  Int2            prots;
  RnaFormPtr      rfp = NULL;
  SeqEntryPtr     sep;
  SeqIdPtr        sip;
  SeqLocPtr       slp;
  BioseqPtr       theBsp;
  Int2            val;

  slp = NULL;
  ppp = (ProductPagePtr) GetObjectExtra (d);
  if (ppp != NULL && ppp->cfp != NULL) {
    cfp = ppp->cfp;
    if (cfp->saveAsMiscFeat) {
      return NULL;
    }
  }

  if (ppp == NULL) return NULL;  

  if (ppp->bsptr != NULL) {
    if (ppp->nucProducts) {
      if (! Visible (d)) return NULL; /* not an mRNA, do not make product */
    }
    rfp = ppp->rfp;
    val = GetValue (ppp->product) - 1;
    prots = 0;
    theBsp = NULL;
    for (i = 0; i < ppp->count; i++) {
      sep = ppp->bsptr [i];
      if (sep != NULL && sep->choice == 1 && sep->data.ptrvalue != NULL) {
        bsp = (BioseqPtr) sep->data.ptrvalue;
        okay = FALSE;
        if (ppp->nucProducts) {
          if (ISA_na (bsp->mol)) {
            okay = TRUE;
          }
        } else {
          if (ISA_aa (bsp->mol)) {
            okay = TRUE;
          }
        }
        if (okay) {
          prots++;
          if (prots == val) {
            theBsp = bsp;
          }
        }
      }
    }
    if (ppp->cfp != NULL && ppp->cfp->usethisbioseq != NULL) {
      theBsp = ppp->cfp->usethisbioseq;
    }
    if (theBsp != NULL && theBsp->id != NULL) {
      slp = ValNodeNew (NULL);
      if (slp != NULL) {
        slp->choice = 3;
        slp->data.ptrvalue = SeqIdStripLocus (SeqIdDup (SeqIdFindBest (theBsp->id, 0)));
      }
    } else if (cfp != NULL) {
      if (Visible (cfp->protSeqIdGrp) && (! GetStatus (cfp->forceNear))) {
        GetTitle (cfp->protSeqIdTxt, id, sizeof (id) - 1);
        if ((! StringHasNoText (id)) && StringChr (id, '|') != NULL) {
          sip = MakeSeqID (id);
          if (sip != NULL) {
            slp = ValNodeNew (NULL);
            if (slp != NULL) {
              slp->choice = 3;
              slp->data.ptrvalue = SeqIdStripLocus (sip);
            }
          }
        }
      }
    }
  }
  return (Pointer) slp;
}

static void CleanupProductPage (GraphiC g, VoidPtr data)

{
  ProductPagePtr  ppp;

  ppp = (ProductPagePtr) data;
  if (ppp != NULL) {
    MemFree (ppp->bsptr);
  }
  MemFree (data);
}

static CharPtr protChangeMsg =
"This will change the protein product assigned to this CDS.\n\
It should only be done if the assignment was incorrect.\n\
Are you really sure you want to do this?";

static void ChangeProteinView (Handle obj)

{
  MsgAnswer       ans;
  BioseqPtr       bsp;
  CdRgnFormPtr    cfp;
  ProductPagePtr  ppp;
  SeqIdPtr        sip;
  SeqLocPtr       slp;

  ppp = GetObjectExtra (obj);
  if (ppp != NULL && ppp->cfp != NULL) {
    if (ppp->nucProducts) return; /* using for mRNA product */
    ans = Message (MSG_OKC, "%s", protChangeMsg);
    if (ans == ANS_CANCEL) {
      SetValue (obj, ppp->oldval);
      return;
    }
    cfp = ppp->cfp;
    if (GetValue (obj) == 1) {
      SafeSetTitle (cfp->protSeqIdTxt, "");
      SafeShow (cfp->protSeqIdGrp);
      SafeDisable (cfp->launchBtn);
      SafeDisable (cfp->edProtBtn);
      SafeDisable (cfp->edProtBtn2);
      SafeDisable (cfp->edSeqBtn);
      SetBestProteinFeature (cfp, NULL);
      PointerToDialog (cfp->protseq, NULL);
      SetProteinLengthDisplay (cfp->protlen, 0);
      Update ();
      return;
    }
    SafeHide (cfp->protSeqIdGrp);
    slp = (SeqLocPtr) ProductToSeqLocPtr (ppp->dialog);
    SetBestProteinFeature (cfp, slp);
    if (slp != NULL) {
      sip = SeqLocId (slp);
      if (sip != NULL) {
        bsp = BioseqFind (sip);
        if (bsp != NULL) {
          PointerToDialog (cfp->protseq, bsp);
          SetProteinLengthDisplay (cfp->protlen, bsp->length);
          SafeEnable (cfp->launchBtn);
          SafeEnable (cfp->edProtBtn);
          SafeEnable (cfp->edProtBtn2);
          SafeEnable (cfp->edSeqBtn);
        }
      }
      SeqLocFree (slp);
    }
    Update ();
  }
}


static DialoG CreateProteinOrMRNAProductDialog (GrouP h, CharPtr title,
                                                CharPtr label, Boolean nucProducts,
                                                SeqEntryPtr sep, CdRgnFormPtr cfp,
                                                RnaFormPtr rfp, BaseFormPtr bfp)

{
  GrouP           m;
  GrouP           p;
  ProductPagePtr  ppp;
  GrouP           s;

  p = HiddenGroup (h, -1, 0, NULL);
  SetGroupSpacing (p, 10, 10);

  ppp = (ProductPagePtr) MemNew (sizeof (ProductPage));
  if (ppp != NULL) {

    SetObjectExtra (p, ppp, CleanupProductPage);
    ppp->dialog = (DialoG) p;
    ppp->todialog = SeqLocPtrToProduct;
    ppp->fromdialog = ProductToSeqLocPtr;
    ppp->testdialog = NULL;
    ppp->nucProducts = nucProducts;

    if (title != NULL && title [0] != '\0') {
      s = NormalGroup (p, 0, -2, title, systemFont, NULL);
    } else {
      s = HiddenGroup (p, 0, -2, NULL);
    }
    m = HiddenGroup (s, -2, 0, NULL);
    /*
    SetGroupSpacing (m, 10, 10);
    */

    StaticPrompt (m, label, 0, popupMenuHeight, programFont, 'l');
    ppp->count = (Int2) BioseqCount (sep);
    ppp->bsptr = MemNew (sizeof (BioseqPtr) * ppp->count);
    if (ppp->bsptr != NULL) {
      BioseqExplore (sep, (Pointer) ppp, FillInProducts);
    }
    ppp->cfp = cfp;
    ppp->rfp = rfp;
    ppp->bfp = bfp;
    if (ppp->count < 32) {
      ppp->usePopupForProduct = TRUE;
      ppp->product = (Handle) PopupList (m, TRUE, (PupActnProc) ChangeProteinView);
    } else {
      ppp->usePopupForProduct = FALSE;
      ppp->product = (Handle) SingleList (m, 10, 3, (LstActnProc) ChangeProteinView);
    }
    SetObjectExtra (ppp->product, ppp, NULL);
    PopulateProductControl (ppp->product, ppp);
  }

  return (DialoG) p;
}


typedef struct rnatranscriptiddlg {
  DIALOG_MESSAGE_BLOCK
  DialoG               product;
  Boolean              usePopupForProduct;
  GrouP                far_id_grp;
  TexT                 far_id;
  Nlm_ChangeNotifyProc change_notify;
  Pointer              change_userdata;
} RnaTranscriptIDDlgData, PNTR RnaTranscriptIdDlgPtr;

static void ChangeTranscriptIDSequenceSelection (Pointer userdata)
{
  RnaTranscriptIdDlgPtr  dlg;
  ValNodePtr             vnp;

  dlg = (RnaTranscriptIdDlgPtr) userdata;
  if (dlg == NULL) return;

  if (dlg->far_id_grp != NULL)
  {
    vnp = DialogToPointer (dlg->product);
    if (vnp == NULL || vnp->data.ptrvalue == NULL) 
    {
      Enable (dlg->far_id_grp);
    }
    else 
    {
      Disable (dlg->far_id_grp);
    }
    vnp = ValNodeFree (vnp);
  }

  if (dlg->change_notify != NULL)
  {
    (dlg->change_notify) (dlg->change_userdata);
  }
}

static void ChangeTranscriptIDText (TexT t)
{
  RnaTranscriptIdDlgPtr  dlg;

  dlg = (RnaTranscriptIdDlgPtr) GetObjectExtra (t);
  if (dlg == NULL) return;

  if (dlg->change_notify != NULL)
  {
    (dlg->change_notify) (dlg->change_userdata);
  }  
}


static void LocToTranscriptIDDialog (DialoG d, Pointer data)
{
  RnaTranscriptIdDlgPtr  dlg;
  SeqLocPtr              slp;
  BioseqPtr              bsp;
  ValNode                vn;
  SeqIdPtr               sip;
  Char                   str [128];

  dlg = (RnaTranscriptIdDlgPtr) GetObjectExtra (d);
  if (dlg == NULL) return;
  slp = (SeqLocPtr) data;

  if (slp == NULL)
  {
    PointerToDialog (dlg->product, NULL);
    SafeSetTitle (dlg->far_id, "");
  }
  else
  {
    sip = SeqLocId (slp);
    bsp = BioseqFind (sip);
    if (bsp == NULL)
    {
      PointerToDialog (dlg->product, NULL);
      SeqIdWrite (sip, str, PRINTID_FASTA_LONG, sizeof (str));
      SafeSetTitle (dlg->far_id, str);
    }
    else
    {
      vn.choice = 0;
      vn.data.ptrvalue = sip;
      vn.next = NULL;
      PointerToDialog (dlg->product, &vn);
      SafeSetTitle (dlg->far_id, "");
    }    
  }
}


static Pointer TranscriptIDDialogToLoc (DialoG d)
{
  RnaTranscriptIdDlgPtr  dlg;
  SeqLocPtr              slp = NULL;
  ValNodePtr             vnp;
  SeqIdPtr               sip = NULL;
  CharPtr                str;

  dlg = (RnaTranscriptIdDlgPtr) GetObjectExtra (d);
  if (dlg == NULL) return NULL;

  vnp = DialogToPointer (dlg->product);
  if (vnp == NULL || vnp->data.ptrvalue == NULL)
  {
    if (!TextHasNoText (dlg->far_id))
    {
      str = SaveStringFromText (dlg->far_id);
      sip = MakeSeqID (str);
      str = MemFree (str);
    }
  }
  else
  {
    sip = SeqIdDup (vnp->data.ptrvalue);
  }
  vnp = ValNodeFree (vnp);
  
  if (sip != NULL)
  {
    slp = ValNodeNew (NULL);
    slp->choice = SEQLOC_WHOLE;
    slp->data.ptrvalue = SeqIdStripLocus (sip);
  }

  return slp;
}


static DialoG CreateRNATranscriptIDDialog (GrouP h, Uint2 entityID, Boolean allow_far_id, Nlm_ChangeNotifyProc change_notify, Pointer change_userdata)
{
  GrouP                  p;
  RnaTranscriptIdDlgPtr  dlg;

  p = HiddenGroup (h, 3, 0, NULL);
  SetGroupSpacing (p, 10, 10);

  dlg = (RnaTranscriptIdDlgPtr) MemNew (sizeof (RnaTranscriptIDDlgData));
  if (dlg == NULL) return NULL;

  SetObjectExtra (p, dlg, StdCleanupExtraProc);
  dlg->dialog = (DialoG) p;
  dlg->todialog = LocToTranscriptIDDialog;
  dlg->fromdialog = TranscriptIDDialogToLoc;

  dlg->change_notify = change_notify;
  dlg->change_userdata = change_userdata;

  StaticPrompt (p, "Transcript Sequence ID", 0, popupMenuHeight, programFont, 'l');
  dlg->product = SequenceSelectionDialogEx (p, ChangeTranscriptIDSequenceSelection, dlg, FALSE, TRUE, TRUE, FALSE, entityID, SHORT_SELECTION_LIST);
  if (allow_far_id) 
  {
    dlg->far_id_grp = HiddenGroup (p, 2, 0, NULL);
    StaticPrompt (dlg->far_id_grp, "Far ID", 0, dialogTextHeight, programFont, 'l');
    dlg->far_id = DialogText (dlg->far_id_grp, "", 5, ChangeTranscriptIDText);
    SetObjectExtra (dlg->far_id, dlg, NULL);
  }

  return (DialoG) p;
}


extern SeqLocPtr PredictCodingRegion (BioseqPtr nuc, BioseqPtr prot, Int2 genCode)

{
  BioseqPtr    bsp;
  SeqLocPtr    oldslp;
  SeqAnnotPtr  sap;
  SeqFeatPtr   sfp;
  SeqIdPtr     sip;
  SeqLocPtr    slp;

  if (nuc == NULL || prot == NULL) return NULL;
  if (nuc->length + 5 >= MAXALLOC) {
    Message (MSG_OK, "Nucleotide sequence is too large for coding region prediction.");
    return NULL;
  }
  slp = NULL;
  /*
  slp = FindSingleCodingInterval (nuc, prot, genCode);
  */
  if (slp == NULL) {
    sap = SuggestCodingRegion (nuc, prot, genCode);
    if (sap != NULL && sap->type == 1) {
      sfp = (SeqFeatPtr) sap->data;
      if (sfp != NULL && sfp->data.choice == SEQFEAT_CDREGION) {
        slp = sfp->location;
        sfp->location = NULL;
        sip = SeqLocId (slp);
        if (sip != NULL) {
          bsp = BioseqFind (sip);
          if (bsp != NULL) {
            if (bsp->repr == Seq_repr_seg) {
              oldslp = slp;
              slp = SegLocToParts (bsp, oldslp);
              FreeAllFuzz (slp);
              SeqLocFree (oldslp);
            }
          }
        }
      }
    }
    sap = SeqAnnotFree (sap);
  }
  StripLocusFromSeqLoc (slp);
  return slp;
}

static void DoPredictCdRegion (CdRgnFormPtr cfp)

{
  Int2          code;
  CdRgnPagePtr  cpp;
  BioseqPtr     nucbsp;
  BioseqPtr     protbsp;
  SeqEntryPtr   psep;
  SeqEntryPtr   sep;
  SeqLocPtr     slp;
  Int2          val;

  if (cfp != NULL) {
    WatchCursor ();
    Update ();
    code = 0;
    cpp = (CdRgnPagePtr) GetObjectExtra (cfp->data);
    if (cpp != NULL) {
      val = gcIndexToId [GetValue (cpp->geneticCode)];
      if (val > 0 && val < NUM_GENETIC_CODES) {
        code = val;
      }
    }
    psep = SeqEntryNew ();
    if (psep != NULL) {
      protbsp = (BioseqPtr) DialogToPointer (cfp->protseq);
      psep->choice = 1;
      psep->data.ptrvalue = protbsp;
      SeqMgrSeqEntry (SM_BIOSEQ, (Pointer) protbsp, psep);
      sep = GetBestTopParentForItemID (cfp->input_entityID,
                                       cfp->input_itemID,
                                       cfp->input_itemtype);
      /*
      sep = GetTopSeqEntryForEntityID (cfp->input_entityID);
      */
      if (sep != NULL) {
        sep = FindNucSeqEntry (sep);
      }
      if (sep != NULL && sep->data.ptrvalue != NULL &&
          protbsp != NULL && protbsp->length > 0) {
        nucbsp = (BioseqPtr) sep->data.ptrvalue;
        slp = PredictCodingRegion (nucbsp, protbsp, code);
        if (slp != NULL && SeqLocStop (slp) > nucbsp->length - 1) {
          Message (MSG_ERROR, "Warning!  Nucleotide is not long enough for protein sequence!");
        } else if (slp == NULL && nucbsp->length / 3 < protbsp->length) {
          Message (MSG_ERROR, "Warning!  Nucleotide is not long enough for protein sequence!");
        }
        PointerToDialog (cfp->location, slp);
        if (slp != NULL) {
          cfp->locvisited = TRUE;
        }
        SeqLocFree (slp);
      }
      SeqEntryFree (psep);
    }
    ArrowCursor ();
    Update ();
  }
}

static void PredictCdRegion (ButtoN b)

{
  CdRgnFormPtr  cfp;

  cfp = (CdRgnFormPtr) GetObjectExtra (b);
  DoPredictCdRegion (cfp);
}

static void DoTranslateProtein (CdRgnFormPtr cfp)

{
  ByteStorePtr  bs;
  BioseqPtr     bsp;
  Char          ch;
  CharPtr       prot;
  CharPtr       ptr;
  SeqFeatPtr    sfp;
  Int4          star_at_end = 0;

  if (cfp != NULL) {
    sfp = SeqFeatNew ();
    if (sfp != NULL) {
      sfp->data.choice = SEQFEAT_CDREGION;
      sfp->data.value.ptrvalue = DialogToPointer (cfp->data);
      sfp->location = DialogToPointer (cfp->location);
      if (sfp->location != NULL) {
        bs = ProteinFromCdRegionEx (sfp, TRUE, FALSE);
        if (bs != NULL) {
          prot = BSMerge (bs, NULL);
          bs = BSFree (bs);
          if (prot != NULL) {
            ptr = prot;
            ch = *ptr;
            while (ch != '\0') {
              *ptr = TO_UPPER (ch);
              if (ch == '*') {
                star_at_end = 1;
              } else {
                star_at_end = 0;
              }
              ptr++;
              ch = *ptr;
            }
            bs = BSNew (1000);
            if (bs != NULL) {
              ptr = prot;
              /*
              if (prot [0] == '-') {
                ptr++;
              }
              */
              BSWrite (bs, (VoidPtr) ptr, (Int4) StringLen (ptr));
            }
          }
          bsp = BioseqNew ();
          if (bsp != NULL) {
            bsp->id = SeqIdParse ("lcl|CdRgnTransl");
            bsp->repr = Seq_repr_raw;
            bsp->mol = Seq_mol_aa;
            bsp->seq_data_type = Seq_code_ncbieaa;
            bsp->seq_data = (SeqDataPtr) bs;
            bsp->length = BSLen (bs);
            PointerToDialog (cfp->protseq, bsp);
            SetProteinLengthDisplay (cfp->protlen, bsp->length - star_at_end);
            bsp->seq_data = NULL;
            BioseqFree (bsp);
          }
          MemFree (prot);
        }
        BSFree (bs);
      }
    }
    SeqFeatFree (sfp);
  }
}

extern void CdRgnTranslateWithFrame (ForM f, Uint1 frame)

{
  CdRgnFormPtr  cfp;
  CdRgnPagePtr  cpp;

  cfp = (CdRgnFormPtr) GetObjectExtra (f);
  if (cfp != NULL) {
    cpp = (CdRgnPagePtr) GetObjectExtra (cfp->data);
    if (cpp != NULL) {
      if (frame < 2) {
        SafeSetValue (cpp->frame, frame + 1);
      } else {
        SafeSetValue (cpp->frame, 0);
      }
    }
    DoTranslateProtein (cfp);
  }
}

static void TranslateProtein (ButtoN b)

{
  CdRgnFormPtr  cfp;

  cfp = (CdRgnFormPtr) GetObjectExtra (b);
  if (cfp != NULL) {
    DoTranslateProtein (cfp);
  }
}

static void SetSynchProc (ButtoN b)

{
  CdRgnFormPtr  cfp;

  cfp = (CdRgnFormPtr) GetObjectExtra (b);
  if (cfp != NULL) {
    cfp->autoUpdatePartials = GetStatus (b);
  }
}

static void SetUpdatemRNAProc (ButtoN b)

{
  CdRgnFormPtr  cfp;

  cfp = (CdRgnFormPtr) GetObjectExtra (b);
  if (cfp != NULL) {
    cfp->autoUpdatemRNASpans = GetStatus (b);
  }
}

static void SetRetransProc (ButtoN b)

{
  CdRgnFormPtr  cfp;

  cfp = (CdRgnFormPtr) GetObjectExtra (b);
  if (cfp != NULL) {
    cfp->autoRetranslate = GetStatus (b);
  }
}

EnumFieldAssocPtr codebreak_alists [] = {
  NULL, NULL
};

Uint2 codebreak_types [] = {
  TAGLIST_TEXT, TAGLIST_POPUP
};

Uint2 codebreak_widths [] = {
  5, 0
};

static void CreateCodeBreakAlist (CdRgnPagePtr cpp)

{
  Char             ch;
  Int2             count;
  Uint1            first;
  Uint1            i;
  Int2             j;
  Char             item [77];
  Uint1            last;
  SeqCodeTablePtr  sctp;
  CharPtr          str;

  if (cpp != NULL) {
    sctp = SeqCodeTableFind (Seq_code_ncbieaa);
    first = FirstResidueInCode (sctp);
    last = LastResidueInCode (sctp);
    count = last - 65 + 4;
    cpp->alist = MemNew (sizeof (EnumFieldAssoc) * (size_t) count);
    cpp->count = count;
    if (cpp->alist != NULL) {
      j = 0;
      cpp->alist [j].name = StringSave ("     ");
      cpp->alist [j].value = (UIEnum) 0;
      j++;
      for (i = 65; i <= last; i++) {
        /*
        if (i == 74 || i == 79) {
          continue;
        }
        */
        ch = GetSymbolForResidue (sctp, i);
        str = (CharPtr) GetNameForResidue (sctp, i);
        cpp->alist [j].name = StringSave (str);
        cpp->alist [j].value = (UIEnum) ch;
        sprintf (item, "%c    %s", ch, str);
        j++;
      }
      i = 42;
      ch = GetSymbolForResidue (sctp, i);
      str = (CharPtr) GetNameForResidue (sctp, i);
      cpp->alist [j].name = StringSave (str);
      cpp->alist [j].value = (UIEnum) ch;
      sprintf (item, "%c    %s", ch, str);
      j++;
      cpp->alist [j].name = NULL;
      cpp->alist [j].value = (UIEnum) 0;
    }
    cpp->alists [0] = NULL;
    cpp->alists [1] = cpp->alist;
  }
}

static void CodeBreakPtrToCodeBreakDialog (DialoG d, Pointer data)

{
  Uint1           aa;
  BioseqPtr       bsp;
  ValNodePtr      head;
  Int2            j;
  CodeBreakPtr    list;
  Int4            pos;
  SeqIdPtr        sip;
  SeqMapTablePtr  smtp;
  Char            str [64];
  TagListPtr      tlp;
  Char            tmp [32];
  ValNodePtr      vnp;

  tlp = (TagListPtr) GetObjectExtra (d);
  list = (CodeBreakPtr) data;
  if (tlp != NULL) {
    head = NULL;
    while (list != NULL) {
      if (list->loc != NULL) {
        vnp = ValNodeNew (head);
        if (head == NULL) {
          head = vnp;
        }
        if (vnp != NULL) {
          pos = 0;
          sip = SeqLocId (list->loc);
          if (sip != NULL) {
            bsp = BioseqFind (sip);
            if (bsp != NULL) {
              pos = GetOffsetInBioseq (list->loc, bsp, SEQLOC_START);
              aa = 0;
              switch (list->aa.choice) {
                case 1 :
                  aa = (Uint1) list->aa.value.intvalue;
                  break;
                case 2 :
                  smtp = SeqMapTableFind (Seq_code_ncbieaa, Seq_code_ncbi8aa);
                  if (smtp != NULL) {
                    aa = SeqMapTableConvert (smtp, (Uint1) list->aa.value.intvalue);
                  }
                  break;
                case 3 :
                  smtp = SeqMapTableFind (Seq_code_ncbieaa, Seq_code_ncbistdaa);
                  if (smtp != NULL) {
                    aa = SeqMapTableConvert (smtp, (Uint1) list->aa.value.intvalue);
                  }
                  break;
                default :
                  aa = (Uint1) list->aa.value.intvalue;
                  break;
              }
              sprintf (tmp, "%ld", (long) (pos + 1));
              StringCpy (str, tmp);
              StringCat (str, "\t");
              sprintf (tmp, "%d", (int) list->aa.value.intvalue);
              StringCat (str, tmp);
              StringCat (str, "\n");
              vnp->data.ptrvalue = StringSave (str);
            }
          }
        }
      }
      list = list->next;
    }
    SendMessageToDialog (tlp->dialog, VIB_MSG_RESET);
    tlp->vnp = head;
    SendMessageToDialog (tlp->dialog, VIB_MSG_REDRAW);
    for (j = 0, vnp = tlp->vnp; vnp != NULL; j++, vnp = vnp->next) {
    }
    tlp->max = MAX ((Int2) 0, (Int2) (j - tlp->rows + 1));
    CorrectBarMax (tlp->bar, tlp->max);
    CorrectBarPage (tlp->bar, tlp->rows - 1, tlp->rows - 1);
  }
}

static Pointer CodeBreakDialogToCodeBreakPtr (DialoG d)

{
  Uint1         aa;
  CodeBreakPtr  cbp;
  CodeBreakPtr  cbplast;
  Char          ch;
  CodeBreakPtr  head;
  Int2          j;
  Int2          len;
  Boolean       okay;
  SeqLocPtr     slp;
  SeqPntPtr     spntp;
  CharPtr       str;
  TagListPtr    tlp;
  CharPtr       tmp;
  int           val;
  ValNodePtr    vnp;

  head = NULL;
  tlp = (TagListPtr) GetObjectExtra (d);
  if (tlp != NULL && tlp->vnp != NULL) {
    cbp = NULL;
    cbplast = NULL;
    for (vnp = tlp->vnp; vnp != NULL; vnp = vnp->next) {
      str = (CharPtr) vnp->data.ptrvalue;
      okay = FALSE;
      len = StringLen (str);
      for (j = 0; j < len; j++) {
        ch = str [j];
        if (ch != ' ' && ch != '\t' && ch != '\n') {
          okay = TRUE;
        }
      }
      if (okay) {
        tmp = ExtractTagListColumn ((CharPtr) vnp->data.ptrvalue, 1);
        if (tmp != NULL && sscanf (tmp, "%d", &val) == 1 && val != 0) {
          aa = (Uint1) val;
          MemFree (tmp);
          tmp = ExtractTagListColumn ((CharPtr) vnp->data.ptrvalue, 0);
          if (tmp != NULL && sscanf (tmp, "%d", &val) == 1 && val != 0) {
            cbp = CodeBreakNew ();
            if (cbplast == NULL) {
              head = cbp;
            } else {
              cbplast->next = cbp;
            }
            cbplast = cbp;
            if (cbp != NULL) {
              if (globalid != NULL) {
                spntp = SeqPntNew ();
                slp = ValNodeNew (NULL);
                slp->choice = SEQLOC_PNT;
                slp->data.ptrvalue = (Pointer) spntp;
                spntp->point = (Int4) (val - 1);
                spntp->id = SeqIdStripLocus (SeqIdDup (globalid));
                cbp->loc = slp;
              }
              cbp->aa.value.intvalue = aa;
              cbp->aa.choice = 1;
            }
          } else {
            MemFree (tmp);
          }
        } else {
          MemFree (tmp);
        }
      }
    }
  }
  return (Pointer) head;
}

static CharPtr cdsToMiscFeatWarning =
"You are indicating that this CDS is non-functional.\n\
Database policy is to convert this into a misc_feature,\n\
and to remove any protein product attached to the CDS.\n\
If this is the case, you should add some explanatory\n\
text as a comment before pressing the Accept button.\n\
Are you sure you want to check this box?";

static void ConvMiscFeatWarn (ButtoN b)

{
  MsgAnswer     ans;
  CdRgnFormPtr  cfp;

  cfp = (CdRgnFormPtr) GetObjectExtra (b);
  if (cfp == NULL) return;
  if (GetStatus (b)) {
    ans = Message (MSG_YN, "%s", cdsToMiscFeatWarning);
    if (ans == ANS_NO) {
      SetStatus (b, FALSE);
    }
  }
}

static Int4 AccessionToGi (CharPtr string)
{
   /*
   CharPtr str;
   LinkSetPtr lsp;
   Int4 gi;

   str = MemNew (StringLen (string) + 10);
   sprintf (str, "\"%s\" [ACCN]", string);
   lsp = EntrezTLEvalString (str, TYP_NT, -1, NULL, NULL);
   MemFree (str);
   if (lsp == NULL) return 0;
   if (lsp->num <= 0) {
       LinkSetFree (lsp);
       return 0;
   }
   gi = lsp->uids [0];
   LinkSetFree (lsp);
   return gi;
   */
   Int4      gi;
   SeqIdPtr  sip;

   sip = SeqIdFromAccessionDotVersion (string);
   if (sip == NULL) return 0;
   gi = GetGIForSeqId (sip);
   SeqIdFree (sip);
   return gi;
}

static void SetGeneAndProtFields (CdRgnFormPtr cfp, CharPtr title);

static BioseqPtr GetProteinFromSep (SeqEntryPtr sep)
{
  BioseqPtr query_bsp;

  query_bsp = NULL;
  SeqEntryExplore(sep, &query_bsp, FindProt);
  return query_bsp;
}

typedef struct proteinimportformdata {
  CdRgnFormPtr cfp;
  WindoW       w;
  TexT         accession;
  GrouP        accntype;
  ButtoN       DownloadProtein;
} ProteinImportFormData, PNTR ProteinImportFormPtr;

static void AddProteinToCDSDialog (
  BioseqPtr bsp,
  CdRgnFormPtr  cfp
)
{
  ValNodePtr vnp;
  CharPtr    vnpstr;
  SeqLocPtr  slp;
  Char          str [128];

  PointerToDialog (cfp->protseq, bsp);
  SetProteinLengthDisplay (cfp->protlen, bsp->length);
  SeqIdWrite (bsp->id, str, PRINTID_REPORT, sizeof (str));
  SafeSetTitle (cfp->protSeqIdTxt, str);
  if (bsp->descr != NULL) {
    vnp = ValNodeFindNext (bsp->descr, NULL, Seq_descr_title);
    if (vnp != NULL && vnp->data.ptrvalue != NULL) {
      vnpstr = (CharPtr) vnp->data.ptrvalue;
      SetGeneAndProtFields (cfp, vnpstr);
    }
  }
  slp = DialogToPointer (cfp->location);
  if (slp == NULL || (! cfp->locvisited)) {
    DoPredictCdRegion (cfp);
  }
  SeqLocFree (slp);
}

static void DownloadProteinFromEntrez (ButtoN b)
{
  SeqEntryPtr   sep = NULL;
  Int4          uid;
  BioseqPtr     bsp;
  CdRgnFormPtr  cfp;
  Char          str [128];
  ProteinImportFormPtr pif;
 
  pif = (ProteinImportFormPtr) GetObjectExtra (b);
  if (pif == NULL) return; 
  cfp = pif->cfp;
  if (cfp == NULL) return;
  GetTitle (pif->accession, str, sizeof (str));
  if (StringHasNoText (str))
  {
    return;
  }
  WatchCursor ();
  if (GetValue (pif->accntype) == 1) {
    uid = AccessionToGi (str);
  } else {
    if (! StrToLong (str, &uid)) {
     uid = 0;
    }
  }

  if (uid > 0) {
    sep = PubSeqSynchronousQuery (uid, 0, -1);
    if (sep == NULL) {
      Message (MSG_OK, "Unable to find this record in the database.");
      return;
    }
    if (IS_Bioseq (sep)) {
    } else if (IS_Bioseq_set (sep)) {
    } else {
      Message (MSG_OK, "Unable to find this record in the database.");
      return;
    }
  }
  bsp = GetProteinFromSep (sep);
  if (bsp == NULL)
  {
    Message (MSG_ERROR, "No protein in record!");
  }
  else
  {
    AddProteinToCDSDialog (bsp, cfp);
  }
  SeqEntryFree (sep);
  Remove (pif->w);
  ArrowCursor ();
  Update ();
}

static void FetchTextProc (TexT t)

{
  Boolean       alldigits;
  Char          ch;
  ProteinImportFormPtr  pif;
  CharPtr       ptr;
  Char          str [32];

  pif = (ProteinImportFormPtr) GetObjectExtra (t);
  if (pif == NULL) return;
  GetTitle (t, str, sizeof (str));
  if (StringHasNoText (str)) {
    SafeDisable (pif->DownloadProtein);
  } else {
    SafeEnable (pif->DownloadProtein);
    TrimSpacesAroundString (str);
    alldigits = TRUE;
    ptr = str;
    ch = *ptr;
    while (ch != '\0') {
      if (! IS_DIGIT (ch)) {
        alldigits = FALSE;
      }
      ptr++;
      ch = *ptr;
    }
    if (alldigits) {
      SafeSetValue (pif->accntype, 2);
    } else {
      SafeSetValue (pif->accntype, 1);
    }
  }
}

static void LoadFASTAProteinFromFile (ButtoN b)
{
  CdRgnFormPtr  cfp;
  CharPtr       extension;
  FILE *        fp;
  SeqEntryPtr   sep;
  BioseqPtr     bsp;
  Char          path [PATH_MAX];
  ProteinImportFormPtr pif;
  Char          tmp [128];
 
  pif = (ProteinImportFormPtr) GetObjectExtra (b);
  if (pif == NULL) return; 
  cfp = pif->cfp;
  if (cfp == NULL) return;

  path [0] = 0;
  extension = GetAppProperty ("FastaProtExtension");
  if (GetInputFileName (path, sizeof (path), extension, "TEXT"))
  {
    fp = FileOpen (path, "r");
    if (fp != NULL) {
      sep = FastaToSeqEntry (fp, FALSE);
      if (sep != NULL && sep->choice == 1 && sep->data.ptrvalue != NULL) {
        bsp = (BioseqPtr) sep->data.ptrvalue;
        if (BioseqFind (bsp->id) != bsp)
        {
          SeqIdWrite (bsp->id, tmp, PRINTID_REPORT, sizeof (tmp));
          bsp->id = SeqIdFree (bsp->id);
          bsp->id = MakeUniqueSeqID (tmp);
        }
        AddProteinToCDSDialog (bsp, cfp);
      }
      SeqEntryFree (sep);
    }
    FileClose (fp);
  }
  Remove (pif->w);
}

static void ShowImportProteinDialog ( CdRgnFormPtr  cfp)
{
  WindoW w;
  GrouP  c, f, g, h;
  ButtoN b;
  ProteinImportFormPtr pif;

  pif = (ProteinImportFormPtr) MemNew ( sizeof (ProteinImportFormData));
  if (pif == NULL) return;
  pif->cfp = cfp;
  w = FixedWindow (-50, -33, -10, -10, "Import Protein", StdCloseWindowProc);
  pif->w = w;
  SetObjectExtra (w, pif, NULL);

  h = HiddenGroup (w, -1, 0, NULL);
  SetGroupSpacing (h, 10, 10);

  g = HiddenGroup (h, 4, 0, NULL);

  pif->accntype = HiddenGroup (g, 4, 0, NULL);
  RadioButton (pif->accntype, "Accession");
  RadioButton (pif->accntype, "GI");
  SetValue (pif->accntype, 1);
  pif->accession = DialogText (g, "", 6, FetchTextProc);
  SetObjectExtra (pif->accession, pif, NULL);
  pif->DownloadProtein = PushButton (g, "Download Protein From Entrez",
                                     DownloadProteinFromEntrez);
  SetObjectExtra (pif->DownloadProtein, pif, NULL);
  Disable (pif->DownloadProtein);

  f = HiddenGroup (h, 4, 0, NULL);
  b = PushButton (f, "Load FASTA Protein From File", LoadFASTAProteinFromFile);
  SetObjectExtra (b, pif, NULL);

  c = HiddenGroup (w, 2, 0, NULL);
  PushButton (c, "Cancel", StdCancelButtonProc);
  AlignObjects (ALIGN_CENTER, (HANDLE) g, (HANDLE) f, (HANDLE) c, NULL);
  RealizeWindow (w);
  Show (w);
  Update ();

}

static void SwitchToProtFeatEd (ButtoN b)

{
  CdRgnFormPtr  cfp;
  Int2          handled;

  cfp = (CdRgnFormPtr) GetObjectExtra (b);
  if (cfp != NULL && cfp->protFound) {
    WatchCursor ();
    Update ();
    handled = GatherProcLaunch (OMPROC_EDIT, FALSE, cfp->protEntityID, cfp->protItemID,
                                cfp->protItemtype, 0, 0, cfp->protItemtype, 0);
    ArrowCursor ();
    Update ();
    if (handled != OM_MSG_RET_DONE || handled == OM_MSG_RET_NOPROC) {
      Message (MSG_ERROR, "Unable to launch editor on protein feature.");
    }
    else
    {
      SendMessageToForm (cfp->form, VIB_MSG_CLOSE);
      Update ();
    }
  }
}


static CharPtr GetProteinString (CdRgnFormPtr cfp, SeqLocPtr location)
{
  SeqFeatPtr   sfp;
  ByteStorePtr bs;
  CharPtr      prot = NULL;

  if (cfp == NULL || location == NULL) return NULL;
  sfp = SeqFeatNew ();
  if (sfp == NULL) return NULL;

  sfp->data.choice = SEQFEAT_CDREGION;
  sfp->data.value.ptrvalue = DialogToPointer (cfp->data);
  sfp->location = location;
  bs = ProteinFromCdRegionEx (sfp, TRUE, FALSE);
  if (bs != NULL) {
    prot = BSMerge (bs, NULL);
    bs = BSFree (bs);
  }
  /* IMPORTANT - do not free the location that was passed in */
  sfp->location = NULL;
  SeqFeatFree (sfp);
  return prot;
}


extern SeqLocPtr TruncateLocation (SeqLocPtr head, Int4 len)
{
    SeqLocPtr slp = NULL;
    SeqIdPtr sip;
    Int4 from = 0, to = 0;
    Boolean changed = FALSE;
    Int4       loc_len = 0;
    Int4       cum_len = 0;
    SeqLocPtr  del_slp;
    ValNodePtr del_slp_list = NULL, vnp;
    Uint1      strand;

    if (head == NULL || len < 1)
        return head;

  slp = SeqLocFindNext (head, slp);
  while (slp != NULL)
  {
    if (cum_len >= len)
    {
      del_slp = AsnIoMemCopy(slp,
                    (AsnReadFunc) SeqLocAsnRead, (AsnWriteFunc) SeqLocAsnWrite);
      ValNodeAddPointer (&del_slp_list, 0, del_slp);
    }
    else
    {
          sip = SeqLocId(slp);
          from = SeqLocStart(slp);
          to = SeqLocStop(slp);
          loc_len = (to - from + 1);
      cum_len += loc_len;
      if (cum_len > len)
      {
        strand = SeqLocStrand (slp);
        /* remove part of this location */
        if (strand == Seq_strand_minus)
        {
          to = cum_len - len + from - 1;
        }
        else
        {
          from = to - (cum_len - len) + 1;
        }
        del_slp = SeqLocIntNew (from, to, strand, sip);
        ValNodeAddPointer (&del_slp_list, 0, del_slp);
      }
    }
        slp = SeqLocFindNext (head, slp);
  }
  
  for (vnp = del_slp_list; vnp != NULL; vnp = vnp->next)
  {
    slp = (SeqLocPtr) vnp->data.ptrvalue;
    if (slp == NULL) continue;
        sip = SeqLocId(slp);
        from = SeqLocStart(slp);
        to = SeqLocStop(slp);
        head = SeqLocDelete(head, sip, from, to, FALSE, &changed);    
  }
  
    return head;
}


static Boolean LengthenLocation (SeqLocPtr head, Int4 len)
{
    SeqLocPtr slp = NULL, last_slp = NULL;
  SeqIntPtr sintp;
  SeqPntPtr spp;
  Int4        orig_len, len_diff;
  Boolean     rval = FALSE;
  BioseqPtr   bsp;

    if (head == NULL || len < 1)
        return FALSE;

  orig_len = SeqLocLen (head);
  len_diff = len - orig_len;
  if (len_diff <= 0) return FALSE;

  slp = SeqLocFindNext (head, slp);
  while (slp != NULL)
  {
    last_slp = slp;
    slp = SeqLocFindNext (head, slp);
  }

  if (last_slp != NULL && last_slp->choice == SEQLOC_INT) {
    sintp = (SeqIntPtr) last_slp->data.ptrvalue;
    if (sintp->strand == Seq_strand_minus) {
      if (sintp->from - len_diff >= 0) {
        sintp->from -= len_diff;
        rval = TRUE;
      }
    } else {
      bsp = BioseqFind (sintp->id);
      if (bsp != NULL && sintp->to + len_diff < bsp->length) {
        sintp->to += len_diff;
        rval = TRUE;
      }
    }      
  } else if (last_slp != NULL && last_slp->choice == SEQLOC_PNT) {
    spp = (SeqPntPtr) last_slp->data.ptrvalue;
    rval = TRUE;
    if (spp->strand == Seq_strand_minus) {
      if (spp->point - len_diff < 0) {
        rval = FALSE;
      }
    } else {
      bsp = BioseqFind (spp->id);
      if (bsp == NULL || spp->point + len_diff >= bsp->length) {
        rval = FALSE;
      }
    }
    if (rval) {
      sintp = SeqIntNew ();
      sintp->id = SeqIdDup (spp->id);
      sintp->strand = spp->strand;
      sintp->if_from = AsnIoMemCopy (spp->fuzz, (AsnReadFunc) IntFuzzAsnRead, (AsnWriteFunc) IntFuzzAsnWrite);
      sintp->if_from = AsnIoMemCopy (spp->fuzz, (AsnReadFunc) IntFuzzAsnRead, (AsnWriteFunc) IntFuzzAsnWrite);
      if (sintp->strand == Seq_strand_minus) {
        sintp->from = spp->point - len_diff;
        sintp->to = spp->point;
      } else {
        sintp->from = spp->point;
        sintp->to = spp->point + len_diff;
      }
      spp = SeqPntFree (spp);
      last_slp->data.ptrvalue = sintp;
      last_slp->choice = SEQLOC_INT;
    }
  }
  
  return rval;
}


static void AdjustForStopCodon (ButtoN b)

{
  CdRgnFormPtr  cfp;
  SeqLocPtr     orig_slp;
  BioseqPtr     nucBsp;
  Boolean       partial5, partial3;
  Uint1         strand;
  CharPtr       prot, cp;
  Int4          desired_cds_len, loc_len, max_len;
  CdRegionPtr   crp;
  Uint1         frame = 0;

  cfp = (CdRgnFormPtr) GetObjectExtra (b);
  if (cfp == NULL) {
    return;
  }

  orig_slp = DialogToPointer (cfp->location);
  if (orig_slp == NULL) {
    Message (MSG_ERROR, "No location to adjust!");
    return;
  }

  CheckSeqLocForPartial (orig_slp, &partial5, &partial3);

  nucBsp = GetBioseqGivenSeqLoc (orig_slp, cfp->input_entityID);
  if (nucBsp == NULL) {
    Message (MSG_ERROR, "Unable to find Bioseq for coding region location");
    orig_slp = SeqLocFree (orig_slp);
    return;
  }

  crp = DialogToPointer (cfp->data);
  if (crp != NULL) {
    frame = crp->frame;
    if (frame > 0) {
      frame--;
    }
    crp = CdRegionFree (crp);
  }

  prot = GetProteinString (cfp, orig_slp);
  if (StringHasNoText (prot)) {
    Message (MSG_ERROR, "Translation failed");
    prot = MemFree (prot);
    orig_slp = SeqLocFree (orig_slp);
    return;
  } 

  cp = StringChr (prot, '*');

  if (cp == NULL) {
    /* lengthen coding region, look for stop codon */
    strand = SeqLocStrand (orig_slp);
    loc_len = SeqLocLen (orig_slp);
    if (strand == Seq_strand_minus) {
      max_len = loc_len + SeqLocStart (orig_slp);
    } else {
      max_len = loc_len + nucBsp->length - SeqLocStop (orig_slp) - 1;
    }
    
    if (LengthenLocation (orig_slp, max_len)) {
      prot = MemFree (prot);
      prot = GetProteinString (cfp, orig_slp);
      cp = StringChr (prot, '*');
      if (cp == NULL) {
        if (ANS_YES == Message (MSG_YN, "No stop codon found - extend to full length of sequence (and make partial)?")) {
          SetSeqLocPartial (orig_slp, partial5, TRUE);
          PointerToDialog (cfp->location, orig_slp);
          DoTranslateProtein (cfp);
        }
        prot = MemFree (prot);
        orig_slp = SeqLocFree (orig_slp);
        return;
      }
    } else {
      Message (MSG_ERROR, "Unable to lengthen location!");
      prot = MemFree (prot);
      orig_slp = SeqLocFree (orig_slp);
      return;
    }
  }

  desired_cds_len = ((cp - prot) + 1) * 3;
  if (frame > 0) {
    desired_cds_len += frame;
  }
    
  loc_len = SeqLocLen (orig_slp);
  if (desired_cds_len < loc_len) {
    /* truncate to correct length */
    orig_slp = TruncateLocation (orig_slp, desired_cds_len);
  }
  SetSeqLocPartial (orig_slp, partial5, FALSE);

  PointerToDialog (cfp->location, orig_slp);
  DoTranslateProtein (cfp);
  prot = MemFree (prot);
  orig_slp = SeqLocFree (orig_slp);
}


static SeqEntryPtr GetSeqEntryForLocation (SeqLocPtr slp, Uint2 entityID)
{
  BioseqPtr bsp;

  if (slp == NULL) 
  {
    return GetTopSeqEntryForEntityID (entityID);
  }

  bsp = BioseqFindFromSeqLoc (slp);
  if (bsp == NULL)
  {
    return GetTopSeqEntryForEntityID (entityID);
  }

  return GetBestTopParentForData (entityID, bsp);   
}


static void SetCdRegionPageGeneticCode (DialoG d)
{
  CdRgnPagePtr  cpp;
  SeqLocPtr     slp;
  Int2          genCode;

  cpp = (CdRgnPagePtr) GetObjectExtra (d);
  if (cpp != NULL) {
    slp = DialogToPointer (cpp->cfp->location);
    genCode = SeqEntryToGeneticCode (GetSeqEntryForLocation (slp, cpp->cfp->input_entityID), NULL, NULL, 0);
    slp = SeqLocFree (slp);
    SetValue (cpp->geneticCode, gcIdToIndex [genCode]);
  }
}


static DialoG CreateCdRgnDialog (GrouP h, CharPtr title, Int2 genCode,
                                 SeqFeatPtr sfp, CdRgnFormPtr cfp)

{
  ButtoN        b;
  GrouP         c;
  CdRgnPagePtr  cpp;
  GrouP         f;
  GrouP         g;
  Boolean       internalVersion;
  GrouP         j;
  GrouP         k;
  GrouP         m;
  GrouP         n;
  GrouP         p;
  PrompT        p2;
  GrouP         q;
  GrouP         s;
  DialoG        tbs;
  TagListPtr    tlp;
  GrouP         v;
  GrouP         x;
  GrouP         y;
  GrouP         z;

  p = HiddenGroup (h, 1, 0, NULL);
  SetGroupSpacing (p, 10, 10);

  cpp = (CdRgnPagePtr) MemNew (sizeof (CdRgnPage));
  if (cpp != NULL) {

    SetObjectExtra (p, cpp, CleanupCdRgnPage);
    cpp->dialog = (DialoG) p;
    cpp->todialog = CdRegionPtrToCdRgnPage;
    cpp->fromdialog = CdRgnPageToCdRegionPtr;
    cpp->testdialog = NULL;

    if (title != NULL && title [0] != '\0') {
      s = NormalGroup (p, 0, -2, title, systemFont, NULL);
    } else {
      s = HiddenGroup (p, 0, -2, NULL);
    }
    m = HiddenGroup (s, -1, 0, NULL);
    SetGroupSpacing (m, 10, 10);

    cpp->cfp = cfp;
    tbs = CreateFolderTabs (m, cdRgnTabs, 0, 0, 0,
                            PROGRAM_FOLDER_TAB,
                            ChangeCdRgnSubPage, (Pointer) cpp);
    cpp->currentPage = 0;
    k = HiddenGroup (m, 0, 0, NULL);

    j = HiddenGroup (k, -1, 0, NULL);
    SetGroupSpacing (j, 10, 10);
    n = HiddenGroup (j, 0, 0, NULL);

    cpp->cdRgnGrp [0] = HiddenGroup (k, -1, 0, NULL);
    g = HiddenGroup (cpp->cdRgnGrp [0], -1, 0, NULL);

    y = HiddenGroup (g, 0, -2, NULL);
    StaticPrompt (y, "Genetic Code", 0, popupMenuHeight, programFont, 'l');
    StaticPrompt (y, "Reading Frame", 0, popupMenuHeight, programFont, 'l');
    cpp->geneticCode = PopupList (y, TRUE, NULL);
    PopulateGeneticCodePopup (cpp->geneticCode);
    SetValue (cpp->geneticCode, gcIdToIndex [genCode]);
    v = HiddenGroup (y, 3, 0, NULL);
    cpp->frame = PopupList (v, TRUE, NULL);
    PopupItem (cpp->frame, " ");
    PopupItem (cpp->frame, "One");
    PopupItem (cpp->frame, "Two");
    PopupItem (cpp->frame, "Three");
    SetValue (cpp->frame, 1);
    StaticPrompt (v, " Protein Length ", 0, stdLineHeight, programFont, 'l');
    cfp->protlen = StaticPrompt (v, "            ", 0, stdLineHeight, systemFont, 'l');

    x = HiddenGroup (g, -1, 0, NULL);
    f = HiddenGroup (x, 6, 0, NULL);
    cfp->product = CreateProteinOrMRNAProductDialog (f, NULL, "Protein Product", FALSE, cfp->sep, cfp, NULL, (BaseFormPtr) cfp);
    cfp->protSeqIdGrp = HiddenGroup (f, -2, 0, NULL);
    StaticPrompt (cfp->protSeqIdGrp, "SeqID", 0, dialogTextHeight, programFont, 'l');
    cfp->protSeqIdTxt = DialogText (cfp->protSeqIdGrp, "", 6, NULL);
    cfp->forceNear = CheckBox (cfp->protSeqIdGrp, "Force Near, Create Product", NULL);

    cfp->protseq = CreateBioseqRawDialog (x, NULL, Seq_mol_aa, cfp);
    cfp->usethisbioseq = NULL;

    c = HiddenGroup (g, 3, 0, NULL);
    b = PushButton (c, "Predict Interval", PredictCdRegion);
    SetObjectExtra (b, cfp, NULL);
    b = PushButton (c, "Translate Product", TranslateProtein);
    SetObjectExtra (b, cfp, NULL);
    cfp->edSeqBtn = PushButton (c, "Edit Protein Sequence", LaunchProtSeqEd);
    SetObjectExtra (cfp->edSeqBtn, cfp, NULL);
    Disable (cfp->edSeqBtn);
    
    AlignObjects (ALIGN_LEFT, (HANDLE) f, (HANDLE) cfp->protseq, NULL);
    
    /* add switch to protein button only if indexer version */
    if (GetAppProperty ("InternalNcbiSequin") != NULL)
    {
      cfp->edProtBtn2 = PushButton (c, "Switch to Protein Feature Editor", SwitchToProtFeatEd);
      SetObjectExtra (cfp->edProtBtn2, cfp, NULL);
      Disable (cfp->edProtBtn2);      
    }
    b = PushButton (c, "Adjust for Stop Codon", AdjustForStopCodon);
    SetObjectExtra (b, cfp, NULL);

    AlignObjects (ALIGN_CENTER, (HANDLE) x, (HANDLE) c, (HANDLE) y, NULL);

    cpp->cdRgnGrp [1] = HiddenGroup (n, -1, 0, NULL);
    f = HiddenGroup (cpp->cdRgnGrp [1], 0, 0, NULL);
    cfp->protTextGrp = HiddenGroup (f, -1, 0, NULL);
    x = HiddenGroup (cfp->protTextGrp, 2, 0, NULL);
    SetGroupSpacing (x, 3, 5);
    StaticPrompt (x, "Name", 0, dialogTextHeight, programFont, 'l');
    cfp->protNameText = DialogText (x, "", 25, NULL);
    StaticPrompt (x, "Description", 0, dialogTextHeight, programFont, 'l');
    cfp->protDescText = DialogText (x, "", 25, NULL);
    cfp->protPromptGrp = HiddenGroup (f, -1, 0, NULL);
    StaticPrompt (cfp->protPromptGrp,
                  "Press Edit Protein Feature to change protein name",
                  0, 0, programFont, 'l');
    Hide (cfp->protPromptGrp);

    cfp->makemRNAMatch = CheckBox (cfp->protTextGrp, "Make overlapping mRNA product match protein name", NULL);
    Hide (cfp->makemRNAMatch);
    AlignObjects (ALIGN_CENTER, (HANDLE) x, (HANDLE) cfp->makemRNAMatch, NULL);

    z = HiddenGroup (cpp->cdRgnGrp [1], 3, 0, NULL);
    cfp->edProtBtn = PushButton (z, "Edit Protein Feature", LaunchProtFeatEd);
    SetObjectExtra (cfp->edProtBtn, cfp, NULL);
    Disable (cfp->edProtBtn);
    cfp->launchBtn = PushButton (z, "Launch Product Viewer", LaunchProteinViewer);
    SetObjectExtra (cfp->launchBtn, cfp, NULL);
    Disable (cfp->launchBtn);
    AlignObjects (ALIGN_CENTER, (HANDLE) cfp->protTextGrp, (HANDLE) cfp->protPromptGrp,
                  (HANDLE) z, NULL);
    Hide (cpp->cdRgnGrp [1]);

    cpp->cdRgnGrp [2] = HiddenGroup (n, -1, 0, NULL);
    CreateCodeBreakAlist (cpp);
    codebreak_widths [0] = 5;
    f = HiddenGroup (cpp->cdRgnGrp [2], 4, 0, NULL);
    StaticPrompt (f, "AA Position", 5 * stdCharWidth, 0, programFont, 'c');
    p2 = StaticPrompt (f, "Amino Acid", 0, 0, programFont, 'c');
    cpp->cdBrk = CreateTagListDialog (cpp->cdRgnGrp [2],
                                      3, 2, 2, codebreak_types,
                                      codebreak_widths, cpp->alists,
                                      CodeBreakPtrToCodeBreakDialog,
                                      CodeBreakDialogToCodeBreakPtr);
    tlp = (TagListPtr) GetObjectExtra (cpp->cdBrk);
    if (tlp != NULL) {
      AlignObjects (ALIGN_JUSTIFY, (HANDLE) tlp->control [1], (HANDLE) p2, NULL);
    }
    Hide (cpp->cdRgnGrp [2]);

    cpp->cdRgnGrp [3] = HiddenGroup (n, -1, 0, NULL);
    SetGroupSpacing (cpp->cdRgnGrp [3], 10, 20);
    q = HiddenGroup (cpp->cdRgnGrp [3], -1, 0, NULL);
    f = HiddenGroup (q, 0, -2, NULL);

    internalVersion = (Boolean) (GetAppProperty ("InternalNcbiSequin") != NULL);

    g = HiddenGroup (f, 5, 0, NULL);

    p2 = StaticPrompt (g, "Flags", 0, 0, programFont, 'l');
    if (! internalVersion) {
      Hide (p2);
    }
    cpp->conflict = CheckBox (g, "Conflict", NULL);
    if (! internalVersion) {
      Hide (cpp->conflict);
    }
    cpp->orf = CheckBox (g, "ORF", NULL);
    Hide (cpp->orf);

    AlignObjects (ALIGN_MIDDLE, (HANDLE) cpp->orf,
                  (HANDLE) cpp->conflict, (HANDLE) p2, NULL);

    cfp->conceptTransA = NULL;
    if (internalVersion) {
      cfp->conceptTransA = CheckBox (q, "Author-supplied conceptual translation", NULL);
    }

    StaticPrompt (q, "", 10, stdLineHeight, programFont, 'l');
    cfp->convertToMiscFeat = CheckBox (q, "Non-functional CDS, convert to misc_feature", ConvMiscFeatWarn);
    SetObjectExtra (cfp->convertToMiscFeat, cfp, NULL);
    cfp->saveAsMiscFeat = FALSE;
    Hide (cpp->cdRgnGrp [3]);

    AlignObjects (ALIGN_CENTER, (HANDLE) cpp->cdRgnGrp [0],
                  (HANDLE) cpp->cdRgnGrp [1], (HANDLE) cpp->cdRgnGrp [2],
                  (HANDLE) cpp->cdRgnGrp [3], NULL);
    AlignObjects (ALIGN_CENTER, (HANDLE) tbs, (HANDLE) k, NULL);
  }

  return (DialoG) p;
}

typedef struct genefinddata {
  ObjMgrPtr     omp;
  CdRgnFormPtr  cfp;
  CharPtr       title;
  Int2          idx;
  Int2          val;
  Boolean       geneFound;
} GeneFindData, PNTR GeneFindPtr;

static Boolean GeneFindFunc (GatherContextPtr gcp)

{
  GeneFindPtr    gfp;
  ObjMgrTypePtr  omtp;
  SeqFeatPtr     sfp;
  Char           thislabel [41];

  if (gcp == NULL) return TRUE;

  gfp = (GeneFindPtr) gcp->userdata;
  if (gfp == NULL ) return TRUE;

  thislabel [0] = '\0';

  if (gcp->thistype == OBJ_SEQFEAT) {
    sfp = (SeqFeatPtr) gcp->thisitem;
    if (sfp != NULL && sfp->data.choice == SEQFEAT_GENE) {
      omtp = ObjMgrTypeFind (gfp->omp, gcp->thistype, NULL, NULL);
      if (omtp == NULL) {
        return TRUE;
      }
      if (omtp->labelfunc != NULL) {
        (*(omtp->labelfunc)) (gcp->thisitem, thislabel, 40, OM_LABEL_CONTENT);
      }
      if (thislabel [0] != '\0') {
        gfp->idx++;
        if (StringICmp (thislabel, gfp->title) == 0) {
          gfp->val = gfp->idx;
          gfp->geneFound = TRUE;
          return FALSE;
        }
      }
    }
  }
  return TRUE;
}

static void SetGeneAndProtFields (CdRgnFormPtr cfp, CharPtr title)

{
  GeneFindData  gfd;
  GatherScope   gs;
  CharPtr       ptr;
  Char          str [128];

  if (cfp != NULL && title != NULL) {
    ptr = StringStr (title, "[prot=");
    if (ptr != NULL) {
      StringNCpy_0 (str, ptr + 6, sizeof (str));
      ptr = StringChr (str, ']');
      if (ptr != NULL) {
        *ptr = '\0';
        ptr = StringChr (str, ';');
        if (ptr != NULL) {
          *ptr = '\0';
          ptr++;
        }
        if (! StringHasNoText (str)) {
          SetTitle (cfp->protNameText, str);
        }
        if (! StringHasNoText (ptr)) {
          SetTitle (cfp->protDescText, ptr);
        }
      }
    }
    ptr = StringStr (title, "[gene=");
    if (ptr != NULL) {
      StringNCpy_0 (str, ptr + 6, sizeof (str));
      ptr = StringChr (str, ']');
      if (ptr != NULL) {
        *ptr = '\0';
        ptr = StringChr (str, ';');
        if (ptr != NULL) {
          *ptr = '\0';
          ptr++;
        }
        if (! StringHasNoText (str)) {
          SetTitle (cfp->geneSymbol, str);
        }
        if (! StringHasNoText (ptr)) {
          SetTitle (cfp->geneDesc, ptr);
        }
      }
      gfd.omp = ObjMgrGet ();
      gfd.cfp = cfp;
      gfd.title = str;
      gfd.idx = 2;
      gfd.val = 1;
      gfd.geneFound = FALSE;
      MemSet ((Pointer) (&gs), 0, sizeof (GatherScope));
      gs.seglevels = 1;
      gs.get_feats_location = TRUE;
      MemSet ((Pointer)(gs.ignore), (int)(TRUE), (size_t)(OBJ_MAX * sizeof(Boolean)));
      gs.ignore[OBJ_BIOSEQ] = FALSE;
      gs.ignore[OBJ_BIOSEQ_SEG] = FALSE;
      gs.ignore[OBJ_SEQFEAT] = FALSE;
      gs.ignore[OBJ_SEQANNOT] = FALSE;
      gs.scope = GetBestTopParentForItemID (cfp->input_entityID,
                                            cfp->input_itemID,
                                            cfp->input_itemtype);
      GatherEntity (cfp->input_entityID, (Pointer) &gfd, GeneFindFunc, &gs);
      if (gfd.geneFound) {
        SetValue (cfp->gene, gfd.val);
        SafeHide (cfp->newGeneGrp);
        SafeShow (cfp->editGeneBtn);
      } else if (TextHasNoText (cfp->geneSymbol)) {
        SetValue (cfp->gene, 1);
        SafeHide (cfp->newGeneGrp);
        SafeShow (cfp->editGeneBtn);
      } else {
        SetValue (cfp->gene, 2);
        SafeHide (cfp->editGeneBtn);
        SafeShow (cfp->newGeneGrp);
      }
    }
  }
}
static Boolean ImportCdRgnForm (ForM f, CharPtr filename)

{
  CdRgnFormPtr  cfp;
  Char          path [PATH_MAX];

  path [0] = '\0';
  StringNCpy_0 (path, filename, sizeof (path));
  cfp = (CdRgnFormPtr) GetObjectExtra (f);
  if (cfp != NULL) {
    switch (cfp->currentPage) {
      case CODING_REGION_PAGE :
        ShowImportProteinDialog ( cfp);
        break;
      case COMMON_PAGE :
        break;
      case LOCATION_PAGE :
        return ImportDialog (cfp->location, filename);
      default :
        break;
    }
  }
  return FALSE;
}

static Boolean ExportCdRgnForm (ForM f, CharPtr filename)

{
  BioseqPtr     bsp;
  CdRgnFormPtr  cfp;
  FILE          *fp;
  ErrSev        oldErrSev;
  Char          path [PATH_MAX];
  SeqEntryPtr   sep;

  path [0] = '\0';
  StringNCpy_0 (path, filename, sizeof (path));
  cfp = (CdRgnFormPtr) GetObjectExtra (f);
  if (cfp != NULL) {
    switch (cfp->currentPage) {
      case CODING_REGION_PAGE :
        if (path [0] != '\0' || GetOutputFileName (path, sizeof (path), NULL)) {
#ifdef WIN_MAC
          fp = FileOpen (path, "r");
          if (fp != NULL) {
            FileClose (fp);
          } else {
            FileCreate (path, "TEXT", "ttxt");
          }
#endif
          fp = FileOpen (path, "w");
          if (fp != NULL) {
            sep = SeqEntryNew ();
            if (sep != NULL) {
              bsp = DialogToPointer (cfp->protseq);
              sep->choice = 1;
              sep->data.ptrvalue = (Pointer) bsp;
              oldErrSev = ErrSetMessageLevel (SEV_MAX);
              /*
              SeqEntryToFasta (sep, fp, FALSE);
              */
              BioseqFastaStream (bsp, fp, STREAM_EXPAND_GAPS | STREAM_CORRECT_INVAL, 70, 0, 0, TRUE);
              ErrSetMessageLevel (oldErrSev);
            }
            FileClose (fp);
            SeqEntryFree (sep);
            return TRUE;
          }
        }
        break;
      case COMMON_PAGE :
        break;
      case LOCATION_PAGE :
        return ExportDialog (cfp->location, filename);
      default :
        break;
    }
  }
  return FALSE;
}

static CharPtr  cdRgnFormTabs [] = {
  "Coding Region", "Properties", "Location", NULL
};

static void CdRgnFormMessage (ForM f, Int2 mssg)

{
  CdRgnFormPtr  cfp;

  cfp = (CdRgnFormPtr) GetObjectExtra (f);
  if (cfp != NULL) {
    switch (mssg) {
      case VIB_MSG_INIT :
        StdInitFeatFormProc (f);
        break;
      case VIB_MSG_IMPORT :
        ImportCdRgnForm (f, NULL);
        break;
      case VIB_MSG_EXPORT :
        ExportCdRgnForm (f, NULL);
        break;
      case VIB_MSG_PRINT :
        break;
      case VIB_MSG_CLOSE :
        Remove (f);
        break;
      case VIB_MSG_CUT :
        StdCutTextProc (NULL);
        break;
      case VIB_MSG_COPY :
        StdCopyTextProc (NULL);
        break;
      case VIB_MSG_PASTE :
        StdPasteTextProc (NULL);
        break;
      case VIB_MSG_DELETE :
        if (cfp->currentPage == LOCATION_PAGE) {
          PointerToDialog (cfp->location, NULL);
        } else {
          StdDeleteTextProc (NULL);
        }
        break;
      default :
        if (cfp->appmessage != NULL) {
          cfp->appmessage (f, mssg);
        }
        break;
    }
  }
}

static CharPtr noLocMessage =
"Feature must have a location!  You can import a\n\
FASTA protein file from the Product subpage, which\n\
will generate a location from the protein sequence.";

static CharPtr cdsToMiscFeatMsg =
"You have indicated that this CDS is non-functional.\n\
Database policy is to convert this into a misc_feature,\n\
and to remove any protein product attached to the CDS.\n\
Are you sure you want to trigger this conversion now?";

static void SetComment (CharPtr new_comment, WindoW w)
{
  FeatureFormPtr ffp;
  CharPtr        old_comment;
  CharPtr        cp;
  CharPtr        complete_comment;
  Int4           old_len, new_len;
  
  if (new_comment == NULL || w == NULL) return;
  ffp = (FeatureFormPtr) GetObjectExtra (w);
  if (ffp == NULL) return;

  old_comment = SaveStringFromText (ffp->comment);
  if (StringHasNoText (old_comment))
  {
    SafeSetTitle (ffp->comment, new_comment);
  }
  else 
  {
    old_len = StringLen (old_comment);
    cp = StringStr (old_comment, new_comment);
    if (cp != NULL
        && (cp == old_comment || *(cp - 1) == ';')
        && (*(cp + old_len) == 0 || *(cp + old_len) == ';'))
    {
      /* do nothing, protein title already present */
    }
    else
    {
      new_len = StringLen (new_comment);
      
      complete_comment = (CharPtr) MemNew ((old_len + new_len + 2) * sizeof (Char));
      if (complete_comment != NULL)
      {
        StringCpy (complete_comment, new_comment);
        StringCat (complete_comment, ";");
        StringCat (complete_comment, old_comment);
        SafeSetTitle (ffp->comment, complete_comment);
      }
    }
  }
  old_comment = MemFree (old_comment); 
}

static Boolean FixCdRegionFormProteinFeatures (CdRgnFormPtr cfp, BioseqPtr newprot)
{
  SeqLocPtr slp;
  SeqIdPtr  sip;
  BioseqPtr target;
  Boolean   rval = TRUE;
  
  if (cfp == NULL || newprot == NULL)
  {
    return FALSE;
  }
  if (!cfp->protFound)
  {
    return TRUE;
  }
  
  slp = DialogToPointer (cfp->product);
  sip = SeqLocId (slp);
  target = BioseqFind (sip);
  if (target != NULL) {
    if (!SeqEdFixProteinFeatures (target, newprot, FALSE, Sequin_GlobalAlign2Seq))
    {
      if (ANS_CANCEL == Message (MSG_OKC, "Unable to construct alignment between old and new proteins.\nYou will need to adjust the protein features manually if you continue."))
      {
        rval = FALSE;
      }
    }
  }
  slp = SeqLocFree (slp);
  return rval;
}

static CharPtr infWarning1 =
"Bad inference qualifier!  You must conform to international nucleotide sequence database\nconventions for inference qualifiers!";

static CharPtr infWarning2 =
"Still has bad inference!  You must conform to international nucleotide sequence database\nconventions for inference qualifiers!";

static CharPtr infAccept =
"Do you want to accept changes with bad inference data?  The bad qualifier will be converted to a note!";

static Boolean SeeIfProtTitleNeedsFixing (BioseqPtr bsp, Uint2 entityID);


static Boolean NoChangeInGene (CdRgnFormPtr cfp, SeqFeatPtr sfp)
{
  Int2 usexref, val, i;
  GeneRefPtr grp_new, grp_orig, grpfeat;
  Boolean    rval = TRUE;
  ValNodePtr vnp;
  CharPtr    str;
  SeqFeatPtr gene;
  Char       symbol [128];
  Char       allele [128];
  Char       locustag [128];
  Char       desc [128];
  BioseqPtr  bsp;
  SeqMgrFeatContext fcontext;

  usexref = GetValue (cfp->useGeneXref);
  if (usexref == 3) {
    /* protein feature can now suppress gene on GenBank view */
    grp_orig = SeqMgrGetGeneXref (sfp);
    if (SeqMgrGeneIsSuppressed(grp_orig)) {
      rval = TRUE;
    } else {
      rval = FALSE;
    }
  } else if (cfp->gene != NULL) {
    val = GetValue (cfp->gene);
    if (usexref == 2) {
      if (val == 2) {
        /* new gene - ok if "new" gene xref matches old gene xref (represents gene not on sequence) */
        grp_orig = SeqMgrGetGeneXref (sfp);
        GetTitle (cfp->geneSymbol, symbol, sizeof (symbol));
        GetTitle (cfp->geneAllele, allele, sizeof (allele));
        GetTitle (cfp->geneDesc, desc, sizeof (desc));
        GetTitle (cfp->locusTag, locustag, sizeof (locustag));
        grp_new = CreateNewGeneRef (symbol, allele, desc, FALSE);
        if (! StringHasNoText (locustag)) {
          if (grp_new == NULL) {
            grp_new = GeneRefNew ();
          }
          grp_new->locus_tag = StringSave (locustag);
        }
        if (!TextHasNoText (cfp->geneSynonym)) {
          if (grp_new == NULL) {
            grp_new = GeneRefNew ();
          }
          ValNodeAddPointer (&(grp_new->syn), 0, SaveStringFromText (cfp->geneSynonym));
        }
        if (AsnIoMemComp (grp_orig, grp_new, (AsnWriteFunc) GeneRefAsnWrite)) {
          rval = TRUE;
        } else {
          rval = FALSE;
        }
        grp_new = GeneRefFree (grp_new);
      } else {
        /* mapping by crossref to existing gene - ok if mapping to same gene */
        grp_orig = SeqMgrGetGeneXref (sfp);
        grp_new = NULL;
        vnp = cfp->geneNames;
        i = val - 3;
        while (i > 0 && vnp != NULL) {
          vnp = vnp->next;
          i--;
        }
        if (vnp != NULL) {
          if (vnp->choice == 1) {
            str = (CharPtr) vnp->data.ptrvalue;
            if (StringDoesHaveText (str)) {
              grp_new = CreateNewGeneRef (str, NULL, NULL, FALSE);
              bsp = BioseqFindFromSeqLoc (sfp->location);
              gene = SeqMgrGetFeatureByLabel (bsp, str, SEQFEAT_GENE, 0, &fcontext);
              if (gene != NULL && gene->data.choice == SEQFEAT_GENE) {
                grpfeat = (GeneRefPtr) gene->data.value.ptrvalue;
                if (grpfeat != NULL) {
                  grp_new->locus_tag = StringSaveNoNull (grpfeat->locus_tag);
                }
              }
            }
          } else if (vnp->choice == 2) {
            grp_new = GeneRefNew ();
            if (grp_new != NULL) {
              grp_new->desc = StringSave ((CharPtr) vnp->data.ptrvalue);
            }
         } else if (vnp->choice == 3) {
            grp_new = GeneRefNew ();
            if (grp_new != NULL) {
              grp_new->locus_tag = StringSave ((CharPtr) vnp->data.ptrvalue);
            }
          }
        }
        if (AsnIoMemComp (grp_orig, grp_new, (AsnWriteFunc) GeneRefAsnWrite)) {
          rval = TRUE;
        } else {
          rval = FALSE;
        }
        grp_new = GeneRefFree (grp_new);
      }
    } else if (usexref == 1 && val == 2) {
      /* overlap to new gene, creates new gene */
      rval = FALSE;
    }
  }
  return rval;
}


static Boolean LabelChangeOnly (CdRgnFormPtr cfp)
{
  SeqFeatPtr        sfp;
  SeqMgrFeatContext context;
  SeqLocPtr         slp;
  Int2              cmp;
  Boolean           pseudo;
  GBQualPtr         qual, gbq;
  BioseqPtr         bsp_new, bsp_old = NULL;
  Boolean           match;
  CharPtr           str1, str2;
  Int2              expev;
  SeqFeatPtr        mrna, prot;
  RnaRefPtr         rrp;
  ProtRefPtr        prp;
  SeqFeatPtr        sfp_tmp;

  /* THIS HAS BEEN DECLARED UNSAFE */
  return FALSE;

  if (cfp == NULL || cfp->input_itemtype != OBJ_SEQFEAT) 
  {
    return FALSE;
  }

  if (GetStatus (cfp->convertToMiscFeat))
  {
    return FALSE;
  }
  
  if (cfp->autoRetranslate || cfp->autoUpdatemRNASpans) 
  {
    return FALSE;
  }

  sfp = SeqMgrGetDesiredFeature (cfp->input_entityID, NULL, cfp->input_itemID, 0, NULL, &context);
  if (sfp == NULL) 
  {
    return FALSE;
  }

  /* location needs to be the same */
  slp = DialogToPointer (cfp->location);
  match = AsnIoMemComp (slp, sfp->location, (AsnWriteFunc) SeqLocAsnWrite);
  slp = SeqLocFree (slp);
  if (!match)
  {
    return FALSE;
  }

  /* either was and is pseudo, or was not and is not pseudo */
  pseudo = FALSE;
  if (GetStatus (cfp->pseudo))
  {
    pseudo = TRUE;
  }
  else
  {
    qual = (GBQualPtr) DialogToPointer (cfp->gbquals);
    gbq = qual;
    while (gbq != NULL) {
      if (StringICmp (gbq->qual, "pseudo") == 0) {
        pseudo = TRUE;
      }
      gbq = gbq->next;
    }
    GBQualFree (qual);
  }

  if ((pseudo && !sfp->pseudo) || (!pseudo && sfp->pseudo)) 
  {
    return FALSE;
  }

  if (!pseudo) 
  {
    /* products need to be the same */
    slp = DialogToPointer (cfp->product);
    cmp = SeqLocCompare (slp, sfp->product);
    
    if (cmp != SLC_A_EQ_B) 
    {
      slp = SeqLocFree (slp);
      return FALSE;
    }
    /* look at product feature */
    bsp_old = BioseqFindFromSeqLoc (slp);
    slp = SeqLocFree (slp);

    prot = SeqMgrGetNextFeature (bsp_old, NULL, SEQFEAT_PROT, FEATDEF_PROT, &context);
    if (prot == NULL) {
      return FALSE;
    }

    /* check translation */
    bsp_new = (BioseqPtr) DialogToPointer (cfp->protseq);

    match = TRUE;
    if (bsp_new == NULL || bsp_old == NULL) {
      match = FALSE;
    } else if (bsp_new->length != bsp_old->length) {
      match = FALSE;
    } else {
      str1 = GetSequenceByBsp (bsp_new);
      str2 = GetSequenceByBsp (bsp_old);
      if (StringCmp (str1, str2) != 0) {
        match = FALSE;
      }
      str1 = MemFree (str1);
      str2 = MemFree (str2);
    }
    bsp_new = BioseqFree (bsp_new);
    if (!match) {
      return FALSE;
    }
  }

  if (!NoChangeInGene(cfp, sfp)) {
    return FALSE;
  }

  /* check feature IDs - no change allowed */
  sfp_tmp = SeqFeatNew ();
  if (cfp->featid != NULL) {
    TextToFeatID (cfp->featid, &(sfp_tmp->id));
  }
  if ((sfp->id.choice == 0 && sfp_tmp->id.choice != 0)
      || (sfp->id.choice != 0 && sfp_tmp->id.choice == 0)) {
    match = FALSE;
  } else {
    match = AsnIoMemComp (&(sfp_tmp->id), &(sfp->id), (AsnWriteFunc) SeqFeatIdAsnWrite);
  }
  if (match) {
    /* check feature ID xrefs - no change allowed */
    if (cfp->fidxref != NULL) {
      TextToFeatXref (cfp->fidxref, sfp_tmp);
    }
    if (sfp->xref != NULL || sfp_tmp->xref != NULL) {
      match = AsnIoMemComp (sfp->xref, sfp_tmp->xref, (AsnWriteFunc) SeqFeatXrefAsnWrite);
    }
  }
  sfp_tmp = SeqFeatFree (sfp_tmp);
  if (!match) {
    return FALSE;
  }


  /* do replacements */
  sfp->data.value.ptrvalue = CdRegionFree (sfp->data.value.ptrvalue);
  sfp->data.value.ptrvalue = DialogToPointer (cfp->data);
  sfp->comment = MemFree (sfp->comment);
  sfp->comment = SaveStringFromText (cfp->comment);
  NewlinesToTildes (sfp->comment);
  expev = GetValue (cfp->evidence);
  if (expev > 0 && expev <= 3) {
    sfp->exp_ev = expev - 1;
  } else {
    sfp->exp_ev = 0;
  }
  sfp->partial = GetStatus (cfp->partial);
  sfp->excpt = GetStatus (cfp->exception);
  sfp->pseudo = GetStatus (cfp->pseudo);
  sfp->except_text = MemFree (sfp->except_text);
  sfp->except_text = SaveStringFromText (cfp->exceptText);
  sfp->title = NULL;
  sfp->cit = PubSetFree(sfp->cit);
  sfp->cit = DialogToPointer (cfp->featcits);
  sfp->xref = SeqFeatXrefFree(sfp->xref);
  sfp->dbxref = DialogToPointer (cfp->dbxrefs);

  CleanupEvidenceGBQuals (&(sfp->qual));
  VisStringDialogToGbquals (sfp, cfp->experiment, "experiment");
  InferenceDialogToGBQuals (cfp->inference, sfp, TRUE);

  FixSpecialCharactersForObject (OBJ_SEQFEAT, sfp, "You may not include special characters in the text.\nIf you do not choose replacement characters, these special characters will be replaced with '#'.", TRUE, NULL);
  if (cfp->gbquals != NULL) {
    sfp->qual = GBQualFree(sfp->qual);
    sfp->qual = DialogToPointer (cfp->gbquals);
  }

 
  /* clean up the feature */
  CleanupOneSeqFeat (sfp);
 
  if (pseudo) {
    /* TODO - put product name/description in comment? */
  } else {
    prot = SeqMgrGetNextFeature (bsp_old, NULL, SEQFEAT_PROT, FEATDEF_PROT, &context);
    if (prot != NULL) {
      prp = (ProtRefPtr) prot->data.value.ptrvalue;
      if (prp == NULL) {
        prp = ProtRefNew ();
        prot->data.value.ptrvalue = prp;
      }
      if (prp->name == NULL) {
        ValNodeAddPointer (&(prp->name), 0, SaveStringFromText (cfp->protNameText));
      } else {
        prp->name->data.ptrvalue = MemFree (prp->name->data.ptrvalue);
        prp->name->data.ptrvalue = SaveStringFromText (cfp->protNameText);
      }
      prp->desc = MemFree (prp->desc);
      prp->desc = SaveStringFromText (cfp->protDescText);
    }
    SeeIfProtTitleNeedsFixing (bsp_old, bsp_old->idx.entityID);
  }

  /* adjust mRNA product name */
  if (GetStatus (cfp->makemRNAMatch)) {
    mrna = SeqMgrGetOverlappingmRNA (sfp->location, &context);
    if (mrna != NULL && mrna->idx.subtype == FEATDEF_mRNA) {
      rrp = (RnaRefPtr) mrna->data.value.ptrvalue;
      if (rrp == NULL) {
        rrp = RnaRefNew();
        mrna->data.value.ptrvalue = rrp;
      }
      rrp->ext.value.ptrvalue = MemFree (rrp->ext.value.ptrvalue);
      if (TextHasNoText (cfp->protNameText)) {
        rrp->ext.choice = 0;
      } else {
        rrp->ext.choice = 1;
        rrp->ext.value.ptrvalue = SaveStringFromText (cfp->protNameText);
      }
    }
  }

  ObjMgrSendMsgOnlyFeatLabelChange (OM_MSG_UPDATE, cfp->input_entityID,
                     cfp->input_itemID, cfp->input_itemtype);

  return TRUE;
}


static void CdRgnFormAcceptButtonProc (ButtoN b)

{
  MsgAnswer      ans;
  Int2           attempts = 3;
  Char           badInfMssg [32];
  Char           badInfQual [256];
  BioseqPtr      bsp;
  CdRgnFormPtr   cfp;
  GBQualPtr      gbq;
  SeqEntryPtr    old;
  OMProcControl  ompc;
  Boolean        pseudo;
  GBQualPtr      qual;
  SeqEntryPtr    sep;
  SeqIdPtr       sip;
  SeqLocPtr      slp = NULL;
  Char           str [128];
  WindoW         w;
  SeqLocPtr      prod_slp;
  Boolean        delete_old_product = FALSE;
  ValNodePtr     err_list = NULL;
  SeqEntryPtr    oldscope;

  if (b != NULL) {
    w = ParentWindow (b);
    cfp = (CdRgnFormPtr) GetObjectExtra (b);

    if (cfp != NULL && cfp->form != NULL && cfp->actproc != NULL) {
      sep = GetTopSeqEntryForEntityID (cfp->input_entityID);
      oldscope = SeqEntrySetScope (sep);
      if (! cfp->locvisited) {
        ErrPostEx (SEV_ERROR, 0, 0, "%s", noLocMessage);
        SeqEntrySetScope (oldscope);
        return;
      }
      if ((! cfp->acceptBadInf) && (! TestInference ((FeatureFormPtr) cfp, badInfQual, sizeof (badInfQual), badInfMssg))) {
        (cfp->badInfAttempts)++;
        if (GetAppProperty ("InternalNcbiSequin") != NULL) {
          attempts = 2;
        }
        if (cfp->badInfAttempts < attempts) {
          if (cfp->badInfAttempts == 2) {
            Message (MSG_OK, "%s - Please fix the %s error in\n%s", infWarning2, badInfMssg, badInfQual);
          } else {
            Message (MSG_OK, "%s - Please fix the %s error in\n%s", infWarning1, badInfMssg, badInfQual);
          }
          return;
        } else {
          if (Message (MSG_YN, "%s", infAccept) == ANS_NO) return;
          cfp->acceptBadInf = TRUE;
        }
      }
      slp = DialogToPointer (cfp->location);
      if (slp == NULL) {
        ErrPostEx (SEV_ERROR, 0, 0, "%s", noLocMessage);
        err_list = TestDialog (cfp->location);
        DisplayErrorMessages ("Location Errors", err_list);
        err_list = ValNodeFree (err_list);
        SeqEntrySetScope (oldscope);
        return;
      }

      cfp->saveAsMiscFeat = FALSE;
      if (GetStatus (cfp->convertToMiscFeat)) {
        ans = Message (MSG_OKC, "%s", cdsToMiscFeatMsg);
        if (ans == ANS_CANCEL) return;
        SeqLocFree (slp);
        Hide (w);
        Update ();
        bsp = NULL;
        slp = DialogToPointer (cfp->product); /* need to get current product reference */
        if (slp != NULL) {
          sip = SeqLocId (slp);
          if (sip != NULL) {
            bsp = BioseqFind (sip);
          }
        }
        SeqLocFree (slp);
        cfp->this_itemtype = OBJ_SEQFEAT;
        cfp->this_subtype = FEATDEF_IMP;
        cfp->saveAsMiscFeat = TRUE; /* now cfp->product dialog will return NULL */
        FeatFormReplaceWithoutUpdateProc (cfp->form);
        GetRidOfEmptyFeatsDescStrings (cfp->input_entityID, NULL);
        if (GetAppProperty ("InternalNcbiSequin") != NULL) {
          ExtendGeneFeatIfOnMRNA (cfp->input_entityID, NULL);
        }
        if (bsp != NULL) {
          MemSet ((Pointer) (&ompc), 0, sizeof (OMProcControl));
          ompc.do_not_reload_from_cache = TRUE;
          ompc.input_entityID = cfp->input_entityID;
          ompc.input_itemID = GetItemIDGivenPointer (cfp->input_entityID, OBJ_BIOSEQ, (Pointer) bsp);
          ompc.input_itemtype = OBJ_BIOSEQ;
          if (! DetachDataForProc (&ompc, FALSE)) {
            Message (MSG_ERROR, "DetachDataForProc failed");
          }
          sep = GetBestTopParentForItemID (cfp->input_entityID,
                                           cfp->input_itemID,
                                           cfp->input_itemtype);
          RenormalizeNucProtSets (sep, TRUE);
        }
        ObjMgrSendMsg (OM_MSG_UPDATE, cfp->input_entityID,
                       cfp->input_itemID, cfp->input_itemtype);
        ObjMgrDeSelect (0, 0, 0, 0, NULL);
        SeqEntrySetScope (oldscope);
        Update ();
        Remove (w);
        return;
      }

      ans = ANS_OK;
      if (cfp->autoRetranslate) {
        DoTranslateProtein (cfp);
      }
      
      pseudo = FALSE;
      if (GetStatus (cfp->pseudo))
      {
        pseudo = TRUE;
      }
      else
      {
        qual = (GBQualPtr) DialogToPointer (cfp->gbquals);
        gbq = qual;
        while (gbq != NULL) {
          if (StringICmp (gbq->qual, "pseudo") == 0) {
            pseudo = TRUE;
          }
          gbq = gbq->next;
        }
        GBQualFree (qual);
      }
      
      if (pseudo)
      {
        Hide (w);
        /* remove old product, if there is one */
        prod_slp = DialogToPointer (cfp->product);
        if (prod_slp != NULL)
        {
          bsp = BioseqFind (SeqLocId (prod_slp));
          if (bsp != NULL) {
            bsp->idx.deleteme = TRUE;
            delete_old_product = TRUE;
          }
          prod_slp = SeqLocFree (prod_slp);
        }
        /* do not create new product */
        PointerToDialog (cfp->product, NULL);
        PointerToDialog (cfp->protseq, NULL);
        
        /* if the protein name is set, move it to a note */
        GetTitle (cfp->protNameText, str, sizeof (str) - 1);
        if (! StringHasNoText (str))
        {
          SetComment (str, w);
          SetTitle (cfp->protNameText, "");
        }
      }
      else
      {
        bsp = (BioseqPtr) DialogToPointer (cfp->protseq);
        if (bsp == NULL) 
        {
          ans = Message (MSG_OKC, "The coding region has no peptide sequence.  Are you sure?");
          if (ans != ANS_OK)
          {
            SeqLocFree (slp);
            SeqEntrySetScope (oldscope);
            return;
          }
        }
        else
        {
          if (!FixCdRegionFormProteinFeatures (cfp, bsp))
          {
            SeqLocFree (slp);
            BioseqFree (bsp);
            SeqEntrySetScope (oldscope);
            return;
          }
        }
      
        bsp = BioseqFree (bsp);
        Hide (w);
        Update ();
        if (Visible (cfp->protSeqIdGrp)) {
          GetTitle (cfp->protSeqIdTxt, str, sizeof (str) - 1);
          if (StringHasNoText (str)) {
            sep = GetBestTopParentForItemID (cfp->input_entityID,
                                             cfp->input_itemID,
                                             cfp->input_itemtype);
            old = SeqEntrySetScope (sep);
            sip = MakeNewProteinSeqId (slp, NULL);
            SeqEntrySetScope (old);
            if (sip != NULL) {
              SeqIdWrite (sip, str, PRINTID_REPORT, sizeof (str));
              SafeSetTitle (cfp->protSeqIdTxt, str);
            }
            SeqIdFree (sip);
          } else {
            cfp->protXrefName = cfp->protNameText;
            cfp->protXrefDesc = cfp->protDescText;
          }
        }
      }
      if (LabelChangeOnly(cfp)) {

      } else {
        (cfp->actproc) (cfp->form);
      }
      SeqEntrySetScope (oldscope);
    }
    SeqLocFree (slp);
    if (delete_old_product)
    {
      DeleteMarkedObjects (cfp->input_entityID, 0, NULL);
      sep = GetBestTopParentForItemID (cfp->input_entityID,
                                       cfp->input_itemID,
                                       cfp->input_itemtype);
      RenormalizeNucProtSets (sep, TRUE);
      ObjMgrSendMsg (OM_MSG_UPDATE, cfp->input_entityID,
                       cfp->input_itemID, cfp->input_itemtype);
      ObjMgrDeSelect (0, 0, 0, 0, NULL);
    }
    
    Update ();
    Remove (w);
  }
}

static void CdRgnFormActivate (WindoW w)

{
  CdRgnFormPtr  cfp;

  cfp = (CdRgnFormPtr) GetObjectExtra (w);
  if (cfp != NULL) {
    if (cfp->activate != NULL) {
      cfp->activate (w);
    }
    SetCdRgnImportExportItems (cfp);
  }
}

extern ForM CreateCdRgnForm (Int2 left, Int2 top, CharPtr title,
                             SeqFeatPtr sfp, SeqEntryPtr sep,
                             FormActnFunc actproc)

{
  ButtoN             b;
  GrouP              c;
  CdRgnFormPtr       cfp;
  CdRgnPagePtr       cpp;
  GrouP              g;
  Int2               genCode;
  GrouP              h;
  GrouP              s;
  StdEditorProcsPtr  sepp;
  WindoW             w;
  GrouP              x;
  GrouP              y;

  w = NULL;
  cfp = (CdRgnFormPtr) MemNew (sizeof (CdRgnForm));
  if (cfp != NULL) {
    w = FixedWindow (left, top, -10, -10, title, StdCloseWindowProc);
    SetObjectExtra (w, cfp, StdFeatFormCleanupProc);
    cfp->form = (ForM) w;
    cfp->actproc = actproc;
    cfp->toform = CdRegionPtrToForm;
    cfp->fromform = NULL;
    cfp->formmessage = CdRgnFormMessage;
    cfp->testform = NULL;
    cfp->importform = ImportCdRgnForm;
    cfp->exportform = ExportCdRgnForm;

#ifndef WIN_MAC
    CreateStdEditorFormMenus (w);
#endif

    cfp->activate = NULL;
    sepp = (StdEditorProcsPtr) GetAppProperty ("StdEditorForm");
    if (sepp != NULL) {
      cfp->activate = sepp->activateForm;
      cfp->appmessage = sepp->handleMessages;
    }
    SetActivate (w, CdRgnFormActivate);

    g = HiddenGroup (w, -1, 0, NULL);
    SetGroupSpacing (g, 3, 10);

    cfp->sep = sep;
    cfp->foldertabs = CreateFolderTabs (g, cdRgnFormTabs, CODING_REGION_PAGE,
                                        0, 0, SYSTEM_FOLDER_TAB,
                                        ChangeCdRgnPage, (Pointer) cfp);
    cfp->currentPage = CODING_REGION_PAGE;

    h = HiddenGroup (g, 0, 0, NULL);

    s = HiddenGroup (h, -1, 0, NULL);
    genCode = SeqEntryToGeneticCode (sep, NULL, NULL, 0);
    cfp->data = CreateCdRgnDialog (s, NULL, genCode, sfp, cfp);
    cfp->pages [CODING_REGION_PAGE] = s;
    Hide (cfp->pages [CODING_REGION_PAGE]);

    s = HiddenGroup (h, -1, 0, NULL);
    /* tell dialog to have canned explanation popup */
    cfp->this_itemtype = OBJ_SEQFEAT;
    cfp->this_subtype = FEATDEF_CDS;
    CreateCommonFeatureGroup (s, (FeatureFormPtr) cfp, sfp, TRUE, TRUE);
    cfp->pages [COMMON_PAGE] = s;
    Hide (cfp->pages [COMMON_PAGE]);

    s = HiddenGroup (h, -1, 0, NULL);
    cfp->location = CreateIntervalEditorDialogEx (s, NULL, 4, 2, sep, TRUE, FALSE,
                                                  TRUE, TRUE, FALSE,
                                                  (FeatureFormPtr) cfp,
                                                  StdFeatIntEdPartialCallback);
    cfp->pages [LOCATION_PAGE] = s;
    Hide (cfp->pages [LOCATION_PAGE]);
    cfp->locvisited = TRUE;

    AlignObjects (ALIGN_CENTER, (HANDLE) cfp->pages [CODING_REGION_PAGE],
                  (HANDLE) cfp->pages [COMMON_PAGE],
                  (HANDLE) cfp->pages [LOCATION_PAGE], NULL);
    AlignObjects (ALIGN_CENTER, (HANDLE) cfp->foldertabs, (HANDLE) h, NULL);

    cpp = (CdRgnPagePtr) GetObjectExtra (cfp->data);
    cpp->commonGrp = HiddenGroup (w, -1, 0, NULL);
    SetGroupSpacing (cpp->commonGrp, 3, 5);
    y = HiddenGroup (cpp->commonGrp, 3, 0, NULL);
    b = CheckBox (y, "Update mRNA Span on Accept", SetUpdatemRNAProc);
    SetObjectExtra (b, cfp, NULL);
    x = HiddenGroup (cpp->commonGrp, 3, 0, NULL);
    b = CheckBox (x, "Retranslate on Accept", SetRetransProc);
    SetObjectExtra (b, cfp, NULL);
    cfp->autoRetranslate = FALSE;
    b = CheckBox (x, "Synchronize Partials", SetSynchProc);
    SetObjectExtra (b, cfp, NULL);
    SetStatus (b, TRUE);
    cfp->autoUpdatePartials = TRUE;
    cfp->autoUpdatemRNASpans = FALSE;
    AlignObjects (ALIGN_CENTER, (HANDLE) x, (HANDLE) y, NULL);

    c = HiddenGroup (w, 2, 0, NULL);
    b = PushButton (c, "Accept", CdRgnFormAcceptButtonProc);
    SetObjectExtra (b, cfp, NULL);
    PushButton (c, "Cancel", StdCancelButtonProc);
    AlignObjects (ALIGN_CENTER, (HANDLE) g, (HANDLE) c, (HANDLE) cpp->commonGrp, NULL);
    RealizeWindow (w);

    SendMessageToDialog (cfp->data, VIB_MSG_INIT);
    SendMessageToDialog (cfp->location, VIB_MSG_INIT);
    SendMessageToDialog (cfp->product, VIB_MSG_INIT);
    Show (cfp->pages [cfp->currentPage]);
    SendMessageToDialog (cfp->data, VIB_MSG_ENTER);
    Update ();
  }
  return (ForM) w;
}

typedef struct partialtrio {
  Boolean       partial;
  Boolean       partial5;
  Boolean       partial3;
  SeqEntryPtr   sep;
} PartialTrio, PNTR PartialTrioPtr;

static Boolean SetProteinPartial (GatherContextPtr gcp)

{
  SeqFeatPtr      sfp;
  PartialTrioPtr  trioPtr;

  trioPtr = (PartialTrioPtr) gcp->userdata;
  sfp = (SeqFeatPtr) gcp->thisitem;
  if (sfp != NULL && trioPtr != NULL) {
    sfp->partial = trioPtr->partial;
    if (trioPtr->sep != NULL) {
      sfp->location = SeqLocFree (sfp->location);
      sfp->location =  CreateWholeInterval (trioPtr->sep);
    }
    SetSeqLocPartial (sfp->location, trioPtr->partial5, trioPtr->partial3);
  }
  return TRUE;
}

static Boolean UpdateProteinName (GatherContextPtr gcp)

{
  CdRgnFormPtr  cfp;
  ProtRefPtr    prp;
  SeqFeatPtr    sfp;
  ValNodePtr    vnp;

  cfp = (CdRgnFormPtr) gcp->userdata;
  sfp = (SeqFeatPtr) gcp->thisitem;
  if (sfp == NULL || cfp == NULL) return TRUE;
  if (sfp->data.choice != SEQFEAT_PROT) return TRUE;
  prp = (ProtRefPtr) sfp->data.value.ptrvalue;
  if (prp == NULL) return TRUE;
  if (prp->name != NULL) {
    if (! TextHasNoText (cfp->protNameText)) {
      vnp = prp->name;
      vnp->data.ptrvalue = MemFree (vnp->data.ptrvalue);
      vnp->data.ptrvalue = SaveStringFromText (cfp->protNameText);
    } else {
      vnp = prp->name;
      vnp->data.ptrvalue = MemFree (vnp->data.ptrvalue);
      prp->name = vnp->next;
      MemFree (vnp);
    }
  } else if (! TextHasNoText (cfp->protNameText)) {
    vnp = ValNodeNew (NULL);
    prp->name = vnp;
    if (vnp != NULL) {
      vnp->data.ptrvalue = SaveStringFromText (cfp->protNameText);
    }
  }
      
  prp->desc = MemFree (prp->desc);
  prp->desc = SaveStringFromText (cfp->protDescText);
  return TRUE;
}

static void LookForRefTrack (
  SeqDescrPtr sdp,
  Pointer userdata
)

{
  BoolPtr        is_refseqP;
  ObjectIdPtr    oip;
  UserObjectPtr  uop;

  if (sdp->choice != Seq_descr_user) return;
  uop = (UserObjectPtr) sdp->data.ptrvalue;
  if (uop == NULL) return;
  oip = uop->type;
  if (oip == NULL || StringICmp (oip->str, "RefGeneTracking") != 0) return;
  is_refseqP = (BoolPtr) userdata;
  *is_refseqP = TRUE;
}

static Boolean SeeIfProtTitleNeedsFixing (BioseqPtr bsp, Uint2 entityID)

{
  MsgAnswer          ans = ANS_YES;
  BioseqSetPtr       bssp;
  CharPtr            buf;
  size_t             buflen = 1001;
  Boolean            indexerVersion;
  Boolean            is_refseq = FALSE;
  Boolean            rsult = FALSE;
  SeqEntryPtr        sep;
  CharPtr            title;
  ValNodePtr         vnp;

  if (bsp == NULL || (! ISA_aa (bsp->mol))) return FALSE;
  vnp = BioseqGetSeqDescr (bsp, Seq_descr_title, NULL);
  if (vnp == NULL) return FALSE;
  if (bsp->idx.parenttype == OBJ_BIOSEQSET) {
    bssp = (BioseqSetPtr) bsp->idx.parentptr;
    while (bssp != NULL && bssp->_class != BioseqseqSet_class_nuc_prot) {
      if (bssp->idx.parenttype == OBJ_BIOSEQSET) {
        bssp = (BioseqSetPtr) bssp->idx.parentptr;
      } else {
        bssp = NULL;
      }
    }
    if (bssp != NULL && bssp->_class == BioseqseqSet_class_nuc_prot) {
      title = (CharPtr) vnp->data.ptrvalue;
      buf = MemNew (sizeof (Char) * (buflen + 1));
      if (buf != NULL && NewCreateDefLineBuf (NULL, bsp, buf, buflen, TRUE, FALSE)) {
        if (StringICmp (buf, title) != 0) {
          indexerVersion = (Boolean) (GetAppProperty ("InternalNcbiSequin") != NULL);
          if (! indexerVersion) {
            ans = Message (MSG_YN, "Do you want to remove and recreate inconsistent title?");
          } else {
            sep = GetTopSeqEntryForEntityID (entityID);
            VisitDescriptorsInSep (sep, (Pointer) &is_refseq, LookForRefTrack);
            if (! is_refseq) {
              /* refseq wants to automatically fix, genbank wants to wait for AutoDef */
              ans = ANS_NO;
            }
          }
          if (ans == ANS_YES) {
            vnp->data.ptrvalue = MemFree (vnp->data.ptrvalue);
            vnp->data.ptrvalue = StringSave (buf);
            rsult = TRUE;
            /*
            if (vnp->extended != 0) {
              ovp = (ObjValNodePtr) vnp;
              ovp->idx.deleteme = TRUE;
              DeleteMarkedObjects (entityID, 0, NULL);
              rsult = TRUE;
            }
            */
          }
        }
      }
      MemFree (buf);
    }
  }
  return rsult;
}


extern void CdRgnFeatFormActnProc (ForM f)

{
  Char          badInfMssg [32];
  Char          badInfQual [256];
  BioseqPtr     bsp;
  CdRgnFormPtr  cfp;
  Char          desc [128];
  ValNodePtr    descr;
  GBQualPtr     gbq;
  MolInfoPtr    mip;
  Char          name [128];
  Boolean       partial;
  Boolean       partial5;
  Boolean       partial3;
  SeqEntryPtr   prot;
  ProtRefPtr    prp;
  GBQualPtr     quals;
  SeqEntryPtr   sep;
  Uint1         seq_data_type;
  SeqFeatPtr    sfp;
  SeqIdPtr      sip;
  SeqLocPtr     slp;
  BioseqPtr     target = NULL;
  PartialTrio   trio;
  ValNodePtr    vnp, err_list;

  cfp = (CdRgnFormPtr) GetObjectExtra (f);
  sep = NULL;
  if (cfp != NULL) {
    if (! cfp->locvisited) {
      ErrPostEx (SEV_ERROR, 0, 0, "Feature must have a location!");
      return;
    }
    if ((! cfp->acceptBadInf) && (! TestInference ((FeatureFormPtr) cfp, badInfQual, sizeof (badInfQual), badInfMssg))) {
      (cfp->badInfAttempts)++;
      if (cfp->badInfAttempts < 3) {
          if (cfp->badInfAttempts == 2) {
            Message (MSG_OK, "%s - Please fix the %s error in\n%s", infWarning2, badInfMssg, badInfQual);
          } else {
            Message (MSG_OK, "%s - Please fix the %s error in\n%s", infWarning1, badInfMssg, badInfQual);
          }
        return;
      } else {
        if (Message (MSG_YN, "%s", infAccept) == ANS_NO) return;
        cfp->acceptBadInf = TRUE;
      }
    }
    slp = DialogToPointer (cfp->location);
    if (slp == NULL) {
      ErrPostEx (SEV_ERROR, 0, 0, "Feature must have a location!");
      err_list = TestDialog (cfp->location);
      DisplayErrorMessages ("Location Errors", err_list);
      err_list = ValNodeFree (err_list);    
      return;
    }
    /** this was causing a coredump ***
    SeqLocFree (slp);
    **********************************/
    /*
    if (cfp->autoRetranslate) {
      DoTranslateProtein (cfp);
    }
    */

    /* check for partial now, so we can set mRNA partial if necessary. */
    CheckSeqLocForPartial (slp, &partial5, &partial3);


    SeqLocFree (slp);
    partial = GetStatus (cfp->partial);
    slp = DialogToPointer (cfp->product);
    if (slp != NULL) {
      quals = DialogToPointer (cfp->gbquals);
      cfp->usethisbioseq = NULL;
      gbq = quals;
      while (gbq != NULL) {
        if (StringICmp (gbq->qual, "pseudo") == 0) {
          GBQualFree (quals);
          SeqLocFree (slp);
          FeatFormReplaceWithoutUpdateProc (f);
          GetRidOfEmptyFeatsDescStrings (cfp->input_entityID, NULL);
          if (GetAppProperty ("InternalNcbiSequin") != NULL) {
            ExtendGeneFeatIfOnMRNA (cfp->input_entityID, NULL);
          }
          ObjMgrSendMsg (OM_MSG_UPDATE, cfp->input_entityID,
                         cfp->input_itemID, cfp->input_itemtype);
          return;
        }
        gbq = gbq->next;
      }
      GBQualFree (quals);
      if (cfp->protFound) {
        sip = SeqLocId (slp);
        if (sip != NULL) {
          target = BioseqFind (sip);
          if (target != NULL) {
            trio.sep = NULL;
            bsp = (BioseqPtr) DialogToPointer (cfp->protseq);
            if (bsp != NULL) {
              seq_data_type = target->seq_data_type;
              target->length = bsp->length;
              target->seq_data = SeqDataFree (target->seq_data, target->seq_data_type);
              target->seq_data = bsp->seq_data;
              target->seq_data_type = bsp->seq_data_type;
              bsp->seq_data = NULL;
              BioseqConvert (target, seq_data_type);
              /*BioseqConvert (target, Seq_code_ncbistdaa);*/
            }
            BioseqFree (bsp);
            trio.sep = SeqMgrGetSeqEntryForData (target);
            if (cfp->autoUpdatePartials) {
              partial = (Boolean) (partial || partial5 || partial3);
              trio.partial = partial;
              trio.partial5 = partial5;
              trio.partial3 = partial3;
              GatherItem (cfp->protEntityID, cfp->protItemID,
                          cfp->protItemtype, (Pointer) &trio, SetProteinPartial);
            }
            if (Visible (cfp->protTextGrp)) {
              GatherItem (cfp->protEntityID, cfp->protItemID,
                          cfp->protItemtype, (Pointer) cfp, UpdateProteinName);
            }
          }
        }
      }
      FeatFormReplaceWithoutUpdateProc (f);
      sip = SeqLocId (slp);
      if (sip != NULL) {
        target = BioseqFind (sip);
        if (target != NULL) {
          bsp = (BioseqPtr) DialogToPointer (cfp->protseq);
          if (bsp != NULL) {
            seq_data_type = target->seq_data_type;
            target->length = bsp->length;
            target->seq_data = SeqDataFree (target->seq_data, target->seq_data_type);
            target->seq_data = bsp->seq_data;
            target->seq_data_type = bsp->seq_data_type;
            bsp->seq_data = NULL;
            BioseqConvert (target, seq_data_type);
            /*BioseqConvert (target, Seq_code_ncbistdaa);*/
          }
          BioseqFree (bsp);
          if ((! cfp->protFound) && Visible (cfp->protTextGrp)) {
            GetTitle (cfp->protNameText, name, sizeof (name));
            GetTitle (cfp->protDescText, desc, sizeof (desc));
            prp = CreateNewProtRef (name, desc, NULL, NULL);
            if (prp != NULL) {
              sep = SeqMgrGetSeqEntryForData (target);
              if (sep != NULL) {
                sfp = CreateNewFeature (sep, NULL, SEQFEAT_PROT, NULL);
                if (sfp != NULL) {
                  sfp->data.value.ptrvalue = (Pointer) prp;
                  if (cfp->autoUpdatePartials) {
                    sfp->partial = (Boolean) (partial || partial5 || partial3);
                    if (sfp->partial) {
                      SetSeqLocPartial (sfp->location, partial5, partial3);
                    }
                  }
                }
              }
            }
          }
          sep = SeqMgrGetSeqEntryForData (target);
          if (sep != NULL && cfp->autoUpdatePartials) {
            vnp = SeqEntryGetSeqDescr (sep, Seq_descr_molinfo, NULL);
            if (vnp == NULL) {
              vnp = CreateNewDescriptor (sep, Seq_descr_molinfo);
              if (vnp != NULL) {
                mip = MolInfoNew ();
                vnp->data.ptrvalue = (Pointer) mip;
                if (mip != NULL) {
                  mip->biomol = 8;
                  mip->tech = 13;
                }
              }
            }
            if (vnp != NULL) {
              mip = (MolInfoPtr) vnp->data.ptrvalue;
              if (mip != NULL) {
                if (cfp->conceptTransA != NULL) {
                  if (GetStatus (cfp->conceptTransA)) {
                    mip->tech = 13;
                  } else {
                    if (mip->tech == 13) {
                      mip->tech = 8;
                    }
                  }
                }
                if (partial5 && partial3) {
                  mip->completeness = 5;
                } else if (partial5) {
                  mip->completeness = 3;
                } else if (partial3) {
                  mip->completeness = 4;
                } else if (partial) {
                  mip->completeness = 2;
                } else {
                  mip->completeness = 0;
                }
              }
            }
          }
        }
      }
    } else {
      bsp = (BioseqPtr) DialogToPointer (cfp->protseq);
      cfp->usethisbioseq = bsp;
      FeatFormReplaceWithoutUpdateProc (f);
      if (bsp != NULL) {
        prot = SeqEntryNew ();
        if (prot != NULL) {
          prot->choice = 1;
          prot->data.ptrvalue = (Pointer) bsp;
          SeqMgrSeqEntry (SM_BIOSEQ, (Pointer) bsp, prot);
          sep = GetBestTopParentForItemID (cfp->input_entityID,
                                           cfp->input_itemID,
                                           cfp->input_itemtype);
          /*
          sep = GetTopSeqEntryForEntityID (cfp->input_entityID);
          */
          descr = ExtractBioSourceAndPubs (sep);
          AddSeqEntryToSeqEntry (sep, prot, TRUE);
          ReplaceBioSourceAndPubs (sep, descr);
          cfp->input_entityID = ObjMgrGetEntityIDForChoice (sep);
          mip = MolInfoNew ();
          if (mip != NULL) {
            mip->biomol = 8;
            mip->tech = 13;
            if (cfp->conceptTransA != NULL) {
              if (! GetStatus (cfp->conceptTransA)) {
                mip->tech = 8;
              }
            }
            if (partial5 && partial3) {
              mip->completeness = 5;
            } else if (partial5) {
              mip->completeness = 3;
            } else if (partial3) {
              mip->completeness = 4;
            } else if (partial) {
              mip->completeness = 2;
            } else {
              mip->completeness = 0;
            }
            vnp = CreateNewDescriptor (prot, Seq_descr_molinfo);
            if (vnp != NULL) {
              vnp->data.ptrvalue = (Pointer) mip;
            }
          }
        }
        if (bsp != NULL && Visible (cfp->protTextGrp)) {
          GetTitle (cfp->protNameText, name, sizeof (name));
          GetTitle (cfp->protDescText, desc, sizeof (desc));
          prp = CreateNewProtRef (name, desc, NULL, NULL);
          if (prp != NULL) {
            sep = SeqMgrGetSeqEntryForData (bsp);
            if (sep != NULL) {
              sfp = CreateNewFeature (sep, NULL, SEQFEAT_PROT, NULL);
              if (sfp != NULL) {
                sfp->data.value.ptrvalue = (Pointer) prp;
                sfp->partial = (Boolean) (partial || partial5 || partial3);
                if (sfp->partial) {
                  SetSeqLocPartial (sfp->location, partial5, partial3);
                }
              }
            }
          }
        }
      }
    }
    SeqLocFree (slp);
    GetRidOfEmptyFeatsDescStrings (cfp->input_entityID, NULL);
    if (GetAppProperty ("InternalNcbiSequin") != NULL) {
      ExtendGeneFeatIfOnMRNA (cfp->input_entityID, NULL);
    }
    ObjMgrSendMsg (OM_MSG_UPDATE, cfp->input_entityID,
                   cfp->input_itemID, cfp->input_itemtype);
    if (target != NULL) {
      if (SeeIfProtTitleNeedsFixing (target, cfp->input_entityID)) {
        ObjMgrSendMsg (OM_MSG_UPDATE, cfp->input_entityID,
                       cfp->input_itemID, cfp->input_itemtype);
      }
    }
  }
}


NLM_EXTERN void UpdatemRNAAfterEditing(DialoG d, SeqLocPtr old_slp, SeqLocPtr new_slp)
{
  CdRgnPagePtr cpp;
  CdRgnFormPtr cfp;
  SeqFeatPtr   mrna;
  RnaRefPtr    rrp;
  SeqMgrFeatContext fcontext;
  Boolean           partial5, partial3;

  cpp = (CdRgnPagePtr) GetObjectExtra (d);
  if (cpp == NULL) {
    return;
  }
  cfp = cpp->cfp;
  if (cfp == NULL) {
    return;
  }
  /* adjust mRNA product name */
  if (GetStatus (cfp->makemRNAMatch) || cfp->autoUpdatemRNASpans) {
    mrna = SeqMgrGetOverlappingmRNA (cfp->autoUpdatemRNASpans ? old_slp : new_slp, &fcontext);
    if (mrna != NULL && mrna->idx.subtype == FEATDEF_mRNA) {
      rrp = (RnaRefPtr) mrna->data.value.ptrvalue;
      if (rrp == NULL) {
        rrp = RnaRefNew();
        mrna->data.value.ptrvalue = rrp;
      }
      if (cfp->autoUpdatemRNASpans) {
        mrna->location = SeqLocFree (mrna->location);
        mrna->location = SeqLocCopy (new_slp);
        CheckSeqLocForPartial (new_slp, &partial5, &partial3);
        mrna->partial = partial5 || partial3;
      }
      if (GetStatus (cfp->makemRNAMatch)) {
        rrp->ext.value.ptrvalue = MemFree (rrp->ext.value.ptrvalue);
        if (TextHasNoText (cfp->protNameText)) {
          rrp->ext.choice = 0;
        } else {
          rrp->ext.choice = 1;
          rrp->ext.value.ptrvalue = SaveStringFromText (cfp->protNameText);
        }
      }
    }
  }
}


static Int4 GuessFrameFromLocation (SeqLocPtr slp, BioseqPtr bsp)
{
  Int4 loc_len, loc_start;
  Int4 frame = 0;
  Uint1 strand;
  Boolean partial5, partial3;

  if (slp == NULL || bsp == NULL) return 0;

  CheckSeqLocForPartial (slp, &partial5, &partial3);
  if (!partial5 || partial3) return 0;

  strand = SeqLocStrand (slp);
  if (strand == Seq_strand_minus)
  {
    loc_start = SeqLocStop (slp);
  } 
  else
  {
    loc_start = SeqLocStart (slp);
  }
  if ((strand == Seq_strand_minus && loc_start == bsp->length -1)
      || (strand != Seq_strand_minus && loc_start == 0))
  {
    loc_len = SeqLocLen (slp);
    frame = 1 + loc_len %3;
  } 
  return frame;
}


extern Int2 LIBCALLBACK CdRgnGenFunc (Pointer data)

{
  BioseqPtr         bsp;
  CdRgnFormPtr      cfp;
  HelpMessageFunc   helpfunc;
  OMProcControlPtr  ompcp;
  OMUserDataPtr     omudp;
  SelStructPtr      sel;
  SeqEntryPtr       sep;
  SeqFeatPtr        sfp;
  WindoW            w;
  CdRgnPagePtr      cpp;
  Int4              frame = 0;

  ompcp = (OMProcControlPtr) data;
  sfp = NULL;
  sep = NULL;
  if (ompcp == NULL || ompcp->proc == NULL) return OM_MSG_RET_ERROR;
  switch (ompcp->input_itemtype) {
    case OBJ_SEQFEAT :
      sfp = (SeqFeatPtr) ompcp->input_data;
      if (sfp != NULL && sfp->data.choice != SEQFEAT_CDREGION) {
        return OM_MSG_RET_ERROR;
      }
      break;
    case OBJ_BIOSEQ :
      break;
    case OBJ_BIOSEQSET :
      break;
    case 0 :
      break;
    default :
      return OM_MSG_RET_ERROR;
  }
  omudp = ItemAlreadyHasEditor (ompcp->input_entityID, ompcp->input_itemID,
                                ompcp->input_itemtype, ompcp->proc->procid);
  if (omudp != NULL) {
    cfp = (CdRgnFormPtr) omudp->userdata.ptrvalue;
    if (cfp != NULL) {
      Select (cfp->form);
    }
    return OM_MSG_RET_DONE;
  }
  sep = GetTopSeqEntryForEntityID (ompcp->input_entityID);
  w = (WindoW) CreateCdRgnForm (-50, -33, "Coding Region", sfp, sep,
                                CdRgnFeatFormActnProc);
  cfp = (CdRgnFormPtr) GetObjectExtra (w);
  if (cfp != NULL) {
    cfp->input_entityID = ompcp->input_entityID;
    cfp->input_itemID = ompcp->input_itemID;
    cfp->input_itemtype = ompcp->input_itemtype;
    cfp->this_itemtype = OBJ_SEQFEAT;
    cfp->this_subtype = FEATDEF_CDS;
    cfp->procid = ompcp->proc->procid;
    cfp->proctype = ompcp->proc->proctype;
    cfp->userkey = OMGetNextUserKey ();
    omudp = ObjMgrAddUserData (ompcp->input_entityID, ompcp->proc->procid,
                               OMPROC_EDIT, cfp->userkey);
    if (omudp != NULL) {
      omudp->userdata.ptrvalue = (Pointer) cfp;
      omudp->messagefunc = StdVibrantEditorMsgFunc;
    }
    SendMessageToForm (cfp->form, VIB_MSG_INIT);
    if (sfp != NULL) {
      PointerToForm (cfp->form, (Pointer) sfp);
      if (SetClosestParentIfDuplicating ((BaseFormPtr) cfp)) {
        SetBestProteinFeature (cfp, NULL);
        PointerToDialog (cfp->product, NULL);
        PointerToDialog (cfp->protseq, NULL);
        SetProteinLengthDisplay (cfp->protlen, 0);
      }
    } else {
      /* restored, but locvisited flag protects against accidental CDS */
      SetNewFeatureDefaultInterval ((FeatureFormPtr) cfp);
      SetCdRegionPageGeneticCode (cfp->data);
      sel = ObjMgrGetSelected ();
      if (sel != NULL && sel->next == NULL && sel->entityID == cfp->input_entityID &&
          sel->itemID == cfp->input_itemID && sel->itemtype == cfp->input_itemtype) {
        if (sel->regiontype == 1 && sel->region != NULL) {
          bsp = GetBioseqGivenIDs (cfp->input_entityID, cfp->input_itemID, cfp->input_itemtype);
          if (bsp != NULL && GetBioseqGivenSeqLoc ((SeqLocPtr) sel->region, cfp->input_entityID) == bsp) {
            cpp = (CdRgnPagePtr) GetObjectExtra (cfp->data);
            if (cpp != NULL) {
              frame = GuessFrameFromLocation (sel->region, bsp);
              SetValue (cpp->frame, frame + 1);
            }
            DoTranslateProtein (cfp);
          }
        }
      }
    }
  }
  Show (w);
  Select (w);
  helpfunc = (HelpMessageFunc) GetAppProperty ("HelpMessageProc");
  if (helpfunc != NULL) {
    helpfunc ("Features", "CDS");
  }
  return OM_MSG_RET_DONE;
}

typedef struct genepage {
  DIALOG_MESSAGE_BLOCK
  TexT          locus;
  TexT          allele;
  TexT          desc;
  TexT          maploc;
  TexT          locus_tag;
  ButtoN        pseudo;
  DialoG        db;
  DialoG        syn;
  TexT          symbol;
  TexT          name;
  TexT          provider;
  TexT          source;
  GrouP         status;
  GrouP         geneGrp [4];
} GenePage, PNTR GenePagePtr;

typedef struct geneform {
  FEATURE_FORM_BLOCK
  SeqEntryPtr   sep;
  GrouP         pages [NUM_PAGES];
  DialoG        foldertabs;
  Int2          currentPage;
} GeneForm, PNTR GeneFormPtr;

static void GeneRefPtrToGenePage (DialoG d, Pointer data)

{
  Char                 buf [32];
  CharPtr              db = NULL;
  CharPtr              ds = NULL;
  DbtagPtr             dbt;
  ObjectIdPtr          oip;
  GenePagePtr          gpp;
  GeneNomenclaturePtr  gnp;
  GeneRefPtr           grp;

  gpp = (GenePagePtr) GetObjectExtra (d);
  grp = (GeneRefPtr) data;
  if (gpp != NULL) {
    if (grp != NULL) {
      SafeSetTitle (gpp->locus, grp->locus);
      SafeSetTitle (gpp->allele, grp->allele);
      SafeSetTitle (gpp->desc, grp->desc);
      SafeSetTitle (gpp->maploc, grp->maploc);
      SafeSetTitle (gpp->locus_tag, grp->locus_tag);
      PointerToDialog (gpp->db, grp->db);
      PointerToDialog (gpp->syn, grp->syn);
      gnp = grp->formal_name;
      if (gnp != NULL) {
        SafeSetValue (gpp->status, gnp->status);
        SafeSetTitle (gpp->symbol, gnp->symbol);
        SafeSetTitle (gpp->name, gnp->name);
        dbt = gnp->source;
        if (dbt != NULL) {
          if (StringDoesHaveText (dbt->db)) {
            db = dbt->db;
          }
          oip = dbt->tag;
          if (oip != NULL) {
            if (StringDoesHaveText (oip->str)) {
              ds = oip->str;
            } else {
              sprintf (buf, "%ld", (long) oip->id);
              ds = buf;
            }
          }
        }
        SafeSetTitle (gpp->provider, db);
        SafeSetTitle (gpp->source, ds);
      } else {
        SafeSetValue (gpp->status, 0);
        SafeSetTitle (gpp->symbol, "");
        SafeSetTitle (gpp->name, "");
        SafeSetTitle (gpp->provider, NULL);
        SafeSetTitle (gpp->source, NULL);
      }
    } else {
      SafeSetTitle (gpp->locus, "");
      SafeSetTitle (gpp->allele, "");
      SafeSetTitle (gpp->desc, "");
      SafeSetTitle (gpp->maploc, "");
      SafeSetTitle (gpp->locus_tag, "");
      PointerToDialog (gpp->db, NULL);
      PointerToDialog (gpp->syn, NULL);
      SafeSetValue (gpp->status, 0);
      SafeSetTitle (gpp->symbol, "");
      SafeSetTitle (gpp->name, "");
      SafeSetTitle (gpp->provider, NULL);
      SafeSetTitle (gpp->source, NULL);
    }
  }
}

static Boolean IsAllDigits (CharPtr str)
{
  CharPtr cp;

  if (StringHasNoText (str)) return FALSE;

  cp = str;
  while (*cp != 0 && isdigit (*cp)) {
    cp++;
  }
  if (*cp == 0) {
    return TRUE;
  } else {
    return FALSE;
  }
}

static Pointer GenePageToGeneRefPtr (DialoG d)

{
  CharPtr              db = NULL;
  CharPtr              ds = NULL;
  DbtagPtr             dbt;
  ObjectIdPtr          oip;
  GenePagePtr          gpp;
  GeneNomenclaturePtr  gnp;
  GeneRefPtr           grp;
  long int             val;

  grp = NULL;
  gpp = (GenePagePtr) GetObjectExtra (d);
  if (gpp != NULL) {
    grp = GeneRefNew ();
    if (grp != NULL) {
      grp->locus = SaveStringFromText (gpp->locus);
      grp->allele = SaveStringFromText (gpp->allele);
      grp->desc = SaveStringFromText (gpp->desc);
      grp->maploc = SaveStringFromText (gpp->maploc);
      grp->locus_tag = SaveStringFromText (gpp->locus_tag);
      grp->db = DialogToPointer (gpp->db);
      grp->syn = DialogToPointer (gpp->syn);
      gnp = GeneNomenclatureNew ();
      if (gnp != NULL) {
        gnp->status = GetValue (gpp->status);
        gnp->symbol = SaveStringFromText (gpp->symbol);
        gnp->name = SaveStringFromText (gpp->name);
        db = SaveStringFromText (gpp->provider);
        ds = SaveStringFromText (gpp->source);
        if (StringDoesHaveText (db) && StringDoesHaveText (ds)) {
          dbt = DbtagNew ();
          if (dbt != NULL) {
            oip = ObjectIdNew ();
            if (oip != NULL) {
              if (IsAllDigits (ds)) {
                sscanf (ds, "%ld", &val);
                oip->id = val;
              } else {
                oip->str = StringSave (ds);
              }
              dbt->db = StringSave (db);
              dbt->tag = oip;
              gnp->source = dbt;
            }
          }
        }
        MemFree (db);
        MemFree (ds);
        if (gnp->status == 0 &&
            StringHasNoText (gnp->symbol) &&
            StringHasNoText (gnp->name) &&
            gnp->source == NULL) {
          gnp = GeneNomenclatureFree (gnp);
        }
        grp->formal_name = gnp;
      }
    }
  }
  return (Pointer) grp;
}

static CharPtr geneTabs1 [] = {
  "General", "Synonyms", NULL
};

static CharPtr geneTabs2 [] = {
  "General", "Synonyms", "Cross-Refs", NULL
};

static CharPtr geneTabs3 [] = {
  "General", "Synonyms", "Nomenclature", NULL
};

static CharPtr geneTabs4 [] = {
  "General", "Synonyms", "Nomenclature", "Cross-Refs", NULL
};

static void ChangeGeneSubPage (VoidPtr data, Int2 newval, Int2 oldval)

{
  GenePagePtr  gpp;

  gpp = (GenePagePtr) data;
  if (gpp != NULL) {
    if (oldval >= 0 && oldval <= 3) {
      SafeHide (gpp->geneGrp [oldval]);
    }
    if (newval >= 0 && newval <= 3) {
      SafeShow (gpp->geneGrp [newval]);
    }
    Update ();
  }
}

static void LookForRefSeq (BioseqPtr bsp, Pointer userdata)

{
  BoolPtr   isRefSeqP;
  SeqIdPtr  sip;

  if (bsp == NULL || userdata == NULL) return;
  isRefSeqP = (BoolPtr) userdata;
  for (sip = bsp->id; sip != NULL; sip = sip->next) {
    if (sip->choice == SEQID_OTHER) {
      *isRefSeqP = TRUE;
      return;
    }
  }
}

static Boolean GeneIsInRefSeq (SeqEntryPtr sep)

{
  Boolean  is_refseq = FALSE;

  if (sep == NULL) return FALSE;
  VisitBioseqsInSep (sep, (Pointer) &is_refseq, LookForRefSeq);
  return is_refseq;
}

static DialoG CreateGeneDialog (GrouP h, CharPtr title, GeneRefPtr grp, GeneFormPtr gfp)

{
  GrouP         f;
  GrouP         g;
  GenePagePtr   gpp;
  GrouP         j;
  Char          just;
  GrouP         k;
  GrouP         m;
  GrouP         p;
  GrouP         q;
  GrouP         s;
  Boolean       showNomen = FALSE;
  Boolean       showXrefs = FALSE;
  GrouP         t;
  CharPtr PNTR  tabs = NULL;
  DialoG        tbs;
  Int2          x;

  p = HiddenGroup (h, 1, 0, NULL);
  SetGroupSpacing (p, 10, 10);

  gpp = (GenePagePtr) MemNew (sizeof (GenePage));
  if (gpp != NULL) {

    SetObjectExtra (p, gpp, StdCleanupExtraProc);
    gpp->dialog = (DialoG) p;
    gpp->todialog = GeneRefPtrToGenePage;
    gpp->fromdialog = GenePageToGeneRefPtr;
    gpp->testdialog = NULL;

    if (title != NULL && title [0] != '\0') {
      s = NormalGroup (p, 0, -2, title, systemFont, NULL);
    } else {
      s = HiddenGroup (p, 0, -2, NULL);
    }
    m = HiddenGroup (s, -1, 0, NULL);
    SetGroupSpacing (m, 10, 10);

    if (grp != NULL && grp->db != NULL) {
      showXrefs = TRUE;
    }
    if (gfp != NULL && GeneIsInRefSeq (gfp->sep)) {
      showNomen = TRUE;
    }

    tabs = geneTabs1;
    if (showXrefs) {
      if (showNomen) {
        tabs = geneTabs4;
      } else {
        tabs = geneTabs2;
      }
    } else if (showNomen) {
      tabs = geneTabs3;
    }

    tbs = CreateFolderTabs (m, tabs, 0, 0, 0,
                            PROGRAM_FOLDER_TAB,
                            ChangeGeneSubPage, (Pointer) gpp);
    k = HiddenGroup (m, 0, 0, NULL);

    for (x = 0; x < 4; x++) {
      gpp->geneGrp [x] = NULL;
    }
    x = 0;

    gpp->geneGrp [x] = HiddenGroup (k, -1, 0, NULL);
    f = HiddenGroup (gpp->geneGrp [x], 2, 0, NULL);
    StaticPrompt (f, "Locus", 0, dialogTextHeight, programFont, 'l');
    gpp->locus = DialogText (f, "", 15, NULL);
    StaticPrompt (f, "Allele", 0, dialogTextHeight, programFont, 'l');
    gpp->allele = DialogText (f, "", 15, NULL);
    StaticPrompt (f, "Description", 0, dialogTextHeight, programFont, 'l');
    gpp->desc = DialogText (f, "", 15, NULL);
    StaticPrompt (f, "Map Location", 0, dialogTextHeight, programFont, 'l');
    gpp->maploc = DialogText (f, "", 15, NULL);
    StaticPrompt (f, "Locus Tag", 0, dialogTextHeight, programFont, 'l');
    gpp->locus_tag = DialogText (f, "", 15, NULL);

    x++;

    gpp->geneGrp [x] = HiddenGroup (k, -1, 0, NULL);
    g = HiddenGroup (gpp->geneGrp [x], 0, 2, NULL);
    StaticPrompt (g, "Synonyms", 0, 0, programFont, 'c');
    gpp->syn = CreateVisibleStringDialog (g, 3, -1, 15);
    Hide (gpp->geneGrp [x]);
    x++;

    if (showNomen) {
      gpp->geneGrp [x] = HiddenGroup (k, -1, 0, NULL);
      j = HiddenGroup (gpp->geneGrp [x], 2, 0, NULL);

      StaticPrompt (j, "Symbol", 0, 0, programFont, 'l');
      gpp->symbol = DialogText (j, "", 15, NULL);
      StaticPrompt (j, "Name", 0, 0, programFont, 'l');
      gpp->name = DialogText (j, "", 15, NULL);
      StaticPrompt (j, "Provider", 0, 0, programFont, 'l');
      gpp->provider = DialogText (j, "", 15, NULL);
      StaticPrompt (j, "Source", 0, 0, programFont, 'l');
      gpp->source = DialogText (j, "", 15, NULL);
      StaticPrompt (j, "Status", 0, 0, programFont, 'l');
      gpp->status = HiddenGroup (j, 3, 0, NULL);
      RadioButton (gpp->status, "Official");
      RadioButton (gpp->status, "Interim");
      AlignObjects (ALIGN_CENTER, (HANDLE) gpp->status, (HANDLE) j, NULL);
      Hide (gpp->geneGrp [x]);
      x++;
    }

    if (showXrefs) {
      gpp->geneGrp [x] = HiddenGroup (k, -1, 0, NULL);
      q = HiddenGroup (gpp->geneGrp [x], -1, 0, NULL);
      if (GetAppProperty ("ReadOnlyDbTags") == NULL) {
        just = 'c';
      } else {
        just = 'l';
        StaticPrompt (q, "This page is read-only", 15 * stdCharWidth, 0, programFont, 'c');
      }
      t = HiddenGroup (q, 2, 0, NULL);
      StaticPrompt (t, "Database", 7 * stdCharWidth, 0, programFont, just);
      StaticPrompt (t, "Object ID", 8 * stdCharWidth, 0, programFont, just);
      gpp->db = CreateDbtagDialog (q, 3, -1, 7, 8);
      Hide (gpp->geneGrp [x]);
    }

    AlignObjects (ALIGN_CENTER, (HANDLE) tbs,
                  (HANDLE) gpp->geneGrp [0], (HANDLE) gpp->geneGrp [1],
                  (HANDLE) gpp->geneGrp [2], (HANDLE) gpp->geneGrp [3], NULL);
  }

  return (DialoG) p;
}

static void SetGeneImportExportItems (GeneFormPtr gfp)

{
  IteM  exportItm;
  IteM  importItm;

  if (gfp != NULL) {
    importItm = FindFormMenuItem ((BaseFormPtr) gfp, VIB_MSG_IMPORT);
    exportItm = FindFormMenuItem ((BaseFormPtr) gfp, VIB_MSG_EXPORT);
    switch (gfp->currentPage) {
      case GENE_PAGE :
        SafeSetTitle (importItm, "Import...");
        SafeSetTitle (exportItm, "Export...");
        SafeDisable (importItm);
        SafeDisable (exportItm);
        break;
      case COMMON_PAGE :
        SafeSetTitle (importItm, "Import...");
        SafeSetTitle (exportItm, "Export...");
        SafeDisable (importItm);
        SafeDisable (exportItm);
        break;
      case LOCATION_PAGE :
        SafeSetTitle (importItm, "Import SeqLoc...");
        SafeSetTitle (exportItm, "Export SeqLoc...");
        SafeEnable (importItm);
        SafeEnable (exportItm);
        break;
      default :
        break;
    }
  }
}

static void ChangeGenePage (VoidPtr data, Int2 newval, Int2 oldval)

{
  GeneFormPtr   gfp;

  gfp = (GeneFormPtr) data;
  if (gfp != NULL) {
    gfp->currentPage = newval;
    SafeHide (gfp->pages [oldval]);
    SafeShow (gfp->pages [newval]);
    switch (newval) {
      case GENE_PAGE :
        SendMessageToDialog (gfp->data, VIB_MSG_ENTER);
        break;
      case COMMON_PAGE :
        break;
      case LOCATION_PAGE :
        SendMessageToDialog (gfp->location, VIB_MSG_ENTER);
        break;
      default :
        break;
    }
    SetGeneImportExportItems (gfp);
    Update ();
  }
}

static Boolean ImportGeneForm (ForM f, CharPtr filename)

{
  GeneFormPtr  gfp;

  gfp = (GeneFormPtr) GetObjectExtra (f);
  if (gfp != NULL) {
    switch (gfp->currentPage) {
      case LOCATION_PAGE :
        return ImportDialog (gfp->location, filename);
      default :
        break;
    }
  }
  return FALSE;
}

static Boolean ExportGeneForm (ForM f, CharPtr filename)

{
  GeneFormPtr  gfp;

  gfp = (GeneFormPtr) GetObjectExtra (f);
  if (gfp != NULL) {
    switch (gfp->currentPage) {
      case LOCATION_PAGE :
        return ExportDialog (gfp->location, filename);
      default :
        break;
    }
  }
  return FALSE;
}

static CharPtr  geneFormTabs [] = {
  "Gene", "Properties", "Location", NULL
};

static void GeneFormMessage (ForM f, Int2 mssg)

{
  GeneFormPtr  gfp;

  gfp = (GeneFormPtr) GetObjectExtra (f);
  if (gfp != NULL) {
    switch (mssg) {
      case VIB_MSG_INIT :
        StdInitFeatFormProc (f);
        break;
      case VIB_MSG_IMPORT :
        ImportGeneForm (f, NULL);
        break;
      case VIB_MSG_EXPORT :
        ExportGeneForm (f, NULL);
        break;
      case VIB_MSG_CLOSE :
        Remove (f);
        break;
      case VIB_MSG_CUT :
        StdCutTextProc (NULL);
        break;
      case VIB_MSG_COPY :
        StdCopyTextProc (NULL);
        break;
      case VIB_MSG_PASTE :
        StdPasteTextProc (NULL);
        break;
      case VIB_MSG_DELETE :
        if (gfp->currentPage == LOCATION_PAGE) {
          PointerToDialog (gfp->location, NULL);
        } else {
          StdDeleteTextProc (NULL);
        }
        break;
      default :
        if (gfp->appmessage != NULL) {
          gfp->appmessage (f, mssg);
        }
        break;
    }
  }
}

static void GeneFormActivate (WindoW w)

{
  GeneFormPtr  gfp;

  gfp = (GeneFormPtr) GetObjectExtra (w);
  if (gfp != NULL) {
    if (gfp->activate != NULL) {
      gfp->activate (w);
    }
    SetGeneImportExportItems (gfp);
  }
}

extern ForM CreateGeneForm (Int2 left, Int2 top, CharPtr title,
                            SeqFeatPtr sfp, SeqEntryPtr sep,
                            FormActnFunc actproc)

{
  ButtoN             b;
  GrouP              c;
  GrouP              g;
  GeneFormPtr        gfp;
  GeneRefPtr         grp;
  GrouP              h;
  GrouP              s;
  StdEditorProcsPtr  sepp;
  WindoW             w;

  w = NULL;
  gfp = (GeneFormPtr) MemNew (sizeof (GeneForm));
  if (gfp != NULL) {
    w = FixedWindow (left, top, -10, -10, title, StdCloseWindowProc);
    SetObjectExtra (w, gfp, StdFeatFormCleanupProc);
    gfp->form = (ForM) w;
    gfp->actproc = actproc;
    gfp->toform = StdSeqFeatPtrToFeatFormProc;
    gfp->fromform = NULL;
    gfp->formmessage = GeneFormMessage;
    gfp->testform = NULL;
    gfp->importform = ImportGeneForm;
    gfp->exportform = ExportGeneForm;

#ifndef WIN_MAC
    CreateStdEditorFormMenus (w);
#endif

    gfp->activate = NULL;
    sepp = (StdEditorProcsPtr) GetAppProperty ("StdEditorForm");
    if (sepp != NULL) {
      gfp->activate = sepp->activateForm;
      gfp->appmessage = sepp->handleMessages;
    }
    SetActivate (w, GeneFormActivate);

    g = HiddenGroup (w, -1, 0, NULL);
    SetGroupSpacing (g, 3, 10);

    gfp->sep = sep;
    gfp->foldertabs = CreateFolderTabs (g, geneFormTabs, GENE_PAGE,
                                        0, 0, SYSTEM_FOLDER_TAB,
                                        ChangeGenePage, (Pointer) gfp);
    gfp->currentPage = GENE_PAGE;

    h = HiddenGroup (g, 0, 0, NULL);

    s = HiddenGroup (h, -1, 0, NULL);
    grp = NULL;
    if (sfp != NULL && sfp->data.choice == SEQFEAT_GENE) {
      grp = sfp->data.value.ptrvalue;
    }
    gfp->data = CreateGeneDialog (s, NULL, grp, gfp);
    gfp->pages [GENE_PAGE] = s;
    Hide (gfp->pages [GENE_PAGE]);

    s = HiddenGroup (h, -1, 0, NULL);
    CreateCommonFeatureGroup (s, (FeatureFormPtr) gfp, sfp, FALSE, TRUE);
    gfp->pages [COMMON_PAGE] = s;
    Hide (gfp->pages [COMMON_PAGE]);

    s = HiddenGroup (h, -1, 0, NULL);
    gfp->location = CreateIntervalEditorDialogEx (s, NULL, 4, 2, sep, TRUE, FALSE,
                                                  TRUE, TRUE, FALSE,
                                                  (FeatureFormPtr) gfp,
                                                  StdFeatIntEdPartialCallback);
    gfp->pages [LOCATION_PAGE] = s;
    Hide (gfp->pages [LOCATION_PAGE]);

    AlignObjects (ALIGN_CENTER, (HANDLE) gfp->pages [GENE_PAGE],
                  (HANDLE) gfp->pages [COMMON_PAGE], (HANDLE) gfp->pages [LOCATION_PAGE],
                  NULL);
    AlignObjects (ALIGN_CENTER, (HANDLE) gfp->foldertabs, (HANDLE) h, NULL);

    c = HiddenGroup (w, 2, 0, NULL);
    b = PushButton (c, "Accept", StdFeatFormAcceptButtonProc);
    SetObjectExtra (b, gfp, NULL);
    PushButton (c, "Cancel", StdCancelButtonProc);
    AlignObjects (ALIGN_CENTER, (HANDLE) g, (HANDLE) c, NULL);
    RealizeWindow (w);

    SendMessageToDialog (gfp->data, VIB_MSG_INIT);
    SendMessageToDialog (gfp->location, VIB_MSG_INIT);
    Show (gfp->pages [gfp->currentPage]);
    SendMessageToDialog (gfp->data, VIB_MSG_ENTER);
    Update ();
  }
  return (ForM) w;
}

static Boolean CDSPseudoByGeneFunc (GatherContextPtr gcp, Boolean pseudo)

{
  GBQualPtr   gbq;
  GBQualPtr   next;
  GBQualPtr PNTR  prev;
  GBQualPtr   qual;
  SeqFeatPtr  sfp;
  SeqLocPtr   slp;

  if (gcp == NULL) return TRUE;
  slp = (SeqLocPtr) gcp->userdata;
  if (slp == NULL) return TRUE;
  if (gcp->thistype == OBJ_SEQFEAT) {
    sfp = (SeqFeatPtr) gcp->thisitem;
    if (sfp != NULL && sfp->data.choice == SEQFEAT_CDREGION) {
      if (SeqLocAinB (sfp->location, slp) >= 0) {
        gbq = sfp->qual;
        prev = (GBQualPtr PNTR) &(sfp->qual);
        while (gbq != NULL) {
          next = gbq->next;
          if (StringICmp (gbq->qual, "pseudo") == 0) {
            if (pseudo) return TRUE;
            *(prev) = gbq->next;
            gbq->next = NULL;
            GBQualFree (gbq);
          } else {
            prev = (GBQualPtr PNTR) &(gbq->next);
          }
          gbq = next;
        }
        if (! pseudo) return TRUE;
        qual = GBQualNew ();
        if (qual != NULL) {
          qual->qual = StringSave ("pseudo");
          qual->val = StringSave ("");
          gbq = sfp->qual;
          if (gbq != NULL) {
            while (gbq->next != NULL) {
              gbq = gbq->next;
            }
            gbq->next = qual;
          } else {
            sfp->qual = qual;
          }
        }
      }
    }
  }
  return TRUE;
}

static Boolean CDSPseudoOnFunc (GatherContextPtr gcp)

{
  return CDSPseudoByGeneFunc (gcp, TRUE);
}

static Boolean CDSPseudoOffFunc (GatherContextPtr gcp)

{
  return CDSPseudoByGeneFunc (gcp, FALSE);
}

static void GeneFeatFormActnProc (ForM f)

{
  GeneFormPtr  gfp;
  GenePagePtr  gpp;
  GatherScope  gs;
  Boolean      pseudo = FALSE;
  SeqEntryPtr  sep;
  SeqLocPtr    slp = NULL;

  gfp = (GeneFormPtr) GetObjectExtra (f);
  if (gfp != NULL) {
    slp = DialogToPointer (gfp->location);
    gpp = GetObjectExtra (gfp->data);
    if (gpp != NULL) {
      pseudo = GetStatus (gpp->pseudo);
    }
  }
  if (FeatFormReplaceWithoutUpdateProc (f)) {
    if (gfp != NULL) {
      sep = GetBestTopParentForItemID (gfp->input_entityID,
                                       gfp->input_itemID,
                                       gfp->input_itemtype);
      if (slp != NULL) {
        MemSet ((Pointer) (&gs), 0, sizeof (GatherScope));
        gs.seglevels = 1;
        gs.get_feats_location = FALSE;
        MemSet ((Pointer)(gs.ignore), (int)(TRUE), (size_t)(OBJ_MAX * sizeof(Boolean)));
        gs.ignore[OBJ_BIOSEQ] = FALSE;
        gs.ignore[OBJ_BIOSEQ_SEG] = FALSE;
        gs.ignore[OBJ_SEQFEAT] = FALSE;
        gs.ignore[OBJ_SEQANNOT] = FALSE;
        gs.scope = sep;
        if (pseudo) {
          GatherEntity (gfp->input_entityID, (Pointer) slp, CDSPseudoOnFunc, &gs);
        } else {
          GatherEntity (gfp->input_entityID, (Pointer) slp, CDSPseudoOffFunc, &gs);
        }
      }
      GetRidOfEmptyFeatsDescStrings (gfp->input_entityID, NULL);
      if (GetAppProperty ("InternalNcbiSequin") != NULL) {
        ExtendGeneFeatIfOnMRNA (gfp->input_entityID, NULL);
      }
      ObjMgrSendMsg (OM_MSG_UPDATE, gfp->input_entityID, 0, 0);
    }
  }
  SeqLocFree (slp);
}

extern Int2 LIBCALLBACK GeneGenFunc (Pointer data)

{
  GeneFormPtr       gfp;
  HelpMessageFunc   helpfunc;
  OMProcControlPtr  ompcp;
  OMUserDataPtr     omudp;
  SeqEntryPtr       sep;
  SeqFeatPtr        sfp;
  WindoW            w;

  ompcp = (OMProcControlPtr) data;
  sfp = NULL;
  sep = NULL;
  if (ompcp == NULL || ompcp->proc == NULL) return OM_MSG_RET_ERROR;
  switch (ompcp->input_itemtype) {
    case OBJ_SEQFEAT :
      sfp = (SeqFeatPtr) ompcp->input_data;
      if (sfp != NULL && sfp->data.choice != SEQFEAT_GENE) {
        return OM_MSG_RET_ERROR;
      }
      break;
    case OBJ_BIOSEQ :
      break;
    case OBJ_BIOSEQSET :
      break;
    case 0 :
      break;
    default :
      return OM_MSG_RET_ERROR;
  }
  omudp = ItemAlreadyHasEditor (ompcp->input_entityID, ompcp->input_itemID,
                                ompcp->input_itemtype, ompcp->proc->procid);
  if (omudp != NULL) {
    gfp = (GeneFormPtr) omudp->userdata.ptrvalue;
    if (gfp != NULL) {
      Select (gfp->form);
    }
    return OM_MSG_RET_DONE;
  }
  sep = GetTopSeqEntryForEntityID (ompcp->input_entityID);
  w = (WindoW) CreateGeneForm (-50, -33, "Gene", sfp, sep,
                               GeneFeatFormActnProc);
  gfp = (GeneFormPtr) GetObjectExtra (w);
  if (gfp != NULL) {
    gfp->input_entityID = ompcp->input_entityID;
    gfp->input_itemID = ompcp->input_itemID;
    gfp->input_itemtype = ompcp->input_itemtype;
    gfp->this_itemtype = OBJ_SEQFEAT;
    gfp->this_subtype = FEATDEF_GENE;
    gfp->procid = ompcp->proc->procid;
    gfp->proctype = ompcp->proc->proctype;
    gfp->userkey = OMGetNextUserKey ();
    omudp = ObjMgrAddUserData (ompcp->input_entityID, ompcp->proc->procid,
                               OMPROC_EDIT, gfp->userkey);
    if (omudp != NULL) {
      omudp->userdata.ptrvalue = (Pointer) gfp;
      omudp->messagefunc = StdVibrantEditorMsgFunc;
    }
    SendMessageToForm (gfp->form, VIB_MSG_INIT);
    if (sfp != NULL) {
      PointerToForm (gfp->form, (Pointer) sfp);
      SetClosestParentIfDuplicating ((BaseFormPtr) gfp);
    } else {
      SetNewFeatureDefaultInterval ((FeatureFormPtr) gfp);
    }
  }
  Show (w);
  Select (w);
  helpfunc = (HelpMessageFunc) GetAppProperty ("HelpMessageProc");
  if (helpfunc != NULL) {
    helpfunc ("Features", "Gene");
  }
  return OM_MSG_RET_DONE;
}

typedef struct protpage {
  DIALOG_MESSAGE_BLOCK
  DialoG        name;
  TexT          desc;
  DialoG        ec;
  DialoG        activity;
  DialoG        db;
  PopuP         processed;
  GrouP         protGrp [4];
} ProtPage, PNTR ProtPagePtr;

typedef struct protform {
  FEATURE_FORM_BLOCK
  SeqEntryPtr   sep;
  GrouP         pages [NUM_PAGES];
  DialoG        foldertabs;
  Int2          currentPage;
  ButtoN        makemRNAMatch;
} ProtForm, PNTR ProtFormPtr;

static void ProtRefPtrToProtPage (DialoG d, Pointer data)

{
  ProtPagePtr  ppp;
  ProtRefPtr   prp;

  ppp = (ProtPagePtr) GetObjectExtra (d);
  prp = (ProtRefPtr) data;
  if (ppp != NULL) {
    if (prp != NULL) {
      PointerToDialog (ppp->name, prp->name);
      SafeSetTitle (ppp->desc, prp->desc);
      PointerToDialog (ppp->ec, prp->ec);
      PointerToDialog (ppp->activity, prp->activity);
      PointerToDialog (ppp->db, prp->db);
      SafeSetValue (ppp->processed, prp->processed + 1);
    } else {
      PointerToDialog (ppp->name, NULL);
      SafeSetTitle (ppp->desc, "");
      PointerToDialog (ppp->ec, NULL);
      PointerToDialog (ppp->activity, NULL);
      PointerToDialog (ppp->db, NULL);
      SafeSetValue (ppp->processed, 1);
    }
  }
}

static Pointer ProtPageToProtRefPtr (DialoG d)

{
  ProtPagePtr  ppp;
  ProtRefPtr   prp;

  prp = NULL;
  ppp = (ProtPagePtr) GetObjectExtra (d);
  if (ppp != NULL) {
    prp = ProtRefNew ();
    if (prp != NULL) {
      prp->name = DialogToPointer (ppp->name);
      prp->desc = SaveStringFromText (ppp->desc);
      prp->ec = DialogToPointer (ppp->ec);
      prp->activity = DialogToPointer (ppp->activity);
      prp->db = DialogToPointer (ppp->db);
      prp->processed = GetValue (ppp->processed) - 1;
    }
  }
  return (Pointer) prp;
}

static CharPtr protTabs [] = {
  "General", "E.C. Number", "Activity", NULL
};

static CharPtr protTabsXref [] = {
  "General", "E.C. Number", "Activity", "Cross-Refs", NULL
};

static void ChangeProtSubPage (VoidPtr data, Int2 newval, Int2 oldval)

{
  ProtPagePtr  ppp;

  ppp = (ProtPagePtr) data;
  if (ppp != NULL) {
    if (oldval >= 0 && oldval <= 3) {
      SafeHide (ppp->protGrp [oldval]);
    }
    if (newval >= 0 && newval <= 3) {
      SafeShow (ppp->protGrp [newval]);
    }
    Update ();
  }
}

static void ChangeProtProcessed (PopuP p)

{
  ProtFormPtr  pfp;
  Int2         val;

  pfp = (ProtFormPtr) GetObjectExtra (p);
  if (pfp == NULL) return;
  val = GetValue (p);
  switch (val) {
    case 1 :
      SafeHide (pfp->product);
      break;
    case 2 :
    case 3 :
    case 4 :
    case 5 :
      SafeShow (pfp->product);
      break;
    default :
      break;
  }
}

typedef struct switchtocds 
{
  SeqFeatPtr  sfp;
  ProtFormPtr pfp;
} SwitchToCDSData, PNTR SwitchToCDSPtr;

static void LaunchCDSEditorFromProtDialog (ButtoN b)
{
  SwitchToCDSPtr    scp;
  BioseqPtr         bsp;
  SeqFeatPtr        cds_sfp;
  SeqEntryPtr       sep;
  Uint2             entityID;
  SeqMgrFeatContext fcontext;
  CdRgnFormPtr      cfp;
  HelpMessageFunc   helpfunc;
  WindoW            w;
  
  scp = (SwitchToCDSPtr) GetObjectExtra (b);
  if (scp == NULL || scp->sfp == NULL || scp->pfp == NULL)
  {
      return;
  }
  
  bsp = BioseqFindFromSeqLoc (scp->sfp->location);
  if (bsp == NULL) return;
  cds_sfp = SeqMgrGetCDSgivenProduct (bsp, &fcontext);
  if (cds_sfp == NULL) return;
  bsp = BioseqFindFromSeqLoc (cds_sfp->location);
  if (bsp == NULL) return;
  entityID = ObjMgrGetEntityIDForPointer (bsp);
  
  sep = GetTopSeqEntryForEntityID (entityID);
  w = (WindoW) CreateCdRgnForm (-50, -33, "Coding Region", cds_sfp, sep,
                                CdRgnFeatFormActnProc);
  cfp = (CdRgnFormPtr) GetObjectExtra (w);
  if (cfp != NULL) {
    cfp->input_entityID = entityID;
    cfp->input_itemID = cds_sfp->idx.itemID;
    cfp->input_itemtype = cds_sfp->idx.itemtype;
    cfp->this_itemtype = OBJ_SEQFEAT;
    cfp->this_subtype = FEATDEF_CDS;
/*    cfp->procid = ompcp->proc->procid;
    cfp->proctype = ompcp->proc->proctype;
    cfp->userkey = OMGetNextUserKey ();
    omudp = ObjMgrAddUserData (ompcp->input_entityID, ompcp->proc->procid,
                               OMPROC_EDIT, cfp->userkey);
    if (omudp != NULL) {
      omudp->userdata.ptrvalue = (Pointer) cfp;
      omudp->messagefunc = StdVibrantEditorMsgFunc;
    } */
    SendMessageToForm (cfp->form, VIB_MSG_INIT);
    PointerToForm (cfp->form, (Pointer) cds_sfp);
    if (SetClosestParentIfDuplicating ((BaseFormPtr) cfp)) {
      SetBestProteinFeature (cfp, NULL);
      PointerToDialog (cfp->product, NULL);
      PointerToDialog (cfp->protseq, NULL);
      SetProteinLengthDisplay (cfp->protlen, 0);
    }
  }
  SendMessageToForm (scp->pfp->form, VIB_MSG_CLOSE);
  Show (w);
  Select (w);
  helpfunc = (HelpMessageFunc) GetAppProperty ("HelpMessageProc");
  if (helpfunc != NULL) {
    helpfunc ("Features", "CDS");
  }
}

static void CleanupCDSSwitchButton (GraphiC g, VoidPtr data)
{
  SwitchToCDSPtr    scp;

  scp = (SwitchToCDSPtr) data;
  if (scp == NULL) return;
  MemFree (scp);  
}

static DialoG CreateProtDialog (GrouP h, CharPtr title, ProtRefPtr prp, SeqFeatPtr sfp, ProtFormPtr pfp)

{
  GrouP        f;
  GrouP        g;
  Char         just;
  GrouP        k;
  GrouP        m;
  GrouP        p;
  ProtPagePtr  ppp;
  GrouP        q;
  GrouP        r;
  GrouP        s;
  Boolean      showXrefs;
  GrouP        t;
  DialoG       tbs;
  ButtoN       b;
  SwitchToCDSPtr    scp;

  p = HiddenGroup (h, 1, 0, NULL);
  SetGroupSpacing (p, 10, 10);

  ppp = (ProtPagePtr) MemNew (sizeof (ProtPage));
  if (ppp != NULL) {

    SetObjectExtra (p, ppp, StdCleanupExtraProc);
    ppp->dialog = (DialoG) p;
    ppp->todialog = ProtRefPtrToProtPage;
    ppp->fromdialog = ProtPageToProtRefPtr;
    ppp->testdialog = NULL;

    if (title != NULL && title [0] != '\0') {
      s = NormalGroup (p, 0, -2, title, systemFont, NULL);
    } else {
      s = HiddenGroup (p, 0, -2, NULL);
    }
    m = HiddenGroup (s, -1, 0, NULL);
    SetGroupSpacing (m, 10, 10);

    showXrefs = FALSE;
    if (prp != NULL && prp->db != NULL) {
      showXrefs = TRUE;
    }

    if (showXrefs) {
      tbs = CreateFolderTabs (m, protTabsXref, 0, 0, 0,
                              PROGRAM_FOLDER_TAB,
                              ChangeProtSubPage, (Pointer) ppp);
    } else {
      tbs = CreateFolderTabs (m, protTabs, 0, 0, 0,
                              PROGRAM_FOLDER_TAB,
                              ChangeProtSubPage, (Pointer) ppp);
    }
    k = HiddenGroup (m, 0, 0, NULL);

    ppp->protGrp [0] = HiddenGroup (k, -1, 0, NULL);
    g = HiddenGroup (ppp->protGrp [0], 0, 10, NULL);
    StaticPrompt (g, "Protein Names", 0, 0, programFont, 'c');
    ppp->name = CreateVisibleStringDialog (g, 3, -1, 25);

    pfp->makemRNAMatch = CheckBox (ppp->protGrp [0], "Make overlapping mRNA product match protein name", NULL);
    Hide (pfp->makemRNAMatch);
    f = HiddenGroup (ppp->protGrp [0], 0, 4, NULL);
    StaticPrompt (f, "Description", 0, dialogTextHeight, programFont, 'c');
    ppp->desc = DialogText (f, "", 25, NULL);

    r = HiddenGroup (ppp->protGrp [0], 2, 0, NULL);
    StaticPrompt (r, "Processing", 0, dialogTextHeight, programFont, 'l');
    ppp->processed = PopupList (r, TRUE, ChangeProtProcessed);
    SetObjectExtra (ppp->processed, (Pointer) pfp, NULL);
    PopupItem (ppp->processed, " ");
    PopupItem (ppp->processed, "Proprotein");
    PopupItem (ppp->processed, "Mature");
    PopupItem (ppp->processed, "Signal peptide");
    PopupItem (ppp->processed, "Transit peptide");
    SetValue (ppp->processed, 1);

    pfp->product = CreateProteinOrMRNAProductDialog (ppp->protGrp [0], NULL, "Processing Product", FALSE,
                                                     pfp->sep, NULL, NULL, (BaseFormPtr) pfp);
    if (sfp == NULL || sfp->product == NULL) {
      if (prp == NULL || prp->processed < 2) {
        SafeHide (pfp->product);
      }
    }
    
    /* button for switching to CDS editor */
    if (GetAppProperty ("InternalNcbiSequin") != NULL)
    {
      scp = (SwitchToCDSPtr) MemNew (sizeof (SwitchToCDSData));
      if (scp != NULL)
      {
        scp->sfp = sfp;
        scp->pfp = pfp;
        b = PushButton (ppp->protGrp [0], "Switch to CDS Feature Editor", LaunchCDSEditorFromProtDialog);
        SetObjectExtra (b, (Pointer) scp, CleanupCDSSwitchButton);    
      }
    }

    ppp->protGrp [1] = HiddenGroup (k, 0, 10, NULL);
    StaticPrompt (ppp->protGrp [1], "Enzyme Commission Number", 0, 0, programFont, 'c');
    ppp->ec = CreateVisibleStringDialog (ppp->protGrp [1], 3, -1, 15);
    Hide (ppp->protGrp [1]);

    ppp->protGrp [2] = HiddenGroup (k, 0, 10, NULL);
    StaticPrompt (ppp->protGrp [2], "Activity", 0, 0, programFont, 'c');
    ppp->activity = CreateVisibleStringDialog (ppp->protGrp [2], 3, -1, 25);
    Hide (ppp->protGrp [2]);

    ppp->protGrp [3] = HiddenGroup (k, -1, 0, NULL);
    if (showXrefs) {
      q = HiddenGroup (ppp->protGrp [3], -1, 0, NULL);
      if (GetAppProperty ("ReadOnlyDbTags") == NULL) {
        just = 'c';
      } else {
        just = 'l';
        StaticPrompt (q, "This page is read-only", 15 * stdCharWidth, 0, programFont, 'c');
      }
      t = HiddenGroup (q, 2, 0, NULL);
      StaticPrompt (t, "Database", 7 * stdCharWidth, 0, programFont, just);
      StaticPrompt (t, "Object ID", 8 * stdCharWidth, 0, programFont, just);
      ppp->db = CreateDbtagDialog (q, 3, -1, 7, 8);
    }
    Hide (ppp->protGrp [3]);

    AlignObjects (ALIGN_CENTER, (HANDLE) ppp->protGrp [0],
                  (HANDLE) ppp->protGrp [1], (HANDLE) ppp->protGrp [2],
                  (HANDLE) ppp->protGrp [3], (HANDLE) tbs,
                  (HANDLE) g, (HANDLE) r, NULL);
  }

  return (DialoG) p;
}

static void SetProtImportExportItems (ProtFormPtr pfp)

{
  IteM  exportItm;
  IteM  importItm;

  if (pfp != NULL) {
    importItm = FindFormMenuItem ((BaseFormPtr) pfp, VIB_MSG_IMPORT);
    exportItm = FindFormMenuItem ((BaseFormPtr) pfp, VIB_MSG_EXPORT);
    switch (pfp->currentPage) {
      case PROT_PAGE :
        SafeSetTitle (importItm, "Import...");
        SafeSetTitle (exportItm, "Export...");
        SafeDisable (importItm);
        SafeDisable (exportItm);
        break;
      case COMMON_PAGE :
        SafeSetTitle (importItm, "Import...");
        SafeSetTitle (exportItm, "Export...");
        SafeDisable (importItm);
        SafeDisable (exportItm);
        break;
      case LOCATION_PAGE :
        SafeSetTitle (importItm, "Import SeqLoc...");
        SafeSetTitle (exportItm, "Export SeqLoc...");
        SafeEnable (importItm);
        SafeEnable (exportItm);
        break;
      default :
        break;
    }
  }
}

static void ChangeProtPage (VoidPtr data, Int2 newval, Int2 oldval)

{
  ProtFormPtr   pfp;

  pfp = (ProtFormPtr) data;
  if (pfp != NULL) {
    pfp->currentPage = newval;
    SafeHide (pfp->pages [oldval]);
    SafeShow (pfp->pages [newval]);
    switch (newval) {
      case PROT_PAGE :
        SendMessageToDialog (pfp->data, VIB_MSG_ENTER);
        break;
      case COMMON_PAGE :
        break;
      case LOCATION_PAGE :
        SendMessageToDialog (pfp->location, VIB_MSG_ENTER);
        break;
      default :
        break;
    }
    SetProtImportExportItems (pfp);
    Update ();
  }
}

static Boolean ImportProtForm (ForM f, CharPtr filename)

{
  ProtFormPtr  pfp;

  pfp = (ProtFormPtr) GetObjectExtra (f);
  if (pfp != NULL) {
    switch (pfp->currentPage) {
      case LOCATION_PAGE :
        return ImportDialog (pfp->location, filename);
      default :
        break;
    }
  }
  return FALSE;
}

static Boolean ExportProtForm (ForM f, CharPtr filename)

{
  ProtFormPtr  pfp;

  pfp = (ProtFormPtr) GetObjectExtra (f);
  if (pfp != NULL) {
    switch (pfp->currentPage) {
      case LOCATION_PAGE :
        return ExportDialog (pfp->location, filename);
      default :
        break;
    }
  }
  return FALSE;
}

static CharPtr  protFormTabs [] = {
  "Protein", "Properties", "Location", NULL
};

static void ProtFormMessage (ForM f, Int2 mssg)

{
  ProtFormPtr  pfp;

  pfp = (ProtFormPtr) GetObjectExtra (f);
  if (pfp != NULL) {
    switch (mssg) {
      case VIB_MSG_INIT :
        StdInitFeatFormProc (f);
        break;
      case VIB_MSG_IMPORT :
        ImportProtForm (f, NULL);
        break;
      case VIB_MSG_EXPORT :
        ExportProtForm (f, NULL);
        break;
      case VIB_MSG_CLOSE :
        Remove (f);
        break;
      case VIB_MSG_CUT :
        StdCutTextProc (NULL);
        break;
      case VIB_MSG_COPY :
        StdCopyTextProc (NULL);
        break;
      case VIB_MSG_PASTE :
        StdPasteTextProc (NULL);
        break;
      case VIB_MSG_DELETE :
        if (pfp->currentPage == LOCATION_PAGE) {
          PointerToDialog (pfp->location, NULL);
        } else {
          StdDeleteTextProc (NULL);
        }
        break;
      default :
        if (pfp->appmessage != NULL) {
          pfp->appmessage (f, mssg);
        }
        break;
    }
  }
}

static void ProtFormActivate (WindoW w)

{
  ProtFormPtr  pfp;

  pfp = (ProtFormPtr) GetObjectExtra (w);
  if (pfp != NULL) {
    if (pfp->activate != NULL) {
      pfp->activate (w);
    }
    SetProtImportExportItems (pfp);
  }
}

static void ShowHideMakemRNAMatch (ProtFormPtr pfp, SeqFeatPtr sfp)
{
  BioseqPtr  bsp;
  SeqFeatPtr cds, mrna;
  SeqMgrFeatContext fcontext;
  if (pfp == NULL) return;

  if (sfp == NULL || sfp->data.choice != SEQFEAT_PROT) {
    Hide (pfp->makemRNAMatch);
  } else {
    /* look for overlapping mRNA */
    bsp = BioseqFindFromSeqLoc (sfp->location);
    cds = SeqMgrGetCDSgivenProduct (bsp, &fcontext);
    if (cds == NULL) {
      Hide (pfp->makemRNAMatch);
    } else {
      mrna = SeqMgrGetOverlappingmRNA (cds->location, &fcontext);
      if (mrna == NULL) {
        Hide (pfp->makemRNAMatch);
      } else {
        Show (pfp->makemRNAMatch);
      }
    }
  }
}


static void ProtRefPtrToForm (ForM f, Pointer data)

{
  SeqEntryPtr  oldsep;
  ProtFormPtr  pfp;
  SeqEntryPtr  sep;
  SeqFeatPtr   sfp;
  Int4         val;

  pfp = (ProtFormPtr) GetObjectExtra (f);
  if (pfp != NULL) {
    sep = GetTopSeqEntryForEntityID (pfp->input_entityID);
    oldsep = SeqEntrySetScope (sep);
    sfp = (SeqFeatPtr) data;
    if (sfp != NULL) {
      switch (sfp->data.choice) {
        case SEQFEAT_BOND :
        case SEQFEAT_SITE :
        case SEQFEAT_PSEC_STR :
          val = (Int4) sfp->data.value.intvalue;
          PointerToDialog (pfp->data, (Pointer) &(val));
          break;
        case SEQFEAT_COMMENT:
          break;
        default :
          PointerToDialog (pfp->data, sfp->data.value.ptrvalue);
          break;
      }
      SeqFeatPtrToCommon ((FeatureFormPtr) pfp, sfp);
      PointerToDialog (pfp->location, sfp->location);
      PointerToDialog (pfp->product, sfp->product);
    }
    ShowHideMakemRNAMatch (pfp, sfp);
    SeqEntrySetScope (oldsep);
  }
}

extern ForM CreateProtForm (Int2 left, Int2 top, CharPtr title,
                            SeqFeatPtr sfp, SeqEntryPtr sep,
                            FormActnFunc actproc)

{
  ButtoN             b;
  GrouP              c;
  GrouP              g;
  GrouP              h;
  ProtFormPtr        pfp;
  ProtRefPtr         prp;
  GrouP              s;
  StdEditorProcsPtr  sepp;
  WindoW             w;

  w = NULL;
  pfp = (ProtFormPtr) MemNew (sizeof (ProtForm));
  if (pfp != NULL) {
    w = FixedWindow (left, top, -10, -10, title, StdCloseWindowProc);
    SetObjectExtra (w, pfp, StdFeatFormCleanupProc);
    pfp->form = (ForM) w;
    pfp->actproc = actproc;
    pfp->toform = ProtRefPtrToForm;
    pfp->fromform = NULL;
    pfp->formmessage = ProtFormMessage;
    pfp->testform = NULL;
    pfp->importform = ImportProtForm;
    pfp->exportform = ExportProtForm;

#ifndef WIN_MAC
    CreateStdEditorFormMenus (w);
#endif

    pfp->activate = NULL;
    sepp = (StdEditorProcsPtr) GetAppProperty ("StdEditorForm");
    if (sepp != NULL) {
      pfp->activate = sepp->activateForm;
      pfp->appmessage = sepp->handleMessages;
    }
    SetActivate (w, ProtFormActivate);

    g = HiddenGroup (w, -1, 0, NULL);
    SetGroupSpacing (g, 3, 10);

    pfp->sep = sep;
    pfp->foldertabs = CreateFolderTabs (g, protFormTabs, PROT_PAGE,
                                        0, 0, SYSTEM_FOLDER_TAB,
                                        ChangeProtPage, (Pointer) pfp);
    pfp->currentPage = PROT_PAGE;

    h = HiddenGroup (g, 0, 0, NULL);

    s = HiddenGroup (h, -1, 0, NULL);
    prp = NULL;
    if (sfp != NULL && sfp->data.choice == SEQFEAT_PROT) {
      prp = sfp->data.value.ptrvalue;
    }
    pfp->data = CreateProtDialog (s, NULL, prp, sfp, pfp);
    pfp->pages [PROT_PAGE] = s;
    Hide (pfp->pages [PROT_PAGE]);

    s = HiddenGroup (h, -1, 0, NULL);
    CreateCommonFeatureGroupEx (s, (FeatureFormPtr) pfp, sfp, FALSE, TRUE, FALSE);
    pfp->pages [COMMON_PAGE] = s;
    Hide (pfp->pages [COMMON_PAGE]);

    s = HiddenGroup (h, -1, 0, NULL);
    pfp->location = CreateIntervalEditorDialogEx (s, NULL, 4, 2, sep, FALSE, TRUE,
                                                  TRUE, TRUE, FALSE,
                                                  (FeatureFormPtr) pfp,
                                                  StdFeatIntEdPartialCallback);
    pfp->pages [LOCATION_PAGE] = s;
    Hide (pfp->pages [LOCATION_PAGE]);

    AlignObjects (ALIGN_CENTER, (HANDLE) pfp->pages [PROT_PAGE],
                  (HANDLE) pfp->pages [COMMON_PAGE], (HANDLE) pfp->pages [LOCATION_PAGE],
                  NULL);
    AlignObjects (ALIGN_CENTER, (HANDLE) pfp->foldertabs, (HANDLE) h, NULL);

    c = HiddenGroup (w, 2, 0, NULL);
    b = PushButton (c, "Accept", StdFeatFormAcceptButtonProc);
    SetObjectExtra (b, pfp, NULL);
    PushButton (c, "Cancel", StdCancelButtonProc);
    AlignObjects (ALIGN_CENTER, (HANDLE) g, (HANDLE) c, NULL);
    RealizeWindow (w);

    SendMessageToDialog (pfp->data, VIB_MSG_INIT);
    SendMessageToDialog (pfp->location, VIB_MSG_INIT);
    Show (pfp->pages [pfp->currentPage]);
    SendMessageToDialog (pfp->data, VIB_MSG_ENTER);
    Update ();
  }
  return (ForM) w;
}


static void MakemRNAProductMatchProteinName (SeqFeatPtr prot)
{
  BioseqPtr    bsp;
  ProtRefPtr   prp;
  SeqFeatPtr   cds, mrna;
  RnaRefPtr    rrp;
  SeqMgrFeatContext fcontext;

  if (prot == NULL || prot->data.choice != SEQFEAT_PROT) return;
  bsp = BioseqFindFromSeqLoc (prot->location);
  if (bsp == NULL) return;

  cds = SeqMgrGetCDSgivenProduct (bsp, &fcontext);
  if (cds != NULL) {
    mrna = SeqMgrGetOverlappingmRNA (cds->location, &fcontext);      
    if (mrna != NULL && mrna->idx.subtype == FEATDEF_mRNA) {
      rrp = (RnaRefPtr) mrna->data.value.ptrvalue;
      if (rrp == NULL) {
        rrp = RnaRefNew();
        mrna->data.value.ptrvalue = rrp;
      }
      rrp->ext.value.ptrvalue = MemFree (rrp->ext.value.ptrvalue);
      prp = (ProtRefPtr) prot->data.value.ptrvalue;
      if (prp == NULL || prp->name == NULL || StringHasNoText (prp->name->data.ptrvalue)) {
        rrp->ext.choice = 0;
      } else {
        rrp->ext.choice = 1;
        rrp->ext.value.ptrvalue = StringSave (prp->name->data.ptrvalue);
      }
    }
  }
}


static void ProtFeatFormActnProc (ForM f)

{
  BioseqPtr    bsp;
  ProtFormPtr  pfp;
  SeqFeatPtr   sfp;
  Boolean      need_update = FALSE;

  StdFeatFormActnProc (f);
  pfp = (ProtFormPtr) GetObjectExtra (f);
  if (pfp == NULL) return;
  
  sfp = SeqMgrGetDesiredFeature (pfp->input_entityID, NULL,
                                 pfp->input_itemID, 0, NULL, NULL);
  if (sfp == NULL) return;

  bsp = BioseqFindFromSeqLoc (sfp->location);
  if (bsp == NULL) return;

  if (GetStatus (pfp->makemRNAMatch)) {    
    MakemRNAProductMatchProteinName (sfp);
    need_update = TRUE;
  }

  if (SeeIfProtTitleNeedsFixing (bsp, pfp->input_entityID)) {
    need_update = TRUE;
  }
  if (need_update) {
    ObjMgrSendMsg (OM_MSG_UPDATE, pfp->input_entityID,
                   pfp->input_itemID, pfp->input_itemtype);
  }  
}

extern Int2 LIBCALLBACK ProtGenFunc (Pointer data)

{
  ProtFormPtr       pfp;
  HelpMessageFunc   helpfunc;
  ObjMgrPtr         omp;
  OMProcControlPtr  ompcp;
  ObjMgrTypePtr     omtp;
  OMUserDataPtr     omudp;
  ProtPagePtr       ppp;
  ObjMgrProcPtr     proc;
  SeqEntryPtr       sep;
  SeqFeatPtr        sfp;
  Uint2             subtype;
  WindoW            w;

  ompcp = (OMProcControlPtr) data;
  sfp = NULL;
  sep = NULL;
  subtype = FEATDEF_PROT;
  if (ompcp == NULL || ompcp->proc == NULL) return OM_MSG_RET_ERROR;
  proc = ompcp->proc;
  switch (ompcp->input_itemtype) {
    case OBJ_SEQFEAT :
      sfp = (SeqFeatPtr) ompcp->input_data;
      if (sfp != NULL && sfp->data.choice != SEQFEAT_PROT) {
        return OM_MSG_RET_ERROR;
      }
      break;
    case OBJ_BIOSEQ :
      break;
    case OBJ_BIOSEQSET :
      break;
    case 0 :
      break;
    default :
      return OM_MSG_RET_ERROR;
  }
  omudp = ItemAlreadyHasEditor (ompcp->input_entityID, ompcp->input_itemID,
                                ompcp->input_itemtype, ompcp->proc->procid);
  if (omudp != NULL) {
    pfp = (ProtFormPtr) omudp->userdata.ptrvalue;
    if (pfp != NULL) {
      Select (pfp->form);
    }
    return OM_MSG_RET_DONE;
  }
  if (sfp == NULL && proc->subinputtype > 0) {
    subtype = proc->subinputtype;
  }
  sep = GetTopSeqEntryForEntityID (ompcp->input_entityID);
  w = (WindoW) CreateProtForm (-50, -33, "Protein", sfp, sep,
                               ProtFeatFormActnProc);
  pfp = (ProtFormPtr) GetObjectExtra (w);
  if (pfp != NULL) {
    pfp->input_entityID = ompcp->input_entityID;
    pfp->input_itemID = ompcp->input_itemID;
    pfp->input_itemtype = ompcp->input_itemtype;
    pfp->this_itemtype = OBJ_SEQFEAT;
    pfp->this_subtype = subtype;
    pfp->procid = ompcp->proc->procid;
    pfp->proctype = ompcp->proc->proctype;
    pfp->userkey = OMGetNextUserKey ();
    omudp = ObjMgrAddUserData (ompcp->input_entityID, ompcp->proc->procid,
                               OMPROC_EDIT, pfp->userkey);
    if (omudp != NULL) {
      omudp->userdata.ptrvalue = (Pointer) pfp;
      omudp->messagefunc = StdVibrantEditorMsgFunc;
    }
    if (sfp != NULL) {
      omp = ObjMgrGet ();
      if (omp != NULL) {
        omtp = ObjMgrTypeFind (omp, OBJ_SEQFEAT, NULL, NULL);
        if (omtp != NULL && omtp->subtypefunc != NULL) {
          pfp->this_subtype = (*(omtp->subtypefunc)) (sfp);
        }
      }
    }
    SendMessageToForm (pfp->form, VIB_MSG_INIT);
    if (sfp != NULL) {
      PointerToForm (pfp->form, (Pointer) sfp);
      SetClosestParentIfDuplicating ((BaseFormPtr) pfp);
    } else {
      ppp = (ProtPagePtr) GetObjectExtra (pfp->data);
      if (ppp != NULL) {
        if (subtype == FEATDEF_PROT) {
          SetValue (ppp->processed, 1);
        } else if (subtype >= FEATDEF_preprotein && subtype <= FEATDEF_transit_peptide_aa) {
          SetValue (ppp->processed, subtype - FEATDEF_preprotein + 2);
        } else {
          SetValue (ppp->processed, 1);
        }
      }
      SetNewFeatureDefaultInterval ((FeatureFormPtr) pfp);
    }
  }
  Show (w);
  Select (w);
  helpfunc = (HelpMessageFunc) GetAppProperty ("HelpMessageProc");
  if (helpfunc != NULL) {
    helpfunc ("Features", "Protein");
  }
  return OM_MSG_RET_DONE;
}

/* rnaform and rnapage moved to top */

static ENUM_ALIST(rna_type_alist)
  {" ",            0},
  {"preRna",       1},
  {"mRNA",         2},
  {"tRNA",         3},
  {"rRNA",         4},
  {"ncRNA",        8},
  {"tmRNA",        9},
  {"misc_RNA",     10},
  {"otherRNA",    255},
END_ENUM_ALIST

static Uint1 check_rna_type (Uint1 type)

{
  if (type > 10 && type != 255) return 0;
  return type;
}

typedef struct mrnauserpage {
  DIALOG_MESSAGE_BLOCK
  DialoG        protein;
} MrnaUserData, PNTR MrnaUserPtr;

static void UserObjectPtrToProtein (DialoG d, Pointer data)

{
  MrnaUserPtr    mup;
  ObjectIdPtr    oip;
  SeqIdPtr       sip;
  CharPtr        str;
  UserFieldPtr   ufp;
  UserObjectPtr  uop;
  ValNode        vn;

  mup = (MrnaUserPtr) GetObjectExtra (d);
  /* uop = (UserObjectPtr) data; */
  if (mup != NULL) {
    uop = FindUopByTag ((UserObjectPtr) data, "MrnaProteinLink");
    if (uop != NULL && uop->type != NULL && StringICmp (uop->type->str, "MrnaProteinLink") == 0) {
      ufp = uop->data;
      if (ufp != NULL && ufp->choice == 1) {
        oip = ufp->label;
        if (oip != NULL && oip->str != NULL && StringICmp (oip->str, "protein seqID") == 0) {
          str = (CharPtr) ufp->data.ptrvalue;
          if (str != NULL) {
            sip = MakeSeqID (str);
            if (sip != NULL) {
              vn.choice = SEQLOC_WHOLE;
              vn.data.ptrvalue = (Pointer) sip;
              PointerToDialog (mup->protein, (Pointer) (&vn));
              SeqIdFree (sip);
              return;
            }
          }
        }
      }
    }
    PointerToDialog (mup->protein, NULL);
  }
}

static Pointer ProteinToUserObjectPtr (DialoG d)

{
  BioseqPtr      bsp;
  MrnaUserPtr    mup;
  SeqIdPtr       sip;
  SeqLocPtr      slp;
  UserObjectPtr  uop;

  uop = NULL;
  mup = (MrnaUserPtr) GetObjectExtra (d);
  if (mup != NULL) {
    slp = DialogToPointer (mup->protein);
    if (slp != NULL) {
      sip = SeqLocId (slp);
      if (sip != NULL) {
        bsp = BioseqFind (sip);
        if (bsp != NULL) {
          uop = CreateMrnaProteinLinkUserObject (bsp);
        }
      }
      SeqLocFree (slp);
    }
  }
  return (Pointer) uop;
}

static DialoG CreateMrnaUserObjectDialog (GrouP h, CharPtr label, SeqEntryPtr sep, BaseFormPtr bfp)

{
  MrnaUserPtr  mup;
  GrouP        p;

  p = HiddenGroup (h, -1, 0, NULL);
  SetGroupSpacing (p, 10, 10);

  mup = (MrnaUserPtr) MemNew (sizeof (MrnaUserData));
  if (mup != NULL) {
    SetObjectExtra (p, mup, StdCleanupExtraProc);
    mup->dialog = (DialoG) p;
    mup->todialog = UserObjectPtrToProtein;
    mup->fromdialog = ProteinToUserObjectPtr;
    mup->testdialog = NULL;
    mup->protein = CreateProteinOrMRNAProductDialog (p, NULL, label, FALSE, sep, NULL, NULL, bfp);
  }

  return (DialoG) p;
}


static Uint1 RnaFeatDefFromRnaRef (RnaRefPtr rrp)
{
  if (rrp == NULL) {
    return FEATDEF_otherRNA;
  }

  switch (rrp->type) 
  {
    case 1:
      return FEATDEF_preRNA;
      break;
    case 2:
      return FEATDEF_mRNA;
      break;
    case 3:
      return FEATDEF_tRNA;
      break;
    case 4:
      return FEATDEF_rRNA;
      break;
    case 5:
      return FEATDEF_snRNA;
      break;
    case 6:
      return FEATDEF_scRNA;
      break;
    case 7:
      return FEATDEF_snoRNA;
      break;
    case 8:
      return FEATDEF_ncRNA;
      break;
    case 9:
      return FEATDEF_tmRNA;
      break;
    case 10:
      return FEATDEF_misc_RNA;
      break;

    case 255:
    default:
      return FEATDEF_otherRNA;
      break;
  }
}


static void RnaRefPtrToRnaPage (DialoG d, Pointer data)

{
  Uint1           aa;
  Uint1           codon [4];
  Uint1           from;
  ValNodePtr      head;
  Uint1           i;
  Int2            j;
  RnaPagePtr      rpp;
  RnaRefPtr       rrp;
  Uint1           shift;
  SeqMapTablePtr  smtp;
  Char            str [8];
  tRNAPtr         trna;
  ValNodePtr      vnp;
  RNAGenPtr       rgp;
  CharPtr         tag_peptide;
  CharPtr         product;

  rpp = (RnaPagePtr) GetObjectExtra (d);
  rrp = (RnaRefPtr) data;

  if (rrp != NULL) {

    SetEnumPopup (rpp->type, rna_type_alist, rrp->type);
    SafeSetStatus (rpp->pseudo, rrp->pseudo);
    switch (rrp->type) {
      case 0 :
        SafeHide (rpp->trnaGrp);
        SafeHide (rpp->nameGrp);
        SafeHide (rpp->rrnaPrompt);
        SafeHide (rpp->ornaPrompt);
        SafeHide (rpp->ncrnaGrp);
        SafeHide (rpp->tmrnaGrp);
        break;
      case 3 :
        SafeSetTitle (rpp->name, "");
        SafeHide (rpp->nameGrp);
        SafeHide (rpp->rrnaPrompt);
        SafeHide (rpp->ornaPrompt);
        SafeHide (rpp->ncrnaGrp);
        SafeHide (rpp->tmrnaGrp);
        SafeSetValue (rpp->AAitem, 1);
        if (rrp->ext.choice == 2) {
          trna = rrp->ext.value.ptrvalue;
          if (trna != NULL) {
            aa = 0;
            if (trna->aatype == 2) {
              aa = trna->aa;
            } else {
              from = 0;
              switch (trna->aatype) {
                case 0 :
                  from = 0;
                  break;
                case 1 :
                  from = Seq_code_iupacaa;
                  break;
                case 2 :
                  from = Seq_code_ncbieaa;
                  break;
                case 3 :
                  from = Seq_code_ncbi8aa;
                  break;
                case 4 :
                  from = Seq_code_ncbistdaa;
                  break;
                default:
                  break;
              }
              smtp = SeqMapTableFind (Seq_code_ncbieaa, from);
              if (smtp != NULL) {
                aa = SeqMapTableConvert (smtp, trna->aa);
              }
            }
            if (aa > 0 && aa != 255) {
              /*
              if (aa <= 74) {
                shift = 0;
              } else if (aa > 79) {
                shift = 2;
              } else {
                shift = 1;
              }
              */
              shift = 0;
              if (aa != '*') {
                i = aa - (64 + shift);
              } else {
                i = 25;
              }
              SetValue (rpp->AAitem, (Int2) i + 1);
            }
            head = NULL;
            for (j = 0; j < 6; j++) {
              if (trna->codon [j] < 64) {
                /* Note - it is important to set the fourth character in the codon array to NULL
                 * because CodonForIndex only fills in the three characters of actual codon,
                 * so if you StringCpy the codon array and the NULL character is not found after
                 * the three codon characters, you will write in memory you did not intend to.
                 */
                codon [3] = 0;
                if (CodonForIndex (trna->codon [j], Seq_code_iupacna, codon)) {
                  StringCpy (str, (CharPtr) codon);
                  str [3] = '\0';
                  vnp = ValNodeNew (head);
                  if (head == NULL) {
                    head = vnp;
                  }
                  if (vnp != NULL) {
                    vnp->data.ptrvalue = StringSave (str);
                  }
                }
              }
            }
            if (head != NULL) {
              PointerToDialog (rpp->codons, head);
              ValNodeFreeData (head);
            }
            if (trna->anticodon == NULL)
            {
              /* attempt to provide default location */
              SeqLocPtr tmp_loc = NULL;
              tmp_loc = DialogToPointer (rpp->rfp->location);
              PointerToDialog (rpp->anticodon, tmp_loc);
              SetSequenceAndStrandForIntervalPage (rpp->anticodon);
              SeqLocFree (tmp_loc);              
            }
            else
            {
              PointerToDialog (rpp->anticodon, trna->anticodon);                
            }
          }
        }
        SafeShow (rpp->trnaGrp);
        break;
      case 4 :
        SafeHide (rpp->trnaGrp);
        if (rrp->ext.choice == 1 && rrp->ext.value.ptrvalue != NULL) {
          SafeSetTitle (rpp->name, (CharPtr) rrp->ext.value.ptrvalue);
        } else {
          SafeSetTitle (rpp->name, "");
        }
        SafeHide (rpp->ornaPrompt);
        SafeShow (rpp->rrnaPrompt);
        SafeShow (rpp->nameGrp);
        SafeHide (rpp->ncrnaGrp);
        SafeHide (rpp->tmrnaGrp);
        break;
      case 5:
      case 6:
      case 7:
      case 8:
        SafeHide (rpp->trnaGrp);
        SafeHide (rpp->nameGrp);
        SafeHide (rpp->rrnaPrompt);
        SafeHide (rpp->ornaPrompt);
        if (rrp->type == 5) {
          PointerToDialog (rpp->ncrnaClass, "snRNA");
        } else if (rrp->type == 6) {
          PointerToDialog (rpp->ncrnaClass, "scRNA");
        } else if (rrp->type == 7) {
          PointerToDialog (rpp->ncrnaClass, "snoRNA");
        }
        SafeHide (rpp->tmrnaGrp);
        SafeShow (rpp->ncrnaGrp);
        product = GetRNARefProductString (rrp, NULL);
        SetTitle (rpp->ncrnaProduct, product);
        product = MemFree (product);
        if (rrp->ext.choice == 3 && (rgp = (RNAGenPtr) rrp->ext.value.ptrvalue) != NULL 
            && rrp->type == 8 && !StringHasNoText (rgp->_class)) {
          PointerToDialog (rpp->ncrnaClass, rgp->_class);
        }
        break;
      case 9:
        SafeHide (rpp->trnaGrp);
        SafeHide (rpp->ornaPrompt);
        SafeHide (rpp->rrnaPrompt);
        SafeHide (rpp->nameGrp);
        SafeHide (rpp->ncrnaGrp);
        SafeShow (rpp->tmrnaGrp);
        product = GetRNARefProductString (rrp, NULL);
        SetTitle (rpp->tmrnaProduct, product);
        product = MemFree (product);
        if (rrp->ext.choice == 3 && (rgp = (RNAGenPtr) rrp->ext.value.ptrvalue) != NULL) {
          tag_peptide = GettmRNATagPeptide (rrp, NULL);
          if (tag_peptide != NULL) {
            PointerToDialog (rpp->tmrnaTagPeptide, tag_peptide);
            tag_peptide = MemFree (tag_peptide);
          }
        }
        break;
      case 10 :
        SafeHide (rpp->trnaGrp);
        SafeShow (rpp->nameGrp);
        SafeShow (rpp->rrnaPrompt);
        SafeHide (rpp->ncrnaGrp);
        SafeHide (rpp->tmrnaGrp);
        SafeHide (rpp->ornaPrompt);
        product = GetRNARefProductString (rrp, NULL);
        SetTitle (rpp->name, product);
        product = MemFree (product);
        break;
      default :
        SafeHide (rpp->trnaGrp);
        product = GetRNARefProductString (rrp, NULL);
        SetTitle (rpp->name, product);
        product = MemFree (product);
        SafeHide (rpp->rrnaPrompt);
        SafeHide (rpp->ornaPrompt);
        SafeShow (rpp->nameGrp);
        SafeHide (rpp->ncrnaGrp);
        SafeHide (rpp->tmrnaGrp);
        break;
    }
  } else {
    SetEnumPopup (rpp->type, rna_type_alist, (UIEnum) 0);
    SafeSetStatus (rpp->pseudo, FALSE);
    SafeSetTitle (rpp->name, "");
    SafeHide (rpp->nameGrp);
    SafeHide (rpp->rrnaPrompt);
    SafeHide (rpp->ornaPrompt);
    SafeSetValue (rpp->AAitem, 0);
    SafeHide (rpp->trnaGrp);
    SafeHide (rpp->ncrnaGrp);
    SafeHide (rpp->tmrnaGrp);
  }
}

static Pointer RnaPageToRnaRefPtr (DialoG d)

{
  Char        ch;
  Uint1       code;
  Uint1       codon [4];
  Boolean     degenerate;
  ValNodePtr  head;
  Int2        i;
  Int2        j;
  Int2        k;
  Int2        q;
  RnaPagePtr  rpp;
  RnaRefPtr   rrp;
  Uint1       shift;
  Char        str [8];
  tRNAPtr     trna;
  UIEnum      val;
  ValNodePtr  vnp;
  RNAGenPtr   rgp;
  CharPtr     tmp;

  rrp = NULL;
  rpp = (RnaPagePtr) GetObjectExtra (d);
  if (rpp != NULL) {
    rrp = RnaRefNew ();
    if (rrp != NULL) {
      if (GetEnumPopup (rpp->type, rna_type_alist, &val)) {
        rrp->type = (Uint1) val;
      }
      rrp->pseudo = GetStatus (rpp->pseudo);
      switch (val) {
        case 3 :
          rrp->ext.choice = 2;
          trna = (tRNAPtr) MemNew (sizeof (tRNA));
          rrp->ext.value.ptrvalue = (Pointer) trna;
          if (trna != NULL) {
            i = GetValue (rpp->AAitem) - 1;
            if (i > 0) {
              trna->aatype = 2;
              /*
              if (i < 10) {
                shift = 0;
              } else if (i > 13) {
                shift = 2;
              } else {
                shift = 1;
              }
              */
              shift = 0;
              trna->aa = (Uint1) i + 64 + shift;
              if (trna->aa == 91) {
                trna->aa = (Uint1) '*';
              }
            }
            for (j = 0; j < 6; j++) {
              trna->codon [j] = 255;
            }
            head = (ValNodePtr) DialogToPointer (rpp->codons);
            if (head != NULL) {
              for (vnp = head, j = 0; vnp != NULL && j < 6; vnp = vnp->next) {
                str [0] = '\0';
                StringNCpy_0 (str, (CharPtr) vnp->data.ptrvalue, sizeof (str));
                if (str [0] != '\0') {
                  degenerate = FALSE;
                  k = 0;
                  q = 0;
                  ch = str [k];
                  while (ch != '\0' && q < 3) {
                    ch = TO_UPPER (ch);
                    if (ch == 'U') {
                      ch = 'T';
                    }
                    if (StringChr ("ACGT", ch) == NULL) {
                      degenerate = TRUE;
                    }
                    codon [q] = (Uint1) ch;
                    q++;
                    k++;
                    ch = str [k];
                  }
                  codon [q] = 0;
                  if (q == 3) {
                    if (degenerate) {
                      ParseDegenerateCodon (trna, codon);
                    } else {
                      code = IndexForCodon (codon, Seq_code_iupacna);
                      if (code != INVALID_RESIDUE) {
                        trna->codon [j] = code;
                      }
                    }
                  }
                  j++;
                }
              }
              ValNodeFreeData (head);
            }
            trna->anticodon = DialogToPointer (rpp->anticodon);
          }
          break;
        case 5:
        case 6:
        case 7:
        case 8: /* ncRNA */
          rgp = RNAGenNew ();
          rgp->_class = DialogToPointer (rpp->ncrnaClass);
          rgp->product = SaveStringFromText (rpp->ncrnaProduct);
          rrp->ext.choice = 3;
          rrp->ext.value.ptrvalue = rgp;
          break;
        case 9: /* tmRNA */
          rgp = RNAGenNew ();
          rrp->ext.choice = 3;
          rrp->ext.value.ptrvalue = rgp;
          if (!TextHasNoText (rpp->tmrnaProduct)) {
            rgp->product = SaveStringFromText (rpp->tmrnaProduct);
          }
          tmp = DialogToPointer (rpp->tmrnaTagPeptide);
          if (!StringHasNoText (tmp)) {
            SettmRNATagPeptide (rrp, NULL, tmp, ExistingTextOption_replace_old);
          }
          tmp = MemFree (tmp);
          break;
        case 10:
          rgp = RNAGenNew ();
          rgp->product = SaveStringFromText (rpp->name);
          rrp->ext.choice = 3;
          rrp->ext.value.ptrvalue = rgp;
          break;
        case 255:
        default :
          if (! TextHasNoText (rpp->name)) {
            rrp->ext.choice = 1;
            rrp->ext.value.ptrvalue = SaveStringFromText (rpp->name);
          }
          break;
      }
    }
  }
  return (Pointer) rrp;
}


static GBQualPtr FindOrAddQual (SeqFeatPtr sfp, CharPtr qual_name)
{
  GBQualPtr gbq;

  if (sfp == NULL) return NULL;
  gbq = sfp->qual;
  while (gbq != NULL && StringCmp (gbq->qual, qual_name) != 0) {
    gbq = gbq->next;
  }
  if (gbq == NULL) {
    gbq = GBQualNew ();
    gbq->qual = StringSave (qual_name);
    gbq->next = sfp->qual;
    sfp->qual = gbq;
  }
  return gbq;
}


static void RemoveQualByName (SeqFeatPtr sfp, CharPtr qual_name)
{
  GBQualPtr gbq, gbq_next, gbq_prev = NULL;

  if (sfp == NULL || StringHasNoText (qual_name))
  {
    return;
  }
  gbq = sfp->qual;
  while (gbq != NULL)
  {
    gbq_next = gbq->next;
    if (StringCmp (gbq->qual, qual_name) == 0)
    {
      if (gbq_prev == NULL)
      {
        sfp->qual = gbq->next;
      }
      else
      {
        gbq_prev->next = gbq->next;
      }
      gbq->next = NULL;
      gbq = GBQualFree (gbq);
    }
    else
    {
      gbq_prev = gbq;
    }
    gbq = gbq_next;
  }
}

extern void ConvertProductQualToRnaRefName (SeqFeatPtr sfp)
{
  RnaRefPtr rrp;
  GBQualPtr gbq, gbq_prev = NULL;

  if (sfp == NULL || sfp->data.choice != SEQFEAT_RNA || sfp->data.value.ptrvalue == NULL) return;
  rrp = (RnaRefPtr) sfp->data.value.ptrvalue;

  gbq = sfp->qual;
  while (gbq != NULL && StringCmp (gbq->qual, "product") != 0) {
    gbq_prev = gbq;
    gbq = gbq->next;
  }
  if (gbq != NULL) {
    rrp->ext.value.ptrvalue = MemFree (rrp->ext.value.ptrvalue);
    rrp->ext.choice = 1;
    rrp->ext.value.ptrvalue = gbq->val;
    gbq->val = NULL;
    if (gbq_prev == NULL) {
      sfp->qual = gbq->next;
    } else {
      gbq_prev->next = gbq->next;
    }
    gbq->next = NULL;
    gbq = GBQualFree (gbq);
  }
}


static void ConvertRnaRefNameToProductQual (SeqFeatPtr sfp)
{
  RnaRefPtr rrp;
  GBQualPtr gbq;

  if (sfp == NULL || sfp->data.choice != SEQFEAT_RNA || sfp->data.value.ptrvalue == NULL) return;
  rrp = (RnaRefPtr) sfp->data.value.ptrvalue;
  if (rrp->ext.choice != 1) return;
  
  gbq = GBQualNew ();
  gbq->next = sfp->qual;
  sfp->qual = gbq;
  gbq->qual = StringSave ("product");
  gbq->val = StringSave (rrp->ext.value.ptrvalue);
}


static void SetRnaImportExportItems (RnaFormPtr rfp)

{
  IteM        exportItm;
  IteM        importItm;
  RnaPagePtr  rpp;
  UIEnum      val;

  if (rfp != NULL) {
    importItm = FindFormMenuItem ((BaseFormPtr) rfp, VIB_MSG_IMPORT);
    exportItm = FindFormMenuItem ((BaseFormPtr) rfp, VIB_MSG_EXPORT);
    switch (rfp->currentPage) {
      case RNA_PAGE :
        rpp = (RnaPagePtr) GetObjectExtra (rfp->data);
        if (rpp != NULL && GetEnumPopup (rpp->type, rna_type_alist, &val) && val == 2) {
          SafeSetTitle (importItm, "Import cDNA FASTA...");
          SafeSetTitle (exportItm, "Export...");
          SafeEnable (importItm);
          SafeDisable (exportItm);
        } else {
          SafeSetTitle (importItm, "Import...");
          SafeSetTitle (exportItm, "Export...");
          SafeDisable (importItm);
          SafeDisable (exportItm);
        }
        break;
      case COMMON_PAGE :
        SafeSetTitle (importItm, "Import...");
        SafeSetTitle (exportItm, "Export...");
        SafeDisable (importItm);
        SafeDisable (exportItm);
        break;
      case LOCATION_PAGE :
        SafeSetTitle (importItm, "Import SeqLoc...");
        SafeSetTitle (exportItm, "Export SeqLoc...");
        SafeEnable (importItm);
        SafeEnable (exportItm);
        break;
      default :
        break;
    }
  }
}

static void SetRnaType (RnaPagePtr rpp, Uint2 subtype)

{
  RnaFormPtr  rfp;

  if (rpp != NULL && rpp->rfp != NULL) {
    rfp = rpp->rfp;
    switch (subtype) {
      case 0 :
        SafeHide (rpp->nameGrp);
        SafeHide (rpp->rrnaPrompt);
        SafeHide (rpp->ornaPrompt);
        SafeHide (rpp->trnaGrp);
        SafeHide (rfp->product);
        SafeHide (rfp->usrobjext);
        SafeHide (rpp->ncrnaGrp);
        SafeHide (rpp->tmrnaGrp);
        break;
      case FEATDEF_mRNA:
        SafeHide (rpp->trnaGrp);
        SafeHide (rpp->rrnaPrompt);
        SafeHide (rpp->ornaPrompt);
        SafeShow (rpp->nameGrp);
        SafeShow (rfp->product);
        SafeShow (rfp->usrobjext);
        SafeHide (rpp->ncrnaGrp);
        SafeHide (rpp->tmrnaGrp);
        break;
     case FEATDEF_preRNA :
        SafeHide (rpp->trnaGrp);
        SafeHide (rpp->rrnaPrompt);
        SafeHide (rpp->ornaPrompt);
        SafeShow (rpp->nameGrp);
        SafeShow (rfp->product);
        SafeShow (rfp->usrobjext);
        SafeHide (rpp->ncrnaGrp);
        SafeHide (rpp->tmrnaGrp);
        break;
      case FEATDEF_tRNA :
        SafeHide (rpp->nameGrp);
        SafeHide (rpp->rrnaPrompt);
        SafeHide (rpp->ornaPrompt);
        SafeShow (rpp->trnaGrp);
        SafeShow (rfp->product);
        SafeHide (rfp->usrobjext);
        SafeHide (rpp->ncrnaGrp);
        SafeHide (rpp->tmrnaGrp);
        break;
      case FEATDEF_rRNA :
        SafeHide (rpp->trnaGrp);
        SafeHide (rpp->ornaPrompt);
        SafeShow (rpp->rrnaPrompt);
        SafeShow (rpp->nameGrp);
        SafeShow (rfp->product);
        SafeHide (rfp->usrobjext);
        SafeHide (rpp->ncrnaGrp);
        SafeHide (rpp->tmrnaGrp);
        break;
      case FEATDEF_snRNA :
      case FEATDEF_scRNA :
      case FEATDEF_snoRNA :
        SafeHide (rpp->trnaGrp);
        SafeHide (rpp->rrnaPrompt);
        SafeHide (rpp->ornaPrompt);
        SafeHide (rpp->nameGrp);
        SafeShow (rfp->product);
        SafeHide (rfp->usrobjext);
        SafeShow (rpp->ncrnaGrp);
        SafeHide (rpp->tmrnaGrp);
        if (subtype == FEATDEF_snRNA) {
          PointerToDialog (rpp->ncrnaClass, "snRNA");
          
        } else if (subtype == FEATDEF_scRNA) {
          PointerToDialog (rpp->ncrnaClass, "scRNA");
        } else if (subtype == FEATDEF_snoRNA) {
          PointerToDialog (rpp->ncrnaClass, "snoRNA");
        } else if (subtype == FEATDEF_snoRNA) {
          PointerToDialog (rpp->ncrnaClass, "miscRNA");
        }
        break;

      case FEATDEF_ncRNA :
        SafeHide (rpp->trnaGrp);
        SafeHide (rpp->rrnaPrompt);
        SafeHide (rpp->ornaPrompt);
        SafeHide (rpp->nameGrp);
        SafeShow (rfp->product);
        SafeHide (rfp->usrobjext);
        SafeShow (rpp->ncrnaGrp);
        SafeHide (rpp->tmrnaGrp);
        break;
      case FEATDEF_tmRNA :
        SafeHide (rpp->trnaGrp);
        SafeHide (rpp->rrnaPrompt);
        SafeHide (rpp->ornaPrompt);
        SafeHide (rpp->nameGrp);
        SafeShow (rfp->product);
        SafeHide (rfp->usrobjext);
        SafeHide (rpp->ncrnaGrp);
        SafeShow (rpp->tmrnaGrp);
        break;
      case FEATDEF_otherRNA :
        SafeHide (rpp->trnaGrp);
        SafeHide (rpp->rrnaPrompt);
        SafeShow (rpp->ornaPrompt);
        SafeShow (rpp->nameGrp);
        SafeShow (rfp->product);
        SafeHide (rfp->usrobjext);
        break;
      default :
        SafeHide (rpp->trnaGrp);
        SafeHide (rpp->rrnaPrompt);
        SafeHide (rpp->ornaPrompt);
        SafeShow (rpp->nameGrp);
        SafeHide (rfp->product);
        SafeHide (rfp->usrobjext);
        SafeHide (rpp->ncrnaGrp);
        SafeHide (rpp->tmrnaGrp);
        break;
    }
    SetRnaImportExportItems (rpp->rfp);
    Update ();
  }
}

static void PopulateAAPopup (PopuP AAitem)

{
  Char             ch;
  Uint1            first;
  Uint1            i;
  Char             item [77];
  Uint1            last;
  SeqCodeTablePtr  sctp;
  CharPtr          str;

  sctp = SeqCodeTableFind (Seq_code_ncbieaa);
  first = FirstResidueInCode (sctp);
  last = LastResidueInCode (sctp);
  PopupItem (AAitem, " ");
  for (i = 65; i <= last; i++) {
    /*
    if (i == 74 || i == 79) {
      continue;
    }
    */
    ch = GetSymbolForResidue (sctp, i);
    str = (CharPtr) GetNameForResidue (sctp, i);
    sprintf (item, "%c    %s", ch, str);
    PopupItem (AAitem, item);
  }
  SetValue (AAitem, 1); 
}

static void ChangetRNASubPage (VoidPtr data, Int2 newval, Int2 oldval)

{
  RnaPagePtr  rpp;

  rpp = (RnaPagePtr) data;
  if (rpp != NULL) {
    if (oldval >= 0 && oldval <= 2) {
      SafeHide (rpp->trnaPages [oldval]);
    }
    if (newval >= 0 && newval <= 2) {
      SafeShow (rpp->trnaPages [newval]);
    }
    SetRnaImportExportItems (rpp->rfp);
    Update ();
  }
}

static CharPtr trnaTabs [] = {
  "Amino Acid", "Codons", "Anticodon", NULL
};


static Uint1 RnaTypeFromFeatdef (Uint2 featdef)
{
  switch (featdef) 
  {
    case FEATDEF_preRNA:
      return 1;
      break;
    case FEATDEF_mRNA:
      return 2;
      break;
    case FEATDEF_tRNA:
      return 3;
      break;
    case FEATDEF_rRNA:
      return 4;
      break;
    case FEATDEF_snRNA:
      return 8;
      break;
    case FEATDEF_scRNA:
      return 8;
      break;
    case FEATDEF_snoRNA:
      return 8;
      break;
    case FEATDEF_ncRNA:
      return 8;
      break;
    case FEATDEF_tmRNA:
      return 9; 
      break;
    case FEATDEF_misc_RNA:
      return 10;
      break;
    case FEATDEF_otherRNA:
    default:
      return 255;
      break;
  }
}


static void ChangeRNAType (PopuP p)

{
  RnaFormPtr       rfp;
  RnaRefPtr        rrp;
  RnaPagePtr       rpp;
  UIEnum           val;
  CharPtr          product = NULL;

  rfp = (RnaFormPtr) GetObjectExtra (p);
  if (rfp == NULL) return;
  rpp = (RnaPagePtr) GetObjectExtra (rfp->data);
  if (rpp == NULL) return;

  if (GetEnumPopup (rpp->type, rna_type_alist, &val)) {
    switch (rpp->prev_rna_type) {
      case 5:
      case 6:
      case 7:
      case 8:
        if (!TextHasNoText (rpp->ncrnaProduct)) {
          product = SaveStringFromText (rpp->ncrnaProduct);
        }
        break;
      case 9:
        if (!TextHasNoText (rpp->tmrnaProduct)) {
          product = SaveStringFromText (rpp->tmrnaProduct);
        }
        break;
      case 3:
        /* don't collect product for tRNA */
        break;
      default:
        if (!TextHasNoText (rpp->name)) {
          product = SaveStringFromText (rpp->name);
        }
        break;
    }
    switch (val) {
      case 3:
        /* don't set product for tRNA */
        break;
      case 5:
      case 6:
      case 7:
      case 8:
        SetTitle (rpp->ncrnaProduct, product);
        break;
      case 9:
        SetTitle (rpp->tmrnaProduct, product);
        break;
      default:
        SetTitle (rpp->name, product);
        break;
    }
    product = MemFree (product);
    rrp = RnaPageToRnaRefPtr (rpp->dialog);
    RnaRefPtrToRnaPage (rpp->dialog, rrp);
    rrp = RnaRefFree (rrp);
    rpp->prev_rna_type = val;
    Update ();
  }
}




static DialoG CreateRnaDialog (GrouP h, CharPtr title,
                               Uint2 subtype, SeqEntryPtr sep,
                               SeqFeatPtr sfp, RnaFormPtr rfp)

{
  GrouP       f;
  GrouP       g;
  Boolean     indexerVersion;
  GrouP       k;
  GrouP       m;
  GrouP       p;
  GrouP       q;
  RnaPagePtr  rpp;
  RnaRefPtr   rrp;
  GrouP       s;
  Boolean     showpseudo;
  PrompT      t;
  DialoG      tbs;
  GrouP       x;
  Uint1       rna_type;
  Uint2       entityID;

  p = HiddenGroup (h, 1, 0, NULL);
  SetGroupSpacing (p, 10, 10);

  rpp = (RnaPagePtr) MemNew (sizeof (RnaPage));
  if (rpp != NULL) {

    SetObjectExtra (p, rpp, StdCleanupExtraProc);
    rpp->dialog = (DialoG) p;
    rpp->todialog = RnaRefPtrToRnaPage;
    rpp->fromdialog = RnaPageToRnaRefPtr;
    rpp->testdialog = NULL;

    rpp->prev_rna_type = -1;

    if (title != NULL && title [0] != '\0') {
      s = NormalGroup (p, 0, -2, title, systemFont, NULL);
    } else {
      s = HiddenGroup (p, 0, -2, NULL);
    }
    m = HiddenGroup (s, -1, 0, NULL);
    SetGroupSpacing (m, 10, 10);

    rpp->rfp = rfp;

    f = HiddenGroup (m, -2, 0, NULL);
    StaticPrompt (f, "Type", 0, popupMenuHeight, programFont, 'l');
    rpp->type = PopupList (f, TRUE, ChangeRNAType);
    SetObjectExtra (rpp->type, rfp, NULL);
    InitEnumPopup (rpp->type, rna_type_alist, NULL);
    SetEnumPopup (rpp->type, rna_type_alist, (UIEnum) 0);

    StaticPrompt (m, "Changing RNA type will recreate the window.",
                        0, 0, programFont, 'c');

    showpseudo = FALSE;
    if (sfp != NULL && sfp->data.choice == SEQFEAT_RNA) {
      rrp = (RnaRefPtr) sfp->data.value.ptrvalue;
      if (rrp != NULL && rrp->pseudo) {
        showpseudo = TRUE;
      }
    }
    indexerVersion = (Boolean) (GetAppProperty ("InternalNcbiSequin") != NULL);
    if (indexerVersion) {
      showpseudo = TRUE;
    }
    if (showpseudo) {
      rpp->pseudo = CheckBox (m, "PseudoRNA", NULL);
    }

    g = HiddenGroup (m, 0, 0, NULL);

    rpp->nameGrp = HiddenGroup (g, -1, 0, NULL);
    q = HiddenGroup (rpp->nameGrp, -2, 0, NULL);
    StaticPrompt (q, "Name", 0, dialogTextHeight, programFont, 'l');
    rpp->name = DialogText (q, "", 20, NULL);
    x = HiddenGroup (rpp->nameGrp, 0, 0, NULL);
    rpp->rrnaPrompt = StaticPrompt (x, "E.g., 16S ribosomal RNA", 0, 0, programFont, 'c');
    rpp->ornaPrompt = StaticPrompt (x, "E.g., internal transcribed spacer 1", 0, 0, programFont, 'c');
    AlignObjects (ALIGN_CENTER, (HANDLE) q, (HANDLE) rpp->rrnaPrompt, (HANDLE) rpp->ornaPrompt, NULL);
    Hide (rpp->nameGrp);

    rpp->trnaGrp = HiddenGroup (g, -1, 0, NULL);
    SetGroupSpacing (rpp->trnaGrp, 10, 10);
    tbs = CreateFolderTabs (rpp->trnaGrp, trnaTabs, 0, 0, 0,
                            PROGRAM_FOLDER_TAB,
                            ChangetRNASubPage, (Pointer) rpp);
    k = HiddenGroup (rpp->trnaGrp, 0, 0, NULL);
    rpp->trnaPages [0] = HiddenGroup (k, -2, 0, NULL);
    StaticPrompt (rpp->trnaPages [0], "Amino Acid", 0, popupMenuHeight, programFont, 'l');
    rpp->AAitem = PopupList (rpp->trnaPages [0], TRUE, NULL);
    PopulateAAPopup (rpp->AAitem);
    rpp->trnaPages [1] = HiddenGroup (k, -1, 0, NULL);
    t = StaticPrompt (rpp->trnaPages [1], "Recognized Codons", 0, 0, programFont, 'c');
    rpp->codons = CreateVisibleStringDialog (rpp->trnaPages [1], 3, -1, 4);
    AlignObjects (ALIGN_CENTER, (HANDLE) t, (HANDLE) rpp->codons, NULL);
    Hide (rpp->trnaPages [1]);
    rpp->trnaPages [2] = HiddenGroup (k, -1, 0, NULL);
    rpp->anticodon = CreateIntervalEditorDialogEx (rpp->trnaPages [2], NULL,
                                                   3, 2, sep, TRUE, FALSE,
                                                   FALSE, FALSE, FALSE, NULL, NULL);
    Hide (rpp->trnaPages [2]);
    Hide (rpp->trnaGrp);

    AlignObjects (ALIGN_CENTER, (HANDLE) tbs,
                  (HANDLE) rpp->trnaPages [0], (HANDLE) rpp->trnaPages [1],
                  (HANDLE) rpp->trnaPages [2], NULL);
    AlignObjects (ALIGN_CENTER, (HANDLE) f, (HANDLE) rpp->nameGrp,
                  (HANDLE) rpp->trnaGrp,  (HANDLE) rpp->pseudo,NULL);
    Hide (rpp->ncrnaGrp);

    rpp->ncrnaGrp = HiddenGroup (g, 2, 0, NULL);
    SetGroupSpacing (rpp->ncrnaGrp, 10, 10);
    StaticPrompt (rpp->ncrnaGrp, "Class", 0, popupMenuHeight, programFont, 'l');
    rpp->ncrnaClass = CreatencRNAClassDialog (rpp->ncrnaGrp, FALSE, NULL, NULL);
    StaticPrompt (rpp->ncrnaGrp, "Product", 0, dialogTextHeight, programFont, 'l');
    rpp->ncrnaProduct = DialogText (rpp->ncrnaGrp, "", 10, NULL);
    Hide (rpp->ncrnaGrp);

    rpp->tmrnaGrp = HiddenGroup (g, 2, 0, NULL);
    SetGroupSpacing (rpp->tmrnaGrp, 10, 10);
    StaticPrompt (rpp->tmrnaGrp, "Tag Peptide", 0, dialogTextHeight, programFont, 'l');
    rpp->tmrnaTagPeptide = CreateRptUnitRangeDialog (rpp->tmrnaGrp, NULL, NULL, NULL, NULL);
    StaticPrompt (rpp->tmrnaGrp, "Product", 0, dialogTextHeight, programFont, 'l');
    rpp->tmrnaProduct = DialogText (rpp->tmrnaGrp, "", 10, NULL);
    Hide (rpp->tmrnaGrp);

    rna_type = RnaTypeFromFeatdef (subtype);

    SetEnumPopup (rpp->type, rna_type_alist, (UIEnum) rna_type);
    rpp->prev_rna_type = rna_type;

    if ((sfp != NULL && sfp->product != NULL) || indexerVersion) {
      entityID = SeqMgrGetEntityIDForSeqEntry(sep);
      rfp->product = CreateRNATranscriptIDDialog (m, entityID, TRUE, NULL, NULL);
    }

    SetRnaType (rpp, subtype);
  }

  return (DialoG) p;
}

static void ChangeRnaPage (VoidPtr data, Int2 newval, Int2 oldval)

{
  RnaFormPtr    rfp;

  rfp = (RnaFormPtr) data;
  if (rfp != NULL) {
    rfp->currentPage = newval;
    SafeHide (rfp->pages [oldval]);
    SafeShow (rfp->pages [newval]);
    switch (newval) {
      case RNA_PAGE :
        SendMessageToDialog (rfp->data, VIB_MSG_ENTER);
        break;
      case COMMON_PAGE :
        break;
      case LOCATION_PAGE :
        SendMessageToDialog (rfp->location, VIB_MSG_ENTER);
        break;
      default :
        break;
    }
    SetRnaImportExportItems (rfp);
    Update ();
  }
}

static Boolean ImportRnaForm (ForM f, CharPtr filename)

{
  BioseqPtr    bsp;
  SeqLocPtr    currslp;
  CharPtr      extension;
  FILE         *fp;
  BioseqPtr    mbsp;
  BioseqPtr    nbsp;
  SeqEntryPtr  nsep;
  SeqLocPtr    oldslp;
  Char         path [PATH_MAX];
  RnaFormPtr   rfp;
  SeqIdPtr     sip;
  SeqEntryPtr  sep;
  SeqLocPtr    slp;

  path [0] = '\0';
  StringNCpy_0 (path, filename, sizeof (path));
  rfp = (RnaFormPtr) GetObjectExtra (f);
  if (rfp != NULL) {
    switch (rfp->currentPage) {
      case RNA_PAGE :
        extension = GetAppProperty ("FastaNucExtension");
        if (path [0] != '\0' || GetInputFileName (path, sizeof (path), extension, "TEXT")) {
          fp = FileOpen (path, "r");
          if (fp != NULL) {
            sep = FastaToSeqEntry (fp, TRUE);
            if (sep != NULL && sep->choice == 1 && sep->data.ptrvalue != NULL) {
              mbsp = (BioseqPtr) sep->data.ptrvalue;
              nsep = GetBestTopParentForItemID (rfp->input_entityID,
                                                rfp->input_itemID,
                                                rfp->input_itemtype);
              if (nsep != NULL) {
                nsep = FindNucSeqEntry (nsep);
              }
              if (nsep != NULL && nsep->data.ptrvalue != NULL) {
                nbsp = (BioseqPtr) nsep->data.ptrvalue;
                slp = AlignmRNA2genomic (nbsp, mbsp);
                if (slp != NULL) {
                  currslp = DialogToPointer (rfp->location);
                  if (currslp != NULL) {
                    if (Message (MSG_OKC, "Replace existing location?") == ANS_CANCEL) {
                      slp = SeqLocFree (slp);
                    }
                    Select (rfp->form);
                    Update ();
                  }
                  SeqLocFree (currslp);
                  if (slp != NULL) {
                    sip = SeqLocId (slp);
                    if (sip != NULL) {
                      bsp = BioseqFind (sip);
                      if (bsp != NULL) {
                        if (bsp->repr == Seq_repr_seg) {
                          oldslp = slp;
                          slp = SegLocToParts (bsp, oldslp);
                          FreeAllFuzz (slp);
                          SeqLocFree (oldslp);
                        }
                      }
                    }
                    StripLocusFromSeqLoc (slp);
                    PointerToDialog (rfp->location, slp);
                    SeqLocFree (slp);
                  }
                }
              }
            }
            SeqEntryFree (sep);
          }
          FileClose (fp);
        }
        break;
      case LOCATION_PAGE :
        return ImportDialog (rfp->location, filename);
      default :
        break;
    }
  }
  return FALSE;
}

static Boolean ExportRnaForm (ForM f, CharPtr filename)

{
  RnaFormPtr  rfp;

  rfp = (RnaFormPtr) GetObjectExtra (f);
  if (rfp != NULL) {
    switch (rfp->currentPage) {
      case LOCATION_PAGE :
        return ExportDialog (rfp->location, filename);
      default :
        break;
    }
  }
  return FALSE;
}

static CharPtr  rnaFormTabs [] = {
  NULL, "Properties", "Location", NULL
};

static void RnaFormMessage (ForM f, Int2 mssg)

{
  RnaFormPtr  rfp;

  rfp = (RnaFormPtr) GetObjectExtra (f);
  if (rfp != NULL) {
    switch (mssg) {
      case VIB_MSG_INIT :
        StdInitFeatFormProc (f);
        break;
      case VIB_MSG_IMPORT :
        ImportRnaForm (f, NULL);
        break;
      case VIB_MSG_EXPORT :
        ExportRnaForm (f, NULL);
        break;
      case VIB_MSG_CLOSE :
        Remove (f);
        break;
      case VIB_MSG_CUT :
        StdCutTextProc (NULL);
        break;
      case VIB_MSG_COPY :
        StdCopyTextProc (NULL);
        break;
      case VIB_MSG_PASTE :
        StdPasteTextProc (NULL);
        break;
      case VIB_MSG_DELETE :
        if (rfp->currentPage == LOCATION_PAGE) {
          PointerToDialog (rfp->location, NULL);
        } else {
          StdDeleteTextProc (NULL);
        }
        break;
      default :
        if (rfp->appmessage != NULL) {
          rfp->appmessage (f, mssg);
        }
        break;
    }
  }
}

static void RnaFormActivate (WindoW w)

{
  RnaFormPtr  rfp;

  rfp = (RnaFormPtr) GetObjectExtra (w);
  if (rfp != NULL) {
    if (rfp->activate != NULL) {
      rfp->activate (w);
    }
    SetRnaImportExportItems (rfp);
  }
}

static void RnaRefPtrToForm (ForM f, Pointer data)

{
  SeqEntryPtr   oldsep;
  RnaFormPtr    rfp;
  SeqEntryPtr   sep;
  SeqFeatPtr    sfp;
  Int4          val;

  rfp = (RnaFormPtr) GetObjectExtra (f);
  if (rfp != NULL) {
    sep = GetTopSeqEntryForEntityID (rfp->input_entityID);
    oldsep = SeqEntrySetScope (sep);
    sfp = (SeqFeatPtr) data;
    if (sfp != NULL) {
      PointerToDialog (rfp->location, sfp->location);
      switch (sfp->data.choice) {
        case SEQFEAT_BOND :
        case SEQFEAT_SITE :
        case SEQFEAT_PSEC_STR :
          val = (Int4) sfp->data.value.intvalue;
          PointerToDialog (rfp->data, (Pointer) &(val));
          break;
        case SEQFEAT_COMMENT:
          break;
        default :
          PointerToDialog (rfp->data, sfp->data.value.ptrvalue);
          break;
      }
      SeqFeatPtrToCommon ((FeatureFormPtr) rfp, sfp);
      PointerToDialog (rfp->product, sfp->product);
    }
    SeqEntrySetScope (oldsep);
  }
}


static void RnaFormClearButtonProc (ButtoN b)
{
  RnaFormPtr         rfp;

  rfp = (RnaFormPtr) GetObjectExtra (b);
  if (rfp != NULL)
  {
    PointerToDialog (rfp->data, NULL);
    SendMessageToDialog (rfp->data, VIB_MSG_INIT);
    SendMessageToDialog (rfp->location, NUM_VIB_MSG + 1);

    SeqFeatPtrToCommon ((FeatureFormPtr) rfp, NULL);
    
  }
}

extern ForM CreateRnaForm (Int2 left, Int2 top, CharPtr title,
                           SeqFeatPtr sfp, SeqEntryPtr sep,
                           Uint2 subtype, FormActnFunc actproc)

{
  ButtoN             b;
  GrouP              c;
  GrouP              g;
  GrouP              h;
  GrouP              k;
  RnaFormPtr         rfp;
  GrouP              s;
  StdEditorProcsPtr  sepp;
  WindoW             w;
  Boolean            indexerVersion;

  w = NULL;
  rfp = (RnaFormPtr) MemNew (sizeof (RnaForm));
  if (rfp != NULL) {
    w = FixedWindow (left, top, -10, -10, title, StdCloseWindowProc);
    SetObjectExtra (w, rfp, StdFeatFormCleanupProc);
    rfp->form = (ForM) w;
    rfp->actproc = actproc;
    rfp->toform = RnaRefPtrToForm;
    rfp->fromform = NULL;
    rfp->formmessage = RnaFormMessage;
    rfp->testform = NULL;
    rfp->importform = ImportRnaForm;
    rfp->exportform = ExportRnaForm;
    
    if (sfp == NULL)
    {
      rfp->empty_feat = TRUE;
    }
    else
    {
      rfp->empty_feat = FALSE;
    }

#ifndef WIN_MAC
    CreateStdEditorFormMenus (w);
#endif

    rfp->activate = NULL;
    sepp = (StdEditorProcsPtr) GetAppProperty ("StdEditorForm");
    if (sepp != NULL) {
      rfp->activate = sepp->activateForm;
      rfp->appmessage = sepp->handleMessages;
    }
    SetActivate (w, RnaFormActivate);

    g = HiddenGroup (w, -1, 0, NULL);
    SetGroupSpacing (g, 3, 10);

    rfp->sep = sep;
    rnaFormTabs [0] = NULL;
    if (title != NULL && *title != '\0') {
      rnaFormTabs [0] = title;
    } else {
      rnaFormTabs [0] = "RNA";
    }
    rfp->foldertabs = CreateFolderTabs (g, rnaFormTabs, RNA_PAGE,
                                        0, 0, SYSTEM_FOLDER_TAB,
                                        ChangeRnaPage, (Pointer) rfp);
    rfp->currentPage = RNA_PAGE;

    h = HiddenGroup (g, 0, 0, NULL);

    s = HiddenGroup (h, -1, 0, NULL);
    SetGroupSpacing (s, 3, 10);
    rfp->data = CreateRnaDialog (s, NULL, subtype, sep, sfp, rfp);
    rfp->pages [RNA_PAGE] = s;
    Hide (rfp->pages [RNA_PAGE]);
    rnaFormTabs [0] = NULL;

    s = HiddenGroup (h, -1, 0, NULL);
    CreateCommonFeatureGroup (s, (FeatureFormPtr) rfp, sfp, TRUE, TRUE);
    rfp->pages [COMMON_PAGE] = s;
    Hide (rfp->pages [COMMON_PAGE]);

    s = HiddenGroup (h, -1, 0, NULL);
    rfp->location = CreateIntervalEditorDialogEx (s, NULL, 4, 2, sep, TRUE, FALSE,
                                                  TRUE, TRUE, FALSE,
                                                  (FeatureFormPtr) rfp,
                                                  StdFeatIntEdPartialCallback);
    rfp->pages [LOCATION_PAGE] = s;
    Hide (rfp->pages [LOCATION_PAGE]);

    AlignObjects (ALIGN_CENTER, (HANDLE) rfp->pages [RNA_PAGE],
                  (HANDLE) rfp->pages [COMMON_PAGE], (HANDLE) rfp->pages [LOCATION_PAGE],
                  NULL);
    AlignObjects (ALIGN_CENTER, (HANDLE) rfp->foldertabs, (HANDLE) h, NULL);

    indexerVersion = (Boolean) (GetAppProperty ("InternalNcbiSequin") != NULL);
    if (rfp->empty_feat && indexerVersion)
    {
      k = HiddenGroup (w, 2, 0, NULL);
      SetGroupSpacing (k, 10, 10);
      b = PushButton (k, "Clear", RnaFormClearButtonProc);
      SetObjectExtra (b, rfp, NULL);
      rfp->leave_dlg_up = CheckBox (k, "Leave Dialog Up", NULL);
    }
    else
    {
      k = NULL;
      rfp->leave_dlg_up = NULL;
    }

    c = HiddenGroup (w, 2, 0, NULL);
    b = PushButton (c, "Accept", StdFeatFormAcceptButtonProc);
    SetObjectExtra (b, rfp, NULL);
    PushButton (c, "Cancel", StdCancelButtonProc);
    
    AlignObjects (ALIGN_CENTER, (HANDLE) g, (HANDLE) c, (HANDLE) k, NULL);
    RealizeWindow (w);

    SendMessageToDialog (rfp->data, VIB_MSG_INIT);
    SendMessageToDialog (rfp->location, VIB_MSG_INIT);
    SendMessageToDialog (rfp->product, VIB_MSG_INIT);
    SendMessageToDialog (rfp->usrobjext, VIB_MSG_INIT);
    Show (rfp->pages [rfp->currentPage]);
    SendMessageToDialog (rfp->data, VIB_MSG_ENTER);
    Update ();
  }
  return (ForM) w;
}

extern Int2 LIBCALLBACK RnaGenFunc (Pointer data)

{
  FeatDefPtr        curr;
  HelpMessageFunc   helpfunc;
  Uint1             key;
  CharPtr           label = NULL;
  ObjMgrPtr         omp;
  OMProcControlPtr  ompcp;
  ObjMgrTypePtr     omtp;
  OMUserDataPtr     omudp;
  ObjMgrProcPtr     proc;
  RnaFormPtr        rfp;
  RnaPagePtr        rpp;
  RnaRefPtr         rrp;
  SeqEntryPtr       sep;
  SeqFeatPtr        sfp;
  Uint2             subtype;
  Char              title [64];
  WindoW            w;

  ompcp = (OMProcControlPtr) data;
  sfp = NULL;
  sep = NULL;
  subtype = FEATDEF_otherRNA;
  if (ompcp == NULL || ompcp->proc == NULL) return OM_MSG_RET_ERROR;
  proc = ompcp->proc;
  switch (ompcp->input_itemtype) {
    case OBJ_SEQFEAT :
      sfp = (SeqFeatPtr) ompcp->input_data;
      if (sfp != NULL && sfp->data.choice != SEQFEAT_RNA) {
        return OM_MSG_RET_ERROR;
      }
      break;
    case OBJ_BIOSEQ :
      break;
    case OBJ_BIOSEQSET :
      break;
    case 0 :
      break;
    default :
      return OM_MSG_RET_ERROR;
  }
  omudp = ItemAlreadyHasEditor (ompcp->input_entityID, ompcp->input_itemID,
                                ompcp->input_itemtype, ompcp->proc->procid);
  if (omudp != NULL) {
    rfp = (RnaFormPtr) omudp->userdata.ptrvalue;
    if (rfp != NULL) {
      Select (rfp->form);
    }
    return OM_MSG_RET_DONE;
  }
  StringCpy (title, "Unknown RNA");
  if (sfp != NULL && sfp->data.value.ptrvalue != NULL) {
    rrp = (RnaRefPtr) sfp->data.value.ptrvalue;
    StringNCpy_0 (title,
                GetEnumName (rrp->type, rna_type_alist),
                sizeof (title));
    if (StringHasNoText (title)) {
      StringCpy (title, "Unknown RNA");
    } else if (proc->subinputtype > 0) {
      subtype = proc->subinputtype;
    }
  } else if (proc->subinputtype > 0) {
    subtype = proc->subinputtype;
    curr = FeatDefFindNext (NULL, &key, &label, FEATDEF_ANY, TRUE);
    while (curr != NULL) {
      if (key != FEATDEF_BAD && curr->seqfeat_key == SEQFEAT_RNA) {
        if (subtype == curr->featdef_key) {
          StringNCpy_0 (title, curr->typelabel, sizeof (title));
          break;
        }
      }
      curr = FeatDefFindNext (curr, &key, &label, FEATDEF_ANY, TRUE);
    }
  }
  sep = GetTopSeqEntryForEntityID (ompcp->input_entityID);
  ModernizeRNAFields (sfp);
  w = (WindoW) CreateRnaForm (-50, -33, title, sfp, sep,
                              subtype, StdFeatFormActnProc);
  rfp = (RnaFormPtr) GetObjectExtra (w);
  if (rfp != NULL) {
    rfp->input_entityID = ompcp->input_entityID;
    rfp->input_itemID = ompcp->input_itemID;
    rfp->input_itemtype = ompcp->input_itemtype;
    rfp->this_itemtype = OBJ_SEQFEAT;
    rfp->this_subtype = subtype;
    rfp->procid = ompcp->proc->procid;
    rfp->proctype = ompcp->proc->proctype;
    rfp->userkey = OMGetNextUserKey ();
    omudp = ObjMgrAddUserData (ompcp->input_entityID, ompcp->proc->procid,
                               OMPROC_EDIT, rfp->userkey);
    if (omudp != NULL) {
      omudp->userdata.ptrvalue = (Pointer) rfp;
      omudp->messagefunc = StdVibrantEditorMsgFunc;
    }
    if (sfp != NULL) {
      omp = ObjMgrGet ();
      if (omp != NULL) {
        omtp = ObjMgrTypeFind (omp, OBJ_SEQFEAT, NULL, NULL);
        if (omtp != NULL && omtp->subtypefunc != NULL) {
          rfp->this_subtype = (*(omtp->subtypefunc)) (sfp);
        }
      }
    }
    SendMessageToForm (rfp->form, VIB_MSG_INIT);
    if (sfp != NULL) {
      PointerToForm (rfp->form, (Pointer) sfp);
      SetClosestParentIfDuplicating ((BaseFormPtr) rfp);
    } else {
      rpp = (RnaPagePtr) GetObjectExtra (rfp->data);
      if (rpp != NULL) {
        SetNewFeatureDefaultInterval ((FeatureFormPtr) rfp);
        SetEnumPopup (rpp->type, rna_type_alist, RnaTypeFromFeatdef (subtype));
        if (subtype == FEATDEF_tRNA)
        {
          /* attempt to provide default location for anticodons */
          SeqLocPtr tmp_loc = NULL;
          
          tmp_loc = DialogToPointer (rpp->rfp->location);
          PointerToDialog (rpp->anticodon, tmp_loc);
          SetSequenceAndStrandForIntervalPage (rpp->anticodon);
          SeqLocFree (tmp_loc);              
        }
      }
    }
  }
  Show (w);
  Select (w);
  helpfunc = (HelpMessageFunc) GetAppProperty ("HelpMessageProc");
  if (helpfunc != NULL) {
    helpfunc ("Features", title);
  }
  return OM_MSG_RET_DONE;
}


typedef struct ncrnaclassdlg {
  DIALOG_MESSAGE_BLOCK
  PopuP                ncrnaclass;
  TexT                 otherclass;
  Nlm_ChangeNotifyProc change_notify;
  Pointer              change_userdata;
  Boolean              is_constraint;

} NcrnaClassDlgData, PNTR NcrnaClassDlgPtr;

static void ChangeNcrnaClass (PopuP p)
{
  NcrnaClassDlgPtr dlg;

  dlg = (NcrnaClassDlgPtr) GetObjectExtra (p);
  if (dlg == NULL) return;

  if (GetValue (dlg->ncrnaclass) == NcrnaOTHER) {
    Show (dlg->otherclass);
  } else {
    Hide (dlg->otherclass);
  }

  if (dlg->change_notify != NULL) {
    (dlg->change_notify) (dlg->change_userdata);
  }
}


static void ChangeNcrnaOtherClass (TexT t)
{
  NcrnaClassDlgPtr dlg;

  dlg = (NcrnaClassDlgPtr) GetObjectExtra (t);
  if (dlg == NULL) return;
  if (dlg->change_notify != NULL) {
    (dlg->change_notify) (dlg->change_userdata);
  }
}


static void PointerToNcrnaClassDialog (DialoG d, Pointer data)
{
  NcrnaClassDlgPtr dlg;
  CharPtr PNTR     cpp;
  Int4             pos = NcrnaOTHER;

  dlg = (NcrnaClassDlgPtr) GetObjectExtra (d);
  if (dlg == NULL) return;

  if (data == NULL) {
    if (dlg->is_constraint) {
      pos = NcrnaOTHER + 1;
    }
  } else {
    if (data != NULL) {
      for (cpp = ncrnaClassList, pos = 1;
          *cpp != NULL && pos < NcrnaOTHER;
          cpp++, pos++) {
        if (StringCmp (*cpp, (CharPtr) data) == 0) {
          break;
        }
      }
    }
  }
  
  SetValue (dlg->ncrnaclass, pos);
  if (pos == NcrnaOTHER) {
    if (StringCmp ((CharPtr) data, "other") == 0) {
      SetTitle (dlg->otherclass, "");
    } else {
      SetTitle (dlg->otherclass, (CharPtr) data);
    }
    Show (dlg->otherclass);
  } else {
    Hide (dlg->otherclass);
  }
}


static Pointer NcrnaClassDialogToPointer (DialoG d)
{
  NcrnaClassDlgPtr dlg;
  Int4             pos;
  CharPtr          rval = NULL;

  dlg = (NcrnaClassDlgPtr) GetObjectExtra (d);
  if (dlg == NULL) return NULL;
  pos = GetValue (dlg->ncrnaclass);
  if (pos > 0 && pos < NcrnaOTHER) {
    rval = StringSave (ncrnaClassList[pos - 1]);
  } else if (pos == NcrnaOTHER) {
    if (TextHasNoText (dlg->otherclass)) {
      rval = StringSave ("other");
    } else {
      rval = SaveStringFromText (dlg->otherclass);
    }
  }  
  return rval;
}


static ValNodePtr TestNcrnaClassDialog (DialoG d)
{
  ValNodePtr err_list = NULL;

  NcrnaClassDlgPtr dlg;
  Int4             pos;

  dlg = (NcrnaClassDlgPtr) GetObjectExtra (d);
  if (dlg == NULL) return NULL;
  pos = GetValue (dlg->ncrnaclass);

  if (pos < 1 && !dlg->is_constraint) {
    ValNodeAddPointer (&err_list, 0, "No ncRNA class");
  } else if (!dlg->is_constraint && pos > NcrnaOTHER) {
    ValNodeAddPointer (&err_list, 0, "No ncRNA class");
  }
  return err_list;
}


extern DialoG CreatencRNAClassDialog (GrouP h, Boolean is_constraint, Nlm_ChangeNotifyProc change_notify, Pointer change_userdata)
{
  NcrnaClassDlgPtr dlg;
  GrouP            p;
  CharPtr PNTR cpp;

  p = HiddenGroup (h, 2, 0, NULL);  
  dlg = (NcrnaClassDlgPtr) MemNew (sizeof (NcrnaClassDlgData));
  SetObjectExtra (p, dlg, StdCleanupExtraProc);
  
  dlg->dialog = (DialoG) p;
  dlg->todialog = PointerToNcrnaClassDialog;
  dlg->fromdialog = NcrnaClassDialogToPointer;
  dlg->testdialog = TestNcrnaClassDialog;
  dlg->change_notify = change_notify;
  dlg->change_userdata = change_userdata;
  dlg->is_constraint = is_constraint;
  
  dlg->ncrnaclass = PopupList (p, TRUE, ChangeNcrnaClass);
  SetObjectExtra (dlg->ncrnaclass, dlg, NULL);
  for (cpp = ncrnaClassList; *cpp != NULL; cpp++) {
    PopupItem (dlg->ncrnaclass, *cpp);
  }
  if (is_constraint) {
    PopupItem (dlg->ncrnaclass, "Any");
    SetValue (dlg->ncrnaclass, NcrnaOTHER + 1);
  } else {
    SetValue (dlg->ncrnaclass, NcrnaOTHER);
  }
  dlg->otherclass = DialogText (p, "", 10, ChangeNcrnaOtherClass);
  SetObjectExtra (dlg->otherclass, dlg, NULL);

  return (DialoG) p;
}


extern RnaTypePtr RnaTypeFree (RnaTypePtr rtp)
{
  if (rtp != NULL)
  {
    rtp->ncrna_class = MemFree (rtp->ncrna_class);
    rtp = MemFree (rtp);
  }
  return rtp;
}


extern Boolean MatchesRnaType (SeqFeatPtr sfp, RnaTypePtr rtp)
{
  RnaRefPtr rrp;
  RNAGenPtr rgp;
  Uint1     featdef;
  Boolean   matches = TRUE;

  if (sfp == NULL || sfp->data.choice != SEQFEAT_RNA) return FALSE;
  if (rtp == NULL || rtp->rna_featdef == FEATDEF_ANY) return TRUE;

  rrp = (RnaRefPtr) sfp->data.value.ptrvalue;
  if (rrp == NULL) return FALSE;

  featdef = RnaFeatDefFromRnaRef (rrp);
  if (featdef != rtp->rna_featdef) return FALSE;

  if (featdef == FEATDEF_ncRNA)
  {
    if (rtp->ncrna_class == NULL) {
      matches = TRUE;
    } else {
      if (rrp->ext.choice == 3) {
        rgp = (RNAGenPtr) rrp->ext.value.ptrvalue;
        if (StringCmp (rgp->_class, rtp->ncrna_class) == 0) {
          matches = TRUE;
        }
      }
    }
  }
  return matches; 
}


extern void ApplyRnaTypeToSeqFeat (SeqFeatPtr sfp, RnaTypePtr rtp)
{
  RnaRefPtr rrp;
  RNAGenPtr rgp;

  if (sfp == NULL || sfp->data.choice != SEQFEAT_RNA)
  {
    return;
  }

  rrp = (RnaRefPtr) sfp->data.value.ptrvalue;
  if (rrp == NULL)
  {
    rrp = RnaRefNew();
    sfp->data.value.ptrvalue = rrp;
  }

  if (rtp == NULL || rtp->rna_featdef == FEATDEF_ANY)
  {
    return;
  }

  switch (rtp->rna_featdef)
  {
    case FEATDEF_preRNA:
      rrp->type = 1;
      break;
    case FEATDEF_mRNA:
      rrp->type = 2;
      break;
    case FEATDEF_tRNA:
      rrp->type = 3;
      break;
    case FEATDEF_rRNA:
      rrp->type = 4;
      break;
    case FEATDEF_scRNA:
    case FEATDEF_snRNA:
    case FEATDEF_snoRNA:
    case FEATDEF_ncRNA:
    case FEATDEF_misc_RNA:
      rrp->type = 8;
      if (rrp->ext.choice == 1) {
        rgp = RNAGenNew ();
        rgp->product = rrp->ext.value.ptrvalue;
        rrp->ext.value.ptrvalue = rgp;
        rrp->ext.choice = 3;
      }

      if (rrp->ext.choice != 3) {
        rrp->ext.choice = 3;        
        rrp->ext.value.ptrvalue = RNAGenNew ();
      }

      rgp = (RNAGenPtr) rrp->ext.value.ptrvalue;
      if (rgp == NULL) {
        rgp = RNAGenNew ();
        rrp->ext.value.ptrvalue = rgp;
      }
      if (rtp->rna_featdef == FEATDEF_scRNA) {
        rgp->_class = StringSave ("scRNA");
      } else if (rtp->rna_featdef == FEATDEF_snRNA) {
        rgp->_class = StringSave ("snRNA");
      } else if (rtp->rna_featdef == FEATDEF_snoRNA) {
        rgp->_class = StringSave ("snoRNA");
      } else if (rtp->rna_featdef == FEATDEF_ncRNA) {
        rgp->_class = StringSave (rtp->ncrna_class);
      } else if (rtp->rna_featdef == FEATDEF_misc_RNA) {
        if (IsStringInNcRNAClassList (rgp->product)) {
          rgp->_class = rgp->product;
          rgp->product = NULL;
        } else {
          rrp->type = 10;
        }
      } 
      break;
    case FEATDEF_tmRNA:
      rrp->type = 9;
      if (rrp->ext.choice == 1) {
        rgp = RNAGenNew ();
        rgp->product = rrp->ext.value.ptrvalue;
        rrp->ext.value.ptrvalue = rgp;
        rrp->ext.choice = 3;
      }

      if (rrp->ext.choice != 3) {
        rrp->ext.choice = 3;        
        rrp->ext.value.ptrvalue = RNAGenNew ();
      }

      rgp = (RNAGenPtr) rrp->ext.value.ptrvalue;
      if (rgp == NULL) {
        rgp = RNAGenNew ();
        rrp->ext.value.ptrvalue = rgp;
      }
      break;
    case FEATDEF_otherRNA:
      rrp->type = 255;
      if (rrp->ext.choice == 3) {
        rrp->ext.value.ptrvalue = RNAGenFree (rrp->ext.value.ptrvalue);
        rrp->ext.choice = 0;
      }
      if (rrp->ext.choice == 1 || rrp->ext.choice == 0) {
        rrp->ext.value.ptrvalue = MemFree (rrp->ext.value.ptrvalue);
        rrp->ext.value.ptrvalue = StringSave ("misc_RNA");
        rrp->ext.choice = 1;
      }
      break;
  }

}


extern void AddToComment (SeqFeatPtr sfp, CharPtr comment)
{
  CharPtr tmp;

  if (sfp == NULL || StringHasNoText (comment)) return;

  if (StringHasNoText (sfp->comment))
  {
    sfp->comment = MemFree (sfp->comment);
    sfp->comment = StringSave (comment);
  }
  else
  {
    tmp = (CharPtr) MemNew (sizeof (Char) * (StringLen (sfp->comment) + StringLen (comment) + 3));
    sprintf (tmp, "%s; %s", sfp->comment, comment);
    sfp->comment = MemFree (sfp->comment);
    sfp->comment = tmp;
  }
}


extern void ApplyProductToRNA (SeqFeatPtr sfp, CharPtr product)
{
  RnaRefPtr rrp;
  Uint1     aa;
  Int4      j;
  Boolean   justTrnaText = FALSE;
  Char      codon [16];
  tRNAPtr   trp;

  if (sfp == NULL || sfp->data.choice != SEQFEAT_RNA
      || sfp->data.value.ptrvalue == NULL)
  {
    return;
  }

  rrp = (RnaRefPtr) sfp->data.value.ptrvalue;
  if (rrp->type == 3)
  {
    RemoveRNAProductString (sfp, NULL);

    aa = ParseTRnaString (product, &justTrnaText, (Uint1Ptr) codon, TRUE);
    if (aa != 0) 
    {
      trp = (tRNAPtr) MemNew (sizeof (tRNA));
      if (trp != NULL) 
      {
        trp->aatype = 2;
        for (j = 0; j < 6; j++)
        {
          trp->codon [j] = 255;
        }
        if (justTrnaText)
        {
          for (j = 0; j < 6; j++)
          {
            trp->codon [j] = codon [j];
          }
        }
        trp->aa = aa;
        rrp->ext.choice = 2;
        rrp->ext.value.ptrvalue = (Pointer) trp;
      }
    }
    if (aa == 0 || !justTrnaText)
    {
      AddToComment (sfp, product);
    }
  }
  else
  {
    SetRNAProductString (sfp, NULL, product, ExistingTextOption_replace_old);
  }
}



typedef struct rnatypedlg {
  DIALOG_MESSAGE_BLOCK
  PopuP                rna_type;
  DialoG               ncrna_class;
  Nlm_ChangeNotifyProc change_notify;
  Pointer              change_userdata;
  Boolean              is_constraint;

} RnaTypeDlgData, PNTR RnaTypeDlgPtr;


static void ChangeRnaTypeDlg (PopuP p)
{
  RnaTypeDlgPtr dlg;
  UIEnum        val;

  dlg = (RnaTypeDlgPtr) GetObjectExtra (p);
  if (dlg != NULL)
  {
    if (GetEnumPopup (dlg->rna_type, rna_type_alist, &val) && val == 8)
    {
      Enable (dlg->ncrna_class);
    }
    else
    {
      Disable (dlg->ncrna_class);
    }
    if (dlg->change_notify != NULL)
    {
      (dlg->change_notify) (dlg->change_userdata);
    }
  }
}


static void PointerToRnaTypeDialog (DialoG d, Pointer data)
{
  RnaTypeDlgPtr dlg;
  RnaTypePtr    rtp = NULL;

  dlg = (RnaTypeDlgPtr) GetObjectExtra (d);
  if (dlg == NULL) return;

  rtp = (RnaTypePtr) data;
  if (rtp == NULL || rtp->rna_featdef == FEATDEF_ANY) 
  {
    SetEnumPopup (dlg->rna_type, rna_type_alist, (UIEnum) 0);
    PointerToDialog (dlg->ncrna_class, NULL);
  }
  else
  {
    SetEnumPopup (dlg->rna_type, rna_type_alist, RnaTypeFromFeatdef (rtp->rna_featdef));
  }
  ChangeRnaTypeDlg (dlg->rna_type);
}


static Pointer RnaTypeDialogToPointer (DialoG d)
{
  RnaTypeDlgPtr dlg;
  RnaTypePtr    rtp = NULL;
  UIEnum        val;

  dlg = (RnaTypeDlgPtr) GetObjectExtra (d);
  if (dlg == NULL) return NULL;

  if (GetEnumPopup (dlg->rna_type, rna_type_alist, &val))
  {
    rtp = (RnaTypePtr) MemNew (sizeof (RnaTypeData));
    rtp->rna_featdef = FEATDEF_ANY;
    rtp->ncrna_class = NULL;
    switch (val)
    {
      case 0:
        rtp->rna_featdef = FEATDEF_ANY;
        break;
      case 1:
        rtp->rna_featdef = FEATDEF_preRNA;
        break;
      case 2:
        rtp->rna_featdef = FEATDEF_mRNA;
        break;
      case 3:
        rtp->rna_featdef = FEATDEF_tRNA;
        break;
      case 4:
        rtp->rna_featdef = FEATDEF_rRNA;
        break;
      case 8:
        rtp->rna_featdef = FEATDEF_ncRNA;
        rtp->ncrna_class = DialogToPointer (dlg->ncrna_class);
        break;
      case 9:
        rtp->rna_featdef = FEATDEF_tmRNA;
        break;
      case 10:
        rtp->rna_featdef = FEATDEF_misc_RNA;
        break;
      case 255:
        rtp->rna_featdef = FEATDEF_otherRNA;
        break;
    }
  }
  return rtp;
}
 

static ValNodePtr TestRnaTypeDialog (DialoG d)
{
  RnaTypeDlgPtr dlg;
  ValNodePtr    err_list = NULL;
  UIEnum        val;

  dlg = (RnaTypeDlgPtr) GetObjectExtra (d);
  if (dlg == NULL) return NULL;
  if (GetEnumPopup (dlg->rna_type, rna_type_alist, &val)) {
    if (val == 8) {
      err_list = TestDialog (dlg->ncrna_class);
    }
  } else {
    ValNodeAddPointer (&err_list, 0, "RNA type");
  }
  return err_list;
}  

extern DialoG RnaTypeDialog (GrouP h, Boolean is_constraint, Nlm_ChangeNotifyProc change_notify, Pointer change_userdata)
{
  RnaTypeDlgPtr dlg;
  GrouP         p;
  CharPtr       title;

  if (is_constraint) {
    title = "RNA Type of Feature to be Edited";
  } else {
    title = "RNA Type";
  }
  
  p = NormalGroup (h, -1, 0, title, programFont, NULL);  
  dlg = (RnaTypeDlgPtr) MemNew (sizeof (RnaTypeDlgData));
  SetObjectExtra (p, dlg, StdCleanupExtraProc);
  
  dlg->dialog = (DialoG) p;
  dlg->todialog = PointerToRnaTypeDialog;
  dlg->fromdialog = RnaTypeDialogToPointer;
  dlg->testdialog = TestRnaTypeDialog;
  dlg->change_notify = change_notify;
  dlg->change_userdata = change_userdata;
  dlg->is_constraint = is_constraint;

  dlg->rna_type = PopupList (p, TRUE, ChangeRnaTypeDlg);
  SetObjectExtra (dlg->rna_type, dlg, NULL);
  InitEnumPopup (dlg->rna_type, rna_type_alist, NULL);
  SetEnumPopup (dlg->rna_type, rna_type_alist, (UIEnum) 0);
  
  dlg->ncrna_class = CreatencRNAClassDialog (p, is_constraint, change_notify, change_userdata);

  AlignObjects (ALIGN_CENTER, (HANDLE) dlg->rna_type, (HANDLE) dlg->ncrna_class, NULL);

  return (DialoG) p;
}

