use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin::Server;
use Blogin::Site;

describe 'the reload client script', {
  it 'polls the reload endpoint', {
    expect(Blogin::Server::reload-script.contains('fetch("/__blogin-reload")')).to.be-truthy;
  }

  it 'reloads the page when the version changes', {
    expect(Blogin::Server::reload-script.contains('location.reload()')).to.be-truthy;
  }

  it 'ignores fetch errors so a paused server does not throw', {
    expect(Blogin::Server::reload-script.contains('.catch(')).to.be-truthy;
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

describe 'the reload channel version', {
  it 'starts at zero', {
    expect(Blogin::Server::ReloadChannel.new.version).to.eq(0);
  }

  it 'bumps the version on notify', {
    my $channel = Blogin::Server::ReloadChannel.new;
    $channel.notify;
    expect($channel.version).to.eq(1);
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
    expect(%response<body>.contains('fetch("/__blogin-reload")')).to.be-truthy;
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

describe 'a rebuild bumping the reload version', {
  it 'runs the rebuild', {
    my $channel = Blogin::Server::ReloadChannel.new;

    my $ran = False;
    Blogin::Server::rebuild-and-reload($channel, { $ran = True });

    expect($ran).to.be-truthy;
  }

  it 'bumps the version so polling clients reload', {
    my $channel = Blogin::Server::ReloadChannel.new;

    Blogin::Server::rebuild-and-reload($channel, { True });

    expect($channel.version).to.eq(1);
  }

  it 'does not bump the version when the rebuild fails', {
    my $channel = Blogin::Server::ReloadChannel.new;

    try Blogin::Server::rebuild-and-reload($channel, { die 'boom' });

    expect($channel.version).to.eq(0);
  }
}
