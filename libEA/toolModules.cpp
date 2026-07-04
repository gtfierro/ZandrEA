//XXXXXXX1XXXXXXXXX2XXXXXXXXX3XXXXXXXXX4XXXXXXXXX5XXXXXXXXX6XXXXXXXXX7XXXXXXXXX8XXXXXXXXX9XXXXXXXXXCXXXXV
/* File summary:
   Built-in analysis modules for the model-driven tool-contract entrypoint.  Each
   module declares its ModuleManifest: the roles it needs, keyed to engine
   construction attributes (EPointName/EDataLabel/EDataUnit/EDataRange/EPlotGroup)
   copied from the corresponding legacy CTool_* constructor so the resolved points
   are identical to the current build.  The FDD Build() step is the next slice.

   Module role names must match the zea:roleName literals in
   EAd/tests/testdata/zea-profiles.ttl; the assembler reconciles the two.
*/
/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8/////////9/////////C////V

#include "analysisModule.hpp"

namespace {

const std::string kProfileNs = "urn:zandrea:tool-profile#";

PointRoleSpec Analog(const char* role,
                     EPointName pointName,
                     EDataLabel  dataLabel,
                     EDataUnit   dataUnit,
                     EDataRange  dataRange,
                     EPlotGroup  plotGroup) {
   return { RoleId{ role }, RoleValueKind::Analog, true,
            pointName, dataLabel, dataUnit, dataRange, plotGroup,
            dataLabel /* factLabel unused for analog */ };
}

PointRoleSpec Binary(const char* role,
                     EPointName pointName,
                     EDataLabel pointLabel,
                     EDataLabel factLabel) {
   // Unit/range/plotGroup are ignored for binary points (CPointBinary does not
   // take them); benign placeholders keep the struct uniform.
   return { RoleId{ role }, RoleValueKind::Binary, true,
            pointName, pointLabel, EDataUnit::Ratio_percent, EDataRange::Analog_percent,
            EPlotGroup::Free, factLabel };
}

//======================================================================================================/
// AHU (single-duct, VAV-reheat) — mirrors CTool_ahu_ibal's point objects.

class CAhuModule : public IAnalysisModule {
   public:
      const ModuleManifest& Manifest(void) const override {
         static const ModuleManifest manifest = {
            kProfileNs + "AhuProfile",
            "ahu_ibal",
            {
               Analog("supplyStaticPressure", EPointName::Pressure_static_air_supply,
                      EDataLabel::Point_pressure_static_air_supply, EDataUnit::PressureGage_Pa,
                      EDataRange::Analog_zeroTo1k, EPlotGroup::Free),
               Analog("outsideAirTemp", EPointName::Temperature_air_outside,
                      EDataLabel::Point_temperature_air_outside, EDataUnit::Temperature_degC,
                      EDataRange::Analog_n18To49, EPlotGroup::Free),
               Analog("mixingDamperCmd", EPointName::Position_damper_mixingBox,
                      EDataLabel::Point_command_damper_mixing, EDataUnit::Ratio_percent,
                      EDataRange::Analog_percent, EPlotGroup::Free),
               Analog("mixedAirTemp", EPointName::Temperature_air_mixed,
                      EDataLabel::Point_temperature_air_mixed, EDataUnit::Temperature_degC,
                      EDataRange::Analog_n18To49, EPlotGroup::Free),
               Analog("returnAirTemp", EPointName::Temperature_air_return,
                      EDataLabel::Point_temperature_air_return, EDataUnit::Temperature_degC,
                      EDataRange::Analog_n18To49, EPlotGroup::Free),
               Analog("chwValveCmd", EPointName::Position_valve_chw,
                      EDataLabel::Point_command_valve_chw, EDataUnit::Ratio_percent,
                      EDataRange::Analog_percent, EPlotGroup::Free),
               Analog("supplyAirTemp", EPointName::Temperature_air_supply,
                      EDataLabel::Point_temperature_air_supply, EDataUnit::Temperature_degC,
                      EDataRange::Analog_n18To49, EPlotGroup::GroupA),
               Analog("supplyAirTempSetpt", EPointName::Temperature_air_supply_setpt,
                      EDataLabel::Point_temperature_air_supply_setpt, EDataUnit::Temperature_degC,
                      EDataRange::Analog_n18To49, EPlotGroup::GroupA),
               Binary("systemOccupied", EPointName::Binary_systemOccupied,
                      EDataLabel::Point_binary_system_occupied, EDataLabel::Fact_direct_Bso),
               Analog("hwValveCmd", EPointName::Position_valve_hw,
                      EDataLabel::Point_command_valve_hw, EDataUnit::Ratio_percent,
                      EDataRange::Analog_percent, EPlotGroup::Free),
               Analog("supplyAirFlow", EPointName::FlowRateVolume_air_ahu,
                      EDataLabel::Point_flowVolume_air_supply, EDataUnit::FlowVolume_Lps,
                      EDataRange::Analog_zeroTo1416, EPlotGroup::Free),
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
            kProfileNs + "VavProfile",
            "vav_ibal",
            {
               Analog("inletStaticPressure", EPointName::Pressure_static_air_supply,
                      EDataLabel::Point_pressure_static_air_inlet, EDataUnit::PressureGage_Pa,
                      EDataRange::Analog_zeroTo1k, EPlotGroup::Free),
               Analog("inletAirTemp", EPointName::Temperature_air_supply,
                      EDataLabel::Point_temperature_air_inlet, EDataUnit::Temperature_degC,
                      EDataRange::Analog_n18To49, EPlotGroup::Free),
               Analog("dischargeAirTemp", EPointName::Temperature_air_discharge,
                      EDataLabel::Point_temperature_air_discharge, EDataUnit::Temperature_degC,
                      EDataRange::Analog_n18To49, EPlotGroup::Free),
               Analog("zoneAirTemp", EPointName::Temperature_air_zone,
                      EDataLabel::Point_temperature_air_zone, EDataUnit::Temperature_degC,
                      EDataRange::Analog_n18To49, EPlotGroup::GroupA),
               Analog("zoneTempSetptHtg", EPointName::Temperature_air_zone_setpt_htg,
                      EDataLabel::Point_temperature_air_zone_setpt_htg, EDataUnit::Temperature_degC,
                      EDataRange::Analog_n18To49, EPlotGroup::GroupA),
               Analog("zoneTempSetptClg", EPointName::Temperature_air_zone_setpt_clg,
                      EDataLabel::Point_temperature_air_zone_setpt_clg, EDataUnit::Temperature_degC,
                      EDataRange::Analog_n18To49, EPlotGroup::GroupA),
               Analog("hwValveCmd", EPointName::Position_valve_hw,
                      EDataLabel::Point_command_valve_hw, EDataUnit::Ratio_percent,
                      EDataRange::Analog_percent, EPlotGroup::Free),
               Analog("damperCmd", EPointName::Position_damper_vav,
                      EDataLabel::Point_command_damper_disch, EDataUnit::Ratio_percent,
                      EDataRange::Analog_percent, EPlotGroup::Free),
               Analog("airFlow", EPointName::FlowRateVolume_air_vav,
                      EDataLabel::Point_flowVolume_air_disch, EDataUnit::FlowVolume_Lps,
                      EDataRange::Analog_zeroTo1416, EPlotGroup::GroupA),
               Analog("airFlowSetpt", EPointName::FlowRateVolume_air_vav_setpt,
                      EDataLabel::Point_flowVolume_air_disch_setpt, EDataUnit::FlowVolume_Lps,
                      EDataRange::Analog_zeroTo1416, EPlotGroup::GroupA),
               Binary("zoneOccupied", EPointName::Binary_zoneOccupied,
                      EDataLabel::Point_binary_zone_occupied, EDataLabel::Fact_direct_Bzo),
            },
            {
               { RoleId{ "airSource" }, kProfileNs + "AhuProfile", true },
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
