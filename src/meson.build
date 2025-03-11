custom_accel_sources = [
  'main.c',
  'custom-accel-application.c',
  'custom-accel-window.c',
  'plot-widget.c',
  'device-manager.c',
  'apply-accel-settings-dialog.c',
]

custom_accel_deps = [
  dependency('gtk4'),
  dependency('libadwaita-1', version: '>= 1.4'),
  dependency('libinput'),
  dependency('libudev'),
  dependency('x11'),
  dependency('xi'),
]

custom_accel_sources += gnome.compile_resources(
  'custom-accel-resources',
  'custom-accel.gresource.xml',
  c_name: 'custom_accel',
)

link_args = ['-lm']

executable(
  'custom-accel',
  custom_accel_sources,
  dependencies: custom_accel_deps,
  link_args: link_args,
  install: true,
)
