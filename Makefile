CC65PATH=
CL65=$(CC65PATH)/bin/cl65

SRC= util.c main.c command.c net.c connect.c pg_protocol.c query.c

all: ypsql.prg

ypsql.prg: $(SRC)
	$(CL65) -L $(CC65PATH)/share/cc65/lib/ \
	--config $(CC65PATH)/share/cc65/cfg/c64.cfg \
	--asm-include-dir $(CC65PATH)/share/cc65/asminc/ \
	-I $(CC65PATH)/share/cc65/include/ \
	-o ypsql.prg -t c64 -O $(SRC) ip65/ip65_tcp.lib ip65/ip65_c64.lib

run: ypsql.prg
	sudo x64sc ypsql.prg

clean:
	rm -f *.o
	rm -f *.s
	rm -f ypsql.prg
