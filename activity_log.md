# Activity Log

## Team
Megan Saint-Jean · Chloe Scarazzo · Kiseong Kim · Michael Mensah · Sadiq Oluwaseyi Alli

## Timeline of contributions

### Kiseong Kim
- July 22, 2025: Implemented core server/client logic — sockets, accept/connect flow, and room broadcast
- July 23, 2025: Defined the three-part structure (`server.c`, `client.c`, `communication.h`) and shared constants
- July 25, 2025: Introduced multithreading with `pthread` (per-client server threads; client receive loop)
- July 27, 2025: Added mutex protection for the client list and documented race-condition hotspots
- July 29, 2025: Wrote initial Ubuntu build/run notes and handed off for feature layer

### Megan Saint-Jean
- July 28, 2025: Parameterized `communication.h` (PORT, MAX_CLIENTS, BUFFER_SIZE); cleaned includes
- July 31, 2025: Implemented `send_all` for atomic writes and integrated server-side logging with its own mutex
- August 2, 2025: Created the Zoom invitation and set up the demo meeting logistics
- August 3, 2025: Hardened disconnect/“server full” handling and did a light race-safety audit
- August 4, 2025: Drafted the in-class demo script and speaker handoffs

### Chloe Scarazzo
- August 1, 2025: Improved console prompts and message formatting for readability
- August 5, 2025: Implemented the GTK-based GUI client (optional but working)
- August 10, 2025: Finalized GUI build/run notes in the Readme and tested on Linux

### Michael Mensah
- August 3, 2025: Added username handshake with unique-name enforcement plus join/leave notices
- August 5, 2025: Implemented command layer: `/who`, `/msg <user> <text>`, `/nick <newname>`, `/quit`
- August 6, 2025: Added timestamps on room messages and integrated them with logging
- August 8, 2025: Final integration cleanup and multi-client sanity checks

### Sadiq Oluwaseyi Alli
- July 31, 2025: Created a Makefile for one-command builds and a `clean` target
- August 4, 2025: Owned formal testing: cross-machine and multi-client runs; captured issues for fixes
- August 7, 2025: Retested edge cases (rapid joins/quits, long lines) and tuned parameters with the team
- August 9, 2025: Packaging for submission and final verification of build steps

## Roles & ownership
- Server foundation: Kim (core sockets/threads), Megan (atomic send + logging + hardening)
- Client: Chloe (GUI), Michael (console client commands)
- Communication header/structure: Kim (initial), Megan (final parameterization)
- Testing & packaging: Sadiq (formal test plan, cross-machine validation, submission bundle)

## Notes
- Console client/server are primary; GUI client is optional but functional.
- All work was completed for this course; no external code was copied.
