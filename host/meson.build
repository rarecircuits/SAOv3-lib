project(
  'raw_i2c_read',
  'c',
  version : '1.0',
  default_options : [
    'warning_level=2',
    'c_std=c11'
  ]
)

incdir = include_directories(['saoh_core/include', '.'])
srcs = [
  'raw_i2c_read.c',
  'linux_i2c_interface.c',
  'saoh_core/src/smbus_i2c_shim.c',
  'saoh_core/src/discovery.c',
  'saoh_core/src/err.c',
  'saoh_core/src/log.c',
  'saoh_core/src/itf.c'
]

executable(
  'raw_i2c_read',
  srcs,
  include_directories : incdir,
  install : true
)
