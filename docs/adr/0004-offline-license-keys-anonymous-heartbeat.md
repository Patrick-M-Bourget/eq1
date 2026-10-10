# Offline License Keys, and a Heartbeat kept apart from licensing

A License Key is a signed blob (email, Tier, major version, issue date, key ID) that eq1 verifies against a public key built into the plugin. It is never activated or checked online, and it isn't bound to a machine. We chose this because producers work offline (studios, touring rigs) and every client-side check gets cracked anyway: showing "Licensed to <email>" stops casual sharing, and anything stronger only costs paying users. The Heartbeat is a separate, anonymous, opt-out report (install ID, version, OS, host, format, Tier) and never carries the email or key ID, so usage numbers can't be tied to a buyer.

## Considered Options

- **Online activation with machine limits** (the store's license API, or a service like Keygen or Moonbase). Rejected: eq1 would stop working offline or when the vendor's service goes away, and it brings support load (new laptop, reinstall).
- **Machine-locked keys issued offline** through a challenge and response file. Rejected: the same support load without online activation, and it doesn't stop cracks.
- **iLok/PACE copy protection.** Rejected: too costly for a solo developer. PACE signing is still needed for AAX, but that isn't copy protection.
- **Ed25519 signatures.** Rejected in favour of JUCE's own RSA so we don't add a dependency.

## Consequences

- A key can't be revoked. A leaked key keeps working for its major version, and a new major version needs new keys.
- Telemetry is never used to enforce licensing, and licensing never needs the network.
