// Assembler probe: run the witness pass, feed it to the generic assembler, and
// dump (a) the resolved role bindings and (b) the S223ToolStartupSpec list the
// assembler projects them onto -- the exact input the existing CTool_*
// constructors consume via AddToolFromS223Spec. See docs/tool-contract-design.md.
//
//   ./assembler_probe [testdata-dir] [--json]
//
#include <shifty/shifty.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../../libEA/toolAssembler.hpp"
#include "../../libEA/toolProfile.hpp"   // EPointNameToString

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

static const char* kind(RoleValueKind k) {
   return k == RoleValueKind::Analog ? "analog" : "binary";
}

static void dumpText(const ResolvedToolModel& model,
                     const std::vector<S223ToolStartupSpec>& specs) {
   std::cout << "=== Resolved tool model (post-assembler) ===\n"
             << model.tools.size() << " tool(s), antecedent-safe order:\n";
   int n = 0;
   for (const auto& t : model.tools) {
      std::cout << "\n[" << ++n << "] " << t.name << "   profile=" << t.toolProfileId
                << "\n    focus: " << t.focusNode << "\n";
      for (const auto& a : t.antecedents) {
         std::cout << "    antecedent " << a.role.name << " -> " << a.focusNode << "\n";
      }
      std::cout << "    points (" << t.points.size() << "):\n";
      for (const auto& p : t.points) {
         std::cout << "      " << p.role.name << " [" << kind(p.spec->valueKind) << "]"
                   << "  pointName=" << EPointNameToString(p.spec->pointName)
                   << "  rdf=" << p.rdfProperty << "\n";
      }
   }
   std::cout << "\ndiagnostics (" << model.diagnostics.size() << "):\n";
   for (const auto& d : model.diagnostics) {
      std::cout << "  [" << d.focusNode << "] " << d.message << "\n";
   }

   std::cout << "\n=== S223ToolStartupSpec list (input to the existing CTool_* c-tors) ===\n";
   for (const auto& s : specs) {
      std::cout << "  profileId=" << s.toolProfileId << "  name=" << s.name
                << "  rdf=" << s.rdfResource << "\n";
      for (const auto& a : s.antecedents) {
         std::cout << "      antecedent " << a.role << ": " << a.name
                   << " (" << a.rdfResource << ")\n";
      }
   }
}

static std::string jstr(const std::string& s) {
   std::string o = "\"";
   for (char c : s) { if (c == '"' || c == '\\') o += '\\'; o += c; }
   return o + "\"";
}

static void dumpJson(const ResolvedToolModel& model,
                     const std::vector<S223ToolStartupSpec>& specs) {
   std::cout << "{\n  \"tools\": [\n";
   for (std::size_t i = 0; i < model.tools.size(); ++i) {
      const auto& t = model.tools[i];
      std::cout << "    {\"order\": " << (i + 1)
                << ", \"name\": " << jstr(t.name)
                << ", \"profile\": " << jstr(t.toolProfileId)
                << ", \"focus\": " << jstr(t.focusNode)
                << ", \"antecedents\": [";
      for (std::size_t a = 0; a < t.antecedents.size(); ++a) {
         std::cout << (a ? ", " : "") << "{" << jstr(t.antecedents[a].role.name)
                   << ": " << jstr(t.antecedents[a].focusNode) << "}";
      }
      std::cout << "], \"points\": [";
      for (std::size_t p = 0; p < t.points.size(); ++p) {
         const auto& pb = t.points[p];
         std::cout << (p ? ", " : "") << "{\"role\": " << jstr(pb.role.name)
                   << ", \"kind\": " << jstr(kind(pb.spec->valueKind))
                   << ", \"pointName\": " << jstr(EPointNameToString(pb.spec->pointName))
                   << ", \"rdf\": " << jstr(pb.rdfProperty) << "}";
      }
      std::cout << "]}" << (i + 1 < model.tools.size() ? "," : "") << "\n";
   }
   std::cout << "  ],\n  \"startupSpecs\": [\n";
   for (std::size_t i = 0; i < specs.size(); ++i) {
      const auto& s = specs[i];
      std::cout << "    {\"profileId\": " << jstr(s.toolProfileId)
                << ", \"name\": " << jstr(s.name)
                << ", \"rdf\": " << jstr(s.rdfResource)
                << ", \"antecedents\": [";
      for (std::size_t a = 0; a < s.antecedents.size(); ++a) {
         std::cout << (a ? ", " : "") << "{" << jstr(s.antecedents[a].role)
                   << ": " << jstr(s.antecedents[a].rdfResource) << "}";
      }
      std::cout << "]}" << (i + 1 < specs.size() ? "," : "") << "\n";
   }
   std::cout << "  ]\n}\n";
}

int main(int argc, char** argv) {
   std::string base = "EAd/tests/testdata";
   bool json = false;
   for (int i = 1; i < argc; ++i) {
      const std::string a = argv[i];
      if (a == "--json") json = true; else base = a;
   }

   const auto model = AssembleTools(RunRoleWitnesses(base), BuiltInAnalysisModules());
   const auto specs = StartupSpecsFromModel(model);
   if (json) dumpJson(model, specs); else dumpText(model, specs);
   return 0;
}
