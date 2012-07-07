{
  'targets': [
    {
      'target_name': 'memwatch',
      'include_dirs': [
      ],
      'variables': {
        'node_ver': '<!(node --version | sed -e "s/^v\([0-9]*\\.[0-9]*\).*$/\\1/")'
      },
      'sources': [
        'src/heapdiff.cc',
        'src/init.cc',
        'src/memwatch.cc',
        'src/util.cc'
      ],
      'target_conditions': [
        ['node_ver=="0.8"', { 'defines': ['NEW_COMPACTION_BEHAVIOR'] } ]
      ]
    }
  ]
}
