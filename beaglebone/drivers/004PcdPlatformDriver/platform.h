
#undef pr_fmt
#define pr_fmt(fmt) "%s:" fmt, __func__

struct pcdev_platform_data{
	int size;
	int perm;
	const char* serial_number;
};

#define RDWR 0x11
#define RDONLY 0x01
#define WRONLY 0x10
