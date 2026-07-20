use v6.d;

use Blogin::Post;
use Blogin::Layout;

unit module Blogin::Site;

class BuildResult {
  has @.written;
  has @.rendered;
}

sub remove-tree(IO::Path:D $dir) {
  return unless $dir.e;

  for $dir.dir -> $entry {
    $entry.d ?? remove-tree($entry) !! $entry.unlink;
  }

  $dir.rmdir;
}

sub copy-tree(IO::Path:D $from, IO::Path:D $to) {
  for $from.dir.sort(*.basename) -> $entry {
    my $dest = $to.add($entry.basename);

    if $entry.d {
      $dest.mkdir;
      copy-tree($entry, $dest);
    }
    else {
      $entry.copy($dest);
    }
  }
}

sub find-markdown(IO::Path:D $dir) {
  return () unless $dir.d;

  gather for $dir.dir.sort(*.basename) -> $entry {
    if $entry.d {
      .take for find-markdown($entry);
    }
    elsif $entry.extension eq 'md' {
      take $entry;
    }
  }
}

sub section-of(IO::Path:D $file, IO::Path:D $content --> Str) {
  my $section = $file.parent.relative($content);

  $section eq '.' ?? '' !! $section;
}

our sub build(
  IO()  :$content!,
  IO()  :$out!,
  IO()  :$layouts = $content.parent.add('layouts'),
  IO()  :$static  = $content.parent.add('static'),
        :%site = %(),
  Bool  :$drafts = False,
  Int   :$jobs = ($*KERNEL.cpu-cores // 1),
  Bool  :$clean-urls = True,
  Str   :$framework = 'none',
  --> BuildResult
) {
  remove-tree($out);
  $out.mkdir;

  my @pages;
  my %seen;

  for find-markdown($content) -> $file {
    my $post    = Blogin::Post.load($file);
    my $section = section-of($file, $content);

    next if $post.draft && !$drafts;

    my $url-path = $section.chars ?? "$section/{ $post.slug }" !! $post.slug;
    my $url      = $clean-urls ?? "/$url-path" !! "/$url-path/";
    my $out-file = $clean-urls
      ?? $out.add("$url-path.html")
      !! $out.add($url-path).add('index.html');

    my $key = $out-file.absolute;

    if %seen{$key}:exists {
      die "two posts resolve to the same URL '$url': "
        ~ "{ %seen{$key} } and { $file }";
    }

    %seen{$key} = $file.Str;

    @pages.push(%(
      post     => $post,
      section  => $section,
      url      => $url,
      out-file => $out-file,
    ));
  }

  .<out-file>.parent.mkdir for @pages;

  my @ordered = @pages.sort({ -.<post>.body.chars });

  my $haml-lock = Lock.new;

  my @written = @ordered.hyper(:degree($jobs max 1), :batch(1)).map(-> $page {
    my $body-html = Blogin::Layout::render-body(
      post      => $page<post>,
      framework => $framework,
    );

    my $html = $haml-lock.protect({
      Blogin::Layout::render-with-layout(
        post      => $page<post>,
        body-html => $body-html,
        layouts   => $layouts,
        site      => %site,
        section   => $page<section>,
        url       => $page<url>,
      );
    });

    $page<out-file>.spurt($html);
    $page<out-file>;
  }).List;

  copy-tree($static, $out) if $static.d;

  BuildResult.new(written => @written, rendered => @pages);
}
