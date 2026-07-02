//XXXXXXX1XXXXXXXXX2XXXXXXXXX3XXXXXXXXX4XXXXXXXXX5XXXXXXXXX6XXXXXXXXX7XXXXXXXXX8XXXXXXXXX9XXXXXXXXXCXXXXV
/* File summary:
   Declares a small ASHRAE 223 RDF/Turtle loading boundary for future SIF/application configuration work.
*/
/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8/////////9/////////C////V

#ifndef S223MODEL_HPP
#define S223MODEL_HPP

#include <cstddef>
#include <optional>
#include <string>

struct S223FileLoadSummary {
   std::string path;
   std::size_t quadCount;
};

struct S223ModelLoadConfig {
   std::string ontologyTurtlePath;
   std::string siteTurtlePath;
};

struct S223ModelLoadSummary {
   S223FileLoadSummary ontology;
   S223FileLoadSummary site;

   std::size_t TotalQuadCount(void) const;
};

std::optional<S223ModelLoadConfig> ReadS223ModelLoadConfigFromEnvironment(void);

S223ModelLoadSummary LoadS223ModelFromTurtleFiles(const S223ModelLoadConfig&);

#endif

//END-OF-FILE ZZZZZ2ZZZZZZZZZ3ZZZZZZZZZ4ZZZZZZZZZ5ZZZZZZZZZ6ZZZZZZZZZ7ZZZZZZZZZ8ZZZZZZZZZ9ZZZZZZZZZCZZZZZ
