//XXXXXXX1XXXXXXXXX2XXXXXXXXX3XXXXXXXXX4XXXXXXXXX5XXXXXXXXX6XXXXXXXXX7XXXXXXXXX8XXXXXXXXX9XXXXXXXXXCXXXXV
/* File summary:
   Declares a small ASHRAE 223 RDF/Turtle loading boundary for future SIF/application configuration work.
*/
/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8/////////9/////////C////V

#ifndef S223MODEL_HPP
#define S223MODEL_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

struct S223FileLoadSummary {
   std::string path;
   std::size_t quadCount;
};

struct S223ShaclSummary {
   bool engineAvailable;
   bool inferenceRun;
   bool validationRun;
   bool conforms;
   std::string diagnosticsJson;
   std::string resultsText;
   std::string reportTurtle;
};

struct S223ModelLoadConfig {
   std::string ontologyTurtlePath;
   std::string siteTurtlePath;

   // Optional zea application-profile shape files (zea-core.ttl + the profile
   // node shapes).  When non-empty, startup tool discovery runs via the shifty
   // witness API against these shapes instead of the legacy generated-CONSTRUCT
   // discovery; the existing CTool_* constructors are used either way.
   std::vector<std::string> profileShapeTurtlePaths;
};

struct S223ModelLoadSummary {
   S223FileLoadSummary ontology;
   S223FileLoadSummary site;
   S223ShaclSummary shacl;

   std::size_t TotalQuadCount(void) const;
};

// One relationship from a discovered tool candidate to another RDF resource.
// If that resource is also matched as a tool candidate, name is populated from
// its zea:name and can be used for display; rdfResource remains the stable key.
struct S223AntecedentStartupSpec {
   std::string role;
   std::string rdfResource;
   std::string name;
};

// Model-native tool instance data produced by the generated startup CONSTRUCT.
// rdfResource is the internal dynamic subject key; name is display text from
// zea:name.  No ERealName enum slot is assigned here.
struct S223ToolStartupSpec {
   std::string toolProfileId;
   std::string rdfResource;
   std::string name;
   std::vector<S223AntecedentStartupSpec> antecedents;
};

// Diagnostic view of one antecedent requirement for one candidate equipment
// resource, regardless of whether that requirement was satisfied.  Lets a
// debugging report explain why a candidate was or was not creatable.
struct S223ToolDiagnosticAntecedent {
   std::string role;
   std::string requiredToolProfileId;
   bool required;
   bool bound;
   std::string rdfResource;         // valid iff bound
   bool boundResourceIsCreatable;   // valid iff bound
};

// Diagnostic view of one point requirement. Points are informational only:
// they do not currently gate tool creation (see S223ToolStartupSpec).
struct S223ToolDiagnosticPoint {
   std::string pointName;
   bool required;
   bool bound;
};

// Every RDF resource that matched a tool profile's focus class, whether or
// not it ended up creatable. This is the "what was close" report: candidates
// that failed are still present, with blockingReasons explaining why.
struct S223ToolDiagnosticCandidate {
   std::string rdfResource;
   std::string profileId;
   std::string profileDisplayName;
   bool hasName;
   std::string name;
   bool creatable;
   std::vector<std::string> blockingReasons;
   std::vector<S223ToolDiagnosticAntecedent> antecedents;
   std::vector<S223ToolDiagnosticPoint> points;
};

struct S223ToolDiagnosticReport {
   std::vector<S223ToolDiagnosticCandidate> candidates;

   std::size_t CreatableCount(void) const;
};

// Full S223 startup result: load/validation diagnostics plus every dynamically
// creatable tool instance found in the model.  Tools are ordered so every
// matched antecedent tool appears before tools that depend on it.
//
// diagnosticReport covers every candidate equipment resource (creatable or
// not); siteGraph*NTriples are the site data graph serialized before and
// after SHACL-AF inference, for REST inspection of what inference added.
struct S223ApplicationStartupModel {
   S223ModelLoadSummary loadSummary;
   std::vector<S223ToolStartupSpec> tools;
   S223ToolDiagnosticReport diagnosticReport;
   std::string siteGraphRawNTriples;
   std::string siteGraphInferredNTriples;
};

std::optional<S223ModelLoadConfig> ReadS223ModelLoadConfigFromEnvironment(void);

S223ModelLoadSummary LoadS223ModelFromTurtleFiles(const S223ModelLoadConfig&);

S223ApplicationStartupModel LoadS223ApplicationStartupModel(const S223ModelLoadConfig&);

#endif

//END-OF-FILE ZZZZZ2ZZZZZZZZZ3ZZZZZZZZZ4ZZZZZZZZZ5ZZZZZZZZZ6ZZZZZZZZZ7ZZZZZZZZZ8ZZZZZZZZZ9ZZZZZZZZZCZZZZZ
