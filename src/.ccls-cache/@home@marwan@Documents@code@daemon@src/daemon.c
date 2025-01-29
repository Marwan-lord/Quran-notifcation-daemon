#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <getopt.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

static int running = 0;
static int delay = 5;
static int counter = 0;
static char *conf_file_name = NULL;
static char *pid_file_name = NULL;
static int pid_fd = -1;
static char *app_name = NULL;
static FILE *log_stream;

/**
 * brief Read configuration from config file
 */
int read_conf_file(int reload)
{
	FILE *conf_file = NULL;
	int ret = -1;

	if (conf_file_name == NULL) return 0;

	conf_file = fopen(conf_file_name, "r");

	if (conf_file == NULL) {
		syslog(LOG_ERR, "Can not open config file: %s, error: %s",
				conf_file_name, strerror(errno));
		return -1;
	}

	ret = fscanf(conf_file, "%d", &delay);

	if (ret > 0) {
		if (reload == 1) {
			syslog(LOG_INFO, "Reloaded configuration file %s of %s",
				conf_file_name,
				app_name);
		} else {
			syslog(LOG_INFO, "Configuration of %s read from file %s",
				app_name,
				conf_file_name);
		}
	}

	fclose(conf_file);

	return ret;
}

/**
 * This function tries to test config file
 */
int test_conf_file(char *_conf_file_name)
{
	FILE *conf_file = NULL;
	int ret = -1;

	conf_file = fopen(_conf_file_name, "r");

	if (conf_file == NULL) {
		fprintf(stderr, "Can't read config file %s\n",
			_conf_file_name);
		return EXIT_FAILURE;
	}

	ret = fscanf(conf_file, "%d", &delay);

	if (ret <= 0) {
		fprintf(stderr, "Wrong config file %s\n",
			_conf_file_name);
	}

	fclose(conf_file);

	if (ret > 0)
		return EXIT_SUCCESS;
	else
		return EXIT_FAILURE;
}

/**
 * Callback function for handling signals.
 * param	sig	identifier of signal
 */
void handle_signal(int sig)
{
	if (sig == SIGINT) {
		fprintf(log_stream, "Debug: stopping daemon ...\n");
		if (pid_fd != -1) {
			lockf(pid_fd, F_ULOCK, 0);
			close(pid_fd);
		}
		if (pid_file_name != NULL) {
			unlink(pid_file_name);
		}
		running = 0;
		signal(SIGINT, SIG_DFL);
	} else if (sig == SIGHUP) {
		fprintf(log_stream, "Debug: reloading daemon config file ...\n");
		read_conf_file(1);
	} else if (sig == SIGCHLD) {
		fprintf(log_stream, "Debug: received SIGCHLD signal\n");
	}
}

/**
 * This function will daemonize this app
 */
static void daemonize()
{
	pid_t pid = 0;
	int fd;

	pid = fork();

	if (pid < 0) {
		exit(EXIT_FAILURE);
	}

	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}

	if (setsid() < 0) {
		exit(EXIT_FAILURE);
	}

	signal(SIGCHLD, SIG_IGN);

	pid = fork();

	if (pid < 0) {
		exit(EXIT_FAILURE);
	}

	if (pid > 0) {
		exit(EXIT_SUCCESS);
	}

	umask(0);

	chdir("/");

	for (fd = sysconf(_SC_OPEN_MAX); fd > 0; fd--) {
		close(fd);
	}

	stdin = fopen("/dev/null", "r");
	stdout = fopen("/dev/null", "w+");
	stderr = fopen("/dev/null", "w+");

	if (pid_file_name != NULL)
	{
		char str[256];
		pid_fd = open(pid_file_name, O_RDWR|O_CREAT, 0640);
		if (pid_fd < 0) {
			exit(EXIT_FAILURE);
		}
		if (lockf(pid_fd, F_TLOCK, 0) < 0) {
			exit(EXIT_FAILURE);
		}
		sprintf(str, "%d\n", getpid());
		write(pid_fd, str, strlen(str));
	}
}

/**
 * Print help for this application
 */
void print_help(void)
{
	printf("\n Usage: %s [OPTIONS]\n\n", app_name);
	printf("  Options:\n");
	printf("   -h --help                 Print this help\n");
	printf("   -c --conf_file filename   Read configuration from the file\n");
	printf("   -t --test_conf filename   Test configuration file\n");
	printf("   -l --log_file  filename   Write logs to the file\n");
	printf("   -d --daemon               Daemonize this application\n");
	printf("   -p --pid_file  filename   PID file used by daemonized app\n");
	printf("\n");
}

int main(int argc, char *argv[])
{
	static struct option long_options[] = {
		{"conf_file", required_argument, 0, 'c'},
		{"test_conf", required_argument, 0, 't'},
		{"log_file", required_argument, 0, 'l'},
		{"help", no_argument, 0, 'h'},
		{"daemon", no_argument, 0, 'd'},
		{"pid_file", required_argument, 0, 'p'},
		{NULL, 0, 0, 0}
	};

	int value, option_index = 0, ret;
	char *log_file_name = NULL;
	int start_daemonized = 0;

	app_name = argv[0];

	while ((value = getopt_long(argc, argv, "c:l:t:p:dh", long_options, &option_index)) != -1) {
		switch (value) {
			case 'c':
				conf_file_name = strdup(optarg);
				break;
			case 'l':
				log_file_name = strdup(optarg);
				break;
			case 'p':
				pid_file_name = strdup(optarg);
				break;
			case 't':
				return test_conf_file(optarg);
			case 'd':
				start_daemonized = 1;
				break;
			case 'h':
				print_help();
				return EXIT_SUCCESS;
			case '?':
				print_help();
				return EXIT_FAILURE;
			default:
				break;
		}
	}

	if (start_daemonized == 1) {
		daemonize();
	}

	openlog(argv[0], LOG_PID|LOG_CONS, LOG_DAEMON);
	syslog(LOG_INFO, "Started %s", app_name);

	signal(SIGINT, handle_signal);
	signal(SIGHUP, handle_signal);

	if (log_file_name != NULL) {
		log_stream = fopen(log_file_name, "a+");
		if (log_stream == NULL) {
			syslog(LOG_ERR, "Can not open log file: %s, error: %s",
				log_file_name, strerror(errno));
			log_stream = stdout;
		}
	} else {
		log_stream = stdout;
	}

	read_conf_file(0);

	running = 1;

	while (running == 1) {

		ret = fprintf(log_stream, "Debug: %d\n", counter++);
		if (ret < 0) {
			syslog(LOG_ERR, "Can not write to log stream: %s, error: %s",
				(log_stream == stdout) ? "stdout" : log_file_name, strerror(errno));
			break;
		}
		ret = fflush(log_stream);
		if (ret != 0) {
			syslog(LOG_ERR, "Can not fflush() log stream: %s, error: %s",
				(log_stream == stdout) ? "stdout" : log_file_name, strerror(errno));
			break;
		}
		pid_t pid;

		if ((pid = fork()) == -1) {
			perror("Cannot fork");
			exit(1);
		} else if (pid == 0) {
			execl("/nix/store/23nz5mjk3dj7027byc6g5avx3mfwwnqm-libnotify-0.8.3/bin/notify-send", "Hello", "World");
		}
		sleep(delay);
	}

	if (log_stream != stdout) {
		fclose(log_stream);
	}

	syslog(LOG_INFO, "Stopped %s", app_name);
	closelog();

	if (conf_file_name != NULL) free(conf_file_name);
	if (log_file_name != NULL) free(log_file_name);
	if (pid_file_name != NULL) free(pid_file_name);

	return EXIT_SUCCESS;
}
