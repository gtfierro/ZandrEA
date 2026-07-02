//XXXXXXX1XXXXXXXXX2XXXXXXXXX3XXXXXXXXX4XXXXXXXXX5XXXXXXXXX6XXXXXXXXX7XXXXXXXXX8XXXXXXXXX9XXXXXXXXXCXXXXV
/* File summary:
   Implements ASHRAE 223 RDF/Turtle loading using rdf4cpp.
*/
/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8/////////9/////////C////V

#include "s223Model.hpp"

#include <rdf4cpp/parser/RDFFileParser.hpp>

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <stdexcept>

namespace {

const char* const S223_ONTOLOGY_ENV = "EA_S223_ONTOLOGY_TTL";
const char* const S223_SITE_ENV = "EA_S223_SITE_TTL";

std::optional<std::string> getenv_string(const char* name) {
   const char* value = std::getenv(name);
   if (value == nullptr || value[0] == '\0') {
      return std::nullopt;
   }
   return std::string(value);
}

void require_readable_file(const std::string& path) {
   if (!std::filesystem::is_regular_file(path)) {
      std::ostringstream msg;
      msg << "S223 Turtle file is not readable as a regular file: " << path;
      throw std::runtime_error(msg.str());
   }
}

S223FileLoadSummary load_turtle_file(const std::string& path) {
   require_readable_file(path);

   std::size_t quadCount = 0;
   rdf4cpp::parser::RDFFileParser parser(path);

   for (const auto& parsed : parser) {
      if (!parsed.has_value()) {
         std::ostringstream msg;
         msg << "Failed parsing S223 Turtle file " << path << ": " << parsed.error();
         throw std::runtime_error(msg.str());
      }
      ++quadCount;
   }

   return { path, quadCount };
}

} // namespace

std::size_t S223ModelLoadSummary::TotalQuadCount(void) const {
   return ontology.quadCount + site.quadCount;
}

std::optional<S223ModelLoadConfig> ReadS223ModelLoadConfigFromEnvironment(void) {
   auto ontologyPath = getenv_string(S223_ONTOLOGY_ENV);
   auto sitePath = getenv_string(S223_SITE_ENV);

   if (!ontologyPath.has_value() && !sitePath.has_value()) {
      return std::nullopt;
   }

   if (!ontologyPath.has_value() || !sitePath.has_value()) {
      std::ostringstream msg;
      msg << "Both " << S223_ONTOLOGY_ENV << " and " << S223_SITE_ENV
          << " must be set to enable ASHRAE 223 startup loading";
      throw std::runtime_error(msg.str());
   }

   return S223ModelLoadConfig{ *ontologyPath, *sitePath };
}

S223ModelLoadSummary LoadS223ModelFromTurtleFiles(const S223ModelLoadConfig& config) {
   return {
      load_turtle_file(config.ontologyTurtlePath),
      load_turtle_file(config.siteTurtlePath)
   };
}

//END-OF-FILE ZZZZZ2ZZZZZZZZZ3ZZZZZZZZZ4ZZZZZZZZZ5ZZZZZZZZZ6ZZZZZZZZZ7ZZZZZZZZZ8ZZZZZZZZZ9ZZZZZZZZZCZZZZZ
