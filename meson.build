project('auracle', 'cpp',
        version : '0',
        license : 'MIT',
        default_options : [
          'cpp_std=c++1z',
        ])

conf = configuration_data()
conf.set('PACKAGE_VERSION', meson.project_version())

configure_file(
    output : 'config.h',
    configuration : conf)

add_project_arguments('-include', 'config.h', language : 'cpp')

libalpm = dependency('libalpm')
libarchive = dependency('libarchive')
libcurl = dependency('libcurl')

nlohmann_json_sp = subproject('nlohmann_json')

pod2man = find_program('pod2man')
ragel = find_program('ragel', required : false)

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
        nlohmann_json_sp.get_variable('nlohmann_json_inc')
    ],
    dependencies : [
        libcurl,
        nlohmann_json_sp.get_variable('nlohmann_json_dep')
    ])

auracle_sources = '''
  src/auracle.cc src/auracle.hh
  src/format.cc src/format.hh
  src/sort.hh
  src/pacman.cc src/pacman.hh
  src/terminal.cc src/terminal.hh
  src/inmemory_repo.cc src/inmemory_repo.hh
'''.split()

if ragel.found()
  gen_ragel = custom_target(
      'gen-ragel',
      input : ['src/format_parser.rl'],
      output : ['format_parser.cc'],
      command : [ ragel, '@INPUT@', '-o', '@OUTPUT@' ])
  auracle_sources += gen_ragel
endif

executable(
    'auracle',
    auracle_sources,
    link_with : [aurlib],
    dependencies : [libarchive, libalpm],
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

