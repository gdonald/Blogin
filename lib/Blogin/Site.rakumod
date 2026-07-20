use v6.d;

use Blogin::Post;
use Blogin::Layout;

unit module Blogin::Site;

class BuildResult {
  has @.written;
  has @.rendered;
  has @.listings;
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

sub date-key(%page) {
  %page<post>.date.defined ?? %page<post>.date.daycount !! 0;
}

sub listing-url(Str $section, Int $page-num, Bool :$at-root, Bool :$clean-urls) {
  my $base = $at-root ?? '' !! $section;

  my $path = $page-num == 1
    ?? $base
    !! ($base.chars ?? "$base/page/$page-num" !! "page/$page-num");

  return '/' unless $path.chars;

  $clean-urls ?? "/$path" !! "/$path/";
}

sub listing-file(IO::Path:D $out, Str $section, Int $page-num, Bool :$at-root, Bool :$clean-urls) {
  my $base = $at-root ?? '' !! $section;

  if $page-num == 1 {
    return $out.add('index.html') unless $base.chars;
    return $clean-urls ?? $out.add("$base.html") !! $out.add($base).add('index.html');
  }

  my $rel = $base.chars ?? "$base/page/$page-num" !! "page/$page-num";

  $clean-urls ?? $out.add("$rel.html") !! $out.add($rel).add('index.html');
}

sub write-section-listing(
  Str $section, @sorted,
  Bool :$at-root = False,
  IO::Path:D :$out!, IO() :$layouts!, :%site, Bool :$clean-urls!,
  Bool :$debug!, Int :$page-size!,
) {
  my @written;

  my @chunks = @sorted.rotor($page-size, :partial);
  @chunks = ([],) unless @chunks;
  my $total = @chunks.elems;

  for @chunks.kv -> $index, @chunk {
    my $page-num = $index + 1;

    my $out-file = listing-file($out, $section, $page-num, :$at-root, :$clean-urls);
    my $prev-url = $page-num > 1     ?? listing-url($section, $page-num - 1, :$at-root, :$clean-urls) !! '';
    my $next-url = $page-num < $total ?? listing-url($section, $page-num + 1, :$at-root, :$clean-urls) !! '';

    my @entries = @chunk.map({
      %( title => .<post>.title, url => .<url>, date => (.<post>.date.defined ?? .<post>.date.Str !! '') )
    });

    my $html = Blogin::Layout::render-listing(
      :$layouts, :$section, :%site, :@entries,
      page-number => $page-num, total-pages => $total,
      :$prev-url, :$next-url, :$debug,
    );

    $out-file.parent.mkdir;
    $out-file.spurt($html);
    @written.push($out-file);
  }

  @written;
}

sub build-listings(
  :@pages, IO::Path:D :$out!, IO() :$layouts!, :%site,
  Bool :$clean-urls!, Bool :$debug!, Int :$page-size!, Str :$home-section!,
  --> Array
) {
  my %by-section;
  %by-section{ .<section> }.push($_) for @pages;

  my @written;

  my sub sorted-of(Str $section) {
    %by-section{$section}.sort({
      (date-key($^b) <=> date-key($^a)) || ($^a<post>.slug leg $^b<post>.slug)
    });
  }

  for %by-section.keys.grep(*.chars).sort -> $section {
    @written.append: write-section-listing(
      $section, sorted-of($section),
      :$out, :$layouts, :%site, :$clean-urls, :$debug, :$page-size,
    );
  }

  if $home-section.chars && (%by-section{$home-section}:exists) {
    @written.append: write-section-listing(
      $home-section, sorted-of($home-section),
      :at-root, :$out, :$layouts, :%site, :$clean-urls, :$debug, :$page-size,
    );
  }

  @written;
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
  Bool  :$debug = False,
  Int   :$page-size = 10,
  Str   :$home-section = '',
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
        debug     => $debug,
      );
    });

    $page<out-file>.spurt($html);
    $page<out-file>;
  }).List;

  my @listings = build-listings(
    :@pages, :$out, :$layouts, :%site, :$clean-urls, :$debug,
    :$page-size, :$home-section,
  );

  copy-tree($static, $out) if $static.d;

  BuildResult.new(written => @written, rendered => @pages, listings => @listings);
}
