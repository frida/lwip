subdir('include')
subdir('core')
subdir('netif')
subdir('api')

sources = core_sources + core_ipv4_sources + core_ipv6_sources + netif_sources + api_sources + port_sources

lwip = library('lwip', sources,
  include_directories: incdirs,
  dependencies: [glib_dep],
  install: true,
)

lwip_dep = declare_dependency(
  include_directories: incdirs,
  link_with: lwip,
)

pkg = import('pkgconfig')
pkg.generate(lwip,
  filebase: 'lwip',
  description: 'A Lightweight TCP/IP stack',
  url: 'https://savannah.nongnu.org/projects/lwip/',
)

meson.override_dependency('lwip', lwip_dep)
