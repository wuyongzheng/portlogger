portlogger: Log TCP Connections

Portlogger detects port scanning. It works by listening on a list of TCP ports
and logging all connections. Unlike iptables or libpcap based solutions, it
doesn't require root privilege.

====
Features

* It is very lightweight in terms of system resources. When there is no
  connection, it consumes zero CPU (calling select(2) with infinite timeout).
* It's a single standlong executable. No need to install.
* No root privilege is needed, because it doesn't use raw sockets or libpcap
  stuff. However, if you want to listend to ports below 1024, root is needed.
* If it is run as root, it will change UID to nobody.
* The log file is in tab-separated-value format.

====
Log Format

Each line (except for the first line, which lists the ports listened to)
represents a single TCP connection. The following information is logged.

* time
* local port
* remote ip
* remote port
* number of bytes received
* data received (only first 128 bytes)

====
Real Log Example

Wed Jul 23 22:29:35 2014	Listening to: 21 23 110 143 161 993 995 1080 5432
Thu Jul 24 02:19:34 2014	23	79.189.129.22	45125	0
Thu Jul 24 02:36:33 2014	23	116.11.206.121	51051	0
Fri Jul 25 23:11:58 2014	21	173.230.157.41	7512	0
Sat Jul 26 04:26:50 2014	23	190.147.218.55	34521	0
Sat Jul 26 05:35:45 2014	23	117.212.36.238	39129	12	root\r\t\r\tsh\r\t
Sun Jul 27 09:08:22 2014	21	93.174.95.55	37215	49	USER anonymous\tPASS Anonymous@anonymous.com\tSTAT\t
Mon Jul 28 15:21:56 2014	993	71.6.165.200	40767	0
Mon Jul 28 20:28:26 2014	110	130.185.157.249	2132	6	QUIT\r\t
Mon Jul 28 20:28:29 2014	110	130.185.157.249	2973	6	QUIT\r\t
Tue Jul 29 23:18:26 2014	23	176.41.5.134	41035	0
Tue Jul 29 23:50:17 2014	5432	66.240.236.119	58239	8	\0\0\0\x08\x04\xd2\x16/
Wed Jul 30 01:18:58 2014	23	58.222.122.209	4253	0
Wed Jul 30 03:33:42 2014	1080	179.99.200.39	18877	0
Wed Jul 30 03:33:42 2014	1080	179.99.200.39	18904	0
Thu Jul 31 14:01:22 2014	993	198.20.70.114	53755	0
Thu Jul 31 14:29:24 2014	1080	199.87.232.185	4080	0
Thu Jul 31 15:45:06 2014	1080	42.156.250.111	36732	9	\x04\x01\x1f\0\0\0\0\0\0
Thu Jul 31 15:45:06 2014	1080	42.156.250.118	59548	9	\x04\x01\x1f\0\0\0\0\0\0
Thu Jul 31 15:45:06 2014	1080	42.120.142.222	45231	4	\x05\x02\0\x02
Thu Jul 31 15:45:06 2014	1080	42.120.142.222	45230	9	\x04\x01\x1f\0\0\0\0\0\0
Thu Jul 31 15:45:06 2014	1080	42.120.142.220	54742	4	\x05\x02\0\x02
