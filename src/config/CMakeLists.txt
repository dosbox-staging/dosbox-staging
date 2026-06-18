target_sources(dosboxcommon PRIVATE
  config.cpp
  setup.cpp

  migrate/file_walker.cpp
  migrate/line_classifier.cpp
  migrate/migrate.cpp
  migrate/rules.cpp
)
