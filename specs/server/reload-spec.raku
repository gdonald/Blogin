use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin::Server;
use Blogin::Site;

describe 'the reload client script', {
  it 'connects an EventSource to the reload endpoint', {
    expect(Blogin::Server::reload-script.contains('new EventSource("/__blogin-reload")')).to.be-truthy;
  }

  it 'reloads the page on a message', {
    expect(Blogin::Server::reload-script.contains('location.reload()')).to.be-truthy;
  }

  it 'closes the stream before the page unloads so connections do not pile up', {
    expect(Blogin::Server::reload-script.contains('pagehide') && Blogin::Server::reload-script.contains('source.close()')).to.be-truthy;
  }
}

describe 'the server-sent reload event', {
  it 'is a Blob so Cro can stream it', {
    expect(Blogin::Server::reload-event() ~~ Blob).to.be-truthy;
  }

  it 'is formatted as an SSE data line', {
    expect(Blogin::Server::reload-event().decode('utf-8')).to.eq("data: reload\n\n");
  }
}

describe 'injecting the reload client', {
  it 'inserts the script before the closing body tag', {
    my $out = Blogin::Server::inject-reload('<html><body>hi</body></html>', '<script>x</script>');
    expect($out).to.eq('<html><body>hi<script>x</script></body></html>');
  }

  it 'appends the script when there is no body tag', {
    expect(Blogin::Server::inject-reload('<p>hi</p>', '<script>x</script>')).to.eq('<p>hi</p><script>x</script>');
  }
}

describe 'the reload channel', {
  it 'delivers a notification to a subscriber', {
    my $channel = Blogin::Server::ReloadChannel.new;

    my @received;
    my $tap = $channel.Supply.tap({ @received.push($_) });

    $channel.notify;
    $tap.close;

    expect(@received.elems).to.eq(1);
  }
}

describe 'the served response', {
  my $BASIC = 'specs/fixtures/basic'.IO;

  let(:out, {
    my $dir = temp-dir('render-response');
    Blogin::Site::build(content => $BASIC.add('content'), out => $dir, layouts => $BASIC.add('layouts'), static => $BASIC.add('static'), home-section => 'posts');
    $dir
  });

  after-each { nuke(out()) }

  it 'injects the reload client into an html page when reload is on', {
    my %response = Blogin::Server::render-response('/posts/hello', out(), inject => True);
    expect(%response<body>.contains('EventSource("/__blogin-reload")')).to.be-truthy;
  }

  it 'does not inject the client when reload is off', {
    my %response = Blogin::Server::render-response('/posts/hello', out(), inject => False);
    expect(%response<body>.decode.contains('__blogin-reload')).to.be-falsy;
  }

  it 'serves a non-html asset without injection', {
    my %response = Blogin::Server::render-response('/style.css', out(), inject => True);
    expect(%response<body> ~~ Blob && !%response<body>.decode.contains('__blogin-reload')).to.be-truthy;
  }

  it 'returns a 404 for a missing path', {
    my %response = Blogin::Server::render-response('/nope', out(), inject => True);
    expect(%response<status>).to.eq(404);
  }
}

describe 'the server-sent event stream', {
  it 'emits one reload Blob per rebuild notification', {
    my $channel = Blogin::Server::ReloadChannel.new;

    my @events;
    my $tap = Blogin::Server::reload-events($channel).tap({ @events.push($_) });

    $channel.notify;
    $tap.close;

    expect(@events.elems == 1 && @events[0] ~~ Blob).to.be-truthy;
  }
}

describe 'a rebuild pushing a reload', {
  it 'runs the rebuild', {
    my $channel = Blogin::Server::ReloadChannel.new;

    my $ran = False;
    Blogin::Server::rebuild-and-reload($channel, { $ran = True });

    expect($ran).to.be-truthy;
  }

  it 'pushes a reload to a connected client after the rebuild', {
    my $channel = Blogin::Server::ReloadChannel.new;

    my @received;
    my $tap = $channel.Supply.tap({ @received.push($_) });

    Blogin::Server::rebuild-and-reload($channel, { True });
    $tap.close;

    expect(@received.elems).to.eq(1);
  }

  it 'does not push a reload when the rebuild fails', {
    my $channel = Blogin::Server::ReloadChannel.new;

    my @received;
    my $tap = $channel.Supply.tap({ @received.push($_) });

    try Blogin::Server::rebuild-and-reload($channel, { die 'boom' });
    $tap.close;

    expect(@received.elems).to.eq(0);
  }
}
