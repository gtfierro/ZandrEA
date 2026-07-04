//XXXXXXX1XXXXXXXXX2XXXXXXXXX3XXXXXXXXX4XXXXXXXXX5XXXXXXXXX6XXXXXXXXX7XXXXXXXXX8XXXXXXXXX9XXXXXXXXXCXXXXV
/* File summary:
   Implements ASHRAE 223 RDF/Turtle loading using shifty.
*/
/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8/////////9/////////C////V

#include "s223Model.hpp"
#include "toolAssembler.hpp"
#include "toolProfile.hpp"

#ifdef EA_HAVE_SHIFTY
#include <shifty/shifty.hpp>
#endif

#include <cstdlib>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace {

const char* const S223_ONTOLOGY_ENV = "EA_S223_ONTOLOGY_TTL";
const char* const S223_SITE_ENV = "EA_S223_SITE_TTL";
const char* const S223_PROFILE_SHAPES_ENV = "EA_S223_PROFILE_SHAPES_TTL";  // ':'-separated
const char* const QUDT_ALL_TURTLE_FILENAME = "qudt-all.ttl";
const char* const STARTUP_PROFILE_IRI = "urn:zandrea:s223-startup#profile";
const char* const STARTUP_NAME_IRI = "urn:zandrea:s223-startup#name";
const char* const STARTUP_ANTECEDENT_PREFIX = "urn:zandrea:s223-startup#antecedent/";
const char* const DIAGNOSTIC_PROFILE_IRI = "urn:zandrea:s223-diagnostic#profile";
const char* const DIAGNOSTIC_NAME_IRI = "urn:zandrea:s223-diagnostic#name";
const char* const DIAGNOSTIC_ANTECEDENT_PREFIX = "urn:zandrea:s223-diagnostic#antecedent/";
const char* const DIAGNOSTIC_POINT_PREFIX = "urn:zandrea:s223-diagnostic#point/";

struct S223StartupQueryResult {
   S223ModelLoadSummary loadSummary;
   std::string startupGraphNTriples;
   std::string diagnosticGraphNTriples;
   std::string siteGraphRawNTriples;
   std::string siteGraphInferredNTriples;
};

struct S223ProfileStartupMatch {
   std::string rdfResource;
   std::string profileId;
   std::string name;
   std::map<std::string, std::set<std::string> > antecedentsByRole;
};

// Diagnostic counterpart to S223ProfileStartupMatch: every candidate is kept
// (nothing is dropped for missing name/antecedents), and antecedent/point
// presence is recorded regardless of whether the requirement is required.
struct S223DiagnosticEquipmentMatch {
   std::string rdfResource;
   std::string profileId;
   bool hasName = false;
   std::string name;
   std::map<std::string, std::set<std::string> > antecedentsByRole;
   std::set<std::string> boundPointNames;
};

struct S223WitnessDiscoveryResult {
   ResolvedToolModel model;
   std::vector<S223ToolStartupSpec> tools;
   S223ToolDiagnosticReport diagnosticReport;
};

struct S223WitnessDiagnosticMatch {
   std::string focusNode;
   std::string shapeId;
   std::map<std::string, std::vector<std::string> > byRole;
};

struct TurtleToken {
   std::string text;
   bool literal = false;
};

std::optional<std::string> getenv_string(const char* name) {
   const char* value = std::getenv(name);
   if (value == nullptr || value[0] == '\0') {
      return std::nullopt;
   }
   return std::string(value);
}

void require_readable_file(const std::string& path) {
   if (!std::filesystem::is_regular_file(path)) {
      std::ostringstream msg;
      msg << "S223 Turtle file is not readable as a regular file: " << path;
      throw std::runtime_error(msg.str());
   }
}

std::vector<std::string> default_s223_shape_paths(const std::string& ontologyTurtlePath) {
   std::vector<std::string> paths;
   paths.push_back(ontologyTurtlePath);

   const auto ontologyPath = std::filesystem::path(ontologyTurtlePath);
   const auto qudtPath = ontologyPath.parent_path() / QUDT_ALL_TURTLE_FILENAME;
   paths.push_back(qudtPath.string());

   return paths;
}

std::string join_paths_for_display(const std::vector<std::string>& paths) {
   std::ostringstream out;
   for (std::size_t i = 0; i < paths.size(); ++i) {
      if (i != 0) {
         out << ":";
      }
      out << paths[i];
   }
   return out.str();
}

std::string read_turtle_files(const std::vector<std::string>& paths) {
   std::ostringstream combined;
   for (const auto& path : paths) {
      require_readable_file(path);
      std::ifstream in(path);
      combined << in.rdbuf() << "\n";
   }
   return combined.str();
}

std::string unescape_ntriples_literal(std::string value) {
   std::string unescaped;
   unescaped.reserve(value.size());

   for (std::size_t i = 0; i < value.size(); ++i) {
      if (value[i] == '\\' && i + 1 < value.size()) {
         const char escaped = value[++i];
         switch (escaped) {
            case 't': unescaped.push_back('\t'); break;
            case 'n': unescaped.push_back('\n'); break;
            case 'r': unescaped.push_back('\r'); break;
            case '"': unescaped.push_back('"'); break;
            case '\\': unescaped.push_back('\\'); break;
            default:
               unescaped.push_back(escaped);
               break;
         }
      } else {
         unescaped.push_back(value[i]);
      }
   }

   return unescaped;
}

bool parse_ntriples_iri(const std::string& line, std::size_t& pos, std::string& iri) {
   while (pos < line.size() && line[pos] == ' ') {
      ++pos;
   }
   if (pos >= line.size() || line[pos] != '<') {
      return false;
   }
   const auto end = line.find('>', pos + 1);
   if (end == std::string::npos) {
      return false;
   }
   iri = line.substr(pos + 1, end - pos - 1);
   pos = end + 1;
   return true;
}

bool parse_ntriples_object(const std::string& line, std::size_t& pos, std::string& object) {
   while (pos < line.size() && line[pos] == ' ') {
      ++pos;
   }

   if (pos < line.size() && line[pos] == '<') {
      return parse_ntriples_iri(line, pos, object);
   }

   if (pos >= line.size() || line[pos] != '"') {
      return false;
   }

   ++pos;
   std::string literal;
   bool escaped = false;
   while (pos < line.size()) {
      const char ch = line[pos++];
      if (ch == '"' && !escaped) {
         object = unescape_ntriples_literal(literal);
         return true;
      }
      literal.push_back(ch);
      escaped = (ch == '\\' && !escaped);
      if (ch != '\\') {
         escaped = false;
      }
   }

   return false;
}

std::string unwrap_witness_term(const std::string& term) {
   if (term.size() >= 2 && term.front() == '<' && term.back() == '>') {
      return term.substr(1, term.size() - 2);
   }
   if (!term.empty() && term.front() == '"') {
      const auto close = term.find('"', 1);
      if (close != std::string::npos) {
         return term.substr(1, close - 1);
      }
   }
   return term;
}

std::string unescape_turtle_string(const std::string& value) {
   std::string unescaped;
   unescaped.reserve(value.size());
   for (std::size_t i = 0; i < value.size(); ++i) {
      if (value[i] == '\\' && i + 1 < value.size()) {
         const char next = value[++i];
         switch (next) {
            case 'n': unescaped.push_back('\n'); break;
            case 'r': unescaped.push_back('\r'); break;
            case 't': unescaped.push_back('\t'); break;
            case '"': unescaped.push_back('"'); break;
            case '\\': unescaped.push_back('\\'); break;
            default:
               unescaped.push_back(next);
               break;
         }
      } else {
         unescaped.push_back(value[i]);
      }
   }
   return unescaped;
}

bool is_turtle_punctuation(char ch) {
   return ch == ';' || ch == ',' || ch == '.' || ch == '[' || ch == ']';
}

std::vector<TurtleToken> tokenize_validation_report_turtle(const std::string& turtle) {
   std::vector<TurtleToken> tokens;
   std::size_t i = 0;

   while (i < turtle.size()) {
      const unsigned char ch = static_cast<unsigned char>(turtle[i]);
      if (std::isspace(ch)) {
         ++i;
         continue;
      }
      if (turtle[i] == '#') {
         while (i < turtle.size() && turtle[i] != '\n') {
            ++i;
         }
         continue;
      }
      if (is_turtle_punctuation(turtle[i])) {
         tokens.push_back({ std::string(1, turtle[i]), false });
         ++i;
         continue;
      }
      if (turtle[i] == '<') {
         const auto start = i++;
         while (i < turtle.size() && turtle[i] != '>') {
            ++i;
         }
         if (i < turtle.size()) {
            ++i;
         }
         tokens.push_back({ turtle.substr(start, i - start), false });
         continue;
      }
      if (turtle[i] == '"') {
         ++i;
         std::string literal;
         bool escaped = false;
         while (i < turtle.size()) {
            const char c = turtle[i++];
            if (c == '"' && !escaped) {
               break;
            }
            literal.push_back(c);
            escaped = (c == '\\' && !escaped);
            if (c != '\\') {
               escaped = false;
            }
         }
         tokens.push_back({ unescape_turtle_string(literal), true });

         // Skip language/datatype suffixes; the endpoint exposes the lexical
         // message text and does not need the literal's RDF datatype.
         if (i < turtle.size() && turtle[i] == '@') {
            while (i < turtle.size()
                   && !std::isspace(static_cast<unsigned char>(turtle[i]))
                   && !is_turtle_punctuation(turtle[i])) {
               ++i;
            }
         } else if (i + 1 < turtle.size() && turtle[i] == '^' && turtle[i + 1] == '^') {
            i += 2;
            while (i < turtle.size()
                   && !std::isspace(static_cast<unsigned char>(turtle[i]))
                   && !is_turtle_punctuation(turtle[i])) {
               ++i;
            }
         }
         continue;
      }

      const auto start = i;
      while (i < turtle.size()
             && !std::isspace(static_cast<unsigned char>(turtle[i]))
             && !is_turtle_punctuation(turtle[i])) {
         ++i;
      }
      tokens.push_back({ turtle.substr(start, i - start), false });
   }

   return tokens;
}

std::string normalize_report_term(const TurtleToken& token) {
   if (token.literal) {
      return token.text;
   }
   return unwrap_witness_term(token.text);
}

std::string shacl_local_name(const TurtleToken& token) {
   const auto term = normalize_report_term(token);
   const std::string shPrefix = "sh:";
   const std::string shIriPrefix = "http://www.w3.org/ns/shacl#";
   if (term.rfind(shPrefix, 0) == 0) {
      return term.substr(shPrefix.size());
   }
   if (term.rfind(shIriPrefix, 0) == 0) {
      return term.substr(shIriPrefix.size());
   }
   return term;
}

void add_validation_report_triple(
   std::map<std::string, S223ValidationResultRecord>& byNode,
   std::vector<std::string>& order,
   const TurtleToken& subject,
   const TurtleToken& predicate,
   const TurtleToken& object
) {
   const auto subjectText = normalize_report_term(subject);
   if (subjectText.empty()) {
      return;
   }

   auto& record = byNode[subjectText];
   if (record.resultNode.empty()) {
      record.resultNode = subjectText;
      order.push_back(subjectText);
   }

   const auto pred = predicate.text;
   if (pred == "a" && object.text == "sh:ValidationResult") {
      return;
   }
   if (pred == "sh:resultSeverity") {
      record.severity = shacl_local_name(object);
   } else if (pred == "sh:sourceConstraintComponent") {
      record.sourceConstraintComponent = shacl_local_name(object);
   } else if (pred == "sh:sourceShape") {
      record.sourceShape = normalize_report_term(object);
   } else if (pred == "sh:focusNode") {
      record.focusNode = normalize_report_term(object);
   } else if (pred == "sh:resultPath") {
      record.hasResultPath = true;
      record.resultPath = normalize_report_term(object);
   } else if (pred == "sh:value") {
      record.hasValue = true;
      record.value = normalize_report_term(object);
   } else if (pred == "sh:resultMessage") {
      record.messages.push_back(normalize_report_term(object));
   }
}

std::vector<S223ValidationResultRecord> parse_validation_report_turtle(
   const std::string& reportTurtle
) {
   const auto tokens = tokenize_validation_report_turtle(reportTurtle);
   std::map<std::string, S223ValidationResultRecord> byNode;
   std::set<std::string> validationResultNodes;
   std::vector<std::string> order;

   std::size_t i = 0;
   while (i < tokens.size()) {
      if (tokens[i].text == "@prefix" || tokens[i].text == "PREFIX") {
         while (i < tokens.size() && tokens[i].text != ".") {
            ++i;
         }
         if (i < tokens.size()) {
            ++i;
         }
         continue;
      }
      if (tokens[i].text == "." || tokens[i].text == ";" || tokens[i].text == ",") {
         ++i;
         continue;
      }

      const TurtleToken subject = tokens[i++];
      while (i < tokens.size() && tokens[i].text != ".") {
         if (tokens[i].text == ";") {
            ++i;
            continue;
         }
         const TurtleToken predicate = tokens[i++];
         while (i < tokens.size()) {
            const TurtleToken object = tokens[i++];
            if (predicate.text == "a" && object.text == "sh:ValidationResult") {
               validationResultNodes.insert(normalize_report_term(subject));
            }
            add_validation_report_triple(byNode, order, subject, predicate, object);

            if (i < tokens.size() && tokens[i].text == ",") {
               ++i;
               continue;
            }
            break;
         }
      }
      if (i < tokens.size() && tokens[i].text == ".") {
         ++i;
      }
   }

   std::vector<S223ValidationResultRecord> results;
   for (const auto& resultNode : order) {
      if (validationResultNodes.find(resultNode) != validationResultNodes.end()) {
         results.push_back(byNode[resultNode]);
      }
   }
   return results;
}

#ifdef EA_HAVE_SHIFTY
S223AlgebraValidationSummary algebra_summary_from_result(
   const shifty::AlgebraResult& algebra
) {
   S223AlgebraValidationSummary summary;
   summary.engineAvailable = true;
   summary.validationRun = true;
   summary.conforms = algebra.conforms();
   summary.resultsText = algebra.results_text();

   for (const auto& violation : algebra.violations()) {
      S223AlgebraViolationRecord record;
      record.focusNode = violation.focus_node;
      record.shapeName = violation.shape_name;
      record.severity = violation.severity;

      for (const auto& reason : violation.reasons) {
         record.reasons.push_back({
            reason.value,
            reason.path,
            reason.message,
            reason.author_message,
            reason.severity
         });
      }

      summary.violations.push_back(std::move(record));
   }

   return summary;
}
#endif

S223StartupQueryResult run_shacl_inference_validation_and_startup_query(
   const S223ModelLoadConfig& config
) {
#ifdef EA_HAVE_SHIFTY
   const auto shapePaths = default_s223_shape_paths(config.ontologyTurtlePath);
   const auto shapes = read_turtle_files(shapePaths);
   require_readable_file(config.siteTurtlePath);

   shifty::PreparedValidator validator(shapes, shifty::RdfFormat::Turtle);

   // Quad count only, kept separate from the shapes prepared above: shifty's
   // PreparedValidator has no triple-count accessor of its own, so this loads
   // the shape files a second time through shifty's parser rather than a second
   // library (rdf4cpp's own Turtle parser previously crashed on real ASHRAE
   // 223 ontology content, so it is no longer used here at all).
   shifty::Dataset ontologyDataset;
   for (const auto& shapePath : shapePaths) {
      ontologyDataset.load_file(shapePath, shifty::RdfFormat::Turtle);
   }

   shifty::Dataset dataset;
   dataset.load_file(config.siteTurtlePath, shifty::RdfFormat::Turtle);
   const std::string rawSiteNTriples = dataset.ntriples();

   shifty::ValidationOptions options;
   options.graph_mode = shifty::GraphMode::Union;
   options.run_inference = true;

   // validate() takes Dataset by const&, but SHACL-AF inference materializes
   // inferred triples into the underlying shifty dataset regardless (the C++
   // constness here does not reach through the FFI handle) -- so dataset now
   // holds site triples plus everything inference added, and ntriples() after
   // this call differs from rawSiteNTriples above whenever inference fired.
   const auto validation = validator.validate(dataset, options);
   const auto reportTurtle = validation.report_turtle();

   auto algebraOptions = options;
   algebraOptions.run_inference = false;
   const auto algebra = validator.validate_algebra(dataset, algebraOptions);
   const std::string inferredSiteNTriples = dataset.ntriples();

   const auto startupGraph = dataset.query(
      GenerateBuiltInToolProfileStartupConstructSparql(false)
   );
   const auto diagnosticGraph = dataset.query(
      GenerateBuiltInToolProfileDiagnosticConstructSparql()
   );

   return {
      {
         { join_paths_for_display(shapePaths), ontologyDataset.size() },
         { config.siteTurtlePath, dataset.size() },
         {
            true,
            true,
            true,
            validation.conforms(),
            validator.diagnostics_json(),
            validation.results_text(),
            reportTurtle,
            parse_validation_report_turtle(reportTurtle),
            algebra_summary_from_result(algebra)
         }
      },
      startupGraph.data(),
      diagnosticGraph.data(),
      rawSiteNTriples,
      inferredSiteNTriples
   };
#else
   (void)config;
   return {
      {
         { join_paths_for_display(default_s223_shape_paths(config.ontologyTurtlePath)), 0 },
         { config.siteTurtlePath, 0 },
         {
            false,
            false,
            false,
            false,
            "",
            "",
            "",
            {},
            {
               false,
               false,
               false,
               "",
               {}
            }
         }
      },
      "",
      "",
      "",
      ""
   };
#endif
}

std::map<std::string, S223ProfileStartupMatch> profile_matches_from_startup_graph(
   const std::string& startupGraphNTriples
) {
   // The generated startup CONSTRUCT emits a compact N-Triples graph:
   // equipment -> profile id, equipment -> zea:name, and equipment ->
   // antecedent role -> antecedent RDF resource.  Parse only that small
   // generated vocabulary here instead of creating a second general RDF model.
   std::map<std::string, S223ProfileStartupMatch> matchesByResource;

   std::istringstream lines(startupGraphNTriples);
   std::string line;
   while (std::getline(lines, line)) {
      std::size_t pos = 0;
      std::string subject;
      std::string predicate;
      std::string object;

      if (!parse_ntriples_iri(line, pos, subject)
          || !parse_ntriples_iri(line, pos, predicate)
          || !parse_ntriples_object(line, pos, object)) {
         continue;
      }

      auto& match = matchesByResource[subject];
      match.rdfResource = subject;

      // STARTUP_* predicates are private ZandrEA bridge terms produced by
      // GenerateToolProfileStartupConstructSparql().
      if (predicate == STARTUP_PROFILE_IRI) {
         match.profileId = object;
      } else if (predicate == STARTUP_NAME_IRI) {
         match.name = object;
      } else if (predicate.rfind(STARTUP_ANTECEDENT_PREFIX, 0) == 0) {
         const auto role = predicate.substr(std::string(STARTUP_ANTECEDENT_PREFIX).size());
         match.antecedentsByRole[role].insert(object);
      }
   }

   for (auto it = matchesByResource.begin(); it != matchesByResource.end(); ) {
      // A startup candidate without a profile or zea:name is not constructible:
      // profile selects the C++ tool class, name becomes display text.
      if (it->second.profileId.empty() || it->second.name.empty()) {
         it = matchesByResource.erase(it);
      } else {
         ++it;
      }
   }

   return matchesByResource;
}

std::map<std::string, S223DiagnosticEquipmentMatch> diagnostic_matches_from_graph(
   const std::string& diagnosticGraphNTriples
) {
   // Same small-vocabulary N-Triples parse as profile_matches_from_startup_graph,
   // but nothing is dropped here: every candidate that matched a focus class
   // survives, including ones missing zea:name or required antecedents/points.
   std::map<std::string, S223DiagnosticEquipmentMatch> matchesByResource;

   std::istringstream lines(diagnosticGraphNTriples);
   std::string line;
   while (std::getline(lines, line)) {
      std::size_t pos = 0;
      std::string subject;
      std::string predicate;
      std::string object;

      if (!parse_ntriples_iri(line, pos, subject)
          || !parse_ntriples_iri(line, pos, predicate)
          || !parse_ntriples_object(line, pos, object)) {
         continue;
      }

      auto& match = matchesByResource[subject];
      match.rdfResource = subject;

      if (predicate == DIAGNOSTIC_PROFILE_IRI) {
         match.profileId = object;
      } else if (predicate == DIAGNOSTIC_NAME_IRI) {
         match.hasName = true;
         match.name = object;
      } else if (predicate.rfind(DIAGNOSTIC_ANTECEDENT_PREFIX, 0) == 0) {
         const auto role = predicate.substr(std::string(DIAGNOSTIC_ANTECEDENT_PREFIX).size());
         match.antecedentsByRole[role].insert(object);
      } else if (predicate.rfind(DIAGNOSTIC_POINT_PREFIX, 0) == 0) {
         const auto pointName = predicate.substr(std::string(DIAGNOSTIC_POINT_PREFIX).size());
         match.boundPointNames.insert(pointName);
      }
   }

   return matchesByResource;
}

S223ToolDiagnosticReport build_diagnostic_report(
   const std::string& startupGraphNTriples,
   const std::string& diagnosticGraphNTriples
) {
   // The startup-graph parse (filtered to profile+name present) tells us which
   // bound antecedent targets are themselves creatable-ish; that's the same
   // gate tool_specs_from_startup_graph() uses before topological sort.
   const auto creatableMatches = profile_matches_from_startup_graph(startupGraphNTriples);
   const auto diagnosticMatches = diagnostic_matches_from_graph(diagnosticGraphNTriples);

   S223ToolDiagnosticReport report;

   for (const auto& [resource, match] : diagnosticMatches) {
      if (match.profileId.empty()) {
         // ?profile is BIND'd unconditionally in the WHERE clause whenever the
         // mandatory focus-class pattern matches, so this should not happen.
         continue;
      }

      const auto profile = FindBuiltInToolProfile(match.profileId);
      if (!profile.has_value()) {
         continue;
      }

      S223ToolDiagnosticCandidate candidate;
      candidate.rdfResource = resource;
      candidate.profileId = match.profileId;
      candidate.profileDisplayName = profile->displayName;
      candidate.hasName = match.hasName;
      candidate.name = match.name;

      bool creatable = match.hasName;
      if (!match.hasName) {
         candidate.blockingReasons.push_back("missing zea:name");
      }

      for (const auto& antecedent : profile->antecedents) {
         S223ToolDiagnosticAntecedent diag;
         diag.role = antecedent.role;
         diag.requiredToolProfileId = antecedent.requiredToolProfileId;
         diag.required = antecedent.required;
         diag.boundResourceIsCreatable = false;

         const auto roleIt = match.antecedentsByRole.find(antecedent.role);
         diag.bound = (roleIt != match.antecedentsByRole.end()) && !roleIt->second.empty();

         if (diag.bound) {
            // Prefer a bound resource that is itself a creatable instance of
            // the required profile; fall back to just reporting the first
            // bound resource if none qualify, so the report still shows what
            // was actually bound.
            std::string chosen;
            for (const auto& boundResource : roleIt->second) {
               const auto found = creatableMatches.find(boundResource);
               if (found != creatableMatches.end()
                   && found->second.profileId == antecedent.requiredToolProfileId) {
                  chosen = boundResource;
                  diag.boundResourceIsCreatable = true;
                  break;
               }
            }
            if (chosen.empty()) {
               chosen = *roleIt->second.begin();
            }
            diag.rdfResource = chosen;
         }

         if (antecedent.required && !diag.bound) {
            creatable = false;
            candidate.blockingReasons.push_back(
               "missing required antecedent '" + antecedent.role + "'"
            );
         } else if (antecedent.required && diag.bound && !diag.boundResourceIsCreatable) {
            creatable = false;
            candidate.blockingReasons.push_back(
               "required antecedent '" + antecedent.role + "' resolves to " + diag.rdfResource
               + " which is not itself a creatable " + antecedent.requiredToolProfileId + " instance"
            );
         }

         candidate.antecedents.push_back(std::move(diag));
      }

      for (const auto& point : profile->points) {
         // Points are informational only: they do not currently gate
         // construction (tool_specs_from_startup_graph doesn't check them),
         // so a missing point never affects `creatable` here either.
         S223ToolDiagnosticPoint diag;
         diag.pointName = EPointNameToString(point.pointName);
         diag.required = point.required;
         diag.bound = match.boundPointNames.count(diag.pointName) > 0;
         candidate.points.push_back(std::move(diag));
      }

      candidate.creatable = creatable;
      report.candidates.push_back(std::move(candidate));
   }

   std::sort(
      report.candidates.begin(),
      report.candidates.end(),
      [](const S223ToolDiagnosticCandidate& a, const S223ToolDiagnosticCandidate& b) {
         if (a.profileId != b.profileId) {
            return a.profileId < b.profileId;
         }
         return a.rdfResource < b.rdfResource;
      }
   );

   return report;
}

std::string tool_profile_id_for_shape(
   const CModuleRegistry& modules,
   const std::string& profileShapeId
) {
   const auto* module = modules.Find(profileShapeId);
   return module == nullptr ? profileShapeId : module->Manifest().toolProfileId;
}

std::string tool_profile_display_name(const std::string& toolProfileId) {
   const auto profile = FindBuiltInToolProfile(toolProfileId);
   return profile.has_value() ? profile->displayName : toolProfileId;
}

std::map<std::string, S223WitnessDiagnosticMatch> witness_matches_by_focus(
   const std::vector<RoleWitness>& witnesses
) {
   std::map<std::string, S223WitnessDiagnosticMatch> matches;

   for (const auto& witness : witnesses) {
      const auto focus = unwrap_witness_term(witness.focusNode);
      auto& match = matches[focus];
      match.focusNode = focus;
      match.shapeId = unwrap_witness_term(witness.shapeId);

      auto& roleValues = match.byRole[witness.roleName];
      for (const auto& value : witness.valueNodes) {
         roleValues.push_back(unwrap_witness_term(value));
      }
   }

   return matches;
}

S223ToolDiagnosticReport build_witness_diagnostic_report(
   const std::vector<RoleWitness>& witnesses,
   const ResolvedToolModel& model,
   const CModuleRegistry& modules
) {
   const auto matches = witness_matches_by_focus(witnesses);

   std::map<std::string, const ResolvedTool*> creatableByFocus;
   for (const auto& tool : model.tools) {
      creatableByFocus[tool.focusNode] = &tool;
   }

   std::map<std::string, std::vector<std::string> > diagnosticsByFocus;
   for (const auto& diagnostic : model.diagnostics) {
      diagnosticsByFocus[diagnostic.focusNode].push_back(diagnostic.message);
   }

   S223ToolDiagnosticReport report;

   for (const auto& [focus, match] : matches) {
      const auto* module = modules.Find(match.shapeId);
      if (module == nullptr) {
         continue;
      }

      const auto& manifest = module->Manifest();
      S223ToolDiagnosticCandidate candidate;
      candidate.rdfResource = focus;
      candidate.profileId = manifest.toolProfileId;
      candidate.profileDisplayName = tool_profile_display_name(manifest.toolProfileId);

      const auto nameIt = match.byRole.find("name");
      candidate.hasName = (nameIt != match.byRole.end()) && !nameIt->second.empty();
      if (candidate.hasName) {
         candidate.name = nameIt->second.front();
      }

      const auto creatableIt = creatableByFocus.find(focus);
      candidate.creatable = creatableIt != creatableByFocus.end();

      const auto diagIt = diagnosticsByFocus.find(focus);
      if (diagIt != diagnosticsByFocus.end()) {
         candidate.blockingReasons.insert(
            candidate.blockingReasons.end(),
            diagIt->second.begin(),
            diagIt->second.end()
         );
      }

      for (const auto& antecedent : manifest.antecedents) {
         S223ToolDiagnosticAntecedent diag;
         diag.role = antecedent.id.name;
         diag.requiredToolProfileId =
            tool_profile_id_for_shape(modules, antecedent.requiredProfileShapeId);
         diag.required = antecedent.required;
         diag.boundResourceIsCreatable = false;

         const auto roleIt = match.byRole.find(antecedent.id.name);
         diag.bound = roleIt != match.byRole.end() && !roleIt->second.empty();
         if (diag.bound) {
            diag.rdfResource = roleIt->second.front();
            for (const auto& boundResource : roleIt->second) {
               const auto boundTool = creatableByFocus.find(boundResource);
               if (boundTool != creatableByFocus.end()
                   && boundTool->second->shapeId == antecedent.requiredProfileShapeId) {
                  diag.rdfResource = boundResource;
                  diag.boundResourceIsCreatable = true;
                  break;
               }
            }
         }

         candidate.antecedents.push_back(std::move(diag));
      }

      for (const auto& point : manifest.points) {
         S223ToolDiagnosticPoint diag;
         diag.pointName = EPointNameToString(point.pointName);
         diag.required = point.required;
         const auto roleIt = match.byRole.find(point.id.name);
         diag.bound = roleIt != match.byRole.end() && !roleIt->second.empty();
         candidate.points.push_back(std::move(diag));
      }

      report.candidates.push_back(std::move(candidate));
   }

   std::sort(
      report.candidates.begin(),
      report.candidates.end(),
      [](const S223ToolDiagnosticCandidate& a, const S223ToolDiagnosticCandidate& b) {
         if (a.profileId != b.profileId) {
            return a.profileId < b.profileId;
         }
         return a.rdfResource < b.rdfResource;
      }
   );

   return report;
}

std::vector<S223AntecedentStartupSpec> antecedent_specs_for_match(
   const S223ProfileStartupMatch& match,
   const std::map<std::string, S223ProfileStartupMatch>& matchesByResource
) {
   std::vector<S223AntecedentStartupSpec> antecedents;

   for (const auto& [role, resources] : match.antecedentsByRole) {
      for (const auto& resource : resources) {
         std::string name;
         const auto found = matchesByResource.find(resource);
         if (found != matchesByResource.end()) {
            // Fill display text when the antecedent is itself a matched tool.
            // The RDF resource remains the stable lookup key either way.
            name = found->second.name;
         }

         antecedents.push_back({
            role,
            resource,
            name
         });
      }
   }

   return antecedents;
}

std::optional<ToolProfile> profile_for_match(
   const S223ProfileStartupMatch& match
) {
   return FindBuiltInToolProfile(match.profileId);
}

std::vector<std::string> dependency_resources_for_requirement(
   const S223ProfileStartupMatch& match,
   const AntecedentRequirement& requirement,
   const std::map<std::string, S223ProfileStartupMatch>& matchesByResource
) {
   // Convert a profile-level antecedent requirement into concrete dependency
   // edges for one matched resource.  Example: if vav_ibal requires air_source
   // of tool profile ahu_ibal, only bound air_source resources that are also
   // matched ahu_ibal candidates become dependencies.
   std::vector<std::string> dependencies;
   const auto roleIt = match.antecedentsByRole.find(requirement.role);

   if (roleIt == match.antecedentsByRole.end()) {
      if (requirement.required) {
         std::ostringstream msg;
         msg << "S223 startup candidate " << match.rdfResource
             << " matched profile " << match.profileId
             << " but has no required antecedent role " << requirement.role;
         throw std::runtime_error(msg.str());
      }
      return dependencies;
   }

   for (const auto& resource : roleIt->second) {
      const auto antecedentIt = matchesByResource.find(resource);
      if (antecedentIt == matchesByResource.end()) {
         continue;
      }

      if (antecedentIt->second.profileId == requirement.requiredToolProfileId) {
         dependencies.push_back(resource);
      }
   }

   if (requirement.required && dependencies.empty()) {
      std::ostringstream msg;
      msg << "S223 startup candidate " << match.rdfResource
          << " matched profile " << match.profileId
          << " but required antecedent role " << requirement.role
          << " did not bind a creatable " << requirement.requiredToolProfileId
          << " tool instance";
      throw std::runtime_error(msg.str());
   }

   return dependencies;
}

std::map<std::string, std::set<std::string> > dependency_graph_for_matches(
   const std::map<std::string, S223ProfileStartupMatch>& matchesByResource
) {
   // Graph representation: key = matched tool resource, values = resources that
   // must be constructed first.  Edges are inferred from ToolProfile antecedent
   // type requirements, not from C++ equipment enums or hardcoded class names.
   std::map<std::string, std::set<std::string> > dependenciesByResource;

   for (const auto& [resource, match] : matchesByResource) {
      const auto profile = profile_for_match(match);
      if (!profile.has_value()) {
         std::ostringstream msg;
         msg << "S223 startup graph referenced unknown tool profile "
             << match.profileId << " for " << resource;
         throw std::runtime_error(msg.str());
      }

      dependenciesByResource.emplace(resource, std::set<std::string>{});

      // Build dependencies from profile types, not from C++ constructor names.
      // If this profile says role R requires tool profile T, then each matched
      // antecedent resource with profile T must be constructed before resource.
      for (const auto& antecedent : profile->antecedents) {
         for (const auto& dependency : dependency_resources_for_requirement(
                 match,
                 antecedent,
                 matchesByResource
              )) {
            dependenciesByResource[resource].insert(dependency);
         }
      }
   }

   return dependenciesByResource;
}

std::vector<std::string> topologically_sorted_resources(
   const std::map<std::string, std::set<std::string> >& dependenciesByResource
) {
   // DFS topological sort over resource-level dependencies.  A back-edge means
   // two or more tool instances depend on each other as antecedents, which would
   // make construction order impossible.
   enum class VisitState {
      Visiting,
      Visited
   };

   std::map<std::string, VisitState> states;
   std::vector<std::string> sorted;
   std::vector<std::string> stack;

   std::function<void(const std::string&)> visit = [&](const std::string& resource) {
      const auto stateIt = states.find(resource);
      if (stateIt != states.end()) {
         if (stateIt->second == VisitState::Visiting) {
            std::ostringstream msg;
            msg << "S223 tool antecedent graph contains a cycle involving "
                << resource;
            if (!stack.empty()) {
               msg << " along path";
               for (const auto& item : stack) {
                  msg << " -> " << item;
               }
               msg << " -> " << resource;
            }
            throw std::runtime_error(msg.str());
         }
         return;
      }

      states.emplace(resource, VisitState::Visiting);
      stack.push_back(resource);

      const auto depsIt = dependenciesByResource.find(resource);
      if (depsIt != dependenciesByResource.end()) {
         for (const auto& dependency : depsIt->second) {
            visit(dependency);
         }
      }

      stack.pop_back();
      states[resource] = VisitState::Visited;
      sorted.push_back(resource);
   };

   for (const auto& [resource, dependencies] : dependenciesByResource) {
      (void)dependencies;
      visit(resource);
   }

   return sorted;
}

S223ToolStartupSpec tool_spec_for_match(
   const S223ProfileStartupMatch& match,
   const std::map<std::string, S223ProfileStartupMatch>& matchesByResource
) {
   // Preserve RDF identity at the application boundary.  Tool constructors use
   // rdfResource as the dynamic subject key instead of assigning ERealName.
   return {
      match.profileId,
      match.rdfResource,
      match.name,
      antecedent_specs_for_match(match, matchesByResource)
   };
}

std::vector<S223ToolStartupSpec> tool_specs_from_startup_graph(
   const std::string& startupGraphNTriples
) {
   const auto matchesByResource = profile_matches_from_startup_graph(startupGraphNTriples);

   if (matchesByResource.empty()) {
      throw std::runtime_error("S223 generated profile discovery did not find any creatable tool candidates");
   }

   const auto dependenciesByResource = dependency_graph_for_matches(matchesByResource);
   const auto sortedResources = topologically_sorted_resources(dependenciesByResource);
   std::vector<S223ToolStartupSpec> tools;
   tools.reserve(sortedResources.size());

   for (const auto& resource : sortedResources) {
      tools.push_back(tool_spec_for_match(matchesByResource.at(resource), matchesByResource));
   }

   if (tools.empty()) {
      throw std::runtime_error("S223 generated profile discovery did not produce any tool specs");
   }

   return tools;
}

// Profile/witness-driven discovery: the replacement for the generated-CONSTRUCT
// path above.  Loads the zea application-profile shapes, runs the shifty witness
// pass over the inferred site graph, reconciles/orders via the analysis-module
// assembler, and projects onto the same S223ToolStartupSpec and diagnostic-report
// surfaces the rest of the application already consumes.
S223WitnessDiscoveryResult discover_tools_via_profile_witnesses(
      const std::vector<std::string>& shapeFiles, const std::string& siteFile) {
#ifdef EA_HAVE_SHIFTY
   std::string shapes;
   for (const auto& shapeFile : shapeFiles) {
      require_readable_file(shapeFile);
      std::ifstream in(shapeFile);
      std::ostringstream buffer;
      buffer << in.rdbuf();
      shapes += buffer.str();
      shapes += "\n";
   }

   shifty::PreparedValidator validator(shapes, shifty::RdfFormat::Turtle);

   shifty::Dataset dataset;
   dataset.load_file(siteFile, shifty::RdfFormat::Turtle);

   shifty::ValidationOptions options;
   options.key_path = "zea:roleName";
   options.run_inference = true;

   std::vector<RoleWitness> witnesses;
   for (const auto& witness : validator.witnesses(dataset, options)) {
      witnesses.push_back(
         { witness.focus_node, witness.shape_id, witness.key, witness.value_nodes });
   }

   auto model = AssembleTools(witnesses, BuiltInAnalysisModules());
   std::cout << "S223 profile/witness discovery: " << witnesses.size()
             << " witness row(s) -> " << model.tools.size() << " tool(s), "
             << model.diagnostics.size() << " diagnostic(s)" << std::endl;
   for (const auto& diagnostic : model.diagnostics) {
      std::cout << "  S223 discovery diagnostic [" << diagnostic.focusNode << "]: "
                << diagnostic.message << std::endl;
   }

   auto tools = StartupSpecsFromModel(model);
   auto diagnosticReport = build_witness_diagnostic_report(
      witnesses,
      model,
      BuiltInAnalysisModules()
   );

   return {
      std::move(model),
      std::move(tools),
      std::move(diagnosticReport)
   };
#else
   (void)shapeFiles;
   (void)siteFile;
   throw std::runtime_error(
      "S223 profile/witness tool discovery requires a build with shifty available");
#endif
}

} // namespace

std::size_t S223ModelLoadSummary::TotalQuadCount(void) const {
   return ontology.quadCount + site.quadCount;
}

std::size_t S223ToolDiagnosticReport::CreatableCount(void) const {
   return static_cast<std::size_t>(std::count_if(
      candidates.begin(),
      candidates.end(),
      [](const S223ToolDiagnosticCandidate& candidate) { return candidate.creatable; }
   ));
}

std::optional<S223ModelLoadConfig> ReadS223ModelLoadConfigFromEnvironment(void) {
   auto ontologyPath = getenv_string(S223_ONTOLOGY_ENV);
   auto sitePath = getenv_string(S223_SITE_ENV);

   if (!ontologyPath.has_value() && !sitePath.has_value()) {
      return std::nullopt;
   }

   if (!ontologyPath.has_value() || !sitePath.has_value()) {
      std::ostringstream msg;
      msg << "Both " << S223_ONTOLOGY_ENV << " and " << S223_SITE_ENV
          << " must be set to enable ASHRAE 223 startup loading";
      throw std::runtime_error(msg.str());
   }

   std::vector<std::string> profileShapePaths;
   if (auto shapes = getenv_string(S223_PROFILE_SHAPES_ENV); shapes.has_value()) {
      std::stringstream stream(*shapes);
      std::string entry;
      while (std::getline(stream, entry, ':')) {
         if (!entry.empty()) {
            profileShapePaths.push_back(entry);
         }
      }
   }

   return S223ModelLoadConfig{ *ontologyPath, *sitePath, std::move(profileShapePaths) };
}

S223ModelLoadSummary LoadS223ModelFromTurtleFiles(const S223ModelLoadConfig& config) {
   return run_shacl_inference_validation_and_startup_query(config).loadSummary;
}

S223ApplicationStartupModel LoadS223ApplicationStartupModel(const S223ModelLoadConfig& config) {
   const auto shaclResult = run_shacl_inference_validation_and_startup_query(config);
   if (!shaclResult.loadSummary.shacl.engineAvailable) {
      throw std::runtime_error("ASHRAE 223 startup loading requires a build with shifty available");
   }

   // Tool discovery: profile/witness-driven when zea profile shapes are
   // configured, otherwise the legacy generated-CONSTRUCT discovery.  Both feed
   // the same S223ToolStartupSpec surface and the same CTool_* constructors.
   std::vector<S223ToolStartupSpec> tools;
   S223ToolDiagnosticReport diagnosticReport;
   if (config.profileShapeTurtlePaths.empty()) {
      tools = tool_specs_from_startup_graph(shaclResult.startupGraphNTriples);
      diagnosticReport = build_diagnostic_report(
         shaclResult.startupGraphNTriples,
         shaclResult.diagnosticGraphNTriples
      );
   } else {
      auto witnessDiscovery = discover_tools_via_profile_witnesses(
         config.profileShapeTurtlePaths,
         config.siteTurtlePath
      );
      tools = std::move(witnessDiscovery.tools);
      diagnosticReport = std::move(witnessDiscovery.diagnosticReport);
   }

   return {
      shaclResult.loadSummary,
      std::move(tools),
      std::move(diagnosticReport),
      shaclResult.siteGraphRawNTriples,
      shaclResult.siteGraphInferredNTriples
   };
}

//END-OF-FILE ZZZZZ2ZZZZZZZZZ3ZZZZZZZZZ4ZZZZZZZZZ5ZZZZZZZZZ6ZZZZZZZZZ7ZZZZZZZZZ8ZZZZZZZZZ9ZZZZZZZZZCZZZZZ
