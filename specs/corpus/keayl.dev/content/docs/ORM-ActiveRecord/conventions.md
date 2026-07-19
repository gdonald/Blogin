---
title: Conventions
slug: conventions
order: 2
description: ORM::ActiveRecord follows a small set of naming conventions. Once you know the two rules Raku imposes on method names, the rest of the API reads the way...
toc: true
---
ORM::ActiveRecord follows a small set of naming conventions. Once you know the
two rules Raku imposes on method names, the rest of the API reads the way you
would expect. The tables below are a quick reference from a plain-English
operation to the method that performs it.

## The two naming rules

Raku method names cannot end in `!` or `?`, and the ecosystem prefers hyphens to
underscores:

- A bang method drops the `!` and gains a `-bang` suffix: `save-bang`.
- A predicate drops the `?` and gains an `is-` prefix: `is-valid`,
  `is-persisted`, `is-new-record`.
- Everything else swaps `_` for `-`: `find-by`, `has-many`, `update-all`.

That is most of the translation. The tables below cover the rest.

## Models and tables

| Operation | ORM::ActiveRecord |
| --------- | ----------------- |
| Declare a model | `class User is Model { }` |
| Override the table name | `method table-name { 'people' }` |
| Belongs-to association | `self.belongs-to: author => class-name => 'Author'` |
| Has-many association | `self.has-many: posts => class-name => 'Post'` |
| Has-one association | `self.has-one: profile => class-name => 'Profile'` |
| Many-to-many association | `self.has-and-belongs-to-many: tags => class-name => 'Tag'` |
| Has-many through | `self.has-many: magazines => %(through => :subscriptions, ...)` |

Associations and validations are declared in `submethod BUILD`. See
[Models](/docs/ORM-ActiveRecord/models) and the [association cookbook](/docs/ORM-ActiveRecord/through-associations).

## Creating and updating

| Operation | ORM::ActiveRecord |
| --------- | ----------------- |
| Create a record | `User.create({ ... })` |
| Create or raise | `User.create-bang({ ... })` |
| Save | `user.save` / `user.save-bang` |
| Update attributes | `user.update({ ... })` / `user.update-bang({ ... })` |
| Destroy | `user.destroy` / `User.destroy-all` |
| Check persistence | `user.is-persisted` / `user.is-new-record` |
| Reload from the database | `user.reload` |
| Touch timestamps | `user.touch` |

See [Persistence](/docs/ORM-ActiveRecord/persistence).

## Querying

| Operation | ORM::ActiveRecord |
| --------- | ----------------- |
| Find by primary key | `User.find(id)` |
| Find by attributes | `User.find-by({ name => 'x' })` |
| Find by attributes or raise | `User.find-by-bang({ ... })` |
| Filter | `User.where({ active => True })` |
| Negated filter | `.not({ ... })` |
| Order | `.order('name')` |
| Limit and offset | `.limit(10).offset(20)` |
| First and last | `.first` / `.last` |
| Count and sum | `.count` / `.sum('col')` |
| Pluck a column | `.pluck('name')` |
| Existence check | `.exists` |
| Bulk update or delete | `.update-all(...)` / `.delete-all` |

A relation is lazy. It runs when you call `.all` / `.perform`, iterate it, or
ask for a count. See [Finders](/docs/ORM-ActiveRecord/finders) and [Queries](/docs/ORM-ActiveRecord/queries).

## Eager loading

| Operation | ORM::ActiveRecord |
| --------- | ----------------- |
| Preload (separate queries) | `.preload(:posts)` |
| Includes (loader chosen for you) | `.includes(:posts)` |
| Eager-load (single JOIN) | `.eager-load(:posts)` |
| Force a JOIN reference | `.references(:posts)` |
| Nested loading | `.preload(posts => :comments)` |

See the [eager-loading cookbook](/docs/ORM-ActiveRecord/eager-loading).

## Validations

| Operation | ORM::ActiveRecord |
| --------- | ----------------- |
| Presence | `self.validate: 'name', { :presence }` |
| Numericality | `self.validate: 'age', { numericality => { ... } }` |
| Validate with a class | `self.validates-with(MyValidator.new)` |
| Validate each attribute | `self.validates-each: <a b>, -> $rec, $attr, $value { ... }` |
| Validity check | `record.is-valid` / `record.is-invalid` |
| Read error messages | `record.errors.full-messages` |

See [Validations](/docs/ORM-ActiveRecord/data) and the
[custom-validator cookbook](/docs/ORM-ActiveRecord/custom-validators).

## Callbacks

| Operation | ORM::ActiveRecord |
| --------- | ----------------- |
| Before save | `self.before-save: -> { ... }` or `self.before-save: 'method-name'` |
| After create / update | `self.after-create`, `self.after-update`, ... |
| After commit / rollback | `self.after-commit`, `self.after-rollback` |

See [Callbacks](/docs/ORM-ActiveRecord/callbacks).

## Transactions

| Operation | ORM::ActiveRecord |
| --------- | ----------------- |
| Connection transaction | `DB.shared.transaction({ })` |
| Model transaction | `Model.transaction({ })` |
| Nested savepoint | `:requires-new` |
| Roll back | `die X::Rollback.new` |
| Isolation level | `:isolation<read_committed>` |

See the [transactions cookbook](/docs/ORM-ActiveRecord/transactions-and-savepoints).

## Higher-level features

| Operation | ORM::ActiveRecord |
| --------- | ----------------- |
| Enum attribute | `self.enum: 'status', { ... }` ([Enums](/docs/ORM-ActiveRecord/enums)) |
| Serialize a column | `self.serialize('prefs', JsonCoder.new)` ([Attribute types](/docs/ORM-ActiveRecord/types)) |
| Normalize a value | `self.normalizes('email', :with(-> $v { ... }))` ([Normalisation](/docs/ORM-ActiveRecord/normalization)) |
| Encrypt a column | `self.encrypts('ssn')` ([Encryption](/docs/ORM-ActiveRecord/encryption)) |
| Secure password | `self.has-secure-password` ([Tokens & secure data](/docs/ORM-ActiveRecord/secure-tokens)) |
| Single-table inheritance | `type` column ([Single-table inheritance](/docs/ORM-ActiveRecord/inheritance)) |
| Soft deletes | [Soft deletes](/docs/ORM-ActiveRecord/discard) |

## Tooling

| Operation | ORM::ActiveRecord |
| --------- | ----------------- |
| Run migrations | `active-record db:migrate` |
| Roll back | `active-record db:rollback` |
| Seed data | `active-record db:seed` |
| Generate a model | `active-record generate model User name:string` |
| Console and runner | `active-record console` / `active-record runner` |
| Database console | `active-record dbconsole` |

See [Database tasks](/docs/ORM-ActiveRecord/db-tasks), [Generators](/docs/ORM-ActiveRecord/generators), and
[Runtime tasks](/docs/ORM-ActiveRecord/runtime).

## Things to keep in mind

- Hashes are passed explicitly: `User.where({ active => True })`, not bareword
  keyword arguments.
- `True` / `False` / `Nil` are the boolean and null literals.
- A model class binds itself into `GLOBAL` (`GLOBAL::<User> := User;`) so that
  associations resolve by class name. The generators emit this for you.
- There is no autoloading: `use` the modules and models you reference.
