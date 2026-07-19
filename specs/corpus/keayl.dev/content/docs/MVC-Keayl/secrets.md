---
title: Secrets
slug: secrets
order: 23
description: MVC::Keayl::Secrets resolves the application secret and derives the keys used for signing and encryption.
toc: true
---
`MVC::Keayl::Secrets` resolves the application secret and derives the keys used
for signing and encryption.

## Resolution

`resolve` reads the secret key base from an explicit config value, then the
`KEAYL_SECRET_KEY_BASE` environment variable, then `SECRET_KEY_BASE`. It dies if
none is set, so a misconfigured deployment fails loudly:

```perl6
my $secrets = MVC::Keayl::Secrets.resolve;                   # from the environment
my $secrets = MVC::Keayl::Secrets.resolve(config => $value); # explicit
```

## Key derivation

`derive-key` produces a key for a given salt, defaulting to 32 bytes returned as
hex. Derivation is deterministic for the same base and salt, and different bases
or salts yield different keys, so one secret key base backs many purpose-specific
keys:

```perl6
$secrets.derive-key('signed cookie');           # 64 hex chars
$secrets.derive-key('messages', length => 16);  # 32 hex chars
```

`signing-key` and `encryption-key` derive the two cookie keys from distinct
salts, so a leak of one does not expose the other. Use the secret key base (or a
derived key) as a controller's `secret`.
