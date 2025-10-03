#include "config_manager.hpp"

class IModule {
public:
    virtual ~IModule() = default;
    virtual void init(const JsonObjectConst& cfg) = 0;
    virtual void loop() = 0;
};