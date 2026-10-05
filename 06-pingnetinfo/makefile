CC = gcc

# Uses a raw ICMP socket, so run it with sudo:
#   sudo ./pingnetinfo <site> <n> <T>     e.g.  sudo ./pingnetinfo 8.8.8.8 3 1
pingnetinfo: pingnetinfo.c
	$(CC) -o $@ $<

# Variant that delays every received packet, for testing on small networks
simulate: pingnetinfo.c
	$(CC) -DSIMULATE_DELAY -o pingnetinfo $<

clean:
	rm -f pingnetinfo
