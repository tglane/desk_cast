#ifndef DEVICE_BASECLASS_HPP
#define DEVICE_BASECLASS_HPP

#include "json.hpp"

using nlohmann::json;

class device
{
    virtual bool connect() = 0;

    virtual bool disconnect() = 0;

    virtual void close_app() = 0;

    virtual bool launch_app(std::string_view, json&&) = 0;

    virtual bool volume_up() = 0;

    virtual bool volume_down() = 0;

    virtual bool toggle_mute() = 0;
};

#endif
