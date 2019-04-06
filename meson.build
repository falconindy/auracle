project('auracle', 'cpp',
        version : '0',
        license : 'MIT',
        default_options : [
          'cpp_std=c++17',
        ])

conf = configuration_data()
conf.set('PACKAGE_VERSION', meson.project_version())

cpp = meson.get_compiler('cpp')

configure_file(
    output : 'config.h',
    configuration : conf)

add_project_arguments('-include', 'config.h', language : 'cpp')

possible_cpp_flags = []
possible_link_flags = []
if get_option('buildtype') != 'debug'
  possible_cpp_flags += [
    '-ffunction-sections',
    '-fdata-sections',
  ]

  possible_link_flags += '-Wl,--gc-sections'
endif

add_project_arguments(cpp.get_supported_arguments(possible_cpp_flags), language : 'cpp')
add_project_link_arguments(cpp.get_supported_link_arguments(possible_link_flags), language : 'cpp')

libalpm = dependency('libalpm')
libarchive = dependency('libarchive')
libcurl = dependency('libcurl')
libsystemd = dependency('libsystemd')
gtest = dependency('gtest', required : false)
gmock = dependency('gmock', required : false)
stdcppfs = cpp.find_library('stdc++fs')

libfmt = dependency('fmt', required : false)
if not libfmt.found()
  libfmt = subproject('fmt').get_variable('fmt_dep')
endif

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
    include_directories : include_directories(
      'src',
      'third_party/nlohmann_json',
    ),
    dependencies : [libcurl, libsystemd, stdcppfs])

libauracle = static_library(
    'auracle',
    files('''
      src/auracle/auracle.cc src/auracle/auracle.hh
      src/auracle/format.cc src/auracle/format.hh
      src/auracle/package_cache.cc src/auracle/package_cache.hh
      src/auracle/pacman.cc src/auracle/pacman.hh
      src/auracle/sort.cc src/auracle/sort.hh
      src/auracle/terminal.cc src/auracle/terminal.hh
    '''.split()),
    include_directories : include_directories(
      'src',
    ),
    link_with : [libaur],
    dependencies : [libalpm, libarchive, libfmt, stdcppfs])

auracle = executable(
    'auracle',
    files('src/auracle_main.cc'),
    include_directories : include_directories(
      'src',
    ),
    link_with : [libauracle],
    install : true)

man = custom_target(
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
    'src/aur/request_test.cc',
    'src/aur/response_test.cc',
  ]
    basename = input.split('/')[-1].split('.')[0]

    test(
      basename,
      executable(
        basename,
        input,
        link_with : [libaur, gtest_main],
        dependencies : [gtest, gmock]))
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
    'tests/download.py',
    'tests/info.py',
    'tests/raw_query.py',
    'tests/regex_search.py',
    'tests/search.py',
    'tests/show.py',
    'tests/sort.py',
    'tests/sync.py',
  ]
    basename = input.split('/')[-1].split('.')[0]

    test(
      '@0@_integration_test'.format(basename),
      py3,
      args : [join_paths(meson.source_root(), input)],
      env : ['PYTHONDONTWRITEBYTECODE=1'])
  endforeach
else
  message('Skipping integration tests, python @0@ not found'.format(
    python_requirement))
endif
