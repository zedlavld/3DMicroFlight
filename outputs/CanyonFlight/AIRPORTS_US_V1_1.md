# v1.1p20: every lower-48 airport is flyable

The 49 compiled runway records (24 airports in Arizona and Nevada, `source/airports.c`) are
joined at start-up by every flyable US runway from the card: 3,944 airports and 5,287 paved
runways, the large and medium airports and the small ones with 3000 ft or more of pavement
(classes 0-2 of the moving map). They are flown exactly like the compiled ones: the terrain is
flattened to the runway profile, the pavement, markings, apron and buildings are drawn, edge,
approach and glideslope lights work at night, any of them is a start airport, an autotest
start or destination and a test-autopilot LAND destination.

## Data

`tools/build_us_runways.py` (OurAirports, public domain) writes `scenery/airports_fly.bin`
(`CFRWY001`, 392 KB, format in the tool's docstring). Card path:
`sdmc:/3ds/CanyonFlight/scenery/airports_fly.bin`, next to `airports_us.bin`. Built copy:
`F:/CirrusFlightData/airports_us/airports_fly.bin`. Without the file the game is the compiled
table only, as before. The geometry is converted exactly as `build_airports.py` does it
(game-frame headings and lengths, end elevations, crossing-runway shift, apron side). The
moving map's `airports_us.bin` is unchanged, and so is its reader.

## Runtime (`source/airport_index.c`)

- `airports[]` is now a pointer: the compiled records at their old indices 0-48 (saves keep
  working), then the card's. `AIRPORT_COUNT` is still 49 (the compiled table, which is what
  the moving map loops over); `airportCount` is the total.
- A 16 km grid lists, per cell, every record whose flattening reach (or scenery-scatter
  keep-out) touches it. `terrain()`, `airportFlattening()`, wheel friction (`ground.c`) and
  the scatter visit only that cell's records (1.8 on average); scenery parts use
  `airportsInBox`, lights and object parts `airportsNear` (nearest first).
- Object parts: runways within 45 km nearest first, round robin over the 4 parts; within 15 km
  in full detail, beyond that coarse asphalt only; a runway that would leave a part less than
  1,800 vertices for the ground scatter is left out (`sceneryRunwayStats`).
- Lights: the nearest 48 runways within 32 km, re-listed every 250 m; approach-light heights in
  a 128-slot cache of runway ends instead of a table per record.
- Saves: the header's reserved word holds a hash of the start airport's id; if the card file
  changed or is missing, the airport is found again by the hash, else the nearest one.

## Menu

The setup screen's airport row steps through all start airports in state order (in a state the
compiled airports first, then large, medium and small by name). L / R jump to the previous / next
state, Y picks the airport nearest the current aircraft. The row shows `AZ  Sedona  KSEZ`.

## Tests (host)

- `tests/us_airports_test.c`: compiled records unchanged; menu order; cell lists and
  `airportsNear` agree with brute force; every new primary is flat on its centreline;
  object parts at KLAX, KORD, KATL, KJFK, KDFW, KMIA, KSEA, KBOS under 6,000 vertices with every
  runway within 15 km drawn; terrain() 97 -> 100 ns per sample near KPHX.
- `tests/route_landing_test.c` with `CF_AIRPORTS_FLY=<bin>` (and `CF_HOST_BASES=data` for the
  US elevation base on the host): KCOS-KDEN 737 and SR22, KPHX-KDEN 737 FL350, KATL-KCLT 737
  varied weather, KLAS-KTUS unchanged.
