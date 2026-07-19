---
title: Forms
slug: forms
order: 11
description: New and edit forms are built from declared fields. Each field maps by :as to an input type:
toc: true
---
New and edit forms are built from declared `field`s. Each field maps by `:as` to
an input type:

| `:as`      | Input                          |
| ---------- | ------------------------------ |
| `string`   | text input                     |
| `text`     | textarea                       |
| `select`   | dropdown (from `:collection`)  |
| `boolean`  | checkbox                       |
| `date`     | date picker                    |
| `time`     | time picker                    |
| `datetime` | datetime-local picker          |
| `number`   | number input                   |
| `password` | password input                 |
| `hidden`   | hidden input                   |
| `file`     | file input                     |

```raku
field('title',     :as<string>, :placeholder<Headline>, :hint('Keep it short.'));
field('body',      :as<text>);
field('published', :as<boolean>);
field('author-id', :as<select>, :collection({ Author.all.all.map(-> $a { $a.id => $a.name }) }));
field('tag-ids',   :as<select>, :multiple, :collection({ Tag.all.all.map(-> $t { $t.id => $t.name }) }));
```

Field labels resolve through I18n (`human-attribute-name`), or pass `:label` to
set one explicitly for a single field. A `:hint` and `:placeholder` render
alongside the input. A `select` renders a dropdown from its explicit
`collection`, and with `:multiple` it renders a multi-select for a collection
association. A `text` field takes `:rows` to size its textarea.

By default a field reads its value from the record's matching column. Pass a
`:value` block to compute the value instead. The block receives the record, so
it can render a virtual attribute or a value derived through a method:

```raku
field('body',  :as<text>, :rows(8));
field('shout', :as<string>, :value({ .title.uc }));
```

A `date`, `time`, or `datetime` field formats its value for the native input,
dropping the timezone offset a `DateTime` would otherwise carry so the browser
control does not blank it. A value that is already a string passes through
unchanged.

## Persistence

Create and update assign only the attributes named in the resource's `permit`
allowlist, so an unlisted parameter is never mass-assigned. On success the action
redirects to the show page with a flash; on a validation failure it re-renders
the form with the submitted values.

```raku
permit(<title body published author-id>);
```

Because an HTML form cannot issue a `PATCH`, the engine also accepts a plain
`POST` to the member path for the update action, so editing works without
JavaScript.

## Validation errors

A failed save re-renders the form. ORM validation errors show inline beneath each
field and in a summary at the top, using the ORM's `full-messages`.

## Nested attributes

Declare a nested association with `nested`, giving its own sub-fields. The model
must back it with `accepts-nested-attributes-for`.

```raku
nested('posts', :multiple, {
  field('title', :as<string>);
});
```

A singular association renders one nested fieldset; a `:multiple` (has-many)
association renders one fieldset per existing child plus a blank row for adding
one. Submitting posts `posts-attributes` to the model, which creates new children,
updates those with an `id`, and (with `allow-destroy`) removes those marked for
deletion. Use the model's `reject-if` to skip a blank add-row.

## File uploads

A `file` field stores its upload through Active Storage. The form becomes
`multipart/form-data`, and on save the upload is attached to the record by its
type, id, and the field name (no storage role on the model required):

```raku
field('cover', :as<file>);
```

Uploading a new file on update replaces the field's previous attachment,
deleting the old blob record and its stored bytes. Saving without choosing a
file leaves the existing attachment in place. Deleting a record purges its file
attachments, so no blob record or stored file is left orphaned.

Configure a storage service and repository in your application's setup, the same
as any Active Storage integration.
