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

typedef struct input_event {
	struct timeval time;
	__u16 type;
	__u16 code;
	__s32 value;
} input_event;

int getData(const char* filename, long start_pos, long *end_pos, int *endof,
		input_event **events) {
	FILE* fd;
	char buffer[64];
	size_t num = 64;
	float time_old, time = 0;
	unsigned int type, code = 0;
	int value = 0;
	fd = fopen(filename, "r");
	if (fd == NULL ) {
		return -1;
	}
	if (start_pos > 0) {
		fseek(fd, start_pos, SEEK_SET);
	}

	int i = 0;
	*events = malloc(sizeof(input_event) * 1024);
	input_event *ptr = *events;
	while (fgets(buffer, num, fd) != NULL && i < MAX_BUFFER) {
		sscanf(buffer, "[%f]%x%x%x", &time, &type, &code, &value);
		ptr->time.tv_usec = time_old == 0 ? 0 : (time - time_old) * 1000000;
		time_old = time;
		ptr->type = type;
		ptr->code = code;
		ptr->value = value;
		ptr++;
		i++;
	}
	*end_pos = ftell(fd);
	*endof = feof(fd);
	fclose(fd);
	return i;
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
	input_event *events = NULL;
	const char *device = DEVICE_FILE;
	const char *event_file_name = DATA_FILE;

	if (argc > 1) {
		LOGE("input is %s", argv[1]);
		event_file_name = argv[1];
	}
	if (argc > 2) {
		LOGE("device is %s", argv[2]);
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
	int count = 0;
	while ((count = getData(event_file_name, start, &end, &endof, &events)) > 0) {
		LOGI("get %d events from %ld to %ld", count, start, end);
		int i = 0;
		for (i = 0; i < count; i++) {
			if (events[i].time.tv_usec > 500) {
				microseconds_sleep(events[i].time.tv_usec);
			}

			struct input_event event = events[i];
			ret = write(fd, &event, sizeof(event));
			if (ret < sizeof(event)) {
				fprintf(stderr, "write event failed, %s\n", strerror(errno));
				return -1;
			}
		}
		if (endof > 0) {
			LOGI("get the end of file");
			break;
		}
		start += end;
		if (events != NULL ) {
			free(events);
			LOGI("free memory");
			events = NULL;
		}
	}
	if (events != NULL ) {
		free(events);
		LOGI("free memory");
		events = NULL;
	}
}
