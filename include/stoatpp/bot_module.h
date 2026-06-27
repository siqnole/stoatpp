#pragma once
#include <memory>

namespace stoatpp {

class cluster;

class bot_module {
public:
    virtual ~bot_module() = default;
    virtual void register_handlers(cluster& bot) = 0;
};

} // namespace stoatpp
