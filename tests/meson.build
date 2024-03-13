gtest_dep = dependency('gtest')
gtest_main_dep = dependency('gtest_main')

executable(
    'w3btest',
    [
        'text_test.cpp',
        'url_test.cpp',
    ],
    install: true,
    dependencies: [
        gtest_dep,
        gtest_main_dep,
        istream_dep,
    ],
)
