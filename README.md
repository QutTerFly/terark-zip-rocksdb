# terark-zip-rocksdb
TerarkZipTable for rocksdb has two modules:

1. a rocksdb SSTable(Static Sorted Table) implementation.
2. a MemTable(Patricia Trie) implementation for rocksdb.

`terark-zip-rocksdb` is a submodule of [TerarkDB](https://github.com/Terark/terarkdb).

## License
This software is open source with Apache 2.0 LICENSE, with NOTES:
  * You can read or redistribute or use the source code under Apache 2.0 license
  * You can not compile this software by yourself, since this software depends on our proprietary core algorithms, which requires a commercial license
  * You can [download](https://github.com/Terark/terarkdb) the precompiled binary library of this software

## Downloads precompiled library
  bmi2-0 means this software can run on older CPU(but the CPU must support popcnt)
  <BR>bmi2-1 means this software can only run on intel-haswell or newer CPU
[download](http://www.terark.com/download/terarkdb/latest)

## Must use our fork of rocksdb
[Our fork of rocksdb](https://github.com/rockeet/rocksdb)
<BR>We changed rocksdb a little to support two-pass scan for building SSTable.

### rocksdb utility tools for terocks(terark-zip-rocksdb)

In this rocksdb fork, we add some extra options to use terocks.

|utility|terocks option|
|-------|--------------|
|ldb|`--use_terocks=1`|

## Restrictions

- User comparator is not supported, you should encoding your keys to make the
  byte lexical order on key is your required order
- `EnvOptions::use_mmap_reads` must be `true`, can be set by `DBOptions::allow_mmap_reads`
- In trial version, we [randomly discard 0.1% of all data](https://github.com/Terark/terark-zip-rocksdb/blob/master/src/table/terark_zip_table.cc#L1002) during SSTable build, so you
  can run benchmark, but you can not use terark-zip-rocksdb in production

## Cautions & Notes
- If calling `rocksdb::DB::Open()` with column families, you must set `table_factory` for each `ColumnFamilyDescriptor`
```
  // Caution: This Open overload, must set column_families[i].options.table_factory
  //
  // You may pass an rocksdb::Option object as db_options, this db_options.table_factory
  // is NOT what we want!
  //
  static Status Open(const DBOptions& db_options, const std::string& name,
                     const std::vector<ColumnFamilyDescriptor>& column_families,
                     std::vector<ColumnFamilyHandle*>* handles, DB** dbptr);
```

## Using TerarkZipTable

### Now just for C++

- Compile flags
```makefile
CXXFLAGS += -I/path/to/terark-zip-rocksdb/src
```
- Linker flags
```makefile
LDFLAGS += -L/path/to/terark-zip-rocksdb-lib
LDFLAGS += -lterark-zip-rocksdb-r # trial version: -lterark-zip-rocksdb-trial-r
LDFLAGS += -lterark-zbs-r -lterark-fsa-r -lterark-core-r
```

- C++ code

```c++
#include <table/terark_zip_table.h>
/// other includes...

  ///....
  TerarkZipTableOptions opt;

  /// TerarkZipTable needs to create temp files during compression
  opt.localTempDir = "/path/to/some/temp/dir"; // default is "/tmp"

  /// 0 : check sum nothing
  /// 1 : check sum meta data and index, check on file load
  /// 2 : check sum all data, not check on file load, checksum is for
  ///     each record, this incurs 4 bytes overhead for each record
  /// 3 : check sum all data with one checksum value, not checksum each record,
  ///     if checksum doesn't match, load will fail
  opt.checksumLevel = 3; // default 1

  ///    < 0 : only last level using terarkZip
  ///          this is equivalent to terarkZipMinLevel == num_levels-1
  /// others : use terarkZip when curlevel >= terarkZipMinLevel
  ///          this includes the two special cases:
  ///                   == 0 : all levels using terarkZip
  ///          >= num_levels : all levels using fallback TableFactory
  /// it shown that set terarkZipMinLevel = 0 is the best choice
  /// if mixed with rocksdb's native SST, those SSTs may using too much
  /// memory & SSD, which degrades the performance
  opt.terarkZipMinLevel = 0; // default

  /// optional
  opt.softZipWorkingMemLimit = 16ull << 30; // default
  opt.hardZipWorkingMemLimit = 32ull << 30; // default

  /// to let rocksdb compaction algo know the estimate SST file size
  opt.estimateCompressionRatio = 0.2;

  /// the global dictionary size over all value size
  opt.sampleRatio = 0.03;
 
  /// other opt are tricky, just use default

  /// rocksdb options when using terark-zip-rocksdb:

  /// fallback can be NULL
  auto fallback = NewBlockBasedTableFactory(); // or NewAdaptiveTableFactory();
  auto factory = NewTerarkZipTableFactory(opt, fallback);
  options.table_factory.reset(factory);

  /// terark-zip use mmap
  options.allow_mmap_reads = true;

  /// universal compaction reduce write amplification and is more friendly for
  /// large SST file, terark SST is better on larger SST file.
  /// although universal compaction needs 2x SSD space on worst case, but
  /// with terark-zip's high compression, the used SSD space is much smaller
  /// than rocksdb's block compression schema
  options.compaction_style = rocksdb::kCompactionStyleUniversal;

  /// larger MemTable yield larger level0 SST file
  /// larger SST file make terark-zip better
  options.write_buffer_size     =  1ull << 30; // 1G
  options.target_file_size_base =  1ull << 30; // 1G

`terark-zip-rocksdb` implements an `SSTable` for [TerarkDB's submodule rocksdb](https://github.com/Terark/rocksdb), [terark-zip-rocksdb license](https://github.com/Terark/terark-zip-rocksdb/blob/master/LICENSE) is Apache 2.0, with NOTES:
  * You can read or redistribute or use the source code under Apache 2.0 license
  * You can not compile this software by yourself, since this software depends on our proprietary core algorithms, which requires a commercial license
  * You can [download](https://github.com/Terark/terarkdb) the precompiled binary library of this software

## Contact
- contact@terark.com
- [terark.com](http://terark.com)

