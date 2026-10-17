.PHONY: all clean

all: test_switch_pro_incoming
	./test_switch_pro_incoming

test_switch_pro_incoming: test_switch_pro_incoming.c \
	../kernel/SwitchProIncoming.c ../kernel/SwitchProIncoming.h \
	../common/include/SwitchProPairing.h
	$(CC) -std=gnu89 -Wall -Wextra -Werror -DSWITCH_PRO_HOST_TEST \
		-iquote ../kernel -iquote ../common/include \
		test_switch_pro_incoming.c ../kernel/SwitchProIncoming.c -o $@

clean:
	$(RM) test_switch_pro_incoming test_switch_pro_incoming_san
