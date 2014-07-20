portlogger: Log TCP Connections

The main purpose of portlogger is to detect port scanning. It works by
listening on a list of ports and log all connections. If you run it as root, it
will change uid to nobody.
