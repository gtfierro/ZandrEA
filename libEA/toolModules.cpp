//XXXXXXX1XXXXXXXXX2XXXXXXXXX3XXXXXXXXX4XXXXXXXXX5XXXXXXXXX6XXXXXXXXX7XXXXXXXXX8XXXXXXXXX9XXXXXXXXXCXXXXV
/* File summary:
   Built-in analysis modules for the model-driven tool-contract entrypoint.  Each
   module declares its ModuleManifest: role -> EPointName for the points the model
   must supply, plus antecedent roles.  The existing CTool_* constructors build the
   point objects and the whole FDD graph; the manifest does not duplicate their
   unit/range/label attributes.

   Module role names must match the zea:roleName literals in
   EAd/tests/testdata/zea-profiles.ttl; the assembler reconciles the two.
*/
/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8/////////9/////////C////V

#include "analysisModule.hpp"

namespace {

// The profile shape IRIs are written as literals so the registry keys stay
// independent of global initialization order.

PointRoleSpec Analog(const char* role, EPointName pointName) {
   return { RoleId{ role }, RoleValueKind::Analog, pointName, true };
}

PointRoleSpec Binary(const char* role, EPointName pointName) {
   return { RoleId{ role }, RoleValueKind::Binary, pointName, true };
}

//======================================================================================================/
// AHU (single-duct, VAV-reheat) — role -> EPointName. The existing
// CTool_ahu_ibal constructor builds the point objects and the whole FDD graph;
// this manifest only names the points the model must supply and the antecedents.

class CAhuModule : public IAnalysisModule {
   public:
      const ModuleManifest& Manifest(void) const override {
         static const ModuleManifest manifest = {
            "urn:zandrea:tool-profile#AhuProfile",
            "ahu_ibal",
            {
               Analog("supplyStaticPressure", EPointName::Pressure_static_air_supply),
               Analog("outsideAirTemp",       EPointName::Temperature_air_outside),
               Analog("mixingDamperCmd",      EPointName::Position_damper_mixingBox),
               Analog("mixedAirTemp",         EPointName::Temperature_air_mixed),
               Analog("returnAirTemp",        EPointName::Temperature_air_return),
               Analog("chwValveCmd",          EPointName::Position_valve_chw),
               Analog("supplyAirTemp",        EPointName::Temperature_air_supply),
               Analog("supplyAirTempSetpt",   EPointName::Temperature_air_supply_setpt),
               Binary("systemOccupied",       EPointName::Binary_systemOccupied),
               Analog("hwValveCmd",           EPointName::Position_valve_hw),
               Analog("supplyAirFlow",        EPointName::FlowRateVolume_air_ahu),
            },
            { /* no required antecedents; CHW/HW plants are optional */ }
         };
         return manifest;
      }
};

//======================================================================================================/
// VAV (single-duct, HW reheat) — mirrors CTool_vav_ibal's point objects.

class CVavModule : public IAnalysisModule {
   public:
      const ModuleManifest& Manifest(void) const override {
         static const ModuleManifest manifest = {
            "urn:zandrea:tool-profile#VavProfile",
            "vav_ibal",
            {
               Analog("inletStaticPressure", EPointName::Pressure_static_air_supply),
               Analog("inletAirTemp",        EPointName::Temperature_air_supply),
               Analog("dischargeAirTemp",    EPointName::Temperature_air_discharge),
               Analog("zoneAirTemp",         EPointName::Temperature_air_zone),
               Analog("zoneTempSetptHtg",    EPointName::Temperature_air_zone_setpt_htg),
               Analog("zoneTempSetptClg",    EPointName::Temperature_air_zone_setpt_clg),
               Analog("hwValveCmd",          EPointName::Position_valve_hw),
               Analog("damperCmd",           EPointName::Position_damper_vav),
               Analog("airFlow",             EPointName::FlowRateVolume_air_vav),
               Analog("airFlowSetpt",        EPointName::FlowRateVolume_air_vav_setpt),
               Binary("zoneOccupied",        EPointName::Binary_zoneOccupied),
            },
            {
               // Role name must match what the existing CTool_vav_ibal c-tor looks
               // up (RequiredAntecedentSubjectKeyFromS223Role(..., "air_source")).
               { RoleId{ "air_source" }, "urn:zandrea:tool-profile#AhuProfile", true },
            }
         };
         return manifest;
      }
};

} // namespace

const CModuleRegistry& BuildBuiltInAnalysisModules(void) {
   static const CAhuModule ahuModule;
   static const CVavModule vavModule;
   static CModuleRegistry registry = [] {
      CModuleRegistry r;
      r.Register(ahuModule);
      r.Register(vavModule);
      return r;
   }();
   return registry;
}

//END-OF-FILE ZZZZZ2ZZZZZZZZZ3ZZZZZZZZZ4ZZZZZZZZZ5ZZZZZZZZZ6ZZZZZZZZZ7ZZZZZZZZZ8ZZZZZZZZZ9ZZZZZZZZZCZZZZZ
