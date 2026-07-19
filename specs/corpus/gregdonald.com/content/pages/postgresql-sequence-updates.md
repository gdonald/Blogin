---
title: PostgreSQL Sequence Updates
date: 2012-01-07
slug: postgresql-sequence-updates
description: I had a problem with PostgreSQL pgdump recently. My setval() calls were all set to '1'. I whipped up this quick script to fix things: #!/usr/bin/env python DB_NAME = 'my_db' from subprocess import...
tags: [postgresql, python]
archives: [2012-01]
---
I had a problem with PostgreSQL pgdump recently. My setval() calls were all set to '1'.

I wrote this script to fix things:

```python
#!/usr/bin/env python
DB_NAME = 'my_db'

from subprocess import Popen, PIPE
import re

exclude = [ 'tablename', 'rows' ]
tp = re.compile( '[^a-z_]' )
ts = Popen( [ "/usr/bin/psql", DB_NAME, "-c SELECT tablename FROM pg_tables WHERE tablename NOT LIKE 'pg_%' AND tablename NOT LIKE 'sql_%' ORDER BY tablename" ], stdout=PIPE ).communicate()[ 0 ].split( ' ' )

tables = []
for t in ts:
    t = tp.sub( '', t )
    if len( t ) == 0 or t in exclude:
        continue
    tables.append( t )

for t in tables:
    sql = "SELECT pg_catalog.setval( pg_get_serial_sequence( '%s', 'id' ), ( SELECT MAX( id ) FROM %s ) + 1 );" % ( t, t )
    print Popen( [ "/usr/bin/psql", DB_NAME, "-c %s" % sql ], stdout=PIPE ).communicate()[ 0 ]
```
