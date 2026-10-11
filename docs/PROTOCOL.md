# Battleships Network Protocol

The [wiki](https://github.com/MartinsGrinfelds/battleships-in-c/wiki) describes the original
protocol (IDs like `DATA-0021`). This page describes how it is implemented and what was added
so that the game could be completed. Code: `packets/shared_packets.h`, `packets/connection.c`.

Client and server talk over one TCP connection. All numbers are in network byte order
(big-endian). Strings are sent without `\0`; their length is sent before them.

## Packet frame (DATA-0021)

TCP is a stream: one read can return half of a packet or several packets. Every packet starts
with a 10-byte header so that the receiver knows where the packet ends.

| Bytes | Field             | Notes                                                    |
|-------|-------------------|----------------------------------------------------------|
| 4     | `sequence_number` | Starts from 1 for each connection. After `0xFFFFFFFF` comes 0. |
| 4     | `content_size`    | Size of content (wiki says `size_t`; sent as 4 bytes so that 32-bit and 64-bit machines agree). Max 100000. |
| 1     | `packet_type`     | See table below.                                         |
| 1     | `checksum`        | XOR of all 9 header bytes before it and all content bytes. |
| N     | content           | Packet content.                                          |

Receiver rules:

- Packet with wrong checksum is dropped.
- Packet with sequence number lower than (or equal to) previous one is dropped, except 0.
- Content size above 100000 means the stream is broken: connection is closed.

## Packet types

| ID | Packet    | Direction       |
|----|-----------|-----------------|
| 0  | HELLO     | client → server |
| 1  | ACK       | server → client |
| 2  | MESSAGE   | both            |
| 3  | STATE     | server → client |
| 4  | YOU_PLACE | server → client |
| 5  | I_PLACE   | client → server |
| 6  | YOU_GO    | server → everyone |
| 7  | I_GO      | client → server |
| 8  | END_GAME  | server → everyone |

### HELLO (0)

`name_length` (1), `name` (1-20 printable ASCII characters, no space at start or end).
A player who lost connection during a game sends HELLO with the same name and gets their ships back.

### ACK (1)

`player_id` (1), `team_id` (1), `message_length` (1), `message`.
`player_id = 0` means registration failed and `message` says why.
`team_id = 0` means the player joined after the lobby closed and can only watch.
The team given in ACK can change when the lobby closes (teams are balanced); STATE always has the
current team.

### MESSAGE (2)

`message_type` (1), `sender_id` (1), `receiver_id` (1), `message_length` (1), `message`.

| Type | Meaning                                       | Who sends |
|------|-----------------------------------------------|-----------|
| 0    | System message (events, errors for player)    | server    |
| 1    | Chat message                                  | client and server |
| 2    | Timer text ("Turn 5/150 Ends In 40s")         | server    |
| 3    | Status text for status area (ship counts etc.) | server   |

`sender_id = 0` is the server, `receiver_id = 0` is everyone. For chat the server ignores the
`sender_id` sent by the client and sends the message to the receiver (and back to the sender).

### STATE (3)

| Bytes | Field |
|-------|-------|
| 1 | `map_width` |
| 1 | `map_height` |
| 1 | `status`: 0 lobby, 1 ship placement, 2 main game, **3 game over (extension)** |
| 1 | `object_count` |
| 7 × count | map objects |
| 1 | `player_count` |
| 24 × count | players |

Map object (DATA-0019): `object_type`, `x`, `y`, `rotation`, `team_id`, `info`, **`owner_id`**
(1 byte each). `owner_id` is an extension: the player who owns the ship (each player has one ship
of each type, so client can tell own ships from teammate ships).
`x`, `y` is the ship tail; the ship goes from the tail towards `rotation`
(0 up, 1 right, 2 down, 3 left). `info` is the damage bitmask; bit 0 is the tail.

Player (DATA-0020): `id` (1), `team_id` (1), `status` (1), `username` (21 bytes, zero padded).
`socket_nr` is not sent; it only means something on the server.

Each client gets its own STATE: enemy ships are included only if the client's team can see them
(fog of war, BIC-73). Sunk ships are visible to everybody. Shot markers (`object_type` 13 miss,
14 hit) are sent only to the team that fired. Observers see all ships. During game over everyone
sees everything.

State is sent once a second and also right after something changes.

### YOU_PLACE (4) and I_PLACE (5)

YOU_PLACE: `player_id` (1), `object_type` (1). The server asks for ships one at a time, types 1 to 5.
I_PLACE: one map object (7 bytes). The server checks it and either asks for the next ship or sends
a system message with the reason and the same YOU_PLACE again. When placement time runs out,
the remaining ships are placed randomly.

### YOU_GO (6)

Sent to everyone at the start of each turn and after each action, so everybody knows whose turn it is.

| Bytes | Field |
|-------|-------|
| 1 | `player_id` whose turn it is (0 - game is waiting for players to reconnect) |
| 2 | `turn_number` (extension) |
| 2 | `max_turns` (extension) |
| 1 | `shots_left` (extension) |
| 1 | `seconds_left` in turn, 0 - no limit (extension) |
| 5 | `moves_left` for ship types 1-5 (extension) |

### I_GO (7)

| Bytes | Field |
|-------|-------|
| 1 | `player_id` (ignored by server, it knows who sent it) |
| 1 | `action`: 0 move, 1 shoot, **2 end turn (extension)** |
| 1 | `x` shot target |
| 1 | `y` shot target |
| 1 | `object_type`: which own ship acts (extension) |
| 1 | `direction` of move, same values as rotation (extension) |

The server checks every action (turn, ship, range, moves left). A rejected action gets a system
message with the reason.

### END_GAME (8)

`winner_id` (1), `winner_team_id` (1). `winner_team_id = 0` is a draw. `winner_id` is the player of
the winning team with most hits (0 if nobody hit anything). A player who connects during game over
also receives END_GAME.
