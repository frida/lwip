if get_option('ipv6').allowed()
  core_ipv6_sources = files(
    'dhcp6.c',
    'ethip6.c',
    'icmp6.c',
    'inet6.c',
    'ip6.c',
    'ip6_addr.c',
    'ip6_frag.c',
    'mld6.c',
    'nd6.c',
  )
else
  core_ipv6_sources = []
endif
