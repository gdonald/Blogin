# Security

## Reporting a vulnerability

Report a vulnerability privately through GitHub, at
[Security Advisories](https://github.com/gdonald/Blogin/security/advisories/new).
That opens a report only the maintainer can read, so a fix can be prepared
before the problem is public.

Please do not open a public issue for a vulnerability.

Expect a first reply within a week. A confirmed report is fixed in a release,
and the advisory is published once that release is out.

## What is in scope

Blogin reads files a site author wrote and produces static HTML. A build takes
Markdown, HAML, JSON, and YAML from the site's own directory, so those inputs
are trusted in the sense that an author is not attacking their own site. A crash
on malformed input is still a bug, because the promise is an error naming the
file, line, and column.

Two areas take input from outside the author:

- **The preview server**, `blogin serve`. It parses HTTP requests and WebSocket
  frames, and resolves a request path to a file on disk. It binds loopback only,
  so the reachable audience is processes on the same machine. A path that
  escapes the output directory, or a request that crashes the server, is a
  vulnerability.
- **Generated output**, where a value from a post or a data file reaches the
  page. Markup that escapes its context, so a post can inject script into a
  page it should not, is a vulnerability.

The release binaries are built by a GitHub Actions workflow and carry build
provenance attestations. Verifying one is covered in the
[README](README.md#verifying-a-download).

## Supported versions

The latest release is the supported one. Fixes land in a new release rather than
being backported.
