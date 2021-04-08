#ifndef DESK_CAST_UTILS_HPP
#define DESK_CAST_UTILS_HPP

#include <string>

namespace utils
{

struct media_data
{
    // TODO Maybe add more states if necessary
    std::string url;
    std::string mime_type;
};

std::string get_local_ipaddr();

// TODO add function to check wether a file (or just the cert and key file) exist

} // utils

#endif
