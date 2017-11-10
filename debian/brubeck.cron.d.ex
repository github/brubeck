#
# Regular cron jobs for the brubeck package
#
0 4	* * *	root	[ -x /usr/bin/brubeck_maintenance ] && /usr/bin/brubeck_maintenance
