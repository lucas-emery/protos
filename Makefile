all:
	$(MAKE) -C src

debug:
	$(MAKE) debug -C src/server
	$(MAKE) debug -C src/client

server:
	$(MAKE) -C src/server

client:
	$(MAKE) -C src/client

clean:
	$(MAKE) clean -C src
	rm server
