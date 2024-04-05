gtest_dep = dependency('gtest')
gtest_main_dep = dependency('gtest_main')

executable(
    'w3btest',
    [
        'text_test.cpp',
        'url_test.cpp',
        'entity_test.cpp',
        'token_test.cpp',
    ],
    install: true,
    dependencies: [
        gtest_dep,
        gtest_main_dep,
        without_gc_dep,
        # with_gc_dep,
        html_dep,
    ],
)
