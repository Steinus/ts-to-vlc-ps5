ifdef PS5_PAYLOAD_SDK
  include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk
else
  $(error PS5_PAYLOAD_SDK is undefined)
endif

CFLAGS := -Wall -O2
LDADD  := -lSceSystemService -lSceUserService

all: vlc-bridge.elf

plugin.h: plugin.js
	xxd -i $< | sed 's/unsigned char plugin_js\[\]/static const unsigned char plugin_js[]/; s/unsigned int plugin_js_len/static const unsigned int plugin_js_len/' > $@

vlc-bridge.elf: main.c plugin.h
	$(CC) $(CFLAGS) main.c $(LDADD) -o $@

clean:
	rm -f vlc-bridge.elf plugin.h
