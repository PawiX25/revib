# SGI STL 3.2

The SGI Standard Template Library 3.2: the 87 files of `stl32.zip` from the
SGI STL distribution (sha256
`40db8c59215c726d316c450245e04b87ee52feb188f945371cc7527d58999c2b`), with
the two changes below. Licence notices from Silicon Graphics and
Hewlett-Packard are in each file.

This is the STL the game was built with. Compiled by the project's
egcs-2.91.66, with the headers of `include/stl_psx` and the configuration
set in the Makefile, its template instances are byte-identical to the ones
in the executable, e.g. `vector<T>::_M_insert_aux`,
`find(..., random_access_iterator_tag)`,
`basic_string<char>::append(const char*, const char*, forward_iterator_tag)`
and `__malloc_alloc_template<0>::_S_oom_malloc`.

## Changes

Two things in the stock headers are compiled into every file that includes
`<string>`, and none of the game's files have them, so the game's copy of
the STL evidently differed here:

- `string`: `__get_c_string()` is a static, non-inline function; it is made
  `inline` so that it is only emitted where used.
- `stl_algobase.h`: the `char` overloads of `lexicographical_compare()` and
  `__lexicographical_compare_3way()` instantiate the `signed char` templates
  even when unused; they are disabled.

Both are marked "Vib-Ribbon" in the source.
