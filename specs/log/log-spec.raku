use lib 'lib';
use BDD::Behave;
use Blogin::Log;

my $seq = 0;

describe 'Blogin::Log', {
  let(:level,       { 'normal' });
  let(:out-path,    { $*TMPDIR.add("blogin-log-out-{$*PID}-{$seq++}.txt") });
  let(:err-path,    { $*TMPDIR.add("blogin-log-err-{$*PID}-{$seq++}.txt") });
  let(:handle,      { out-path().open(:w) });
  let(:err-handle,  { err-path().open(:w) });
  let(:log,         { Blogin::Log.new(:level(level()), :out(handle()), :err(err-handle())) });

  after-each {
    for (out-path(), err-path()) -> $path {
      $path.unlink if $path.e;
    }
  }

  it 'writes an info message to out', {
    log().info('hello');
    handle().close;

    expect(out-path().slurp).to.eq("hello\n");
  }

  it 'writes a warning to err', {
    log().warn('careful');
    err-handle().close;

    expect(err-path().slurp).to.eq("careful\n");
  }

  it 'writes an error to err', {
    log().error('boom');
    err-handle().close;

    expect(err-path().slurp).to.eq("boom\n");
  }

  context 'at normal level', {
    it 'suppresses verbose output', {
      log().verbose('nope');
      handle().close;

      expect(out-path().slurp).to.eq('');
    }
  }

  context 'at verbose level', {
    let(:level, { 'verbose' });

    it 'writes verbose output', {
      log().verbose('deep');
      handle().close;

      expect(out-path().slurp).to.eq("deep\n");
    }
  }

  context 'at quiet level', {
    let(:level, { 'quiet' });

    it 'suppresses info output', {
      log().info('nope');
      handle().close;

      expect(out-path().slurp).to.eq('');
    }

    it 'suppresses warnings', {
      log().warn('nope');
      err-handle().close;

      expect(err-path().slurp).to.eq('');
    }

    it 'still writes errors', {
      log().error('boom');
      err-handle().close;

      expect(err-path().slurp).to.eq("boom\n");
    }
  }

  context 'with an unknown level', {
    it 'rejects construction', {
      expect({ Blogin::Log.new(:level('bogus')) }).to.throw;
    }
  }
}
