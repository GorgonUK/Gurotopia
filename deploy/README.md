# Runtime folder for docker-compose mounts.
# Copy the *.example files from the repo root, then edit:
#
#   cp ../database.cfg.example database.cfg
#   cp ../server.cfg.example server.cfg
#   cp ../server_data.php.example server_data.php
#   # items.dat: copy from your Growtopia cache or current machine
#
# When using docker-compose, set database.cfg host to the service name:
#   host|db
#
# server.cfg:
#   growth_speed|10   # 10x faster tree/provider growth (1 = normal)
#
# server_data.php:
#   server|<LAN or public IP that clients connect to>
