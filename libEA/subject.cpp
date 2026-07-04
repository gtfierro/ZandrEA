//XXXXXXX1XXXXXXXXX2XXXXXXXXX3XXXXXXXXX4XXXXXXXXX5XXXXXXXXX6XXXXXXXXX7XXXXXXXXX8XXXXXXXXX9XXXXXXXXXCXXXXV
/* Source code file to an "EA" part of the ZandrEA (tm) project at: https://github.com/usnistgov/ZandrEA
This file last edited in base repo by: DAV, U.S. National Institute of Standards and Technology (NIST).
As a Work of the United States Government, this file is not subject to copyright within the United
States. For other countries, Copyright 2025-2026 National Institute of Standards and Technology.
For countries other than the United States, this file is licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy
of the License at: https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and limitations under the License. */
//XXXXXXX1XXXXXXXXX2XXXXXXXXX3XXXXXXXXX4XXXXXXXXX5XXXXXXXXX6XXXXXXXXX7XXXXXXXXX8XXXXXXXXX9XXXXXXXXXCXXXXV
/* File summary:
   Implementation of classes for Subject objects and for Domain object (one Domain per Application)
*/
/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8/////////9/////////C////V

#include "subject.hpp"
#include "mvc_view.hpp"    // self-register to View
#include "case.hpp"        // hits on CCaseKit methods

#include <utility>         // needed for swap
#include <sstream>
#include <iomanip>
#include <iterator>
#include <algorithm>       // find
#include <numeric>         // accumulate
#include <functional>      // plus
#include <cctype>

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8/////////9/////////C/////
// Implementation of abstract base class for all Subject objects

// ownLabel is the compiled equipment/model identity (for example,
// Subject_vav_pressIndep_hwReheat).  Export it as text so REST clients can
// key on the model without depending on libEA's internal enum type.
static std::string SubjectLabelId( EDataLabel label ) {

   switch( label ) {
      case EDataLabel::Subject_ahu_singleDuct_vavReheat:
         return "Subject_ahu_singleDuct_vavReheat";
      case EDataLabel::Subject_vav_pressIndep_hwReheat:
         return "Subject_vav_pressIndep_hwReheat";
      case EDataLabel::Subject_chlr_ibal:
         return "Subject_chlr_ibal";
      case EDataLabel::Subject_chwPlant_ibal:
         return "Subject_chwPlant_ibal";
      case EDataLabel::Subject_hwPlant_ibal:
         return "Subject_hwPlant_ibal";
      case EDataLabel::Subject_tes_ibal:
         return "Subject_tes_ibal";
      default:
         return "Undefined";
   }
}

static std::string LegacySubjectKey( ERealName name ) {
   // Legacy subjects still need a stable string key so Domain/View code can use
   // one identity table for both old enum-backed and new RDF-backed subjects.
   return "legacy:" + std::to_string( static_cast<unsigned int>( name ) );
}

static std::string DiskFileStemFromNameText( const std::string& nameText ) {
   // Dynamic S223 subjects do not have a LookUpDiskFile() enum entry.  Derive a
   // conservative filename stem from zea:name for case/report persistence.
   std::string stem;
   stem.reserve( nameText.size() + 1 );

   for ( const unsigned char ch : nameText ) {
      if ( std::isalnum( ch ) ) {
         stem.push_back( static_cast<char>( std::tolower( ch ) ) );
      } else if ( !stem.empty() && stem.back() != '_' ) {
         stem.push_back( '_' );
      }
   }

   if ( stem.empty() ) {
      stem = "subject";
   } else if ( stem.back() != '_' ) {
      stem.push_back( '_' );
   }

   return stem;
}

ASubject::ASubject(  EUnitSystem arg0,
                     CDomain& arg1,
                     EDataLabel arg2,
                     ERealName arg3 )
                     :  IGuiShadow( EApiType::Subject ),
                        pinnedRuleFailHistories_byRuleKit(),
                        infoText(0),
                        featureKeys(0),
                        knobKeys(0),
                        ruleKitDisplayKeys(0),
                        uaiInUse_features(0),
                        sgiInUse_ruleKits(0),
                        unitSys (arg0),
                        ownLabel (arg2),
                        ownName(arg3),
                        ownSubjectKey( LegacySubjectKey( arg3 ) ),
                        ownNameText( LookUpText( arg3 ) ),
                        ownDiskFileStem( LookUpDiskFile( arg3 ) ),
                        nextSgiForCases (1u),
                        nextSgiForRuleKits (1u),
                        unitOutputOkay (true),
                        DomainRef (arg1),
                        u_CaseKit ( std::make_unique<CCaseKit>(
                                       *this,
                                       arg1.SayEnergyPricesRef(),
                                       arg1.SayRootTextForDiskFilenames() + ownDiskFileStem )
                        ) {
// empty ABC c-tor
}

// Dynamic subject constructor used by the S223 startup path.  The subject key is
// normally the RDF resource IRI; nameText is user-facing display text from the
// model, normally zea:name.
ASubject::ASubject(  EUnitSystem arg0,
                     CDomain& arg1,
                     EDataLabel arg2,
                     std::string subjectKey,
                     std::string nameText )
                     :  IGuiShadow( EApiType::Subject ),
                        pinnedRuleFailHistories_byRuleKit(),
                        infoText(0),
                        featureKeys(0),
                        knobKeys(0),
                        ruleKitDisplayKeys(0),
                        uaiInUse_features(0),
                        sgiInUse_ruleKits(0),
                        unitSys (arg0),
                        ownLabel (arg2),
                        ownName(ERealName::Undefined),
                        ownSubjectKey( std::move( subjectKey ) ),
                        ownNameText( std::move( nameText ) ),
                        ownDiskFileStem( DiskFileStemFromNameText( ownNameText ) ),
                        nextSgiForCases (1u),
                        nextSgiForRuleKits (1u),
                        unitOutputOkay (true),
                        DomainRef (arg1),
                        u_CaseKit ( std::make_unique<CCaseKit>(
                                       *this,
                                       arg1.SayEnergyPricesRef(),
                                       arg1.SayRootTextForDiskFilenames() + ownDiskFileStem )
                        ) {
   if ( ownSubjectKey.empty() || ownNameText.empty() ) {
      throw std::logic_error( "Dynamic Subject identity requires non-empty key and name" );
   }
}


ASubject::~ASubject( void ) { }


//VVVVVVV1VVVVVVVVV2VVVVVVVVV3VVVVVVVVV4VVVVVVVVV5VVVVVVVVV6VVVVVVVVV7VVVVVVVVV8VVVVVVVVV9VVVVVVVVVCVVVVV
// Public methods of ASubject interface

GuiPackSubjectBasic_t ASubject::SayBasicGuiPack( void ) const {

   // *** TBD to include Subject's const params as additional 'infoLines'
   // This pack is the REST layer's subject metadata source: model id
   // (ownLabel), display name (ownName), display label (ownLabel tag), and
   // child object keys all cross the libEA/API boundary here.

   return SGuiPackSubjectBasic(  LookUpGuiType( ownApiType ),
                                 ownGuiKey,
                                 DomainRef.SayGuiKey(),
                                 SubjectLabelId( ownLabel ),
                                 ownNameText,
                                 std::vector<std::string>( 1, LookUpTag( ownLabel ) ),
                                 featureKeys,
                                 knobKeys,
                                 ruleKitDisplayKeys
   );
}


GuiPackSubjectCases_t ASubject::SayCurrentCases( void ) const {

   return ( SGuiPackSubjectCases(   u_CaseKit->SayCaseKeysByDecrRank(),
                                    u_CaseKit->SayCaseNamesByDecrRank() )
   );
}


const SEnergyPrices& ASubject::SayEnergyPricesRef( void ) const { return DomainRef.SayEnergyPricesRef(); }


Nzint_t ASubject::GenerateAndSaySgiForNewCase( void ) {

   // Cannot be class-static; need distinct S/N progression for each object of an ISubject subclass
   return ( ( nextSgiForCases < 255u ) ? nextSgiForCases++ : 1u );
}


std::string ASubject::SayRootTextForDiskFilenames( void ) const {

   return ( DomainRef.SayRootTextForDiskFilenames() + ownDiskFileStem );
}


std::string ASubject::SayNumRuleKitsAsText( void ) const {

   return std::to_string( ruleKitDisplayKeys.size() );
}


std::string ASubject::SayDomainAndOwnNameAsText( void ) const {

   return ( LookUpText( DomainRef.SayName() ) + ": " + ownNameText ) ;
}


std::string ASubject::SayNameAsText( void ) const {

   return ownNameText;
}

std::string ASubject::SaySubjectKey( void ) const { return ownSubjectKey; }


EDataLabel ASubject::SayLabel( void ) const { return ownLabel; }


ERealName ASubject::SayName( void ) const { return ownName; }


CCaseKit& ASubject::SayCaseKitRef( void ) const { return *u_CaseKit; }


CView& ASubject::SayViewRef( void ) const { return *(DomainRef.SayViewPtr()); }


Nzint_t ASubject::GenerateAndSaySgiForNewRuleKit( void ) {

   Nzint_t newRuleKitSgi = nextSgiForRuleKits++;  // post-increment for next use

   sgiInUse_ruleKits.push_back( newRuleKitSgi );

   std::unique_ptr<std::deque<int> > u_historyBuffer_locallyScopedToHeap =
      std::make_unique< std::deque<int> >( FIXED_SUBJECT_CYCLESPINNEDFAILHISTORY, 0 ); 

   std::pair<IndexedFifoBuffers_t::iterator, bool> bufferVerbReply =
      pinnedRuleFailHistories_byRuleKit.emplace(
         std::pair<Nzint_t, std::unique_ptr<std::deque<int>> >(
            newRuleKitSgi,
            u_historyBuffer_locallyScopedToHeap.release() )  // xfers ownership from and nulls local ptr
   );
   return newRuleKitSgi;
}   // destroys nulled local unique ptr


bool ASubject::IsUnitOutputOkay( void ) const { return unitOutputOkay; }


void ASubject::SubmitTrueIfGotFailOnPinnedRule( Nzint_t sgiOfKitReporting, bool gotFailOnPinnedRule ) {

   pinnedRuleFailHistories_byRuleKit[sgiOfKitReporting]->push_back( (gotFailOnPinnedRule ? 1 : 0) );
   pinnedRuleFailHistories_byRuleKit[sgiOfKitReporting]->pop_front();

   // "okay" means no fail from any Rule pinned to unit output in any of the Subject's rule kits
   // being marked "pinned" (or not) to unit ouput is a part of each Rule object's definition

   int countPinnedRuleFailOccurencesInHistory_allKits = 0;

   for (auto sgi : sgiInUse_ruleKits ) {

      countPinnedRuleFailOccurencesInHistory_allKits +=
         std::accumulate(  pinnedRuleFailHistories_byRuleKit[sgi]->begin(),
                           pinnedRuleFailHistories_byRuleKit[sgi]->end(),
                           0,
                           std::plus<int>()
         );
   }
   unitOutputOkay = ( countPinnedRuleFailOccurencesInHistory_allKits == 0 );
   return;
}


void ASubject::CheckFeatureUaiFreeThenKeep( Nzint_t proposedUserAssignedIdentifier ) {

   // currently reqd only for Feature, but later on could include histograms, etc.

   if (  std::find(  uaiInUse_features.begin(),
                     uaiInUse_features.end(),
                     proposedUserAssignedIdentifier )
          == uaiInUse_features.end() ) {

      uaiInUse_features.push_back( proposedUserAssignedIdentifier );
   }
   else { throw std::logic_error( "GUI Feature given UAI already in use" ); }
   return;
}


void ASubject::AddRuleKitDisplay( NGuiKey kitDisplayKey ) {

   ruleKitDisplayKeys.push_back( kitDisplayKey);
   return;
}



void ASubject::AddFeatureKey( NGuiKey keyGiven ) { featureKeys.push_back( keyGiven ); return; }


void ASubject::TagAndPostAlertToDomain(   time_t timestamp,
                                          EDataLabel sourceLabel,
                                          EAlertMsg alertFromSource ) {

   // end-to-end P.B.V. as presuming chain of copy elision by compiler whenever optimal
   DomainRef.PostAsNewAlert( timestamp, ownNameText, sourceLabel, alertFromSource );
   return;
 }


/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8/////////9/////////C/////

CSubj_vav_ibal::CSubj_vav_ibal(  EUnitSystem bArg0,
                                 CDomain& bArg1,
                                 EDataLabel bArg2, // own label
                                 ERealName bArg3,  // own name
                                 ERealName arg0,   // AHU name
                                 ERealName arg1,   // Reheat source name (electric or HW plant sim)
                                 float arg2,       // duct diameter
                                 float arg3,       // duct area
                                 float arg4,       // air flow, rated
                                 float arg5,       // reheat HW flow, rated (zero if electric RH)
                                 float arg6 )      // kW reheat, rated (zero if RH by HW plant sim)
                                 :  ASubject(bArg0,
                                             bArg1,
                                             bArg2,
                                             bArg3
                                    ),
                                    nameAntecedentAhu (arg0),
                                    keyAntecedentAhu (LegacySubjectKey( arg0 )),
                                    nameAntecedentHwPlant (arg1),
                                    diamDuct (arg2),
                                    areaAirflow ( arg3),
                                    airFlowRated (arg4),
                                    hwFlowRated (arg5),
                                    kWReheatRated (arg6) {

   bArg1.Register( this, ownName );
}

CSubj_vav_ibal::CSubj_vav_ibal(  EUnitSystem bArg0,
                                 CDomain& bArg1,
                                 EDataLabel bArg2,
                                 std::string subjectKey,
                                 std::string nameText,
                                 std::string ahuSubjectKey,
                                 ERealName arg1,
                                 float arg2,
                                 float arg3,
                                 float arg4,
                                 float arg5,
                                 float arg6 )
                                 :  ASubject(bArg0,
                                             bArg1,
                                             bArg2,
                                             std::move( subjectKey ),
                                             std::move( nameText )
                                    ),
                                    nameAntecedentAhu (ERealName::Undefined),
                                    keyAntecedentAhu (std::move( ahuSubjectKey )),
                                    nameAntecedentHwPlant (arg1),
                                    diamDuct (arg2),
                                    areaAirflow ( arg3),
                                    airFlowRated (arg4),
                                    hwFlowRated (arg5),
                                    kWReheatRated (arg6) {

   // Register only by dynamic key; ownName is deliberately Undefined for RDF
   // instances so they cannot collide with the fixed legacy enum slots.
   bArg1.Register( this, ownSubjectKey );
}


CSubj_vav_ibal::~CSubj_vav_ibal( void ) {

   // empty
}

ERealName CSubj_vav_ibal::SayNameOfAntecedentAhu( void ) const { return nameAntecedentAhu; }

ERealName CSubj_vav_ibal::SayNameOfAntecedentHwPlant( void ) const { return nameAntecedentHwPlant; }

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8/////////9/////////C/////

CSubj_ahu_ibal::CSubj_ahu_ibal(  EUnitSystem bArg0,
                                 CDomain& bArg1,
                                 EDataLabel bArg2,          // own label
                                 ERealName bArg3,           // own name
                                 ERealName arg0,            // CHW plant name
                                 ERealName arg1,            // preheat source name
                                 float arg2,                // air flow, rated
                                 float arg3,                // chw flow, rated
                                 float arg4,                // kW preheat, rated
                                 float arg5 )               // min fraction OA
                                 :  ASubject(bArg0,
                                             bArg1,
                                             bArg2,
                                             bArg3
                                    ),
                                    nameAntecedentChwPlant (arg0),
                                    nameAntecedentHwPlant (arg1),
                                    airFlowRated (arg2),
                                    chwFlowRated (arg3),
                                    kwPreheatRated (arg4),
                                    minFracOA (arg5) {

   bArg1.Register( this, ownName );
}

CSubj_ahu_ibal::CSubj_ahu_ibal(  EUnitSystem bArg0,
                                 CDomain& bArg1,
                                 EDataLabel bArg2,
                                 std::string subjectKey,
                                 std::string nameText,
                                 ERealName arg0,
                                 ERealName arg1,
                                 float arg2,
                                 float arg3,
                                 float arg4,
                                 float arg5 )
                                 :  ASubject(bArg0,
                                             bArg1,
                                             bArg2,
                                             std::move( subjectKey ),
                                             std::move( nameText )
                                    ),
                                    nameAntecedentChwPlant (arg0),
                                    nameAntecedentHwPlant (arg1),
                                    airFlowRated (arg2),
                                    chwFlowRated (arg3),
                                    kwPreheatRated (arg4),
                                    minFracOA (arg5) {

   // Register only by dynamic key; this lets the model create any number of
   // AHUs without extending ERealName.
   bArg1.Register( this, ownSubjectKey );
}


CSubj_ahu_ibal::~CSubj_ahu_ibal( void ) {

   // empty
}

ERealName CSubj_ahu_ibal::SayNameOfAntecedentChwPlant( void ) const { return nameAntecedentChwPlant; }

ERealName CSubj_ahu_ibal::SayNameOfAntecedentHwPlant( void ) const { return nameAntecedentHwPlant; }


/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8/////////9/////////C/////

CSubj_chlr_ibal::CSubj_chlr_ibal(   EUnitSystem bArg0,
                                    CDomain& bArg1,
                                    EDataLabel bArg2, //own
                                    ERealName bArg3,  // own
                                    ERealName arg0,   // of antecdent cooling tower (CT) or heat sink
                                    float arg1,       // kw, rated ( = 3.517 x tons)
                                    float arg2,       // evap CHW flow, rated
                                    float arg3 )      // cond CTW flow, rated
                                    :  ASubject(   bArg0,
                                                   bArg1,
                                                   bArg2,
                                                   bArg3
                                    ),
                                    nameAntecedentCT (arg0),
                                    kWRated (arg1),
                                    evapFlowRated (arg2),
                                    condFlowRated (arg3) {

   bArg1.Register( this, ownName );
}


CSubj_chlr_ibal::~CSubj_chlr_ibal( void ) {

   // empty
}

ERealName CSubj_chlr_ibal::SayNameOfAntecedentCT( void ) const { return nameAntecedentCT; }


/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8/////////9/////////C/////

CSubj_tes_ibal::CSubj_tes_ibal(  EUnitSystem bArg0,
                                 CDomain& bArg1,
                                 EDataLabel bArg2, //own
                                 ERealName bArg3,  // own
                                 ERealName arg0,   // of antecdent chw plant (primary loop)
                                 float arg2,       // kw-hours, rated ( = 3.517 x ton-hours)
                                 float arg3 )      // CHW flow, rated
                                 :  ASubject(bArg0,
                                             bArg1,
                                             bArg2,
                                             bArg3
                                    ),
                                    nameAntecedentChwPlant (arg0),
                                    kWhRated (arg2),
                                    tubeFlowRated (arg3) {

   bArg1.Register( this, ownName );
}


CSubj_tes_ibal::~CSubj_tes_ibal( void ) {

   // empty
}

ERealName CSubj_tes_ibal::SayNameOfAntecedentChwPlant( void ) const { return nameAntecedentChwPlant; }

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8/////////9/////////C/////

CSubj_chwp_ibal::CSubj_chwp_ibal(   EUnitSystem bArg0,
                                    CDomain& bArg1,
                                    EDataLabel bArg2, //own
                                    ERealName bArg3,  // own
                                    ERealName arg0,   // of antecdent chiller #1
                                    ERealName arg1,   // of antecdent chiller #2
                                    float arg2,       // total plant kW, rated ( = 3.517 x tons)
                                    float arg3 )      // primary loop flow, rated
                                    :  ASubject(   bArg0,
                                                   bArg1,
                                                   bArg2,
                                                   bArg3
                                    ),
                                    nameAntecedentChlrOne (arg0),
                                    nameAntecedentChlrTwo (arg1),
                                    plantkWRated (arg2),
                                    loopFlowRated (arg3) {

   bArg1.Register( this, ownName );
}


CSubj_chwp_ibal::~CSubj_chwp_ibal( void ) {

   // empty
}

ERealName CSubj_chwp_ibal::SayNameOfAntecedentChlrOne( void ) const { return nameAntecedentChlrOne; }
ERealName CSubj_chwp_ibal::SayNameOfAntecedentChlrTwo( void ) const { return nameAntecedentChlrTwo; }

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8/////////9/////////C/////
//CDomain implementation

CDomain::CDomain( ERealName arg )
                  :  IGuiShadow( EApiType::Domain ),
                     p_SubjOutputs_byName_byLabel(),
                     p_Subjects_byName(),
                     p_Subjects_byKey(),
                     p_View (nullptr),
                     unsaidAlertsFifo(),
                     energyPrices( SEnergyPrices(0, 0, 0, 0, 0) ),
                     domainName (arg) {
}

CDomain::~CDomain( void ) {

}


std::queue<std::string> CDomain::SayNewAlertsFifoThenClear( void ) {

   std::queue<std::string> reply;         // constructs empty

   std::swap( unsaidAlertsFifo, reply );  // exchanges values between left and right arguments

   return reply;
}


std::vector<NGuiKey>  CDomain::SaySubjectKeys( void ) const {

   std::vector<NGuiKey> reply(0);

   for ( const auto& cr_pair : p_Subjects_byKey ) {

      reply.push_back( cr_pair.second->SayGuiKey() );
   }
   return reply;
}


GuiPackDomain_t CDomain::SayGuiPack( void ) const {

   return ( SGuiPackDomain(   ownGuiKey,
                              LookUpGuiType(ownApiType),
                              LookUpText(domainName),
                              SaySubjectKeys()
            )
   );
}


const SEnergyPrices& CDomain::SayEnergyPricesRef( void ) const { return energyPrices; }


std::string CDomain::SayRootTextForDiskFilenames( void ) const {

   return LookUpDiskFile( domainName );
}


ERealName CDomain::SayName( void ) const { return domainName; }


const ASubject* const CDomain::SayPtrToSubjectNamed( ERealName nameGiven ) const {

   auto iter = p_Subjects_byName.find( nameGiven );
   ASubject* reply = (  iter == p_Subjects_byName.end() ?
                        nullptr :
                        iter->second
   );
   return reply;
}                     
 
const ASubject* const CDomain::SayPtrToSubjectKey( const std::string& keyGiven ) const {
   // Dynamic antecedent lookup.  S223-created tools use RDF resource IRIs here.

   auto iter = p_Subjects_byKey.find( keyGiven );
   ASubject* reply = (  iter == p_Subjects_byKey.end() ?
                        nullptr :
                        iter->second
   );
   return reply;
}


CView* const CDomain::SayViewPtr( void ) const {

   if ( p_View == nullptr ) throw std::logic_error( "Asked for Domain View while null" );
   return p_View; }


void CDomain::Register( ASubject* const p_subj, ERealName subjName ) {

   if ( p_View == nullptr ) throw std::logic_error( "Attempted adding Subject to Domain prior to View" );

   p_Subjects_byName.insert( std::make_pair(subjName, p_subj ) );
   // Also register legacy subjects in the string-key table so downstream code
   // can use SaySubjectKey() without knowing which construction path was used.
   p_Subjects_byKey.insert( std::make_pair(p_subj->SaySubjectKey(), p_subj ) );

   p_View->AddSubject( *p_subj );

   return;
}

void CDomain::Register( ASubject* const p_subj, const std::string& subjKey ) {
   // S223 dynamic registration: the key is supplied by the model, usually the
   // equipment RDF resource IRI.

   if ( p_View == nullptr ) throw std::logic_error( "Attempted adding Subject to Domain prior to View" );

   p_Subjects_byKey.insert( std::make_pair(subjKey, p_subj ) );

   p_View->AddSubject( *p_subj );

   return;
}


void CDomain::Register( CView* const arg ) { p_View = arg; return; }


void CDomain::PostAsNewAlert( time_t timestamp,
                              ERealName forwardingSubjectsName,
                              EDataLabel sourceLabel,
                              EAlertMsg alertFromSource ) {

   std::string timeAsText("");
   WriteTimestampAsTextTo( timestamp, timeAsText );

   unsaidAlertsFifo.push(  timeAsText + " " +
                           LookUpText( domainName ) + " " +
                           LookUpText( forwardingSubjectsName ) + " " +
                           LookUpTag( sourceLabel ) + " " +
                           LookUpText( alertFromSource )
   );
   return;
}

void CDomain::PostAsNewAlert( time_t timestamp,
                              const std::string& forwardingSubjectsName,
                              EDataLabel sourceLabel,
                              EAlertMsg alertFromSource ) {

   std::string timeAsText("");
   WriteTimestampAsTextTo( timestamp, timeAsText );

   unsaidAlertsFifo.push(  timeAsText + " " +
                           LookUpText( domainName ) + " " +
                           forwardingSubjectsName + " " +
                           LookUpTag( sourceLabel ) + " " +
                           LookUpText( alertFromSource )
   );
   return;
}

void CDomain::SetS223StartupModel( S223ApplicationStartupModel arg ) {
   s223StartupModel = std::move( arg );
   return;
}

const std::optional<S223ApplicationStartupModel>& CDomain::SayS223StartupModel( void ) const {
   return s223StartupModel;
}

//END-OF-FILE ZZZZZ2ZZZZZZZZZ3ZZZZZZZZZ4ZZZZZZZZZ5ZZZZZZZZZ6ZZZZZZZZZ7ZZZZZZZZZ8ZZZZZZZZZ9ZZZZZZZZZCZZZZZ
