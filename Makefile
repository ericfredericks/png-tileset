
all : targetcheck init $(TARGET)/bin/a.exe


clean : targetcheck
	@rm $(TARGET)/bin/*.exe
	@rm $(TARGET)/dep/*
	@rm $(TARGET)/obj/*


targetcheck :
ifeq ($(TARGET),)
  $(error Please define variable TARGET)
endif

init :
ifeq ($(OS),Windows_NT)
  CFLAGS := -Wall -Iinclude -IC:/c/include
  LFLAGS := -LC:/c/lib -l:libpng.a -l:libz.a
else
  UNAME := $(shell uname)
  ifneq ($(UNAME),Linux)
    $(error Please compile under Windows or Linux)
  endif
  CFLAGS := -Wall -Iinclude
  LFLAGS := 
endif
SRCS	= $(wildcard src/*.c)
OBJS	= $(SRCS:src/%.c=$(TARGET)/obj/%.o)
DEPS	= $(SRCS:src/%.c=$(TARGET)/dep/%.d)
DFLAGS	= -MT $@ -MMD -MP -MF $(TARGET)/dep/$*.d


$(TARGET)/bin/a.exe : $(OBJS) | $(TARGET)/bin
	@echo "Linking ..."
	gcc -o $@ $^ $(LFLAGS) 

$(TARGET)/obj/%.o : src/%.c $(TARGET)/dep/%.d | $(TARGET) $(TARGET)/obj $(TARGET)/dep
	@echo "Compiling ..."
	gcc -o $@ $(CFLAGS) $(DFLAGS) -c $< 

$(DEPS) :
include $(wildcard $(DEPS))


ifeq ($(OS),Windows_NT)
  $(TARGET) $(TARGET)/bin $(TARGET)/obj $(TARGET)/dep : ; @mkdir $(subst /,\,$@)
else
  $(TARGET) $(TARGET)/bin $(TARGET)/obj $(TARGET)/dep : ; @mkdir $@
endif
