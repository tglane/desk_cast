#ifndef CAST_APP_HPP
#define CAST_APP_HPP

#include <string>
#include <json.hpp>

using nlohmann::json;

namespace googlecast
{

class cast_device;

// Currently this is only a default media receiver
class cast_app
{
public:
    cast_app() = default;
    cast_app(const cast_app&) = delete;
    cast_app& operator=(const cast_app&) = delete;
    cast_app(cast_app&&) = delete;
    cast_app& operator=(cast_app&&) = delete;

    cast_app(cast_device* device, const std::string& app_id, const std::string& session_id, 
        const std::string& transport_id, const json& nspaces);

    bool run() const;

private:

    cast_device* m_device;

    const std::string m_app_id;
    const std::string m_session_id;
    const std::string m_transport_id;
    const json m_namespaces;

    friend class cast_device;
};

}; // namespace googlecast

#endif
