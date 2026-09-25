# Migration to v0.4.7

No EVSE pin/state-machine changes are required from v0.4.6.

## Wi-Fi

The recommended configuration is now:

```yaml
wifi:
  networks:
    - ssid: !secret wifi_ssid
      password: !secret wifi_password

    # Optional:
    # - ssid: !secret wifi_ssid_2
    #   password: !secret wifi_password_2

  ap:
    ssid: "${name}-setup"
    password: !secret wifi_fallback_password

captive_portal:
```

Add a fallback AP password to `secrets.yaml`:

```yaml
wifi_fallback_password: "choose-a-strong-password"
```

If the configured station networks cannot be reached, the device exposes the
setup AP and the captive portal can be used to save different Wi-Fi
credentials.

## Compiler warning

v0.4.7 also fixes the `%u` / `uint32_t` warning in the native LEDC startup log.
