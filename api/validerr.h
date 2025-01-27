#ifndef __MODULE_valid__
#define __MODULE_valid__

#define ERR_SEQ_INST  1,0
#define ERR_SEQ_INST_ExtNotAllowed  1,1
#define ERR_SEQ_INST_ExtBadOrMissing  1,2
#define ERR_SEQ_INST_SeqDataNotFound  1,3
#define ERR_SEQ_INST_SeqDataNotAllowed  1,4
#define ERR_SEQ_INST_ReprInvalid  1,5
#define ERR_SEQ_INST_CircularProtein  1,6
#define ERR_SEQ_INST_DSProtein  1,7
#define ERR_SEQ_INST_MolNotSet  1,8
#define ERR_SEQ_INST_MolOther  1,9
#define ERR_SEQ_INST_FuzzyLen  1,10
#define ERR_SEQ_INST_InvalidLen  1,11
#define ERR_SEQ_INST_InvalidAlphabet  1,12
#define ERR_SEQ_INST_SeqDataLenWrong  1,13
#define ERR_SEQ_INST_SeqPortFail  1,14
#define ERR_SEQ_INST_InvalidResidue  1,15
#define ERR_SEQ_INST_StopInProtein  1,16
#define ERR_SEQ_INST_PartialInconsistent  1,17
#define ERR_SEQ_INST_ShortSeq  1,18
#define ERR_SEQ_INST_NoIdOnBioseq  1,19
#define ERR_SEQ_INST_BadDeltaSeq  1,20
#define ERR_SEQ_INST_LongHtgsSequence  1,21
#define ERR_SEQ_INST_LongLiteralSequence  1,22
#define ERR_SEQ_INST_SequenceExceeds350kbp  1,23
#define ERR_SEQ_INST_ConflictingIdsOnBioseq  1,24
#define ERR_SEQ_INST_MolNuclAcid  1,25
#define ERR_SEQ_INST_ConflictingBiomolTech  1,26
#define ERR_SEQ_INST_SeqIdNameHasSpace  1,27
#define ERR_SEQ_INST_IdOnMultipleBioseqs  1,28
#define ERR_SEQ_INST_DuplicateSegmentReferences  1,29
#define ERR_SEQ_INST_TrailingX  1,30
#define ERR_SEQ_INST_BadSeqIdFormat  1,31
#define ERR_SEQ_INST_PartsOutOfOrder  1,32
#define ERR_SEQ_INST_BadSecondaryAccn  1,33
#define ERR_SEQ_INST_ZeroGiNumber  1,34
#define ERR_SEQ_INST_RnaDnaConflict  1,35
#define ERR_SEQ_INST_HistoryGiCollision  1,36
#define ERR_SEQ_INST_GiWithoutAccession  1,37
#define ERR_SEQ_INST_MultipleAccessions  1,38
#define ERR_SEQ_INST_HistAssemblyMissing  1,39
#define ERR_SEQ_INST_TerminalNs  1,40
#define ERR_SEQ_INST_UnexpectedIdentifierChange  1,41
#define ERR_SEQ_INST_InternalNsInSeqLit  1,42
#define ERR_SEQ_INST_SeqLitGapLength0  1,43
#define ERR_SEQ_INST_TpaAssmeblyProblem  1,44
#define ERR_SEQ_INST_SeqLocLength  1,45
#define ERR_SEQ_INST_MissingGaps  1,46
#define ERR_SEQ_INST_CompleteTitleProblem  1,47
#define ERR_SEQ_INST_CompleteCircleProblem  1,48
#define ERR_SEQ_INST_BadHTGSeq  1,49
#define ERR_SEQ_INST_GapInProtein  1,50
#define ERR_SEQ_INST_BadProteinStart  1,51
#define ERR_SEQ_INST_TerminalGap  1,52
#define ERR_SEQ_INST_OverlappingDeltaRange  1,53
#define ERR_SEQ_INST_LeadingX  1,54
#define ERR_SEQ_INST_InternalNsInSeqRaw  1,55
#define ERR_SEQ_INST_InternalNsAdjacentToGap  1,56
#define ERR_SEQ_INST_CaseDifferenceInSeqID  1,57
#define ERR_SEQ_INST_DeltaComponentIsGi0  1,58
#define ERR_SEQ_INST_FarFetchFailure  1,59
#define ERR_SEQ_INST_InternalGapsInSeqRaw  1,60
#define ERR_SEQ_INST_SelfReferentialSequence  1,61
#define ERR_SEQ_INST_WholeComponent  1,62
#define ERR_SEQ_INST_TSAHistAssemblyMissing  1,63
#define ERR_SEQ_INST_ProteinsHaveGeneralID  1,64
#define ERR_SEQ_INST_HighNContent  1,65
#define ERR_SEQ_INST_SeqLitDataLength0  1,66
#define ERR_SEQ_INST_DSmRNA  1,67
#define ERR_SEQ_INST_HighNContentStretch  1,68
#define ERR_SEQ_INST_HighNContentPercent  1,69
#define ERR_SEQ_INST_BadSegmentedSeq  1,70
#define ERR_SEQ_INST_SeqLitGapFuzzNot100  1,71
#define ERR_SEQ_DESCR  2,0
#define ERR_SEQ_DESCR_BioSourceMissing  2,1
#define ERR_SEQ_DESCR_InvalidForType  2,2
#define ERR_SEQ_DESCR_FileOpenCollision  2,3
#define ERR_SEQ_DESCR_Unknown  2,4
#define ERR_SEQ_DESCR_NoPubFound  2,5
#define ERR_SEQ_DESCR_NoOrgFound  2,6
#define ERR_SEQ_DESCR_MultipleBioSources  2,7
#define ERR_SEQ_DESCR_NoMolInfoFound  2,8
#define ERR_SEQ_DESCR_BadCountryCode  2,9
#define ERR_SEQ_DESCR_NoTaxonID  2,10
#define ERR_SEQ_DESCR_InconsistentBioSources  2,11
#define ERR_SEQ_DESCR_MissingLineage  2,12
#define ERR_SEQ_DESCR_SerialInComment  2,13
#define ERR_SEQ_DESCR_BioSourceNeedsFocus  2,14
#define ERR_SEQ_DESCR_BadOrganelle  2,15
#define ERR_SEQ_DESCR_MultipleChromosomes  2,16
#define ERR_SEQ_DESCR_BadSubSource  2,17
#define ERR_SEQ_DESCR_BadOrgMod  2,18
#define ERR_SEQ_DESCR_InconsistentProteinTitle  2,19
#define ERR_SEQ_DESCR_Inconsistent  2,20
#define ERR_SEQ_DESCR_ObsoleteSourceLocation  2,21
#define ERR_SEQ_DESCR_ObsoleteSourceQual  2,22
#define ERR_SEQ_DESCR_StructuredSourceNote  2,23
#define ERR_SEQ_DESCR_UnnecessaryBioSourceFocus  2,24
#define ERR_SEQ_DESCR_RefGeneTrackingWithoutStatus  2,25
#define ERR_SEQ_DESCR_UnwantedCompleteFlag  2,26
#define ERR_SEQ_DESCR_CollidingPublications  2,27
#define ERR_SEQ_DESCR_TransgenicProblem  2,28
#define ERR_SEQ_DESCR_TaxonomyLookupProblem  2,29
#define ERR_SEQ_DESCR_MultipleTitles  2,30
#define ERR_SEQ_DESCR_RefGeneTrackingOnNonRefSeq  2,31
#define ERR_SEQ_DESCR_BioSourceInconsistency  2,32
#define ERR_SEQ_DESCR_FastaBracketTitle  2,33
#define ERR_SEQ_DESCR_MissingText  2,34
#define ERR_SEQ_DESCR_BadCollectionDate  2,35
#define ERR_SEQ_DESCR_BadPCRPrimerSequence  2,36
#define ERR_SEQ_DESCR_BadPunctuation  2,37
#define ERR_SEQ_DESCR_BadPCRPrimerName  2,38
#define ERR_SEQ_DESCR_BioSourceOnProtein  2,39
#define ERR_SEQ_DESCR_BioSourceDbTagConflict  2,40
#define ERR_SEQ_DESCR_DuplicatePCRPrimerSequence  2,41
#define ERR_SEQ_DESCR_MultipleNames  2,42
#define ERR_SEQ_DESCR_MultipleComments  2,43
#define ERR_SEQ_DESCR_LatLonProblem  2,44
#define ERR_SEQ_DESCR_LatLonFormat  2,45
#define ERR_SEQ_DESCR_LatLonRange  2,46
#define ERR_SEQ_DESCR_LatLonValue  2,47
#define ERR_SEQ_DESCR_LatLonCountry  2,48
#define ERR_SEQ_DESCR_LatLonState  2,49
#define ERR_SEQ_DESCR_BadSpecificHost  2,50
#define ERR_SEQ_DESCR_RefGeneTrackingIllegalStatus  2,51
#define ERR_SEQ_DESCR_ReplacedCountryCode  2,52
#define ERR_SEQ_DESCR_BadInstitutionCode  2,53
#define ERR_SEQ_DESCR_BadCollectionCode  2,54
#define ERR_SEQ_DESCR_BadVoucherID  2,55
#define ERR_SEQ_DESCR_UnstructuredVoucher  2,56
#define ERR_SEQ_DESCR_ChromosomeLocation  2,57
#define ERR_SEQ_DESCR_MultipleSourceQualifiers  2,58
#define ERR_SEQ_DESCR_UnbalancedParentheses  2,59
#define ERR_SEQ_DESCR_MultipleSourceVouchers  2,60
#define ERR_SEQ_DESCR_BadCountryCapitalization  2,61
#define ERR_SEQ_DESCR_WrongVoucherType  2,62
#define ERR_SEQ_DESCR_UserObjectProblem  2,63
#define ERR_SEQ_DESCR_TitleHasPMID  2,64
#define ERR_SEQ_DESCR_BadKeyword  2,65
#define ERR_SEQ_DESCR_NoOrganismInTitle  2,66
#define ERR_SEQ_DESCR_MissingChromosome  2,67
#define ERR_SEQ_DESCR_LatLonAdjacent  2,68
#define ERR_SEQ_DESCR_BadStrucCommInvalidFieldName  2,69
#define ERR_SEQ_DESCR_BadStrucCommInvalidFieldValue  2,70
#define ERR_SEQ_DESCR_BadStrucCommMissingField  2,71
#define ERR_SEQ_DESCR_BadStrucCommFieldOutOfOrder  2,72
#define ERR_SEQ_DESCR_BadStrucCommMultipleFields  2,73
#define ERR_SEQ_DESCR_BioSourceNeedsChromosome  2,74
#define ERR_SEQ_DESCR_MolInfoConflictsWithBioSource  2,75
#define ERR_SEQ_DESCR_MissingKeyword  2,76
#define ERR_SEQ_DESCR_FakeStructuredComment  2,77
#define ERR_SEQ_DESCR_StructuredCommentPrefixOrSuffixMissing  2,78
#define ERR_SEQ_DESCR_LatLonWater  2,79
#define ERR_SEQ_DESCR_LatLonOffshore  2,80
#define ERR_SEQ_DESCR_MissingPersonalCollectionName  2,81
#define ERR_SEQ_DESCR_LatLonPrecision  2,82
#define ERR_SEQ_DESCR_DBLinkProblem  2,83
#define ERR_SEQ_DESCR_FinishedStatusForWGS  2,84
#define ERR_GENERIC  3,0
#define ERR_GENERIC_NonAsciiAsn  3,1
#define ERR_GENERIC_Spell  3,2
#define ERR_GENERIC_AuthorListHasEtAl  3,3
#define ERR_GENERIC_MissingPubInfo  3,4
#define ERR_GENERIC_UnnecessaryPubEquiv  3,5
#define ERR_GENERIC_BadPageNumbering  3,6
#define ERR_GENERIC_MedlineEntryPub  3,7
#define ERR_GENERIC_BadDate  3,8
#define ERR_GENERIC_StructuredCitGenCit  3,9
#define ERR_GENERIC_CollidingSerialNumbers  3,10
#define ERR_GENERIC_EmbeddedScript  3,11
#define ERR_GENERIC_PublicationInconsistency  3,12
#define ERR_GENERIC_SgmlPresentInText  3,13
#define ERR_GENERIC_UnexpectedPubStatusComment  3,14
#define ERR_GENERIC_PastReleaseDate  3,15
#define ERR_SEQ_PKG  4,0
#define ERR_SEQ_PKG_NoCdRegionPtr  4,1
#define ERR_SEQ_PKG_NucProtProblem  4,2
#define ERR_SEQ_PKG_SegSetProblem  4,3
#define ERR_SEQ_PKG_EmptySet  4,4
#define ERR_SEQ_PKG_NucProtNotSegSet  4,5
#define ERR_SEQ_PKG_SegSetNotParts  4,6
#define ERR_SEQ_PKG_SegSetMixedBioseqs  4,7
#define ERR_SEQ_PKG_PartsSetMixedBioseqs  4,8
#define ERR_SEQ_PKG_PartsSetHasSets  4,9
#define ERR_SEQ_PKG_FeaturePackagingProblem  4,10
#define ERR_SEQ_PKG_GenomicProductPackagingProblem  4,11
#define ERR_SEQ_PKG_InconsistentMolInfoBiomols  4,12
#define ERR_SEQ_PKG_ArchaicFeatureLocation  4,13
#define ERR_SEQ_PKG_ArchaicFeatureProduct  4,14
#define ERR_SEQ_PKG_GraphPackagingProblem  4,15
#define ERR_SEQ_PKG_InternalGenBankSet  4,16
#define ERR_SEQ_PKG_ConSetProblem  4,17
#define ERR_SEQ_PKG_NoBioseqFound  4,18
#define ERR_SEQ_PKG_INSDRefSeqPackaging  4,19
#define ERR_SEQ_PKG_GPSnonGPSPackaging  4,20
#define ERR_SEQ_PKG_RefSeqPopSet  4,21
#define ERR_SEQ_PKG_BioseqSetClassNotSet  4,22
#define ERR_SEQ_PKG_OrphanedProtein  4,23
#define ERR_SEQ_PKG_MissingSetTitle  4,24
#define ERR_SEQ_PKG_NucProtSetHasTitle  4,25
#define ERR_SEQ_PKG_ComponentMissingTitle  4,26
#define ERR_SEQ_PKG_SingleItemSet  4,27
#define ERR_SEQ_PKG_MisplacedMolInfo  4,28
#define ERR_SEQ_PKG_ImproperlyNestedSets  4,29
#define ERR_SEQ_FEAT  5,0
#define ERR_SEQ_FEAT_InvalidForType  5,1
#define ERR_SEQ_FEAT_PartialProblem  5,2
#define ERR_SEQ_FEAT_InvalidType  5,3
#define ERR_SEQ_FEAT_Range  5,4
#define ERR_SEQ_FEAT_MixedStrand  5,5
#define ERR_SEQ_FEAT_SeqLocOrder  5,6
#define ERR_SEQ_FEAT_CdTransFail  5,7
#define ERR_SEQ_FEAT_StartCodon  5,8
#define ERR_SEQ_FEAT_InternalStop  5,9
#define ERR_SEQ_FEAT_NoProtein  5,10
#define ERR_SEQ_FEAT_MisMatchAA  5,11
#define ERR_SEQ_FEAT_TransLen  5,12
#define ERR_SEQ_FEAT_NoStop  5,13
#define ERR_SEQ_FEAT_TranslExcept  5,14
#define ERR_SEQ_FEAT_NoProtRefFound  5,15
#define ERR_SEQ_FEAT_NotSpliceConsensus  5,16
#define ERR_SEQ_FEAT_OrfCdsHasProduct  5,17
#define ERR_SEQ_FEAT_GeneRefHasNoData  5,18
#define ERR_SEQ_FEAT_ExceptInconsistent  5,19
#define ERR_SEQ_FEAT_ProtRefHasNoData  5,20
#define ERR_SEQ_FEAT_GenCodeMismatch  5,21
#define ERR_SEQ_FEAT_RNAtype0  5,22
#define ERR_SEQ_FEAT_UnknownImpFeatKey  5,23
#define ERR_SEQ_FEAT_UnknownImpFeatQual  5,24
#define ERR_SEQ_FEAT_WrongQualOnImpFeat  5,25
#define ERR_SEQ_FEAT_MissingQualOnImpFeat  5,26
#define ERR_SEQ_FEAT_PseudoCdsHasProduct  5,27
#define ERR_SEQ_FEAT_IllegalDbXref  5,28
#define ERR_SEQ_FEAT_FarLocation  5,29
#define ERR_SEQ_FEAT_DuplicateFeat  5,30
#define ERR_SEQ_FEAT_UnnecessaryGeneXref  5,31
#define ERR_SEQ_FEAT_TranslExceptPhase  5,32
#define ERR_SEQ_FEAT_TrnaCodonWrong  5,33
#define ERR_SEQ_FEAT_BothStrands  5,34
#define ERR_SEQ_FEAT_CDSgeneRange  5,35
#define ERR_SEQ_FEAT_CDSmRNArange  5,36
#define ERR_SEQ_FEAT_OverlappingPeptideFeat  5,37
#define ERR_SEQ_FEAT_SerialInComment  5,38
#define ERR_SEQ_FEAT_MultipleCDSproducts  5,39
#define ERR_SEQ_FEAT_FocusOnBioSourceFeature  5,40
#define ERR_SEQ_FEAT_PeptideFeatOutOfFrame  5,41
#define ERR_SEQ_FEAT_InvalidQualifierValue  5,42
#define ERR_SEQ_FEAT_MultipleMRNAproducts  5,43
#define ERR_SEQ_FEAT_mRNAgeneRange  5,44
#define ERR_SEQ_FEAT_TranscriptLen  5,45
#define ERR_SEQ_FEAT_TranscriptMismatches  5,46
#define ERR_SEQ_FEAT_CDSproductPackagingProblem  5,47
#define ERR_SEQ_FEAT_DuplicateInterval  5,48
#define ERR_SEQ_FEAT_PolyAsiteNotPoint  5,49
#define ERR_SEQ_FEAT_ImpFeatBadLoc  5,50
#define ERR_SEQ_FEAT_LocOnSegmentedBioseq  5,51
#define ERR_SEQ_FEAT_UnnecessaryCitPubEquiv  5,52
#define ERR_SEQ_FEAT_ImpCDShasTranslation  5,53
#define ERR_SEQ_FEAT_ImpCDSnotPseudo  5,54
#define ERR_SEQ_FEAT_MissingMRNAproduct  5,55
#define ERR_SEQ_FEAT_AbuttingIntervals  5,56
#define ERR_SEQ_FEAT_CollidingGeneNames  5,57
#define ERR_SEQ_FEAT_MultiIntervalGene  5,58
#define ERR_SEQ_FEAT_FeatContentDup  5,59
#define ERR_SEQ_FEAT_BadProductSeqId  5,60
#define ERR_SEQ_FEAT_RnaProductMismatch  5,61
#define ERR_SEQ_FEAT_MissingCDSproduct  5,62
#define ERR_SEQ_FEAT_BadTrnaCodon  5,63
#define ERR_SEQ_FEAT_BadTrnaAA  5,64
#define ERR_SEQ_FEAT_OnlyGeneXrefs  5,65
#define ERR_SEQ_FEAT_UTRdoesNotAbutCDS  5,66
#define ERR_SEQ_FEAT_BadConflictFlag  5,67
#define ERR_SEQ_FEAT_ConflictFlagSet  5,68
#define ERR_SEQ_FEAT_LocusTagProblem  5,69
#define ERR_SEQ_FEAT_CollidingLocusTags  5,70
#define ERR_SEQ_FEAT_AltStartCodon  5,71
#define ERR_SEQ_FEAT_PartialsInconsistent  5,72
#define ERR_SEQ_FEAT_GenesInconsistent  5,73
#define ERR_SEQ_FEAT_DuplicateTranslExcept  5,74
#define ERR_SEQ_FEAT_TranslExceptAndRnaEditing  5,75
#define ERR_SEQ_FEAT_NoNameForProtein  5,76
#define ERR_SEQ_FEAT_TaxonDbxrefOnFeature  5,77
#define ERR_SEQ_FEAT_UnindexedFeature  5,78
#define ERR_SEQ_FEAT_CDSmRNAmismatch  5,79
#define ERR_SEQ_FEAT_UnnecessaryException  5,80
#define ERR_SEQ_FEAT_LocusTagProductMismatch  5,81
#define ERR_SEQ_FEAT_MrnaTransFail  5,82
#define ERR_SEQ_FEAT_PseudoCdsViaGeneHasProduct  5,83
#define ERR_SEQ_FEAT_MissingGeneXref  5,84
#define ERR_SEQ_FEAT_FeatureCitationProblem  5,85
#define ERR_SEQ_FEAT_NestedSeqLocMix  5,86
#define ERR_SEQ_FEAT_WrongQualOnFeature  5,87
#define ERR_SEQ_FEAT_MissingQualOnFeature  5,88
#define ERR_SEQ_FEAT_CodonQualifierUsed  5,89
#define ERR_SEQ_FEAT_UnknownFeatureQual  5,90
#define ERR_SEQ_FEAT_BadCharInAuthorName  5,91
#define ERR_SEQ_FEAT_PolyATail  5,92
#define ERR_SEQ_FEAT_ProteinNameEndsInBracket  5,93
#define ERR_SEQ_FEAT_CDSwithMultipleMRNAs  5,94
#define ERR_SEQ_FEAT_MultipleEquivBioSources  5,95
#define ERR_SEQ_FEAT_MultipleEquivPublications  5,96
#define ERR_SEQ_FEAT_BadFullLengthFeature  5,97
#define ERR_SEQ_FEAT_RedundantFields  5,98
#define ERR_SEQ_FEAT_CDSwithNoMRNAOverlap  5,99
#define ERR_SEQ_FEAT_FeatureProductInconsistency  5,100
#define ERR_SEQ_FEAT_ImproperBondLocation  5,101
#define ERR_SEQ_FEAT_GeneXrefWithoutGene  5,102
#define ERR_SEQ_FEAT_SeqFeatXrefProblem  5,103
#define ERR_SEQ_FEAT_ProductFetchFailure  5,104
#define ERR_SEQ_FEAT_SuspiciousGeneXref  5,105
#define ERR_SEQ_FEAT_MissingTrnaAA  5,106
#define ERR_SEQ_FEAT_CollidingFeatureIDs  5,107
#define ERR_SEQ_FEAT_ExceptionProblem  5,108
#define ERR_SEQ_FEAT_PolyAsignalNotRange  5,109
#define ERR_SEQ_FEAT_OldLocusTagMismtach  5,110
#define ERR_SEQ_FEAT_DuplicateGeneOntologyTerm  5,111
#define ERR_SEQ_FEAT_InvalidInferenceValue  5,112
#define ERR_SEQ_FEAT_HpotheticalProteinMismatch  5,113
#define ERR_SEQ_FEAT_FeatureRefersToAccession  5,114
#define ERR_SEQ_FEAT_SelfReferentialProduct  5,115
#define ERR_SEQ_FEAT_ITSdoesNotAbutRRNA  5,116
#define ERR_SEQ_FEAT_FeatureSeqIDCaseDifference  5,117
#define ERR_SEQ_FEAT_FeatureLocationIsGi0  5,118
#define ERR_SEQ_FEAT_GapFeatureProblem  5,119
#define ERR_SEQ_FEAT_PseudoCdsHasProtXref  5,120
#define ERR_SEQ_FEAT_ErroneousException  5,121
#define ERR_SEQ_FEAT_SegmentedGeneProblem  5,122
#define ERR_SEQ_FEAT_WholeLocation  5,123
#define ERR_SEQ_FEAT_BadEcNumberFormat  5,124
#define ERR_SEQ_FEAT_BadEcNumberValue  5,125
#define ERR_SEQ_FEAT_EcNumberProblem  5,126
#define ERR_SEQ_FEAT_VectorContamination  5,127
#define ERR_SEQ_FEAT_MinusStrandProtein  5,128
#define ERR_SEQ_FEAT_BadProteinName  5,129
#define ERR_SEQ_FEAT_GeneXrefWithoutLocus  5,130
#define ERR_SEQ_FEAT_UTRdoesNotExtendToEnd  5,131
#define ERR_SEQ_FEAT_CDShasTooManyXs  5,132
#define ERR_SEQ_FEAT_SuspiciousFrame  5,133
#define ERR_SEQ_FEAT_TerminalXDiscrepancy  5,134
#define ERR_SEQ_FEAT_UnnecessaryTranslExcept  5,135
#define ERR_SEQ_FEAT_SuspiciousQualifierValue  5,136
#define ERR_SEQ_FEAT_NotSpliceConsensusDonor  5,137
#define ERR_SEQ_FEAT_NotSpliceConsensusAcceptor  5,138
#define ERR_SEQ_FEAT_RareSpliceConsensusDonor  5,139
#define ERR_SEQ_FEAT_SeqFeatXrefNotReciprocal  5,140
#define ERR_SEQ_FEAT_SeqFeatXrefFeatureMissing  5,141
#define ERR_SEQ_FEAT_FeatureInsideGap  5,142
#define ERR_SEQ_FEAT_FeatureCrossesGap  5,143
#define ERR_SEQ_FEAT_BadAuthorSuffix  5,144
#define ERR_SEQ_FEAT_BadAnticodonAA  5,145
#define ERR_SEQ_FEAT_BadAnticodonCodon  5,146
#define ERR_SEQ_FEAT_BadAnticodonStrand  5,147
#define ERR_SEQ_FEAT_UndesiredGeneSynonym  5,148
#define ERR_SEQ_FEAT_UndesiredProteinName  5,149
#define ERR_SEQ_FEAT_FeatureBeginsOrEndsInGap  5,150
#define ERR_SEQ_FEAT_GeneOntologyTermMissingGOID  5,151
#define ERR_SEQ_FEAT_PseudoRnaHasProduct  5,152
#define ERR_SEQ_FEAT_PseudoRnaViaGeneHasProduct  5,153
#define ERR_SEQ_FEAT_BadRRNAcomponentOrder  5,154
#define ERR_SEQ_FEAT_BadRRNAcomponentOverlap  5,155
#define ERR_SEQ_FEAT_MissingGeneLocusTag  5,156
#define ERR_SEQ_FEAT_MultipleProtRefs  5,157
#define ERR_SEQ_FEAT_BadInternalCharacter  5,158
#define ERR_SEQ_FEAT_BadTrailingCharacter  5,159
#define ERR_SEQ_FEAT_BadTrailingHyphen  5,160
#define ERR_SEQ_FEAT_MultipleGeneOverlap  5,161
#define ERR_SEQ_FEAT_BadCharInAuthorLastName  5,162
#define ERR_SEQ_FEAT_PseudoCDSmRNArange  5,163
#define ERR_SEQ_FEAT_ExtendablePartialProblem  5,164
#define ERR_SEQ_FEAT_GeneXrefNeeded  5,165
#define ERR_SEQ_FEAT_RubiscoProblem  5,166
#define ERR_SEQ_FEAT_UnqualifiedException  5,167
#define ERR_SEQ_FEAT_ProteinNameHasPMID  5,168
#define ERR_SEQ_FEAT_BadGeneOntologyFormat  5,169
#define ERR_SEQ_FEAT_InconsistentGeneOntologyTermAndId  5,170
#define ERR_SEQ_FEAT_MultiplyAnnotatedGenes  5,171
#define ERR_SEQ_FEAT_ReplicatedGeneSequence  5,172
#define ERR_SEQ_FEAT_ShortIntron  5,173
#define ERR_SEQ_FEAT_GeneXrefStrandProblem  5,174
#define ERR_SEQ_FEAT_CDSmRNAXrefLocationProblem  5,175
#define ERR_SEQ_FEAT_LocusCollidesWithLocusTag  5,176
#define ERR_SEQ_FEAT_IdenticalGeneSymbolAndSynonym  5,177
#define ERR_SEQ_FEAT_NeedsNote  5,178
#define ERR_SEQ_FEAT_RptUnitRangeProblem  5,179
#define ERR_SEQ_FEAT_TooManyInferenceAccessions  5,180
#define ERR_SEQ_FEAT_IntervalBeginsOrEndsInGap  5,181
#define ERR_SEQ_ALIGN  6,0
#define ERR_SEQ_ALIGN_SeqIdProblem  6,1
#define ERR_SEQ_ALIGN_StrandRev  6,2
#define ERR_SEQ_ALIGN_DensegLenStart  6,3
#define ERR_SEQ_ALIGN_StartLessthanZero  6,4
#define ERR_SEQ_ALIGN_StartMorethanBiolen  6,5
#define ERR_SEQ_ALIGN_EndLessthanZero  6,6
#define ERR_SEQ_ALIGN_EndMorethanBiolen  6,7
#define ERR_SEQ_ALIGN_LenLessthanZero  6,8
#define ERR_SEQ_ALIGN_LenMorethanBiolen  6,9
#define ERR_SEQ_ALIGN_SumLenStart  6,10
#define ERR_SEQ_ALIGN_AlignDimSeqIdNotMatch  6,11
#define ERR_SEQ_ALIGN_SegsDimSeqIdNotMatch  6,12
#define ERR_SEQ_ALIGN_FastaLike  6,13
#define ERR_SEQ_ALIGN_NullSegs  6,14
#define ERR_SEQ_ALIGN_SegmentGap  6,15
#define ERR_SEQ_ALIGN_SegsDimOne  6,16
#define ERR_SEQ_ALIGN_AlignDimOne  6,17
#define ERR_SEQ_ALIGN_Segtype  6,18
#define ERR_SEQ_ALIGN_BlastAligns  6,19
#define ERR_SEQ_ALIGN_PercentIdentity  6,20
#define ERR_SEQ_ALIGN_ShortAln  6,21
#define ERR_SEQ_ALIGN_UnexpectedAlignmentType  6,22
#define ERR_SEQ_GRAPH  7,0
#define ERR_SEQ_GRAPH_GraphMin  7,1
#define ERR_SEQ_GRAPH_GraphMax  7,2
#define ERR_SEQ_GRAPH_GraphBelow  7,3
#define ERR_SEQ_GRAPH_GraphAbove  7,4
#define ERR_SEQ_GRAPH_GraphByteLen  7,5
#define ERR_SEQ_GRAPH_GraphOutOfOrder  7,6
#define ERR_SEQ_GRAPH_GraphBioseqLen  7,7
#define ERR_SEQ_GRAPH_GraphSeqLitLen  7,8
#define ERR_SEQ_GRAPH_GraphSeqLocLen  7,9
#define ERR_SEQ_GRAPH_GraphStartPhase  7,10
#define ERR_SEQ_GRAPH_GraphStopPhase  7,11
#define ERR_SEQ_GRAPH_GraphDiffNumber  7,12
#define ERR_SEQ_GRAPH_GraphACGTScore  7,13
#define ERR_SEQ_GRAPH_GraphNScore  7,14
#define ERR_SEQ_GRAPH_GraphGapScore  7,15
#define ERR_SEQ_GRAPH_GraphOverlap  7,16
#define ERR_SEQ_GRAPH_GraphBioseqId  7,17
#define ERR_SEQ_GRAPH_GraphACGTScoreMany  7,18
#define ERR_SEQ_GRAPH_GraphNScoreMany  7,19
#define ERR_SEQ_GRAPH_GraphLocInvalid  7,20
#define ERR_SEQ_ANNOT  8,0
#define ERR_SEQ_ANNOT_AnnotIDs  8,1
#define ERR_SEQ_ANNOT_AnnotLOCs  8,2

#endif
