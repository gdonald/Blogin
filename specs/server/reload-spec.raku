use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use Blogin::Server;

describe 'the reload client script', {
  it 'connects an EventSource to the reload endpoint', {
    expect(Blogin::Server::reload-script.contains('new EventSource("/__blogin-reload")')).to.be-truthy;
  }

  it 'reloads the page on a message', {
    expect(Blogin::Server::reload-script.contains('location.reload()')).to.be-truthy;
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
