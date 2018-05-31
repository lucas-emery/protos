MODULES=echo toUpper

default: all

all:
	$(foreach var,$(MODULES),$(MAKE) -C $(var);)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(IDIR)/*~
