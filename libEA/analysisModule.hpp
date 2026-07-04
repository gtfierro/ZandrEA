//XXXXXXX1XXXXXXXXX2XXXXXXXXX3XXXXXXXXX4XXXXXXXXX5XXXXXXXXX6XXXXXXXXX7XXXXXXXXX8XXXXXXXXX9XXXXXXXXXCXXXXV
/* File summary:
   The analysis-module boundary for the model-driven tool-contract entrypoint.
   A module owns one tool type's role vocabulary (its ModuleManifest) and, later,
   builds the engine object graph (CFact/CRule/...) against roles resolved from
   RDF.  Modules are registered by profile node-shape IRI, which equals the shifty
   witness shape_id, so a conforming focus node dispatches straight to its module.

   See docs/tool-contract-design.md.
*/
/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8/////////9/////////C////V

#ifndef ANALYSISMODULE_HPP
#define ANALYSISMODULE_HPP

#include <map>
#include <string>
#include <vector>

#include "toolRole.hpp"

// A module is engine-type-specific but tool-instance- and RDF-agnostic.  For now
// it exposes only its manifest (the role/antecedent/engine-attr contract); the
// Build(RoleBoundPoints, BuildContext) hook that instantiates points/facts is the
// next slice and is intentionally not declared here yet so this layer compiles
// without the full engine.
class IAnalysisModule {
   public:
      virtual ~IAnalysisModule(void) = default;
      virtual const ModuleManifest& Manifest(void) const = 0;
};

// Registry keyed by profile node-shape IRI (== witness shape_id).
class CModuleRegistry {
   public:
      void Register(const IAnalysisModule& module);
      const IAnalysisModule* Find(const std::string& profileShapeId) const;
      std::vector<const IAnalysisModule*> All(void) const;

   private:
      std::map<std::string, const IAnalysisModule*> byShapeId;
};

// The process-wide registry with the built-in AHU/VAV modules registered.
const CModuleRegistry& BuiltInAnalysisModules(void);

#endif

//END-OF-FILE ZZZZZ2ZZZZZZZZZ3ZZZZZZZZZ4ZZZZZZZZZ5ZZZZZZZZZ6ZZZZZZZZZ7ZZZZZZZZZ8ZZZZZZZZZ9ZZZZZZZZZCZZZZZ
