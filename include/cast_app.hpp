#ifndef CAST_APP_HPP
#define CAST_APP_HPP

#include <string>

#include <json.hpp>

using nlohmann::json;

namespace googlecast
{

struct cast_app
{
    std::string app_id;
    std::string transport_id;
    std::string session_id;
    json namespaces;
};

} // namespace googlecast

#endif
