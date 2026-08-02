INCLUDES := ./includes
SOURCES := ./sources
BUILD := ./build

TARGET := screen-fleet-pilot-server
CC := g++
DEFINES := -DLINUX

# 分离编译选项和链接选项
CFLAGS := -g -Wall $(shell pkg-config --cflags json-c sqlite3) $(DEFINES)
LDFLAGS := $(shell pkg-config --libs json-c sqlite3)  -lpthread

# 如果需要静态链接，取消下面这行注释（但要确保系统有 .a 静态库）
# LDFLAGS += -static

OBJS := main.o logmgr.o epollmgr.o socketmgr.o databasemgr.o schedulemgr.o

$(TARGET): $(OBJS)
	@rm -rf $(BUILD)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $^ -o $(TARGET) $(LDFLAGS)
	@mv ./$(TARGET) $(BUILD)/
	@mv ./*.o $(BUILD)/

%.o: $(SOURCES)/%.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean: 
	rm -rf $(BUILD)

run: 
	@./$(BUILD)/$(TARGET)

.PHONY: clean run