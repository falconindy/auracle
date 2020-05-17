project('auracle', 'cpp',
        version : '0',
        license : 'MIT',
        meson_version : '>=0.50',
        default_options : [
          'cpp_std=c++17',
          'warning_level=3',
        ])

cpp = meson.get_compiler('cpp')

add_project_arguments(
    '-DPROJECT_VERSION="@0@"'.format(meson.project_version()),
    language : 'cpp')

add_project_arguments(cpp.get_supported_arguments([
  '-ffunction-sections',
  '-fdata-sections',
]), language : 'cpp')

add_project_link_arguments(cpp.get_supported_link_arguments([
  '-Wl,--gc-sections',
]), language : 'cpp')

json = dependency('nlohmann_json',
                  version : '>=3.7.1',
                  fallback: ['nlohmann_json', 'nlohmann_json_dep'])
libalpm = dependency('libalpm')
libcurl = dependency('libcurl')
libfmt = dependency('fmt',
                    version : '>=6.0.0',
                    fallback : ['fmt', 'fmt_dep'])
libsystemd = dependency('libsystemd')
gtest = dependency('gtest', required : false)
gmock = dependency('gmock', required : false)

pod2man = find_program('pod2man')

python = import('python')
py3 = python.find_installation('python3')

libaur = static_library(
  'aur',
  files('''
    src/aur/aur.cc src/aur/aur.hh
    src/aur/package.cc src/aur/package.hh
    src/aur/request.cc src/aur/request.hh
    src/aur/response.cc src/aur/response.hh
    src/aur/json_internal.hh
  '''.split()),
  dependencies : [json, libcurl, libsystemd])

libauracle = static_library(
  'auracle',
  files('''
    src/auracle/auracle.cc src/auracle/auracle.hh
    src/auracle/dependency_kind.cc src/auracle/dependency_kind.hh
    src/auracle/format.cc src/auracle/format.hh
    src/auracle/package_cache.cc src/auracle/package_cache.hh
    src/auracle/pacman.cc src/auracle/pacman.hh
    src/auracle/sort.cc src/auracle/sort.hh
    src/auracle/terminal.cc src/auracle/terminal.hh
  '''.split()),
  include_directories : [
    'src',
  ],
  link_with : [libaur],
  dependencies : [libalpm, libfmt])

executable(
  'auracle',
  files('src/auracle_main.cc'),
  include_directories : [
    'src',
  ],
  link_with : [libauracle],
  install : true)

custom_target(
  'man',
  output : 'auracle.1',
  input : 'man/auracle.1.pod',
  command : [
    pod2man,
    '--section=1',
    '--center=Auracle Manual',
    '--name=AURACLE',
    '--release=Auracle @0@'.format(meson.project_version()),
    '@INPUT@', '@OUTPUT@'
  ],
  install : true,
  install_dir : join_paths(get_option('mandir'), 'man1'))

install_data(
  files('extra/bash_completion'),
  rename : ['auracle'],
  install_dir : join_paths(get_option('datadir'), 'bash-completion/completions'))

install_data(
  files('extra/zsh_completion'),
  rename : ['_auracle'],
  install_dir : join_paths(get_option('datadir'), 'zsh/site-functions'))

run_target(
  'fmt',
  command : [
    join_paths(meson.source_root(), 'build-aux/clang-format')
  ])

run_target(
  'tidy',
  command : [
    join_paths(meson.source_root(), 'build-aux/clang-tidy')
  ])

# unit tests
if gtest.found() and gmock.found()
  gtest_main = static_library(
    'gtest_main',
    'src/test/gtest_main.cc')

  foreach input : [
    {
      'prefix': 'libaur',
      'link_with' : [libaur],
      'tests' : [
        'src/aur/request_test.cc',
        'src/aur/response_test.cc',
      ],
    },
    {
      'prefix': 'libauracle',
      'link_with' : [libauracle],
      'tests' : [
        'src/auracle/dependency_kind_test.cc',
        'src/auracle/package_cache_test.cc',
        'src/auracle/format_test.cc',
        'src/auracle/sort_test.cc',
      ],
    },
  ]

    foreach source : input.get('tests')
      basename = source.split('/')[-1].split('.')[0]
      test(
        '@0@_@1@'.format(input.get('prefix'), basename),
        executable(
          basename,
          source,
          include_directories : [
            'src',
          ],
          link_with : [gtest_main] + input.get('link_with'),
          dependencies : [gtest, gmock]))
    endforeach
  endforeach
else
  message('Skipping unit tests, gtest or gmock not found')
endif

# integration tests
python_requirement = '>=3.7'
if py3.found() and py3.language_version().version_compare(python_requirement)
  foreach input : [
    'tests/buildorder.py',
    'tests/clone.py',
    'tests/custom_format.py',
    'tests/info.py',
    'tests/outdated.py',
    'tests/raw_query.py',
    'tests/regex_search.py',
    'tests/search.py',
    'tests/show.py',
    'tests/sort.py',
    'tests/update.py',
  ]
    basename = input.split('/')[-1].split('.')[0]

    test(
      'auracle_@0@_integration_test'.format(basename),
      py3,
      args : [join_paths(meson.source_root(), input)],
      env : ['PYTHONDONTWRITEBYTECODE=1'])
  endforeach
else
  message('Skipping integration tests, python @0@ not found'.format(
    python_requirement))
endif
