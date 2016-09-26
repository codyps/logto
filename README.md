= logto =

Redirect program output to logging locations.

Outputs:

 - kmsg
 - syslog
 - stdout

Supports:

 - using printk & systemd style `<N>` priority prefixes
 - prefixing output with a 'name: ' (and auto-selecting that name based on exe name)
 - chooses to fork or just fixes up fds based on request

== Todo ==

 - Rotated file
 - logto as a library
 - LD_PRELOAD tricks to avoid second process in some cases
 - stderr


=== Way out there ===

 - support start-stop-daemon like functionality (ie: remove the need to use
   start-stop-daemon if using logto).
 - support "started" notification via sd_notify() api for start-stop-daemon
   users.

== License ==

GPLv3 or later.

All contributions are licensed as this unless otherwise noted.
