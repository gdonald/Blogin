---
title: Active Storage
slug: active-storage
order: 31
description: Active Storage attaches files to model records through a pluggable storage service. Blob and attachment records hold the metadata, a service stores the...
toc: true
---
Active Storage attaches files to model records through a pluggable storage
service. Blob and attachment records hold the metadata, a service stores the
bytes, and signed URLs serve them back. Persistence is delegated through a
repository so the web-side glue stays independent of any one ORM.

## Blobs

A `MVC::Keayl::Storage::Blob` records a stored file: its storage `key`, original
`filename`, `content-type`, `byte-size`, and a `checksum`. `Blob.build` derives
the size and checksum from the data:

```perl6
use MVC::Keayl::Storage;

my $blob = MVC::Keayl::Storage::Blob.build(
  $bytes,
  filename     => 'avatar.png',
  content-type => 'image/png',
);

$blob.byte-size;   # number of bytes
$blob.checksum;    # SHA-1 of the contents
$blob.extension;   # 'png'
$blob.is-image;    # True
```

`checksum-for($data)` and `generate-key()` are available on their own when you
need them.

## Services

A service stores and retrieves bytes by key. `MVC::Keayl::Storage::Service`
defines the interface: `upload`, `download`, `delete`, `exist`, and `url`.

```perl6
use MVC::Keayl::Storage::Service;

my $disk = DiskService.new(root => '/var/storage'.IO);

$disk.upload($key, $bytes);
$disk.download($key);    # Buf of bytes, or Nil
$disk.exist($key);       # Bool
$disk.delete($key);
$disk.url($key);         # absolute on-disk path
```

`DiskService` writes under a configured root, sharding files by a digest of the
key. `ExternalService` wraps a client object shaped like an S3 or GCS adapter
(`upload`, `download`, `delete`, `exist`, `url`). `MirrorService` writes and
deletes through a primary service and a list of mirrors, reading from the
primary:

```perl6
my $service = MirrorService.new(primary => $disk, mirrors => [$backup]);
```

## Attaching to records

A model includes `MVC::Keayl::Storage::Attachable` and declares its attachments.
`has-one-attached` gives a single attachment, `has-many-attached` a collection.

```perl6
use MVC::Keayl::Storage::Attached;

class User does Attachable {
  has $.id;
}
User.has-one-attached('avatar');
User.has-many-attached('documents');
```

Configure the default service and repository once at boot:

```perl6
set-storage-service(DiskService.new(root => 'storage'.IO));
set-storage-repository(MemoryRepository.new);   # or an ORM-backed repository
set-storage-secret($secret-key-base);           # signs blob URLs and ids
```

Attach from an uploaded file, a raw IO hash, an existing blob, or a signed blob
id:

```perl6
$user.avatar.attach(%( io => $bytes, filename => 'me.png', content-type => 'image/png' ));

$user.avatar.is-attached;     # True
$user.avatar.filename;        # 'me.png'
$user.avatar.download;        # the stored bytes
$user.avatar.signed-id;       # a tamper-proof id for later attach

$user.avatar.detach;          # unlink the attachment
$user.avatar.purge;           # unlink and delete the blob and its bytes
```

Attaching to a `has-one-attached` again replaces the existing attachment.
`has-many-attached` appends:

```perl6
$user.documents.attach(
  %( io => $one, filename => 'a.pdf' ),
  %( io => $two, filename => 'b.pdf' ),
);

$user.documents.elems;   # 2
$user.documents.blobs;   # the attached blobs
```

Each accessor call (`$user.avatar`, `$user.documents`) returns a proxy that
caches its repository lookup, so reads like `is-attached`, `blob`, and
`download` on the same proxy share one query. `attach`, `detach`, and `purge`
through a proxy refresh its cache, and a fresh accessor call re-reads the
repository.

### Repository

A repository persists blob and attachment records.
`MVC::Keayl::Storage::Repository` defines the interface, and `MemoryRepository`
is an in-process implementation. An ORM-backed repository implements the same
methods (`create-blob`, `find-blob`, `attachments-for`, and the rest) and is
passed to `set-storage-repository`.

## Serving and URLs

Blobs are served through signed URLs. `blob-serving-path` builds a path carrying
a signed blob id; `:proxy` selects streaming over redirecting, and `expires-in`
sets an expiry:

```perl6
use MVC::Keayl::Storage::Attached;

blob-serving-path($blob);                       # /keayl/blobs/redirect/<signed>/<filename>
blob-serving-path($blob, :proxy);               # /keayl/blobs/proxy/<signed>/<filename>
blob-serving-path($blob, expires-in => 300);    # a short-lived url

$user.avatar.path;                              # the same, for an attachment
$user.avatar.path(:proxy);
```

Two controllers serve the routes. `RedirectController` verifies the signed id
and redirects to the service URL. `ProxyController` verifies the signed id,
downloads the bytes, and streams them with the blob's content type and a
content disposition (`inline` by default, `attachment` when the `disposition`
param asks for it).

A signed id only verifies for its purpose and before its expiry, so tampered or
stale URLs return `404`.

## Variants and direct upload

`MVC::Keayl::Storage::Variant` transforms an image blob and caches the result as
a separate object keyed by a digest of the transformations. A variant is
processed lazily on first request:

```perl6
use MVC::Keayl::Storage::Variant;

my $variant = $user.avatar.variant(resize => '100x100');

$variant.is-processed;   # False until requested
$variant.download;       # transforms, stores, and returns the bytes
$variant.url;            # the service url for the cached variant
```

The transformer is pluggable. `IdentityTransformer` passes the bytes through;
`CallableTransformer` wraps a block; a production transformer shells out to an
image library. Set the default with `set-storage-transformer`.

Direct upload lets a client send bytes straight to storage.
`DirectUploadsController#create` records a blob from the submitted metadata and
returns its signed id plus an upload URL:

```json
{
  "signed-id": "...",
  "key": "...",
  "direct-upload": { "url": "/keayl/disk/<token>", "headers": { "Content-Type": "text/plain" } }
}
```

The client then `PUT`s the bytes to that URL, which `DiskController#update`
verifies and stores. The `file-field` form helper wires the browser side:

```perl6
form.file-field('avatar', %( direct-upload => '/keayl/direct-uploads' ));
# <input type="file" data-direct-upload-url="/keayl/direct-uploads" ...>
```
