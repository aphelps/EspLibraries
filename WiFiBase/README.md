# WiFiBase

ESP32 WiFi bring-up with a fallback access point and a small REST management
server. You hand it a list of networks it may join; it joins the first one that
answers, and if none do it can raise its own access point instead so the device
is still reachable. Either way it stands up an HTTP server on port 80 with a
handful of built-in endpoints, and lets you add your own.

There are two audiences for this document and they want different things, so
they are separated:

* **[Part 1 — For programmers](#part-1--for-programmers)**: integrating WiFiBase
  into firmware.
* **[Part 2 — For people using a device](#part-2--for-people-using-a-device-built-on-wifibase)**:
  you are holding hardware that uses this library and you need to get it onto
  your wifi.

Everything below describes the code in `WiFiBase.h`, `WiFiBase.cpp` and
`WiFiBaseHandlers.cpp` as it stands. Where the behaviour is surprising, the
surprise is called out rather than smoothed over.

---

## Part 1 — For programmers

### Scope and dependencies

ESP32 only. `WiFiBase.h` includes `<WiFiManager.h>` and `<Ticker.h>`
unconditionally and the implementation uses `<WiFi.h>`, `WebServer` and
`esp_wifi_disconnect()`, so there is no AVR or ESP8266 build of this. If you
share a `platformio.ini` with AVR environments, put WiFiBase in a `lib_ignore`
list for those environments — PlatformIO's dependency finder does not evaluate
`#if defined(ESP32)` and will otherwise try to compile it.

Required libraries: `WIFIMANAGER-ESP32` (the tzapu WiFiManager fork), the core
`WiFi` / `WebServer` / `Ticker` libraries, and `Debug.h` from the HMTL library
set.

Source layout:

| File | Contents |
| --- | --- |
| `WiFiBase.h` | The whole public API. |
| `WiFiBase.cpp` | Construction, configuration, known-network list, connect logic, AP/portal bring-up. |
| `WiFiBaseHandlers.cpp` | Server creation, endpoint registration, the built-in endpoint handlers, the auth gate, `checkServer()`. |
| `examples/WiFiBaseBasics/` | Minimal WiFiBase sketch, with a `platformio.ini`. |
| `examples/WifiManagerBasics/` | Bare WiFiManager portal demo, no WiFiBase — the baseline for telling WiFiBase bugs from WiFiManager bugs. |
| `test/` | Unity tests, run on hardware (`platformio test`). |

### Shape of a typical integration

```cpp
static WiFiBase wfb(false);          // see the constructor note below

void api_setup() {
  wfb.setConnectTimeoutMs(1500);     // bound the blocking bring-up
  wfb.addKnownNetwork("", "");       // sentinel: use the SDK's stored credentials
  wfb.addKnownNetwork(MY_SSID, MY_PASSWD);
  wfb.configureAccessPoint("my-device", MY_AP_PASSWD);   // >= 8 chars
  wfb.setAuthPassword(MY_AP_PASSWD); // unlocks /network, /scan, /known

  if (wfb.startup() && wfb.getServer()) {
    wfb.getServer()->enableDelay(false);
    wfb.addRESTEndpoint("/status", handleStatus, "\"description\":\"device status\"");
  }
}

void api_loop() {
  wfb.checkServer();                 // no-ops safely when there is no server
}
```

Order matters: everything that configures must happen before `startup()`, and
`addRESTEndpoint()` must happen after it (the server does not exist until then).

### The constructor, and why `useStored = false`

```cpp
WiFiBase(boolean useStored = true);
```

With `useStored = true` the constructor does this:

```cpp
if (useStored && WiFi.SSID()) {
  addKnownNetwork(WiFi.SSID().c_str(), "\0");
}
```

Two things make that flag close to useless in the normal construction order, and
actively harmful in one case:

1. **`WiFi.SSID()` is not "the stored SSID"** — it is the SSID of the AP the
   radio is *currently associated with*. In `WIFI_MODE_NULL` it returns an empty
   `String`, and an empty `String` (whose buffer is `nullptr`) tests **false**.
   So for any instance built before a connect has happened — a file-scope
   `static WiFiBase wfb;` whose constructor runs before `setup()`, or an object
   constructed at the top of `setup()` — nothing is added at all and the flag is
   a silent no-op.
2. **When it does fire, it pairs the SSID with an empty password.** An instance
   constructed while already associated gets known network 0 =
   `{ "TheSSID", "" }`, and the connect loop will later call
   `WiFi.begin("TheSSID", "")` — which cannot rejoin a WPA network.

On top of that, a file-scope `static WiFiBase wfb;` runs its constructor during
static initialisation, before Arduino's `initArduino()`/`setup()` — reaching into
the WiFi driver there is not something to rely on.

**So: pass `useStored = false`, and if you want the credentials the SDK persisted
to flash, add the sentinel yourself.** `_connectToNetwork()` special-cases a
*first* known network whose SSID is the empty string and calls bare
`WiFi.begin()` for it, which uses the stored SSID **and password**:

```cpp
static WiFiBase wfb(false);
wfb.addKnownNetwork("", "");   // must be the first entry to be treated as the sentinel
```

Other construction notes:

* **No copy constructor and no copy assignment.** The destructor frees
  `_knownNetworks`, `delete`s `_server` and calls `WiFi.disconnect()`, so
  `wfb = WiFiBase();` memberwise-copies the pointers and then the temporary's
  destructor frees them, leaving the assigned object dangling. Construct one
  instance in place and configure it. (The header records the intent to make
  these `= delete` once the last copy-assigning caller is migrated.)
* `addKnownNetwork()` returns the **index**, and the first network's index is
  `0`. Do not test the return value for truthiness; compare against
  `WiFiBase::INDEX_DISCONNECTED` (`(uint8_t)-1`). It `strdup()`s both strings,
  and re-adding an existing SSID returns the existing index without touching the
  stored password.
* `configureAccessPoint()`, `setAuthPassword()` and the AP SSID are stored **by
  pointer, not copied**. Those strings must outlive the WiFiBase object — string
  literals, `static` buffers, or something with matching lifetime. (Only the
  known-network list is duplicated.)

### Bring-up: `startup()` is synchronous, and `configBackground()` says so

```cpp
bool configBackground(bool background);   // returns FALSE for `true`
```

Background bring-up is not implemented. Rather than accept a flag that does
nothing, `configBackground(true)` logs `WFB: background startup not implemented`
and returns `false`; `_background` stays `false`. If you need a non-blocking
bring-up, run `startup()` on your own task — `HMTL_Fire_Control` puts the whole
of WiFiBase, bring-up and serving alike, on a FreeRTOS task pinned to core 0 so
`loop()` on core 1 is never delayed by it.

What `startup()` actually does:

1. `_connectToNetwork()` returns immediately if `WiFi.status()` is already
   `WL_CONNECTED`. Otherwise it walks the known-network list **in insertion
   order** — the empty-SSID sentinel first if present, then each
   `WiFi.begin(ssid, passwd)` — waiting on each with `_connectWait()`.
2. `_connectWait()` polls `WiFi.status()` with `delay(100)` until
   `WL_CONNECTED` (true), or `WL_CONNECT_FAILED` (false, returns early), or
   `_connectionTimeoutMs` elapses, in which case it calls
   `esp_wifi_disconnect()` and returns false.
3. On a join, `startup()` creates the server and returns `true`.
4. On no join: if an AP was configured with `configureAccessPoint()`, then
   either `_startupConfigPortal()` (when `useConfigPortal(true)`) or
   `_startupAccessPoint()` runs; the AP path creates the server and returns
   `true`.
5. If nothing joined and no AP is enabled, `startup()` returns **`false` and no
   server is created**.

**Budget the blocking.** Worst case is roughly
`_connectionTimeoutMs × (number of known networks)`, all of it inside
`startup()`. `_connectionTimeoutMs` defaults to `DEFAULT_CONNECT_TIMEOUT`
(10 s); `setConnectTimeoutMs()` lowers it. With three networks at the default
you are looking at half a minute in `setup()`.

The config-portal path (`useConfigPortal(true)`) is a different order of
blocking again: it calls WiFiManager's `startConfigPortal()`, which loops until
someone configures a network through the portal. WiFiBase sets no portal
timeout, and WiFiManager treats a timeout of 0 as *no* timeout — so that call
does not return until the device is configured. Do not use the portal path in
firmware that has to keep doing something else.

### The no-server case is a real, normal path

`_server` is `nullptr` until `_createServer()` runs, which only happens on a
successful `startup()`. For a device with no known network in range and no AP
fallback that is the *ordinary* field state, so all three deref sites tolerate
it:

* `addRESTEndpoint()` logs `WFB: addRESTEndpoint without server` and returns.
* `checkServer()` returns immediately.
* `authorizeConfigRequest()` returns `false` without sending anything.

Nothing panics, but your endpoints silently do not exist. If you care, check
`startup()`'s return value and/or `getServer() != nullptr`, as the sketch above
does.

### Serving

`checkServer()` is just a guarded `_server->handleClient()`. Call it regularly
from wherever you own the CPU.

Two things to know before you call it from a latency-sensitive loop:

* **`handleClient()` is not time-bounded.** The framework's header parsing is
  bounded only per line, and `WebServer::_uploadReadByte()` (`Parsing.cpp`)
  spins on `delay(2)` for as long as a peer keeps a multipart upload connection
  open without sending a body. A single unhelpful client can therefore park the
  thread that calls `checkServer()` indefinitely. This is a framework property,
  not something WiFiBase can guard.
* **It idles with `delay(1)`** — `WebServer`'s `_nullDelay` defaults to `true`.
  Call `getServer()->enableDelay(false)` once the server exists if you want the
  serving thread to spend its own time budget instead.

Both of those are why a dedicated task is the right place for `checkServer()` in
anything with real-time obligations.

`addRESTEndpoint(path, handler, docJson)` registers the handler on the server and
appends `"path":{docJson}` to the JSON served by `/documentation`. Note that
`WebServer` appends handlers and `_parseRequest` takes the **first** matching
one, so a later registration cannot shadow an earlier path — you cannot override
a built-in endpoint from outside the class.

### Built-in endpoints

Created by `_createServer()`, before any caller-added endpoint, on
`setServerPort()`'s port (default 80).

| Endpoint | Auth | Behaviour |
| --- | --- | --- |
| `GET /documentation` | none | JSON map of every registered endpoint and its doc string. |
| `GET /info` | none | JSON: `connected`, `connect_ssid`, `local_IP`, `access_point`, `AP_ssid`, `AP_IP`. |
| `GET /network?ssid=…&passwd=…` | **required** | `connectAddKnownNetwork()` — joins the given network and adds it to the known list. **Mutating**, and blocks inside `handleClient()` for up to the connect timeout. `200` with `{"connected":true,…}` or `400` with `{"connected":false,…}`; both include `elapsed` in ms. |
| `GET /scan` | **required** | `WiFi.scanNetworks()` inline — a multi-second block. Returns `count`, `elapsed`, and `networks` as `[ssid, rssi, "*"\|""]` triples (`"*"` = encrypted). |
| `GET /known` | **required** | Lists every stored SSID. Gated because it is a disclosure, not a mutation. |
| anything else | none | `404`, `text/plain`, `Endpoint <uri> not defined`. |

`/network` and `/scan` are the two largest single-call blockers in the library.
If that matters for your device, the answer is to isolate the thread that runs
`checkServer()`, because you cannot un-register them.

### The auth gate, and how to opt out

`_authorizedConfig()` runs at the top of `/network`, `/scan` and `/known`, in
this order:

1. If `allowUnauthenticatedConfig(true)` was called → allowed, no check.
2. Else if no auth password has been set → **`403`**
   `{"error":"no auth password configured"}`.
3. Else → HTTP Basic with username **`admin`** and the password from
   `setAuthPassword()`. On mismatch, `requestAuthentication()` sends the `401`
   challenge.

So the default is closed: a build that sets no password serves `403` on the
config endpoints, and the unauthenticated surface is read-only status. A caller
that wants the old open behaviour has to ask for it:

```cpp
wfb.allowUnauthenticatedConfig(true);   // opt out of the gate entirely
```

`setAuthPassword()` refuses `nullptr` and the empty string (returns `false` and
leaves the previous value in place), and remember it keeps the pointer rather
than a copy.

For your own mutating endpoints, reuse the same gate:

```cpp
void handleSomethingDangerous() {
  if (!wfb.authorizeConfigRequest()) {
    return;   // it has already sent the 401 or 403
  }
  ...
}
```

`authorizeConfigRequest()` sends the refusal itself, so the handler just returns.
It returns `false` with no response when there is no server.

Note HTTP Basic is plaintext. Over the fallback AP the password is only as
private as the WPA2 link, and over a LAN it is visible to anything that can
watch the traffic. It is a "keep casual visitors out of the wifi config" gate,
not a security boundary.

### Access point details

* `configureAccessPoint(ssid, passwd)` enables the fallback but does not start
  it — that happens inside `startup()` only when no known network joins. It
  **refuses a non-empty password shorter than 8 characters**, because the SDK
  silently ignores a short passphrase and brings the AP up **open**; better to
  fail the call than ship an open AP.
* `useConfigPortal(true)` swaps the plain AP for WiFiManager's captive portal.
  Both are refused once the AP is already active.
* `disableAccessPoint()` clears the flag, shutting the AP down first if it is
  running.
* Known gap: `_startupAccessPoint()` ignores `WiFi.softAP()`'s return value and
  always reports success, so an SDK-level softAP failure is not surfaced.

### Debug output

Both translation units honour a per-file debug level set before `<Debug.h>` is
included, falling back to `DEBUG_HIGH`:

```
-DDEBUG_LEVEL_WIFIBASE=3 -DDEBUG_LEVEL_WIFIBASEHANDLERS=3
```

All output is prefixed `WFB:`. Level 3 gets you the connect attempts, the AP
bring-up and the assigned IP; 4 and 5 add the known-network bookkeeping and
per-request lines.

### Testing

`test/` holds Unity tests that run **on hardware** — they exercise the real
radio, so they need a real network:

```sh
cd WiFiBase/test
PLATFORMIO_BUILD_FLAGS='-DUSE_SSID=\"network\" -DUSE_PASSWD=\"password\"' pio test
```

`examples/WiFiBaseBasics/` is the quickest manual smoke test; its
`platformio.ini` points `lib_dir` at the shared Arduino library directory.

---

## Part 2 — For people using a device built on WiFiBase

You have a device — a controller, a light, a sensor — whose firmware uses this
library to get onto wifi. This section is about what you will see and what to do
about it. Names, passwords and extra features are chosen by whoever built the
device, so its own documentation wins over this one wherever they disagree.

### What the device does when you power it on

It tries the wifi networks it already knows, one at a time, and stops at the
first one that lets it in. Each attempt can take up to the device's connect
timeout — a few seconds, ten at the library default — so a device with several
known networks may sit quietly for a while before it gives up.

* **If it gets on a network**, you will see nothing from the wifi side at all —
  it just works. To talk to it you need its IP address, which the device usually
  reports on its serial/startup output, and which your router will also list
  among its connected clients. Then `http://<that-address>/info` in a browser
  shows what the device thinks its wifi situation is.
* **If it cannot get on any network**, one of two things happens, depending on
  how the device was built: it raises **its own wifi network** so you can still
  reach it, or it simply carries on without wifi. Some devices deliberately do
  not raise their own network — a safety-critical controller, for instance,
  where an unnecessary radio is a liability. If nothing appears, that is a
  legitimate design choice and not necessarily a fault.

### Joining the device's own network

When the device raises its own network, it looks like any other wifi network in
your phone's or laptop's list. **The name and password come from the device**,
not from this library — look them up in the device's documentation or its
startup output. Almost always it is password-protected: the library rejects a
password that is too short for WPA2 rather than quietly downgrading the network
to an open one, so if the device sets a password at all, the network is properly
protected. (A device that deliberately sets no password gets an open network; if
the setup network needs no password, that was the builder's choice.)

Join that network from a phone or laptop, exactly as you would join a café
network. Your device will probably warn you that there is no internet on it —
that is correct and expected. It is a single small device, not a router. Tell
your phone to stay connected anyway.

Once you are on it, there are two flavours of setup, again depending on how the
device was built.

**Flavour A — a setup page appears.** A configuration page opens by itself
(the same "sign in to this network" flow you get in hotels); if it does not,
type `192.168.4.1` into your browser. Choose "Configure WiFi", pick your own
network from the list, type its password, and save. The device joins your
network and its own setup network disappears. Note that while it is waiting for
you it waits indefinitely — it will not give up and it will not move on to
anything else, so finish the setup or power-cycle it.

**Flavour B — no page, just an address.** Open `http://192.168.4.1/info` in a
browser. You will get a line of machine-readable text describing the device's
wifi state — that is normal, this flavour has no pretty pages. To move the
device onto your own network, visit:

```
http://192.168.4.1/network?ssid=YourNetworkName&passwd=YourNetworkPassword
```

Your browser will ask for a username and password first. **The username is
`admin`** and the password is the device's — usually the same one you just used
to join its setup network, but check the device's documentation. If it worked
you get back text containing `"connected":true` and the address the device
picked up on your network; if it did not, `"connected":false`.

Two other addresses are useful, and ask for the same username and password:

* `http://192.168.4.1/scan` — the networks the device can currently see. Slow;
  give it several seconds.
* `http://192.168.4.1/known` — the networks the device has remembered.

### When it goes wrong

**No device network appears.** Either the device already found a network it knows
(check its status output — it may be happily online), or this device was built
without the fallback network. There is no way to make one appear from outside.

**Your phone keeps dropping the device network, or refuses to stay on it.** Turn
off "automatically switch to a network with internet" / "smart network switch"
for the duration. Phones dislike networks that go nowhere.

**Your network does not show up in the scan list.** Two common causes. The
device's radio is 2.4 GHz only, so a network broadcasting only on 5 GHz will
never be visible to it — if your router publishes both bands under one name,
this usually just works, and if it splits them, use the 2.4 GHz name. Or the
network is hidden, in which case it will not be listed but you can still try it
by typing its exact name into the `/network` address above.

**`/network` came back with `"connected":false`.** The device could not join. In
order of likelihood: the password is wrong; the network name is misspelled
(names are case-sensitive and spaces count); the network is out of range from
where the device is sitting.

**Important, and unintuitive:** the ESP32 saves the network details it was handed
*before* it tries them. So a failed attempt overwrites whatever network the
device had previously remembered. If a `/network` attempt fails and you then
power-cycle the device, do not be surprised when it comes up on its own setup
network rather than the network it used to be on — the old details are gone. Just
issue `/network` again with the correct details. If you have a working setup and
only want to *add* another network, be aware you are risking the working one.

**The browser asks for a username and password you do not have.** The
network-configuration addresses are deliberately locked; there is no bypass from
the network side. The password belongs to the device — its documentation, its
label, or its startup output. Without it you can still read `/info`, which needs
no password.

**You get `no auth password configured` instead of a password prompt.** The
device was built with no configuration password and without permitting
unprotected configuration, so the configuration addresses are closed to
everybody. That is the safe default, not a malfunction, and it cannot be opened
from the network — it takes a firmware or device-side change.

**The device forgot the network after you changed your wifi password.** Expected:
it stores what it was told. Re-do the setup above with the new password.

**Everything is slow or unresponsive while you are reconfiguring.** Both the scan
and the join are slow operations that the device performs while you wait — a scan
takes seconds, a join can take as long as its connect timeout. Give each request
time to finish rather than reloading repeatedly.
