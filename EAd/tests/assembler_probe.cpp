// Assembler probe: run the witness pass, feed it to the generic assembler, and
// dump the full ResolvedToolModel -- everything "in hand" at the end of the
// assembler, i.e. the exact state the per-tool Build() step will consume.
// Proves the RDF -> resolved-tool half end to end. See docs/tool-contract-design.md.
//
//   ./assembler_probe [testdata-dir] [--json]
//
#include <shifty/shifty.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../../libEA/toolAssembler.hpp"
#include "../../libEA/toolProfile.hpp"   // EPointNameToString

static std::string slurp(const std::string& p) {
   std::ifstream f(p);
   std::stringstream ss; ss << f.rdbuf();
   return ss.str();
}

// The thin shifty witness runner. In the real entrypoint this lives in
// s223Model.cpp; here it is inline so the probe is self-contained.
static std::vector<RoleWitness> RunRoleWitnesses(const std::string& base) {
   const std::string shapes =
      slurp(base + "/zea-core.ttl") + "\n" + slurp(base + "/zea-profiles.ttl");
   shifty::PreparedValidator validator(shapes, shifty::RdfFormat::Turtle);

   shifty::Dataset data;
   data.load_file(base + "/simple-ahu-vav-223.ttl", shifty::RdfFormat::Turtle);

   shifty::ValidationOptions opts;
   opts.key_path = "zea:roleName";
   opts.run_inference = true;

   std::vector<RoleWitness> out;
   for (const auto& w : validator.witnesses(data, opts)) {
      out.push_back({ w.focus_node, w.shape_id, w.key, w.value_nodes });
   }
   return out;
}

static const char* kind(RoleValueKind k) {
   return k == RoleValueKind::Analog ? "analog" : "binary";
}

// Readable names for the engine construction enums. Symbolic (matching source),
// covering the values the AHU/VAV manifests actually use; anything else falls
// back to its numeric code so nothing is silently mislabelled.
static std::string labelStr(EDataLabel v) {
   switch (v) {
      case EDataLabel::Point_pressure_static_air_supply:      return "Point_pressure_static_air_supply";
      case EDataLabel::Point_pressure_static_air_inlet:       return "Point_pressure_static_air_inlet";
      case EDataLabel::Point_temperature_air_outside:         return "Point_temperature_air_outside";
      case EDataLabel::Point_temperature_air_mixed:           return "Point_temperature_air_mixed";
      case EDataLabel::Point_temperature_air_return:          return "Point_temperature_air_return";
      case EDataLabel::Point_temperature_air_supply:          return "Point_temperature_air_supply";
      case EDataLabel::Point_temperature_air_supply_setpt:    return "Point_temperature_air_supply_setpt";
      case EDataLabel::Point_temperature_air_inlet:           return "Point_temperature_air_inlet";
      case EDataLabel::Point_temperature_air_discharge:       return "Point_temperature_air_discharge";
      case EDataLabel::Point_temperature_air_zone:            return "Point_temperature_air_zone";
      case EDataLabel::Point_temperature_air_zone_setpt_htg:  return "Point_temperature_air_zone_setpt_htg";
      case EDataLabel::Point_temperature_air_zone_setpt_clg:  return "Point_temperature_air_zone_setpt_clg";
      case EDataLabel::Point_command_damper_mixing:           return "Point_command_damper_mixing";
      case EDataLabel::Point_command_damper_disch:            return "Point_command_damper_disch";
      case EDataLabel::Point_command_valve_chw:               return "Point_command_valve_chw";
      case EDataLabel::Point_command_valve_hw:                return "Point_command_valve_hw";
      case EDataLabel::Point_flowVolume_air_supply:           return "Point_flowVolume_air_supply";
      case EDataLabel::Point_flowVolume_air_disch:            return "Point_flowVolume_air_disch";
      case EDataLabel::Point_flowVolume_air_disch_setpt:      return "Point_flowVolume_air_disch_setpt";
      case EDataLabel::Point_binary_system_occupied:          return "Point_binary_system_occupied";
      case EDataLabel::Point_binary_zone_occupied:            return "Point_binary_zone_occupied";
      case EDataLabel::Fact_direct_Bso:                       return "Fact_direct_Bso";
      case EDataLabel::Fact_direct_Bzo:                       return "Fact_direct_Bzo";
      default: return "EDataLabel#" + std::to_string(static_cast<int>(v));
   }
}
static std::string unitStr(EDataUnit v) {
   switch (v) {
      case EDataUnit::PressureGage_Pa: return "Pa(gauge)";
      case EDataUnit::Temperature_degC: return "degC";
      case EDataUnit::Ratio_percent:    return "%";
      case EDataUnit::FlowVolume_Lps:   return "L/s";
      default: return "EDataUnit#" + std::to_string(static_cast<int>(v));
   }
}
static std::string rangeStr(EDataRange v) {
   switch (v) {
      case EDataRange::Analog_percent:    return "0..100%";
      case EDataRange::Analog_zeroTo1k:   return "0..1000";
      case EDataRange::Analog_n18To49:    return "-18..49";
      case EDataRange::Analog_zeroTo1416: return "0..1416";
      default: return "EDataRange#" + std::to_string(static_cast<int>(v));
   }
}
static std::string plotStr(EPlotGroup v) {
   switch (v) {
      case EPlotGroup::Undefined: return "Undefined";
      case EPlotGroup::Alone:     return "Alone";
      case EPlotGroup::Free:      return "Free";
      case EPlotGroup::GroupA:    return "GroupA";
      case EPlotGroup::GroupB:    return "GroupB";
      case EPlotGroup::GroupC:    return "GroupC";
      case EPlotGroup::GroupD:    return "GroupD";
      default: return "EPlotGroup#" + std::to_string(static_cast<int>(v));
   }
}

static void dumpText(const ResolvedToolModel& model) {
   std::cout << "=== Resolved tool model (post-assembler) ===\n"
             << model.tools.size() << " tool(s), antecedent-safe order:\n";
   int n = 0;
   for (const auto& t : model.tools) {
      std::cout << "\n[" << ++n << "] " << t.name << "   profile=" << t.toolProfileId << "\n"
                << "    focus: " << t.focusNode << "\n"
                << "    shape: " << t.shapeId << "\n";
      for (const auto& a : t.antecedents) {
         std::cout << "    antecedent " << a.role.name << " -> " << a.focusNode << "\n";
      }
      std::cout << "    points (" << t.points.size() << "):\n";
      for (const auto& p : t.points) {
         const auto& s = *p.spec;
         std::cout << "      " << p.role.name << " [" << kind(s.valueKind) << "]"
                   << "  pointName=" << EPointNameToString(s.pointName)
                   << "\n          rdf=" << p.rdfProperty
                   << "\n          engine: label=" << labelStr(s.dataLabel)
                   << " unit=" << unitStr(s.dataUnit)
                   << " range=" << rangeStr(s.dataRange)
                   << " plot=" << plotStr(s.plotGroup)
                   << (s.valueKind == RoleValueKind::Binary
                          ? " factLabel=" + labelStr(s.factLabel) : "")
                   << "\n";
      }
   }
   std::cout << "\ndiagnostics (" << model.diagnostics.size() << "):\n";
   for (const auto& d : model.diagnostics) {
      std::cout << "  [" << d.focusNode << "] " << d.message << "\n";
   }
}

static std::string jstr(const std::string& s) {
   std::string o = "\"";
   for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += c; }
   return o + "\"";
}

static void dumpJson(const ResolvedToolModel& model) {
   std::cout << "{\n  \"tools\": [\n";
   for (std::size_t i = 0; i < model.tools.size(); ++i) {
      const auto& t = model.tools[i];
      std::cout << "    {\n      \"order\": " << (i + 1)
                << ",\n      \"name\": " << jstr(t.name)
                << ",\n      \"profile\": " << jstr(t.toolProfileId)
                << ",\n      \"focus\": " << jstr(t.focusNode)
                << ",\n      \"antecedents\": [";
      for (std::size_t a = 0; a < t.antecedents.size(); ++a) {
         std::cout << (a ? ", " : "") << "{" << jstr(t.antecedents[a].role.name)
                   << ": " << jstr(t.antecedents[a].focusNode) << "}";
      }
      std::cout << "],\n      \"points\": [\n";
      for (std::size_t p = 0; p < t.points.size(); ++p) {
         const auto& pb = t.points[p]; const auto& s = *pb.spec;
         std::cout << "        {\"role\": " << jstr(pb.role.name)
                   << ", \"kind\": " << jstr(kind(s.valueKind))
                   << ", \"pointName\": " << jstr(EPointNameToString(s.pointName))
                   << ", \"rdf\": " << jstr(pb.rdfProperty)
                   << ", \"label\": " << jstr(labelStr(s.dataLabel))
                   << ", \"unit\": " << jstr(unitStr(s.dataUnit))
                   << ", \"range\": " << jstr(rangeStr(s.dataRange))
                   << ", \"plotGroup\": " << jstr(plotStr(s.plotGroup)) << "}"
                   << (p + 1 < t.points.size() ? "," : "") << "\n";
      }
      std::cout << "      ]\n    }" << (i + 1 < model.tools.size() ? "," : "") << "\n";
   }
   std::cout << "  ],\n  \"diagnostics\": [";
   for (std::size_t d = 0; d < model.diagnostics.size(); ++d) {
      std::cout << (d ? ", " : "") << "{" << jstr(model.diagnostics[d].focusNode)
                << ": " << jstr(model.diagnostics[d].message) << "}";
   }
   std::cout << "]\n}\n";
}

int main(int argc, char** argv) {
   std::string base = "EAd/tests/testdata";
   bool json = false;
   for (int i = 1; i < argc; ++i) {
      const std::string a = argv[i];
      if (a == "--json") json = true; else base = a;
   }

   const auto model = AssembleTools(RunRoleWitnesses(base), BuiltInAnalysisModules());
   if (json) dumpJson(model); else dumpText(model);
   return 0;
}
