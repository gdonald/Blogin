---
title: Doctype
slug: doctype
order: 6
description: A line beginning with !!! declares the document's doctype. It must be the first non-blank line of the template; placing it after any tag or statement...
toc: true
---
A line beginning with `!!!` declares the document's doctype. It must be the first non-blank line of the template; placing it after any tag or statement raises `X::HAML::DoctypeNotFirst`.

## Argument-less form

```haml
!!!
```

```html
<!DOCTYPE html>
```

With no argument, `!!!` emits the format-default doctype. The format
comes from the [`format`](/docs/Template-HAML/config) config option and the default is
`html5`. The other formats produce different defaults:

| `format` | Argument-less `!!!` output                                                                                |
|----------|-----------------------------------------------------------------------------------------------------------|
| `html5`  | `<!DOCTYPE html>`                                                                                          |
| `html4`  | `<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">`  |
| `xhtml`  | `<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">` |

Named variants (below) ignore the `format` option and always emit the
exact doctype the variant names.

## Named variants

| Source           | Output |
|------------------|--------|
| `!!! 5`          | `<!DOCTYPE html>` |
| `!!! 1.1`        | XHTML 1.1 |
| `!!! Strict`     | XHTML 1.0 Strict |
| `!!! Frameset`   | XHTML 1.0 Frameset |
| `!!! Mobile`     | XHTML Mobile 1.2 |
| `!!! RDFa`       | XHTML+RDFa 1.0 |
| `!!! Basic`      | XHTML Basic 1.1 |
| `!!! XML`        | `<?xml version='1.0' encoding='utf-8' ?>` |
| `!!! XML <enc>`  | XML declaration with the given encoding |

```haml
!!! XML iso-8859-1
```

```html
<?xml version='1.0' encoding='iso-8859-1' ?>
```

## Errors

An unrecognized variant raises `X::HAML::UnknownDoctype`:

```haml
!!! Bogus
```

```text
[HAML source:1:1] unknown doctype 'Bogus'; expected one of: 5 1.1 Strict Frameset Mobile RDFa Basic XML
```
