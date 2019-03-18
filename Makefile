CFLAGS = -std=c89 -fdiagnostics-color=always -pedantic -g -Wall
LDLIBS = -lm
all default:
	$(info Usage:)
	$(info make target)
	$(info )
	$(info Replace target with one of:)
	$(info pi0              ~    Build for Raspberry Pi Zero or Zero W)
	$(info pi1              ~    Build for Raspberry Pi 1)
	$(info pi2              ~    Build for Raspberry Pi 2)
	$(info pi3              ~    Build for Raspberry Pi 3)
	$(info clean            ~    Remove built files, leaving only source code)
	$(info )
	$(error Target not specified)
clean:
	@printf "\033[1;33m[\033[1;36mREMOVING BUILT BINARIES\033[1;33m]\033[0m\n"
	rm -rf *.o *.d include/*.o include/*.d $(SRC:.c=)
SRC = $(wildcard *.c)
INCLUDES = include/regtool.o include/player.o
pi0 pi1: DEFINES = -DHARDWARE=1
pi2 pi3: DEFINES = -DHARDWARE=2
pi0 pi1 pi2 pi3: $(SRC:.c=)
$(SRC:.c=): % : $(INCLUDES) $(addsuffix .o,$(basename %))
	@printf "\033[1;33m[\033[1;35mLINKING\033[1;36m"
	@printf "     include/regtool.o \033[1;37m+\033[1;36m include/player.o"
	@printf " \033[1;37m+\033[1;36m $(word 3,$^) \033[1;33m->\033[1;32m"
	@printf " $@\033[1;33m]\033[0m\n"
	gcc $(DEFINES) $(CFLAGS) $(LDLIBS) $(INCLUDES) $(word 3,$^) -o $@
	@echo
%.o : %.c
	@printf "\033[1;33m[\033[1;31mCOMPILING\033[1;34m"
	@printf "   $< \033[1;33m->\033[1;36m $@ \033[1;37m+\033[1;36m"
	@printf " $(addsuffix .d,$(basename $@))\033[1;33m]\033[0m\n"
	gcc $(DEFINES) $(CFLAGS) -MD -c $< -o $@
	@cp $*.d $*.e
	@sed -e's/#.*//' -e's/^[^:]*: *//' -e's/ *\\$$//' -e'/^$$/ d' \
	-e's/$$/ :/'<$*.d>>$*.e
	@rm -f $*.d
	@mv $*.e $*.d
	@echo
-include *.d include/*.d
