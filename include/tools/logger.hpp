#pragma once

#include <string>

/* Append one line to /app/logs/gurotopia-YYYY-MM-DD.log (or ./logs/ when not in Docker).
   Cheap, line-buffered append only — never call from movement / high-frequency paths. */
void log_event(
    const std::string &category,
    const std::string &who,
    const std::string &world,
    const std::string &detail);
