//XXXXXXX1XXXXXXXXX2XXXXXXXXX3XXXXXXXXX4XXXXXXXXX5XXXXXXXXX6XXXXXXXXX7XXXXXXXXX8XXXXXXXXX9XXXXXXXXXCXXXXV
/* File summary:
   Declares a parallel, declarative profile layer for tool RDF requirements.
*/
/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8/////////9/////////C////V

#ifndef TOOLPROFILE_HPP
#define TOOLPROFILE_HPP

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "exportTypes.hpp"

enum class S223PropertyPathOperator {
   Predicate,
   Sequence,
   Alternative,
   Inverse,
   ZeroOrMore,
   OneOrMore,
   ZeroOrOne
};

// Minimal AST for S223/SPARQL property paths.  Keeping paths structured lets us
// render the same requirement as SPARQL for discovery and SHACL path Turtle for
// validation, including /, |, ^, *, +, and ? operators.
class S223PropertyPath {
   public:
      // Leaf predicate, either a prefixed name such as s223:connectedFrom or a
      // full IRI accepted by the renderer.
      static S223PropertyPath Predicate(std::string iriOrPrefixedName);

      // Ordered path composition: p1 / p2 / ...
      static S223PropertyPath Sequence(std::vector<S223PropertyPath> operands);

      // Alternative paths: p1 | p2 | ...
      static S223PropertyPath Alternative(std::vector<S223PropertyPath> operands);

      // Inverse path: ^p
      static S223PropertyPath Inverse(S223PropertyPath operand);

      // Repetition modifiers: p*, p+, p?
      static S223PropertyPath ZeroOrMore(S223PropertyPath operand);
      static S223PropertyPath OneOrMore(S223PropertyPath operand);
      static S223PropertyPath ZeroOrOne(S223PropertyPath operand);

      S223PropertyPathOperator Operator(void) const;
      const std::string& PredicateName(void) const;
      const std::vector<S223PropertyPath>& Operands(void) const;

      std::string AsSparqlPropertyPath(void) const;
      std::string AsShaclPathTurtle(void) const;

   private:
      S223PropertyPath(S223PropertyPathOperator, std::string, std::vector<S223PropertyPath>);

      S223PropertyPathOperator pathOperator;
      std::string predicateName;
      std::vector<S223PropertyPath> operands;
};

enum class ToolPointValueKind {
   Analog,
   Binary
};

enum class LegacyToolConstructorId {
   AhuIbal,
   VavIbal,
   ChillerIbal,
   TesIbal
};

enum class ToolProfileDiscoveryQueryMode {
   // Return equipment that matches the profile focus classes, without requiring
   // every binding needed to instantiate a tool.
   CandidateEquipment,

   // Return only equipment that has the profile's required name, antecedent,
   // and point patterns.
   CreatableTools
};

// Focus requirements identify which S223 equipment resources are candidates for
// a tool profile.  They intentionally use ontology classes, not IBAL instance
// names or ZandrEA enum slots.
struct S223FocusRequirement {
   std::vector<std::string> targetClasses;
};

// Antecedent requirements describe model relationships between tools, such as a
// VAV's upstream AHU.  The property path is rendered both as SHACL and SPARQL.
struct AntecedentRequirement {
   std::string role;
   std::string requiredToolProfileId;
   bool required;
   S223PropertyPath path;
   std::vector<std::string> requiredClasses;
};

// Point requirements are the future binding contract between RDF points and
// existing point objects.  They are generated into shapes now; runtime startup
// can opt into requiring them once point binding is wired through the tools.
struct PointRequirement {
   EPointName pointName;
   ToolPointValueKind valueKind;
   bool required;
   S223PropertyPath path;
   std::vector<std::string> requiredClasses;
   std::vector<std::string> requiredQuantityKinds;
};

// Declarative contract for one ZandrEA tool type.  This is the source used to
// generate both SHACL requirements and SPARQL discovery/startup queries.
struct ToolProfile {
   std::string id;
   std::string displayName;
   LegacyToolConstructorId legacyConstructor;
   S223FocusRequirement focus;
   std::vector<AntecedentRequirement> antecedents;
   std::vector<PointRequirement> points;
};

std::string ToolPointValueKindToString(ToolPointValueKind);
std::string EPointNameToString(EPointName);

const std::vector<ToolProfile>& BuiltInToolProfiles(void);
std::optional<ToolProfile> FindBuiltInToolProfile(std::string_view id);

std::string GenerateToolProfileShapesTurtle(const std::vector<ToolProfile>& profiles);
std::string GenerateBuiltInToolProfileShapesTurtle(void);

std::string GenerateToolProfileDiscoverySparql(
   const std::vector<ToolProfile>& profiles,
   ToolProfileDiscoveryQueryMode mode
);
std::string GenerateBuiltInToolProfileDiscoverySparql(ToolProfileDiscoveryQueryMode mode);

std::string GenerateToolProfileStartupConstructSparql(
   const std::vector<ToolProfile>& profiles,
   bool requirePointBindings
);
std::string GenerateBuiltInToolProfileStartupConstructSparql(bool requirePointBindings);

// Diagnostic CONSTRUCT: like the startup CONSTRUCT, but every antecedent and
// point pattern is wrapped in OPTIONAL regardless of ToolProfile::required,
// so callers can see which requirements were and were not satisfied for every
// candidate equipment resource, not just the ones that ended up creatable.
std::string GenerateToolProfileDiagnosticConstructSparql(
   const std::vector<ToolProfile>& profiles
);
std::string GenerateBuiltInToolProfileDiagnosticConstructSparql(void);

#endif

//END-OF-FILE ZZZZZ2ZZZZZZZZZ3ZZZZZZZZZ4ZZZZZZZZZ5ZZZZZZZZZ6ZZZZZZZZZ7ZZZZZZZZZ8ZZZZZZZZZ9ZZZZZZZZZCZZZZZ
