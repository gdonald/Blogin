---
title: Action Mailbox
slug: action-mailbox
order: 30
description: Action Mailbox routes inbound email into mailbox classes. A raw message enters through an ingress, becomes an inbound-email record with a processing state...
toc: true
---
Action Mailbox routes inbound email into mailbox classes. A raw message enters
through an ingress, becomes an inbound-email record with a processing state
machine, is matched to a mailbox by recipient, sender, or subject, and is
processed with callbacks.

## Inbound email

`parse-message` turns a raw RFC 822 string into a `Message` with unfolded
headers and a body:

```perl6
use MVC::Keayl::Mailbox;

my $message = parse-message($raw);

$message.from;        # the sender address
$message.to;          # the recipient addresses
$message.cc;
$message.recipients;  # to + cc
$message.subject;
$message.message-id;
$message.header('x-spam-score');
$message.body;
```

An `InboundEmail` wraps the raw message and tracks its state. It starts
`pending` and transitions through `processing`, then `delivered`, `bounced`, or
`failed`:

```perl6
my $email = InboundEmail.new(:$raw);

$email.is-pending;        # True
$email.processing-bang;
$email.delivered-bang;    # or bounced-bang / failed-bang

$email.message;           # the parsed Message
```

## Mailboxes

A mailbox subclasses `Mailbox` and implements `process`. Callbacks run around
processing, and the inbound email is marked `delivered` when `process` returns:

```perl6
class SupportMailbox is Mailbox {
  method process {
    # self.message is the parsed inbound message
  }
}

SupportMailbox.before-processing(-> $mailbox { ... });
SupportMailbox.after-processing(-> $mailbox { ... });
```

`perform-processing` drives the lifecycle: it marks the email `processing`, runs
the before callbacks, calls `process`, runs the after callbacks, and marks the
email `delivered`.

### Bouncing

Calling `bounce` inside processing marks the email `bounced`, runs the
`on-bounce` callbacks, and halts the rest of processing (the after callbacks are
skipped):

```perl6
class RepliesMailbox is Mailbox {
  method process {
    self.bounce unless self.message.from;
  }
}

RepliesMailbox.on-bounce(-> $mailbox { ... });
```

### Failures

An exception raised during processing marks the email `failed`. A matching
`rescue-from` handler runs; with no handler, the exception is re-raised:

```perl6
class ImportMailbox is Mailbox {
  method process { ... }
}

ImportMailbox.rescue-from(X::Parse, -> $mailbox, $error { ... });
```

## Routing

A `Router` maps inbound email to a mailbox. Each `routing` rule matches on
recipient, `to`, `from`, or `subject`, takes a `matching` predicate over the
inbound email, or `all` for a catch-all. Conditions accept an exact string or a
regex (subject and a string match as a substring); every condition in a rule
must match. The first matching rule wins:

```perl6
use MVC::Keayl::Mailbox::Router;

my $router = Router.new;
$router.routing(to => 'support@example.com', mailbox => SupportMailbox);
$router.routing(from => rx:i/ '@billing.example.com' $/, mailbox => BillingMailbox);
$router.routing(subject => 'invoice', mailbox => InvoiceMailbox);
$router.routing(matching => -> $email { $email.message.header('x-spam').defined }, mailbox => SpamMailbox);
$router.routing(all => True, mailbox => DefaultMailbox);

$router.route($email);          # finds the mailbox and processes the email
$router.mailbox-for($email);    # the matched mailbox class, or Nil
```

## Ingress

An ingress receives raw email, records an `InboundEmail`, and hands it to the
router. `RelayIngress` takes one message at a time, as from a webhook relay:

```perl6
use MVC::Keayl::Mailbox::Ingress;

my $ingress = RelayIngress.new(:$router);
$ingress.receive($raw);          # records and routes the message
$ingress.repository.all;         # every recorded inbound email
```

`SourceIngress` pulls from a generic source (an SMTP or POP mailbox), processing
every fetched message:

```perl6
my $ingress = SourceIngress.new(:$router, :$source);
$ingress.poll;                   # fetches from source.fetch and processes each
```

Records persist through a repository.
`MVC::Keayl::Mailbox::Ingress::Repository` defines the interface and
`MemoryRepository` is the in-process implementation.

### Relay endpoint

`RelayController` is the HTTP relay endpoint. Configure the ingress once, and the
controller hands each posted raw email to it:

```perl6
set-mailbox-ingress(RelayIngress.new(:$router));
```

`POST`ing a raw message to the controller returns `204`; with no configured
ingress it returns `503`.
