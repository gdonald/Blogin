---
title: Mailer
slug: mailer
order: 27
description: MVC::Keayl::Mailer builds email the way a controller builds a response: an action renders HAML views into the parts of a message, and a delivery method...
toc: true
---
`MVC::Keayl::Mailer` builds email the way a controller builds a response: an
action renders HAML views into the parts of a message, and a delivery method
sends it.

## Defining a mailer

Subclass `MVC::Keayl::Mailer` and give each action a method that calls `mail`:

```perl6
class NoticeMailer is MVC::Keayl::Mailer {
  method welcome($user) {
    self.mail(
      to      => $user.email,
      from    => 'noreply@example.com',
      subject => 'Welcome',
      locals  => %( name => $user.name ),
    );
  }
}
```

`build($action, |args)` runs the action and returns the
[`MVC::Keayl::Mail`](#the-message); `deliver($action, |args)` builds it and hands
it to the configured delivery, returning the mail.

```perl6
my $mailer = NoticeMailer.new(view-renderer => $view, delivery => $delivery);
$mailer.deliver('welcome', $user);
```

## Views

`mail` renders the message body from views named for the mailer and action,
`<mailer-path>/<action>` (so `NoticeMailer#welcome` looks under
`notice_mailer/welcome`). It renders an `html` and a `text` part when both
templates exist:

```
app/views/notice_mailer/welcome.html.haml
app/views/notice_mailer/welcome.text.haml
```

A message with both parts is multipart; one with a single template carries just
that part. Pass `:template` to override the lookup, `:locals` for the view
locals, `:cc` / `:bcc` for extra recipients, `:reply-to` for a reply address,
and `:headers` for custom headers. `default-from` on the mailer supplies a sender
when an action gives none.

## Class-level defaults

`default` sets values an action falls back to. An explicit value on the action
always wins:

```perl6
class NoticeMailer is MVC::Keayl::Mailer { ... }
NoticeMailer.default(from => 'noreply@example.com', reply-to => 'support@example.com');
```

Keys other than `from` and `reply-to` become default headers merged into every
message.

## Callbacks

`before-action` and `after-action` run around the action. Each receives the
mailer; the built message is available as `self.message` after the action runs:

```perl6
NoticeMailer.before-action(-> $mailer { ... });
NoticeMailer.after-action(-> $mailer { $mailer.message.headers<X-Mailer> = 'Keayl' });
```

## Translated subjects

When an action omits `:subject` and the mailer has an `i18n` backend, the subject
is looked up at `<mailer-path>.<action>.subject` and interpolated with the view
locals:

```yaml
en:
  notice_mailer:
    welcome:
      subject: "Welcome, %{name}"
```

## The message

`MVC::Keayl::Mail` holds `from`, `to`, `cc`, `bcc`, `reply-to`, `subject`, the
`html-part` and `text-part`, custom `headers`, and `attachments`. `encoded`
serializes it to an RFC-822 message, using `multipart/alternative` for a text and
an html part, wrapped in `multipart/mixed` when there are attachments.

## Attachments

An action populates `attachments` before calling `mail`. Assign a string or
`Blob` of content, or a hash with `content-type` and `content`. The content type
is inferred from the filename when not given. `attachments.inline` marks an
attachment inline and gives it a `Content-ID` for referencing from the body:

```perl6
method newsletter {
  self.attachments<report.pdf> = $pdf-bytes;
  self.attachments.inline<logo.png> = %( content-type => 'image/png', content => $png-bytes );
  self.mail(to => 'reader@example.com', subject => 'This week');
}
```

## Async delivery

`deliver-later` enqueues a delivery job through the configured
[job queue](/docs/MVC-Keayl/jobs) instead of sending inline. When the job runs, it delivers
the message:

```perl6
$mailer.deliver-later('welcome', $user);
```

## Interceptors and observers

Interceptors run before delivery and may rewrite the message (redirecting all
mail in staging, for example). Observers run after delivery. Each is a callable
taking the mail, or an object with a `delivering-email` / `delivered-email`
method:

```perl6
MVC::Keayl::Mailer.register-interceptor(-> $mail { $mail.to = ['staging@example.com'] });
MVC::Keayl::Mailer.register-observer(-> $mail { log-delivery($mail) });
```

## Previews

A preview class subclasses `MVC::Keayl::Mailer::Preview` with a method per sample
message that builds and returns a mail. Register it, and `PreviewController`
serves an index and the rendered parts at a dev route:

```perl6
class NoticePreview is Preview {
  method welcome { NoticeMailer.new(view-renderer => $view).build('welcome', $sample-user) }
}
Previews.register('notice_mailer', NoticePreview);
```

`PreviewController#show` reads `preview`, `email`, and `part` params and renders
the `html` part (the default), the `text` part, or the `raw` encoded message.

## Delivery

A delivery is any `MVC::Keayl::Mailer::Delivery` (a role with one `deliver(Mail)`
method). Three are built in:

- **Test** (`MVC::Keayl::Mailer::Delivery::Test`) collects messages in a
  process-wide list for assertions. `deliveries` reads them, `clear` empties it.
- **File** (`MVC::Keayl::Mailer::Delivery::File`) writes each `encoded` message to
  a numbered `.eml` file under its `directory`.
- **SMTP** (`MVC::Keayl::Mailer::Delivery::SMTP`) builds the envelope (`host`,
  `port`, `from`, `to`, `data`) and hands it to a pluggable `transport` callable,
  which performs the actual send.

```perl6
my $delivery = MVC::Keayl::Mailer::Delivery::SMTP.new(
  host      => 'mail.example.com',
  port      => 587,
  transport => -> %envelope { send-over-smtp(%envelope) },
);
```
