use v6.d;

use Blogin::Markdown::Node;
use Blogin::Framework;
use Blogin::Slug;
use Blogin::Highlight;
use Blogin::Shortcode;

sub html-escape(Str $text --> Str) {
  $text.trans([ '&', '<', '>' ] => [ '&amp;', '&lt;', '&gt;' ]);
}

sub attr-escape(Str $text --> Str) {
  $text.trans([ '&', '<', '>', '"' ] => [ '&amp;', '&lt;', '&gt;', '&quot;' ]);
}

class Blogin::Markdown::Html::Result {
  has Str $.html;
  has Str $.text;
  has     @.headings;
}

class Blogin::Markdown::Html {
  has Profile $.framework = Blogin::Framework::profile('none');
  has Bool $.highlight = False;
  has      %.shortcodes;
  has Str $!html = '';
  has Str $!text = '';
  has     @!headings;

  method render(Document $doc --> Blogin::Markdown::Html::Result) {
    $!html = '';
    $!text = '';
    @!headings = ();

    self!blocks($doc.children);

    Blogin::Markdown::Html::Result.new(html => $!html, text => $!text.trim, headings => @!headings);
  }

  method !class-attr(Str $class --> Str) {
    $class.chars ?? " class=\"{ attr-escape($class) }\"" !! '';
  }

  method !align-style($align --> Str) {
    $align.defined ?? " style=\"text-align:{ $align }\"" !! '';
  }

  method !attrs-str(%attrs, Str :$extra-class = '' --> Str) {
    my @classes;
    @classes.push($extra-class) if $extra-class.chars;
    @classes.push(%attrs<class>) if %attrs<class>:exists && %attrs<class>.chars;

    my $out = '';
    $out ~= " class=\"{ attr-escape(@classes.join(' ')) }\"" if @classes;

    for %attrs.sort(*.key) -> $pair {
      next if $pair.key eq 'class';
      $out ~= " { $pair.key }=\"{ attr-escape($pair.value.Str) }\"";
    }

    $out;
  }

  method !node-text(@nodes --> Str) {
    my $out = '';

    for @nodes -> $node {
      given $node {
        when Text     { $out ~= $node.text; }
        when CodeSpan { $out ~= $node.text; }
        when Image    { $out ~= $node.alt; }
        when Emphasis | Strong | Strikethrough | Link {
          $out ~= self!node-text($node.children);
        }
        default {}
      }
    }

    $out;
  }

  method !blocks(@blocks) {
    self!block($_) for @blocks;
  }

  method !block($node) {
    given $node {
      when Paragraph {
        $!html ~= '<p>';
        self!inline($node.children);
        $!html ~= "</p>\n";
        $!text ~= "\n";
      }

      when Heading {
        my $text  = self!node-text($node.children);
        my $slug  = Blogin::Slug::slugify($text);
        my $class = $.framework.class-for('heading');

        @!headings.push(%( level => $node.level, text => $text, id => $slug ));

        $!html ~= "<h{ $node.level } id=\"{ attr-escape($slug) }\"{ self!class-attr($class) }>";
        self!inline($node.children);
        $!html ~= "<a class=\"anchor\" href=\"#{ attr-escape($slug) }\">#</a>";
        $!html ~= "</h{ $node.level }>\n";
        $!text ~= "\n";
      }

      when ThematicBreak {
        $!html ~= "<hr />\n";
      }

      when Shortcode {
        $!html ~= do if %!shortcodes{$node.name}:exists {
          Blogin::Shortcode::render-template(%!shortcodes{$node.name}, $node.args);
        }
        elsif Blogin::Shortcode::known($node.name) {
          Blogin::Shortcode::expand($node.name, $node.args);
        }
        else {
          html-escape($node.raw);
        }
        $!html ~= "\n";
      }

      when Footnotes {
        $!html ~= "<section class=\"footnotes\">\n<ol>\n";

        for $node.items -> $item {
          my $id = attr-escape($item.label);

          $!html ~= "<li id=\"fn-$id\">";
          self!inline($item.children);
          $!html ~= " <a href=\"#fnref-$id\" class=\"footnote-back\">&#8617;</a>";
          $!html ~= "</li>\n";
        }

        $!html ~= "</ol>\n</section>\n";
      }

      when CodeBlock {
        my $language = $node.info.chars ?? "language-{ $node.info }" !! '';
        my $extra    = $.framework.class-for('code-block');
        my $plain    = $.highlight && $node.info.chars && !Blogin::Highlight::supports($node.info) ?? 'hl-plain' !! '';
        my $class    = ($language, $plain, $extra).grep(*.chars).join(' ');

        $!html ~= '<pre><code' ~ self!class-attr($class) ~ '>';
        $!html ~= $.highlight
          ?? Blogin::Highlight::highlight($node.text, $node.info)
          !! html-escape($node.text);
        $!html ~= "</code></pre>\n";
        $!text ~= $node.text ~ "\n";
      }

      when BlockQuote {
        my $class = $.framework.class-for('blockquote');

        $!html ~= '<blockquote' ~ self!class-attr($class) ~ ">\n";
        self!blocks($node.children);
        $!html ~= "</blockquote>\n";
      }

      when List {
        self!list($node);
      }

      when Table {
        self!table($node);
      }

      when DefinitionList {
        self!definition-list($node);
      }

      default {}
    }
  }

  method !definition-list(DefinitionList $node) {
    my $class = $.framework.class-for('definition-list');

    $!html ~= '<dl' ~ self!class-attr($class) ~ ">\n";

    for $node.items -> $item {
      $!html ~= '<dt>';
      self!inline($item.term);
      $!html ~= "</dt>\n";
      $!text ~= "\n";

      for $item.definitions -> $definition {
        $!html ~= '<dd>';
        self!inline($definition);
        $!html ~= "</dd>\n";
        $!text ~= "\n";
      }
    }

    $!html ~= "</dl>\n";
  }

  method !list(List $node) {
    my $class = $.framework.class-for('list');

    if $node.ordered {
      my $start = $node.start != 1 ?? " start=\"{ $node.start }\"" !! '';
      $!html ~= '<ol' ~ $start ~ self!class-attr($class) ~ ">\n";
    }
    else {
      $!html ~= '<ul' ~ self!class-attr($class) ~ ">\n";
    }

    self!list-item($_) for $node.items;

    $!html ~= $node.ordered ?? "</ol>\n" !! "</ul>\n";
  }

  method !list-item(ListItem $item) {
    $!html ~= '<li>';

    if $item.task {
      my $checked = $item.checked ?? ' checked' !! '';
      $!html ~= "<input type=\"checkbox\" disabled$checked /> ";
    }

    if $item.children.elems == 1 && $item.children[0] ~~ Paragraph {
      self!inline($item.children[0].children);
      $!text ~= "\n";
    }
    else {
      self!blocks($item.children);
    }

    $!html ~= "</li>\n";
  }

  method !table(Table $node) {
    my $class = $.framework.class-for('table');

    $!html ~= '<table' ~ self!class-attr($class) ~ ">\n<thead>\n<tr>\n";

    for $node.header.kv -> $index, $cell {
      $!html ~= '<th' ~ self!align-style($node.aligns[$index]) ~ '>';
      self!inline($cell);
      $!html ~= "</th>\n";
    }

    $!html ~= "</tr>\n</thead>\n<tbody>\n";

    for $node.rows -> $row {
      $!html ~= "<tr>\n";

      for $row.kv -> $index, $cell {
        $!html ~= '<td' ~ self!align-style($node.aligns[$index]) ~ '>';
        self!inline($cell);
        $!html ~= "</td>\n";
      }

      $!html ~= "</tr>\n";
    }

    $!html ~= "</tbody>\n</table>\n";
    $!text ~= "\n";
  }

  method !inline(@nodes) {
    self!inline-node($_) for @nodes;
  }

  method !inline-node($node) {
    given $node {
      when Text {
        $!html ~= html-escape($node.text);
        $!text ~= $node.text;
      }

      when Emphasis {
        $!html ~= '<em>';
        self!inline($node.children);
        $!html ~= '</em>';
      }

      when Strong {
        $!html ~= '<strong>';
        self!inline($node.children);
        $!html ~= '</strong>';
      }

      when Strikethrough {
        $!html ~= '<del>';
        self!inline($node.children);
        $!html ~= '</del>';
      }

      when CodeSpan {
        $!html ~= '<code>' ~ html-escape($node.text) ~ '</code>';
        $!text ~= $node.text;
      }

      when Link {
        $!html ~= '<a href="' ~ attr-escape($node.url) ~ '"';
        $!html ~= ' title="' ~ attr-escape($node.title) ~ '"' if $node.title.chars;
        $!html ~= self!attrs-str($node.attrs);
        $!html ~= '>';
        self!inline($node.children);
        $!html ~= '</a>';
      }

      when Image {
        my $extra = $.framework.class-for('image');

        $!html ~= '<img src="' ~ attr-escape($node.url) ~ '" alt="' ~ attr-escape($node.alt) ~ '"';
        $!html ~= ' title="' ~ attr-escape($node.title) ~ '"' if $node.title.chars;
        $!html ~= self!attrs-str($node.attrs, :extra-class($extra));
        $!html ~= ' />';
        $!text ~= $node.alt;
      }

      when FootnoteRef {
        if $node.number > 0 {
          my $id     = attr-escape($node.label);
          my $ref-id = $node.occurrence == 1 ?? "fnref-$id" !! "fnref-$id-{ $node.occurrence }";
          $!html ~= "<sup class=\"footnote-ref\"><a href=\"#fn-$id\" id=\"$ref-id\">{ $node.number }</a></sup>";
        }
        else {
          $!html ~= html-escape("[^{ $node.label }]");
          $!text ~= "[^{ $node.label }]";
        }
      }

      when SoftBreak {
        $!html ~= "\n";
        $!text ~= ' ';
      }

      when LineBreak {
        $!html ~= "<br />\n";
        $!text ~= ' ';
      }

      default {}
    }
  }
}
