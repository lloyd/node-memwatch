{
  'targets': [
    {
      'target_name': 'memwatch',
      'include_dirs': [
        'node_modules/nan'
      ],
      'sources': [
        'src/heapdiff.cc',
        'src/init.cc',
        'src/memwatch.cc',
        'src/util.cc'
      ]
    }
  ]
}
