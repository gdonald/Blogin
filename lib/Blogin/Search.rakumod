use v6.d;

use JSON::Fast;

unit module Blogin::Search;

# Ranking weights, mirrored in the emitted search.js.
constant TITLE-WEIGHT = 10;
constant TAG-WEIGHT   = 5;
constant BODY-WEIGHT  = 1;

our sub tokenize(Str $text --> List) is export {
  $text.lc.comb(/ \w+ /).List;
}

sub word-count(Str $text, Str $token --> Int) {
  tokenize($text).grep(* eq $token).elems;
}

our sub rank(@records, Str $query, Int :$cap = 10 --> Array) is export {
  my @tokens = tokenize($query);
  return [] unless @tokens;

  my @scored;

  for @records -> %record {
    my $score = 0;

    for @tokens -> $token {
      $score += TITLE-WEIGHT * word-count(%record<title> // '', $token);
      $score += TAG-WEIGHT   * ((%record<tags> // []).map(*.lc).grep(* eq $token).elems);
      $score += BODY-WEIGHT  * word-count(%record<text> // '', $token);
    }

    @scored.push(%( record => %record, score => $score )) if $score > 0;
  }

  @scored .= sort({
    ($^b<score> <=> $^a<score>) || ($^a<record><title> leg $^b<record><title>)
  });

  @scored[^min($cap, @scored.elems)].map(*.<record>).Array;
}

our sub index-records(@pages, Int :$text-length = 2000 --> Array) is export {
  @pages.map(-> $page {
    my $post = $page<post>;
    my $text = $page<text> // '';

    %(
      title       => $post.title,
      url         => $page<url>,
      date        => $post.date-str,
      tags        => $post.tags,
      description => $post.description,
      text        => ($text.chars > $text-length ?? $text.substr(0, $text-length) !! $text),
    )
  }).Array;
}

our sub index-json(@pages, Int :$text-length = 2000 --> Str) is export {
  to-json(index-records(@pages, :$text-length), :sorted-keys);
}

my constant SEARCH-JS = q:to/JS/;
(function () {
  const WEIGHT = { title: 10, tag: 5, body: 1 };
  const CAP = (typeof BLOGIN_SEARCH_CAP === 'number') ? BLOGIN_SEARCH_CAP : 10;

  const form = document.querySelector('[data-blogin-search]');
  if (!form) return;

  const input = form.querySelector('input');
  const results = document.querySelector('[data-blogin-results]');

  let records = [];
  fetch('/search-index.json').then(function (r) { return r.json(); }).then(function (data) {
    records = data;
  });

  function tokenize(text) {
    return (text.toLowerCase().match(/\w+/g) || []);
  }

  function wordCount(text, token) {
    return tokenize(text).filter(function (w) { return w === token; }).length;
  }

  function rank(query) {
    const tokens = tokenize(query);
    if (!tokens.length) return [];

    const scored = [];
    for (const rec of records) {
      let score = 0;
      for (const t of tokens) {
        score += WEIGHT.title * wordCount(rec.title || '', t);
        score += WEIGHT.tag * (rec.tags || []).map(function (x) { return x.toLowerCase(); })
          .filter(function (x) { return x === t; }).length;
        score += WEIGHT.body * wordCount(rec.text || '', t);
      }
      if (score > 0) scored.push({ rec: rec, score: score });
    }

    scored.sort(function (a, b) {
      return (b.score - a.score) || a.rec.title.localeCompare(b.rec.title);
    });

    return scored.slice(0, CAP).map(function (s) { return s.rec; });
  }

  function snippet(text, tokens) {
    const lower = text.toLowerCase();
    for (const t of tokens) {
      const i = lower.indexOf(t);
      if (i >= 0) {
        const start = Math.max(0, i - 30);
        return (start > 0 ? '…' : '') + text.slice(start, i + 60) + '…';
      }
    }
    return text.slice(0, 90);
  }

  function render(query) {
    results.innerHTML = '';
    const tokens = tokenize(query);
    for (const rec of rank(query)) {
      const li = document.createElement('li');

      const a = document.createElement('a');
      a.href = rec.url;
      a.textContent = rec.title;
      li.appendChild(a);

      const p = document.createElement('p');
      p.textContent = snippet(rec.text || '', tokens);
      li.appendChild(p);

      results.appendChild(li);
    }
  }

  input.addEventListener('input', function () { render(input.value); });
})();
JS

our sub search-js(Int :$cap = 10 --> Str) is export {
  "const BLOGIN_SEARCH_CAP = $cap;\n" ~ SEARCH-JS;
}
