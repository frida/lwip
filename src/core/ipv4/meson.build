if get_option('ipv4').allowed()
  core_ipv4_sources = files(
    'acd.c',
    'autoip.c',
    'dhcp.c',
    'etharp.c',
    'icmp.c',
    'igmp.c',
    'ip4_frag.c',
    'ip4.c',
    'ip4_addr.c',
  )
else
  core_ipv4_sources = []
endif
