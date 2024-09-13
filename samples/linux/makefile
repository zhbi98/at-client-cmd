# Specify the compiler name
CROSS_COMPILE ?= 

# Name of the target
TARGET_NAME := demo

# Source File Directory
source_path  := ./ ../../src
source_path  += ./src 
source_path  += ./cmd 

# Source Files
source_files := 

# Header file directory
include_path := ./include ../../include

# Exclude files that do not participate in compilation
exclude_files := ../../src/at_port.c

#Dependency library
LIB = 

#MAKEFLAGS := -j $(shell nproc)

#
# 警告选项
#
WARNINGS := -Wall -Wextra -Werror\
		-Wshadow -Wundef -Wmaybe-uninitialized   \
		-Wno-unused-function -Wno-error=strict-prototypes -Wpointer-arith -fno-strict-aliasing -Wno-error=cpp -Wuninitialized \
		 -Wno-missing-field-initializers  -Wno-parentheses -Wno-format-nonliteral -Wno-cast-qual -Wunreachable-code -Wno-switch-default  \
		-Wreturn-type -Wmultichar -Wformat-security -Wno-ignored-qualifiers -Wno-error=pedantic -Wno-sign-compare -Wno-error=missing-prototypes -Wdouble-promotion -Wclobbered -Wdeprecated  \
		-Wempty-body  -Wstack-usage=4096 -Wno-unused-parameter \
		-Wtype-limits -Wsizeof-pointer-memaccess -Wpointer-arith -Wno-pointer-to-int-cast  -Wno-pointer-sign

##############################################################################
# Compile options
#
CFLAGS := -O0 -g -fPIC -Wall  $(WARNINGS)
CFLAGS += -std=gnu99

#############################################################################
# Linker options
#
# Link script
LDFLAGS = -Tlinker.lds -Wl,-Map=$(TARGET).map
#pthread 
LDFLAGS += -pthread -lpthread

#############################################################################
AS		    = $(CROSS_COMPILE)as
LD		    = $(CROSS_COMPILE)ld
CC		    = $(CROSS_COMPILE)gcc
CPP		    = $(CC) -E
AR		    = $(CROSS_COMPILE)ar
NM		    = $(CROSS_COMPILE)nm

#Target directory
BIN_DIR   := ./output/
OBJ_DIR   := ./output/obj/
TARGET    := $(BIN_DIR)$(TARGET_NAME)
TOPDIR    := $(shell pwd)/

#Source file directory
SRC_DIRS  := $(source_path)

#Search the list of source files
CSRCS := $(shell find $(SRC_DIRS) -maxdepth 1 -path '*.c') 
CSRCS += $(source_files)
CSRCS := $(notdir $(CSRCS))
#排除文件列表
EXCLUDE_LIST := $(notdir $(exclude_files))

CSRCS := $(filter-out $(EXCLUDE_LIST), $(CSRCS))

#
# Target files(*.o)
#
OBJS := $(addprefix $(OBJ_DIR),$(CSRCS:.c=.o) )


CFLAGS += $(patsubst %,-I$(TOPDIR)%,$(subst ./, ,$(SRC_DIRS)))
CFLAGS += $(addprefix -I$(TOPDIR),$(include_path) )

#指定搜索目录
VPATH += $(addprefix :, $(SRC_DIRS))

all: default $(TARGET)

$(TARGET):$(OBJS)
	@$(CC) $(LDFLAGS) -o  $@ $^
	@echo Creating $(notdir $(TARGET)) ...

$(OBJ_DIR)%.o: %.c
	@echo 'Compiling ' $< ...
	@$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: all clean default

#创建输出目录
default:
	@rm -rf $(TARGET)
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(TARGET).map
	rm -rf $(TARGET)
	@rm -rf $(OBJS)
	@rm -rf tmp