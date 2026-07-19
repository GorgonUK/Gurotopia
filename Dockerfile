# syntax=docker/dockerfile:1

FROM ubuntu:24.04 AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        libssl-dev \
        libmariadb-dev \
        pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .
RUN make -j"$(nproc)"

FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        libssl3 \
        libmariadb3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=build /src/main.out /app/main.out
COPY --from=build /src/resources /app/resources

COPY database.cfg.example /app/database.cfg.example
COPY server.cfg.example /app/server.cfg.example
COPY server_data.php.example /app/server_data.php.example

EXPOSE 17091/udp
EXPOSE 443/tcp

CMD ["./main.out"]
