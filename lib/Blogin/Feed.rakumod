use v6.d;

unit module Blogin::Feed;

sub xml-escape(Str $text --> Str) {
  $text.trans(
    [ '&', '<', '>', '"', "'" ] =>
    [ '&amp;', '&lt;', '&gt;', '&quot;', '&apos;' ],
  );
}

sub atom-time(Str $date --> Str) {
  $date.chars ?? "{ $date }T00:00:00Z" !! '1970-01-01T00:00:00Z';
}

our sub atom(
  Str  :$title!,
  Str  :$site-url = '',
  Str  :$feed-url = '',
  Str  :$updated = '',
       :@entries,
  --> Str
) is export {
  my @lines;

  @lines.push('<?xml version="1.0" encoding="utf-8"?>');
  @lines.push('<feed xmlns="http://www.w3.org/2005/Atom">');
  @lines.push("  <title>{ xml-escape($title) }</title>");
  @lines.push("  <id>{ xml-escape($site-url) }</id>");
  @lines.push("  <link href=\"{ xml-escape($site-url) }\"/>");
  @lines.push("  <link href=\"{ xml-escape($feed-url) }\" rel=\"self\"/>");
  @lines.push("  <updated>{ atom-time($updated) }</updated>");

  for @entries -> $entry {
    @lines.push('  <entry>');
    @lines.push("    <title>{ xml-escape($entry<title>) }</title>");
    @lines.push("    <id>{ xml-escape($entry<url>) }</id>");
    @lines.push("    <link href=\"{ xml-escape($entry<url>) }\"/>");
    @lines.push("    <updated>{ atom-time($entry<date>) }</updated>");

    with $entry<summary> -> $summary {
      @lines.push("    <summary>{ xml-escape($summary) }</summary>") if $summary.chars;
    }

    @lines.push('  </entry>');
  }

  @lines.push('</feed>');

  @lines.join("\n") ~ "\n";
}

our sub sitemap(:@locs --> Str) is export {
  my @lines;

  @lines.push('<?xml version="1.0" encoding="utf-8"?>');
  @lines.push('<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">');

  for @locs -> $loc {
    @lines.push("  <url><loc>{ xml-escape($loc) }</loc></url>");
  }

  @lines.push('</urlset>');

  @lines.join("\n") ~ "\n";
}
