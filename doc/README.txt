=== SureAliveD ===

1. What is SureAlived?

SureAliveD is a very efective LVS real service tester. We split
it on two separated programs - surealived (tester) and ipvssync
(kernel ipvs table synchroniser). 
We deceide to replace keepalived tester functionality (not VRRP)
- it has some bugs and a lack of some features we would like to have.

2. Program details:

surealived:
* epoll based
* extensible xml based configuration (module specific)
* real service testers are dynamicaly loaded modules
* currently tcp, http, dns, exec and no-test (virtual tester)
  tester are implemented
* transparent ssl
* gather and store test statistics
* possibility to dump communication of a tested real 
* hosts state are stored in 'offline' and 'override' files honoured
  when aplication is restarted
* surealived restart enforces full ipvs sync
* configuration for ipvssync is saved at start and every minute as
  main file (ipvssync.cfg), changes are saved in special diffs files
* tests startup are spread in time, decreasing connecting impact
  and cpu usage
* you can write your own tester for a specified service (see http
  tester how to do that)
* you can check your config file before you try to restart surealived
  (-t option)
 
ipvssync:
* uses configuration stored in ipvsfull.cfg and diffs files
* allows to leave/remove unmanaged reals/virtuals
* synchronize config to ipvs, allows delete/leave unmanaged virtuals
  (-u option)
* you can check your configuration (-t option)

3. Installation

To compile application need development tools and libraries:
* gcc
* cmake
* libxml2-dev
* libssl-dev
* libpackagekit-glib2-dev
* liblua5.1-0-dev

From src directory (from root account):
* mkdir build
* cd build
* cmake ..
* make #to biuld binaries, or
* make package # to build deb package

surealived needn't to be executed from root account, but you
need to set apropriate grants for such user. 
You need following directories created:
/var/lib/surealived/
/var/lib/surealived/diffs/
/var/lib/surealived/stats/
/var/log/surealived/
/var/log/surealived/comm/

4. Main configuration

4.1 surealived uses configuration files:
  - /etc/surealived/surealived.cfg - main config, describes program
    behaviour and offline.dump, override.dump paths
  - xml configuration (set as argument)

4.2 ipvssync uses configuration files:
  - /etc/surealived/surealived.cfg 
    really only variables: ipvssync_log, ipvssync_logging, no_sync,
    lock_sync_file, full_sync_file, full_reload_file, diff_sync_dir are used

5. surealived XML configuration file
I'll explain semantics on following example:

<surealived>

<virtual name="onet" addr="192.168.0.1" port="80" proto="tcp" sched="wrr" rt="dr">
  <tester loopdelay="1" timeout="2" retries2fail="1" retries2ok="1"
          proto="http" testport="80" url="/" host="www.onet.pl" />
  <real name="sg" addr="213.180.146.27" port="80" weight="10"/>
</virtual>

</surealived>

* virtual attributes:
  * name="string" [mandatory] (max 31 chars)
  * addr="ip" [mandatory if attribute fwmark is not set, if fwmark is set	
               "0.0.0.0" will be used]
  * port="int16" [0<=port<=65535, mandatory if fwmark is not set, otherwise "0"]
  * proto="tcp|udp|fwmark" [mandatory]
  * sched="string" [mandatory] (such ipvs scheduler will be used)
  * rt="dr|masq|tun" [mandatory]
  * fwmark="int" [optional, if > 0 proto="fwmark" should be set]
  * pers="int" [optional, for persistent connections - this is timeout value]

* tester attributes:
  * loopdelay="int" [optional, default 3] - defines delay in seconds between
		     next testing loop for this virtual
  * timeout="int" [optional, default 5] - each real server must completly
		  respond in this time]
  * retries2ok="int" [optional, default 1] - how much tests must succeed to 
		     treat real as online
  * retries2fail="int" [optional, default 1] - how much tests must fail to
		       treat real as offline
  * remove_on_fail="0|1" [optional, default 0 (false)] - if true and real
			 is offline remove it from ipvs
  * debugcomm="0|1" [optional, default 0 (false)] - enable dumping communication
		    details for reals
  * logmicro="0|1"} [optional, default 0 (false)] - do use microseconds resolution
				while saving the statistics
  * proto="string" [mandatory] - which module is used as tester module
  * testport="int" [mandatory] - which port is tested (can be differ than
		   real port)
  * SSL="0|1" [optional, default 0 - no ssl]

  *** other attributes are parsed individual by module specification ***

  Implemented testers:
  --------------------
  a) HTTP

  http tester attributes:
  * url="string" [mandatory] (max BUFSIZ length, 8192 bytes)
  * host="string" [mandatory]
  * naive="0|1" [optional], only response code is read in module and compared 
		to retcode
  * retcode="string" [optional, default "200"]

  b) DNS
  dns tester attributes:
  * request="string" [mandatory], domain for which soa will be checked

  c) TCP
  No additional attributes are required for this tester.

  d) EXEC
  - TO DESCRIBE -

  e) NO-TEST
  All hosts in this tester are treated as ONLINE always

* real attributes
  * name="string" [mandatory, max 31 bytes].
  * addr="ip" [mandatory], 
  * port="int16" [mandatory]
  * weight="int" [mandatory]
  * uthresh="int" [optional]
  * lthresh="int" [optional]
  * testport="int16" [optional, override tester "testport" attribute]
  * rt="string" [optional, override tester "rt" attribute]

6. Runtime weight balancing and rstate management

surealived listens at port 1337 by default, you can currently do two
important tasks:

- change weight: by setting absolute value or percent of configuration weight.
  If you'll have weight=20 and you'll set rweight=1% you'll get... 1. We deceide
  to do that when we want to add a real server which is cache for something 
  and we want to increase its weight depends on cache fill. 

- change rstate to:
  a) DOWN - will remove a real from ipvs
  b) OFFLINE - will set real weight to 0
  c) ONLINE - will bring real to configuration value IF online == ONLINE,
     otherwise host will be evaluated by (online, remove_on_fail)

To manage real weight you can use commands:

> printf "vlist\n" | nc -q 1 localhost 1337
0. vname=onet vproto=tcp vaddr=192.168.0.1 vport=80 vfwmark=0 vrt=dr vsched=wrr
1. vname=wp vproto=tcp vaddr=192.168.0.2 vport=80 vfwmark=0 vrt=dr vsched=wrr

> printf "rlist vproto=tcp vaddr=192.168.0.2 vport=80\n" | nc -q 1 localhost 1337
1. raddr=212.77.100.101 rport=80 currwgt=11 confwgt=11 ronline=TRUE rstate=ONLINE

> printf "rset vproto=tcp vaddr=192.168.0.2 vport=80 raddr=212.77.100.101 rport=80 rstate=OFFLINE\n" | nc -q 1 localhost 1337
Set: rstate=OFFLINE, weight=-1, inpercent=FALSE

> printf "rlist vproto=tcp vaddr=192.168.0.2 vport=80\n" | nc -q 1 localhost 1337
1. raddr=212.77.100.101 rport=80 currwgt=0 confwgt=11 ronline=TRUE rstate=OFFLINE

> printf "stats\n" | nc -q 1 localhost 1337
... useful statistics here ... 

See and try example xml files in examples directory.

Suggested startup when you don't wont to daemonize:

a) 
# surealived -vvv ./examples/onetwp.xml

This will create /var/lib/surealived/ipvsfull.cfg file which is mandatory
for ipvssync. ipvsfull.reload will also be created which enforces ipvssync
to reload configuration.

b) 
# ipvssync -vvvv -u  

This gives you a lot of debug information (-vvvv) and delete unmanaged virtuals.

---------
More info and detailed manual will be soon...

2009-09-07, wegorz.
