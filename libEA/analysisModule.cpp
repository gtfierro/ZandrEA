//XXXXXXX1XXXXXXXXX2XXXXXXXXX3XXXXXXXXX4XXXXXXXXX5XXXXXXXXX6XXXXXXXXX7XXXXXXXXX8XXXXXXXXX9XXXXXXXXXCXXXXV
/* File summary:
   Registry implementation for analysis modules.  The concrete AHU/VAV module
   manifests live in toolModules.cpp; this file only wires the registry.
*/
/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8/////////9/////////C////V

#include "analysisModule.hpp"

#include <stdexcept>

void CModuleRegistry::Register(const IAnalysisModule& module) {
   const auto& shapeId = module.Manifest().profileShapeId;
   if (byShapeId.count(shapeId) != 0) {
      throw std::runtime_error("Duplicate analysis module for profile shape " + shapeId);
   }
   byShapeId.emplace(shapeId, &module);
}

const IAnalysisModule* CModuleRegistry::Find(const std::string& profileShapeId) const {
   const auto found = byShapeId.find(profileShapeId);
   return found == byShapeId.end() ? nullptr : found->second;
}

std::vector<const IAnalysisModule*> CModuleRegistry::All(void) const {
   std::vector<const IAnalysisModule*> modules;
   modules.reserve(byShapeId.size());
   for (const auto& entry : byShapeId) {
      modules.push_back(entry.second);
   }
   return modules;
}

// Defined in toolModules.cpp: constructs the registry with the built-in modules.
const CModuleRegistry& BuildBuiltInAnalysisModules(void);

const CModuleRegistry& BuiltInAnalysisModules(void) {
   static const CModuleRegistry& registry = BuildBuiltInAnalysisModules();
   return registry;
}

//END-OF-FILE ZZZZZ2ZZZZZZZZZ3ZZZZZZZZZ4ZZZZZZZZZ5ZZZZZZZZZ6ZZZZZZZZZ7ZZZZZZZZZ8ZZZZZZZZZ9ZZZZZZZZZCZZZZZ
