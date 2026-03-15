CXX      := g++
CXXFLAGS := -Wall -Wextra -O2 -std=c++20 -MMD -MP -I..
LDFLAGS  := -lboost_thread -lpthread

TARGET := preprocess

# Recursively collect C++ sources from subdirectories.
rwildcard = $(foreach d,$(wildcard $(1)/*),$(call rwildcard,$(d),$(2)) $(filter $(subst *,%,$(2)),$(d)))
SRCS := $(sort $(call rwildcard,.,*.cpp))
OBJS := $(SRCS:.cpp=.o)
DEPS := $(OBJS:.o=.d)

DEBUG_CXXFLAGS := -Wall -Wextra -Og -g -std=c++20 -MMD -MP -I.. \
                  -fsanitize=address,undefined -fno-omit-frame-pointer
DEBUG_TARGET   := preprocess_debug
DEBUG_OBJS     := $(SRCS:.cpp=.debug.o)
DEBUG_DEPS     := $(DEBUG_OBJS:.o=.d)

.PHONY: all debug clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

debug: $(DEBUG_TARGET)

$(DEBUG_TARGET): $(DEBUG_OBJS)
	$(CXX) $(DEBUG_CXXFLAGS) -o $@ $^

%.debug.o: %.cpp
	$(CXX) $(DEBUG_CXXFLAGS) -c -o $@ $<

-include $(DEPS) $(DEBUG_DEPS)

clean:
	rm -f $(OBJS) $(DEPS) $(DEBUG_OBJS) $(DEBUG_DEPS) $(TARGET) $(DEBUG_TARGET)
