all:
	$(MAKE) -C src

server:
	$(MAKE) -C src/server

client:
	$(MAKE) -C src/client

clean:
	$(MAKE) clean -C src
	rm server client
