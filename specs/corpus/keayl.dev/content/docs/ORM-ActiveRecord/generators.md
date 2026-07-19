---
title: Generators
slug: generators
order: 5
description: active-record generate scaffolds the files you would otherwise hand-write, and active-record destroy removes what a matching generate produced. Both take...
toc: true
---
`active-record generate` scaffolds the files you would otherwise hand-write, and
`active-record destroy` removes what a matching `generate` produced. Both take a type
(`model`, `migration`, `scope`, `validator`) followed by a name and optional
field specs. `g` and `d` are short aliases.

Generated files land under the current project:

- models in `app/models/`
- validators in `app/validators/`
- migrations in `db/migrate/`, with a timestamped filename so they sort and run
  after the existing ones.

## Field specs

Fields are `name:type` tokens. The type defaults to `string` when omitted, and
maps to a migration column adverb (`string`, `integer`, `bigint`, `boolean`,
`decimal`, `datetime`, `text`, `uuid`, `json`, ...). Two extras:

- `name:references` (or `belongs_to`) adds a reference column and, in a model, a
  `belongs-to`.
- a trailing `:uniq` makes the column unique; a trailing `:index` adds an index.

```
title:string  price:decimal  author:references  email:string:uniq  slug:string:index
```

## active-record generate model

```
active-record generate model Article title:string body:text author:references
```

Writes `app/models/Article.rakumod`:

```perl6
use ORM::ActiveRecord::Model;

class Article is Model {
  submethod BUILD {
    self.belongs-to: author => class-name => 'Author';
  }
}

GLOBAL::<Article> := Article;
```

and a create-table migration `db/migrate/<timestamp>-create-articles.raku`:

```perl6
use ORM::ActiveRecord::Schema::Migration;

class CreateArticles is Migration {
  method up {
    self.create-table: 'articles', [
      title  => { :string },
      body   => { :text },
      author => { :reference },
    ];
  }

  method down {
    self.drop-table: 'articles';
  }
}
```

The table name follows the ORM's own rule (the class name snake-cased, then
plus `s`, so `PageTag` becomes `page_tags`), so the model and its table always
agree.

## active-record generate migration

The migration body is inferred from the name:

```
active-record generate migration CreateProducts name:string price:decimal
active-record generate migration AddStockToProducts stock:integer
active-record generate migration RemovePriceFromProducts price:decimal
active-record generate migration BackfillProducts
```

- `Create<Things>` builds a `create-table` with the fields and a `drop-table`
  down.
- `Add<...>To<Table>` adds the fields, and removes them on the way down.
- `Remove<...>From<Table>` removes the fields, and restores them on the way
  down.
- anything else gets an empty `up` / `down` stub to fill in.

## active-record generate scope

Inserts a named scope into an existing model file:

```
active-record generate scope Article published published:True
```

adds to `app/models/Article.rakumod`:

```perl6
  submethod BUILD {
    self.scope: 'published', -> { self.where({ published => True }) };
    ...
  }
```

Field tokens become the `where` conditions. A numeric or `True` / `False` value
is emitted bare; anything else is quoted.

## active-record generate validator

```
active-record generate validator NotEvil
```

Writes `app/validators/NotEvilValidator.rakumod` with a `validate($record)`
skeleton, ready to use with `self.validates-with(NotEvilValidator)`. `Validator`
is appended to the name when it is not already there.

## active-record destroy

`active-record destroy` is the inverse:

```
active-record destroy model Article          # remove the model and its create migration
active-record destroy migration AddStockToProducts
active-record destroy scope Article published
active-record destroy validator NotEvil
```

Migrations are matched by the kebab-cased name regardless of their timestamp
prefix, so `active-record destroy migration CreateProducts` removes
`<timestamp>-create-products.raku`.
