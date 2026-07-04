//XXXXXXX1XXXXXXXXX2XXXXXXXXX3XXXXXXXXX4XXXXXXXXX5XXXXXXXXX6XXXXXXXXX7XXXXXXXXX8XXXXXXXXX9XXXXXXXXXCXXXXV
/* File summary:
   The generic tool assembler for the model-driven entrypoint.  It consumes the
   role witnesses produced by the shifty witness pass, dispatches each conforming
   focus node to its analysis module by profile shape IRI, reconciles the witness
   bindings against the module manifest, resolves antecedents, and topologically
   orders the tools so every antecedent precedes its dependent.  No per-tool code.

   See docs/tool-contract-design.md.
*/
/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8/////////9/////////C////V

#ifndef TOOLASSEMBLER_HPP
#define TOOLASSEMBLER_HPP

#include <vector>

#include "analysisModule.hpp"
#include "s223Model.hpp"
#include "toolRole.hpp"

// Turn raw role witnesses into an ordered, reconciled tool model.  Anything that
// cannot be assembled (unknown profile, missing required role, unresolvable or
// cyclic antecedent) is dropped from tools and explained in diagnostics.
ResolvedToolModel AssembleTools(const std::vector<RoleWitness>& witnesses,
                                const CModuleRegistry& modules);

// Project the resolved model onto the existing S223 startup-spec surface that
// AddToolFromS223Spec / the CApplication instantiation loop already consume.  The
// existing CTool_* constructors build the point/fact/rule graph; this only
// carries which tool, its display name, and its antecedent bindings by RDF
// resource.  Order is preserved (antecedents first).
std::vector<S223ToolStartupSpec> StartupSpecsFromModel(const ResolvedToolModel& model);

#endif

//END-OF-FILE ZZZZZ2ZZZZZZZZZ3ZZZZZZZZZ4ZZZZZZZZZ5ZZZZZZZZZ6ZZZZZZZZZ7ZZZZZZZZZ8ZZZZZZZZZ9ZZZZZZZZZCZZZZZ
