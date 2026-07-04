// Assembler probe: run the witness pass, feed it to the generic assembler, and
// print the ordered, reconciled tool model. Proves the RDF -> resolved-tool half
// of the model-driven entrypoint end to end. See docs/tool-contract-design.md.
#include <shifty/shifty.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../../libEA/toolAssembler.hpp"

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

int main(int argc, char** argv) {
   const std::string base = argc > 1 ? argv[1] : "EAd/tests/testdata";

   const auto witnesses = RunRoleWitnesses(base);
   const auto model = AssembleTools(witnesses, BuiltInAnalysisModules());

   std::cout << "Assembled " << model.tools.size()
             << " tool(s) in antecedent-safe order:\n";
   int n = 0;
   for (const auto& t : model.tools) {
      std::cout << "\n[" << ++n << "] " << t.name
                << "  (" << t.toolProfileId << ")\n"
                << "    focus:  " << t.focusNode << "\n"
                << "    points: " << t.points.size() << " bound\n";
      for (const auto& a : t.antecedents) {
         std::cout << "    antecedent " << a.role.name << " -> " << a.focusNode << "\n";
      }
      for (const auto& p : t.points) {
         std::cout << "      " << p.role.name << " -> " << p.rdfProperty << "\n";
      }
   }

   if (!model.diagnostics.empty()) {
      std::cout << "\nDiagnostics (" << model.diagnostics.size() << "):\n";
      for (const auto& d : model.diagnostics) {
         std::cout << "  [" << d.focusNode << "] " << d.message << "\n";
      }
   }
   return 0;
}
