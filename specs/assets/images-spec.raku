use lib 'lib';
use lib 'specs/support';
use BDD::Behave;
use BloginTest;
use Blogin::Assets;

describe 'Blogin::Assets::variant-name', {
  it 'inserts the width before the extension', {
    expect(Blogin::Assets::variant-name('photo.png', 320)).to.eq('photo-320.png');
  }
}

describe 'Blogin::Assets::srcset-value', {
  it 'lists each variant and the original by width', {
    my @variants = %( width => 320, url => '/img/photo-320.png' ), %( width => 640, url => '/img/photo-640.png' );
    expect(Blogin::Assets::srcset-value('/img/photo.png', 800, @variants))
      .to.eq('/img/photo-320.png 320w, /img/photo-640.png 640w, /img/photo.png 800w');
  }
}

describe 'Blogin::Assets::add-srcset', {
  it 'injects a srcset next to the matching src', {
    my %srcsets = '/img/photo.png' => '/img/photo-320.png 320w, /img/photo.png 800w';
    my $out = Blogin::Assets::add-srcset('<img src="/img/photo.png" alt="x" />', %srcsets);
    expect($out).to.eq('<img src="/img/photo.png" srcset="/img/photo-320.png 320w, /img/photo.png 800w" alt="x" />');
  }
}

describe 'an image resizer is available in the test environment', {
  it 'finds ImageMagick or sips', {
    expect(Blogin::Assets::resizer().chars).to.be-truthy;
  }
}

describe 'responsive images through a build', {
  my $FIXTURE = 'specs/fixtures/images'.IO;

  let(:out, {
    my $dir = temp-dir('images');
    build-fixture($FIXTURE, $dir, image-widths => [320, 640, 1000]);
    $dir
  });

  after-each { nuke(out()) }

  it 'writes a resized variant for a width below the source width', {
    expect(out().add('img/photo-320.png').e).to.be-truthy;
  }

  it 'resizes the variant to the requested width', {
    expect(Blogin::Assets::image-width(out().add('img/photo-320.png'), Blogin::Assets::resizer())).to.eq(320);
  }

  it 'writes a variant for each width below the source', {
    expect(out().add('img/photo-640.png').e).to.be-truthy;
  }

  it 'skips a width at or above the source width', {
    expect(out().add('img/photo-1000.png').e).to.be-falsy;
  }

  it 'adds a srcset to the image reference', {
    expect(out().add('posts/hello.html').slurp.contains('srcset="/img/photo-320.png 320w, /img/photo-640.png 640w, /img/photo.png 800w"')).to.be-truthy;
  }

  it 'keeps the original src', {
    expect(out().add('posts/hello.html').slurp.contains('src="/img/photo.png"')).to.be-truthy;
  }
}

describe 'responsive images combined with fingerprinting', {
  my $FIXTURE = 'specs/fixtures/images'.IO;

  let(:out, {
    my $dir = temp-dir('images-fp');
    build-fixture($FIXTURE, $dir, image-widths => [320], fingerprint => True);
    $dir
  });

  after-each { nuke(out()) }

  it 'fingerprints the resized variant', {
    expect(out().add('img').dir.grep({ .basename ~~ / ^ 'photo-320.' <[0..9 a..f]>+ '.png' $ / }).elems).to.be-truthy;
  }

  it 'rewrites the srcset url to the fingerprinted variant', {
    my $html = out().add('posts/hello.html').slurp;
    expect($html.contains('photo-320.png 320w')).to.be-falsy;
  }
}
