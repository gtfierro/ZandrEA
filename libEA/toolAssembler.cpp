//XXXXXXX1XXXXXXXXX2XXXXXXXXX3XXXXXXXXX4XXXXXXXXX5XXXXXXXXX6XXXXXXXXX7XXXXXXXXX8XXXXXXXXX9XXXXXXXXXCXXXXV
/* File summary:
   Implementation of the generic tool assembler.  See toolAssembler.hpp.
*/
/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8/////////9/////////C////V

#include "toolAssembler.hpp"

#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <string>

namespace {

// shifty renders witness terms as RDF: <iri>, "lit", "lit"@lang, "lit"^^<dt>.
// The assembler works in bare lexical form (IRI text / literal value).
std::string UnwrapTerm(const std::string& term) {
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

// One conforming focus node's witness rows, indexed by role name.
struct FocusWitness {
   std::string shapeId;
   std::map<std::string, std::vector<std::string>> byRole;   // role -> unwrapped value nodes
};

} // namespace

ResolvedToolModel AssembleTools(const std::vector<RoleWitness>& witnesses,
                                const CModuleRegistry& modules) {
   ResolvedToolModel model;

   // 1. Group witnesses by focus node (sorted for deterministic output).
   std::map<std::string, FocusWitness> byFocus;
   for (const auto& w : witnesses) {
      const auto focus = UnwrapTerm(w.focusNode);
      auto& fw = byFocus[focus];
      fw.shapeId = UnwrapTerm(w.shapeId);
      auto& values = fw.byRole[w.roleName];
      for (const auto& v : w.valueNodes) {
         values.push_back(UnwrapTerm(v));
      }
   }

   // 2. Resolve each focus into a candidate tool via its module manifest.
   std::vector<ResolvedTool> candidates;
   for (const auto& [focus, fw] : byFocus) {
      const auto* module = modules.Find(fw.shapeId);
      if (module == nullptr) {
         model.diagnostics.push_back(
            { focus, "no analysis module registered for profile shape " + fw.shapeId });
         continue;
      }

      const auto& manifest = module->Manifest();
      ResolvedTool tool;
      tool.focusNode = focus;
      tool.shapeId = fw.shapeId;
      tool.toolProfileId = manifest.toolProfileId;
      tool.manifest = &manifest;

      const auto nameRole = fw.byRole.find("name");
      if (nameRole != fw.byRole.end() && !nameRole->second.empty()) {
         tool.name = nameRole->second.front();
      }

      bool complete = true;
      for (const auto& spec : manifest.points) {
         const auto binding = fw.byRole.find(spec.id.name);
         if (binding == fw.byRole.end() || binding->second.empty()) {
            if (spec.required) {
               model.diagnostics.push_back(
                  { focus, "missing required point role '" + spec.id.name + "'" });
               complete = false;
            }
            continue;
         }
         tool.points.push_back({ spec.id, binding->second.front(), &spec });
      }

      for (const auto& spec : manifest.antecedents) {
         const auto binding = fw.byRole.find(spec.id.name);
         if (binding == fw.byRole.end() || binding->second.empty()) {
            if (spec.required) {
               model.diagnostics.push_back(
                  { focus, "missing required antecedent role '" + spec.id.name + "'" });
               complete = false;
            }
            continue;
         }
         tool.antecedents.push_back({ spec.id, binding->second.front() });
      }

      if (complete) {
         candidates.push_back(std::move(tool));
      }
   }

   // 3. Topologically order so every antecedent tool precedes its dependent.
   //    A required antecedent must resolve to another candidate tool.
   std::set<std::string> candidateFocus;
   for (const auto& t : candidates) {
      candidateFocus.insert(t.focusNode);
   }

   std::map<std::string, std::size_t> indexByFocus;
   for (std::size_t i = 0; i < candidates.size(); ++i) {
      indexByFocus[candidates[i].focusNode] = i;
   }

   std::vector<std::set<std::string>> deps(candidates.size());       // focus -> antecedent focuses
   std::vector<std::vector<std::size_t>> dependents(candidates.size());
   std::vector<std::size_t> inDegree(candidates.size(), 0);
   std::set<std::size_t> dropped;

   for (std::size_t i = 0; i < candidates.size(); ++i) {
      for (const auto& ante : candidates[i].antecedents) {
         if (candidateFocus.count(ante.focusNode) == 0) {
            model.diagnostics.push_back(
               { candidates[i].focusNode,
                 "antecedent '" + ante.role.name + "' resolves to " + ante.focusNode
                 + ", which is not a creatable tool" });
            dropped.insert(i);
            continue;
         }
         if (deps[i].insert(ante.focusNode).second) {
            dependents[indexByFocus[ante.focusNode]].push_back(i);
            ++inDegree[i];
         }
      }
   }

   // Kahn's algorithm, seeding in sorted focus order for determinism.
   std::vector<std::size_t> order;
   auto ready = [&](std::size_t i) { return inDegree[i] == 0 && dropped.count(i) == 0; };

   std::vector<std::size_t> frontier;
   for (std::size_t i = 0; i < candidates.size(); ++i) {
      if (ready(i)) frontier.push_back(i);
   }
   auto byFocusName = [&](std::size_t a, std::size_t b) {
      return candidates[a].focusNode < candidates[b].focusNode;
   };
   std::sort(frontier.begin(), frontier.end(), byFocusName);

   while (!frontier.empty()) {
      const auto i = frontier.front();
      frontier.erase(frontier.begin());
      order.push_back(i);
      std::vector<std::size_t> newlyReady;
      for (const auto dep : dependents[i]) {
         if (dropped.count(dep) != 0) continue;
         if (--inDegree[dep] == 0) newlyReady.push_back(dep);
      }
      std::sort(newlyReady.begin(), newlyReady.end(), byFocusName);
      frontier.insert(frontier.end(), newlyReady.begin(), newlyReady.end());
   }

   for (std::size_t i = 0; i < candidates.size(); ++i) {
      if (dropped.count(i) == 0
          && std::find(order.begin(), order.end(), i) == order.end()) {
         model.diagnostics.push_back(
            { candidates[i].focusNode,
              "dropped: antecedent cycle prevents ordering" });
      }
   }

   model.tools.reserve(order.size());
   for (const auto i : order) {
      model.tools.push_back(std::move(candidates[i]));
   }

   return model;
}

std::vector<S223ToolStartupSpec> StartupSpecsFromModel(const ResolvedToolModel& model) {
   std::map<std::string, std::string> nameByFocus;
   for (const auto& t : model.tools) {
      nameByFocus[t.focusNode] = t.name;
   }

   std::vector<S223ToolStartupSpec> specs;
   specs.reserve(model.tools.size());
   for (const auto& t : model.tools) {
      S223ToolStartupSpec spec;
      spec.toolProfileId = t.toolProfileId;
      spec.rdfResource   = t.focusNode;
      spec.name          = t.name;
      for (const auto& a : t.antecedents) {
         const auto known = nameByFocus.find(a.focusNode);
         spec.antecedents.push_back(
            { a.role.name, a.focusNode, known == nameByFocus.end() ? "" : known->second });
      }
      specs.push_back(std::move(spec));
   }
   return specs;
}

//END-OF-FILE ZZZZZ2ZZZZZZZZZ3ZZZZZZZZZ4ZZZZZZZZZ5ZZZZZZZZZ6ZZZZZZZZZ7ZZZZZZZZZ8ZZZZZZZZZ9ZZZZZZZZZCZZZZZ
