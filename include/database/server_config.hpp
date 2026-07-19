#pragma once

class server_config
{
public:
    /* wall-clock growth multiplier: 10 = trees/providers mature 10x faster */
    float growth_speed{10.0f};
    /* gem quantity multiplier: 10 = each successful gem drop awards 10x gems */
    float gem_drop_multiplier{10.0f};
};

extern ::server_config gServer_config;

/*
    @return server_config read from 'server.cfg'.
    if the file does not exist, it will be created with default values.
*/
extern ::server_config load_server_config();
