//XXXXXXX1XXXXXXXXX2XXXXXXXXX3XXXXXXXXX4XXXXXXXXX5XXXXXXXXX6XXXXXXXXX7XXXXXXXXX8XXXXXXXXX9XXXXXXXXXCXXXXV
/* File summary:
   Implements the parallel, declarative profile layer for tool RDF requirements.
*/
/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8/////////9/////////C////V

#include "toolProfile.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {

S223PropertyPath P(const std::string& iriOrPrefixedName) {
   return S223PropertyPath::Predicate(iriOrPrefixedName);
}

bool is_prefixed_name(const std::string& value) {
   const auto colon = value.find(':');
   if (colon == std::string::npos || colon == 0 || colon + 1 >= value.size()) {
      return false;
   }

   return value.rfind("http://", 0) != 0
          && value.rfind("https://", 0) != 0
          && value.rfind("urn:", 0) != 0;
}

std::string turtle_resource(const std::string& iriOrPrefixedName) {
   if (is_prefixed_name(iriOrPrefixedName)) {
      return iriOrPrefixedName;
   }
   return "<" + iriOrPrefixedName + ">";
}

std::string turtle_literal(const std::string& value) {
   std::string escaped;
   escaped.reserve(value.size() + 2);
   escaped.push_back('"');
   for (const char ch : value) {
      switch (ch) {
         case '\\': escaped += "\\\\"; break;
         case '"': escaped += "\\\""; break;
         case '\n': escaped += "\\n"; break;
         case '\r': escaped += "\\r"; break;
         case '\t': escaped += "\\t"; break;
         default: escaped.push_back(ch); break;
      }
   }
   escaped.push_back('"');
   return escaped;
}

std::string sparql_path_operand(const S223PropertyPath& path) {
   if (path.Operator() == S223PropertyPathOperator::Predicate) {
      return path.AsSparqlPropertyPath();
   }
   return "(" + path.AsSparqlPropertyPath() + ")";
}

std::string join_sparql_operands(const std::vector<S223PropertyPath>& operands,
                                 const std::string& separator) {
   if (operands.empty()) {
      throw std::runtime_error("S223 property path operator requires at least one operand");
   }

   std::ostringstream joined;
   for (std::size_t i = 0; i < operands.size(); ++i) {
      if (i > 0) {
         joined << separator;
      }
      joined << sparql_path_operand(operands[i]);
   }
   return joined.str();
}

std::string shacl_path_operand(const S223PropertyPath& path) {
   return path.AsShaclPathTurtle();
}

std::string shacl_path_list(const std::vector<S223PropertyPath>& operands) {
   if (operands.empty()) {
      throw std::runtime_error("S223 SHACL property path list requires at least one operand");
   }

   std::ostringstream list;
   list << "( ";
   for (const auto& operand : operands) {
      list << shacl_path_operand(operand) << " ";
   }
   list << ")";
   return list.str();
}

std::string shape_name_for_profile(const ToolProfile& profile) {
   std::string shapeName = "zea:";
   bool capitalizeNext = true;

   for (const char ch : profile.id) {
      if (ch == '_' || ch == '-' || ch == ' ') {
         capitalizeNext = true;
         continue;
      }

      if (capitalizeNext && ch >= 'a' && ch <= 'z') {
         shapeName.push_back(static_cast<char>(ch - 'a' + 'A'));
      } else {
         shapeName.push_back(ch);
      }
      capitalizeNext = false;
   }

   shapeName += "Shape";
   return shapeName;
}

std::string sparql_variable_token(const std::string& value) {
   std::string token;
   token.reserve(value.size());

   for (const char ch : value) {
      if ((ch >= 'a' && ch <= 'z')
          || (ch >= 'A' && ch <= 'Z')
          || (ch >= '0' && ch <= '9')) {
         token.push_back(ch);
      } else {
         token.push_back('_');
      }
   }

   if (token.empty() || (token.front() >= '0' && token.front() <= '9')) {
      token.insert(token.begin(), '_');
   }

   return token;
}

void append_focus_patterns(std::ostringstream& query, const ToolProfile& profile) {
   if (profile.focus.targetClasses.empty()) {
      throw std::runtime_error("Tool profile discovery requires at least one focus target class");
   }

   query << "    VALUES ?focusClass { ";
   for (const auto& targetClass : profile.focus.targetClasses) {
      query << turtle_resource(targetClass) << " ";
   }
   query << "}\n";
   query << "    ?equipment a ?focusClass .\n";
}

void append_class_patterns(std::ostringstream& query,
                           const std::string& variable,
                           const std::vector<std::string>& requiredClasses,
                           const std::string& indent) {
   for (const auto& requiredClass : requiredClasses) {
      query << indent << variable << " a " << turtle_resource(requiredClass) << " .\n";
   }
}

void append_antecedent_patterns(std::ostringstream& query, const ToolProfile& profile) {
   for (const auto& antecedent : profile.antecedents) {
      const auto var = "?antecedent_" + sparql_variable_token(antecedent.role);
      if (antecedent.required) {
         query << "    ?equipment " << antecedent.path.AsSparqlPropertyPath()
               << " " << var << " .\n";
         append_class_patterns(query, var, antecedent.requiredClasses, "    ");
      } else {
         query << "    OPTIONAL {\n";
         query << "      ?equipment " << antecedent.path.AsSparqlPropertyPath()
               << " " << var << " .\n";
         append_class_patterns(query, var, antecedent.requiredClasses, "      ");
         query << "    }\n";
      }
   }
}

void append_point_patterns(std::ostringstream& query, const ToolProfile& profile) {
   for (const auto& point : profile.points) {
      const auto pointName = EPointNameToString(point.pointName);
      const auto var = "?point_" + sparql_variable_token(pointName);
      const auto appendPointBody = [&]() {
         query << "      ?equipment " << point.path.AsSparqlPropertyPath()
               << " " << var << " .\n";
         append_class_patterns(query, var, point.requiredClasses, "      ");
         for (const auto& quantityKind : point.requiredQuantityKinds) {
            query << "      " << var << " qudt:hasQuantityKind "
                  << turtle_resource(quantityKind) << " .\n";
         }
      };

      if (point.required) {
         appendPointBody();
      } else {
         query << "    OPTIONAL {\n";
         appendPointBody();
         query << "    }\n";
      }
   }
}

void append_required_classes(std::ostringstream& ttl,
                             const std::vector<std::string>& requiredClasses,
                             const std::string& indent) {
   for (const auto& requiredClass : requiredClasses) {
      ttl << indent << "sh:class " << turtle_resource(requiredClass) << " ;\n";
   }
}

void append_quantity_kind_node(std::ostringstream& ttl,
                               const PointRequirement& point,
                               const std::string& indent) {
   if (point.requiredQuantityKinds.empty()) {
      return;
   }

   ttl << indent << "sh:node [\n";
   ttl << indent << "   sh:property [\n";
   ttl << indent << "      sh:path qudt:hasQuantityKind ;\n";
   ttl << indent << "      sh:in ( ";
   for (const auto& quantityKind : point.requiredQuantityKinds) {
      ttl << turtle_resource(quantityKind) << " ";
   }
   ttl << ") ;\n";
   ttl << indent << "   ] ;\n";
   ttl << indent << "] ;\n";
}

std::vector<std::string> quantifiable(void) {
   return { "s223:QuantifiableObservableProperty" };
}

std::vector<std::string> enumerable(void) {
   return { "s223:EnumeratedObservableProperty" };
}

PointRequirement analog_point(EPointName pointName,
                              const std::vector<std::string>& quantityKinds) {
   return {
      pointName,
      ToolPointValueKind::Analog,
      true,
      P("s223:hasProperty"),
      quantifiable(),
      quantityKinds
   };
}

PointRequirement binary_point(EPointName pointName) {
   return {
      pointName,
      ToolPointValueKind::Binary,
      true,
      P("s223:hasProperty"),
      enumerable(),
      {}
   };
}

// Function-local statics keep profile construction independent of cross-TU
// global initialization order.
const std::vector<PointRequirement>& AhuPoints(void) {
   static const std::vector<PointRequirement> points = {
      analog_point(EPointName::Pressure_static_air_supply, { "quantitykind:GaugePressure" }),
      analog_point(EPointName::Temperature_air_outside, { "quantitykind:Temperature" }),
      analog_point(EPointName::Position_damper_mixingBox, { "quantitykind:DimensionlessRatio" }),
      analog_point(EPointName::Temperature_air_mixed, { "quantitykind:Temperature" }),
      analog_point(EPointName::Temperature_air_return, { "quantitykind:Temperature" }),
      analog_point(EPointName::Position_valve_chw, { "quantitykind:DimensionlessRatio" }),
      analog_point(EPointName::Temperature_air_supply, { "quantitykind:Temperature" }),
      analog_point(EPointName::Temperature_air_supply_setpt, { "quantitykind:Temperature" }),
      binary_point(EPointName::Binary_systemOccupied),
      analog_point(EPointName::Position_valve_hw, { "quantitykind:DimensionlessRatio" }),
      analog_point(EPointName::FlowRateVolume_air_ahu, { "quantitykind:VolumeFlowRate" })
   };
   return points;
}

const std::vector<PointRequirement>& VavPoints(void) {
   static const std::vector<PointRequirement> points = {
      analog_point(EPointName::Pressure_static_air_supply, { "quantitykind:GaugePressure" }),
      analog_point(EPointName::Temperature_air_supply, { "quantitykind:Temperature" }),
      analog_point(EPointName::Temperature_air_discharge, { "quantitykind:Temperature" }),
      analog_point(EPointName::Temperature_air_zone, { "quantitykind:Temperature" }),
      analog_point(EPointName::Temperature_air_zone_setpt_htg, { "quantitykind:Temperature" }),
      analog_point(EPointName::Temperature_air_zone_setpt_clg, { "quantitykind:Temperature" }),
      analog_point(EPointName::Position_valve_hw, { "quantitykind:DimensionlessRatio" }),
      analog_point(EPointName::Position_damper_vav, { "quantitykind:DimensionlessRatio" }),
      analog_point(EPointName::FlowRateVolume_air_vav, { "quantitykind:VolumeFlowRate" }),
      analog_point(EPointName::FlowRateVolume_air_vav_setpt, { "quantitykind:VolumeFlowRate" }),
      binary_point(EPointName::Binary_zoneOccupied)
   };
   return points;
}

} // namespace

S223PropertyPath::S223PropertyPath(S223PropertyPathOperator argOperator,
                                   std::string argPredicateName,
                                   std::vector<S223PropertyPath> argOperands)
      : pathOperator(argOperator),
        predicateName(std::move(argPredicateName)),
        operands(std::move(argOperands)) {
}

S223PropertyPath S223PropertyPath::Predicate(std::string iriOrPrefixedName) {
   if (iriOrPrefixedName.empty()) {
      throw std::runtime_error("S223 predicate property path cannot be empty");
   }
   return S223PropertyPath(S223PropertyPathOperator::Predicate, std::move(iriOrPrefixedName), {});
}

S223PropertyPath S223PropertyPath::Sequence(std::vector<S223PropertyPath> operands) {
   return S223PropertyPath(S223PropertyPathOperator::Sequence, "", std::move(operands));
}

S223PropertyPath S223PropertyPath::Alternative(std::vector<S223PropertyPath> operands) {
   return S223PropertyPath(S223PropertyPathOperator::Alternative, "", std::move(operands));
}

S223PropertyPath S223PropertyPath::Inverse(S223PropertyPath operand) {
   return S223PropertyPath(S223PropertyPathOperator::Inverse, "", { std::move(operand) });
}

S223PropertyPath S223PropertyPath::ZeroOrMore(S223PropertyPath operand) {
   return S223PropertyPath(S223PropertyPathOperator::ZeroOrMore, "", { std::move(operand) });
}

S223PropertyPath S223PropertyPath::OneOrMore(S223PropertyPath operand) {
   return S223PropertyPath(S223PropertyPathOperator::OneOrMore, "", { std::move(operand) });
}

S223PropertyPath S223PropertyPath::ZeroOrOne(S223PropertyPath operand) {
   return S223PropertyPath(S223PropertyPathOperator::ZeroOrOne, "", { std::move(operand) });
}

S223PropertyPathOperator S223PropertyPath::Operator(void) const {
   return pathOperator;
}

const std::string& S223PropertyPath::PredicateName(void) const {
   return predicateName;
}

const std::vector<S223PropertyPath>& S223PropertyPath::Operands(void) const {
   return operands;
}

std::string S223PropertyPath::AsSparqlPropertyPath(void) const {
   switch (pathOperator) {
      case S223PropertyPathOperator::Predicate:
         return turtle_resource(predicateName);

      case S223PropertyPathOperator::Sequence:
         return join_sparql_operands(operands, "/");

      case S223PropertyPathOperator::Alternative:
         return join_sparql_operands(operands, "|");

      case S223PropertyPathOperator::Inverse:
         if (operands.size() != 1) {
            throw std::runtime_error("S223 inverse property path requires exactly one operand");
         }
         return "^" + sparql_path_operand(operands.front());

      case S223PropertyPathOperator::ZeroOrMore:
         if (operands.size() != 1) {
            throw std::runtime_error("S223 zero-or-more property path requires exactly one operand");
         }
         return sparql_path_operand(operands.front()) + "*";

      case S223PropertyPathOperator::OneOrMore:
         if (operands.size() != 1) {
            throw std::runtime_error("S223 one-or-more property path requires exactly one operand");
         }
         return sparql_path_operand(operands.front()) + "+";

      case S223PropertyPathOperator::ZeroOrOne:
         if (operands.size() != 1) {
            throw std::runtime_error("S223 zero-or-one property path requires exactly one operand");
         }
         return sparql_path_operand(operands.front()) + "?";
   }

   throw std::runtime_error("Unhandled S223 property path operator");
}

std::string S223PropertyPath::AsShaclPathTurtle(void) const {
   switch (pathOperator) {
      case S223PropertyPathOperator::Predicate:
         return turtle_resource(predicateName);

      case S223PropertyPathOperator::Sequence:
         return shacl_path_list(operands);

      case S223PropertyPathOperator::Alternative:
         return "[ sh:alternativePath " + shacl_path_list(operands) + " ]";

      case S223PropertyPathOperator::Inverse:
         if (operands.size() != 1) {
            throw std::runtime_error("S223 inverse SHACL property path requires exactly one operand");
         }
         return "[ sh:inversePath " + shacl_path_operand(operands.front()) + " ]";

      case S223PropertyPathOperator::ZeroOrMore:
         if (operands.size() != 1) {
            throw std::runtime_error("S223 zero-or-more SHACL property path requires exactly one operand");
         }
         return "[ sh:zeroOrMorePath " + shacl_path_operand(operands.front()) + " ]";

      case S223PropertyPathOperator::OneOrMore:
         if (operands.size() != 1) {
            throw std::runtime_error("S223 one-or-more SHACL property path requires exactly one operand");
         }
         return "[ sh:oneOrMorePath " + shacl_path_operand(operands.front()) + " ]";

      case S223PropertyPathOperator::ZeroOrOne:
         if (operands.size() != 1) {
            throw std::runtime_error("S223 zero-or-one SHACL property path requires exactly one operand");
         }
         return "[ sh:zeroOrOnePath " + shacl_path_operand(operands.front()) + " ]";
   }

   throw std::runtime_error("Unhandled S223 SHACL property path operator");
}

std::string ToolPointValueKindToString(ToolPointValueKind kind) {
   switch (kind) {
      case ToolPointValueKind::Analog: return "analog";
      case ToolPointValueKind::Binary: return "binary";
   }
   throw std::runtime_error("Unhandled tool point value kind");
}

std::string EPointNameToString(EPointName pointName) {
   switch (pointName) {
      case EPointName::Undefined: return "Undefined";
      case EPointName::Binary_systemOccupied: return "Binary_systemOccupied";
      case EPointName::Binary_zoneOccupied: return "Binary_zoneOccupied";
      case EPointName::Command_damper_mixingBox: return "Command_damper_mixingBox";
      case EPointName::Command_damper_outsideAir: return "Command_damper_outsideAir";
      case EPointName::Command_damper_vav: return "Command_damper_vav";
      case EPointName::Command_fanSpeed: return "Command_fanSpeed";
      case EPointName::Command_valve_chw: return "Command_valve_chw";
      case EPointName::Command_valve_hw: return "Command_valve_hw";
      case EPointName::FlowRateVolume_air_ahu: return "FlowRateVolume_air_ahu";
      case EPointName::FlowRateVolume_air_ahu_setpt: return "FlowRateVolume_air_ahu_setpt";
      case EPointName::FlowRateVolume_air_vav: return "FlowRateVolume_air_vav";
      case EPointName::FlowRateVolume_air_vav_setpt: return "FlowRateVolume_air_vav_setpt";
      case EPointName::Position_damper_mixingBox: return "Position_damper_mixingBox";
      case EPointName::Position_damper_outsideAir: return "Position_damper_outsideAir";
      case EPointName::Position_damper_vav: return "Position_damper_vav";
      case EPointName::Position_valve_chw: return "Position_valve_chw";
      case EPointName::Position_valve_hw: return "Position_valve_hw";
      case EPointName::Pressure_static_air_inlet: return "Pressure_static_air_inlet";
      case EPointName::Pressure_static_air_supply: return "Pressure_static_air_supply";
      case EPointName::Pressure_static_air_supply_setpt: return "Pressure_static_air_supply_setpt";
      case EPointName::Temperature_air_discharge: return "Temperature_air_discharge";
      case EPointName::Temperature_air_inlet: return "Temperature_air_inlet";
      case EPointName::Temperature_air_mixed: return "Temperature_air_mixed";
      case EPointName::Temperature_air_outside: return "Temperature_air_outside";
      case EPointName::Temperature_air_return: return "Temperature_air_return";
      case EPointName::Temperature_air_supply: return "Temperature_air_supply";
      case EPointName::Temperature_air_supply_setpt: return "Temperature_air_supply_setpt";
      case EPointName::Temperature_air_zone: return "Temperature_air_zone";
      case EPointName::Temperature_air_zone_setpt_clg: return "Temperature_air_zone_setpt_clg";
      case EPointName::Temperature_air_zone_setpt_htg: return "Temperature_air_zone_setpt_htg";
      case EPointName::Temperature_glycol_leaving: return "Temperature_glycol_leaving";
      case EPointName::Temperature_glycol_leaving_setpt: return "Temperature_glycol_leaving_setpt";
      case EPointName::Temperature_water_leaving: return "Temperature_water_leaving";
   }
   throw std::runtime_error("Unhandled EPointName");
}

const std::vector<ToolProfile>& BuiltInToolProfiles(void) {
   static const std::vector<ToolProfile> profiles = {
      {
         "ahu_ibal",
         "AHU IBAL",
         LegacyToolConstructorId::AhuIbal,
         { { "s223:AirHandlingUnit" } },
         {
            {
               "chilled_water_source",
               "chw_plant",
               false,
               S223PropertyPath::Alternative({
                  P("s223:connectedFrom"),
                  S223PropertyPath::Sequence({
                     P("s223:hasConnectionPoint"),
                     S223PropertyPath::Inverse(P("s223:connectsThrough")),
                     S223PropertyPath::Inverse(P("s223:hasConnectionPoint"))
                  })
               }),
               {}
            },
            {
               "hot_water_source",
               "hw_plant",
               false,
               S223PropertyPath::Alternative({
                  P("s223:connectedFrom"),
                  S223PropertyPath::Sequence({
                     P("s223:hasConnectionPoint"),
                     S223PropertyPath::Inverse(P("s223:connectsThrough")),
                     S223PropertyPath::Inverse(P("s223:hasConnectionPoint"))
                  })
               }),
               {}
            }
         },
         AhuPoints()
      },
      {
         "vav_ibal",
         "VAV IBAL",
         LegacyToolConstructorId::VavIbal,
         {
            {
               "s223:TerminalUnit",
               "s223:SingleDuctTerminal",
               "s223:DualDuctTerminal",
               "s223:FanPoweredTerminal"
            }
         },
         {
            {
               "air_source",
               "ahu_ibal",
               true,
               S223PropertyPath::OneOrMore(P("s223:connectedFrom")),
               { "s223:AirHandlingUnit" }
            },
            {
               "hot_water_source",
               "hw_plant",
               false,
               S223PropertyPath::Alternative({
                  P("s223:connectedFrom"),
                  S223PropertyPath::Sequence({
                     P("s223:hasConnectionPoint"),
                     S223PropertyPath::Inverse(P("s223:connectsThrough")),
                     S223PropertyPath::Inverse(P("s223:hasConnectionPoint"))
                  })
               }),
               {}
            }
         },
         VavPoints()
      }
   };

   return profiles;
}

std::optional<ToolProfile> FindBuiltInToolProfile(std::string_view id) {
   const auto& profiles = BuiltInToolProfiles();
   const auto found = std::find_if(
      profiles.begin(),
      profiles.end(),
      [id](const ToolProfile& profile) {
         return profile.id == id;
      }
   );

   if (found == profiles.end()) {
      return std::nullopt;
   }
   return *found;
}

std::string GenerateToolProfileShapesTurtle(const std::vector<ToolProfile>& profiles) {
   std::ostringstream ttl;

   // Shapes are generated from the same profile objects used for startup
   // discovery.  That keeps validation requirements and runtime creation
   // requirements from drifting apart.
   ttl << "@prefix sh: <http://www.w3.org/ns/shacl#> .\n";
   ttl << "@prefix qudt: <http://qudt.org/schema/qudt/> .\n";
   ttl << "@prefix quantitykind: <http://qudt.org/vocab/quantitykind/> .\n";
   ttl << "@prefix s223: <http://data.ashrae.org/standard223#> .\n";
   ttl << "@prefix zea: <urn:zandrea:tool-profile#> .\n\n";

   for (const auto& profile : profiles) {
      ttl << shape_name_for_profile(profile) << "\n";
      ttl << "   a sh:NodeShape ;\n";
      ttl << "   zea:toolType " << turtle_literal(profile.id) << " ;\n";
      ttl << "   sh:name " << turtle_literal(profile.displayName) << " ;\n";

      for (const auto& targetClass : profile.focus.targetClasses) {
         ttl << "   sh:targetClass " << turtle_resource(targetClass) << " ;\n";
      }

      for (const auto& antecedent : profile.antecedents) {
         ttl << "   sh:property [\n";
         ttl << "      zea:requirementKind \"antecedent\" ;\n";
         ttl << "      zea:antecedentRole " << turtle_literal(antecedent.role) << " ;\n";
         ttl << "      zea:requiredToolType " << turtle_literal(antecedent.requiredToolProfileId) << " ;\n";
         ttl << "      sh:path " << antecedent.path.AsShaclPathTurtle() << " ;\n";
         ttl << "      sh:minCount " << (antecedent.required ? 1 : 0) << " ;\n";
         append_required_classes(ttl, antecedent.requiredClasses, "      ");
         ttl << "   ] ;\n";
      }

      for (const auto& point : profile.points) {
         ttl << "   sh:property [\n";
         ttl << "      zea:requirementKind \"point\" ;\n";
         ttl << "      zea:pointName " << turtle_literal(EPointNameToString(point.pointName)) << " ;\n";
         ttl << "      zea:pointValueKind "
             << turtle_literal(ToolPointValueKindToString(point.valueKind)) << " ;\n";
         ttl << "      sh:path " << point.path.AsShaclPathTurtle() << " ;\n";
         ttl << "      sh:minCount " << (point.required ? 1 : 0) << " ;\n";
         append_required_classes(ttl, point.requiredClasses, "      ");
         append_quantity_kind_node(ttl, point, "      ");
         ttl << "   ] ;\n";
      }

      ttl << "   zea:profileStatus \"draft\" .\n\n";
   }

   return ttl.str();
}

std::string GenerateBuiltInToolProfileShapesTurtle(void) {
   return GenerateToolProfileShapesTurtle(BuiltInToolProfiles());
}

std::string GenerateToolProfileDiscoverySparql(
   const std::vector<ToolProfile>& profiles,
   ToolProfileDiscoveryQueryMode mode
) {
   std::ostringstream query;

   query << "PREFIX qudt: <http://qudt.org/schema/qudt/>\n";
   query << "PREFIX quantitykind: <http://qudt.org/vocab/quantitykind/>\n";
   query << "PREFIX rdfs: <http://www.w3.org/2000/01/rdf-schema#>\n";
   query << "PREFIX s223: <http://data.ashrae.org/standard223#>\n";
   query << "PREFIX zea: <urn:zandrea:tool-profile#>\n";
   query << "\n";
   query << "SELECT DISTINCT ?profile ?equipment ?name ?label\n";
   query << "WHERE {\n";

   for (std::size_t i = 0; i < profiles.size(); ++i) {
      const auto& profile = profiles[i];
      if (i > 0) {
         query << "  UNION\n";
      }
      query << "  {\n";
      query << "    BIND(" << turtle_literal(profile.id) << " AS ?profile)\n";
      append_focus_patterns(query, profile);
      if (mode == ToolProfileDiscoveryQueryMode::CreatableTools) {
         query << "    ?equipment zea:name ?name .\n";
      } else {
         query << "    OPTIONAL { ?equipment zea:name ?name . }\n";
      }
      query << "    OPTIONAL { ?equipment rdfs:label ?label . }\n";

      if (mode == ToolProfileDiscoveryQueryMode::CreatableTools) {
         append_antecedent_patterns(query, profile);
         append_point_patterns(query, profile);
      }

      query << "  }\n";
   }

   query << "}\n";
   query << "ORDER BY ?profile ?equipment\n";

   return query.str();
}

std::string GenerateBuiltInToolProfileDiscoverySparql(ToolProfileDiscoveryQueryMode mode) {
   return GenerateToolProfileDiscoverySparql(BuiltInToolProfiles(), mode);
}

std::string GenerateToolProfileStartupConstructSparql(
   const std::vector<ToolProfile>& profiles,
   bool requirePointBindings
) {
   std::ostringstream query;

   // Startup uses a small private RDF bridge graph instead of binding directly
   // to C++ types in SPARQL.  s223Model.cpp parses these triples into
   // S223ToolStartupSpec while preserving RDF resources as dynamic subject keys.
   query << "PREFIX qudt: <http://qudt.org/schema/qudt/>\n";
   query << "PREFIX quantitykind: <http://qudt.org/vocab/quantitykind/>\n";
   query << "PREFIX rdfs: <http://www.w3.org/2000/01/rdf-schema#>\n";
   query << "PREFIX s223: <http://data.ashrae.org/standard223#>\n";
   query << "PREFIX zea: <urn:zandrea:tool-profile#>\n";
   query << "\n";
   query << "CONSTRUCT {\n";
   query << "  ?equipment <urn:zandrea:s223-startup#profile> ?profile .\n";
   query << "  ?equipment <urn:zandrea:s223-startup#name> ?name .\n";

   for (const auto& profile : profiles) {
      for (const auto& antecedent : profile.antecedents) {
         query << "  ?equipment <urn:zandrea:s223-startup#antecedent/"
               << antecedent.role << "> ?antecedent_"
               << sparql_variable_token(antecedent.role) << " .\n";
      }
   }

   query << "}\n";
   query << "WHERE {\n";

   for (std::size_t i = 0; i < profiles.size(); ++i) {
      const auto& profile = profiles[i];
      if (i > 0) {
         query << "  UNION\n";
      }
      query << "  {\n";
      query << "    BIND(" << turtle_literal(profile.id) << " AS ?profile)\n";
      append_focus_patterns(query, profile);
      query << "    ?equipment zea:name ?name .\n";
      append_antecedent_patterns(query, profile);
      if (requirePointBindings) {
         append_point_patterns(query, profile);
      }
      query << "  }\n";
   }

   query << "}\n";

   return query.str();
}

std::string GenerateBuiltInToolProfileStartupConstructSparql(bool requirePointBindings) {
   return GenerateToolProfileStartupConstructSparql(BuiltInToolProfiles(), requirePointBindings);
}

namespace {

void append_antecedent_patterns_always_optional(std::ostringstream& query, const ToolProfile& profile) {
   for (const auto& antecedent : profile.antecedents) {
      const auto var = "?antecedent_" + sparql_variable_token(antecedent.role);
      query << "    OPTIONAL {\n";
      query << "      ?equipment " << antecedent.path.AsSparqlPropertyPath()
            << " " << var << " .\n";
      append_class_patterns(query, var, antecedent.requiredClasses, "      ");
      query << "    }\n";
   }
}

void append_point_patterns_always_optional(std::ostringstream& query, const ToolProfile& profile) {
   for (const auto& point : profile.points) {
      const auto pointName = EPointNameToString(point.pointName);
      const auto var = "?point_" + sparql_variable_token(pointName);
      query << "    OPTIONAL {\n";
      query << "      ?equipment " << point.path.AsSparqlPropertyPath()
            << " " << var << " .\n";
      append_class_patterns(query, var, point.requiredClasses, "      ");
      for (const auto& quantityKind : point.requiredQuantityKinds) {
         query << "      " << var << " qudt:hasQuantityKind "
               << turtle_resource(quantityKind) << " .\n";
      }
      query << "    }\n";
   }
}

} // namespace

std::string GenerateToolProfileDiagnosticConstructSparql(
   const std::vector<ToolProfile>& profiles
) {
   std::ostringstream query;

   // Same private bridge vocabulary style as the startup CONSTRUCT, under a
   // distinct namespace so s223Model.cpp can tell the two graphs apart.
   query << "PREFIX qudt: <http://qudt.org/schema/qudt/>\n";
   query << "PREFIX quantitykind: <http://qudt.org/vocab/quantitykind/>\n";
   query << "PREFIX rdfs: <http://www.w3.org/2000/01/rdf-schema#>\n";
   query << "PREFIX s223: <http://data.ashrae.org/standard223#>\n";
   query << "PREFIX zea: <urn:zandrea:tool-profile#>\n";
   query << "\n";
   query << "CONSTRUCT {\n";
   query << "  ?equipment <urn:zandrea:s223-diagnostic#profile> ?profile .\n";
   query << "  ?equipment <urn:zandrea:s223-diagnostic#name> ?name .\n";

   for (const auto& profile : profiles) {
      for (const auto& antecedent : profile.antecedents) {
         query << "  ?equipment <urn:zandrea:s223-diagnostic#antecedent/"
               << antecedent.role << "> ?antecedent_"
               << sparql_variable_token(antecedent.role) << " .\n";
      }
      for (const auto& point : profile.points) {
         const auto pointName = EPointNameToString(point.pointName);
         query << "  ?equipment <urn:zandrea:s223-diagnostic#point/"
               << pointName << "> ?point_"
               << sparql_variable_token(pointName) << " .\n";
      }
   }

   query << "}\n";
   query << "WHERE {\n";

   for (std::size_t i = 0; i < profiles.size(); ++i) {
      const auto& profile = profiles[i];
      if (i > 0) {
         query << "  UNION\n";
      }
      query << "  {\n";
      query << "    BIND(" << turtle_literal(profile.id) << " AS ?profile)\n";
      append_focus_patterns(query, profile);
      query << "    OPTIONAL { ?equipment zea:name ?name . }\n";
      append_antecedent_patterns_always_optional(query, profile);
      append_point_patterns_always_optional(query, profile);
      query << "  }\n";
   }

   query << "}\n";

   return query.str();
}

std::string GenerateBuiltInToolProfileDiagnosticConstructSparql(void) {
   return GenerateToolProfileDiagnosticConstructSparql(BuiltInToolProfiles());
}

//END-OF-FILE ZZZZZ2ZZZZZZZZZ3ZZZZZZZZZ4ZZZZZZZZZ5ZZZZZZZZZ6ZZZZZZZZZ7ZZZZZZZZZ8ZZZZZZZZZ9ZZZZZZZZZCZZZZZ
