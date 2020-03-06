#
# 2018/01/02 updated 
# 2020/03/06 changed to not make ffn everytime
#
CFLAGS = -g -Wall
CC = gcc

# getting fancy here testing for root permissions
SU := $(shell sudo -n ls 1>/dev/null 2>/dev/null && echo root || echo noroot)

INSTALL_DIR = $(HOME)/bin
MANPATH = /usr/local/man/man1

BINS	= ffn
SCRIPTS = mktestnames
OBJS	=
MANSRC  = ffn.man
MANFILE	= ffn.1
LIBS	=
# INCS	:= $(wildcard *.h)


.PHONY: all

all: ffn

ffn: ffn.c
	$(CC) $(CFLAGS) -o ffn ffn.c

install:
	strip ffn
	cp ffn $(INSTALL_DIR)

uninstall:
	rm -f $(INSTALL_DIR)/$(BINS)
ifeq ($(SU), root)
	sudo rm -f $(MANPATH)/$(MANFILE) 
else
	@echo you need root/sudo to un-install man pages
endif

clean:
	rm -rf $(BINS)

man: ffn.man
	@echo "<$(SU)>"
ifeq ($(SU), root)
	sudo cp $(MANSRC) $(MANPATH)/$(MANFILE)
else
	@echo you need root/sudo to install man pages
endif


