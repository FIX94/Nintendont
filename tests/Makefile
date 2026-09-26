.PHONY: all clean

all: test_switch_pro
	./test_switch_pro

test_switch_pro: test_switch_pro.c ../kernel/SwitchPro.c ../kernel/SwitchPro.h
	$(CC) -std=gnu89 -Wall -Wextra -DSWITCH_PRO_HOST_TEST -iquote ../kernel test_switch_pro.c ../kernel/SwitchPro.c -o $@

clean:
	$(RM) test_switch_pro
