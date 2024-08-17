project(
    'auracle',
    'cpp',
    version: '0',
    license: 'MIT',
    meson_version: '>=0.56',
    default_options: ['cpp_std=c++23', 'warning_level=2'],
)

cpp = meson.get_compiler('cpp')

add_project_arguments(
    '-DPROJECT_VERSION="@0@"'.format(meson.project_version()),
    language: 'cpp',
)

add_project_arguments(
    cpp.get_supported_arguments(['-ffunction-sections', '-fdata-sections']),
    language: 'cpp',
)

add_project_link_arguments(
    cpp.get_supported_link_arguments(['-Wl,--gc-sections']),
    language: 'cpp',
)

json = dependency(
    'nlohmann_json',
    version: '>=3.7.1',
    fallback: ['nlohmann_json', 'nlohmann_json_dep'],
)
libalpm = dependency('libalpm')
libcurl = dependency('libcurl')
libfmt = dependency(
    'fmt',
    version: '>=6.0.0',
    fallback: ['fmt', 'fmt_dep'],
    static: 'true',
)
libsystemd = dependency('libsystemd')
gtest = dependency(
    'gtest',
    version: '>=1.10.0',
    required: get_option('unittests'),
    disabler: true,
)
gmock = dependency(
    'gmock',
    version: '>=1.10.0',
    required: get_option('unittests'),
    disabler: true,
)

abseil_proj = subproject(
    'abseil-cpp',
    default_options: [
        'cpp_std=c++20',
        'default_library=static',
        'warning_level=0',
    ],
)
abseil = declare_dependency(
    dependencies: [
        abseil_proj.get_variable('absl_container_dep'),
        abseil_proj.get_variable('absl_status_dep'),
        abseil_proj.get_variable('absl_strings_dep'),
        abseil_proj.get_variable('absl_time_dep'),
    ],
    include_directories: [abseil_proj.get_variable('absl_include_dir')],
)

pod2man = find_program('pod2man')

python = import('python')
py3 = python.find_installation('python3')

libaur = declare_dependency(
    link_with: [
        static_library(
            'aur',
            files(
                '''
        src/aur/client.cc src/aur/client.hh
        src/aur/package.cc src/aur/package.hh
        src/aur/request.cc src/aur/request.hh
        src/aur/response.cc src/aur/response.hh
        src/aur/json_internal.hh
      '''.split(),
            ),
            dependencies: [abseil, json, libcurl, libsystemd],
            include_directories: ['src'],
        ),
    ],
    include_directories: ['src'],
)

libauracle = declare_dependency(
    link_with: [
        static_library(
            'auracle',
            files(
                '''
        src/auracle/auracle.cc src/auracle/auracle.hh
        src/auracle/dependency.cc src/auracle/dependency.hh
        src/auracle/dependency_kind.cc src/auracle/dependency_kind.hh
        src/auracle/format.cc src/auracle/format.hh
        src/auracle/package_cache.cc src/auracle/package_cache.hh
        src/auracle/pacman.cc src/auracle/pacman.hh
        src/auracle/search_fragment.cc src/auracle/search_fragment.hh
        src/auracle/sort.cc src/auracle/sort.hh
        src/auracle/terminal.cc src/auracle/terminal.hh
      '''.split(),
            ),
            dependencies: [abseil, libalpm, libaur, libfmt],
            include_directories: ['src'],
        ),
    ],
    include_directories: ['src'],
)

executable(
    'auracle',
    files('src/auracle_main.cc'),
    dependencies: [abseil, libauracle],
    install: true,
)

custom_target(
    'man',
    output: 'auracle.1',
    input: 'man/auracle.1.pod',
    command: [
        pod2man,
        '--section=1',
        '--center=Auracle Manual',
        '--name=AURACLE',
        '--release=Auracle @0@'.format(meson.project_version()),
        '@INPUT@',
        '@OUTPUT@',
    ],
    install: true,
    install_dir: join_paths(get_option('mandir'), 'man1'),
)

install_data(
    files('extra/bash_completion'),
    rename: ['auracle'],
    install_dir: join_paths(get_option('datadir'), 'bash-completion/completions'),
)

install_data(
    files('extra/zsh_completion'),
    rename: ['_auracle'],
    install_dir: join_paths(get_option('datadir'), 'zsh/site-functions'),
)

run_target(
    'fmt',
    command: [
        join_paths(meson.project_source_root(), 'build-aux/source-format'),
    ],
)

# unit tests
test(
    'libaur',
    executable(
        'libaur_test',
        files(
            '''
      src/test/gtest_main.cc
      src/aur/request_test.cc
      src/aur/response_test.cc
    '''.split(),
        ),
        dependencies: [abseil, gtest, gmock, libaur],
    ),
    protocol: 'gtest',
    suite: 'libaur',
)

test(
    'libauracle',
    executable(
        'libauracle_test',
        files(
            '''
      src/test/gtest_main.cc
      src/auracle/dependency_kind_test.cc
      src/auracle/package_cache_test.cc
      src/auracle/dependency_test.cc
      src/auracle/format_test.cc
      src/auracle/search_fragment_test.cc
      src/auracle/sort_test.cc
    '''.split(),
        ),
        dependencies: [abseil, gtest, gmock, libauracle],
    ),
    protocol: 'gtest',
    suite: 'libauracle',
)

# integration tests
python_requirement = '>=3.7'
if py3.found() and py3.language_version().version_compare(python_requirement)
    foreach input : [
        'tests/test_buildorder.py',
        'tests/test_clone.py',
        'tests/test_custom_format.py',
        'tests/test_info.py',
        'tests/test_outdated.py',
        'tests/test_raw_query.py',
        'tests/test_regex_search.py',
        'tests/test_resolve.py',
        'tests/test_search.py',
        'tests/test_show.py',
        'tests/test_sort.py',
        'tests/test_update.py',
    ]
        basename = input.split('/')[-1].split('.')[0]

        test(
            basename,
            py3,
            suite: 'auracle',
            args: ['-W', 'all', join_paths(meson.project_source_root(), input)],
            env: ['PYTHONDONTWRITEBYTECODE=1'],
        )
    endforeach
else
    message(
        'Skipping integration tests, python @0@ not found'.format(
            python_requirement,
        ),
    )
endif
