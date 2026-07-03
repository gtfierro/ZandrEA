// Witness probe: prove profile-driven role binding against the clean model.
#include <shifty/shifty.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

static std::string slurp(const std::string& p) {
   std::ifstream f(p);
   std::stringstream ss; ss << f.rdbuf();
   return ss.str();
}

int main(int argc, char** argv) {
   const std::string base = argc > 1 ? argv[1] : "EAd/tests/testdata";
   const std::string shapes =
      slurp(base + "/zea-core.ttl") + "\n" + slurp(base + "/zea-profiles.ttl");

   shifty::PreparedValidator validator(shapes, shifty::RdfFormat::Turtle);

   shifty::Dataset data;
   data.load_file(base + "/simple-ahu-vav-223.ttl", shifty::RdfFormat::Turtle);

   shifty::ValidationOptions opts;
   opts.key_path = "zea:roleName";
   opts.run_inference = true;

   auto vr = validator.validate(data, opts);
   std::cout << "Conforms: " << (vr.conforms() ? "YES" : "NO") << "\n";
   if (!vr.conforms()) {
      std::cout << "---- validation report ----\n" << vr.results_text() << "\n";
   }

   auto ws = validator.witnesses(data, opts);

   std::map<std::string, std::map<std::string, std::vector<std::string>>> byFocus;
   for (auto& w : ws) byFocus[w.focus_node][w.key] = w.value_nodes;

   for (auto& [focus, roles] : byFocus) {
      std::cout << "\n=== " << focus << "  (" << roles.size() << " roles) ===\n";
      for (auto& [role, vals] : roles) {
         std::cout << "  " << role << " =";
         for (auto& v : vals) std::cout << " " << v;
         if (vals.empty()) std::cout << " <UNBOUND>";
         if (vals.size() > 1) std::cout << "   <-- AMBIGUOUS (" << vals.size() << ")";
         std::cout << "\n";
      }
   }
   std::cout << "\nFocus nodes: " << byFocus.size()
             << ", total witness rows: " << ws.size() << "\n";
   return 0;
}
