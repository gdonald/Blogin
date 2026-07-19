---
title: MongoDB Data Durability
date: 2014-01-31
slug: mongodb-data-durability
description: It doesn't seem you can lose data even when you might otherwise expect to ;) I setup a small replica set using mongod --fork --logpath a.log --smallfiles --oplogSize 50 --port 27001 --dbpath data/z1...
tags: [mongodb]
archives: [2014-01]
---
It doesn't seem you can lose data even when you might otherwise expect to ;)

I setup a small replica set using

```bash
mongod --fork --logpath a.log --smallfiles --oplogSize 50 --port 27001 --dbpath data/z1 --replSet z
mongod --fork --logpath b.log --smallfiles --oplogSize 50 --port 27002 --dbpath data/z2 --replSet z
mongod --fork --logpath c.log --smallfiles --oplogSize 50 --port 27003 --dbpath data/z3 --replSet z
```

And initalized it:

```bash
> rs.initiate(
    { _id:'z',
      members:[
        { _id:1, host:'localhost:27001' },
        { _id:2, host:'localhost:27002' },
        { _id:3, host:'localhost:27003' }
      ]
    }
);
```

Then I killed all three processes:

```bash
kill -9 25542 25496 25483
```

Next I brought one of them back up

```bash
mongod --fork --logpath c.log --smallfiles --oplogSize 50 --port 27003 --dbpath data/z3
```

and inserted a doc

```bash
> db.foo.insert({a:1})
```

Then I killed that process

```bash
kill -9 25885
```

and brought the replica set back online using

```bash
mongod --fork --logpath a.log --smallfiles --oplogSize 50 --port 27001 --dbpath data/z1 --replSet z
mongod --fork --logpath b.log --smallfiles --oplogSize 50 --port 27002 --dbpath data/z2 --replSet z
mongod --fork --logpath c.log --smallfiles --oplogSize 50 --port 27003 --dbpath data/z3 --replSet z
```

and my data is still there:

```bash
> mongo --port 27003
MongoDB shell version: 2.4.6
connecting to: 127.0.0.1:27003/test
z:SECONDARY> db.setSlaveOk()
z:SECONDARY> db.foo.find()
{ "_id" : ObjectId("52eb09f9df67edd1c6d33e71"), "a" : 1 }
z:SECONDARY>
```

In addition, if I insert data into the primary it seems to be perfectly happy to live right there alongside my existing data on the secondary:

```bash
> mongo --port 27001
MongoDB shell version: 2.4.6
connecting to: 127.0.0.1:27001/test
z:PRIMARY> db.foo.insert({b:2})
z:PRIMARY> db.foo.find()
{ "_id" : ObjectId("52eb0cefdf3e83d897ae68ec"), "b" : 2 }
z:PRIMARY>
``` ```bash
> mongo --port 27003
MongoDB shell version: 2.4.6
connecting to: 127.0.0.1:27003/test
z:SECONDARY> db.setSlaveOk()
z:SECONDARY> db.foo.find()
{ "_id" : ObjectId("52eb09f9df67edd1c6d33e71"), "a" : 1 }
{ "_id" : ObjectId("52eb0cefdf3e83d897ae68ec"), "b" : 2 }
```
