all:
	$(MAKE) -C src
	$(MAKE) -C bin

debug:
	$(MAKE) debug -C src/server
	$(MAKE) -C bin
#	$(MAKE) debug -C src/client

server:
	$(MAKE) -C src/server

client:
	$(MAKE) -C src/client

clean:
	$(MAKE) clean -C bin
	$(MAKE) clean -C src
	rm server
