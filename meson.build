project('lwip', 'c', version: '2.2.1')

lwip_debug = get_option('lwip_debug')
if lwip_debug.enabled() or (lwip_debug.auto() and get_option('debug') and get_option('optimization') in ['0', 'g'])
  add_project_arguments('-DLWIP_DEBUG', language: 'c')
endif

ndebug = get_option('b_ndebug')
optimize_for_prod = ndebug == 'true' or (ndebug == 'if-release' and not get_option('debug'))
if optimize_for_prod
  add_project_arguments(
    '-DLWIP_NOASSERT',
    '-DG_DISABLE_ASSERT',
    '-DG_DISABLE_CHECKS',
    '-DG_DISABLE_CAST_CHECKS',
    language: 'c')
endif

glib_options = [
  'tests=false',
]
if optimize_for_prod
  glib_options += [
    'glib_debug=disabled',
    'glib_assert=false',
    'glib_checks=false',
  ]
endif
glib_dep = dependency('glib-2.0', default_options: glib_options)

conf_data = configuration_data()

conf_data.set10('LWIP_IPV4', get_option('ipv4').allowed())
conf_data.set10('LWIP_IPV6', get_option('ipv6').allowed())
conf_data.set10('LWIP_DNS', get_option('dns').allowed())
conf_data.set10('LWIP_ARP', get_option('arp').allowed())
conf_data.set10('LWIP_ETHERNET', get_option('ethernet').allowed())
conf_data.set('TCP_MSS', get_option('tcp_mss'))
conf_data.set('TCP_SND_BUF', get_option('tcp_snd_buf'))
conf_data.set('TCP_WND', get_option('tcp_wnd'))

configure_file(
  input: 'lwipopts.h.meson.in',
  output: 'lwipopts.h',
  configuration: conf_data,
  install: true,
  install_dir: get_option('includedir') / 'lwip',
)

incdirs = include_directories(
  'src' / 'include',
  'contrib' / 'ports' / 'glib' / 'include',
  '.',
)

subdir('contrib')
subdir('src')
