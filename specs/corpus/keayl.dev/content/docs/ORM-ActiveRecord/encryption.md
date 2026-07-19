---
title: Encryption
slug: encryption
order: 32
description: encrypts stores a column's value encrypted at rest. Declare it in submethod BUILD. Encryption needs at least one key:
toc: true
---
`encrypts` stores a column's value encrypted at rest. Declare it in
`submethod BUILD`. Encryption needs at least one key:

```perl6
use ORM::ActiveRecord::Support::Secrets;
encryption-keys('a-strong-key');     # or set AR_ENCRYPTION_KEY / AR_ENCRYPTION_KEYS
```

```perl6
class User is Model {
  submethod BUILD {
    self.encrypts('ssn', :deterministic);        # queryable
    self.encrypts('notes');                        # random IV, not queryable
    self.encrypts('email', :deterministic, :downcase);
  }
}
```

The model works in plaintext; the column holds AES-256-CBC ciphertext
authenticated with HMAC-SHA256 (encrypt-then-MAC). Reads decrypt transparently:

```perl6
my $u = User.create({ ssn => '123-45-6789' });
User.find($u.id).ssn;        # '123-45-6789'
```

## Deterministic vs random

Random encryption (the default) uses a fresh IV each time, so equal plaintexts
produce different ciphertexts and the column cannot be searched.

Deterministic encryption derives the IV from the plaintext, so equal plaintexts
produce equal ciphertexts. That makes the column queryable: a `where` on a
deterministic column encrypts the search value automatically.

```perl6
User.where({ ssn => '123-45-6789' });   # matches, value encrypted for the lookup
```

`:downcase` normalises the value before encrypting, which also makes a
deterministic lookup case-insensitive.

## Key rotation

`encryption-keys` takes the keys primary-first. New values are encrypted with
the primary key; decryption tries each key in turn. To rotate, prepend a new
key — existing data still decrypts with the old one:

```perl6
encryption-keys('new-key', 'old-key');
```

## Backfilling

A value that matches no key is returned unchanged, so a column can be migrated
from plaintext. `encrypt-existing` re-saves every record, encrypting any value
that is still plaintext:

```perl6
User.encrypt-existing;
```
