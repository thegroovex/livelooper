# livelooper remote protocol v1.0

Shared contract between livelooper (server, `src/remote/`) and livelooper control (Android client).
Change only by bumping minor (compatible additions) or major (breaking). Both repos keep a copy; this file is the master.

## Transport
- **Discovery:** server broadcasts a UDP datagram every 1 s to 255.255.255.255:**47801** (and each interface's broadcast address) while Remote is on:
  ASCII `LLRC1 ` + JSON `{"name":"<instance name>","port":47800,"version":"<livelooper version>","id":"<instance uuid>"}`.
- **Control:** TCP, server listens on **47800** (configurable), `TCP_NODELAY` both sides. Several clients may be connected.
- **Frame:** `u32 length` (LE, bytes of type+payload, max 1 MiB) · `u8 type` · `payload`.
  JSON payloads are UTF-8 objects; binary payloads are little endian.

## Session
1. Client → `HELLO`, server → `WELCOME` (contains a fresh random 16-byte nonce).
2. Not paired yet: client → `PAIR` with the 6-digit code shown in livelooper (Settings → Remote). Server → `PAIRED {key}` (32 random bytes, hex) and stores {deviceId, deviceName, key}. Wrong code → `ERROR bad_code` (max 5 tries per minute per connection, then close).
3. Client → `AUTH {deviceId, hmac}` with `hmac = hex(HMAC-SHA256(key, nonce))`. Server → `AUTH_OK` then immediately `PARAM_TABLE`, `STATE`, a `WAVE` per non-empty track.
4. Before `AUTH_OK` the server answers only HELLO/PAIR/AUTH/PING; everything else → `ERROR unauthorized`.
5. Major mismatch → `ERROR proto_major` (message tells which side to update) and close.

## Messages
| type | name | dir | payload |
|---|---|---|---|
| 0x01 | HELLO | C→S | JSON `{"proto":[1,0],"app":"0.2.0","deviceId":"<uuid>","deviceName":"Galaxy S25 Ultra","caps":0}` |
| 0x02 | WELCOME | S→C | JSON `{"proto":[1,0],"livelooper":"1.7.x","instance":"<name>","nonce":"<32 hex>","caps":0}` |
| 0x03 | AUTH | C→S | JSON `{"deviceId":"…","hmac":"<64 hex>"}` |
| 0x04 | PAIR | C→S | JSON `{"deviceId":"…","deviceName":"…","code":"123456"}` |
| 0x05 | PAIRED | S→C | JSON `{"key":"<64 hex>"}` |
| 0x06 | AUTH_OK | S→C | JSON `{}` |
| 0x07 | ERROR | S→C | JSON `{"code":"bad_code|unauthorized|proto_major|bad_frame|not_paired","message":"…"}` (`not_paired` = deviceId unknown/revoked → client forgets key and pairs again) |
| 0x10 | PARAM_TABLE | S→C | JSON `{"hash":"…","params":[{"i":0,"id":"track1Level","name":"Track 1 Level","min":0,"max":2,"def":1,"type":"float|bool|choice","choices":["…"]}]}` — `i` is the wire index for this session; `id` is the stable host parameter ID (append-only). |
| 0x11 | PARAM_SET | both | binary `u16 i · f32 normalized(0..1)`. Client→server sets the host parameter via `setValueNotifyingHost`; server→client echoes every change (host automation, MIDI, GUI) at most 60 Hz per param. |
| 0x12 | GESTURE | C→S | binary `u16 i · u8 begin(1)/end(0)` → beginChangeGesture/endChangeGesture |
| 0x13 | TRIGGER | C→S | binary `u8 action · u8 track(0-based, 255 = global) · u8 down(1)/up(0)` – routed through the same command path as MIDI-mapped actions, so every button stays MIDI-mappable. Actions: 1 REC_PLAY_DUB, 2 STOP, 3 CLEAR, 4 UNDO, 5 MUTE_TOGGLE, 6 SOLO_TOGGLE, 7 ALL_START_STOP, 8 TAP_TEMPO, 9 CAPTURE. Unknown → ignored. |
| 0x20 | STATE | S→C | JSON on change (max 20 Hz) and every 1 s: `{"bpm":120.0,"playing":true,"tracks":[{"state":"EMPTY|REC|PLAY|DUB|STOP|ARMED","bars":2.0,"level":0.5,"muted":false,"solo":false,"pan":0.5}]}` (level/pan normalized like their params) |
| 0x21 | TICK | S→C | binary ~30 Hz: `f32 bpm · u8 n · n×(f32 phase 0..1, f32 meter 0..1) · f32 masterMeter` |
| 0x22 | WAVE | S→C | binary on content change: `u8 track · u16 count · count×u8 peak(0..255)` (count ≤ 512, overview of the whole loop) |
| 0x30 | PING | C→S | binary `u32 seq · u64 clientNanos` |
| 0x31 | PONG | S→C | echo of PING payload |

## Example frames (hex)
- PING seq 1, t 0: `0d000000 30 01000000 0000000000000000`
- PARAM_SET i=3 v=0.5: `07000000 11 0300 0000003f`
- TRIGGER REC_PLAY_DUB track 0 down: `04000000 13 01 00 01`

## Server rules (livelooper)
- Own thread(s); never lock or allocate on the audio thread. Telemetry (phase, meters, track state) is read from atomics the engine already updates or a lock-free snapshot.
- Off by default. Settings → Remote (EN+DE): on/off, port, pairing code (regenerate), paired devices (revoke). Paired devices persist in app settings.
- Listens on all interfaces but drops connections from non-private, non-link-local, non-loopback addresses.

## livelooper notes
Implementation in this repo: `src/remote/` (RemoteProtocol, RemoteServer, RemoteSettings), glue in `src/plugin/ProcessorRemote.cpp`, UI in `src/plugin/EditorRemote.cpp` (SETUP → Remote), tests in `tests/RemoteTests.cpp`. Where the implementation fills a gap or reads the spec in a particular way:

- **Initial parameter values:** `PARAM_TABLE` carries no current values, so right after `AUTH_OK · PARAM_TABLE · STATE · WAVE…` the server sends one `PARAM_SET` per parameter with its current value (the normal echo path, first poll). Echoes come from 30 Hz polling (≤ 60 Hz per param); a value a client set itself is not echoed back to that client unless the host changes it.
- **Wrong HMAC → `ERROR not_paired`** (not `unauthorized`): the device holds a key livelooper no longer has (re-paired elsewhere, settings reset), so the client forgets it and pairs again. `unauthorized` is used for AUTH before HELLO and for any other message before AUTH_OK.
- **Pairing rate limit:** 5 wrong codes within 60 s on one connection → the 5th `ERROR bad_code` is followed by close (a new connection starts counting again). The pairing code does not change after a successful pairing (SETUP → Remote → Pairing → New pairing code).
- **Nonce:** HMAC is computed over the raw 16 nonce bytes with the raw 32 key bytes (not over the hex text). One nonce per HELLO; it is consumed by a successful AUTH.
- **Parameter table:** every host parameter in host order (`i` = index in the plug-in's parameter list, `id` = stable host parameter ID, `name` = localised host name). `hash` = first 8 bytes (16 hex) of SHA-256 over the IDs joined with `\n`. AudioParameterInt → `float`. The master level has the ID `master`, track level/pan `t1_level`, `t1_pan` … (livelooper control's heuristic needs `master` to match without "level").
- **TRIGGER mapping** (fires on `down=1`, `up` is ignored; track 0-based, 255 = global):
  1 REC_PLAY_DUB → `tN_recplay` (track only) · 2 STOP → `tN_stop`, global `allstop` · 3 CLEAR → `tN_clear`, global `clearall` · 4 UNDO → `tN_undo` (track only, there is no global undo) · 5 MUTE_TOGGLE → toggles `tN_mute` · 6 SOLO_TOGGLE → toggles `tN_solo` · 7 ALL_START_STOP → `allstartstop` · 8 TAP_TEMPO → `tap` · 9 CAPTURE → `tN_capture`, global `capture`.
  Triggers press the same trigger parameters MIDI learn and the computer keyboard use (rising edge on the audio thread → the same engine command); toggles flip the parameter like a learned MIDI toggle. REC_PLAY_DUB / UNDO / MUTE / SOLO with track 255 are ignored.
- **STATE:** `tracks` has one entry per visible track (5 or 8). `ARMED` = empty track waiting for the quantised start of a recording (also auto record). `bars` = loop length in bars (0 while the first recording is running). Sent on change (checked at 20 Hz) and at least once a second.
- **TICK:** `phase` = loop position 0..1, `meter` = track peak since the last tick (input while recording / overdubbing, else output), `masterMeter` = main output peak since the last tick; linear 0..1, not dB.
- **WAVE:** 512 peaks over the whole loop (or the recording so far), linear peak × 255; refreshed about 5 times a second while a device is connected, sent only when the content changed; a track that became empty gets `count = 0`.
- **Discovery:** JSON `id` is a fresh UUID per server start; `name` = `livelooper (<computer name>)` or `livelooper plug-in (<computer name>)`. Sent to 255.255.255.255 and every interface broadcast address.
- **Connections:** at most 16 clients; a client that stops reading is dropped when its send queue passes 4 MiB or nothing could be written for 10 s. IPv4 only (JUCE sockets).
- **Settings:** on/off (default off), port (47800, 47810, 47820, 47830, 47840), pairing code and paired devices (id, name, key, time) are stored in `livelooper.settings` (per computer, shared by all instances). Several instances: the first takes the port, the others retry every 5 s.
- **Not implemented yet:** `caps` is always 0.
