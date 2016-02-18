#include <list>
#include <string>

#include <stdio.h>
#include <sys/time.h>
#include <errno.h>
#include <android/log.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define  LOG_TAG    "NDK"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#define EVIOCGVERSION		_IOR('E', 0x01, int)			/* get driver version */
#define DATA_FILE "/sdcard/crash/record_stuff.txt"
#define DEVICE_FILE "/dev/input/event6"
#define MAX_BUFFER 1024
using namespace std;
typedef struct input_event {
	struct timeval time;
	__u16 type;
	__u16 code;
	__s32 value;
} input_event;
class EventPump {
private:
	string file_name;
	long cur;
	list<input_event> events;
	EventPump();
	bool getDataImpl();
public:
	bool getData(input_event &event);
	void reset();
	EventPump(string);
	~EventPump();
};
EventPump::~EventPump() {
	events.clear();
	LOGI("clean collection");
}
EventPump::EventPump(string fileName) {
	this->file_name = fileName;
	this->cur = 0;
}
void EventPump::reset() {
	this->cur = 0;
	events.clear();
}
bool EventPump::getData(input_event &event) {
	if (file_name.length() == 0) {
		return false;
	}
	if (events.empty() && !getDataImpl()) {
		return false;
	}
	event = events.front();
	events.pop_front();
	return true;
}
bool EventPump::getDataImpl() {
	FILE* fd;
	char buffer[64];
	size_t num = 64;
	float time_old, time = 0;
	unsigned int type, code = 0;
	int value = 0;
	fd = fopen(file_name.c_str(), "r");
	if (fd == NULL) {
		return false;
	}
	if (cur > 0) {
		fseek(fd, cur, SEEK_SET);
	}

	if (feof(fd) > 0) {
		fclose(fd);
		return false;
	}
	int i = 0;
	while (fgets(buffer, num, fd) != NULL && i < MAX_BUFFER) {
		sscanf(buffer, "[%f]%x%x%x", &time, &type, &code, &value);
		input_event *ptr = new input_event;
		ptr->time.tv_usec = time_old < 1 ? 0 : (time - time_old) * 1000000;
		time_old = time;
		ptr->type = type;
		ptr->code = code;
		ptr->value = value;
		events.push_back(*ptr);
		i++;
	}
	LOGI("get %d events from %ld to %ld", events.size(), cur, ftell(fd));
	cur = ftell(fd);
	fclose(fd);
	if (events.empty()) {
		return false;
	} else {
		return true;
	}
}
void microseconds_sleep(unsigned long uSec) {
	struct timeval tv;
	tv.tv_sec = uSec / 1000000;
	tv.tv_usec = uSec % 1000000;
	int err;
	do {
		err = select(0, NULL, NULL, NULL, &tv);
	} while (err < 0 && errno == EINTR);
}
int main(int argc, char *argv[]) {
	int fd;
	int version;
	int ret;
	long start, end = 0;
	int endof = 0;
	input_event event;
	const char *device = DEVICE_FILE;
	const char *event_file_name = DATA_FILE;

	if (argc > 1) {
		LOGI("input is %s", argv[1]);
		event_file_name = argv[1];
	}
	if (argc > 2) {
		LOGI("device is %s", argv[2]);
		device = argv[2];
	}

	fd = open(device, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "could not open %s, %s\n", "file", strerror(errno));
		return 1;
	}
	if (ioctl(fd, EVIOCGVERSION, &version)) {
		fprintf(stderr, "could not get driver version for %s, %s\n", "file",strerror(errno));
		return 1;
	}
	EventPump eventPump(event_file_name);
	while (eventPump.getData(event)) {
		if (event.time.tv_usec > 500) {
			microseconds_sleep(event.time.tv_usec);
		}
		ret = write(fd, &event, sizeof(event));
		if (ret < sizeof(event)) {
			fprintf(stderr, "write event failed, %s\n", strerror(errno));
			return -1;
		}
	}
	close(fd);
	return 0;
}
