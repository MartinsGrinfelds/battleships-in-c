# Battleships server (no graphics needed).
#   docker build -t battleships-server .
#   docker run -d --name battleships -p 12345:12345 battleships-server
# Settings can be passed as arguments:
#   docker run -p 12345:12345 battleships-server --teams 4 --turn-time 45
# Or with config file:
#   docker run -p 12345:12345 -v $PWD/server.conf:/etc/battleships/server.conf battleships-server --config /etc/battleships/server.conf

# Official gcc image already has compiler and make.
FROM gcc:13-bookworm AS build
WORKDIR /src
COPY . .
ARG VERSION=docker
RUN make BattleshipsServer BattleshipsBot VERSION=$VERSION

FROM debian:bookworm-slim
RUN useradd --system --no-create-home battleships
COPY --from=build /src/BattleshipsServer /src/BattleshipsBot /usr/local/bin/
USER battleships
EXPOSE 12345
ENTRYPOINT ["BattleshipsServer"]
