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

    // virtual void close_app() = 0;

    // virtual bool launch_app(std::string_view, json&&) = 0;

    virtual bool set_volume(double level) = 0;

    virtual bool set_muted(bool muted) = 0;

    virtual const std::string& get_name() const = 0;
};

using device_ptr = std::unique_ptr<device>;

#endif
