ifndef JAVA_HOME
    $(error JAVA_HOME not set)
endif

INCLUDE= -I"$(JAVA_HOME)/include" -I"$(JAVA_HOME)/include/linux"
CFLAGS=-Wall -Werror -fPIC -shared $(INCLUDE)

TARGET=libjvmkill.so

.PHONY: all clean test

all:
	gcc $(CFLAGS) -o $(TARGET) jvmkill.c
	chmod 644 $(TARGET)

clean:
	rm -f $(TARGET)
	rm -f *.class
	rm -f *.hprof

test: all
	$(JAVA_HOME)/bin/javac JvmKillTest.java
	$(JAVA_HOME)/bin/java -Xmx1m \
	    -XX:+HeapDumpOnOutOfMemoryError \
	    -XX:OnOutOfMemoryError='/bin/echo hello' \
	    -agentpath:$(PWD)/$(TARGET) \
	    -cp $(PWD) JvmKillTest

grey: all
	$(JAVA_HOME)/bin/javac JvmKillTestGreyFailure.java
	$(JAVA_HOME)/bin/java -Xmx100m \
	    -XX:OnOutOfMemoryError='/bin/echo OOMKILL' \
		-XX:+UseParNewGC \
		-XX:+UseConcMarkSweepGC \
		-XX:CMSInitiatingOccupancyFraction=75 \
		-XX:+PrintGCDetails \
		-XX:+PrintGCDateStamps \
		-XX:+PrintGCApplicationConcurrentTime \
		-XX:+PrintGCApplicationStoppedTime \
		-Xloggc:gclog \
	    -agentpath:$(PWD)/$(TARGET) \
	    -cp $(PWD) JvmKillTestGreyFailure
