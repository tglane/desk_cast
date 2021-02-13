#ifndef DEVICE_BASECLASS_HPP
#define DEVICE_BASECLASS_HPP

#include <memory>
#include <string>

#include "json.hpp"

using nlohmann::json;

class device
{
public:
    virtual bool connect() = 0;

    virtual bool disconnect() = 0;

    virtual void close_app() = 0;

    virtual bool launch_app(std::string_view, json&&) = 0;

    virtual bool volume_up() = 0;

    virtual bool volume_down() = 0;

    virtual bool toggle_mute() = 0;

    virtual const std::string& get_name() const = 0;
};

using device_ptr = std::unique_ptr<device>;

#endif

