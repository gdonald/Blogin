---
title: Test support
slug: test-support
order: 37
description: MVC::Keayl::TestSupport drives requests through an application in-process and asserts on the result, with helpers for routing, controllers, mailers, jobs,...
toc: true
---
`MVC::Keayl::TestSupport` drives requests through an application in-process and
asserts on the result, with helpers for routing, controllers, mailers, jobs, and
cable. Every assertion throws `X::MVC::Keayl::Test::AssertionFailed` on failure
and returns on success, so it works with `Test` (`lives-ok`/`dies-ok`) and with
`BDD::Behave` (`expect({ ... }).not.to.throw`).

## Integration sessions

An `IntegrationSession` wraps any application endpoint (a dispatcher or the full
`Application.endpoint`) and issues requests, persisting the cookie jar across
them so sessions and flash survive:

```perl6
use MVC::Keayl::TestSupport;

my $session = IntegrationSession.new(app => $dispatcher);

$session.get('/login');
$session.post('/sessions', body => 'user=ada');
$session.get('/dashboard');     # the session cookie from the login is sent back
```

`get`, `post`, `put`, `patch`, and `delete` take a target and optional `headers`
and `body`. The last response is `session.response`. `follow-redirect` issues the
redirected request after a 3xx.

### Response assertions

```perl6
$session.assert-response(200);          # or a name: 'ok', 'not-found', ...
$session.assert-redirected-to('/login');
$session.assert-select('Welcome');      # substring or a Regex over the body
```

`assert-select` matches against the response body (a string substring or a
regex), with an optional `text =>` to require additional content.

## Live server

`IntegrationSession` drives the app in memory, which covers most tests. When a
test needs a real listening socket — driving the app from an external HTTP client
or a browser — `LiveServer` serves a built endpoint over the
[Cro adapter](/docs/MVC-Keayl/adapters) on an ephemeral port:

```perl6
use MVC::Keayl::TestSupport;

my $server = LiveServer.new(app => $application.endpoint).start;

my $url = $server.url('/dashboard');   # http://127.0.0.1:<port>/dashboard
# ... drive $url with any HTTP client or browser ...

$server.stop;
```

`new` picks a free localhost port; override it with `host`, `port`, or `scheme`.
`start` binds the socket and returns the server, `base-url` and `url($path)`
build addresses against it, and `stop` shuts it down. Each `LiveServer` defaults
to a distinct port, so several can run at once.

## Routing assertions

These check a router both ways:

```perl6
assert-recognizes($router, 'GET', '/widgets/5', matching => %( controller => 'widgets', action => 'show', id => '5' ));
assert-generates($router, 'widget', '/widgets/5', 5);
assert-routing($router, 'widget', 'GET', '/widgets/5', matching => %( action => 'show' ), 5);
```

## Controller and view introspection

`RecordingRenderer` stands in for the view renderer and records what was
rendered, so a controller test can dispatch an action and inspect the result:

```perl6
my $renderer   = RecordingRenderer.new;
my $controller = WidgetsController.new(view-renderer => $renderer);
$controller.dispatch('show');

assert-rendered($renderer, 'show');
assert-assigned($controller, 'widget', $widget);
```

## Component helpers

Mailer, job, and cable activity is asserted around a block:

```perl6
assert-emails(1, { UserMailer.new(delivery => $test-delivery).deliver('welcome', $user) });
delivered-emails();   # the collected test deliveries

assert-enqueued-jobs(2, $test-adapter, { ImportJob.perform-later($a); ImportJob.perform-later($b) });
perform-enqueued-jobs($test-adapter);

assert-broadcasts($pubsub, 'room:1', 2, { $pubsub.broadcast('room:1', 'a'); $pubsub.broadcast('room:1', 'b') });
assert-stream-subscribed($channel, 'room:1');
```

## With BDD::Behave

The same helpers read naturally in a behave spec by wrapping the assertion in a
block:

```perl6
describe 'the dashboard', {
  let(:session, { IntegrationSession.new(app => $app.endpoint) });

  it 'greets a signed-in user', {
    session.get('/dashboard');
    expect({ session.assert-select('Welcome') }).not.to.throw;
  }
}
```
