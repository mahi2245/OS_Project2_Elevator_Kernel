#include <unistd.h>
#include <sys/syscall.h>


int main() {
	syscall(SYS_getpid);
	syscall(SYS_getppid);
	syscall(SYS_chdir, ".");
	syscall(SYS_getuid);
	syscall(SYS_getgid);
	return 0;


}
