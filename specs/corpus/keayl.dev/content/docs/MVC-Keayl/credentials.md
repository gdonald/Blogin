---
title: Encrypted credentials
slug: credentials
order: 24
description: MVC::Keayl::Credentials stores secrets in an encrypted file checked into the repository, decrypted at runtime with a master key kept out of it. The file...
toc: true
---
`MVC::Keayl::Credentials` stores secrets in an encrypted file checked into the
repository, decrypted at runtime with a master key kept out of it. The file
holds YAML; the master key is 32 hex characters.

## The vault

`resolve` loads the credentials for an app root. It reads the master key from
`KEAYL_MASTER_KEY`, falling back to `config/master.key`,
and decrypts `config/credentials.yml.enc`:

```perl6
use MVC::Keayl::Credentials;

my $credentials = MVC::Keayl::Credentials.resolve;
```

Resolving without a master key raises.

## Reading credentials

A top-level value reads through the associative accessor; a nested value reads
through `read`:

```perl6
$credentials<secret-key-base>;
$credentials.read('aws', 'access-key-id');   # Nil if any key is missing
$credentials.to-hash;                         # the whole decrypted tree
```

## Per-environment credentials

Pass `env` to use an environment-specific file and key,
`config/credentials/<env>.yml.enc` decrypted with `config/credentials/<env>.key`:

```perl6
my $production = MVC::Keayl::Credentials.resolve(env => 'production');
```

## Editing

`keayl credentials-edit` decrypts the file, opens it in `$EDITOR`, and
re-encrypts whatever you save:

```
keayl credentials-edit
keayl credentials-edit --env=production
```

The command reports a missing master key rather than writing an unreadable file.

## Master keys

`keayl new` generates `config/master.key` and an initial encrypted
`config/credentials.yml.enc` containing a `secret-key-base`, and gitignores the
key. Generate a key on its own with `generate-master-key`. `encrypt-content` and
`decrypt-content` encrypt and decrypt a string with a master key (AES-256 with an
HMAC over the payload), should you need the primitives directly.
