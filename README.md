# TerocksDB 

TerocksDB is a core product of [Terark](http://terark.com). It is a RocksDB distribution that powered by &copy;Terark&trade; algorithms. with these algorithms, TerocksDB is able to store more data and access much faster than official RocksDB(3+X more data and 10+X faster) on same hardware.

TerocksDB is completely compatible(binary compatible) with official RocksDB, TerocksDB has two components:

## Terark modified RocksDB

We forked RocksDB and made a few changes, here is [Terark modified RocksDB](http://github/rockeet/rocksdb).

Our changes for RocksDB does not change any RocksDB API, and does not incurred any extra dependencies, say, Terark modified RocksDB does not depend on TerarkZipTable. Our changes includes:

  1. Add optional two pass scan on SSTable build, existing SSTable is not impacted.
  
  2. Add [TerarkZipTable config by env var](), this change using functions in libterark-zip-table as weak symbol, this is why it does not depends on TerarkZipTable.

## TerarkZipTable

TerarkZipTable is an SSTable implementation using  &copy;Terark&trade; algorithms for RocksDB. It must work with [Terark modified RocksDB](http://github/rockeet/rocksdb).

# License
This software is open source, you can read the source code,
but you can not compile this software by yourself,
you must get [our](http://terark.com) comercial license to use this software in production.

- contact@terark.com
- [Terark.com](www.terark.com)

# Documentation
Now you can experience our product easily, please refer to our product documentation for more detail.

[TerocksDB Documentation](https://github.com/Terark/terark-zip-rocksdb/wiki)
