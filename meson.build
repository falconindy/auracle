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

pod2man = find_program('pod2man')

aurlib_sources = '''
  src/aur/aur.cc src/aur/aur.hh
  src/aur/package.cc src/aur/package.hh
  src/aur/request.cc src/aur/request.hh
  src/aur/response.cc src/aur/response.hh
  src/aur/json_internal.hh
'''.split()

aurlib = static_library(
    'aurlib',
    aurlib_sources,
    include_directories : [
        include_directories('src'),
        include_directories('/usr/include/nlohmann')
    ],
    dependencies : [libcurl, libsystemd])

auracle_sources = '''
  src/auracle.cc src/auracle.hh
  src/format.cc src/format.hh
  src/sort.hh
  src/pacman.cc src/pacman.hh
  src/terminal.cc src/terminal.hh
  src/inmemory_repo.cc src/inmemory_repo.hh
'''.split()

auracle = executable(
    'auracle',
    auracle_sources,
    link_with : [aurlib],
    dependencies : [libarchive, libalpm, stdcppfs],
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
  'fmt', command : [
    'find',
    '-name', '*.[ch][ch]',
    '-exec', 'clang-format', '-i', '-style=Google', '{}', '+'
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
        link_with : [aurlib, gtest_main],
        dependencies : [gtest, gmock]))
    endforeach
endif

# integration tests
python = find_program('python')
foreach input : [
  'tests/clone.py',
  'tests/download.py',
  'tests/info.py',
  'tests/pkgbuild.py',
  'tests/raw_query.py',
  'tests/regex_search.py',
  'tests/search.py',
]
  basename = input.split('/')[-1].split('.')[0]
  test(
    '@0@_integration_test'.format(basename),
    python,
    args : [join_paths(meson.source_root(), input)],
    env : ['PYTHONDONTWRITEBYTECODE=1'])
endforeach
