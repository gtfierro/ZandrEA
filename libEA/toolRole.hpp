//XXXXXXX1XXXXXXXXX2XXXXXXXXX3XXXXXXXXX4XXXXXXXXX5XXXXXXXXX6XXXXXXXXX7XXXXXXXXX8XXXXXXXXX9XXXXXXXXXCXXXXV
/* File summary:
   Role vocabulary for the model-driven tool-contract entrypoint.  A "role" is a
   tool-local semantic slot (outsideAirTemp, airSource) that joins the RDF-facing
   contract (a profile finder + zea:pointRole) to the engine-facing analysis
   module.  These types carry no shifty/RDF dependency and only the leaf engine
   enums, so the assembler and modules stay decoupled from both.

   See docs/tool-contract-design.md.
*/
/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8/////////9/////////C////V

#ifndef TOOLROLE_HPP
#define TOOLROLE_HPP

#include <string>
#include <vector>

#include "customTypes.hpp"   // EDataLabel/EDataUnit/EDataRange/EPlotGroup (+ EPointName via exportTypes)

// Tool-local semantic slot name.  Equality by name; module code refers to these
// through named constants (see analysisModule.hpp) so a typo is a compile error,
// while the RDF profile binds the same string via zea:roleName.
struct RoleId {
   std::string name;
   bool operator==(const RoleId& other) const { return name == other.name; }
};

enum class RoleValueKind { Analog, Binary };

// Engine construction contract for one point role.  The analysis engine is fixed
// and still needs these enums to build a CPointAnalog/CPointBinary, so the module
// owns them; the RDF profile owns only the finder.  pointName is the ingestion
// join key the engine requires today (an RDF property IRI is threaded alongside
// it at resolution time for future channel-native ingestion).
struct PointRoleSpec {
   RoleId        id;
   RoleValueKind valueKind;
   bool          required;

   EPointName    pointName;    // engine ingestion key
   EDataLabel    dataLabel;    // point UI label
   EDataUnit     dataUnit;     // analog only
   EDataRange    dataRange;    // analog only
   EPlotGroup    plotGroup;    // analog only
   EDataLabel    factLabel;    // binary only: the direct-fact label CPointBinary needs
};

// An antecedent role binds to another equipment that must itself be a creatable
// tool of the required profile (e.g. a VAV's upstream AHU).
struct AntecedentRoleSpec {
   RoleId      id;
   std::string requiredProfileShapeId;   // the upstream tool's profile node-shape IRI
   bool        required;
};

// Everything the assembler and engine need for one tool type, owned by its
// analysis module.  profileShapeId is the registry key and equals the shifty
// witness shape_id, so a conforming focus node maps straight to its module.
struct ModuleManifest {
   std::string                     profileShapeId;
   std::string                     toolProfileId;   // display/logging only
   std::vector<PointRoleSpec>      points;
   std::vector<AntecedentRoleSpec> antecedents;
};

// ---- Witness input (produced from shifty PreparedValidator::witnesses) --------

// One (focus node, role) binding as returned by the witness pass.  valueNodes are
// rendered RDF terms (<iri> / "lit" / "lit"^^<dt>); the assembler unwraps them.
struct RoleWitness {
   std::string              focusNode;
   std::string              shapeId;
   std::string              roleName;   // the witness key (== zea:roleName)
   std::vector<std::string> valueNodes;
};

// ---- Resolved output ----------------------------------------------------------

struct ResolvedPointBinding {
   RoleId              role;
   std::string         rdfProperty;   // bound observable/actuatable property IRI
   const PointRoleSpec* spec;         // engine construction attributes
};

struct ResolvedAntecedent {
   RoleId      role;
   std::string focusNode;             // upstream equipment IRI (a resolved tool)
};

struct ResolvedTool {
   std::string                       focusNode;
   std::string                       shapeId;
   std::string                       toolProfileId;
   std::string                       name;          // from zea:name
   std::vector<ResolvedPointBinding> points;
   std::vector<ResolvedAntecedent>   antecedents;
   const ModuleManifest*             manifest;
};

struct AssemblyDiagnostic {
   std::string focusNode;   // "" for whole-model diagnostics
   std::string message;
};

// Tools are topologically ordered so every antecedent tool precedes the tool that
// depends on it.  diagnostics explains anything dropped (unknown profile, missing
// required role, unresolvable/cyclic antecedent).
struct ResolvedToolModel {
   std::vector<ResolvedTool>       tools;
   std::vector<AssemblyDiagnostic> diagnostics;
};

#endif

//END-OF-FILE ZZZZZ2ZZZZZZZZZ3ZZZZZZZZZ4ZZZZZZZZZ5ZZZZZZZZZ6ZZZZZZZZZ7ZZZZZZZZZ8ZZZZZZZZZ9ZZZZZZZZZCZZZZZ
