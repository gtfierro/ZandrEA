// Exercises the real public entrypoint LoadS223ApplicationStartupModel with the
// zea profile shapes configured, proving the witness-driven discovery
// (tool_specs_via_profile_witnesses) produces the S223ToolStartupSpec list the
// CApplication loop feeds to the existing CTool_* constructors.
//
// Build (needs EA_HAVE_SHIFTY + shifty prefix):
//   g++ -std=c++17 -DEA_HAVE_SHIFTY -I$PREFIX/include -IlibEA \
//       EAd/tests/s223_startup_probe.cpp libEA/s223Model.cpp libEA/toolAssembler.cpp \
//       libEA/analysisModule.cpp libEA/toolModules.cpp libEA/toolProfile.cpp \
//       -o /tmp/s223_startup_probe -L$PREFIX/lib -lshifty_cpp -ldl -pthread -lm
//   /tmp/s223_startup_probe EAd/tests/testdata
//
#include "../../libEA/s223Model.hpp"
#include <iostream>

int main(int argc, char** argv) {
   const std::string base = argc > 1 ? argv[1] : "EAd/tests/testdata";

   S223ModelLoadConfig config;
   config.ontologyTurtlePath = "223p.ttl";
   config.siteTurtlePath = base + "/simple-ahu-vav-223.ttl";
   config.profileShapeTurtlePaths = { base + "/zea-core.ttl", base + "/zea-profiles.ttl" };

   const auto model = LoadS223ApplicationStartupModel(config);

   std::cout << "tools discovered via witnesses: " << model.tools.size() << "\n";
   for (const auto& t : model.tools) {
      std::cout << "  " << t.toolProfileId << "  name=" << t.name
                << "  rdf=" << t.rdfResource << "\n";
      for (const auto& a : t.antecedents) {
         std::cout << "      antecedent " << a.role << " -> " << a.name
                   << " (" << a.rdfResource << ")\n";
      }
   }
   return 0;
}
